/* $Id: tmux.c,v 1.72 2008-07-02 21:22:57 nicm Exp $ */

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

#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#ifndef NO_PATHS_H
#include <paths.h>
#endif

#include "tmux.h"

#ifdef DEBUG
#ifdef __OpenBSD__
const char	*malloc_options = "AFGJPX";
#endif
#ifdef __FreeBSD__
const char	*_malloc_options = "AJX";
#endif
#endif

volatile sig_atomic_t sigwinch;
volatile sig_atomic_t sigterm;

char		*cfg_file;
struct options	 global_options;

int		 debug_level;
int		 be_quiet;

void		 sighandler(int);
__dead void	 usage(void);

#ifdef NO_PROGNAME
const char      *__progname = "tmux";
#endif

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-qVv] [-f file] [-S socket-path] [command [flags]]\n",
	    __progname);
	exit(1);
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
	int	saved_errno;

	saved_errno = errno;
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
	errno = saved_errno;
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
	struct passwd		*pw;
	char			*path, *cause, *home;
	char			 rpath[MAXPATHLEN];
	int	 		 n, opt;

	path = NULL;
        while ((opt = getopt(argc, argv, "f:qS:Vv")) != EOF) {
                switch (opt) {
		case 'f':
			cfg_file = xstrdup(optarg);
			break;
		case 'S':
			path = xstrdup(optarg);
			break;
		case 'q':
			be_quiet = 1;
			break;
		case 'v':
			debug_level++;
			break;
		case 'V':
			printf("%s " BUILD "\n", __progname);
			exit(0);
                default:
			usage();
                }
        }
	argc -= optind;
	argv += optind;

	log_open(stderr, LOG_USER, debug_level);
	siginit();

	options_init(&global_options, NULL);
	options_set_number(&global_options, "status", 1);
	options_set_number(&global_options, "status-fg", 0);
	options_set_number(&global_options, "status-bg", 2);
	options_set_number(&global_options, "bell-action", BELL_ANY);
	options_set_number(&global_options, "history-limit", 2000);
	options_set_number(&global_options, "display-time", 750);
	options_set_number(&global_options, "prefix", META);
	options_set_string(&global_options, "status-left", "%s", ""); /* ugh */
	options_set_string(
	    &global_options, "status-right", "%%H:%%M %%d-%%b-%%y");
	options_set_number(&global_options, "status-interval", 15);
	options_set_number(&global_options, "set-titles", 1);
	options_set_number(&global_options, "buffer-limit", 9);
	options_set_number(&global_options, "remain-by-default", 0);
	options_set_number(&global_options, "mode-keys", MODEKEY_EMACS);

	if (cfg_file == NULL) {
		home = getenv("HOME");
		if (home == NULL || *home == '\0') {
			pw = getpwuid(getuid());
			if (pw != NULL)
				home = pw->pw_dir;
			endpwent();
		}
		xasprintf(&cfg_file, "%s/%s", home, DEFAULT_CFG);
		if (access(cfg_file, R_OK) != 0) {
			xfree(cfg_file);
			cfg_file = NULL;
		}
	} else {
		if (access(cfg_file, R_OK) != 0) {
			log_warn("%s", cfg_file);
			exit(1);
		}
	}

	if (path == NULL) {
		xasprintf(&path,
		    "%s/%s-%lu", _PATH_TMP, __progname, (u_long) getuid());
	}
	if (realpath(path, rpath) == NULL) {
		if (errno != ENOENT) {
			log_warn("%s", path);
			exit(1);
		}
		/*
		 * Linux appears to fill in the buffer fine but then returns
		 * ENOENT if the file doesn't exist. But since it returns an
		 * error, we can't rely on the buffer. Grr.
		 */
		if (strlcpy(rpath, path, sizeof rpath) >= sizeof rpath) {
			log_warnx("%s: %s", path, strerror(ENAMETOOLONG));
			exit(1);
		}
	}
	xfree(path);

	shell = getenv("SHELL");
	if (shell == NULL || *shell == '\0') {
		pw = getpwuid(getuid());
		if (pw != NULL)
			shell = pw->pw_shell;
		endpwent();
		if (shell == NULL || *shell == '\0')
			shell = _PATH_BSHELL;
	}
	options_set_string(
	    &global_options, "default-command", "exec %s", shell);

	if (argc == 0) {
		cmd = xmalloc(sizeof *cmd);
		cmd->entry = &cmd_new_session_entry;
		cmd->entry->init(cmd, 0);
	} else if ((cmd = cmd_parse(argc, argv, &cause)) == NULL) {
		log_warnx("%s", cause);
		exit(1);
	}

	memset(&cctx, 0, sizeof cctx);
	client_fill_session(&data);
	if (client_init(rpath, &cctx, cmd->entry->flags & CMD_STARTSERVER) != 0)
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
	options_free(&global_options);

	close(cctx.srv_fd);
	buffer_destroy(cctx.srv_in);
	buffer_destroy(cctx.srv_out);

#ifdef DEBUG
	xmalloc_report(getpid(), "client");
#endif
	return (n);
}
