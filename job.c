/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Job scheduling. Run queued commands in the background and record their
 * output.
 */

static void	job_read_callback(struct bufferevent *, void *);
static void	job_write_callback(struct bufferevent *, void *);
static void	job_error_callback(struct bufferevent *, short, void *);

/* A single job. */
struct job {
	enum {
		JOB_RUNNING,
		JOB_DEAD,
		JOB_CLOSED
	} state;

	int			 flags;

	char			*cmd;
	pid_t			 pid;
	char		         tty[TTY_NAME_MAX];
	int			 status;

	int			 fd;
	struct bufferevent	*event;

	job_update_cb		 updatecb;
	job_complete_cb		 completecb;
	job_free_cb		 freecb;
	void			*data;

	LIST_ENTRY(job)		 entry;
};

/* All jobs list. */
static LIST_HEAD(joblist, job) all_jobs = LIST_HEAD_INITIALIZER(all_jobs);

/* Start a job running. */
struct job *
job_run(const char *cmd, int argc, char **argv, struct environ *e,
    struct session *s, const char *cwd, job_update_cb updatecb,
    job_complete_cb completecb, job_free_cb freecb, void *data, int flags,
    int sx, int sy)
{
	struct job	 *job;
	struct environ	 *env;
	pid_t		  pid;
	int		  nullfd, out[2], master;
	const char	 *home, *shell;
	sigset_t	  set, oldset;
	struct winsize	  ws;
	char		**argvp, tty[TTY_NAME_MAX], *argv0;
	struct options	 *oo;

	/*
	 * Do not set TERM during .tmux.conf (second argument here), it is nice
	 * to be able to use if-shell to decide on default-terminal based on
	 * outside TERM.
	 */
	env = environ_for_session(s, !cfg_finished);
	if (e != NULL)
		environ_copy(e, env);

	if (~flags & JOB_DEFAULTSHELL)
		shell = _PATH_BSHELL;
	else {
		if (s != NULL)
			oo = s->options;
		else
			oo = global_s_options;
		shell = options_get_string(oo, "default-shell");
		if (!checkshell(shell))
			shell = _PATH_BSHELL;
	}
	argv0 = shell_argv0(shell, 0);

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oldset);

	if (flags & JOB_PTY) {
		memset(&ws, 0, sizeof ws);
		ws.ws_col = sx;
		ws.ws_row = sy;
		pid = fdforkpty(ptm_fd, &master, tty, NULL, &ws);
	} else {
		if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, out) != 0)
			goto fail;
		pid = fork();
	}
	if (cmd == NULL) {
		cmd_log_argv(argc, argv, "%s:", __func__);
		log_debug("%s: cwd=%s, shell=%s", __func__,
		    cwd == NULL ? "" : cwd, shell);
	} else {
		log_debug("%s: cmd=%s, cwd=%s, shell=%s", __func__, cmd,
		    cwd == NULL ? "" : cwd, shell);
	}

	switch (pid) {
	case -1:
		if (~flags & JOB_PTY) {
			close(out[0]);
			close(out[1]);
		}
		goto fail;
	case 0:
		proc_clear_signals(server_proc, 1);
		sigprocmask(SIG_SETMASK, &oldset, NULL);

		if ((cwd == NULL || chdir(cwd) != 0) &&
		    ((home = find_home()) == NULL || chdir(home) != 0) &&
		    chdir("/") != 0)
			fatal("chdir failed");

		environ_push(env);
		environ_free(env);

		if (~flags & JOB_PTY) {
			if (dup2(out[1], STDIN_FILENO) == -1)
				fatal("dup2 failed");
			if (dup2(out[1], STDOUT_FILENO) == -1)
				fatal("dup2 failed");
			if (out[1] != STDIN_FILENO && out[1] != STDOUT_FILENO)
				close(out[1]);
			close(out[0]);

			nullfd = open(_PATH_DEVNULL, O_RDWR);
			if (nullfd == -1)
				fatal("open failed");
			if (dup2(nullfd, STDERR_FILENO) == -1)
				fatal("dup2 failed");
			if (nullfd != STDERR_FILENO)
				close(nullfd);
		}
		closefrom(STDERR_FILENO + 1);

		if (cmd != NULL) {
			setenv("SHELL", shell, 1);
			execl(shell, argv0, "-c", cmd, (char *)NULL);
			fatal("execl failed");
		} else {
			argvp = cmd_copy_argv(argc, argv);
			execvp(argvp[0], argvp);
			fatal("execvp failed");
		}
	}

	sigprocmask(SIG_SETMASK, &oldset, NULL);
	environ_free(env);
	free(argv0);

	job = xmalloc(sizeof *job);
	job->state = JOB_RUNNING;
	job->flags = flags;

	if (cmd != NULL)
		job->cmd = xstrdup(cmd);
	else
		job->cmd = cmd_stringify_argv(argc, argv);
	job->pid = pid;
	strlcpy(job->tty, tty, sizeof job->tty);
	job->status = 0;

	LIST_INSERT_HEAD(&all_jobs, job, entry);

	job->updatecb = updatecb;
	job->completecb = completecb;
	job->freecb = freecb;
	job->data = data;

	if (~flags & JOB_PTY) {
		close(out[1]);
		job->fd = out[0];
	} else
		job->fd = master;
	setblocking(job->fd, 0);

	job->event = bufferevent_new(job->fd, job_read_callback,
	    job_write_callback, job_error_callback, job);
	if (job->event == NULL)
		fatalx("out of memory");
	bufferevent_enable(job->event, EV_READ|EV_WRITE);

	log_debug("run job %p: %s, pid %ld", job, job->cmd, (long)job->pid);
	return (job);

fail:
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	environ_free(env);
	free(argv0);
	return (NULL);
}

/* Take job's file descriptor and free the job. */
int
job_transfer(struct job *job, pid_t *pid, char *tty, size_t ttylen)
{
	int	fd = job->fd;

	log_debug("transfer job %p: %s", job, job->cmd);

	if (pid != NULL)
		*pid = job->pid;
	if (tty != NULL)
		strlcpy(tty, job->tty, ttylen);

	LIST_REMOVE(job, entry);
	free(job->cmd);

	if (job->freecb != NULL && job->data != NULL)
		job->freecb(job->data);

	if (job->event != NULL)
		bufferevent_free(job->event);

	free(job);
	return (fd);
}

/* Kill and free an individual job. */
void
job_free(struct job *job)
{
	log_debug("free job %p: %s", job, job->cmd);

	LIST_REMOVE(job, entry);
	free(job->cmd);

	if (job->freecb != NULL && job->data != NULL)
		job->freecb(job->data);

	if (job->pid != -1)
		kill(job->pid, SIGTERM);
	if (job->event != NULL)
		bufferevent_free(job->event);
	if (job->fd != -1)
		close(job->fd);

	free(job);
}

/* Resize job. */
void
job_resize(struct job *job, u_int sx, u_int sy)
{
	struct winsize	 ws;

	if (job->fd == -1 || (~job->flags & JOB_PTY))
		return;

	log_debug("resize job %p: %ux%u", job, sx, sy);

	memset(&ws, 0, sizeof ws);
	ws.ws_col = sx;
	ws.ws_row = sy;
	if (ioctl(job->fd, TIOCSWINSZ, &ws) == -1)
		fatal("ioctl failed");
}

/* Job buffer read callback. */
static void
job_read_callback(__unused struct bufferevent *bufev, void *data)
{
	struct job	*job = data;

	if (job->updatecb != NULL)
		job->updatecb(job);
}

/*
 * Job buffer write callback. Fired when the buffer falls below watermark
 * (default is empty). If all the data has been written, disable the write
 * event.
 */
static void
job_write_callback(__unused struct bufferevent *bufev, void *data)
{
	struct job	*job = data;
	size_t		 len = EVBUFFER_LENGTH(EVBUFFER_OUTPUT(job->event));

	log_debug("job write %p: %s, pid %ld, output left %zu", job, job->cmd,
	    (long) job->pid, len);

	if (len == 0 && (~job->flags & JOB_KEEPWRITE)) {
		shutdown(job->fd, SHUT_WR);
		bufferevent_disable(job->event, EV_WRITE);
	}
}

/* Job buffer error callback. */
static void
job_error_callback(__unused struct bufferevent *bufev, __unused short events,
    void *data)
{
	struct job	*job = data;

	log_debug("job error %p: %s, pid %ld", job, job->cmd, (long) job->pid);

	if (job->state == JOB_DEAD) {
		if (job->completecb != NULL)
			job->completecb(job);
		job_free(job);
	} else {
		bufferevent_disable(job->event, EV_READ);
		job->state = JOB_CLOSED;
	}
}

/* Job died (waitpid() returned its pid). */
void
job_check_died(pid_t pid, int status)
{
	struct job	*job;

	LIST_FOREACH(job, &all_jobs, entry) {
		if (pid == job->pid)
			break;
	}
	if (job == NULL)
		return;
	if (WIFSTOPPED(status)) {
		if (WSTOPSIG(status) == SIGTTIN || WSTOPSIG(status) == SIGTTOU)
			return;
		killpg(job->pid, SIGCONT);
		return;
	}
	log_debug("job died %p: %s, pid %ld", job, job->cmd, (long) job->pid);

	job->status = status;

	if (job->state == JOB_CLOSED) {
		if (job->completecb != NULL)
			job->completecb(job);
		job_free(job);
	} else {
		job->pid = -1;
		job->state = JOB_DEAD;
	}
}

/* Get job status. */
int
job_get_status(struct job *job)
{
	return (job->status);
}

/* Get job data. */
void *
job_get_data(struct job *job)
{
	return (job->data);
}

/* Get job event. */
struct bufferevent *
job_get_event(struct job *job)
{
	return (job->event);
}

/* Kill all jobs. */
void
job_kill_all(void)
{
	struct job	*job;

	LIST_FOREACH(job, &all_jobs, entry) {
		if (job->pid != -1)
			kill(job->pid, SIGTERM);
	}
}

/* Are any jobs still running? */
int
job_still_running(void)
{
	struct job	*job;

	LIST_FOREACH(job, &all_jobs, entry) {
		if ((~job->flags & JOB_NOWAIT) && job->state == JOB_RUNNING)
			return (1);
	}
	return (0);
}

/* Print job summary. */
void
job_print_summary(struct cmdq_item *item, int blank)
{
	struct job	*job;
	u_int		 n = 0;

	LIST_FOREACH(job, &all_jobs, entry) {
		if (blank) {
			cmdq_print(item, "%s", "");
			blank = 0;
		}
		cmdq_print(item, "Job %u: %s [fd=%d, pid=%ld, status=%d]",
		    n, job->cmd, job->fd, (long)job->pid, job->status);
		n++;
	}
}
