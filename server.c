/* $Id: server.c,v 1.7 2007-08-27 15:28:07 nicm Exp $ */

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
#include <unistd.h>
#include <util.h>

#include "tmux.h"

/*
 * Main server functions.
 */

/* Client list. */
struct clients	 clients;

int		 server_main(int);
void		 fill_windows(struct pollfd **);
void		 handle_windows(struct pollfd **);
void		 fill_clients(struct pollfd **);
void		 handle_clients(struct pollfd **);
struct client	*accept_client(int);
void		 lost_client(struct client *);
void	 	 user_start(struct client *, const char *, const char *,
		     size_t, void (*)(struct client *, const char *));
void		 user_input(struct client *, size_t);
void		 write_message(struct client *, const char *, ...);
void		 write_client(struct client *, u_int, void *, size_t);
void		 write_client2(
		     struct client *, u_int, void *, size_t, void *, size_t);
void		 write_clients(struct window *, u_int, void *, size_t);
void		 new_window(struct window *);
void		 lost_window(struct window *);
void		 changed_window(struct client *);
void		 draw_client(struct client *, u_int, u_int);
void	 	 process_client(struct client *);
void		 process_new_msg(struct client *, struct hdr *);
void		 process_attach_msg(struct client *, struct hdr *);
void		 process_create_msg(struct client *, struct hdr *);
void		 process_next_msg(struct client *, struct hdr *);
void		 process_previous_msg(struct client *, struct hdr *);
void		 process_select_msg(struct client *, struct hdr *);
void		 process_size_msg(struct client *, struct hdr *);
void		 process_input_msg(struct client *, struct hdr *);
void		 process_refresh_msg(struct client *, struct hdr *);
void		 process_sessions_msg(struct client *, struct hdr *);
void		 process_windows_msg(struct client *, struct hdr *);
void		 process_rename_msg(struct client *, struct hdr *);
void		 rename_callback(struct client *, const char *);

/* Fork and start server process. */
int
server_start(void)
{
	mode_t			mode;
	int		   	fd;
	struct sockaddr_un	sa;
	size_t			sz;
	pid_t			pid;
	FILE		       *f;
	char		       *path;

	/* Fork the server process. */
	switch (pid = fork()) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		return (0);
	}

	/* Start logging to file. */
	if (debug_level > 0) {
		xasprintf(&path,
		    "%s-server-%ld.log", __progname, (long) getpid());
		f = fopen(path, "w");
		log_open(f, LOG_DAEMON, debug_level);
		xfree(path);
	}
	log_debug("server started, pid %ld", (long) getpid());

	/* Create the socket. */
	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	sz = strlcpy(sa.sun_path, socket_path, sizeof sa.sun_path);
	if (sz >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		fatal("socket failed");
	}
	unlink(sa.sun_path);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("socket failed");

	mode = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if (bind(fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1)
		fatal("bind failed");
	umask(mode);

	if (listen(fd, 16) == -1)
		fatal("listen failed");

	/*
	 * Detach into the background. This means the PID changes which will
	 * have to be fixed in some way at some point... XXX
	 */
	if (daemon(1, 1) != 0)
		fatal("daemon failed");
	log_debug("server daemonised, pid now %ld", (long) getpid());

	setproctitle("server (%s)", socket_path);
	exit(server_main(fd));
}

/* Main server loop. */
int
server_main(int srv_fd)
{
	struct pollfd  		*pfds, *pfd;
	int			 nfds, mode;
	struct sigaction	 act;

	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;

	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR1, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGINT, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGQUIT, &act, NULL) != 0)
		fatal("sigaction failed");

	ARRAY_INIT(&windows);
	ARRAY_INIT(&clients);
	ARRAY_INIT(&sessions);

	if ((mode = fcntl(srv_fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(srv_fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");

	pfds = NULL;
	while (!sigterm) {
		/* Initialise pollfd array. */
		nfds = 1 + ARRAY_LENGTH(&windows) + ARRAY_LENGTH(&clients);
		pfds = xrealloc(pfds, nfds, sizeof *pfds);
		pfd = pfds;

		/* Fill server socket. */
		pfd->fd = srv_fd;
		pfd->events = POLLIN;
		pfd++;

		/* Fill window and client sockets. */
		fill_windows(&pfd);
		fill_clients(&pfd);

		/* Do the poll. */
		if (poll(pfds, nfds, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}
		pfd = pfds;

		/* Handle server socket. */
		if (pfd->revents & (POLLERR|POLLNVAL|POLLHUP))
			fatalx("lost server socket");
		if (pfd->revents & POLLIN) {
			accept_client(srv_fd);
			continue;
		}
		pfd++;

		/*
		 * Handle window and client sockets. Clients can create
		 * windows, so windows must come first to avoid messing up by
		 * increasing the array size.
		 */
		handle_windows(&pfd);
		handle_clients(&pfd);
	}

	close(srv_fd);
	unlink(socket_path);

	return (0);
}

/* Fill window pollfds. */
void
fill_windows(struct pollfd **pfd)
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
		}
		(*pfd)++;
	}
}

/* Handle window pollfds. */
void
handle_windows(struct pollfd **pfd)
{
	struct window	*w;
	u_int		 i;
	struct buffer	*b;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if ((w = ARRAY_ITEM(&windows, i)) != NULL) {
			if (window_poll(w, *pfd) != 0)
				lost_window(w);
			else {
				b = buffer_create(BUFSIZ);
				window_output(w, b);
				if (BUFFER_USED(b) != 0) {
					write_clients(w, MSG_OUTPUT,
					    BUFFER_OUT(b), BUFFER_USED(b));
				}
				buffer_destroy(b);
			}
		}
		(*pfd)++;
	}
}

/* Fill client pollfds. */
void
fill_clients(struct pollfd **pfd)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if ((c = ARRAY_ITEM(&clients, i)) == NULL)
			(*pfd)->fd = -1;
		else {
			(*pfd)->fd = c->fd;
			(*pfd)->events = POLLIN;
			if (BUFFER_USED(c->out) > 0)
				(*pfd)->events |= POLLOUT;
		}
		(*pfd)++;
	}
}

/* Handle client pollfds. */
void
handle_clients(struct pollfd *(*pfd))
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if ((c = ARRAY_ITEM(&clients, i)) != NULL) {
			if (buffer_poll((*pfd), c->in, c->out) != 0)
				lost_client(c);
			else
				process_client(c);
		}
		(*pfd)++;
	}
}

/* accept(2) and create new client. */
struct client *
accept_client(int srv_fd)
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

	c = xmalloc(sizeof *c);
	c->fd = client_fd;
	c->in = buffer_create(BUFSIZ);
	c->out = buffer_create(BUFSIZ);
	c->session = NULL;
	c->prompt = NULL;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == NULL) {
			ARRAY_SET(&clients, i, c);
			return (c);
		}
	}
	ARRAY_ADD(&clients, c);
	return (c);
}

/* Lost a client. */
void
lost_client(struct client *c)
{
	u_int	i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == c)
			ARRAY_SET(&clients, i, NULL);
	}
	
	close(c->fd);
	buffer_destroy(c->in);
	buffer_destroy(c->out);
	xfree(c);
}

/* Write message command to a client. */
void
write_message(struct client *c, const char *fmt, ...)
{
	struct hdr	 hdr;
	va_list		 ap;
	char		*msg;
	size_t		 size;
	u_int		 i;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	input_store_zero(c->out, CODE_CURSOROFF);
	input_store_two(c->out, CODE_CURSORMOVE, c->sy, 1);
	input_store_one(c->out, CODE_ATTRIBUTES, 2);
	input_store16(c->out, 0);
	input_store16(c->out, 7);
	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	buffer_write(c->out, msg, strlen(msg));
	xfree(msg);
	va_end(ap);
	for (i = strlen(msg); i < c->sx; i++)
		input_store8(c->out, ' ');

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_OUTPUT;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);

	hdr.type = MSG_PAUSE;
	hdr.size = 0;
	buffer_write(c->out, &hdr, sizeof hdr);

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	screen_draw(&c->session->window->screen, c->out, c->sy - 1, c->sy - 1);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_OUTPUT;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
}

/* Start user input. */
void
user_start(struct client *c, const char *prompt, const char *now, 
    size_t len, void (*callback)(struct client *, const char *))
{
	struct hdr	 hdr;
	size_t		 size;
	u_int		 i;

	c->callback = callback;
	c->prompt = prompt;

	c->len = len;
	if (c->len > c->sx - strlen(c->prompt))
		c->len = c->sx - strlen(c->prompt);
	c->buf = xmalloc(c->len + 1);
	strlcpy(c->buf, now, c->len + 1);
	c->idx = strlen(c->buf);

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	input_store_zero(c->out, CODE_CURSOROFF);
	input_store_two(c->out, CODE_CURSORMOVE, c->sy, 1);
	input_store_one(c->out, CODE_ATTRIBUTES, 2);
	input_store16(c->out, 0);
	input_store16(c->out, 7);

	i = 0;
	buffer_write(c->out, c->prompt, strlen(c->prompt));
	i += strlen(c->prompt);
	if (*c->buf != '\0') {
		buffer_write(c->out, c->buf, strlen(c->buf));
		i += strlen(c->buf);
	}
	for (; i < c->sx; i++)
		input_store8(c->out, ' ');

	input_store_two(c->out,
	    CODE_CURSORMOVE, c->sy, 1 + strlen(c->prompt) + c->idx);
	input_store_zero(c->out, CODE_CURSORON);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_OUTPUT;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
}

/* Handle user input. */
void
user_input(struct client *c, size_t in)
{
	struct hdr	hdr;
	size_t		size;
	int		key;
	u_int		i;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	while (in != 0) {
		if (in < 1)
			break;
		in--;
		key = input_extract8(c->in);
		if (key == '\e') {
			if (in < 2)
				fatalx("underflow");
			in -= 2;
			key = (int16_t) input_extract16(c->in);
		}

	again:
		if (key == '\r') {
			screen_draw(&c->session->window->screen,
			    c->out, c->sy - 1, c->sy - 1);

			c->callback(c, c->buf);
			c->prompt = NULL;
			xfree(c->buf);
			break;
		}

		switch (key) {
		case KEYC_LEFT:
			if (c->idx > 0)
				c->idx--;
			input_store_two(c->out, CODE_CURSORMOVE,
			    c->sy, 1 + strlen(c->prompt) + c->idx);
			break;
		case KEYC_RIGHT:
			if (c->idx < strlen(c->buf))
				c->idx++;
			input_store_two(c->out, CODE_CURSORMOVE,
			    c->sy, 1 + strlen(c->prompt) + c->idx);
			break;
		case KEYC_HOME:
			c->idx = 0;
			input_store_two(c->out, CODE_CURSORMOVE,
			    c->sy, 1 + strlen(c->prompt) + c->idx);
			break;
		case KEYC_LL:
			c->idx = strlen(c->buf);
			input_store_two(c->out, CODE_CURSORMOVE,
			    c->sy, 1 + strlen(c->prompt) + c->idx);
			break;
		case KEYC_BACKSPACE:
			if (c->idx == 0)
				break;
			if (strlen(c->buf) == 0)
				break;
			if (c->idx == strlen(c->buf))
				c->buf[c->idx - 1] = '\0';
			else {
				memmove(c->buf + c->idx - 1,
				    c->buf + c->idx, c->len - c->idx);
			}
			c->idx--;
			input_store_one(c->out, CODE_CURSORLEFT, 1);
			input_store_one(c->out, CODE_DELETECHARACTER, 1);
			input_store_zero(c->out, CODE_CURSOROFF);
			input_store_two(c->out, CODE_CURSORMOVE, c->sy, c->sx);
			input_store8(c->out, ' ');
			input_store_two(c->out, CODE_CURSORMOVE,
			    c->sy, 1 + strlen(c->prompt) + c->idx);
			input_store_zero(c->out, CODE_CURSORON);
			break;
		case KEYC_DC:
			if (strlen(c->buf) == 0)
				break;
			if (c->idx == strlen(c->buf))
				break;
			memmove(c->buf + c->idx,
			    c->buf + c->idx + 1, c->len - c->idx - 1);
			input_store_one(c->out, CODE_DELETECHARACTER, 1);
			input_store_zero(c->out, CODE_CURSOROFF);
			input_store_two(c->out,CODE_CURSORMOVE, c->sy, c->sx);
			input_store8(c->out, ' ');
			input_store_two(c->out, CODE_CURSORMOVE,
			    c->sy, 1 + strlen(c->prompt) + c->idx);
			input_store_zero(c->out, CODE_CURSORON);
			break;
		default:
			if (key >= ' ' && key != '\177') {
				if (c->idx == c->len)
					break;
				if (strlen(c->buf) == c->len)
					break;
				memmove(c->buf + c->idx + 1,
				    c->buf + c->idx, c->len - c->idx);
				c->buf[c->idx++] = key;
				input_store_one(
				    c->out, CODE_INSERTCHARACTER, 1);
				input_store8(c->out, key);
				break;
			}
			switch (key) {
			case '\001':
				key = KEYC_HOME;
				goto again;
			case '\005':
				key = KEYC_LL;
				goto again;
			case '\013':
				c->buf[c->idx + 1] = '\0';
				input_store_zero(c->out, CODE_CURSOROFF);
				i = 1 + strlen(c->prompt) + c->idx;
				for (; i < c->sx; i++)
					input_store8(c->out, ' ');
				input_store_two(c->out, CODE_CURSORMOVE,
				    c->sy, 1 + strlen(c->prompt) + c->idx);
				input_store_zero(c->out, CODE_CURSORON);
				break;
			}
		}
	}

	size = BUFFER_USED(c->out) - size;
	if (size != 0) {
		hdr.type = MSG_OUTPUT;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	} else
		buffer_reverse_add(c->out, sizeof hdr);
}

/* Write command to a client. */
void
write_client(struct client *c, u_int cmd, void *buf, size_t len)
{
	struct hdr	 hdr;

	hdr.type = cmd;
	hdr.size = len;

	buffer_write(c->out, &hdr, sizeof hdr);
	if (buf != NULL)
		buffer_write(c->out, buf, len);
}

/* Write command to a client with two buffers. */
void
write_client2(struct client *c,
    u_int cmd, void *buf1, size_t len1, void *buf2, size_t len2)
{
	struct hdr	 hdr;

	hdr.type = cmd;
	hdr.size = len1 + len2;

	buffer_write(c->out, &hdr, sizeof hdr);
	if (buf1 != NULL)
		buffer_write(c->out, buf1, len1);
	if (buf2 != NULL)
		buffer_write(c->out, buf2, len2);
}

/* Write command to all clients attached to a specific window. */
void
write_clients(struct window *w, u_int cmd, void *buf, size_t len)
{
	struct client	*c;
 	struct hdr	 hdr;
	u_int		 i;

	hdr.type = cmd;
	hdr.size = len;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL) {
			if (c->session->window == w) {
				buffer_write(c->out, &hdr, sizeof hdr);
				if (buf != NULL)
					buffer_write(c->out, buf, len);
			}
		}
	}
}

/* Lost window: move clients on to next window. */
void
lost_window(struct window *w)
{
	struct client	*c;
	struct session	*s;
	u_int		 i, j;
	int		 destroyed;

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;
		if (!session_has(s, w))
			continue;

		/* Detach window from session. */
		session_detach(s, w);

		/* Try to flush session and either redraw or kill clients. */
		destroyed = session_flush(s);		
		for (j = 0; j < ARRAY_LENGTH(&clients); j++) {
			c = ARRAY_ITEM(&clients, j);
			if (c == NULL || c->session != s)
				continue;
			if (destroyed) {
				c->session = NULL;
				write_client(c, MSG_EXIT, NULL, 0);
			} else
				changed_window(c);
		}
	}
}

/* Changed client window. */
void
changed_window(struct client *c)
{
	struct window	*w;

	w = c->session->window;
	if (c->sx != w->screen.sx || c->sy != w->screen.sy)
		window_resize(w, c->sx, c->sy);
	draw_client(c, 0, c->sy - 1);
}

/* Draw window on client. */
void
draw_client(struct client *c, u_int py_upper, u_int py_lower)
{
	struct hdr	hdr;
	size_t		size;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	screen_draw(&c->session->window->screen, c->out, py_upper, py_lower);

	size = BUFFER_USED(c->out) - size;
	log_debug("redrawing screen, %zu bytes", size);
	if (size != 0) {
		hdr.type = MSG_OUTPUT;
		hdr.size = size;
		memcpy(
		    BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	} else
		buffer_reverse_add(c->out, sizeof hdr);
}

/* Process a command from the client. */
void
process_client(struct client *c)
{
	struct hdr	hdr;

	if (BUFFER_USED(c->in) < sizeof hdr)
		return;
	memcpy(&hdr, BUFFER_OUT(c->in), sizeof hdr);
	if (BUFFER_USED(c->in) < (sizeof hdr) + hdr.size)
		return;
	buffer_remove(c->in, sizeof hdr);

	switch (hdr.type) {
	case MSG_NEW:
		process_new_msg(c, &hdr);
		break;
	case MSG_ATTACH:
		process_attach_msg(c, &hdr);
		break;
	case MSG_CREATE:
		process_create_msg(c, &hdr);
		break;
	case MSG_NEXT:
		process_next_msg(c, &hdr);
		break;
	case MSG_PREVIOUS:
		process_previous_msg(c, &hdr);
		break;
	case MSG_SIZE:
		process_size_msg(c, &hdr);
		break;
	case MSG_INPUT:
		process_input_msg(c, &hdr);
		break;
	case MSG_REFRESH:
		process_refresh_msg(c, &hdr);
		break;
	case MSG_SELECT:
		process_select_msg(c, &hdr);
		break;
	case MSG_SESSIONS:
		process_sessions_msg(c, &hdr);
		break;
	case MSG_WINDOWS:
		process_windows_msg(c, &hdr);
		break;
	case MSG_RENAME:
		process_rename_msg(c, &hdr);
		break;
	default:
		fatalx("unexpected message");
	}
}

/* New message from client. */
void
process_new_msg(struct client *c, struct hdr *hdr)
{
	struct new_data	 data;
	const char      *shell;
	char		*cmd, *msg;
	
	if (c->session != NULL)
		return;
	if (hdr->size != sizeof data)
		fatalx("bad MSG_NEW size");
	buffer_read(c->in, &data, hdr->size);

	c->sx = data.sx;
	if (c->sx == 0)
		c->sx = 80;
	c->sy = data.sy;
	if (c->sy == 0)
		c->sy = 25;

	if (*data.name != '\0' && session_find(data.name) != NULL) {
		xasprintf(&msg, "duplicate session: %s", data.name);
		write_client(c, MSG_READY, msg, strlen(msg));
		xfree(msg);
		return;
	}

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = "/bin/ksh";
	xasprintf(&cmd, "%s -l", shell);
	c->session = session_create(data.name, cmd, c->sx, c->sy);
	if (c->session == NULL)
		fatalx("session_create failed");
	xfree(cmd);
	
	write_client(c, MSG_READY, NULL, 0);
	draw_client(c, 0, c->sy - 1);
}

/* Attach message from client. */
void
process_attach_msg(struct client *c, struct hdr *hdr)
{
	struct attach_data	 data;
	char			*msg;
	
	if (c->session != NULL)
		return;
	if (hdr->size != sizeof data)
		fatalx("bad MSG_ATTACH size");
	buffer_read(c->in, &data, hdr->size);

	c->sx = data.sx;
	if (c->sx == 0)
		c->sx = 80;
	c->sy = data.sy;
	if (c->sy == 0)
		c->sy = 25;

	if (*data.name != '\0')
		c->session = session_find(data.name);
	if (c->session == NULL) {
		xasprintf(&msg, "session not found: %s", data.name);
		write_client(c, MSG_READY, msg, strlen(msg));
		xfree(msg);
		return;
	}

	write_client(c, MSG_READY, NULL, 0);
	draw_client(c, 0, c->sy - 1);
}

/* Create message from client. */
void
process_create_msg(struct client *c, struct hdr *hdr)
{
	const char	*shell;
	char		*cmd;

	if (c->session == NULL)
		return;
	if (hdr->size != 0)
		fatalx("bad MSG_CREATE size");

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = "/bin/ksh";
	xasprintf(&cmd, "%s -l", shell);
	if (session_new(c->session, cmd, c->sx, c->sy) != 0)
		fatalx("session_new failed");
	xfree(cmd);

	draw_client(c, 0, c->sy - 1);
}

/* Next message from client. */
void
process_next_msg(struct client *c, struct hdr *hdr)
{
	if (c->session == NULL)
		return;
	if (hdr->size != 0)
		fatalx("bad MSG_NEXT size");

	if (session_next(c->session) == 0)
		changed_window(c);
	else
		write_message(c, "No next window"); 
}

/* Previous message from client. */
void
process_previous_msg(struct client *c, struct hdr *hdr)
{
	if (c->session == NULL)
		return;
	if (hdr->size != 0)
		fatalx("bad MSG_PREVIOUS size");

	if (session_previous(c->session) == 0)
		changed_window(c);
	else
		write_message(c, "No previous window"); 
}

/* Size message from client. */
void
process_size_msg(struct client *c, struct hdr *hdr)
{
	struct size_data	data;

	if (c->session == NULL)
		return;
	if (hdr->size != sizeof data)
		fatalx("bad MSG_SIZE size");
	buffer_read(c->in, &data, hdr->size);

	c->sx = data.sx;
	if (c->sx == 0)
		c->sx = 80;
	c->sy = data.sy;
	if (c->sy == 0)
		c->sy = 25;

	if (window_resize(c->session->window, c->sx, c->sy) != 0)
		draw_client(c, 0, c->sy - 1);
}

/* Input message from client. */
void
process_input_msg(struct client *c, struct hdr *hdr)
{
	if (c->session == NULL)
		return;

	if (c->prompt == NULL)
		window_input(c->session->window, c->in, hdr->size);
	else
		user_input(c, hdr->size);
}

/* Refresh message from client. */
void
process_refresh_msg(struct client *c, struct hdr *hdr)
{
	struct refresh_data	data;

	if (c->session == NULL)
		return;
	if (hdr->size != 0 && hdr->size != sizeof data)
		fatalx("bad MSG_REFRESH size");

	draw_client(c, 0, c->sy - 1);
}

/* Select message from client. */
void
process_select_msg(struct client *c, struct hdr *hdr)
{
	struct select_data	data;

	if (c->session == NULL)
		return;
	if (hdr->size != sizeof data)
		fatalx("bad MSG_SELECT size");
	buffer_read(c->in, &data, hdr->size);

	if (c->session == NULL)
		return;
	if (session_select(c->session, data.idx) == 0)
		changed_window(c);
	else
		write_message(c, "Window %u not present", data.idx); 
}

/* Sessions message from client. */
void
process_sessions_msg(struct client *c, struct hdr *hdr)
{
	struct sessions_data	 data;
	struct sessions_entry	 entry;
	struct session		*s;
	u_int			 i, j;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_SESSIONS size");
	buffer_read(c->in, &data, hdr->size);

	data.sessions = 0;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if (ARRAY_ITEM(&sessions, i) != NULL)
			data.sessions++;
	}
	write_client2(c, MSG_SESSIONS,
	    &data, sizeof data, NULL, data.sessions * sizeof entry);

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;
		strlcpy(entry.name, s->name, sizeof entry.name);
		entry.tim = s->tim;
		entry.windows = 0;
		for (j = 0; j < ARRAY_LENGTH(&s->windows); j++) {
			if (ARRAY_ITEM(&s->windows, j) != NULL)
				entry.windows++;
		}
		buffer_write(c->out, &entry, sizeof entry);
	}
}

/* Windows message from client. */
void
process_windows_msg(struct client *c, struct hdr *hdr)
{
	struct windows_data	 data;
	struct windows_entry	 entry;
	struct session		*s;
	struct window		*w;
	u_int			 i;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_WINDOWS size");
	buffer_read(c->in, &data, hdr->size);

	s = session_find(data.name);
	if (s == NULL) {
		data.windows = 0;
		write_client(c, MSG_WINDOWS, &data, sizeof data);
		return;
	}

	data.windows = 0;
	for (i = 0; i < ARRAY_LENGTH(&s->windows); i++) {
		if (ARRAY_ITEM(&windows, i) != NULL)
			data.windows++;
	}
	write_client2(c, MSG_WINDOWS,
	    &data, sizeof data, NULL, data.windows * sizeof entry);
	
	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;
		entry.idx = i;
		strlcpy(entry.name, w->name, sizeof entry.name);
		strlcpy(entry.title, w->screen.title, sizeof entry.title);
		if (ttyname_r(w->fd, entry.tty, sizeof entry.tty) != 0)
			*entry.tty = '\0';
		buffer_write(c->out, &entry, sizeof entry);
	}
}

/* Rename message from client. */
void
process_rename_msg(struct client *c, struct hdr *hdr)
{
	if (c->session == NULL)
		return;
	if (hdr->size != 0)
		fatalx("bad MSG_RENAME size");

	user_start(c, "Window name: ",
	    c->session->window->name, MAXNAMELEN, rename_callback);
}

/* Callback for rename. */
void
rename_callback(struct client *c, const char *string)
{
	strlcpy(
	    c->session->window->name, string, sizeof c->session->window->name);
}
