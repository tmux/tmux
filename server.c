/* $Id: server.c,v 1.216 2009-11-04 22:42:31 tcunha Exp $ */

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

/* Mapping of a pollfd to an fd independent of its position in the array. */
struct poll_item {
	int	 fd;
	int	 events;

	void	 (*fn)(int, int, void *);
	void	*data;

	RB_ENTRY(poll_item) entry;
};
RB_HEAD(poll_items, poll_item) poll_items;

int		 server_poll_cmp(struct poll_item *, struct poll_item *);
struct poll_item*server_poll_lookup(int);
struct pollfd	*server_poll_flatten(int *);
void		 server_poll_dispatch(struct pollfd *, int);
void		 server_poll_reset(void);
RB_PROTOTYPE(poll_items, poll_item, entry, server_poll_cmp);
RB_GENERATE(poll_items, poll_item, entry, server_poll_cmp);

int		 server_create_socket(void);
void		 server_callback(int, int, void *);
int		 server_main(int);
void		 server_shutdown(void);
int		 server_should_shutdown(void);
void		 server_child_signal(void);
void		 server_clean_dead(void);
void		 server_second_timers(void);
void		 server_lock_server(void);
void		 server_lock_sessions(void);
int		 server_update_socket(void);

int
server_poll_cmp(struct poll_item *pitem1, struct poll_item *pitem2)
{
	return (pitem1->fd - pitem2->fd);
}

void
server_poll_add(int fd, int events, void (*fn)(int, int, void *), void *data)
{
	struct poll_item	*pitem;

	pitem = xmalloc(sizeof *pitem);
	pitem->fd = fd;
	pitem->events = events;

	pitem->fn = fn;
	pitem->data = data;

	RB_INSERT(poll_items, &poll_items, pitem);
}

struct poll_item *
server_poll_lookup(int fd)
{
	struct poll_item	pitem;

	pitem.fd = fd;
	return (RB_FIND(poll_items, &poll_items, &pitem));
}

struct pollfd *
server_poll_flatten(int *nfds)
{
	struct poll_item	*pitem;
	struct pollfd		*pfds;

	pfds = NULL;
	*nfds = 0;
	RB_FOREACH(pitem, poll_items, &poll_items) {
		pfds = xrealloc(pfds, (*nfds) + 1, sizeof *pfds);
		pfds[*nfds].fd = pitem->fd;
		pfds[*nfds].events = pitem->events;
		(*nfds)++;
	}
	return (pfds);
}

void
server_poll_dispatch(struct pollfd *pfds, int nfds)
{
	struct poll_item	*pitem;
	struct pollfd		*pfd;

	while (nfds > 0) {
		pfd = &pfds[--nfds];
		if (pfd->revents != 0) {
			pitem = server_poll_lookup(pfd->fd);
			pitem->fn(pitem->fd, pfd->revents, pitem->data);
		}
	}
	xfree(pfds);
}

void
server_poll_reset(void)
{
	struct poll_item	*pitem;

	while (!RB_EMPTY(&poll_items)) {
		pitem = RB_ROOT(&poll_items);
		RB_REMOVE(poll_items, &poll_items, pitem);
		xfree(pitem);
	}
}

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

	return (fd);
}

/* Callback for server socket. */
void
server_callback(int fd, int events, unused void *data)
{
	struct sockaddr_storage	sa;
	socklen_t		slen = sizeof sa;
	int			newfd;

	if (events & (POLLERR|POLLNVAL|POLLHUP))
		fatalx("lost server socket");
	if (!(events & POLLIN))
		return;

	newfd = accept(fd, (struct sockaddr *) &sa, &slen);
	if (newfd == -1) {
		if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED)
			return;
		fatal("accept failed");
	}
	if (sigterm) {
		close(newfd);
		return;
	}
	server_client_create(newfd);
}

/* Fork new server. */
int
server_start(char *path)
{
	struct client	*c;
	int		 pair[2], srv_fd;
	char		*cause;
#ifdef HAVE_SETPROCTITLE
	char		 rpathbuf[MAXPATHLEN];
#endif

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

#ifdef HAVE_SETPROCTITLE
	if (realpath(socket_path, rpathbuf) == NULL)
		strlcpy(rpathbuf, socket_path, sizeof rpathbuf);
	log_debug("socket path %s", socket_path);
	setproctitle("server (%s)", rpathbuf);
#endif

	srv_fd = server_create_socket();
	server_client_create(pair[1]);

	if (access(SYSTEM_CFG, R_OK) == 0) {
		if (load_cfg(SYSTEM_CFG, NULL, &cause) != 0)
			goto error;
	} else if (errno != ENOENT) {
		xasprintf(&cause, "%s: %s", strerror(errno), SYSTEM_CFG);
		goto error;
	}
	if (cfg_file != NULL && load_cfg(cfg_file, NULL, &cause) != 0)
		goto error;

	exit(server_main(srv_fd));

error:
	/* Write the error and shutdown the server. */
	c = ARRAY_FIRST(&clients);

	server_write_error(c, cause);
	xfree(cause);

	sigterm = 1;
	server_shutdown();

	exit(server_main(srv_fd));
}

/* Main server loop. */
int
server_main(int srv_fd)
{
	struct pollfd	*pfds;
	int		 nfds, xtimeout;
	u_int		 i;
	time_t		 now, last;

	siginit();
	log_debug("server socket is %d", srv_fd);

	last = time(NULL);

	pfds = NULL;
	for (;;) {
		/* If sigterm, kill all windows and clients. */
		if (sigterm)
			server_shutdown();

		/* Stop if no sessions or clients left. */
		if (server_should_shutdown())
			break;

		/* Handle child exit. */
		if (sigchld) {
			sigchld = 0;
			server_child_signal();
			continue;
		}

		/* Recreate socket on SIGUSR1. */
		if (sigusr1) {
			sigusr1 = 0;
			close(srv_fd);
			srv_fd = server_create_socket();
			continue;
		}

		/* Initialise pollfd array and add server socket. */
		server_poll_reset();
		server_poll_add(srv_fd, POLLIN, server_callback, NULL);

		/* Fill window and client sockets. */
		server_job_prepare();
		server_window_prepare();
		server_client_prepare();
		
		/* Update socket permissions. */
		xtimeout = INFTIM;
		if (server_update_socket() != 0)
			xtimeout = POLL_TIMEOUT;

		/* Do the poll. */
		pfds = server_poll_flatten(&nfds);
		if (poll(pfds, nfds, xtimeout) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}
		server_poll_dispatch(pfds, nfds);

		/* Call second-based timers. */
		now = time(NULL);
		if (now != last) {
			last = now;
			server_second_timers();
		}

		/* Run once-per-loop events. */
		server_job_loop();
		server_window_loop();
		server_client_loop();

		/* Collect any unset key bindings. */
		key_bindings_clean();

		/* Collect dead clients and sessions. */
		server_clean_dead();
	}
	server_poll_reset();

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if (ARRAY_ITEM(&sessions, i) != NULL)
			session_destroy(ARRAY_ITEM(&sessions, i));
	}
	ARRAY_FREE(&sessions);

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) != NULL)
			server_client_lost(ARRAY_ITEM(&clients, i));
	}
	ARRAY_FREE(&clients);

	mode_key_free_trees();
	key_bindings_free();

	close(srv_fd);

	unlink(socket_path);
	xfree(socket_path);

	options_free(&global_s_options);
	options_free(&global_w_options);

	return (0);
}

/* Kill all clients. */
void
server_shutdown(void)
{
	struct session	*s;
	struct client	*c;
	u_int		 i, j;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL) {
			if (c->flags & (CLIENT_BAD|CLIENT_SUSPENDED))
				server_client_lost(c);
			else
				server_write_client(c, MSG_SHUTDOWN, NULL, 0);
		}
	}

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		for (j = 0; j < ARRAY_LENGTH(&clients); j++) {
			c = ARRAY_ITEM(&clients, j);
			if (c != NULL && c->session == s) {
				s = NULL;
				break;
			}
		}
		if (s != NULL)
			session_destroy(s);
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

/* Handle SIGCHLD. */
void
server_child_signal(void)
{
	struct window		*w;
	struct window_pane	*wp;
	struct job		*job;
	int		 	 status;
	pid_t		 	 pid;
	u_int		 	 i;

	for (;;) {
		switch (pid = waitpid(WAIT_ANY, &status, WNOHANG|WUNTRACED)) {
		case -1:
			if (errno == ECHILD)
				return;
			fatal("waitpid failed");
		case 0:
			return;
		}
		if (!WIFSTOPPED(status)) {
			SLIST_FOREACH(job, &all_jobs, lentry) {
				if (pid == job->pid) {
					job->pid = -1;
					job->status = status;
				}
			}
			continue;
		}
		if (WSTOPSIG(status) == SIGTTIN || WSTOPSIG(status) == SIGTTOU)
			continue;

		for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
			w = ARRAY_ITEM(&windows, i);
			if (w == NULL)
				continue;
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (wp->pid == pid) {
					if (killpg(pid, SIGCONT) != 0)
						kill(pid, SIGCONT);
				}
			}
		}
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

/* Call any once-per-second timers. */
void
server_second_timers(void)
{
	struct window		*w;
	struct window_pane	*wp;
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

/* Update socket execute permissions based on whether sessions are attached. */
int
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

	return (n);
}
