/* $Id$ */

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
#include <syslog.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Main server functions.
 */

/* Client list. */
struct clients	 clients;
struct clients	 dead_clients;

int		 server_fd;
int		 server_shutdown;
struct event	 server_ev_accept;
struct event	 server_ev_second;

struct paste_stack global_buffers;

int		 server_create_socket(void);
void		 server_loop(void);
int		 server_should_shutdown(void);
void		 server_send_shutdown(void);
void		 server_clean_dead(void);
void		 server_accept_callback(int, short, void *);
void		 server_signal_callback(int, short, void *);
void		 server_child_signal(void);
void		 server_child_exited(pid_t, int);
void		 server_child_stopped(pid_t, int);
void		 server_second_callback(int, short, void *);
void		 server_lock_server(void);
void		 server_lock_sessions(void);

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
		fatal("socket failed");
	}
	unlink(sa.sun_path);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("socket failed");

	mask = umask(S_IXUSR|S_IXGRP|S_IRWXO);
	if (bind(fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1)
		fatal("bind failed");
	umask(mask);

	if (listen(fd, 16) == -1)
		fatal("listen failed");
	setblocking(fd, 0);

	server_update_socket();

	return (fd);
}

/* Fork new server. */
int
server_start(void)
{
	struct window_pane	*wp;
	int	 		 pair[2];
	char			*cause;
	struct timeval		 tv;
	u_int			 i;

	/* The first client is special and gets a socketpair; create it. */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pair) != 0)
		fatal("socketpair failed");

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

	/*
	 * Must daemonise before loading configuration as the PID changes so
	 * $TMUX would be wrong for sessions created in the config file.
	 */
	if (daemon(1, 0) != 0)
		fatal("daemon failed");

	/* event_init() was called in our parent, need to reinit. */
	if (event_reinit(ev_base) != 0)
		fatal("event_reinit failed");
	clear_signals(0);

	logfile("server");
	log_debug("server started, pid %ld", (long) getpid());

	ARRAY_INIT(&windows);
	RB_INIT(&all_window_panes);
	ARRAY_INIT(&clients);
	ARRAY_INIT(&dead_clients);
	RB_INIT(&sessions);
	RB_INIT(&dead_sessions);
	TAILQ_INIT(&session_groups);
	ARRAY_INIT(&global_buffers);
	mode_key_init_trees();
	key_bindings_init();
	utf8_build();

	start_time = time(NULL);
	log_debug("socket path %s", socket_path);
#ifdef HAVE_SETPROCTITLE
	setproctitle("server (%s)", socket_path);
#endif

	server_fd = server_create_socket();
	server_client_create(pair[1]);

	if (access(SYSTEM_CFG, R_OK) == 0)
		load_cfg(SYSTEM_CFG, NULL, &cfg_causes);
	else if (errno != ENOENT) {
		cfg_add_cause(
		    &cfg_causes, "%s: %s", strerror(errno), SYSTEM_CFG);
	}
	if (cfg_file != NULL)
		load_cfg(cfg_file, NULL, &cfg_causes);

	/*
	 * If there is a session already, put the current window and pane into
	 * more mode.
	 */
	if (!RB_EMPTY(&sessions) && !ARRAY_EMPTY(&cfg_causes)) {
		wp = RB_MIN(sessions, &sessions)->curw->window->active;
		window_pane_set_mode(wp, &window_copy_mode);
		window_copy_init_for_output(wp);
		for (i = 0; i < ARRAY_LENGTH(&cfg_causes); i++) {
			cause = ARRAY_ITEM(&cfg_causes, i);
			window_copy_add(wp, "%s", cause);
			xfree(cause);
		}
		ARRAY_FREE(&cfg_causes);
	}
	cfg_finished = 1;

	event_set(&server_ev_accept,
	    server_fd, EV_READ|EV_PERSIST, server_accept_callback, NULL);
	event_add(&server_ev_accept, NULL);

	memset(&tv, 0, sizeof tv);
	tv.tv_sec = 1;
	evtimer_set(&server_ev_second, server_second_callback, NULL);
	evtimer_add(&server_ev_second, &tv);

	set_signals(server_signal_callback);
	server_loop();
	exit(0);
}

/* Main server loop. */
void
server_loop(void)
{
	while (!server_should_shutdown()) {
		event_loop(EVLOOP_ONCE);

		server_window_loop();
		server_client_loop();

		key_bindings_clean();
		server_clean_dead();
	}
}

/* Check if the server should be shutting down (no more clients or sessions). */
int
server_should_shutdown(void)
{
	u_int	i;

	if (!options_get_number(&global_options, "exit-unattached")) {
		if (!RB_EMPTY(&sessions))
			return (0);
	}
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) != NULL)
			return (0);
	}
	return (1);
}

/* Shutdown the server by killing all clients and windows. */
void
server_send_shutdown(void)
{
	struct client	*c;
	struct session	*s, *next_s;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL) {
			if (c->flags & (CLIENT_BAD|CLIENT_SUSPENDED))
				server_client_lost(c);
			else
				server_write_client(c, MSG_SHUTDOWN, NULL, 0);
			c->session = NULL;
		}
	}

	s = RB_MIN(sessions, &sessions);
	while (s != NULL) {
		next_s = RB_NEXT(sessions, &sessions, s);
		session_destroy(s);
		s = next_s;
	}
}

/* Free dead, unreferenced clients and sessions. */
void
server_clean_dead(void)
{
	struct session	*s, *next_s;
	struct client	*c;
	u_int		 i;

	s = RB_MIN(sessions, &dead_sessions);
	while (s != NULL) {
		next_s = RB_NEXT(sessions, &dead_sessions, s);
		if (s->references == 0) {
			RB_REMOVE(sessions, &dead_sessions, s);
			xfree(s->name);
			xfree(s);
		}
		s = next_s;
	}

	for (i = 0; i < ARRAY_LENGTH(&dead_clients); i++) {
		c = ARRAY_ITEM(&dead_clients, i);
		if (c == NULL || c->references != 0)
			continue;
		ARRAY_SET(&dead_clients, i, NULL);
		xfree(c);
	}
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
/* ARGSUSED */
void
server_accept_callback(int fd, short events, unused void *data)
{
	struct sockaddr_storage	sa;
	socklen_t		slen = sizeof sa;
	int			newfd;

	if (!(events & EV_READ))
		return;

	newfd = accept(fd, (struct sockaddr *) &sa, &slen);
	if (newfd == -1) {
		if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED)
			return;
		fatal("accept failed");
	}
	if (server_shutdown) {
		close(newfd);
		return;
	}
	server_client_create(newfd);
}

/* Signal handler. */
/* ARGSUSED */
void
server_signal_callback(int sig, unused short events, unused void *data)
{
	switch (sig) {
	case SIGTERM:
		server_shutdown = 1;
		server_send_shutdown();
		break;
	case SIGCHLD:
		server_child_signal();
		break;
	case SIGUSR1:
		event_del(&server_ev_accept);
		close(server_fd);
		server_fd = server_create_socket();
		event_set(&server_ev_accept, server_fd,
		    EV_READ|EV_PERSIST, server_accept_callback, NULL);
		event_add(&server_ev_accept, NULL);
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
	struct window		*w;
	struct window_pane	*wp;
	struct job		*job;
	u_int		 	 i;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if ((w = ARRAY_ITEM(&windows, i)) == NULL)
			continue;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->pid == pid) {
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
	u_int			 i;

	if (WSTOPSIG(status) == SIGTTIN || WSTOPSIG(status) == SIGTTOU)
		return;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if ((w = ARRAY_ITEM(&windows, i)) == NULL)
			continue;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->pid == pid) {
				if (killpg(pid, SIGCONT) != 0)
					kill(pid, SIGCONT);
			}
		}
	}
}

/* Handle once-per-second timer events. */
/* ARGSUSED */
void
server_second_callback(unused int fd, unused short events, unused void *arg)
{
	struct window		*w;
	struct window_pane	*wp;
	struct timeval		 tv;
	u_int		 	 i;

	if (options_get_number(&global_s_options, "lock-server"))
		server_lock_server();
	else
		server_lock_sessions();

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->mode != NULL && wp->mode->timer != NULL)
				wp->mode->timer(wp);
		}
	}

	server_client_status_timer();

	evtimer_del(&server_ev_second);
	memset(&tv, 0, sizeof tv);
	tv.tv_sec = 1;
	evtimer_add(&server_ev_second, &tv);
}

/* Lock the server if ALL sessions have hit the time limit. */
void
server_lock_server(void)
{
	struct session  *s;
	int		 timeout;
	time_t           t;

	t = time(NULL);
	RB_FOREACH(s, sessions, &sessions) {
		if (s->flags & SESSION_UNATTACHED)
			continue;
		timeout = options_get_number(&s->options, "lock-after-time");
		if (timeout <= 0 || t <= s->activity_time.tv_sec + timeout)
			return;	/* not timed out */
	}

	server_lock();
	recalculate_sizes();
}

/* Lock any sessions which have timed out. */
void
server_lock_sessions(void)
{
	struct session  *s;
	int		 timeout;
	time_t		 t;

	t = time(NULL);
	RB_FOREACH(s, sessions, &sessions) {
		if (s->flags & SESSION_UNATTACHED)
			continue;
		timeout = options_get_number(&s->options, "lock-after-time");
		if (timeout > 0 && t > s->activity_time.tv_sec + timeout) {
			server_lock_session(s);
			recalculate_sizes();
		}
	}
}
