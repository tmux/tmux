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
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tmux.h"

void	client_handle_winch(struct client_ctx *);

int
client_init(char *path, struct client_ctx *cctx, int start_server, int flags)
{
	struct sockaddr_un		sa;
	struct stat			sb;
	struct msg_identify_data	data;
	struct winsize			ws;
	size_t				size;
	int				mode;
	struct buffer		       *b;
	char			       *name;

	if (lstat(path, &sb) != 0) {
		if (start_server && errno == ENOENT) {
			if ((cctx->srv_fd = server_start(path)) == -1)
				goto start_failed;
			goto server_started;
		}
		goto not_found;
	}
	if (!S_ISSOCK(sb.st_mode)) {
		errno = ENOTSOCK;
		goto not_found;
	}

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		goto not_found;
	}

	if ((cctx->srv_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("socket");

	if (connect(
	    cctx->srv_fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1) {
		if (errno == ECONNREFUSED) {
			if (unlink(path) != 0 || !start_server)
				goto not_found;
			if ((cctx->srv_fd = server_start(path)) == -1)
				goto start_failed;
			goto server_started;
		}
		goto not_found;
	}

server_started:
	if ((mode = fcntl(cctx->srv_fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(cctx->srv_fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");
	cctx->srv_in = buffer_create(BUFSIZ);
	cctx->srv_out = buffer_create(BUFSIZ);

	if (isatty(STDIN_FILENO)) {
		if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
			fatal("ioctl(TIOCGWINSZ)");
		data.version = PROTOCOL_VERSION;
		data.flags = flags;
		data.sx = ws.ws_col;
		data.sy = ws.ws_row;
		*data.tty = '\0';
		if (getcwd(data.cwd, sizeof data.cwd) == NULL)
			*data.cwd = '\0';

		if ((name = ttyname(STDIN_FILENO)) == NULL)
			fatal("ttyname failed");
		if (strlcpy(data.tty, name, sizeof data.tty) >= sizeof data.tty)
			fatalx("ttyname failed");

		b = buffer_create(BUFSIZ);
		cmd_send_string(b, getenv("TERM"));
		client_write_server2(cctx, MSG_IDENTIFY,
		    &data, sizeof data, BUFFER_OUT(b), BUFFER_USED(b));
		buffer_destroy(b);
	}

	return (0);

start_failed:
	log_warnx("server failed to start");
	return (1);

not_found:
	log_warn("server not found");
	return (1);
}

int
client_main(struct client_ctx *cctx)
{
	struct pollfd	 pfd;
	char		*error;
	int		 xtimeout; /* Yay for ncurses namespace! */

	siginit();

	logfile("client");
	setproctitle("client");

	error = NULL;
	xtimeout = INFTIM;
	while (!sigterm) {
		if (sigchld) {
			waitpid(WAIT_ANY, NULL, WNOHANG);
			sigchld = 0;
		}
		if (sigwinch)
			client_handle_winch(cctx);
		if (sigcont) {
			siginit();
			client_write_server(cctx, MSG_WAKEUP, NULL, 0);
			sigcont = 0;
		}

		switch (client_msg_dispatch(cctx, &error)) {
		case -1:
			goto out;
		case 0:
			/* May be more in buffer, don't let poll block. */
			xtimeout = 0;
			break;
		default:
			/* Out of data, poll may block. */
			xtimeout = INFTIM;
			break;
		}

		pfd.fd = cctx->srv_fd;
		pfd.events = POLLIN;
		if (BUFFER_USED(cctx->srv_out) > 0)
			pfd.events |= POLLOUT;

		if (poll(&pfd, 1, xtimeout) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		if (buffer_poll(&pfd, cctx->srv_in, cctx->srv_out) != 0)
			goto server_dead;
	}

out:
 	if (sigterm) {
 		printf("[terminated]\n");
 		return (1);
 	}

	if (cctx->flags & CCTX_SHUTDOWN) {
		printf("[server exited]\n");
		return (0);
	}

	if (cctx->flags & CCTX_EXIT) {
		printf("[exited]\n");
		return (0);
	}

	if (cctx->flags & CCTX_DETACH) {
		printf("[detached]\n");
		return (0);
	}

	printf("[error: %s]\n", error);
	return (1);

server_dead:
	printf("[lost server]\n");
	return (0);
}

void
client_handle_winch(struct client_ctx *cctx)
{
	struct msg_resize_data	data;
	struct winsize		ws;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		fatal("ioctl failed");

	data.sx = ws.ws_col;
	data.sy = ws.ws_row;
	client_write_server(cctx, MSG_RESIZE, &data, sizeof data);

	sigwinch = 0;
}
