/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Main server functions.
 */

struct clients		 clients;

struct tmuxproc		*server_proc;
static int		 server_fd = -1;
static uint64_t		 server_client_flags;
static int		 server_exit;
static struct event	 server_ev_accept;
static struct event	 server_ev_tidy;

struct cmd_find_state	 marked_pane;

static u_int		 message_next;
struct message_list	 message_log;

static int	server_loop(void);
static void	server_send_exit(void);
static void	server_accept(int, short, void *);
static void	server_signal(int);
static void	server_child_signal(void);
static void	server_child_exited(pid_t, int);
static void	server_child_stopped(pid_t, int);

/* Set marked pane. */
void
server_set_marked(struct session *s, struct winlink *wl, struct window_pane *wp)
{
	cmd_find_clear_state(&marked_pane, 0);
	marked_pane.s = s;
	marked_pane.wl = wl;
	marked_pane.w = wl->window;
	marked_pane.wp = wp;
}

/* Clear marked pane. */
void
server_clear_marked(void)
{
	cmd_find_clear_state(&marked_pane, 0);
}

/* Is this the marked pane? */
int
server_is_marked(struct session *s, struct winlink *wl, struct window_pane *wp)
{
	if (s == NULL || wl == NULL || wp == NULL)
		return (0);
	if (marked_pane.s != s || marked_pane.wl != wl)
		return (0);
	if (marked_pane.wp != wp)
		return (0);
	return (server_check_marked());
}

/* Check if the marked pane is still valid. */
int
server_check_marked(void)
{
	return (cmd_find_valid_state(&marked_pane));
}

/* Create server socket. */
static int
server_create_socket(int flags, char **cause)
{
	struct sockaddr_un	sa;
	size_t			size;
	mode_t			mask;
	int			fd, saved_errno;

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, socket_path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		goto fail;
	}
	unlink(sa.sun_path);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		goto fail;

	if (flags & CLIENT_DEFAULTSOCKET)
		mask = umask(S_IXUSR|S_IXGRP|S_IRWXO);
	else
		mask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if (bind(fd, (struct sockaddr *)&sa, sizeof sa) == -1) {
		saved_errno = errno;
		close(fd);
		errno = saved_errno;
		goto fail;
	}
	umask(mask);

	if (listen(fd, 128) == -1) {
		saved_errno = errno;
		close(fd);
		errno = saved_errno;
		goto fail;
	}
	setblocking(fd, 0);

	return (fd);

fail:
	if (cause != NULL) {
		xasprintf(cause, "error creating %s (%s)", socket_path,
		    strerror(errno));
	}
	return (-1);
}

/* Tidy up every hour. */
static void
server_tidy_event(__unused int fd, __unused short events, __unused void *data)
{
    struct timeval	tv = { .tv_sec = 3600 };
    uint64_t		t = get_timer();

    format_tidy_jobs();

#ifdef HAVE_MALLOC_TRIM
    malloc_trim(0);
#endif

    log_debug("%s: took %llu milliseconds", __func__,
        (unsigned long long)(get_timer() - t));
    evtimer_add(&server_ev_tidy, &tv);
}

/* Fork new server. */
int
server_start(struct tmuxproc *client, int flags, struct event_base *base,
    int lockfd, char *lockfile)
{
	int		 fd;
	sigset_t	 set, oldset;
	struct client	*c = NULL;
	char		*cause = NULL;
	struct timeval	 tv = { .tv_sec = 3600 };

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oldset);

	if (~flags & CLIENT_NOFORK) {
		if (proc_fork_and_daemon(&fd) != 0) {
			sigprocmask(SIG_SETMASK, &oldset, NULL);
			return (fd);
		}
	}
	proc_clear_signals(client, 0);
	server_client_flags = flags;

	if (event_reinit(base) != 0)
		fatalx("event_reinit failed");
	server_proc = proc_start("server");

	proc_set_signals(server_proc, server_signal);
	sigprocmask(SIG_SETMASK, &oldset, NULL);

	if (log_get_level() > 1)
		tty_create_log();
	if (pledge("stdio rpath wpath cpath fattr unix getpw recvfd proc exec "
	    "tty ps", NULL) != 0)
		fatal("pledge failed");

	input_key_build();
	RB_INIT(&windows);
	RB_INIT(&all_window_panes);
	TAILQ_INIT(&clients);
	RB_INIT(&sessions);
	key_bindings_init();
	TAILQ_INIT(&message_log);

	gettimeofday(&start_time, NULL);

	server_fd = server_create_socket(flags, &cause);
	if (server_fd != -1)
		server_update_socket();
	if (~flags & CLIENT_NOFORK)
		c = server_client_create(fd);
	else
		options_set_number(global_options, "exit-empty", 0);

	if (lockfd >= 0) {
		unlink(lockfile);
		free(lockfile);
		close(lockfd);
	}

	if (cause != NULL) {
		if (c != NULL) {
			cmdq_append(c, cmdq_get_error(cause));
			c->flags |= CLIENT_EXIT;
		}
		free(cause);
	}

	evtimer_set(&server_ev_tidy, server_tidy_event, NULL);
	evtimer_add(&server_ev_tidy, &tv);

	server_add_accept(0);
	proc_loop(server_proc, server_loop);

	job_kill_all();
	status_prompt_save_history();

	exit(0);
}

/* Server loop callback. */
static int
server_loop(void)
{
	struct client	*c;
	u_int		 items;

	do {
		items = cmdq_next(NULL);
		TAILQ_FOREACH(c, &clients, entry) {
			if (c->flags & CLIENT_IDENTIFIED)
				items += cmdq_next(c);
		}
	} while (items != 0);

	server_client_loop();

	if (!options_get_number(global_options, "exit-empty") && !server_exit)
		return (0);

	if (!options_get_number(global_options, "exit-unattached")) {
		if (!RB_EMPTY(&sessions))
			return (0);
	}

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL)
			return (0);
	}

	/*
	 * No attached clients therefore want to exit - flush any waiting
	 * clients but don't actually exit until they've gone.
	 */
	cmd_wait_for_flush();
	if (!TAILQ_EMPTY(&clients))
		return (0);

	if (job_still_running())
		return (0);

	return (1);
}

/* Exit the server by killing all clients and windows. */
static void
server_send_exit(void)
{
	struct client	*c, *c1;
	struct session	*s, *s1;

	cmd_wait_for_flush();

	TAILQ_FOREACH_SAFE(c, &clients, entry, c1) {
		if (c->flags & CLIENT_SUSPENDED)
			server_client_lost(c);
		else {
			c->flags |= CLIENT_EXIT;
			c->exit_type = CLIENT_EXIT_SHUTDOWN;
		}
		c->session = NULL;
	}

	RB_FOREACH_SAFE(s, sessions, &sessions, s1)
		session_destroy(s, 1, __func__);
}

/* Update socket execute permissions based on whether sessions are attached. */
void
server_update_socket(void)
{
	struct session	*s;
	static int	 last = -1;
	int		 n, mode;
	struct stat      sb;

	n = 0;
	RB_FOREACH(s, sessions, &sessions) {
		if (s->attached != 0) {
			n++;
			break;
		}
	}

	if (n != last) {
		last = n;

		if (stat(socket_path, &sb) != 0)
			return;
		mode = sb.st_mode & ACCESSPERMS;
		if (n != 0) {
			if (mode & S_IRUSR)
				mode |= S_IXUSR;
			if (mode & S_IRGRP)
				mode |= S_IXGRP;
			if (mode & S_IROTH)
				mode |= S_IXOTH;
		} else
			mode &= ~(S_IXUSR|S_IXGRP|S_IXOTH);
		chmod(socket_path, mode);
	}
}

/* Callback for server socket. */
static void
server_accept(int fd, short events, __unused void *data)
{
	struct sockaddr_storage	sa;
	socklen_t		slen = sizeof sa;
	int			newfd;

	server_add_accept(0);
	if (!(events & EV_READ))
		return;

	newfd = accept(fd, (struct sockaddr *) &sa, &slen);
	if (newfd == -1) {
		if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED)
			return;
		if (errno == ENFILE || errno == EMFILE) {
			/* Delete and don't try again for 1 second. */
			server_add_accept(1);
			return;
		}
		fatal("accept failed");
	}
	if (server_exit) {
		close(newfd);
		return;
	}
	server_client_create(newfd);
}

/*
 * Add accept event. If timeout is nonzero, add as a timeout instead of a read
 * event - used to backoff when running out of file descriptors.
 */
void
server_add_accept(int timeout)
{
	struct timeval tv = { timeout, 0 };

	if (server_fd == -1)
		return;

	if (event_initialized(&server_ev_accept))
		event_del(&server_ev_accept);

	if (timeout == 0) {
		event_set(&server_ev_accept, server_fd, EV_READ, server_accept,
		    NULL);
		event_add(&server_ev_accept, NULL);
	} else {
		event_set(&server_ev_accept, server_fd, EV_TIMEOUT,
		    server_accept, NULL);
		event_add(&server_ev_accept, &tv);
	}
}

/* Signal handler. */
static void
server_signal(int sig)
{
	int	fd;

	log_debug("%s: %s", __func__, strsignal(sig));
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		server_exit = 1;
		server_send_exit();
		break;
	case SIGCHLD:
		server_child_signal();
		break;
	case SIGUSR1:
		event_del(&server_ev_accept);
		fd = server_create_socket(server_client_flags, NULL);
		if (fd != -1) {
			close(server_fd);
			server_fd = fd;
			server_update_socket();
		}
		server_add_accept(0);
		break;
	case SIGUSR2:
		proc_toggle_log(server_proc);
		break;
	}
}

/* Handle SIGCHLD. */
static void
server_child_signal(void)
{
	int	 status;
	pid_t	 pid;

	for (;;) {
		switch (pid = waitpid(WAIT_ANY, &status, WNOHANG|WUNTRACED)) {
		case -1:
			if (errno == ECHILD)
				return;
			fatal("waitpid failed");
		case 0:
			return;
		}
		if (WIFSTOPPED(status))
			server_child_stopped(pid, status);
		else if (WIFEXITED(status) || WIFSIGNALED(status))
			server_child_exited(pid, status);
	}
}

/* Handle exited children. */
static void
server_child_exited(pid_t pid, int status)
{
	struct window		*w, *w1;
	struct window_pane	*wp;

	RB_FOREACH_SAFE(w, windows, &windows, w1) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->pid == pid) {
				wp->status = status;
				wp->flags |= PANE_STATUSREADY;

				log_debug("%%%u exited", wp->id);
				wp->flags |= PANE_EXITED;

				if (window_pane_destroy_ready(wp))
					server_destroy_pane(wp, 1);
				break;
			}
		}
	}
	job_check_died(pid, status);
}

/* Handle stopped children. */
static void
server_child_stopped(pid_t pid, int status)
{
	struct window		*w;
	struct window_pane	*wp;

	if (WSTOPSIG(status) == SIGTTIN || WSTOPSIG(status) == SIGTTOU)
		return;

	RB_FOREACH(w, windows, &windows) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->pid == pid) {
				if (killpg(pid, SIGCONT) != 0)
					kill(pid, SIGCONT);
			}
		}
	}
	job_check_died(pid, status);
}

/* Add to message log. */
void
server_add_message(const char *fmt, ...)
{
	struct message_entry	*msg, *msg1;
	char			*s;
	va_list			 ap;
	u_int			 limit;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);

	log_debug("message: %s", s);

	msg = xcalloc(1, sizeof *msg);
	gettimeofday(&msg->msg_time, NULL);
	msg->msg_num = message_next++;
	msg->msg = s;
	TAILQ_INSERT_TAIL(&message_log, msg, entry);

	limit = options_get_number(global_options, "message-limit");
	TAILQ_FOREACH_SAFE(msg, &message_log, entry, msg1) {
		if (msg->msg_num + limit >= message_next)
			break;
		free(msg->msg);
		TAILQ_REMOVE(&message_log, msg, entry);
		free(msg);
	}
}
