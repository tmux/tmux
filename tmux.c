/* $Id: tmux.c,v 1.171 2009-09-04 20:37:40 tcunha Exp $ */

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
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tmux.h"

#ifdef DEBUG
/* DragonFly uses an OpenBSD-like malloc() since 1.6 */
#if defined(__OpenBSD__) || defined(__DragonFly__)
const char	*malloc_options = "AFGJPX";
#endif
#ifdef __FreeBSD__
const char	*_malloc_options = "AJX";
#endif
#endif

volatile sig_atomic_t sigwinch;
volatile sig_atomic_t sigterm;
volatile sig_atomic_t sigcont;
volatile sig_atomic_t sigchld;
volatile sig_atomic_t sigusr1;
volatile sig_atomic_t sigusr2;

char		*cfg_file;
struct options	 global_s_options;	/* session options */
struct options	 global_w_options;	/* window options */
struct environ	 global_environ;

int		 server_locked;
struct passwd	*server_locked_pw;
u_int		 password_failures;
time_t		 password_backoff;
char		*server_password;
time_t		 server_activity;

int		 debug_level;
int		 be_quiet;
time_t		 start_time;
char		*socket_path;
int		 login_shell;

__dead void	 usage(void);
char 		*makesockpath(const char *);
int		 prepare_unlock(enum msgtype *, void **, size_t *, int);
int		 prepare_cmd(enum msgtype *, void **, size_t *, int, char **);
int		 dispatch_imsg(struct client_ctx *, int *);

#ifndef HAVE_PROGNAME
char      *__progname = (char *) "tmux";
#endif

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-28dlqUuv] [-f file] [-L socket-name]\n"
	    "            [-S socket-path] [command [flags]]\n",
	    __progname);
	exit(1);
}

void
logfile(const char *name)
{
	char	*path;

	log_close();
	if (debug_level > 0) {
		xasprintf(&path, "tmux-%s-%ld.log", name, (long) getpid());
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

const char *
getshell(void)
{
	struct passwd	*pw;
	const char	*shell;

	shell = getenv("SHELL");
	if (checkshell(shell))
		return (shell);

	pw = getpwuid(getuid());
	if (pw != NULL && checkshell(pw->pw_shell))
		return (pw->pw_shell);

	return (_PATH_BSHELL);
}

int
checkshell(const char *shell)
{
	if (shell == NULL || *shell == '\0' || areshell(shell))
		return (0);
	if (access(shell, X_OK) != 0)
		return (0);
	return (1);
}

int
areshell(const char *shell)
{
	const char	*progname, *ptr;

	if ((ptr = strrchr(shell, '/')) != NULL)
		ptr++;
	else
		ptr = shell;
	progname = __progname;
	if (*progname == '-')
		progname++;
	if (strcmp(ptr, progname) == 0)
		return (1);
	return (0);
}

char *
makesockpath(const char *label)
{
	char		base[MAXPATHLEN], *path;
	struct stat	sb;
	u_int		uid;

	uid = getuid();
	xsnprintf(base, MAXPATHLEN, "%s/tmux-%d", _PATH_TMP, uid);

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
prepare_unlock(enum msgtype *msg, void **buf, size_t *len, int argc)
{
	static struct msg_unlock_data	 unlockdata;
	char				*pass;

	if (argc != 0) {
		log_warnx("can't specify a command when unlocking");
		return (-1);
	}
	
	if ((pass = getpass("Password:")) == NULL)
		return (-1);

	if (strlen(pass) >= sizeof unlockdata.pass) {
		log_warnx("password too long");
		return (-1);
	}
		
	strlcpy(unlockdata.pass, pass, sizeof unlockdata.pass);
	memset(pass, 0, strlen(pass));

	*buf = &unlockdata;
	*len = sizeof unlockdata;

	*msg = MSG_UNLOCK;
	return (0);
}

int
prepare_cmd(enum msgtype *msg, void **buf, size_t *len, int argc, char **argv)
{
	static struct msg_command_data	 cmddata;

	client_fill_session(&cmddata);
	
	cmddata.argc = argc;
	if (cmd_pack_argv(argc, argv, cmddata.argv, sizeof cmddata.argv) != 0) {
		log_warnx("command too long");
		return (-1);
	}

	*buf = &cmddata;
	*len = sizeof cmddata;

	*msg = MSG_COMMAND;
	return (0);
}

int
main(int argc, char **argv)
{
	struct client_ctx	 cctx;
	struct cmd_list		*cmdlist;
 	struct cmd		*cmd;
	struct pollfd	 	 pfd;
	enum msgtype		 msg;
	struct passwd		*pw;
	struct options		*so, *wo;
	char			*s, *path, *label, *home, *cause, **var;
	char			 cwd[MAXPATHLEN];
	void			*buf;
	size_t			 len;
	int	 		 retcode, opt, flags, unlock, cmdflags = 0;
	int			 nfds;

	unlock = flags = 0;
	label = path = NULL;
	login_shell = (**argv == '-');
	while ((opt = getopt(argc, argv, "28df:lL:qS:uUv")) != -1) {
		switch (opt) {
		case '2':
			flags |= IDENTIFY_256COLOURS;
			flags &= ~IDENTIFY_88COLOURS;
			break;
		case '8':
			flags |= IDENTIFY_88COLOURS;
			flags &= ~IDENTIFY_256COLOURS;
			break;
		case 'd':
			flags |= IDENTIFY_HASDEFAULTS;
			break;
		case 'f':
			if (cfg_file != NULL)
				xfree(cfg_file);
			cfg_file = xstrdup(optarg);
			break;
		case 'l':
			login_shell = 1;
			break;
		case 'L':
			if (label != NULL)
				xfree(label);
			label = xstrdup(optarg);
			break;
		case 'q':
			be_quiet = 1;
			break;
		case 'S':
			if (path != NULL)
				xfree(path);
			path = xstrdup(optarg);
			break;
		case 'u':
			flags |= IDENTIFY_UTF8;
			break;
		case 'U':
			unlock = 1;
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

	if (!(flags & IDENTIFY_UTF8)) {
		/*
		 * If the user has set whichever of LC_ALL, LC_CTYPE or LANG
		 * exist (in that order) to contain UTF-8, it is a safe
		 * assumption that either they are using a UTF-8 terminal, or
		 * if not they know that output from UTF-8-capable programs may
		 * be wrong.
		 */
		if ((s = getenv("LC_ALL")) == NULL) {
			if ((s = getenv("LC_CTYPE")) == NULL)
				s = getenv("LANG");
		}
		if (s != NULL && (strcasestr(s, "UTF-8") != NULL ||
		    strcasestr(s, "UTF8") != NULL))
			flags |= IDENTIFY_UTF8;
	}

	environ_init(&global_environ);
 	for (var = environ; *var != NULL; var++)
		environ_put(&global_environ, *var);

	options_init(&global_s_options, NULL);
	so = &global_s_options;
	options_set_number(so, "base-index", 0);
	options_set_number(so, "bell-action", BELL_ANY);
	options_set_number(so, "buffer-limit", 9);
	options_set_string(so, "default-command", "%s", "");
	options_set_string(so, "default-shell", "%s", getshell());
	options_set_string(so, "default-terminal", "screen");
	options_set_number(so, "display-panes-colour", 4);
	options_set_number(so, "display-panes-time", 1000);
	options_set_number(so, "display-time", 750);
	options_set_number(so, "history-limit", 2000);
	options_set_number(so, "lock-after-time", 0);
	options_set_number(so, "message-attr", 0);
	options_set_number(so, "message-bg", 3);
	options_set_number(so, "message-fg", 0);
	options_set_number(so, "prefix", '\002');
	options_set_number(so, "repeat-time", 500);
	options_set_number(so, "set-remain-on-exit", 0);
	options_set_number(so, "set-titles", 0);
	options_set_number(so, "status", 1);
	options_set_number(so, "status-attr", 0);
	options_set_number(so, "status-bg", 2);
	options_set_number(so, "status-fg", 0);
	options_set_number(so, "status-interval", 15);
	options_set_number(so, "status-justify", 0);
	options_set_number(so, "status-keys", MODEKEY_EMACS);
	options_set_string(so, "status-left", "[#S]");
	options_set_number(so, "status-left-attr", 0);
	options_set_number(so, "status-left-bg", 8);
	options_set_number(so, "status-left-fg", 8);
	options_set_number(so, "status-left-length", 10);
	options_set_string(so, "status-right", "\"#22T\" %%H:%%M %%d-%%b-%%y");
	options_set_number(so, "status-right-attr", 0);
	options_set_number(so, "status-right-bg", 8);
	options_set_number(so, "status-right-fg", 8);
	options_set_number(so, "status-right-length", 40);
	options_set_string(so, "terminal-overrides",
	    "*88col*:colors=88,*256col*:colors=256");
	options_set_string(so, "update-environment", "DISPLAY "
	    "WINDOWID SSH_ASKPASS SSH_AUTH_SOCK SSH_AGENT_PID SSH_CONNECTION");
	options_set_number(so, "visual-activity", 0);
	options_set_number(so, "visual-bell", 0);
	options_set_number(so, "visual-content", 0);

	options_init(&global_w_options, NULL);
	wo = &global_w_options;
	options_set_number(wo, "aggressive-resize", 0);
	options_set_number(wo, "automatic-rename", 1);
	options_set_number(wo, "clock-mode-colour", 4);
	options_set_number(wo, "clock-mode-style", 1);
	options_set_number(wo, "force-height", 0);
	options_set_number(wo, "force-width", 0);
	options_set_number(wo, "main-pane-height", 24);
	options_set_number(wo, "main-pane-width", 81);
	options_set_number(wo, "mode-attr", 0);
	options_set_number(wo, "mode-bg", 3);
	options_set_number(wo, "mode-fg", 0);
	options_set_number(wo, "mode-keys", MODEKEY_EMACS);
	options_set_number(wo, "mode-mouse", 0);
	options_set_number(wo, "monitor-activity", 0);
	options_set_string(wo, "monitor-content", "%s", "");
	options_set_number(wo, "window-status-attr", 0);
	options_set_number(wo, "window-status-bg", 8);
	options_set_number(wo, "window-status-current-attr", 0);
	options_set_number(wo, "window-status-current-bg", 8);
	options_set_number(wo, "window-status-current-fg", 8);
	options_set_number(wo, "window-status-fg", 8);
	options_set_number(wo, "xterm-keys", 0);
 	options_set_number(wo, "remain-on-exit", 0);

 	if (flags & IDENTIFY_UTF8) {
		options_set_number(so, "status-utf8", 1);
		options_set_number(wo, "utf8", 1);
	} else {
		options_set_number(so, "status-utf8", 0);
		options_set_number(wo, "utf8", 0);
	}

	if (getcwd(cwd, sizeof cwd) == NULL) {
		pw = getpwuid(getuid());
		if (pw->pw_dir != NULL && *pw->pw_dir != '\0')
			strlcpy(cwd, pw->pw_dir, sizeof cwd);
		else
			strlcpy(cwd, "/", sizeof cwd);
	}
	options_set_string(so, "default-path", "%s", cwd);

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

	if (unlock) {
		if (prepare_unlock(&msg, &buf, &len, argc) != 0)
			exit(1);
	} else {
		if (prepare_cmd(&msg, &buf, &len, argc, argv) != 0)
			exit(1);
	}

	if (unlock)
		cmdflags &= ~CMD_STARTSERVER;
	else if (argc == 0)	/* new-session is the default */
		cmdflags |= CMD_STARTSERVER|CMD_SENDENVIRON;
	else {
		/*
		 * It sucks parsing the command string twice (in client and
		 * later in server) but it is necessary to get the start server
		 * flag.
		 */
		if ((cmdlist = cmd_list_parse(argc, argv, &cause)) == NULL) {
			log_warnx("%s", cause);
			exit(1);
		}
		cmdflags &= ~CMD_STARTSERVER;
		TAILQ_FOREACH(cmd, cmdlist, qentry) {
			if (cmd->entry->flags & CMD_STARTSERVER)
				cmdflags |= CMD_STARTSERVER;
			if (cmd->entry->flags & CMD_SENDENVIRON)
				cmdflags |= CMD_SENDENVIRON;
		}
		cmd_list_free(cmdlist);
	}

 	memset(&cctx, 0, sizeof cctx);
	if (client_init(path, &cctx, cmdflags, flags) != 0)
		exit(1);
	xfree(path);

	client_write_server(&cctx, msg, buf, len);
	memset(buf, 0, len);

	retcode = 0;
	for (;;) {
		pfd.fd = cctx.ibuf.fd;
		pfd.events = POLLIN;
		if (cctx.ibuf.w.queued != 0)
			pfd.events |= POLLOUT;

		if ((nfds = poll(&pfd, 1, INFTIM)) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}
		if (nfds == 0)
			continue;

		if (pfd.revents & (POLLERR|POLLHUP|POLLNVAL))
			fatalx("socket error");

                if (pfd.revents & POLLIN) {
			if (dispatch_imsg(&cctx, &retcode) != 0)
				break;
		}

		if (pfd.revents & POLLOUT) {
			if (msgbuf_write(&cctx.ibuf.w) < 0)
				fatalx("msgbuf_write failed");
		}
	}

	options_free(&global_s_options);
	options_free(&global_w_options);

	return (retcode);
}

int
dispatch_imsg(struct client_ctx *cctx, int *retcode)
{
	struct imsg		imsg;
	ssize_t			n, datalen;
	struct msg_print_data	printdata;

        if ((n = imsg_read(&cctx->ibuf)) == -1 || n == 0)
		fatalx("imsg_read failed");

	for (;;) {
		if ((n = imsg_get(&cctx->ibuf, &imsg)) == -1)
			fatalx("imsg_get failed");
		if (n == 0)
			return (0);
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		switch (imsg.hdr.type) {
		case MSG_EXIT:
		case MSG_SHUTDOWN:
			if (datalen != 0)
				fatalx("bad MSG_EXIT size");

			return (-1);
		case MSG_ERROR:
			*retcode = 1;
			/* FALLTHROUGH */
		case MSG_PRINT:
			if (datalen != sizeof printdata)
				fatalx("bad MSG_PRINT size");
			memcpy(&printdata, imsg.data, sizeof printdata);
			printdata.msg[(sizeof printdata.msg) - 1] = '\0';

			log_info("%s", printdata.msg);
			break;
		case MSG_READY:
			if (datalen != 0)
				fatalx("bad MSG_READY size");

			*retcode = client_main(cctx);
			return (-1);
		case MSG_VERSION:
			if (datalen != 0)
				fatalx("bad MSG_VERSION size");

			log_warnx("protocol version mismatch (client %u, "
			    "server %u)", PROTOCOL_VERSION, imsg.hdr.peerid);
			*retcode = 1;
			return (-1);
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
	}
}
