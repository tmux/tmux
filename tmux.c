/* $Id: tmux.c,v 1.203 2010-02-08 18:32:34 tcunha Exp $ */

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
#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tmux.h"

#if defined(DEBUG) && defined(__OpenBSD__)
extern char	*malloc_options;
#endif

char		*cfg_file;
struct options	 global_options;	/* server options */
struct options	 global_s_options;	/* session options */
struct options	 global_w_options;	/* window options */
struct environ	 global_environ;

int		 debug_level;
time_t		 start_time;
char		*socket_path;
int		 login_shell;

struct env_data {
	char	*path;
	pid_t	 pid;
	u_int	 idx;
};

__dead void	 usage(void);
void	 	 parse_env(struct env_data *);
char 		*makesockpath(const char *);
__dead void	 shell_exec(const char *, const char *);

struct imsgbuf	*main_ibuf;
struct event	 main_ev_sigterm;
int	         main_exitval;

void		 main_set_signals(void);
void		 main_clear_signals(void);
void		 main_signal(int, short, unused void *);
void		 main_callback(int, short, void *);
void		 main_dispatch(const char *);

#ifndef HAVE_PROGNAME
char      *__progname = (char *) "tmux";
#endif

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-28lquv] [-c shell-command] [-f file] [-L socket-name]\n"
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

void
parse_env(struct env_data *data)
{
	char		*env, *path_pid, *pid_idx, buf[256];
	size_t		 len;
	const char	*errstr;
	long long	 ll;

	data->pid = -1;
	if ((env = getenv("TMUX")) == NULL)
		return;

	if ((path_pid = strchr(env, ',')) == NULL || path_pid == env)
		return;
	if ((pid_idx = strchr(path_pid + 1, ',')) == NULL)
		return;
	if ((pid_idx == path_pid + 1 || pid_idx[1] == '\0'))
		return;

	/* path */
	len = path_pid - env;
	data->path = xmalloc (len + 1);
	memcpy(data->path, env, len);
	data->path[len] = '\0';

	/* pid */
	len = pid_idx - path_pid - 1;
	if (len > (sizeof buf) - 1)
		return;
	memcpy(buf, path_pid + 1, len);
	buf[len] = '\0';

	ll = strtonum(buf, 0, LONG_MAX, &errstr);
	if (errstr != NULL)
		return;
	data->pid = ll;

	/* idx */
	ll = strtonum(pid_idx+1, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return;
	data->idx = ll;
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

__dead void
shell_exec(const char *shell, const char *shellcmd)
{
	const char	*shellname, *ptr;
	char		*argv0;

	ptr = strrchr(shell, '/');
	if (ptr != NULL && *(ptr + 1) != '\0')
		shellname = ptr + 1;
	else
		shellname = shell;
	if (login_shell)
		xasprintf(&argv0, "-%s", shellname);
	else
		xasprintf(&argv0, "%s", shellname);
	setenv("SHELL", shell, 1);

	execl(shell, argv0, "-c", shellcmd, (char *) NULL);
	fatal("execl failed");
}

int
main(int argc, char **argv)
{
	struct cmd_list		*cmdlist;
	struct cmd		*cmd;
	enum msgtype		 msg;
	struct passwd		*pw;
	struct options		*oo, *so, *wo;
	struct keylist		*keylist;
	struct env_data		 envdata;
	struct msg_command_data	 cmddata;
	char			*s, *shellcmd, *path, *label, *home, *cause;
	char			 cwd[MAXPATHLEN], **var;
	void			*buf;
	size_t			 len;
	int	 		 opt, flags, quiet = 0, cmdflags = 0;
	short		 	 events;

#if defined(DEBUG) && defined(__OpenBSD__)
	malloc_options = (char *) "AFGJPX";
#endif

	flags = 0;
	shellcmd = label = path = NULL;
	envdata.path = NULL;
	login_shell = (**argv == '-');
	while ((opt = getopt(argc, argv, "28c:df:lL:qS:uUv")) != -1) {
		switch (opt) {
		case '2':
			flags |= IDENTIFY_256COLOURS;
			flags &= ~IDENTIFY_88COLOURS;
			break;
		case '8':
			flags |= IDENTIFY_88COLOURS;
			flags &= ~IDENTIFY_256COLOURS;
			break;
		case 'c':
			if (shellcmd != NULL)
				xfree(shellcmd);
			shellcmd = xstrdup(optarg);
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
			quiet = 1;
			break;
		case 'S':
			if (path != NULL)
				xfree(path);
			path = xstrdup(optarg);
			break;
		case 'u':
			flags |= IDENTIFY_UTF8;
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

	if (shellcmd != NULL && argc != 0)
		usage();

	log_open_tty(debug_level);

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

	options_init(&global_options, NULL);
	oo = &global_options;
	options_set_number(oo, "quiet", quiet);
	options_set_number(oo, "escape-time", 500);

	options_init(&global_s_options, NULL);
	so = &global_s_options;
	options_set_number(so, "base-index", 0);
	options_set_number(so, "bell-action", BELL_ANY);
	options_set_number(so, "buffer-limit", 9);
	options_set_string(so, "default-command", "%s", "");
	options_set_string(so, "default-shell", "%s", getshell());
	options_set_string(so, "default-terminal", "screen");
	options_set_number(so, "display-panes-colour", 4);
	options_set_number(so, "display-panes-active-colour", 1);
	options_set_number(so, "display-panes-time", 1000);
	options_set_number(so, "display-time", 750);
	options_set_number(so, "history-limit", 2000);
	options_set_number(so, "lock-after-time", 0);
	options_set_string(so, "lock-command", "lock -np");
	options_set_number(so, "lock-server", 1);
	options_set_number(so, "message-attr", 0);
	options_set_number(so, "message-bg", 3);
	options_set_number(so, "message-fg", 0);
	options_set_number(so, "message-limit", 20);
	options_set_number(so, "mouse-select-pane", 0);
	options_set_number(so, "pane-active-border-bg", 2);
	options_set_number(so, "pane-active-border-fg", 8);
	options_set_number(so, "pane-border-bg", 8);
	options_set_number(so, "pane-border-fg", 8);
	options_set_number(so, "repeat-time", 500);
	options_set_number(so, "set-remain-on-exit", 0);
	options_set_number(so, "set-titles", 0);
	options_set_string(so, "set-titles-string", "#S:#I:#W - \"#T\"");
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

	keylist = xmalloc(sizeof *keylist);
	ARRAY_INIT(keylist);
	ARRAY_ADD(keylist, '\002');
	options_set_data(so, "prefix", keylist, xfree);

	options_init(&global_w_options, NULL);
	wo = &global_w_options;
	options_set_number(wo, "aggressive-resize", 0);
	options_set_number(wo, "alternate-screen", 1);
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
	options_set_string(wo, "window-status-format", "#I:#W#F");
	options_set_string(wo, "window-status-current-format", "#I:#W#F");
	options_set_number(wo, "xterm-keys", 0);
	options_set_number(wo, "remain-on-exit", 0);
	options_set_number(wo, "synchronize-panes", 0);

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
		if (access(cfg_file, R_OK) != 0 && errno == ENOENT) {
			xfree(cfg_file);
			cfg_file = NULL;
		}
	}

	/*
	 * Figure out the socket path. If specified on the command-line with
	 * -S or -L, use it, otherwise try $TMUX or assume -L default.
	 */
	parse_env(&envdata);
	if (path == NULL) {
		/* No -L. Try $TMUX, or default. */
		if (label == NULL) {
			path = envdata.path;
			if (path == NULL)
				label = xstrdup("default");
		}

		/* -L or default set. */
		if (label != NULL) {
			if ((path = makesockpath(label)) == NULL) {
				log_warn("can't create socket");
				exit(1);
			}
		}
	}
	if (label != NULL)
		xfree(label);

	if (shellcmd != NULL) {
		msg = MSG_SHELL;
		buf = NULL;
		len = 0;
	} else {
		cmddata.pid = envdata.pid;
		cmddata.idx = envdata.idx;

		/* Prepare command for server. */
		cmddata.argc = argc;
		if (cmd_pack_argv(
		    argc, argv, cmddata.argv, sizeof cmddata.argv) != 0) {
			log_warnx("command too long");
			exit(1);
		}

		msg = MSG_COMMAND;
		buf = &cmddata;
		len = sizeof cmddata;
	}

	if (shellcmd != NULL)
		cmdflags |= CMD_STARTSERVER;
	else if (argc == 0)	/* new-session is the default */
		cmdflags |= CMD_STARTSERVER|CMD_SENDENVIRON|CMD_CANTNEST;
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
			if (cmd->entry->flags & CMD_CANTNEST)
				cmdflags |= CMD_CANTNEST;
		}
		cmd_list_free(cmdlist);
	}

	/*
	 * Check if this could be a nested session, if the command can't nest:
	 * if the socket path matches $TMUX, this is probably the same server.
	 */
	if (shellcmd == NULL && envdata.path != NULL &&
	    cmdflags & CMD_CANTNEST &&
	    (path == envdata.path || strcmp(path, envdata.path) == 0)) {
		log_warnx("sessions should be nested with care. "
		    "unset $TMUX to force.");
		exit(1);
	}

	if ((main_ibuf = client_init(path, cmdflags, flags)) == NULL)
		exit(1);
	xfree(path);

#ifdef HAVE_BROKEN_KQUEUE
	if (setenv("EVENT_NOKQUEUE", "1", 1) != 0)
		fatal("setenv failed");
#endif
#ifdef HAVE_BROKEN_POLL
	if (setenv("EVENT_NOPOLL", "1", 1) != 0)
		fatal("setenv failed");
#endif
	event_init();
#ifdef HAVE_BROKEN_KQUEUE
	unsetenv("EVENT_NOKQUEUE");
#endif
#ifdef HAVE_BROKEN_POLL
	unsetenv("EVENT_NOPOLL");
#endif

	imsg_compose(main_ibuf, msg, PROTOCOL_VERSION, -1, -1, buf, len);

	main_set_signals();

	events = EV_READ;
	if (main_ibuf->w.queued > 0)
		events |= EV_WRITE;
	event_once(main_ibuf->fd, events, main_callback, shellcmd, NULL);

	main_exitval = 0;
	event_dispatch();

	main_clear_signals();

	client_main();	/* doesn't return */
}

void
main_set_signals(void)
{
	struct sigaction	sigact;

	memset(&sigact, 0, sizeof sigact);
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_IGN;
	if (sigaction(SIGINT, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGPIPE, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR1, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTSTP, &sigact, NULL) != 0)
		fatal("sigaction failed");

	signal_set(&main_ev_sigterm, SIGTERM, main_signal, NULL);
	signal_add(&main_ev_sigterm, NULL);
}

void
main_clear_signals(void)
{
	struct sigaction	sigact;

	memset(&sigact, 0, sizeof sigact);
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGPIPE, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR1, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTSTP, &sigact, NULL) != 0)
		fatal("sigaction failed");

	event_del(&main_ev_sigterm);
}

/* ARGSUSED */
void
main_signal(int sig, unused short events, unused void *data)
{
	switch (sig) {
	case SIGTERM:
		exit(1);
	}
}

/* ARGSUSED */
void
main_callback(unused int fd, short events, void *data)
{
	char	*shellcmd = data;

	if (events & EV_READ)
		main_dispatch(shellcmd);

	if (events & EV_WRITE) {
		if (msgbuf_write(&main_ibuf->w) < 0)
			fatalx("msgbuf_write failed");
	}

	events = EV_READ;
	if (main_ibuf->w.queued > 0)
		events |= EV_WRITE;
	event_once(main_ibuf->fd, events, main_callback, shellcmd, NULL);
}

void
main_dispatch(const char *shellcmd)
{
	struct imsg		imsg;
	ssize_t			n, datalen;
	struct msg_print_data	printdata;
	struct msg_shell_data	shelldata;

	if ((n = imsg_read(main_ibuf)) == -1 || n == 0)
		fatalx("imsg_read failed");

	for (;;) {
		if ((n = imsg_get(main_ibuf, &imsg)) == -1)
			fatalx("imsg_get failed");
		if (n == 0)
			return;
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		switch (imsg.hdr.type) {
		case MSG_EXIT:
		case MSG_SHUTDOWN:
			if (datalen != 0)
				fatalx("bad MSG_EXIT size");

			exit(main_exitval);
		case MSG_ERROR:
		case MSG_PRINT:
			if (datalen != sizeof printdata)
				fatalx("bad MSG_PRINT size");
			memcpy(&printdata, imsg.data, sizeof printdata);
			printdata.msg[(sizeof printdata.msg) - 1] = '\0';

			log_info("%s", printdata.msg);
			if (imsg.hdr.type == MSG_ERROR)
				main_exitval = 1;
			break;
		case MSG_READY:
			if (datalen != 0)
				fatalx("bad MSG_READY size");

			event_loopexit(NULL);	/* move to client_main() */
			break;
		case MSG_VERSION:
			if (datalen != 0)
				fatalx("bad MSG_VERSION size");

			log_warnx("protocol version mismatch (client %u, "
			    "server %u)", PROTOCOL_VERSION, imsg.hdr.peerid);
			exit(1);
		case MSG_SHELL:
			if (datalen != sizeof shelldata)
				fatalx("bad MSG_SHELL size");
			memcpy(&shelldata, imsg.data, sizeof shelldata);
			shelldata.shell[(sizeof shelldata.shell) - 1] = '\0';

			main_clear_signals();

			shell_exec(shelldata.shell, shellcmd);
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
	}
}
