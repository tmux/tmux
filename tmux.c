/* $Id: tmux.c,v 1.228 2010-12-27 21:22:24 tcunha Exp $ */

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
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

#if defined(DEBUG) && defined(__OpenBSD__)
extern char	*malloc_options;
#endif

struct options	 global_options;	/* server options */
struct options	 global_s_options;	/* session options */
struct options	 global_w_options;	/* window options */
struct environ	 global_environ;

struct event_base *ev_base;

char		*cfg_file;
char		*shell_cmd;
int		 debug_level;
time_t		 start_time;
char		 socket_path[MAXPATHLEN];
int		 login_shell;
char		*environ_path;
pid_t		 environ_pid;
u_int		 environ_idx;

__dead void	 usage(void);
void	 	 parseenvironment(void);
char 		*makesocketpath(const char *);

#ifndef HAVE_PROGNAME
char      *__progname = (char *) "tmux";
#endif

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-28lquvV] [-c shell-command] [-f file] [-L socket-name]\n"
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
parseenvironment(void)
{
	char		*env, *path_pid, *pid_idx, buf[256];
	size_t		 len;
	const char	*errstr;
	long long	 ll;

	environ_pid = -1;
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
	environ_path = xmalloc(len + 1);
	memcpy(environ_path, env, len);
	environ_path[len] = '\0';

	/* pid */
	len = pid_idx - path_pid - 1;
	if (len > (sizeof buf) - 1)
		return;
	memcpy(buf, path_pid + 1, len);
	buf[len] = '\0';

	ll = strtonum(buf, 0, LONG_MAX, &errstr);
	if (errstr != NULL)
		return;
	environ_pid = ll;

	/* idx */
	ll = strtonum(pid_idx + 1, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return;
	environ_idx = ll;
}

char *
makesocketpath(const char *label)
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
	int		 mode;

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

	if ((mode = fcntl(STDIN_FILENO, F_GETFL)) != -1)
		fcntl(STDIN_FILENO, F_SETFL, mode & ~O_NONBLOCK);
	if ((mode = fcntl(STDOUT_FILENO, F_GETFL)) != -1)
		fcntl(STDOUT_FILENO, F_SETFL, mode & ~O_NONBLOCK);
	if ((mode = fcntl(STDERR_FILENO, F_GETFL)) != -1)
		fcntl(STDERR_FILENO, F_SETFL, mode & ~O_NONBLOCK);
	closefrom(STDERR_FILENO + 1);

	execl(shell, argv0, "-c", shellcmd, (char *) NULL);
	fatal("execl failed");
}

int
main(int argc, char **argv)
{
	struct passwd	*pw;
	struct options	*oo, *so, *wo;
	struct keylist	*keylist;
	char		*s, *path, *label, *home, **var;
	int	 	 opt, flags, quiet, keys;

#if defined(DEBUG) && defined(__OpenBSD__)
	malloc_options = (char *) "AFGJPX";
#endif

	quiet = flags = 0;
	label = path = NULL;
	login_shell = (**argv == '-');
	while ((opt = getopt(argc, argv, "28c:df:lL:qS:uUvV")) != -1) {
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
			if (shell_cmd != NULL)
				xfree(shell_cmd);
			shell_cmd = xstrdup(optarg);
			break;
		case 'V':
			printf("%s %s\n", __progname, BUILD);
			exit(0);
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

	if (shell_cmd != NULL && argc != 0)
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
	options_set_number(oo, "exit-unattached", 0);

	options_init(&global_s_options, NULL);
	so = &global_s_options;
	options_set_number(so, "base-index", 0);
	options_set_number(so, "bell-action", BELL_ANY);
	options_set_number(so, "buffer-limit", 9);
	options_set_string(so, "default-command", "%s", "");
	options_set_string(so, "default-path", "%s", "");
	options_set_string(so, "default-shell", "%s", getshell());
	options_set_string(so, "default-terminal", "screen");
	options_set_number(so, "destroy-unattached", 0);
	options_set_number(so, "detach-on-destroy", 1);
	options_set_number(so, "display-panes-active-colour", 1);
	options_set_number(so, "display-panes-colour", 4);
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
	options_set_number(so, "pane-active-border-bg", 8);
	options_set_number(so, "pane-active-border-fg", 2);
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
	options_set_string(so, "update-environment",
	    "DISPLAY "
	    "SSH_ASKPASS SSH_AUTH_SOCK SSH_AGENT_PID SSH_CONNECTION "
	    "WINDOWID "
	    "XAUTHORITY");
	options_set_number(so, "visual-activity", 0);
	options_set_number(so, "visual-bell", 0);
	options_set_number(so, "visual-content", 0);
	options_set_number(so, "visual-silence", 0);

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
	options_set_number(wo, "main-pane-width", 80);
	options_set_number(wo, "mode-attr", 0);
	options_set_number(wo, "mode-bg", 3);
	options_set_number(wo, "mode-fg", 0);
	options_set_number(wo, "mode-mouse", 0);
	options_set_number(wo, "monitor-activity", 0);
	options_set_string(wo, "monitor-content", "%s", "");
	options_set_number(wo, "monitor-silence", 0);
	options_set_number(wo, "other-pane-height", 0);
	options_set_number(wo, "other-pane-width", 0);
	options_set_number(wo, "window-status-attr", 0);
	options_set_number(wo, "window-status-bg", 8);
	options_set_number(wo, "window-status-current-attr", 0);
	options_set_number(wo, "window-status-current-bg", 8);
	options_set_number(wo, "window-status-current-fg", 8);
	options_set_number(wo, "window-status-fg", 8);
	options_set_number(wo, "window-status-alert-attr", GRID_ATTR_REVERSE);
	options_set_number(wo, "window-status-alert-bg", 8);
	options_set_number(wo, "window-status-alert-fg", 8);
	options_set_string(wo, "window-status-format", "#I:#W#F");
	options_set_string(wo, "window-status-current-format", "#I:#W#F");
	options_set_string(wo, "word-separators", " -_@");
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

	keys = MODEKEY_EMACS;
	if ((s = getenv("VISUAL")) != NULL || (s = getenv("EDITOR")) != NULL) {
		if (strrchr(s, '/') != NULL)
			s = strrchr(s, '/') + 1;
		if (strstr(s, "vi") != NULL)
			keys = MODEKEY_VI;
	}
	options_set_number(so, "status-keys", keys);
	options_set_number(wo, "mode-keys", keys);

	/* Locate the configuration file. */
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
	 * Figure out the socket path. If specified on the command-line with -S
	 * or -L, use it, otherwise try $TMUX or assume -L default.
	 */
	parseenvironment();
	if (path == NULL) {
		/* If no -L, use the environment. */
		if (label == NULL) {
			if (environ_path != NULL)
				path = xstrdup(environ_path);
			else
				label = xstrdup("default");
		}

		/* -L or default set. */
		if (label != NULL) {
			if ((path = makesocketpath(label)) == NULL) {
				log_warn("can't create socket");
				exit(1);
			}
		}
	}
	if (label != NULL)
		xfree(label);
	if (realpath(path, socket_path) == NULL)
		strlcpy(socket_path, path, sizeof socket_path);
	xfree(path);

#ifdef HAVE_SETPROCTITLE
	/* Set process title. */
	setproctitle("%s (%s)", __progname, socket_path);
#endif

	/* Pass control to the client. */
#ifdef HAVE_BROKEN_KQUEUE
	if (setenv("EVENT_NOKQUEUE", "1", 1) != 0)
		fatal("setenv failed");
#endif
#ifdef HAVE_BROKEN_POLL
	if (setenv("EVENT_NOPOLL", "1", 1) != 0)
		fatal("setenv failed");
#endif
	ev_base = event_init();
#ifdef HAVE_BROKEN_KQUEUE
	unsetenv("EVENT_NOKQUEUE");
#endif
#ifdef HAVE_BROKEN_POLL
	unsetenv("EVENT_NOPOLL");
#endif
	exit(client_main(argc, argv, flags));
}
