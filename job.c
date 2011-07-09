/* $Id$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Job scheduling. Run queued commands in the background and record their
 * output.
 */

void	job_callback(struct bufferevent *, short, void *);

/* All jobs list. */
struct joblist	all_jobs = LIST_HEAD_INITIALIZER(all_jobs);

/* Start a job running, if it isn't already. */
struct job *
job_run(const char *cmd,
    void (*callbackfn)(struct job *), void (*freefn)(void *), void *data)
{
	struct job	*job;
	struct environ	 env;
	pid_t		 pid;
	int		 nullfd, out[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, out) != 0)
		return (NULL);

	environ_init(&env);
	environ_copy(&global_environ, &env);
	server_fill_environ(NULL, &env);

	switch (pid = fork()) {
	case -1:
		environ_free(&env);
		return (NULL);
	case 0:		/* child */
		clear_signals(1);

		environ_push(&env);
		environ_free(&env);

		if (dup2(out[1], STDOUT_FILENO) == -1)
			fatal("dup2 failed");
		if (out[1] != STDOUT_FILENO)
			close(out[1]);
		close(out[0]);

		nullfd = open(_PATH_DEVNULL, O_RDWR, 0);
		if (nullfd < 0)
			fatal("open failed");
		if (dup2(nullfd, STDIN_FILENO) == -1)
			fatal("dup2 failed");
		if (dup2(nullfd, STDERR_FILENO) == -1)
			fatal("dup2 failed");
		if (nullfd != STDIN_FILENO && nullfd != STDERR_FILENO)
			close(nullfd);

		closefrom(STDERR_FILENO + 1);

		execl(_PATH_BSHELL, "sh", "-c", cmd, (char *) NULL);
		fatal("execl failed");
	}

	/* parent */
	environ_free(&env);
	close(out[1]);

	job = xmalloc(sizeof *job);
	job->cmd = xstrdup(cmd);
	job->pid = pid;
	job->status = 0;

	LIST_INSERT_HEAD(&all_jobs, job, lentry);

	job->callbackfn = callbackfn;
	job->freefn = freefn;
	job->data = data;

	job->fd = out[0];
	setblocking(job->fd, 0);

	job->event = bufferevent_new(job->fd, NULL, NULL, job_callback, job);
	bufferevent_enable(job->event, EV_READ);

	log_debug("run job %p: %s, pid %ld", job, job->cmd, (long) job->pid);
	return (job);
}

/* Kill and free an individual job. */
void
job_free(struct job *job)
{
	log_debug("free job %p: %s", job, job->cmd);

	LIST_REMOVE(job, lentry);
	xfree(job->cmd);

	if (job->freefn != NULL && job->data != NULL)
		job->freefn(job->data);

	if (job->pid != -1)
		kill(job->pid, SIGTERM);
	if (job->fd != -1)
		close(job->fd);
	if (job->event != NULL)
		bufferevent_free(job->event);

	xfree(job);
}

/* Job buffer error callback. */
/* ARGSUSED */
void
job_callback(unused struct bufferevent *bufev, unused short events, void *data)
{
	struct job	*job = data;

	log_debug("job error %p: %s, pid %ld", job, job->cmd, (long) job->pid);

	if (job->pid == -1) {
		if (job->callbackfn != NULL)
			job->callbackfn(job);
		job_free(job);
	} else {
		bufferevent_disable(job->event, EV_READ);
		close(job->fd);
		job->fd = -1;
	}
}

/* Job died (waitpid() returned its pid). */
void
job_died(struct job *job, int status)
{
	log_debug("job died %p: %s, pid %ld", job, job->cmd, (long) job->pid);

	job->status = status;

	if (job->fd == -1) {
		if (job->callbackfn != NULL)
			job->callbackfn(job);
		job_free(job);
	} else
		job->pid = -1;
}
