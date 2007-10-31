/* $Id: client.c,v 1.19 2007-10-31 14:26:26 nicm Exp $ */

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
int	client_process_local(struct client_ctx *, char **);

int
client_init(char *path, struct client_ctx *cctx, int start_server)
{
	struct sockaddr_un		sa;
	struct stat			sb;
	struct msg_identify_data	data;
	struct winsize			ws;
	size_t				size;
	int				mode;
	u_int				retries;

	if (path == NULL) {
		xasprintf(&path,
		    "%s/%s-%lu", _PATH_TMP, __progname, (u_long) getuid());
	} else
		path = xstrdup(path);

	retries = 0;
retry:
	if (stat(path, &sb) != 0) {
		if (start_server && errno == ENOENT && retries < 10) {
			if (server_start(path) != 0)
				goto error;
			usleep(10000);
			retries++;
			goto retry;
		}
		log_warn("%s: stat", path);
		goto error;
	}
	if (!S_ISSOCK(sb.st_mode)) {
		log_warnx("%s: %s", path, strerror(ENOTSOCK));
		goto error;
	}

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		log_warnx("%s: %s", path, strerror(ENAMETOOLONG));
		goto error;
	}

	if ((cctx->srv_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("%s: socket", path);
		goto error;
	}
	if (connect(
	    cctx->srv_fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1) {
		if (start_server && errno == ECONNREFUSED && retries < 10) {
			if (unlink(path) != 0) {
				log_warn("%s: unlink", path);
				goto error;
			}
			usleep(10000);
			retries++;
			goto retry;
		}
		log_warn("%s: connect", path);
		goto error;
	}

	if ((mode = fcntl(cctx->srv_fd, F_GETFL)) == -1) {
		log_warn("%s: fcntl", path);
		goto error;
	}
	if (fcntl(cctx->srv_fd, F_SETFL, mode|O_NONBLOCK) == -1) {
		log_warn("%s: fcntl", path);
		goto error;
	}
	cctx->srv_in = buffer_create(BUFSIZ);
	cctx->srv_out = buffer_create(BUFSIZ);

	if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
		if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
			log_warn("ioctl(TIOCGWINSZ)");
			goto error;
		}

		data.sx = ws.ws_col;
		data.sy = ws.ws_row;
		if (ttyname_r(STDIN_FILENO, data.tty, sizeof data.tty) != 0)
			fatal("ttyname_r failed");
		client_write_server(cctx, MSG_IDENTIFY, &data, sizeof data);
	}

	xfree(path);
	return (0);

error:
	xfree(path);
	return (-1);
}

int
client_main(struct client_ctx *cctx)
{
	struct pollfd	 pfds[2];
	char		*error;
	int		 timeout;

	logfile("client");
#ifndef NO_SETPROCTITLE
	setproctitle("client");
#endif

	siginit();
	if ((cctx->loc_fd = local_init(&cctx->loc_in, &cctx->loc_out)) == -1)
		return (1);

	error = NULL;
	timeout = INFTIM;
	while (!sigterm) {
		if (sigwinch)
			client_handle_winch(cctx);

		pfds[0].fd = cctx->srv_fd;
		pfds[0].events = POLLIN;
		if (BUFFER_USED(cctx->srv_out) > 0)
			pfds[0].events |= POLLOUT;
		pfds[1].fd = cctx->loc_fd;
		pfds[1].events = POLLIN;
		if (BUFFER_USED(cctx->loc_out) > 0)
			pfds[1].events |= POLLOUT;
	
		if (poll(pfds, 2, timeout) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		if (buffer_poll(&pfds[0], cctx->srv_in, cctx->srv_out) != 0)
			goto server_dead;
		if (buffer_poll(&pfds[1], cctx->loc_in, cctx->loc_out) != 0)
			goto local_dead;

		if (cctx->flags & CCTX_PAUSE) {
			usleep(750000);	
			cctx->flags = 0;
		}

		if (client_process_local(cctx, &error) == -1)
			goto out;
		
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
 	local_done();
 
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
	local_done();

	printf("[lost server]\n");
	return (0);

local_dead:
	/* Can't do much here. Log and die. */
	fatalx("local socket dead");
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

int
client_process_local(struct client_ctx *cctx, unused char **error)
{
	struct buffer	*b;
	int		 key;

	b = buffer_create(BUFSIZ);
	while ((key = local_key()) != KEYC_NONE)
		input_store16(b, (uint16_t) key);

	log_debug("transmitting %zu bytes of input", BUFFER_USED(b));
	if (BUFFER_USED(b) != 0) {
		client_write_server(
		    cctx, MSG_KEYS, BUFFER_OUT(b), BUFFER_USED(b));
	}

	buffer_destroy(b);
	return (0);
}

