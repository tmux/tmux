/* $Id: client.c,v 1.23 2007-11-27 19:23:33 nicm Exp $ */

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
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tmux.h"

void	client_handle_winch(struct client_ctx *);

int
client_init(const char *path, struct client_ctx *cctx, int start_server)
{
	struct sockaddr_un		sa;
	struct stat			sb;
	struct msg_identify_data	data;
	struct winsize			ws;
	size_t				size;
	int				mode;
	u_int				retries;

	retries = 0;
retry:
	if (stat(path, &sb) != 0) {
		if (start_server && errno == ENOENT && retries < 10) {
			if (server_start(path) != 0)
				return (-1);
			usleep(10000);
			retries++;
			goto retry;
		}
		log_warn("%s: stat", path);
		return (-1);
	}
	if (!S_ISSOCK(sb.st_mode)) {
		log_warnx("%s: %s", path, strerror(ENOTSOCK));
		return (-1);
	}

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		log_warnx("%s: %s", path, strerror(ENAMETOOLONG));
		return (-1);
	}

	if ((cctx->srv_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("%s: socket", path);
		return (-1);
	}
	if (connect(
	    cctx->srv_fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1) {
		if (start_server && errno == ECONNREFUSED && retries < 10) {
			if (unlink(path) != 0) {
				log_warn("%s: unlink", path);
				return (-1);
			}
			usleep(10000);
			retries++;
			goto retry;
		}
		log_warn("%s: connect", path);
		return (-1);
	}

	if ((mode = fcntl(cctx->srv_fd, F_GETFL)) == -1) {
		log_warn("%s: fcntl", path);
		return (-1);
	}
	if (fcntl(cctx->srv_fd, F_SETFL, mode|O_NONBLOCK) == -1) {
		log_warn("%s: fcntl", path);
		return (-1);
	}
	cctx->srv_in = buffer_create(BUFSIZ);
	cctx->srv_out = buffer_create(BUFSIZ);

	if (isatty(STDIN_FILENO)) {
		if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
			log_warn("ioctl(TIOCGWINSZ)");
			return (-1);
		}

		data.sx = ws.ws_col;
		data.sy = ws.ws_row;
		if (ttyname_r(STDIN_FILENO, data.tty, sizeof data.tty) != 0)
			fatal("ttyname_r failed");
		client_write_server(cctx, MSG_IDENTIFY, &data, sizeof data);
		cmd_send_string(cctx->srv_out, getenv("TERM"));
	}

	return (0);
}

int
client_main(struct client_ctx *cctx)
{
	struct pollfd	 pfd;
	char		*error;
	int		 timeout;

	siginit();

	logfile("client");
#ifndef NO_SETPROCTITLE
	setproctitle("client");
#endif

	error = NULL;
	timeout = INFTIM;
	while (!sigterm) {
		if (sigwinch)
			client_handle_winch(cctx);

		pfd.fd = cctx->srv_fd;
		pfd.events = POLLIN;
		if (BUFFER_USED(cctx->srv_out) > 0)
			pfd.events |= POLLOUT;
	
		if (poll(&pfd, 1, timeout) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		if (buffer_poll(&pfd, cctx->srv_in, cctx->srv_out) != 0)
			goto server_dead;

		switch (client_msg_dispatch(cctx, &error)) {
		case -1:
			goto out;
		case 0:
			/* May be more in buffer, don't let poll block. */
			timeout = 0;
			break;
		default:
			/* Out of data, poll may block. */
			timeout = INFTIM;
			break;
		}
	}
 
out:
 	if (sigterm) {
 		printf("[terminated]\n");
 		return (1);
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
