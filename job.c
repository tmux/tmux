/* $OpenBSD$ */

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
#include <paths.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Job scheduling. Run queued commands in the background and record their
 * output.
 */

/* All jobs list. */
struct joblist	all_jobs = SLIST_HEAD_INITIALIZER(all_jobs);

RB_GENERATE(jobs, job, entry, job_cmp);

void	job_callback(struct bufferevent *, short, void *);

int
job_cmp(struct job *job1, struct job *job2)
{
	return (strcmp(job1->cmd, job2->cmd));
}

/* Initialise job tree. */
void
job_tree_init(struct jobs *jobs)
{
	RB_INIT(jobs);
}

/* Destroy a job tree. */
void
job_tree_free(struct jobs *jobs)
{
	struct job	*job;

	while (!RB_EMPTY(jobs)) {
		job = RB_ROOT(jobs);
		RB_REMOVE(jobs, jobs, job);
		job_free(job);
	}
}

/* Find a job and return it. */
struct job *
job_get(struct jobs *jobs, const char *cmd)
{
	struct job	job;

	job.cmd = (char *) cmd;
	return (RB_FIND(jobs, jobs, &job));
}

/* Add a job. */
struct job *
job_add(struct jobs *jobs, int flags, struct client *c, const char *cmd,
    void (*callbackfn)(struct job *), void (*freefn)(void *), void *data)
{
	struct job	*job;

	job = xmalloc(sizeof *job);
	job->cmd = xstrdup(cmd);
	job->pid = -1;
	job->status = 0;

	job->client = c;

	job->fd = -1;
	job->event = NULL;

	job->callbackfn = callbackfn;
	job->freefn = freefn;
	job->data = data;

	job->flags = flags;

	if (jobs != NULL)
		RB_INSERT(jobs, jobs, job);
	SLIST_INSERT_HEAD(&all_jobs, job, lentry);

	return (job);
}

/* Remove job from tree and free. */
void
job_remove(struct jobs *jobs, struct job *job)
{
	if (jobs != NULL)
		RB_REMOVE(jobs, jobs, job);
	job_free(job);
}

/* Kill and free an individual job. */
void
job_free(struct job *job)
{
	job_kill(job);

	SLIST_REMOVE(&all_jobs, job, job, lentry);
	xfree(job->cmd);

	if (job->freefn != NULL && job->data != NULL)
		job->freefn(job->data);

	if (job->fd != -1)
		close(job->fd);
	if (job->event != NULL)
		bufferevent_free(job->event);

	xfree(job);
}

/* Start a job running, if it isn't already. */
int
job_run(struct job *job)
{
	int	nullfd, out[2], mode;

	if (job->fd != -1 || job->pid != -1)
		return (0);

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, out) != 0)
		return (-1);

	switch (job->pid = fork()) {
	case -1:
		return (-1);
	case 0:		/* child */
		clear_signals();

		environ_push(&global_environ);

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

		execl(_PATH_BSHELL, "sh", "-c", job->cmd, (char *) NULL);
		fatal("execl failed");
	default:	/* parent */
		close(out[1]);

		job->fd = out[0];
		if ((mode = fcntl(job->fd, F_GETFL)) == -1)
			fatal("fcntl failed");
		if (fcntl(job->fd, F_SETFL, mode|O_NONBLOCK) == -1)
			fatal("fcntl failed");
		if (fcntl(job->fd, F_SETFD, FD_CLOEXEC) == -1)
			fatal("fcntl failed");

		if (job->event != NULL)
			bufferevent_free(job->event);
		job->event =
		    bufferevent_new(job->fd, NULL, NULL, job_callback, job);
		bufferevent_enable(job->event, EV_READ);

		return (0);
	}
}

/* Job buffer error callback. */
/* ARGSUSED */
void
job_callback(unused struct bufferevent *bufev, unused short events, void *data)
{
	struct job	*job = data;

	bufferevent_disable(job->event, EV_READ);
	close(job->fd);
	job->fd = -1;

	if (job->pid == -1) {
		if (job->callbackfn != NULL)
			job->callbackfn(job);
		if ((!job->flags & JOB_PERSIST))
			job_free(job);
	}
}

/* Job died (waitpid() returned its pid). */
void
job_died(struct job *job, int status)
{
	job->status = status;
	job->pid = -1;

	if (job->fd == -1) {
		if (job->callbackfn != NULL)
			job->callbackfn(job);
		if ((!job->flags & JOB_PERSIST))
			job_free(job);
	}
}

/* Kill a job. */
void
job_kill(struct job *job)
{
	if (job->pid == -1)
		return;
	kill(job->pid, SIGTERM);
	job->pid = -1;
}
