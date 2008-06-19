/* $Id: server.c,v 1.70 2008-06-19 19:40:34 nicm Exp $ */

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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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

int		 server_main(const char *, int);
void		 server_fill_windows(struct pollfd **);
void		 server_handle_windows(struct pollfd **);
void		 server_fill_clients(struct pollfd **);
void		 server_handle_clients(struct pollfd **);
struct client	*server_accept_client(int);
void		 server_handle_client(struct client *);
void		 server_handle_window(struct window *);
void		 server_lost_client(struct client *);
void	 	 server_lost_window(struct window *);
void		 server_check_redraw(struct client *);
void		 server_check_timers(struct client *);
void		 server_update_socket(const char *);

/* Fork new server. */
int
server_start(const char *path)
{
	struct sockaddr_un	sa;
	size_t			size;
	mode_t			mask;
	int		   	n, fd, pair[2], mode;
	char		       *cause;
	u_char			ch;

	/* Make a little socketpair to wait for the server to be ready. */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pair) != 0)
		fatal("socketpair failed");

	switch (fork()) {
	case -1:
		fatal("fork failed");
	case 0:
		break;
	default:
		close(pair[1]);

		ch = 0x00;
		if (read(pair[0], &ch, 1) == 1 && ch == 0xff) {
			close(pair[0]);
			return (0);
		}
		ch = 0x00;
		if (write(pair[1], &ch, 1) != 1)
			fatal("write failed");
		close(pair[0]);
		return (1);
	}
	close(pair[0]);

#ifdef DEBUG
	xmalloc_clear();
#endif

	/*
	 * Must daemonise before loading configuration as the PID changes so
	 * $TMUX would be wrong for sessions created in the config file.
	 */
	if (daemon(1, 1) != 0)
		fatal("daemon failed");

	ARRAY_INIT(&windows);
	ARRAY_INIT(&clients);
	ARRAY_INIT(&sessions);
	key_bindings_init();

	if (cfg_file != NULL && load_cfg(cfg_file, &cause) != 0) {
		log_warnx("%s", cause);
		exit(1);
	}

	logfile("server");
#ifndef NO_SETPROCTITLE
	setproctitle("server (%s)", path);
#endif
	log_debug("server started, pid %ld", (long) getpid());

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
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

	ch = 0xff;
	if (write(pair[1], &ch, 1) != 1)
		fatal("write failed");
	read(pair[1], &ch, 1); /* Ignore errors; just to wait before closing. */
	close(pair[1]);

	n = server_main(path, fd);
#ifdef DEBUG
	xmalloc_report(getpid(), "server");
#endif
	exit(n);
}

/* Main server loop. */
int
server_main(const char *srv_path, int srv_fd)
{
	struct pollfd	*pfds, *pfd;
	int		 nfds;
	u_int		 i, n;

	siginit();

	pfds = NULL;
	while (!sigterm) {
		/* Initialise pollfd array. */
		nfds = 1 + ARRAY_LENGTH(&windows) + ARRAY_LENGTH(&clients) * 2;
		pfds = xrealloc(pfds, nfds, sizeof *pfds);
		pfd = pfds;

		/* Fill server socket. */
		pfd->fd = srv_fd;
		pfd->events = POLLIN;
		pfd++;

		/* Fill window and client sockets. */
		server_fill_windows(&pfd);
		server_fill_clients(&pfd);

		/* Do the poll. */
		log_debug("polling %d fds", nfds);
		if ((nfds = poll(pfds, nfds, 500)) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}
		pfd = pfds;
		log_debug("poll returned %d", nfds);

		/* Handle server socket. */
		if (pfd->revents & (POLLERR|POLLNVAL|POLLHUP))
			fatalx("lost server socket");
		if (pfd->revents & POLLIN) {
			server_accept_client(srv_fd);
			continue;
		}
		pfd++;

		/*
		 * Handle window and client sockets. Clients can create
		 * windows, so windows must come first to avoid messing up by
		 * increasing the array size.
		 */
		server_handle_windows(&pfd);
		server_handle_clients(&pfd);

		/* Update socket permissions. */
		server_update_socket(srv_path);

		/*
		 * If we have no sessions and clients left, let's get out
		 * of here...
		 */
		n = 0;
		for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
			if (ARRAY_ITEM(&sessions, i) != NULL)
				n++;
		}
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			if (ARRAY_ITEM(&clients, i) != NULL)
				n++;
		}
		if (n == 0)
			break;
	}
	if (pfds != NULL)
		xfree(pfds);

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

	key_bindings_free();

	close(srv_fd);
	unlink(srv_path);

	return (0);
}

/* Fill window pollfds. */
void
server_fill_windows(struct pollfd **pfd)
{
	struct window	*w;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if ((w = ARRAY_ITEM(&windows, i)) == NULL)
			(*pfd)->fd = -1;
		else {
			(*pfd)->fd = w->fd;
			(*pfd)->events = POLLIN;
			if (BUFFER_USED(w->out) > 0)
				(*pfd)->events |= POLLOUT;
			log_debug("adding window %d (%d)", (*pfd)->fd, w->fd);
		}
		(*pfd)++;
	}
}

/* Handle window pollfds. */
void
server_handle_windows(struct pollfd **pfd)
{
	struct window	*w;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if ((w = ARRAY_ITEM(&windows, i)) != NULL) {
			log_debug("testing window %d (%d)", (*pfd)->fd, w->fd);
			if (buffer_poll(*pfd, w->in, w->out) != 0)
				server_lost_window(w);
			else
				server_handle_window(w);
		}
		(*pfd)++;
	}
}

/* Check for general redraw on client. */
void
server_check_redraw(struct client *c)
{
	struct session		       *s;
	struct screen_redraw_ctx	ctx;
	struct screen			screen;
	u_int				xx, yy, sx, sy;
	char			        title[BUFSIZ];

	if (c == NULL || c->session == NULL)
		return;
	s = c->session;

	if (options_get_number(&s->options, "set-titles")) {
		xsnprintf(title, sizeof title,
		    "%s:%u:%s - \"%s\"", s->name, s->curw->idx,
		    s->curw->window->name, s->curw->window->base.title);
		if (c->title == NULL || strcmp(title, c->title) != 0) {
			if (c->title != NULL)
				xfree(c->title);
			c->title = xstrdup(title);
			tty_set_title(&c->tty, c->title);
		}
	}

	xx = c->sx;
	yy = c->sy - 1; //options_get_number(&s->options, "status-lines");
	if (c->flags & CLIENT_REDRAW) {
		sx = screen_size_x(s->curw->window->screen);
		sy = screen_size_y(s->curw->window->screen);
		if (sx < xx || sy < yy) {
			/*
			 * Fake up a blank(ish) screen and use it to draw the
			 * empty regions. NOTE: because this uses
			 * tty_write_client but doesn't write the client's
			 * screen, this can't use anything which relies on
			 * cursor position.
			 */
			screen_create(&screen, xx, yy, 0);
			screen_redraw_start(&ctx, &screen, tty_write_client, c);
			if (sx < xx)
				screen_redraw_columns(&ctx, sx, xx - sx);
			if (sy < yy)  {
				screen_fill_area(&screen,
				    0, sy, xx, 1, '-', 0, 0x70);
				screen_redraw_lines(&ctx, sy, yy - sy);
			}
			screen_redraw_stop(&ctx);
			screen_destroy(&screen);
		}

		screen_redraw_start_client(&ctx, c);
		screen_redraw_lines(&ctx, 0, screen_size_y(ctx.s));
		screen_redraw_stop(&ctx);

		c->flags |= CLIENT_STATUS;
	}

	if (c->flags & CLIENT_STATUS) {
		if (c->message_string != NULL)
			status_message_redraw(c);
		else if (c->prompt_string != NULL)
			status_prompt_redraw(c);
		else
			status_redraw(c);
	}

	c->flags &= ~(CLIENT_REDRAW|CLIENT_STATUS);
}

/* Check for timers on client. */
void
server_check_timers(struct client *c)
{
	struct session	*s;
	struct timespec	 ts, ts2;
	u_int		 nlines, interval;

	if (c == NULL || c->session == NULL)
		return;
	s = c->session;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		fatal("clock_gettime");
	
	if (c->message_string != NULL && timespeccmp(&ts, &c->message_timer, >))
		server_clear_client_message(c);

	nlines = options_get_number(&s->options, "status-lines");
	if (nlines == 0)
		return;
	interval = options_get_number(&s->options, "status-interval");
	if (interval == 0)
		return;

	memcpy(&ts2, &ts, sizeof ts2);
	ts2.tv_sec -= interval;
	if (timespeccmp(&c->status_timer, &ts2, <))
		c->flags |= CLIENT_STATUS;
}

/* Fill client pollfds. */
void
server_fill_clients(struct pollfd **pfd)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);

		server_check_timers(c);
		server_check_redraw(c);

		if (c == NULL)
			(*pfd)->fd = -1;
		else {
			(*pfd)->fd = c->fd;
			(*pfd)->events = POLLIN;
			if (BUFFER_USED(c->out) > 0)
				(*pfd)->events |= POLLOUT;
			log_debug("adding client %d (%d)", (*pfd)->fd, c->fd);
		}
		(*pfd)++;

		if (c == NULL || c->tty.fd == -1 || c->session == NULL)
			(*pfd)->fd = -1;
		else {
			(*pfd)->fd = c->tty.fd;
			(*pfd)->events = POLLIN;
			if (BUFFER_USED(c->tty.out) > 0)
				(*pfd)->events |= POLLOUT;
			log_debug("adding tty %d (%d)", (*pfd)->fd, c->tty.fd);
		}
		(*pfd)++;
	}
}

/* Handle client pollfds. */
void
server_handle_clients(struct pollfd **pfd)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);

		if (c != NULL) {
			log_debug("testing client %d (%d)", (*pfd)->fd, c->fd);
			if (buffer_poll(*pfd, c->in, c->out) != 0) {
				server_lost_client(c);
				(*pfd) += 2;
				continue;
			} else
				server_msg_dispatch(c);
		}
		(*pfd)++;

		if (c != NULL && c->tty.fd != -1 && c->session != NULL) {
			log_debug("testing tty %d (%d)", (*pfd)->fd, c->tty.fd);
			if (buffer_poll(*pfd, c->tty.in, c->tty.out) != 0)
				server_lost_client(c);
			else
				server_handle_client(c);
		}
		(*pfd)++;
	}
}

/* accept(2) and create new client. */
struct client *
server_accept_client(int srv_fd)
{
	struct client	       *c;
	struct sockaddr_storage	sa;
	socklen_t		slen = sizeof sa;
	int		 	client_fd, mode;
	u_int			i;

	client_fd = accept(srv_fd, (struct sockaddr *) &sa, &slen);
	if (client_fd == -1) {
		if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED)
			return (NULL);
		fatal("accept failed");
	}
	if ((mode = fcntl(client_fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(client_fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");

	c = xcalloc(1, sizeof *c);
	c->fd = client_fd;
	c->in = buffer_create(BUFSIZ);
	c->out = buffer_create(BUFSIZ);

	c->tty.fd = -1;
	c->title = NULL;

	c->session = NULL;
	c->sx = 80;
	c->sy = 25;

	c->message_string = NULL;

	c->prompt_string = NULL;
	c->prompt_buffer = NULL;
	c->prompt_index = 0;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == NULL) {
			ARRAY_SET(&clients, i, c);
			return (c);
		}
	}
	ARRAY_ADD(&clients, c);
	return (c);
}

/* Input data from client. */
void
server_handle_client(struct client *c)
{
	struct window	*w = c->session->curw->window;
	int		 key, prefix;

	prefix = options_get_key(&c->session->options, "prefix-key");
	while (tty_keys_next(&c->tty, &key) == 0) {
		server_clear_client_message(c);
		if (c->prompt_string != NULL) {
			status_prompt_key(c, key);
			continue;
		}

		if (c->flags & CLIENT_PREFIX) {
			key_bindings_dispatch(key, c);
			c->flags &= ~CLIENT_PREFIX;
			continue;
		} else if (key == prefix)
			c->flags |= CLIENT_PREFIX;
		else
			window_key(w, key);
	}
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

	tty_free(&c->tty);

	close(c->fd);
	buffer_destroy(c->in);
	buffer_destroy(c->out);
	xfree(c);

	recalculate_sizes();
}

/* Handle window data. */
void
server_handle_window(struct window *w)
{
	struct session	*s;
	u_int		 i;
	int		 action, update;

	window_parse(w);

	if (!(w->flags & WINDOW_BELL) && !(w->flags & WINDOW_ACTIVITY))
		return;

	update = 0;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL || !session_has(s, w))
			continue;

		if (w->flags & WINDOW_BELL &&
		    !session_alert_has_window(s, w, WINDOW_BELL)) {
			session_alert_add(s, w, WINDOW_BELL);

			action = options_get_number(&s->options, "bell-action");
			switch (action) {
			case BELL_ANY:
				tty_write_session(s, TTY_CHARACTER, '\007');
				break;
			case BELL_CURRENT:
				if (s->curw->window != w)
					break;
				tty_write_session(s, TTY_CHARACTER, '\007');
				break;
			}
			update = 1;
		}

		if ((w->flags & WINDOW_MONITOR) &&
		    (w->flags & WINDOW_ACTIVITY) &&
		    !session_alert_has_window(s, w, WINDOW_ACTIVITY)) {
			session_alert_add(s, w, WINDOW_ACTIVITY);
			update = 1;
		}
	}
	if (update)
		server_status_window(w);

	w->flags &= ~(WINDOW_BELL|WINDOW_ACTIVITY);
}

/* Lost window: move clients on to next window. */
void
server_lost_window(struct window *w)
{
	struct client	*c;
	struct session	*s;
	struct winlink	*wl;
	u_int		 i, j;
	int		 destroyed;

	log_debug("lost window %d", w->fd);

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
			destroyed = session_detach(s, wl);
			for (j = 0; j < ARRAY_LENGTH(&clients); j++) {
				c = ARRAY_ITEM(&clients, j);
				if (c == NULL || c->session != s)
					continue;
				if (!destroyed) {
					server_redraw_client(c);
					continue;
				}
				c->session = NULL;
				server_write_client(c, MSG_EXIT, NULL, 0);
			}
			/* If the session was destroyed, bail now. */
			if (destroyed)
				break;
			goto restart;
		}
	}

	recalculate_sizes();
}

void
server_update_socket(const char *path)
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
			chmod(path, S_IRWXU);
		else
			chmod(path, S_IRUSR|S_IWUSR);
	}
}
