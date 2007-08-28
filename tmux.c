/* $Id: tmux.c,v 1.6 2007-08-28 09:36:33 nicm Exp $ */

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

void		 op_new(char *, struct winsize *);
void		 op_attach(char *, struct winsize *);
int	 	 connect_server(void);
int		 process_server(char **);
int		 process_local(void);
void		 sighandler(int);
__dead void	 usage(void);
__dead void	 main_list(char *);
void		 process_list(const char *);

/* SIGWINCH received flag. */
volatile sig_atomic_t sigwinch;

/* SIGTERM received flag. */
volatile sig_atomic_t sigterm;

/* Debug output level. */
int		 debug_level;

/* Path to server socket. */
char		 socket_path[MAXPATHLEN];

/* Server socket and buffers. */
int		 server_fd = -1;
struct buffer	*server_in;
struct buffer	*server_out;

/* Local socket and buffers. */
int		 local_fd = -1;
struct buffer	*local_in;
struct buffer	*local_out;

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-v] [-n name] [-s path] command\n", __progname);
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
	int	 		 opt, mode, n;
	char			*path, *error, name[MAXNAMELEN];
	FILE			*f;
	enum op			 op;
	struct pollfd		 pfds[2];
	struct hdr		 hdr;
	struct size_data	 sd;
	struct winsize	 	 ws;
	struct sigaction	 act;
	struct stat		 sb;

	*name = '\0';
	path = NULL;

        while ((opt = getopt(argc, argv, "n:s:v?")) != EOF) {
                switch (opt) {
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
	if (argc != 1)
		usage();

	/* Determine command. */
	if (strncmp(argv[0], "list", strlen(argv[0])) == 0)
		op = OP_LIST;
	else if (strncmp(argv[0], "new", strlen(argv[0])) == 0)
		op = OP_NEW;
	else if (strncmp(argv[0], "attach", strlen(argv[0])) == 0)
		op = OP_ATTACH;
	else
		usage();

	/* Sort out socket path. */
	if (path == NULL) {
		xasprintf(&path,
		    "%s/%s-%lu", _PATH_TMP, __progname, (u_long) getuid());
	}
	if (realpath(path, socket_path) == NULL)
		err(1, "realpath");
	xfree(path);

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
	n = 0;
restart:
	if (stat(socket_path, &sb) != 0) {
		if (errno != ENOENT)
			err(1, "%s", socket_path);
		else if (op != OP_LIST) {
			if (server_start() != 0)
				errx(1, "couldn't start server");	
			sleep(1);
		}
	} else {
		if (!S_ISSOCK(sb.st_mode))
			errx(1, "%s: not a socket", socket_path);
	}

	/* Connect to server. */
	if ((server_fd = connect_server()) == -1) {
		if (errno == ECONNREFUSED && n++ < 5) {
			unlink(socket_path);
			goto restart;
		}
		errx(1, "couldn't find server");
	}
	if ((mode = fcntl(server_fd, F_GETFL)) == -1)
		err(1, "fcntl");
	if (fcntl(server_fd, F_SETFL, mode|O_NONBLOCK) == -1)
		err(1, "fcntl");
	server_in = buffer_create(BUFSIZ);
	server_out = buffer_create(BUFSIZ);

	/* Skip to list function if listing. */
	if (op == OP_LIST)
		main_list(name);

	/* Check stdin/stdout. */
	if (!isatty(STDIN_FILENO))
		errx(1, "stdin is not a tty");
	if (!isatty(STDOUT_FILENO))
		errx(1, "stdout is not a tty");

	/* Find window size. */
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		err(1, "ioctl(TIOCGWINSZ)");

	/* Send initial data. */
	switch (op) {
	case OP_NEW:
		op_new(name, &ws);
		break;
	case OP_ATTACH:
		op_attach(name, &ws);
		break;
	default:
		fatalx("unknown op");
	}

	/* Start logging to file. */
	if (debug_level > 0) {
		xasprintf(&path,
		    "%s-client-%ld.log", __progname, (long) getpid());
		f = fopen(path, "w");
		log_open(f, LOG_USER, debug_level);
		xfree(path);
	}
	setproctitle("client (%s)", name);

	/* Main loop. */
	n = 0;
	while (!sigterm) {
		/* Handle SIGWINCH if necessary. */
		if (local_fd != -1 && sigwinch) {
			if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
				fatal("ioctl failed");

			hdr.type = MSG_SIZE;
			hdr.size = sizeof sd;
			buffer_write(server_out, &hdr, sizeof hdr);
			sd.sx = ws.ws_col;
			sd.sy = ws.ws_row;
			buffer_write(server_out, &sd, hdr.size);

			sigwinch = 0;
		}

		/* Set up pollfds. */
		pfds[0].fd = server_fd;
		pfds[0].events = POLLIN;
		if (BUFFER_USED(server_out) > 0)
			pfds[0].events |= POLLOUT;
		pfds[1].fd = local_fd;
		pfds[1].events = POLLIN;
		if (local_fd != -1 && BUFFER_USED(local_out) > 0)
			pfds[1].events |= POLLOUT;
	
		/* Do the poll. */
		if (poll(pfds, 2, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		/* Read/write from sockets. */
		if (buffer_poll(&pfds[0], server_in, server_out) != 0)
			goto server_dead;
		if (local_fd != -1 && 
		    buffer_poll(&pfds[1], local_in, local_out) != 0)
			fatalx("lost local socket");

		/* Output flushed; pause if requested. */
		if (n)
			usleep(750000);

		/* Process any data. */
		if ((n = process_server(&error)) == -1)
			break;
		if (process_local() == -1)
			break;
	}

	if (local_fd != -1)
		local_done();

	if (error != NULL)
		errx(1, "%s", error);

	if (!sigterm)
		printf("[detached]\n");
	else
		printf("[terminated]\n");
	exit(0);

server_dead:
	if (local_fd != -1)
		local_done();

	printf("[lost server]\n");
	exit(1);
}

/* New command. */
void
op_new(char *name, struct winsize *ws)
{
	struct new_data	data;
	struct hdr	hdr;

	hdr.type = MSG_NEW;
	hdr.size = sizeof data;
	buffer_write(server_out, &hdr, sizeof hdr);

	strlcpy(data.name, name, sizeof data.name);
	data.sx = ws->ws_col;
	data.sy = ws->ws_row;
	buffer_write(server_out, &data, hdr.size);
}

/* Attach command. */
void
op_attach(char *name, struct winsize *ws)
{
	struct attach_data	data;
	struct hdr		hdr;

	hdr.type = MSG_ATTACH;
	hdr.size = sizeof data;
	buffer_write(server_out, &hdr, sizeof hdr);

	strlcpy(data.name, name, sizeof data.name);
	data.sx = ws->ws_col;
	data.sy = ws->ws_row;
	buffer_write(server_out, &data, hdr.size);
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
process_server(char **error)
{
	struct hdr	hdr;

	*error = NULL;
	for (;;) {
		if (BUFFER_USED(server_in) < sizeof hdr)
			break;
		memcpy(&hdr, BUFFER_OUT(server_in), sizeof hdr);
		if (BUFFER_USED(server_in) < (sizeof hdr) + hdr.size)
			break;
		buffer_remove(server_in, sizeof hdr);

		switch (hdr.type) {
		case MSG_READY:
			if (hdr.size != 0) {
				xasprintf(error, "%.*s",
				    (int) hdr.size, BUFFER_OUT(server_in));
				return (-1);
			}
			local_fd = local_init(&local_in, &local_out);
			break;
		case MSG_OUTPUT:
			local_output(server_in, hdr.size);
			break;
		case MSG_PAUSE:
			if (hdr.size != 0)
				fatalx("bad MSG_PAUSE size");
			return (1);
		case MSG_EXIT:
			return (-1);
		default:
			fatalx("unexpected message");
		}
	}

	return (0);
}

/* Handle data from local terminal. */
int
process_local(void)
{
	struct buffer	*b;
	struct hdr	 hdr;
	size_t		 size;
	int		 n, key;

	if (local_fd == -1)
		return (0);

	n = 0;
	b = buffer_create(BUFSIZ);

	while ((key = local_key(&size)) != KEYC_NONE) {
		log_debug("key code: %d", key);

		if (key == cmd_prefix) {
			if ((key = local_key(NULL)) == KEYC_NONE) {
				buffer_reverse_remove(local_in, size);
				break;
			}
			n = cmd_execute(key, server_out);
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

	hdr.type = MSG_INPUT;
	hdr.size = BUFFER_USED(b);
	buffer_write(server_out, &hdr, sizeof hdr);
	buffer_write(server_out, BUFFER_OUT(b), BUFFER_USED(b));

	buffer_destroy(b);
	return (n);
}

/* List sessions or windows. */
__dead void
main_list(char *name)
{
	struct sessions_data	 sd;
	struct windows_data	 wd;
	struct pollfd	 	 pfd;
	struct hdr		 hdr;

	/* Send query data. */
	if (*name == '\0') {
		hdr.type = MSG_SESSIONS;
		hdr.size = sizeof sd;
		buffer_write(server_out, &hdr, sizeof hdr);
		buffer_write(server_out, &sd, hdr.size);
	} else {
		hdr.type = MSG_WINDOWS;
		hdr.size = sizeof wd;
		buffer_write(server_out, &hdr, sizeof hdr);
		strlcpy(wd.name, name, sizeof wd.name);
		buffer_write(server_out, &wd, hdr.size);
	}

	/* Main loop. */
	for (;;) {
		/* Set up pollfd. */
		pfd.fd = server_fd;
		pfd.events = POLLIN;
		if (BUFFER_USED(server_out) > 0)
			pfd.events |= POLLOUT;

		/* Do the poll. */
		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			err(1, "poll");
		}

		/* Read/write from sockets. */
		if (buffer_poll(&pfd, server_in, server_out) != 0)
			errx(1, "lost server"); 

		/* Process data. */
		process_list(name);
	}
}

void
process_list(const char *name)
{
	struct sessions_data	 sd;
	struct sessions_entry	 se;
	struct windows_data	 wd;
	struct windows_entry	 we;
	struct hdr		 hdr;
	char		        *tim;
	
	for (;;) {
		if (BUFFER_USED(server_in) < sizeof hdr)
			break;
		memcpy(&hdr, BUFFER_OUT(server_in), sizeof hdr);
		if (BUFFER_USED(server_in) < (sizeof hdr) + hdr.size)
			break;
		buffer_remove(server_in, sizeof hdr);
		
		switch (hdr.type) {
		case MSG_SESSIONS:
			if (hdr.size < sizeof sd)
				errx(1, "bad MSG_SESSIONS size");
			buffer_read(server_in, &sd, sizeof sd);
			hdr.size -= sizeof sd; 
			if (sd.sessions == 0 && hdr.size == 0)
				exit(0);
			if (hdr.size < sd.sessions * sizeof se)
				errx(1, "bad MSG_SESSIONS size");
			while (sd.sessions-- > 0) {
				buffer_read(server_in, &se, sizeof se);
				tim = ctime(&se.tim);
				*strchr(tim, '\n') = '\0';
				printf("%s: %u windows (created %s)\n",
				    se.name, se.windows, tim);
			}
			exit(0);
		case MSG_WINDOWS:
			if (hdr.size < sizeof wd)
				errx(1, "bad MSG_WINDOWS size");
			buffer_read(server_in, &wd, sizeof wd);
			hdr.size -= sizeof wd; 
			if (wd.windows == 0 && hdr.size == 0)
				errx(1, "session not found: %s", name);
			if (hdr.size < wd.windows * sizeof we)
				errx(1, "bad MSG_WINDOWS size");
			while (wd.windows-- > 0) {
				buffer_read(server_in, &we, sizeof we);
				if (*we.title != '\0') {
					printf("%u: %s \"%s\" (%s)\n",
					    we.idx, we.name, we.title, we.tty); 
				} else {
					printf("%u: %s (%s)\n",
					    we.idx, we.name, we.tty);
				}
			}
			exit(0);
		default:
			fatalx("unexpected message");
		}
	}
}
