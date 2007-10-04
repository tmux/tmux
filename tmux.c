/* $Id: tmux.c,v 1.29 2007-10-04 11:52:03 nicm Exp $ */

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
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <paths.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tmux.h"

#ifdef DEBUG
const char	*malloc_options = "AFGJPX";
#endif

volatile sig_atomic_t sigwinch;
volatile sig_atomic_t sigterm;
int		 debug_level;
int		 prefix_key = META;
u_int		 status_lines;
char		*default_command;

void		 sighandler(int);

void
usage(char **ptr, const char *fmt, ...)
{
	char	*msg;
	va_list	 ap;

	if (fmt == NULL) {
		xasprintf(ptr,
		    "usage: %s [-v] [-S path] command [flags]", __progname);
	} else {
		va_start(ap, fmt);
		xvasprintf(&msg, fmt, ap);
		va_end(ap);

		xasprintf(ptr, "usage: %s [-v] [-S path] %s", __progname, msg);
		xfree(msg);
	}
}

void
logfile(const char *name)
{
	FILE	*f;
	char	*path;

	log_close();
	if (debug_level > 0) {
		xasprintf(
		    &path, "%s-%s-%ld.log", __progname, name, (long) getpid());
		f = fopen(path, "w");
		log_open(f, LOG_DAEMON, debug_level);
		xfree(path);
	}
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

void
siginit(void)
{
	struct sigaction	 act;

	memset(&act, 0, sizeof act);
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
	if (sigaction(SIGTSTP, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGQUIT, &act, NULL) != 0)
		fatal("sigaction failed");

	act.sa_handler = sighandler;
	if (sigaction(SIGWINCH, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTERM, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGCHLD, &act, NULL) != 0)
		fatal("sigaction failed");
}

void
sigreset(void)
{
	struct sigaction act;
	
	memset(&act, 0, sizeof act);
	sigemptyset(&act.sa_mask);
	
	act.sa_handler = SIG_DFL;
	if (sigaction(SIGPIPE, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR1, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGINT, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTSTP, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGQUIT, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGWINCH, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTERM, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGCHLD, &act, NULL) != 0)
		fatal("sigaction failed");
}

int
main(int argc, char **argv)
{
	struct client_ctx	 cctx;
	struct msg_command_data	 data;
	struct buffer		*b;
	struct cmd		*cmd;
	struct pollfd	 	 pfd;
	struct hdr	 	 hdr;
	const char		*shell;
	char			*path, *cause, name[MAXNAMELEN];
	int	 		 n, opt;

	*name = '\0';
	path = NULL;
        while ((opt = getopt(argc, argv, "S:s:v")) != EOF) {
                switch (opt) {
		case 'S':
			path = xstrdup(optarg);
			break;
		case 's':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name)
				errx(1, "session name too long: %s", optarg);
			break;
		case 'v':
			debug_level++;
			break;
                default:
			goto usage;
                }
        }
	argc -= optind;
	argv += optind;
	if (argc == 0)
		goto usage;

	log_open(stderr, LOG_USER, debug_level);

	status_lines = 1;

	shell = getenv("SHELL");
	if (shell == NULL || *shell == '\0')
		shell = "/bin/ksh";
	xasprintf(&default_command, "exec %s -l", shell);

	if ((cmd = cmd_parse(argc, argv, &cause)) == NULL) {
		if (cause == NULL)
			goto usage;
		log_warnx("%s", cause);
		exit(1);
	}

	if (!(cmd->entry->flags & CMD_NOSESSION))
		client_fill_sessid(&data.sid, name);
	if (client_init(path, &cctx, cmd->entry->flags & CMD_STARTSERVER) != 0)
		exit(1);
	b = buffer_create(BUFSIZ);
	cmd_send(cmd, b);
	cmd_free(cmd);

	client_write_server2(&cctx,
	    MSG_COMMAND, &data, sizeof data, BUFFER_OUT(b), BUFFER_USED(b));
	buffer_destroy(b);
	
	for (;;) {
		pfd.fd = cctx.srv_fd;
		pfd.events = POLLIN;
		if (BUFFER_USED(cctx.srv_out) > 0)
			pfd.events |= POLLOUT;
	
		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		if (buffer_poll(&pfd, cctx.srv_in, cctx.srv_out) != 0)
			fatalx("lost server");

	restart:
		if (BUFFER_USED(cctx.srv_in) < sizeof hdr)
			continue;
		memcpy(&hdr, BUFFER_OUT(cctx.srv_in), sizeof hdr);
		if (BUFFER_USED(cctx.srv_in) < (sizeof hdr) + hdr.size)
			continue;
		buffer_remove(cctx.srv_in, sizeof hdr);

		switch (hdr.type) {
		case MSG_EXIT:
			n = 0;
			goto out;
		case MSG_PRINT:
			if (hdr.size > INT_MAX - 1)
				fatalx("bad MSG_PRINT size");
			log_info("%.*s",
			    (int) hdr.size, BUFFER_OUT(cctx.srv_in));
			buffer_remove(cctx.srv_in, hdr.size);
			goto restart;
		case MSG_ERROR:
			if (hdr.size > INT_MAX - 1)
				fatalx("bad MSG_ERROR size");
			log_warnx("%.*s",
			    (int) hdr.size, BUFFER_OUT(cctx.srv_in));	
			buffer_remove(cctx.srv_in, hdr.size);
			n = 1;
			goto out;
		case MSG_READY:
			n = client_main(&cctx);
			goto out;
		default:
			fatalx("unexpected command");
		}
	}

out:
#ifdef DEBUG
	xmalloc_report(getpid(), "client");
#endif
	return (n);

usage:
	usage(&cause, NULL);
	fprintf(stderr, "%s\n", cause);
	exit(1);
}
