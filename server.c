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
#include <paths.h>
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
struct event	 server_ev_sigterm;
struct event	 server_ev_sigusr1;
struct event	 server_ev_sigchld;
struct event	 server_ev_second;

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
	int			fd, mode;

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

	mask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if (bind(fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1)
		fatal("bind failed");
	umask(mask);

	if (listen(fd, 16) == -1)
		fatal("listen failed");

	if ((mode = fcntl(fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");

	server_update_socket();

	return (fd);
}

/* Fork new server. */
int
server_start(char *path)
{
	struct window_pane	*wp;
	int	 		 pair[2];
	char			 rpathbuf[MAXPATHLEN], *cause;
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

	logfile("server");
	log_debug("server started, pid %ld", (long) getpid());

	ARRAY_INIT(&windows);
	ARRAY_INIT(&clients);
	ARRAY_INIT(&dead_clients);
	ARRAY_INIT(&sessions);
	ARRAY_INIT(&dead_sessions);
	TAILQ_INIT(&session_groups);
	mode_key_init_trees();
	key_bindings_init();
	utf8_build();

	start_time = time(NULL);
	socket_path = path;

	if (realpath(socket_path, rpathbuf) == NULL)
		strlcpy(rpathbuf, socket_path, sizeof rpathbuf);
	log_debug("socket path %s", socket_path);
	setproctitle("server (%s)", rpathbuf);

	event_init();

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
	if (!ARRAY_EMPTY(&sessions) && !ARRAY_EMPTY(&cfg_causes)) {
		wp = ARRAY_FIRST(&sessions)->curw->window->active;
		window_pane_set_mode(wp, &window_more_mode);
		for (i = 0; i < ARRAY_LENGTH(&cfg_causes); i++) {
			cause = ARRAY_ITEM(&cfg_causes, i);
			window_more_add(wp, "%s", cause);
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

	server_signal_set();
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

/* Check if the server should be shutting down (no more clients or windows). */
int
server_should_shutdown(void)
{
	u_int	i;

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if (ARRAY_ITEM(&sessions, i) != NULL)
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
	struct session	*s;
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

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if ((s = ARRAY_ITEM(&sessions, i)) != NULL)
			session_destroy(s);
	}
}

/* Free dead, unreferenced clients and sessions. */
void
server_clean_dead(void)
{
	struct session	*s;
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&dead_sessions); i++) {
		s = ARRAY_ITEM(&dead_sessions, i);
		if (s == NULL || s->references != 0)
			continue;
		ARRAY_SET(&dead_sessions, i, NULL);
		xfree(s);
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
	u_int		 i;
	static int	 last = -1;
	int		 n;

	n = 0;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s != NULL && !(s->flags & SESSION_UNATTACHED)) {
			n++;
			break;
		}
	}

	if (n != last) {
		last = n;
		if (n != 0)
			chmod(socket_path, S_IRWXU);
		else
			chmod(socket_path, S_IRUSR|S_IWUSR);
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

/* Set up server signal handling. */
void
server_signal_set(void)
{
	struct sigaction	 sigact;

	memset(&sigact, 0, sizeof sigact);
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_IGN;
	if (sigaction(SIGINT, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGPIPE, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTSTP, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGHUP, &sigact, NULL) != 0)
		fatal("sigaction failed");

	signal_set(&server_ev_sigchld, SIGCHLD, server_signal_callback, NULL);
	signal_add(&server_ev_sigchld, NULL);
	signal_set(&server_ev_sigterm, SIGTERM, server_signal_callback, NULL);
	signal_add(&server_ev_sigterm, NULL);
	signal_set(&server_ev_sigusr1, SIGUSR1, server_signal_callback, NULL);
	signal_add(&server_ev_sigusr1, NULL);
}

/* Destroy server signal events. */
void
server_signal_clear(void)
{
	struct sigaction	 sigact;

	memset(&sigact, 0, sizeof sigact);
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGPIPE, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTSTP, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGHUP, &sigact, NULL) != 0)
		fatal("sigaction failed");

	signal_del(&server_ev_sigchld);
	signal_del(&server_ev_sigterm);
	signal_del(&server_ev_sigusr1);
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

	SLIST_FOREACH(job, &all_jobs, lentry) {
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
	u_int            i;
	int		 timeout;
	time_t           t;

	t = time(NULL);
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if ((s = ARRAY_ITEM(&sessions, i)) == NULL)
			continue;

		if (s->flags & SESSION_UNATTACHED) {
			if (gettimeofday(&s->activity_time, NULL) != 0)
				fatal("gettimeofday failed");
			continue;
		}

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
	u_int		 i;
	int		 timeout;
	time_t		 t;

	t = time(NULL);
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if ((s = ARRAY_ITEM(&sessions, i)) == NULL)
			continue;

		if (s->flags & SESSION_UNATTACHED) {
			if (gettimeofday(&s->activity_time, NULL) != 0)
				fatal("gettimeofday failed");
			continue;
		}

		timeout = options_get_number(&s->options, "lock-after-time");
		if (timeout > 0 && t > s->activity_time.tv_sec + timeout) {
			server_lock_session(s);
			recalculate_sizes();
		}
	}
}
