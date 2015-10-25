/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <event.h>
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

struct clients	 clients;

int		 server_fd;
int		 server_exit;
struct event	 server_ev_accept;

struct session		*marked_session;
struct winlink		*marked_winlink;
struct window		*marked_window;
struct window_pane	*marked_window_pane;
struct layout_cell	*marked_layout_cell;

int	server_create_socket(void);
void	server_loop(void);
int	server_should_exit(void);
void	server_send_exit(void);
void	server_accept_callback(int, short, void *);
void	server_signal_callback(int, short, void *);
void	server_child_signal(void);
void	server_child_exited(pid_t, int);
void	server_child_stopped(pid_t, int);

/* Set marked pane. */
void
server_set_marked(struct session *s, struct winlink *wl, struct window_pane *wp)
{
	marked_session = s;
	marked_winlink = wl;
	marked_window = wl->window;
	marked_window_pane = wp;
	marked_layout_cell = wp->layout_cell;
}

/* Clear marked pane. */
void
server_clear_marked(void)
{
	marked_session = NULL;
	marked_winlink = NULL;
	marked_window = NULL;
	marked_window_pane = NULL;
	marked_layout_cell = NULL;
}

/* Is this the marked pane? */
int
server_is_marked(struct session *s, struct winlink *wl, struct window_pane *wp)
{
	if (s == NULL || wl == NULL || wp == NULL)
		return (0);
	if (marked_session != s || marked_winlink != wl)
		return (0);
	if (marked_window_pane != wp)
		return (0);
	return (server_check_marked());
}

/* Check if the marked pane is still valid. */
int
server_check_marked(void)
{
	struct winlink	*wl;

	if (marked_window_pane == NULL)
		return (0);
	if (marked_layout_cell != marked_window_pane->layout_cell)
		return (0);

	if (!session_alive(marked_session))
		return (0);
	RB_FOREACH(wl, winlinks, &marked_session->windows) {
		if (wl->window == marked_window && wl == marked_winlink)
			break;
	}
	if (wl == NULL)
		return (0);

	if (!window_has_pane(marked_window, marked_window_pane))
		return (0);
	return (window_pane_visible(marked_window_pane));
}

/* Create server socket. */
int
server_create_socket(void)
{
	struct sockaddr_un	sa;
	size_t			size;
	mode_t			mask;
	int			fd;

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, socket_path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	unlink(sa.sun_path);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return (-1);

	mask = umask(S_IXUSR|S_IXGRP|S_IRWXO);
	if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) == -1)
		return (-1);
	umask(mask);

	if (listen(fd, 16) == -1)
		return (-1);
	setblocking(fd, 0);

	return (fd);
}

/* Fork new server. */
int
server_start(struct event_base *base, int lockfd, char *lockfile)
{
	int	pair[2];

	/* The first client is special and gets a socketpair; create it. */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pair) != 0)
		fatal("socketpair failed");
	log_debug("starting server");

	switch (fork()) {
	case -1:
		fatal("fork failed");
	case 0:
		break;
	default:
		close(pair[1]);
		return (pair[0]);
	}
	close(pair[0]);

#ifdef __OpenBSD__
	if (pledge("stdio rpath wpath cpath fattr unix recvfd proc exec tty "
	    "ps", NULL) != 0)
		fatal("pledge failed");
#endif

	/*
	 * Must daemonise before loading configuration as the PID changes so
	 * $TMUX would be wrong for sessions created in the config file.
	 */
	if (daemon(1, 0) != 0)
		fatal("daemon failed");

	/* event_init() was called in our parent, need to reinit. */
	clear_signals(0);
	if (event_reinit(base) != 0)
		fatal("event_reinit failed");

	logfile("server");
	log_debug("server started, pid %ld", (long) getpid());

	RB_INIT(&windows);
	RB_INIT(&all_window_panes);
	TAILQ_INIT(&clients);
	RB_INIT(&sessions);
	TAILQ_INIT(&session_groups);
	mode_key_init_trees();
	key_bindings_init();
	utf8_build();

	start_time = time(NULL);
	log_debug("socket path %s", socket_path);
#ifdef HAVE_SETPROCTITLE
	setproctitle("server (%s)", socket_path);
#endif

	server_fd = server_create_socket();
	if (server_fd == -1)
		fatal("couldn't create socket");
	server_update_socket();
	server_client_create(pair[1]);

	unlink(lockfile);
	free(lockfile);
	close(lockfd);

	start_cfg();

	status_prompt_load_history();

	server_add_accept(0);

	set_signals(server_signal_callback);
	server_loop();
	status_prompt_save_history();
	exit(0);
}

/* Main server loop. */
void
server_loop(void)
{
	while (!server_should_exit()) {
		log_debug("event dispatch enter");
		event_loop(EVLOOP_ONCE);
		log_debug("event dispatch exit");

		server_client_loop();
	}
}

/* Check if the server should exit (no more clients or sessions). */
int
server_should_exit(void)
{
	struct client	*c;

	if (!options_get_number(&global_options, "exit-unattached")) {
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

	return (1);
}

/* Exit the server by killing all clients and windows. */
void
server_send_exit(void)
{
	struct client	*c, *c1;
	struct session	*s, *s1;

	cmd_wait_for_flush();

	TAILQ_FOREACH_SAFE(c, &clients, entry, c1) {
		if (c->flags & (CLIENT_BAD|CLIENT_SUSPENDED))
			server_client_lost(c);
		else
			server_write_client(c, MSG_SHUTDOWN, NULL, 0);
		c->session = NULL;
	}

	RB_FOREACH_SAFE(s, sessions, &sessions, s1)
		session_destroy(s);
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
		if (!(s->flags & SESSION_UNATTACHED)) {
			n++;
			break;
		}
	}

	if (n != last) {
		last = n;

		if (stat(socket_path, &sb) != 0)
			return;
		mode = sb.st_mode;
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
void
server_accept_callback(int fd, short events, unused void *data)
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

	if (event_initialized(&server_ev_accept))
		event_del(&server_ev_accept);

	if (timeout == 0) {
		event_set(&server_ev_accept,
		    server_fd, EV_READ, server_accept_callback, NULL);
		event_add(&server_ev_accept, NULL);
	} else {
		event_set(&server_ev_accept,
		    server_fd, EV_TIMEOUT, server_accept_callback, NULL);
		event_add(&server_ev_accept, &tv);
	}
}

/* Signal handler. */
void
server_signal_callback(int sig, unused short events, unused void *data)
{
	int	fd;

	switch (sig) {
	case SIGTERM:
		server_exit = 1;
		server_send_exit();
		break;
	case SIGCHLD:
		server_child_signal();
		break;
	case SIGUSR1:
		event_del(&server_ev_accept);
		fd = server_create_socket();
		if (fd != -1) {
			close(server_fd);
			server_fd = fd;
			server_update_socket();
		}
		server_add_accept(0);
		break;
	}
}

/* Handle SIGCHLD. */
void
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
void
server_child_exited(pid_t pid, int status)
{
	struct window		*w, *w1;
	struct window_pane	*wp;
	struct job		*job;

	RB_FOREACH_SAFE(w, windows, &windows, w1) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->pid == pid) {
				wp->status = status;
				server_destroy_pane(wp);
				break;
			}
		}
	}

	LIST_FOREACH(job, &all_jobs, lentry) {
		if (pid == job->pid) {
			job_died(job, status);	/* might free job */
			break;
		}
	}
}

/* Handle stopped children. */
void
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
}
