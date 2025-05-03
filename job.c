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

#include <errno.h> /* Added for errno */
#include <fcntl.h>
#include <signal.h>
#include <spawn.h> /* Added for posix_spawn */
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

	/* Use posix_spawn for non-PTY NOWAIT jobs. */
	if ((flags & JOB_NOWAIT) && !(flags & JOB_PTY)) {
		posix_spawnattr_t	 attr;
		posix_spawn_file_actions_t file_actions;
		int			 status, saved_errno;
		const char		*spawn_path;
		int			 spawn_argc = 0; /* For cmd_free_argv */
		char		       **envp = NULL; /* For posix_spawn env */
		int			 pipefd[2] = { -1, -1 }; /* Pipe for stdout */

		log_debug("%s: using posix_spawn for NOWAIT job (cwd=%s, ignored)",
		    __func__, cwd == NULL ? "default" : cwd);

		/* Create pipe for stdout. */
		if (pipe(pipefd) != 0) {
			log_debug("%s: pipe failed: %s", __func__, strerror(errno));
			goto fail_spawn; /* errno set by pipe */
		}

		/* Prepare arguments. */
		if (cmd != NULL) {
			spawn_argc = 3; /* shell, -c, cmd */
			argvp = xcalloc(spawn_argc + 1, sizeof *argvp);
			argvp[0] = argv0;
			argvp[1] = (char *)"-c";
			argvp[2] = (char *)cmd;
			argvp[3] = NULL;
			spawn_path = shell;
		} else {
			spawn_argc = argc;
			argvp = cmd_copy_argv(argc, argv);
			if (argc == 0 || argvp == NULL || argvp[0] == NULL) {
				log_debug("%s: posix_spawn requires argv[0]", __func__);
				goto fail_spawn;
			}
			spawn_path = argvp[0];
		}

		/* Prepare attributes and file actions. */
		if ((status = posix_spawnattr_init(&attr)) != 0 ||
		    (status = posix_spawnattr_setsigmask(&attr, &oldset)) != 0 ||
		    (status = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK)) != 0 ||
		    (status = posix_spawn_file_actions_init(&file_actions)) != 0 ||
		    /* Redirect stdin from /dev/null */
		    (status = posix_spawn_file_actions_addopen(&file_actions, STDIN_FILENO, _PATH_DEVNULL, O_RDONLY, 0)) != 0 ||
		    /* Duplicate pipe write end to stdout */
		    (status = posix_spawn_file_actions_adddup2(&file_actions, pipefd[1], STDOUT_FILENO)) != 0 ||
		    /* Redirect stderr to /dev/null */
		    (status = posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO, _PATH_DEVNULL, O_WRONLY | O_CREAT | O_TRUNC, 0644)) != 0 ||
		    /* Close pipe read end in child */
		    (status = posix_spawn_file_actions_addclose(&file_actions, pipefd[0])) != 0 ||
		    /* Close original pipe write end in child (after dup) */
		    (status = posix_spawn_file_actions_addclose(&file_actions, pipefd[1])) != 0) {
			log_debug("%s: posix_spawn prep failed: %s", __func__, strerror(status));
			if (status == 0) /* Only destroy actions if attr init failed */
			    posix_spawn_file_actions_destroy(&file_actions);
			posix_spawnattr_destroy(&attr);
			goto fail_spawn;
		}

		/* Prepare environment array. */
		envp = environ_get_envp(env);

		/* Log the envp array before spawning */
		log_debug("%s: spawning with envp:", __func__);
		for (int j = 0; envp[j] != NULL; j++)
			log_debug("%s: envp[%d] = %s", __func__, j, envp[j]);

		/* Spawn the process. */
		status = posix_spawn(&pid, spawn_path, &file_actions, &attr, argvp, envp);
		saved_errno = errno;

		/* Clean up spawn resources (parent side). */
		posix_spawn_file_actions_destroy(&file_actions);
		posix_spawnattr_destroy(&attr);
		environ_free_envp(envp); /* Free the envp array */
		close(pipefd[1]); /* Close write end in parent */
		pipefd[1] = -1; /* Mark as closed */

		if (status != 0) {
			log_debug("%s: posix_spawn failed for %s: %s", __func__, spawn_path, strerror(status));
			errno = saved_errno;
			close(pipefd[0]); /* Close read end on failure */
			pipefd[0] = -1;
			goto fail_spawn;
		}
		log_debug("%s: posix_spawn succeeded, pid %ld, fd %d", __func__, (long)pid, pipefd[0]);

		/* Job struct setup. */
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		environ_free(env);
		free(argv0);
		if (cmd != NULL)
			free(argvp);
		else
			cmd_free_argv(spawn_argc, argvp);

		job = xcalloc(1, sizeof *job);
		job->state = JOB_RUNNING;
		job->flags = flags;
		if (cmd != NULL)
			job->cmd = xstrdup(cmd);
		else
			job->cmd = cmd_stringify_argv(argc, argv);
		job->pid = pid;
		job->fd = pipefd[0]; /* Store read end of pipe */
		job->status = 0;
		job->updatecb = updatecb;
		job->completecb = completecb;
		job->freecb = freecb;
		job->data = data;

		/* Set fd non-blocking and create bufferevent. */
		setblocking(job->fd, 0);
		job->event = bufferevent_new(job->fd, job_read_callback,
		    job_write_callback, job_error_callback, job);
		if (job->event == NULL) {
			log_debug("%s: bufferevent_new failed", __func__);
			close(job->fd); /* Close fd before failing */
			job->fd = -1;
			/* Need to free job struct partially created */
			free(job->cmd);
			free(job);
			/* Restore errno? Maybe not needed as we return NULL */
			goto fail_spawn_post_job; /* Special fail path */
		}
		bufferevent_enable(job->event, EV_READ|EV_WRITE);

		LIST_INSERT_HEAD(&all_jobs, job, entry);
		log_debug("run job %p: %s, pid %ld (NOWAIT)", job, job->cmd, (long)job->pid);
		return (job);

	fail_spawn: /* Failure before or during posix_spawn */
		if (pipefd[0] != -1)
			close(pipefd[0]);
		if (pipefd[1] != -1)
			close(pipefd[1]);
		/* Fall through */

	fail_spawn_post_job: /* Failure after job struct allocated */
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		environ_free(env);
		free(argv0);
		if (cmd != NULL)
			free(argvp);
		else
			cmd_free_argv(spawn_argc, argvp);
		environ_free_envp(envp);
		return (NULL);
	}

	/* Original fork/pty logic for other cases. */
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
	case 0: /* Child process (fork/pty path) */
		proc_clear_signals(server_proc, 1);
		sigprocmask(SIG_SETMASK, &oldset, NULL);

		if ((cwd == NULL || chdir(cwd) != 0) &&
		    ((home = find_home()) == NULL || chdir(home) != 0) &&
		    chdir("/") != 0)
			fatal("chdir failed");

		environ_push(env); /* Environment for fork/pty child */
		environ_log(env, "%s: fork child env after push: ", __func__); /* Log env */
		environ_free(env); /* Free child's copy */

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
			if (argc == 0 || argvp == NULL || argvp[0] == NULL)
				fatal("execvp requires arguments");
			execvp(argvp[0], argvp);
			fatal("execvp failed");
		}
	}

	/* Parent process (fork/pty path) */
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	environ_free(env); /* Free parent's copy */
	free(argv0);

	job = xcalloc(1, sizeof *job);
	job->state = JOB_RUNNING;
	job->flags = flags;

	if (cmd != NULL)
		job->cmd = xstrdup(cmd);
	else
		job->cmd = cmd_stringify_argv(argc, argv);
	job->pid = pid;
	if (flags & JOB_PTY)
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

fail: /* General failure path */
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
