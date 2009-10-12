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

/* Mapping of a pollfd to an fd independent of its position in the array. */
struct poll_item {
	struct pollfd	 pfd;

	RB_ENTRY(poll_item) entry;
};
RB_HEAD(poll_items, poll_item) poll_items;

int		 server_poll_cmp(struct poll_item *, struct poll_item *);
struct pollfd	*server_poll_lookup(int);
void		 server_poll_add(int, int);
struct pollfd	*server_poll_flatten(int *);
void		 server_poll_parse(struct pollfd *);
void		 server_poll_reset(void);
RB_PROTOTYPE(poll_items, poll_item, entry, server_poll_cmp);
RB_GENERATE(poll_items, poll_item, entry, server_poll_cmp);

void		 server_create_client(int);
int		 server_create_socket(void);
int		 server_main(int);
void		 server_shutdown(void);
int		 server_should_shutdown(void);
void		 server_child_signal(void);
void		 server_fill_windows(void);
void		 server_handle_windows(void);
void		 server_fill_clients(void);
void		 server_handle_clients(void);
void		 server_fill_jobs(void);
void		 server_handle_jobs(void);
void		 server_accept_client(int);
void		 server_handle_client(struct client *);
void		 server_handle_window(struct window *, struct window_pane *);
int		 server_check_window_bell(struct session *, struct window *);
int		 server_check_window_activity(struct session *,
		      struct window *);
int		 server_check_window_content(struct session *, struct window *,
		      struct window_pane *);
void		 server_clean_dead(void);
void		 server_lost_client(struct client *);
void	 	 server_check_window(struct window *);
void		 server_check_redraw(struct client *);
void		 server_set_title(struct client *);
void		 server_check_timers(struct client *);
void		 server_check_jobs(void);
void		 server_lock_server(void);
void		 server_lock_sessions(void);
void		 server_check_clients(void);
void		 server_second_timers(void);
int		 server_update_socket(void);

int
server_poll_cmp(struct poll_item *pitem1, struct poll_item *pitem2)
{
	return (pitem1->pfd.fd - pitem2->pfd.fd);
}

struct pollfd *
server_poll_lookup(int fd)
{
	struct poll_item	pitem;

	pitem.pfd.fd = fd;
	return (&RB_FIND(poll_items, &poll_items, &pitem)->pfd);
}

void
server_poll_add(int fd, int events)
{
	struct poll_item	*pitem;

	pitem = xmalloc(sizeof *pitem);
	pitem->pfd.fd = fd;
	pitem->pfd.events = events;
	RB_INSERT(poll_items, &poll_items, pitem);
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
		pfds[*nfds].fd = pitem->pfd.fd;
		pfds[*nfds].events = pitem->pfd.events;
		(*nfds)++;
	}
	return (pfds);
}

void
server_poll_parse(struct pollfd *pfds)
{
	struct poll_item	*pitem;
	int			 nfds;

	nfds = 0;
	RB_FOREACH(pitem, poll_items, &poll_items) {
		pitem->pfd.revents = pfds[nfds].revents;
		nfds++;
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

/* Create a new client. */
void
server_create_client(int fd)
{
	struct client	*c;
	int		 mode;
	u_int		 i;

	if ((mode = fcntl(fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");

	c = xcalloc(1, sizeof *c);
	c->references = 0;
	imsg_init(&c->ibuf, fd);
	
	if (gettimeofday(&c->tv, NULL) != 0)
		fatal("gettimeofday failed");

	ARRAY_INIT(&c->prompt_hdata);

	c->tty.fd = -1;
	c->title = NULL;

	c->session = NULL;
	c->tty.sx = 80;
	c->tty.sy = 24;

	screen_init(&c->status, c->tty.sx, 1, 0);
	job_tree_init(&c->status_jobs);

	c->message_string = NULL;

	c->prompt_string = NULL;
	c->prompt_buffer = NULL;
	c->prompt_index = 0;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == NULL) {
			ARRAY_SET(&clients, i, c);
			return;
		}
	}
	ARRAY_ADD(&clients, c);
	log_debug("new client %d", fd);
}

/* Fork new server. */
int
server_start(char *path)
{
	struct client	*c;
	int		 pair[2], srv_fd;
	char		*cause;
	char		 rpathbuf[MAXPATHLEN];

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

	srv_fd = server_create_socket();
	server_create_client(pair[1]);

	if (access(SYSTEM_CFG, R_OK) != 0) {
		if (errno != ENOENT) {
			xasprintf(
			    &cause, "%s: %s", strerror(errno), SYSTEM_CFG);
			goto error;
		}
	} else if (load_cfg(SYSTEM_CFG, NULL, &cause) != 0)
		goto error;
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

/* Main server loop. */
int
server_main(int srv_fd)
{
	struct pollfd	*pfds, *pfd;
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
			server_child_signal();
			sigchld = 0;
		}

		/* Recreate socket on SIGUSR1. */
		if (sigusr1) {
			close(srv_fd);
			srv_fd = server_create_socket();
			sigusr1 = 0;
		}

		/* Collect any jobs that have died and process clients. */
		server_check_jobs();
		server_check_clients();

		/* Initialise pollfd array and add server socket. */
		server_poll_reset();
		server_poll_add(srv_fd, POLLIN);

		/* Fill window and client sockets. */
		server_fill_jobs();
		server_fill_windows();
		server_fill_clients();

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
		server_poll_parse(pfds);

		/* Handle server socket. */
		pfd = server_poll_lookup(srv_fd);
		if (pfd->revents & (POLLERR|POLLNVAL|POLLHUP))
			fatalx("lost server socket");
		if (pfd->revents & POLLIN) {
			server_accept_client(srv_fd);
			continue;
		}

		/* Call second-based timers. */
		now = time(NULL);
		if (now != last) {
			last = now;
			server_second_timers();
		}

		/* Set window names. */
		set_window_names();

		/* Handle window and client sockets. */
		server_handle_jobs();
		server_handle_windows();
		server_handle_clients();

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
			server_lost_client(ARRAY_ITEM(&clients, i));
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
				server_lost_client(c);
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

/* Fill window pollfds. */
void
server_fill_windows(void)
{
	struct window		*w;
	struct window_pane	*wp;
	u_int		 	 i;
	int			 events;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		TAILQ_FOREACH(wp, &w->panes, entry) {	
			if (wp->fd == -1)
				continue;
			events = POLLIN;
			if (BUFFER_USED(wp->out) > 0)
				events |= POLLOUT;
			server_poll_add(wp->fd, events);

			if (wp->pipe_fd == -1)
				continue;
			events = 0;
			if (BUFFER_USED(wp->pipe_buf) > 0)
				events |= POLLOUT;
			server_poll_add(wp->pipe_fd, events);
		}
	}
}

/* Handle window pollfds. */
void
server_handle_windows(void)
{
	struct window		*w;
	struct window_pane	*wp;
	struct pollfd		*pfd;
	u_int		 	 i;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd == -1)
				continue;
			if ((pfd = server_poll_lookup(wp->fd)) == NULL)
				continue;
			if (buffer_poll(pfd, wp->in, wp->out) != 0) {
				close(wp->fd);
				wp->fd = -1;
			} else
				server_handle_window(w, wp);

			if (wp->pipe_fd == -1)
				continue;
			if ((pfd = server_poll_lookup(wp->pipe_fd)) == NULL)
				continue;
			if (buffer_poll(pfd, NULL, wp->pipe_buf) != 0) {
				buffer_destroy(wp->pipe_buf);
				close(wp->pipe_fd);
				wp->pipe_fd = -1;
			}
		}

		server_check_window(w);
	}
}

/* Check clients for redraw and timers. */
void
server_check_clients(void)
{
	struct client		*c;
	struct window		*w;
	struct window_pane	*wp;
	u_int		 	 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		server_check_timers(c);
		server_check_redraw(c);
	}

	/*
	 * Clear any window redraw flags (will have been redrawn as part of
	 * client).
	 */
	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		w->flags &= ~WINDOW_REDRAW;
		TAILQ_FOREACH(wp, &w->panes, entry)
			wp->flags &= ~PANE_REDRAW;
	}
}

/* Check for general redraw on client. */
void
server_check_redraw(struct client *c)
{
	struct session		*s = c->session;
	struct window_pane	*wp;
	int		 	 flags, redraw;

	flags = c->tty.flags & TTY_FREEZE;
	c->tty.flags &= ~TTY_FREEZE;

	if (c->flags & (CLIENT_REDRAW|CLIENT_STATUS)) {
		if (options_get_number(&s->options, "set-titles"))
			server_set_title(c);
	
		if (c->message_string != NULL)
			redraw = status_message_redraw(c);
		else if (c->prompt_string != NULL)
			redraw = status_prompt_redraw(c);
		else
			redraw = status_redraw(c);
		if (!redraw)
			c->flags &= ~CLIENT_STATUS;
	}

	if (c->flags & CLIENT_REDRAW) {
		screen_redraw_screen(c, 0);
		c->flags &= ~CLIENT_STATUS;
	} else {
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry) {
			if (wp->flags & PANE_REDRAW)
				screen_redraw_pane(c, wp);
		}
	}

	if (c->flags & CLIENT_STATUS)
		screen_redraw_screen(c, 1);

	c->tty.flags |= flags;

	c->flags &= ~(CLIENT_REDRAW|CLIENT_STATUS);
}

/* Set client title. */
void
server_set_title(struct client *c)
{
	struct session	*s = c->session;
	const char	*template;
	char		*title;

	template = options_get_string(&s->options, "set-titles-string");
	
	title = status_replace(c, template, time(NULL));
	if (c->title == NULL || strcmp(title, c->title) != 0) {
		if (c->title != NULL)
			xfree(c->title);
		c->title = xstrdup(title);
		tty_set_title(&c->tty, c->title);
	}
	xfree(title);
}

/* Check for timers on client. */
void
server_check_timers(struct client *c)
{
	struct session	*s = c->session;
	struct job	*job;
	struct timeval	 tv;
	u_int		 interval;

	if (gettimeofday(&tv, NULL) != 0)
		fatal("gettimeofday failed");

	if (c->flags & CLIENT_IDENTIFY && timercmp(&tv, &c->identify_timer, >))
		server_clear_identify(c);

	if (c->message_string != NULL && timercmp(&tv, &c->message_timer, >))
		status_message_clear(c);

	if (c->message_string != NULL || c->prompt_string != NULL) {
		/*
		 * Don't need timed redraw for messages/prompts so bail now.
		 * The status timer isn't reset when they are redrawn anyway.
		 */
		return;
	}
	if (!options_get_number(&s->options, "status"))
		return;

	/* Check timer; resolution is only a second so don't be too clever. */
	interval = options_get_number(&s->options, "status-interval");
	if (interval == 0)
		return;
	if (tv.tv_sec < c->status_timer.tv_sec ||
	    ((u_int) tv.tv_sec) - c->status_timer.tv_sec >= interval) {
		/* Run the jobs for this client and schedule for redraw. */
		RB_FOREACH(job, jobs, &c->status_jobs)
			job_run(job);
		c->flags |= CLIENT_STATUS;
	}
}

/* Fill client pollfds. */
void
server_fill_clients(void)
{
	struct client	*c;
	u_int		 i;
	int		 events;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);

		if (c != NULL) {
			events = 0;
			if (!(c->flags & CLIENT_BAD))
				events |= POLLIN;
			if (c->ibuf.w.queued > 0)
				events |= POLLOUT;
			server_poll_add(c->ibuf.fd, events);
		}

		if (c != NULL && !(c->flags & CLIENT_SUSPENDED) &&
		    c->tty.fd != -1 && c->session != NULL) {
			events = POLLIN;
			if (BUFFER_USED(c->tty.out) > 0)
				events |= POLLOUT;
			server_poll_add(c->tty.fd, events);
		}
	}
}

/* Fill in job fds. */
void
server_fill_jobs(void)
{
	struct job	*job;

	SLIST_FOREACH(job, &all_jobs, lentry) {
		if (job->fd == -1)
			continue;
		server_poll_add(job->fd, POLLIN);
	}
}

/* Handle job fds. */
void
server_handle_jobs(void)
{
	struct job	*job;
	struct pollfd	*pfd;

	SLIST_FOREACH(job, &all_jobs, lentry) {
		if (job->fd == -1)
			continue;
		if ((pfd = server_poll_lookup(job->fd)) == NULL)
			continue;
		if (buffer_poll(pfd, job->out, NULL) != 0) {
			close(job->fd);
			job->fd = -1;
		}
	}
}

/* Handle job fds. */
void
server_check_jobs(void)
{
	struct job	*job;
	
restart:
	SLIST_FOREACH(job, &all_jobs, lentry) {
		if (job->flags & JOB_DONE || job->fd != -1 || job->pid != -1)
			continue;
		job->flags |= JOB_DONE;

		if (job->callbackfn != NULL) {
			job->callbackfn(job);
			goto restart;	/* could be freed by callback */
		}
	}
}

/* Handle client pollfds. */
void
server_handle_clients(void)
{
	struct client	*c;
	struct pollfd	*pfd;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);

		if (c != NULL) {
			if ((pfd = server_poll_lookup(c->ibuf.fd)) == NULL)
				continue;
			if (pfd->revents & (POLLERR|POLLNVAL|POLLHUP)) {
				server_lost_client(c);
				continue;
			}

			if (pfd->revents & POLLOUT) {
				if (msgbuf_write(&c->ibuf.w) < 0) {
					server_lost_client(c);
					continue;
				}
			}

			if (c->flags & CLIENT_BAD) {
				if (c->ibuf.w.queued == 0)
					server_lost_client(c);
				continue;
			} else if (pfd->revents & POLLIN) {
				if (server_msg_dispatch(c) != 0) {
					server_lost_client(c);
					continue;
				}
			}
		}

		if (c != NULL && !(c->flags & CLIENT_SUSPENDED) &&
		    c->tty.fd != -1 && c->session != NULL) {
			if ((pfd = server_poll_lookup(c->tty.fd)) == NULL)
				continue;
			if (buffer_poll(pfd, c->tty.in, c->tty.out) != 0)
				server_lost_client(c);
			else
				server_handle_client(c);
		}
	}
}

/* accept(2) and create new client. */
void
server_accept_client(int srv_fd)
{
	struct sockaddr_storage	sa;
	socklen_t		slen = sizeof sa;
	int			fd;

	fd = accept(srv_fd, (struct sockaddr *) &sa, &slen);
	if (fd == -1) {
		if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED)
			return;
		fatal("accept failed");
	}
	if (sigterm) {
		close(fd);
		return;
	}
	server_create_client(fd);
}

/* Input data from client. */
void
server_handle_client(struct client *c)
{
	struct window		*w;
	struct window_pane	*wp;
	struct screen		*s;
	struct options		*oo;
	struct timeval	 	 tv;
	struct key_binding	*bd;
	struct keylist		*keylist;
	struct mouse_event	 mouse;
	int		 	 key, status, xtimeout, mode, isprefix;
	u_int			 i;

	xtimeout = options_get_number(&c->session->options, "repeat-time");
	if (xtimeout != 0 && c->flags & CLIENT_REPEAT) {
		if (gettimeofday(&tv, NULL) != 0)
			fatal("gettimeofday failed");
		if (timercmp(&tv, &c->repeat_timer, >))
			c->flags &= ~(CLIENT_PREFIX|CLIENT_REPEAT);
	}

	/* Process keys. */
	keylist = options_get_data(&c->session->options, "prefix");
	while (tty_keys_next(&c->tty, &key, &mouse) == 0) {
		if (c->session == NULL)
			return;

		c->session->activity = time(NULL);
		w = c->session->curw->window;
		wp = w->active;	/* could die */
		oo = &c->session->options;

		/* Special case: number keys jump to pane in identify mode. */
		if (c->flags & CLIENT_IDENTIFY && key >= '0' && key <= '9') {	
			wp = window_pane_at_index(w, key - '0');
			if (wp != NULL && window_pane_visible(wp))
				window_set_active_pane(w, wp);
			server_clear_identify(c);
			continue;
		}
		
		status_message_clear(c);
		server_clear_identify(c);
		if (c->prompt_string != NULL) {
			status_prompt_key(c, key);
			continue;
		}

		/* Check for mouse keys. */
		if (key == KEYC_MOUSE) {
			if (options_get_number(oo, "mouse-select-pane")) {
				window_set_active_at(w, mouse.x, mouse.y);
				wp = w->active;
			}
			window_pane_mouse(wp, c, &mouse);
			continue;
		}

		/* Is this a prefix key? */
		isprefix = 0;
		for (i = 0; i < ARRAY_LENGTH(keylist); i++) {
			if (key == ARRAY_ITEM(keylist, i)) {
				isprefix = 1;
				break;
			}
		}

		/* No previous prefix key. */
		if (!(c->flags & CLIENT_PREFIX)) {
			if (isprefix)
				c->flags |= CLIENT_PREFIX;
			else {
				/* Try as a non-prefix key binding. */
				if ((bd = key_bindings_lookup(key)) == NULL)
					window_pane_key(wp, c, key);
				else
					key_bindings_dispatch(bd, c);
			}
			continue;
		}

		/* Prefix key already pressed. Reset prefix and lookup key. */
		c->flags &= ~CLIENT_PREFIX;
		if ((bd = key_bindings_lookup(key | KEYC_PREFIX)) == NULL) {
			/* If repeating, treat this as a key, else ignore. */
			if (c->flags & CLIENT_REPEAT) {
				c->flags &= ~CLIENT_REPEAT;
				if (isprefix)
					c->flags |= CLIENT_PREFIX;
				else
					window_pane_key(wp, c, key);
			}
			continue;
		}

		/* If already repeating, but this key can't repeat, skip it. */
		if (c->flags & CLIENT_REPEAT && !bd->can_repeat) {
			c->flags &= ~CLIENT_REPEAT;
			if (isprefix)
				c->flags |= CLIENT_PREFIX;
			else
				window_pane_key(wp, c, key);
			continue;
		}

		/* If this key can repeat, reset the repeat flags and timer. */
		if (xtimeout != 0 && bd->can_repeat) {
			c->flags |= CLIENT_PREFIX|CLIENT_REPEAT;

			tv.tv_sec = xtimeout / 1000;
			tv.tv_usec = (xtimeout % 1000) * 1000L;
			if (gettimeofday(&c->repeat_timer, NULL) != 0)
				fatal("gettimeofday failed");
			timeradd(&c->repeat_timer, &tv, &c->repeat_timer);
		}

		/* Dispatch the command. */
		key_bindings_dispatch(bd, c);
	}
	if (c->session == NULL)
		return;
	w = c->session->curw->window;
	wp = w->active;
	oo = &c->session->options;
	s = wp->screen;

	/*
	 * Update cursor position and mode settings. The scroll region and
	 * attributes are cleared across poll(2) as this is the most likely
	 * time a user may interrupt tmux, for example with ~^Z in ssh(1). This
	 * is a compromise between excessive resets and likelihood of an
	 * interrupt.
	 *
	 * tty_region/tty_reset/tty_update_mode already take care of not
	 * resetting things that are already in their default state.
	 */
	tty_region(&c->tty, 0, c->tty.sy - 1);

	status = options_get_number(oo, "status");
	if (!window_pane_visible(wp) || wp->yoff + s->cy >= c->tty.sy - status)
		tty_cursor(&c->tty, 0, 0);
	else
		tty_cursor(&c->tty, wp->xoff + s->cx, wp->yoff + s->cy);

	mode = s->mode;
	if (TAILQ_NEXT(TAILQ_FIRST(&w->panes), entry) != NULL &&
	    options_get_number(oo, "mouse-select-pane"))
		mode |= MODE_MOUSE;
	tty_update_mode(&c->tty, mode);
	tty_reset(&c->tty);
}

/* Lost a client. */
void
server_lost_client(struct client *c)
{
	u_int	i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == c)
			ARRAY_SET(&clients, i, NULL);
	}
	log_debug("lost client %d", c->ibuf.fd);

	/*
	 * If CLIENT_TERMINAL hasn't been set, then tty_init hasn't been called
	 * and tty_free might close an unrelated fd.
	 */
	if (c->flags & CLIENT_TERMINAL)
		tty_free(&c->tty);

	screen_free(&c->status);
	job_tree_free(&c->status_jobs);

	if (c->title != NULL)
		xfree(c->title);

	if (c->message_string != NULL)
		xfree(c->message_string);

	if (c->prompt_string != NULL)
		xfree(c->prompt_string);
	if (c->prompt_buffer != NULL)
		xfree(c->prompt_buffer);
	for (i = 0; i < ARRAY_LENGTH(&c->prompt_hdata); i++)
		xfree(ARRAY_ITEM(&c->prompt_hdata, i));
	ARRAY_FREE(&c->prompt_hdata);

	if (c->cwd != NULL)
		xfree(c->cwd);

	close(c->ibuf.fd);
	imsg_clear(&c->ibuf);

	for (i = 0; i < ARRAY_LENGTH(&dead_clients); i++) {
		if (ARRAY_ITEM(&dead_clients, i) == NULL) {
			ARRAY_SET(&dead_clients, i, c);
			break;
		}
	}
	if (i == ARRAY_LENGTH(&dead_clients))
		ARRAY_ADD(&dead_clients, c);
	c->flags |= CLIENT_DEAD;

	recalculate_sizes();
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

/* Handle window data. */
void
server_handle_window(struct window *w, struct window_pane *wp)
{
	struct session	*s;
	u_int		 i;
	int		 update;

	window_pane_parse(wp);

	if ((w->flags & (WINDOW_BELL|WINDOW_ACTIVITY|WINDOW_CONTENT)) == 0)
		return;

	update = 0;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL || !session_has(s, w))
			continue;

		update += server_check_window_bell(s, w);
		update += server_check_window_activity(s, w);
		update += server_check_window_content(s, w, wp);
	}
	if (update)
		server_status_window(w);

	w->flags &= ~(WINDOW_BELL|WINDOW_ACTIVITY|WINDOW_CONTENT);
}

int
server_check_window_bell(struct session *s, struct window *w)
{
	struct client	*c;
	u_int		 i;
	int		 action, visual;

	if (!(w->flags & WINDOW_BELL))
		return (0);
	if (session_alert_has_window(s, w, WINDOW_BELL))
		return (0);
	session_alert_add(s, w, WINDOW_BELL);

	action = options_get_number(&s->options, "bell-action");
	switch (action) {
	case BELL_ANY:
		if (s->flags & SESSION_UNATTACHED)
			break;
		visual = options_get_number(&s->options, "visual-bell");
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
			if (!visual) {
				tty_putcode(&c->tty, TTYC_BEL);
				continue;
			}
 			if (c->session->curw->window == w) {
				status_message_set(c, "Bell in current window");
				continue;
			}
			status_message_set(c, "Bell in window %u",
			    winlink_find_by_window(&s->windows, w)->idx);
		}
		break;
	case BELL_CURRENT:
		if (s->flags & SESSION_UNATTACHED)
			break;
		visual = options_get_number(&s->options, "visual-bell");
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
 			if (c->session->curw->window != w)
				continue;
			if (!visual) {
				tty_putcode(&c->tty, TTYC_BEL);
				continue;
			}
			status_message_set(c, "Bell in current window");
		}
		break;
	}
	return (1);
}

int
server_check_window_activity(struct session *s, struct window *w)
{
	struct client	*c;
	u_int		 i;

	if (!(w->flags & WINDOW_ACTIVITY))
		return (0);

	if (!options_get_number(&w->options, "monitor-activity"))
		return (0);

	if (session_alert_has_window(s, w, WINDOW_ACTIVITY))
		return (0);
	if (s->curw->window == w)
		return (0);

	session_alert_add(s, w, WINDOW_ACTIVITY);
	if (s->flags & SESSION_UNATTACHED)
		return (0);
 	if (options_get_number(&s->options, "visual-activity")) {
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
			status_message_set(c, "Activity in window %u",
			    winlink_find_by_window(&s->windows, w)->idx);
		}
	}

	return (1);
}

int
server_check_window_content(
    struct session *s, struct window *w, struct window_pane *wp)
{
	struct client	*c;
	u_int		 i;
	char		*found, *ptr;
	
	if (!(w->flags & WINDOW_ACTIVITY))	/* activity for new content */
		return (0);

	ptr = options_get_string(&w->options, "monitor-content");
	if (ptr == NULL || *ptr == '\0')
		return (0);

	if (session_alert_has_window(s, w, WINDOW_CONTENT))
		return (0);
	if (s->curw->window == w)
		return (0);

	if ((found = window_pane_search(wp, ptr, NULL)) == NULL)
		return (0);
    	xfree(found);

	session_alert_add(s, w, WINDOW_CONTENT);
	if (s->flags & SESSION_UNATTACHED)
		return (0);
 	if (options_get_number(&s->options, "visual-content")) {
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
			status_message_set(c, "Content in window %u",
			    winlink_find_by_window(&s->windows, w)->idx);
		}
	}

	return (1);
}

/* Check if window still exists. */
void
server_check_window(struct window *w)
{
	struct window_pane	*wp, *wq;
	struct options		*oo = &w->options;
	struct session		*s;
	struct winlink		*wl;
	u_int		 	 i;
	int		 	 destroyed;

	destroyed = 1;

	wp = TAILQ_FIRST(&w->panes);
	while (wp != NULL) {
		wq = TAILQ_NEXT(wp, entry);
		/*
		 * If the pane has died and the remain-on-exit flag is not set,
		 * remove the pane; otherwise, if the flag is set, don't allow
		 * the window to be destroyed (or it'll close when the last
		 * pane dies).
		 */
		if (wp->fd == -1 && !options_get_number(oo, "remain-on-exit")) {
			layout_close_pane(wp);
			window_remove_pane(w, wp);
			server_redraw_window(w);
		} else 
			destroyed = 0;
		wp = wq;
	}

	if (!destroyed)
		return;

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;
		if (!session_has(s, w))
			continue;

	restart:
		/* Detach window and either redraw or kill clients. */
		RB_FOREACH(wl, winlinks, &s->windows) {
			if (wl->window != w)
				continue;
			if (session_detach(s, wl)) {
				server_destroy_session_group(s);
				break;
			}
			server_redraw_session(s);
			server_status_session_group(s);
			goto restart;
		}
	}

	recalculate_sizes();
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

		timeout = options_get_number(&s->options, "lock-after-time");
		if (timeout <= 0 || t <= s->activity + timeout)
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
        u_int            i;
	int		 timeout;
        time_t           t;

        t = time(NULL);
        for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if ((s = ARRAY_ITEM(&sessions, i)) == NULL)
			continue;

		timeout = options_get_number(&s->options, "lock-after-time");
		if (timeout > 0 && t > s->activity + timeout) {
			server_lock_session(s);
			recalculate_sizes();
		}
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
