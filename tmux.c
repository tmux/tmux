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
#include <sys/stat.h>

#include <errno.h>
#include <paths.h>
#include <pwd.h>
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
volatile sig_atomic_t sigcont;
volatile sig_atomic_t sigchld;
volatile sig_atomic_t sigusr1;
volatile sig_atomic_t sigusr2;

char		*cfg_file;
struct options	 global_options;
struct options	 global_window_options;

int		 server_locked;
char		*server_password;
time_t		 server_activity;

int		 debug_level;
int		 be_quiet;
time_t		 start_time;
char		*socket_path;

__dead void	 usage(void);
char 		*makesockpath(const char *);

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-28dqUuVv] [-f file] "
	    "[-L socket-name] [-S socket-path] [command [flags]]\n",
	    __progname);
	exit(1);
}

void
logfile(const char *name)
{
	char	*path;

	log_close();
	if (debug_level > 0) {
		xasprintf(
		    &path, "%s-%s-%ld.log", __progname, name, (long) getpid());
		log_open_file(debug_level, path);
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
		sigchld = 1;
		break;
	case SIGCONT:
		sigcont = 1;
		break;
	case SIGUSR1:
		sigusr1 = 1;
		break;
	case SIGUSR2:
		sigusr2 = 1;
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
	if (sigaction(SIGUSR1, &act, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &act, NULL) != 0)
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

char *
makesockpath(const char *label)
{
	char		base[MAXPATHLEN], *path;
	struct stat	sb;
	u_int		uid;

	uid = getuid();
	xsnprintf(base, MAXPATHLEN, "%s/%s-%d", _PATH_TMP, __progname, uid);

	if (mkdir(base, S_IRWXU) != 0 && errno != EEXIST)
		return (NULL);

	if (lstat(base, &sb) != 0)
		return (NULL);
	if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return (NULL);
	}
	if (sb.st_uid != uid || (sb.st_mode & (S_IRWXG|S_IRWXO)) != 0) {
		errno = EACCES;
		return (NULL);
	}

	xasprintf(&path, "%s/%s", base, label);
	return (path);
}

int
main(int argc, char **argv)
{
	struct client_ctx	 cctx;
	struct msg_command_data	 cmddata;
	struct buffer		*b;
	struct cmd_list		*cmdlist;
 	struct cmd		*cmd;
	struct pollfd	 	 pfd;
	struct hdr	 	 hdr;
	const char		*shell;
	struct passwd		*pw;
	char			*s, *path, *label, *cause, *home, *pass = NULL;
	char			 cwd[MAXPATHLEN];
	int	 		 retcode, opt, flags, unlock, start_server;

	unlock = flags = 0;
	label = path = NULL;
        while ((opt = getopt(argc, argv, "28df:L:qS:uUv")) != -1) {
                switch (opt) {
		case '2':
			flags |= IDENTIFY_256COLOURS;
			flags &= ~IDENTIFY_88COLOURS;
			break;
		case '8':
			flags |= IDENTIFY_88COLOURS;
			flags &= ~IDENTIFY_256COLOURS;
			break;
		case 'f':
			cfg_file = xstrdup(optarg);
			break;
		case 'L':
			if (path != NULL) {
				log_warnx("-L and -S cannot be used together");
				exit(1);
			}
			if (label != NULL)
				xfree(label);
			label = xstrdup(optarg);
			break;
		case 'S':
			if (label != NULL) {
				log_warnx("-L and -S cannot be used together");
				exit(1);
			}
			if (path != NULL)
				xfree(path);
			path = xstrdup(optarg);
			break;
		case 'q':
			be_quiet = 1;
			break;
		case 'u':
			flags |= IDENTIFY_UTF8;
			break;
		case 'U':
			unlock = 1;
			break;
		case 'd':
			flags |= IDENTIFY_HASDEFAULTS;
			break;
		case 'v':
			debug_level++;
			break;
                default:
			usage();
                }
        }
	argc -= optind;
	argv += optind;

	log_open_tty(debug_level);
	siginit();

	options_init(&global_options, NULL);
	options_set_number(&global_options, "bell-action", BELL_ANY);
	options_set_number(&global_options, "buffer-limit", 9);
	options_set_number(&global_options, "display-time", 750);
	options_set_number(&global_options, "history-limit", 2000);
	options_set_number(&global_options, "lock-after-time", 0);
	options_set_number(&global_options, "message-attr", GRID_ATTR_REVERSE);
	options_set_number(&global_options, "message-bg", 3);
	options_set_number(&global_options, "message-fg", 0);
	options_set_number(&global_options, "prefix", '\002');
	options_set_number(&global_options, "repeat-time", 500);
	options_set_number(&global_options, "set-remain-on-exit", 0);
	options_set_number(&global_options, "set-titles", 1);
	options_set_number(&global_options, "status", 1);
	options_set_number(&global_options, "status-attr", GRID_ATTR_REVERSE);
	options_set_number(&global_options, "status-bg", 2);
	options_set_number(&global_options, "status-fg", 0);
	options_set_number(&global_options, "status-interval", 15);
	options_set_number(&global_options, "status-keys", MODEKEY_EMACS);
	options_set_number(&global_options, "status-left-length", 10);
	options_set_number(&global_options, "status-right-length", 40);
	options_set_string(&global_options, "status-left", "[#S]");
	options_set_string(
	    &global_options, "status-right", "\"#24T\" %%H:%%M %%d-%%b-%%y");

	options_init(&global_window_options, NULL);
	options_set_number(&global_window_options, "aggressive-resize", 0);
	options_set_number(&global_window_options, "automatic-rename", 1);
	options_set_number(&global_window_options, "clock-mode-colour", 4);
	options_set_number(&global_window_options, "clock-mode-style", 1);
	options_set_number(&global_window_options, "force-height", 0);
	options_set_number(&global_window_options, "force-width", 0);
	options_set_number(
	    &global_window_options, "mode-attr", GRID_ATTR_REVERSE);
	options_set_number(&global_window_options, "main-pane-width", 81);
	options_set_number(&global_window_options, "main-pane-height", 24);
	options_set_number(&global_window_options, "mode-bg", 3);
	options_set_number(&global_window_options, "mode-fg", 0);
	options_set_number(&global_window_options, "mode-keys", MODEKEY_EMACS);
	options_set_number(&global_window_options, "monitor-activity", 0);
	options_set_string(&global_window_options, "monitor-content", "%s", "");
	options_set_number(&global_window_options, "utf8", 0);
	options_set_number(&global_window_options, "window-status-attr", 0);
	options_set_number(&global_window_options, "window-status-bg", 8);
	options_set_number(&global_window_options, "window-status-fg", 8);
	options_set_number(&global_window_options, "xterm-keys", 0);
 	options_set_number(&global_window_options, "remain-on-exit", 0);

	if (!(flags & IDENTIFY_UTF8)) {
		/*
		 * If the user has set LANG to contain UTF-8, it is a safe
		 * assumption that either they are using a UTF-8 terminal, or
		 * if not they know that output from UTF-8-capable programs may
		 * be wrong.
		 */
		if ((s = getenv("LANG")) != NULL && strstr(s, "UTF-8") != NULL)
			flags |= IDENTIFY_UTF8;
	}

	if (cfg_file == NULL) {
		home = getenv("HOME");
		if (home == NULL || *home == '\0') {
			pw = getpwuid(getuid());
			if (pw != NULL)
				home = pw->pw_dir;
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

	if (label == NULL)
		label = xstrdup("default");
	if (path == NULL && (path = makesockpath(label)) == NULL) {
		log_warn("can't create socket");
		exit(1);
	}
	xfree(label);

	shell = getenv("SHELL");
	if (shell == NULL || *shell == '\0') {
		pw = getpwuid(getuid());
		if (pw != NULL)
			shell = pw->pw_shell;
		if (shell == NULL || *shell == '\0')
			shell = _PATH_BSHELL;
	}
	options_set_string(
	    &global_options, "default-command", "exec %s", shell);

	if (getcwd(cwd, sizeof cwd) == NULL) {
		log_warn("getcwd");
		exit(1);
	}
	options_set_string(&global_options, "default-path", "%s", cwd);

	if (unlock) {
		if (argc != 0) {
			log_warnx("can't specify a command when unlocking");
			exit(1);
		}
		cmdlist = NULL;
		if ((pass = getpass("Password: ")) == NULL)
			exit(1);
		start_server = 0;
	} else {
		if (argc == 0) {
			cmd = xmalloc(sizeof *cmd);
			cmd->entry = &cmd_new_session_entry;
			cmd->entry->init(cmd, 0);

			cmdlist = xmalloc(sizeof *cmdlist);
			TAILQ_INIT(cmdlist);
			TAILQ_INSERT_HEAD(cmdlist, cmd, qentry);
		} else {
			cmdlist = cmd_list_parse(argc, argv, &cause);
			if (cmdlist == NULL) {
				log_warnx("%s", cause);
				exit(1);
			}
		}
		start_server = 0;
		TAILQ_FOREACH(cmd, cmdlist, qentry) {
			if (cmd->entry->flags & CMD_STARTSERVER) {
				start_server = 1;
				break;
			}
		}
	}

 	memset(&cctx, 0, sizeof cctx);
	if (client_init(path, &cctx, start_server, flags) != 0)
		exit(1);
	xfree(path);

	b = buffer_create(BUFSIZ);
	if (unlock) {
		cmd_send_string(b, pass);
		client_write_server(
		    &cctx, MSG_UNLOCK, BUFFER_OUT(b), BUFFER_USED(b));
	} else {
		cmd_list_send(cmdlist, b);
		cmd_list_free(cmdlist);
		client_fill_session(&cmddata);
		client_write_server2(&cctx, MSG_COMMAND,
		    &cmddata, sizeof cmddata, BUFFER_OUT(b), BUFFER_USED(b));
	}
	buffer_destroy(b);

	retcode = 0;
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
			goto out;

	restart:
		if (BUFFER_USED(cctx.srv_in) < sizeof hdr)
			continue;
		memcpy(&hdr, BUFFER_OUT(cctx.srv_in), sizeof hdr);
		if (BUFFER_USED(cctx.srv_in) < (sizeof hdr) + hdr.size)
			continue;
		buffer_remove(cctx.srv_in, sizeof hdr);

		switch (hdr.type) {
		case MSG_EXIT:
		case MSG_SHUTDOWN:
			goto out;
		case MSG_ERROR:
			retcode = 1;
			/* FALLTHROUGH */
		case MSG_PRINT:
			if (hdr.size > INT_MAX - 1)
				fatalx("bad MSG_PRINT size");
			log_info("%.*s",
			    (int) hdr.size, BUFFER_OUT(cctx.srv_in));
			if (hdr.size != 0)
				buffer_remove(cctx.srv_in, hdr.size);
			goto restart;
		case MSG_READY:
			retcode = client_main(&cctx);
			goto out;
		default:
			fatalx("unexpected command");
		}
	}

out:
	options_free(&global_options);
	options_free(&global_window_options);

	close(cctx.srv_fd);
	buffer_destroy(cctx.srv_in);
	buffer_destroy(cctx.srv_out);

	return (retcode);
}
