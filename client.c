/* $Id: client.c,v 1.6 2007-09-27 09:52:03 nicm Exp $ */

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
	struct sockaddr_un	sa;
	struct stat		sb;
	size_t			sz;
	int			mode;
	u_int			retries;

	if (path == NULL) {
		xasprintf(&path,
		    "%s/%s-%lu", _PATH_TMP, __progname, (u_long) getuid());
	}

	retries = 0;
retry:
	if (stat(path, &sb) != 0) {
		if (!start_server || errno != ENOENT) {
			log_warn("%s", path);
			return (-1);
		}
		if (server_start(path) != 0)
			return (-1);
		sleep(1); /* XXX */
		goto retry;
	}
	if (!S_ISSOCK(sb.st_mode)) {
		log_warnx("%s: %s", path, strerror(ENOTSOCK));
		return (-1);
	}

	if (start_server) {
		if (!isatty(STDIN_FILENO)) {
			log_warnx("stdin is not a tty");
			return (-1);
		}
		if (!isatty(STDOUT_FILENO)) {
			log_warnx("stdout is not a tty");
			return (-1);
		}

		if (ioctl(STDIN_FILENO, TIOCGWINSZ, &cctx->ws) == -1) {
			log_warn("ioctl(TIOCGWINSZ)");
			return (-1);
		}
	}

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	sz = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (sz >= sizeof sa.sun_path) {
		log_warnx("%s: %s", path, strerror(ENAMETOOLONG));
		return (-1);
	}

	if ((cctx->srv_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("%s: socket", path);
		return (-1);
	}
	if (connect(
	    cctx->srv_fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1) {
		if (start_server && errno == ECONNREFUSED && retries < 5) {
			if (unlink(path) != 0) {
				log_warn("%s: unlink", path);
				return (-1);
			}
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

	return (0);
}

int
client_main(struct client_ctx *cctx)
{
	struct pollfd	 pfds[2];
	char		*error;
	int		 n;

	logfile("client");
	setproctitle("client");

	siginit();
	if ((cctx->loc_fd = local_init(&cctx->loc_in, &cctx->loc_out)) == -1)
		return (1);

	n = 0;
	error = NULL;
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
	
		if (poll(pfds, 2, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		if (buffer_poll(&pfds[0], cctx->srv_in, cctx->srv_out) != 0)
			goto server_dead;
		if (buffer_poll(&pfds[1], cctx->loc_in, cctx->loc_out) != 0)
			goto local_dead;

		/* XXX Output flushed; pause if required. */
		if (n)
			usleep(750000);
		/* XXX XXX special return code for pause? or flag in cctx? */
		if ((n = client_process_local(cctx, &error)) == -1)
			break;
		if ((n = client_msg_dispatch(cctx, &error)) == -1)
			break;
	}

	local_done();

	if (error != NULL) {
		printf("[error: %s]\n", error);
		return (1);
	}
	if (sigterm) {
		printf("[terminated]\n");
		return (1);
	}
	printf("[detached]\n");
	return (0);

server_dead:
	local_done();

	printf("[lost server]\n");
	return (1);

local_dead:
	/* Can't do much here. Log and die. */
	fatalx("local socket dead");
}

void
client_fill_sessid(struct sessid *sid, char name[MAXNAMELEN])
{
	char		*env, *ptr, buf[256];
	const char	*errstr;
	long long	 ll;

	strlcpy(sid->name, name, sizeof sid->name);

	sid->pid = -1;
	if ((env = getenv("TMUX")) == NULL)
		return;
	if ((ptr = strchr(env, ',')) == NULL)
		return;
	if ((size_t) (ptr - env) > sizeof buf)
		return;
	memcpy(buf, env, ptr - env);
	buf[ptr - env] = '\0';

	ll = strtonum(ptr + 1, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return;
	sid->idx = ll;

	ll = strtonum(buf, 0, LLONG_MAX, &errstr);
	if (errstr != NULL)
		return;
	sid->pid = ll;
}

void
client_write_server(
    struct client_ctx *cctx, enum hdrtype type, void *buf, size_t len)
{
	struct hdr	hdr;

	hdr.type = type;
	hdr.size = len;
	buffer_write(cctx->srv_out, &hdr, sizeof hdr);
	if (len > 0)
		buffer_write(cctx->srv_out, buf, len);
}

void
client_handle_winch(struct client_ctx *cctx)
{
	struct size_data	data;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &cctx->ws) == -1)
		fatal("ioctl failed");

	data.sx = cctx->ws.ws_col;
	data.sy = cctx->ws.ws_row;
	client_write_server(cctx, MSG_SIZE, &data, sizeof data);
	
	sigwinch = 0;
}

int
client_process_local(struct client_ctx *cctx, char **error)
{
	struct buffer	*b;
	size_t		 size;
	int		 n, key;

	n = 0;
	b = buffer_create(BUFSIZ);

	while ((key = local_key(&size)) != KEYC_NONE) {
		log_debug("key code: %d", key);

		if (key == client_cmd_prefix) {
			if ((key = local_key(NULL)) == KEYC_NONE) {
				/* XXX sux */
				buffer_reverse_remove(cctx->loc_in, size);
				break;
			}
			n = client_cmd_dispatch(key, cctx, error);
			break;
		}

		input_store8(b, '\e');
		input_store16(b, (uint16_t) key /*XXX*/);
	}

	log_debug("transmitting %zu bytes of input", BUFFER_USED(b));
	if (BUFFER_USED(b) == 0) {
		buffer_destroy(b);
		return (n);
	}
	client_write_server(cctx, MSG_INPUT, BUFFER_OUT(b), BUFFER_USED(b));
	buffer_destroy(b);
	return (n);
}

