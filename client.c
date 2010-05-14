/* $Id: client.c,v 1.92 2010-05-14 14:35:26 tcunha Exp $ */

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
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tmux.h"

struct imsgbuf	client_ibuf;
struct event	client_event;
const char     *client_exitmsg;
int		client_exitval;

void		client_send_identify(int);
void		client_send_environ(void);
void		client_write_server(enum msgtype, void *, size_t);
void		client_update_event(void);
void		client_signal(int, short, void *);
void		client_callback(int, short, void *);
int		client_dispatch(void);

struct imsgbuf *
client_init(char *path, int cmdflags, int flags)
{
	struct sockaddr_un	sa;
	size_t			size;
	int			fd, mode;
#ifdef HAVE_SETPROCTITLE
	char		        rpathbuf[MAXPATHLEN];
#endif

#ifdef HAVE_SETPROCTITLE
	if (realpath(path, rpathbuf) == NULL)
		strlcpy(rpathbuf, path, sizeof rpathbuf);
	setproctitle("client (%s)", rpathbuf);
#endif

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		goto not_found;
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("socket failed");

	if (connect(fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1) {
		if (!(cmdflags & CMD_STARTSERVER))
			goto not_found;
		switch (errno) {
		case ECONNREFUSED:
			if (unlink(path) != 0)
				goto not_found;
			/* FALLTHROUGH */
		case ENOENT:
			if ((fd = server_start(path)) == -1)
				goto start_failed;
			goto server_started;
		}
		goto not_found;
	}

server_started:
	if ((mode = fcntl(fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");
	imsg_init(&client_ibuf, fd);

	if (cmdflags & CMD_SENDENVIRON)
		client_send_environ();
	if (isatty(STDIN_FILENO))
		client_send_identify(flags);

	return (&client_ibuf);

start_failed:
	log_warnx("server failed to start");
	return (NULL);

not_found:
	log_warn("server not found");
	return (NULL);
}

void
client_send_identify(int flags)
{
	struct msg_identify_data	data;
	struct winsize			ws;
	char			       *term;
	int				fd;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		fatal("ioctl(TIOCGWINSZ)");
	data.flags = flags;

	if (getcwd(data.cwd, sizeof data.cwd) == NULL)
		*data.cwd = '\0';

	term = getenv("TERM");
	if (term == NULL ||
	    strlcpy(data.term, term, sizeof data.term) >= sizeof data.term)
		*data.term = '\0';

	if ((fd = dup(STDIN_FILENO)) == -1)
		fatal("dup failed");
	imsg_compose(&client_ibuf,
	    MSG_IDENTIFY, PROTOCOL_VERSION, -1, fd, &data, sizeof data);
}

void
client_send_environ(void)
{
	struct msg_environ_data	data;
	char		      **var;

	for (var = environ; *var != NULL; var++) {
		if (strlcpy(data.var, *var, sizeof data.var) >= sizeof data.var)
			continue;
		client_write_server(MSG_ENVIRON, &data, sizeof data);
	}
}

void
client_write_server(enum msgtype type, void *buf, size_t len)
{
	imsg_compose(&client_ibuf, type, PROTOCOL_VERSION, -1, -1, buf, len);
}

void
client_update_event(void)
{
	short	events;

	event_del(&client_event);
	events = EV_READ;
	if (client_ibuf.w.queued > 0)
		events |= EV_WRITE;
	event_set(&client_event, client_ibuf.fd, events, client_callback, NULL);
	event_add(&client_event, NULL);
}

__dead void
client_main(void)
{
	logfile("client");

	/* Note: event_init() has already been called. */

	/* Set up signals. */
	set_signals(client_signal);

	/*
	 * imsg_read in the first client poll loop (before the terminal has
	 * been initialised) may have read messages into the buffer after the
	 * MSG_READY switched to here. Process anything outstanding now to
	 * avoid hanging waiting for messages that have already arrived.
	 */
	if (client_dispatch() != 0)
		goto out;

	/* Set the event and dispatch. */
	client_update_event();
	event_dispatch();

out:
	/* Print the exit message, if any, and exit. */
	if (client_exitmsg != NULL && !login_shell)
		printf("[%s]\n", client_exitmsg);
	exit(client_exitval);
}

/* ARGSUSED */
void
client_signal(int sig, unused short events, unused void *data)
{
	struct sigaction	sigact;

	switch (sig) {
	case SIGHUP:
		client_exitmsg = "lost tty";
		client_exitval = 1;
		client_write_server(MSG_EXITING, NULL, 0);
		break;
	case SIGTERM:
		client_exitmsg = "terminated";
		client_exitval = 1;
		client_write_server(MSG_EXITING, NULL, 0);
		break;
	case SIGWINCH:
		client_write_server(MSG_RESIZE, NULL, 0);
		break;
	case SIGCONT:
		memset(&sigact, 0, sizeof sigact);
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_RESTART;
		sigact.sa_handler = SIG_IGN;
		if (sigaction(SIGTSTP, &sigact, NULL) != 0)
			fatal("sigaction failed");
		client_write_server(MSG_WAKEUP, NULL, 0);
		break;
	}

	client_update_event();
}

/* ARGSUSED */
void
client_callback(unused int fd, short events, unused void *data)
{
	ssize_t	n;

	if (events & EV_READ) {
		if ((n = imsg_read(&client_ibuf)) == -1 || n == 0)
			goto lost_server;
		if (client_dispatch() != 0) {
			event_loopexit(NULL);
			return;
		}
	}

	if (events & EV_WRITE) {
		if (msgbuf_write(&client_ibuf.w) < 0)
			goto lost_server;
	}

	client_update_event();
	return;

lost_server:
	client_exitmsg = "lost server";
	client_exitval = 1;
	event_loopexit(NULL);
}

int
client_dispatch(void)
{
	struct imsg		imsg;
	struct msg_lock_data	lockdata;
	struct sigaction	sigact;
	ssize_t			n, datalen;

	for (;;) {
		if ((n = imsg_get(&client_ibuf, &imsg)) == -1)
			fatalx("imsg_get failed");
		if (n == 0)
			return (0);
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		log_debug("client got %d", imsg.hdr.type);
		switch (imsg.hdr.type) {
		case MSG_DETACH:
			if (datalen != 0)
				fatalx("bad MSG_DETACH size");

			client_write_server(MSG_EXITING, NULL, 0);
			client_exitmsg = "detached";
			break;
		case MSG_EXIT:
			if (datalen != 0)
				fatalx("bad MSG_EXIT size");

			client_write_server(MSG_EXITING, NULL, 0);
			client_exitmsg = "exited";
			break;
		case MSG_EXITED:
			if (datalen != 0)
				fatalx("bad MSG_EXITED size");

			imsg_free(&imsg);
			return (-1);
		case MSG_SHUTDOWN:
			if (datalen != 0)
				fatalx("bad MSG_SHUTDOWN size");

			client_write_server(MSG_EXITING, NULL, 0);
			client_exitmsg = "server exited";
			client_exitval = 1;
			break;
		case MSG_SUSPEND:
			if (datalen != 0)
				fatalx("bad MSG_SUSPEND size");

			memset(&sigact, 0, sizeof sigact);
			sigemptyset(&sigact.sa_mask);
			sigact.sa_flags = SA_RESTART;
			sigact.sa_handler = SIG_DFL;
			if (sigaction(SIGTSTP, &sigact, NULL) != 0)
				fatal("sigaction failed");
			kill(getpid(), SIGTSTP);
			break;
		case MSG_LOCK:
			if (datalen != sizeof lockdata)
				fatalx("bad MSG_LOCK size");
			memcpy(&lockdata, imsg.data, sizeof lockdata);

			lockdata.cmd[(sizeof lockdata.cmd) - 1] = '\0';
			system(lockdata.cmd);
			client_write_server(MSG_UNLOCK, NULL, 0);
			break;
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
	}
}
