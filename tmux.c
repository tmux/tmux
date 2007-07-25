/* $Id: tmux.c,v 1.2 2007-07-25 23:13:18 nicm Exp $ */

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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tmux.h"

#ifdef DEBUG
const char	*malloc_options = "AFGJPX";
#endif

int	 	 connect_server(void);
int		 process_server(struct buffer *);
int		 process_local(struct buffer *, struct buffer *);
void		 sighandler(int);
__dead void	 usage(void);
__dead void	 main_list(char *);
void		 process_list(struct buffer *, const char *);

/* SIGWINCH received flag. */
volatile sig_atomic_t sigwinch;

/* SIGTERM received flag. */
volatile sig_atomic_t sigterm;

/* Debug output level. */
int		 debug_level;

/* Path to server socket. */
char		 socket_path[MAXPATHLEN];

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-v] [-n name] [-s path]\n", __progname);
        exit(1);
}

void
sighandler(int sig)
{
	switch (sig) {
	case SIGWINCH:
		sigwinch = 1;
		break;
	case SIGTERM:
		sigterm = 1;
		break;
	case SIGCHLD:
		waitpid(WAIT_ANY, NULL, WNOHANG);
		break;
	}
}

int
main(int argc, char **argv)
{
	int	 		 opt, srv_fd, loc_fd, mode, listf, n;
	char			*path, name[MAXNAMELEN];
	FILE			*f;
	struct buffer		*srv_in, *srv_out, *loc_in, *loc_out;
	struct pollfd		 pfds[2];
	struct hdr		 hdr;
	struct identify_data	 id;
	struct size_data	 sd;
	struct winsize	 	 ws;
	struct sigaction	 act;
	struct stat		 sb;

	*name = '\0';
	path = NULL;
	listf = 0;

        while ((opt = getopt(argc, argv, "ln:s:v?")) != EOF) {
                switch (opt) {
		case 'l':
			listf = 1;
			break;
		case 'n':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name)
				errx(1, "name too long");
			break;
		case 's':
			path = xstrdup(optarg);
			break;
		case 'v':
			debug_level++;
			break;
                case '?':
                default:
                        usage();
                }
        }
	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();

	/* Sort out socket path. */
	if (path == NULL) {
		xasprintf(&path,
		    "%s/%s-%lu", _PATH_TMP, __progname, (u_long) getuid());
	}
	if (realpath(path, socket_path) == NULL)
		err(1, "realpath");
	xfree(path);

	/* Skip to list function if listing. */
	if (listf) {
		if (*name == '\0')
			main_list(NULL);
		else
			main_list(name);
	}

	/* And fill name. */
	if (*name == '\0') 
		xsnprintf(name, sizeof name, "s-%lu", (u_long) getpid());
	
	/* Check stdin/stdout. */
	if (!isatty(STDIN_FILENO))
		errx(1, "stdin is not a tty");
	if (!isatty(STDOUT_FILENO))
		errx(1, "stdout is not a tty");

	/* Set up signal handlers. */
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;

	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, NULL) != 0)
		err(1, "sigaction");
	if (sigaction(SIGUSR1, &act, NULL) != 0)
		err(1, "sigaction");
	if (sigaction(SIGUSR2, &act, NULL) != 0)
		err(1, "sigaction");
	if (sigaction(SIGINT, &act, NULL) != 0)
		err(1, "sigaction");
	if (sigaction(SIGTSTP, &act, NULL) != 0)
		err(1, "sigaction");
	if (sigaction(SIGQUIT, &act, NULL) != 0)
		err(1, "sigaction");

	act.sa_handler = sighandler;
	if (sigaction(SIGWINCH, &act, NULL) != 0)
		err(1, "sigaction");
	if (sigaction(SIGTERM, &act, NULL) != 0)
		err(1, "sigaction");
	if (sigaction(SIGCHLD, &act, NULL) != 0)
		err(1, "sigaction");

	/* Start server if necessary. */
	if (stat(socket_path, &sb) != 0) {
		if (errno != ENOENT)
			err(1, "%s", socket_path);
		else {
			if (server_start() != 0)
				errx(1, "couldn't start server");	
			sleep(1);
		}
	} else {
		if (!S_ISSOCK(sb.st_mode))
			errx(1, "%s: not a socket", socket_path);
	}

	/* Connect to server. */
	if ((srv_fd = connect_server()) == -1)
		errx(1, "couldn't find server");
	if ((mode = fcntl(srv_fd, F_GETFL)) == -1)
		err(1, "fcntl");
	if (fcntl(srv_fd, F_SETFL, mode|O_NONBLOCK) == -1)
		err(1, "fcntl");
	srv_in = buffer_create(BUFSIZ);
	srv_out = buffer_create(BUFSIZ);

	/* Find window size. */
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		err(1, "ioctl(TIOCGWINSZ)");

	/* Send initial data. */
	hdr.code = MSG_IDENTIFY;
	hdr.size = sizeof id;
	buffer_write(srv_out, &hdr, sizeof hdr);
	strlcpy(id.name, name, sizeof id.name);
	id.sx = ws.ws_col;
	id.sy = ws.ws_row;
	buffer_write(srv_out, &id, hdr.size);

	/* Start logging to file. */
	if (debug_level > 0) {
		xasprintf(&path,
		    "%s-client-%ld.log", __progname, (long) getpid());
		f = fopen(path, "w");
		log_open(f, LOG_USER, debug_level);
		xfree(path);
	}

	/* Initialise terminal. */
	loc_fd = local_init(&loc_in, &loc_out);
	setproctitle("client (%s)", name);

	/* Main loop. */
	n = 0;
	while (!sigterm) {
		/* Handle SIGWINCH if necessary. */
		if (sigwinch) {
			if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
				fatal("ioctl failed");

			hdr.code = MSG_SIZE;
			hdr.size = sizeof sd;
			buffer_write(srv_out, &hdr, sizeof hdr);
			sd.sx = ws.ws_col;
			sd.sy = ws.ws_row;
			buffer_write(srv_out, &sd, hdr.size);

			sigwinch = 0;
		}

		/* Set up pollfds. */
		pfds[0].fd = srv_fd;
		pfds[0].events = POLLIN;
		if (BUFFER_USED(srv_out) > 0)
			pfds[0].events |= POLLOUT;
		pfds[1].fd = loc_fd;
		pfds[1].events = POLLIN;
		if (BUFFER_USED(loc_out) > 0)
			pfds[1].events |= POLLOUT;

		/* Do the poll. */
		if (poll(pfds, 2, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		/* Read/write from sockets. */
		if (buffer_poll(&pfds[0], srv_in, srv_out) != 0)
			goto server_dead;
		if (buffer_poll(&pfds[1], loc_in, loc_out) != 0)
			fatalx("lost local socket");

		/* Output flushed; pause if requested. */
		if (n)
			usleep(750000);

		/* Process any data. */
		if ((n = process_server(srv_in)) == -1)
			break;
		if (process_local(loc_in, srv_out) == -1)
			break;
	}

	local_done();

	if (!sigterm)
		printf("[detached]\n");
	else
		printf("[terminated]\n");
	exit(0);

server_dead:
	local_done();

	printf("[lost server]\n");
	exit(1);
}

/* Connect to server socket from PID. */
int
connect_server(void)
{
	int			fd;
	struct sockaddr_un	sa;
	size_t			sz;

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	sz = strlcpy(sa.sun_path, socket_path, sizeof sa.sun_path);
	if (sz >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		return (-1);
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return (-1);
	if (connect(fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1)
		return (-1);
	return (fd);
}

/* Handle data from server. */
int
process_server(struct buffer *srv_in)
{
	struct hdr	hdr;

	for (;;) {
		if (BUFFER_USED(srv_in) < sizeof hdr)
			break;
		memcpy(&hdr, BUFFER_OUT(srv_in), sizeof hdr);
		if (BUFFER_USED(srv_in) < (sizeof hdr) + hdr.size)
			break;
		buffer_remove(srv_in, sizeof hdr);

		switch (hdr.code) {
		case MSG_OUTPUT:
			local_output(srv_in, hdr.size);
			break;
		case MSG_PAUSE:
			if (hdr.size != 0)
				fatalx("bad MSG_PAUSE size");
			return (1);
		case MSG_EXIT:
			return (-1);
		}
	}

	return (0);
}

/* Handle data from local terminal. */
int
process_local(struct buffer *loc_in, struct buffer *srv_out)
{
	struct buffer	*b;
	struct hdr	 hdr;
	size_t		 size;
	int		 n, key;

	n = 0;
	b = buffer_create(BUFSIZ);

	while ((key = local_key(&size)) != KEYC_NONE) {
		log_debug("key code: %d", key);

		if (key == cmd_prefix) {
			if ((key = local_key(NULL)) == KEYC_NONE) {
				buffer_reverse_remove(loc_in, size);
				break;
			}
			n = cmd_execute(key, srv_out);
			break;
		}

		input_store8(b, '\e');
		input_store16(b, (uint16_t) key);
	}

	if (BUFFER_USED(b) == 0) {
		buffer_destroy(b);
		return (n);
	}
	log_debug("transmitting %zu bytes of input", BUFFER_USED(b));

	hdr.code = MSG_INPUT;
	hdr.size = BUFFER_USED(b);
	buffer_write(srv_out, &hdr, sizeof hdr);
	buffer_write(srv_out, BUFFER_OUT(b), BUFFER_USED(b));

	buffer_destroy(b);
	return (n);
}

/* List sessions or windows. */
__dead void
main_list(char *name)
{
	struct sessions_data	 sd;
	struct windows_data	 wd;
	int			 srv_fd, mode;
	struct buffer		*srv_in, *srv_out;
	struct pollfd	 	 pfd;
	struct hdr		 hdr;

	/* Connect to server. */
	if ((srv_fd = connect_server()) == -1)
		errx(1, "couldn't find server");
	if ((mode = fcntl(srv_fd, F_GETFL)) == -1)
		err(1, "fcntl");
	if (fcntl(srv_fd, F_SETFL, mode|O_NONBLOCK) == -1)
		err(1, "fcntl");
	srv_in = buffer_create(BUFSIZ);
	srv_out = buffer_create(BUFSIZ);

	/* Send query data. */
	if (name == NULL) {
		hdr.code = MSG_SESSIONS;
		hdr.size = sizeof sd;
		buffer_write(srv_out, &hdr, sizeof hdr);
		buffer_write(srv_out, &sd, hdr.size);
	} else {
		hdr.code = MSG_WINDOWS;
		hdr.size = sizeof wd;
		buffer_write(srv_out, &hdr, sizeof hdr);
		strlcpy(wd.name, name, sizeof wd.name);
		buffer_write(srv_out, &wd, hdr.size);
	}

	/* Main loop. */
	for (;;) {
		/* Set up pollfd. */
		pfd.fd = srv_fd;
		pfd.events = POLLIN;
		if (BUFFER_USED(srv_out) > 0)
			pfd.events |= POLLOUT;

		/* Do the poll. */
		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			err(1, "poll");
		}

		/* Read/write from sockets. */
		if (buffer_poll(&pfd, srv_in, srv_out) != 0)
			errx(1, "lost server"); 

		/* Process data. */
		process_list(srv_in, name);
	}
}

void
process_list(struct buffer *srv_in, const char *name)
{
	struct sessions_data	 sd;
	struct sessions_entry	 se;
	struct windows_data	 wd;
	struct windows_entry	 we;
	struct hdr		 hdr;
	char		        *tim;
	
	for (;;) {
		if (BUFFER_USED(srv_in) < sizeof hdr)
			break;
		memcpy(&hdr, BUFFER_OUT(srv_in), sizeof hdr);
		if (BUFFER_USED(srv_in) < (sizeof hdr) + hdr.size)
			break;
		buffer_remove(srv_in, sizeof hdr);
		
		switch (hdr.code) {
		case MSG_SESSIONS:
			if (hdr.size < sizeof sd)
				errx(1, "bad MSG_SESSIONS size");
			buffer_read(srv_in, &sd, sizeof sd);
			hdr.size -= sizeof sd; 
			if (sd.sessions == 0 && hdr.size == 0)
				exit(0);
			if (hdr.size < sd.sessions * sizeof se)
				errx(1, "bad MSG_SESSIONS size");
			while (sd.sessions-- > 0) {
				buffer_read(srv_in, &se, sizeof se);
				tim = ctime(&se.tim);
				*strchr(tim, '\n') = '\0';
				printf("%s: %u windows (created %s)\n",
				    se.name, se.windows, tim);
			}
			exit(0);
		case MSG_WINDOWS:
			if (hdr.size < sizeof wd)
				errx(1, "bad MSG_WINDOWS size");
			buffer_read(srv_in, &wd, sizeof wd);
			hdr.size -= sizeof wd; 
			if (wd.windows == 0 && hdr.size == 0)
				errx(1, "session \"%s\" not found", name);
			if (hdr.size < wd.windows * sizeof we)
				errx(1, "bad MSG_WINDOWS size");
			while (wd.windows-- > 0) {
				buffer_read(srv_in, &we, sizeof we);
				if (*we.title != '\0') {
					printf("%u: %s \"%s\" (%s)\n",
					    we.idx, we.name, we.title, we.tty); 
				} else {
					printf("%u: %s (%s)\n",
					    we.idx, we.name, we.tty);
				}
			}
			exit(0);
		}
	}
}
