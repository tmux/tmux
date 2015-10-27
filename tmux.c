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

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

#if defined(DEBUG) && defined(__OpenBSD__)
extern char	*malloc_options;
#endif

struct options	*global_options;	/* server options */
struct options	*global_s_options;	/* session options */
struct options	*global_w_options;	/* window options */
struct environ	 global_environ;

char		*shell_cmd;
int		 debug_level;
time_t		 start_time;
char		 socket_path[PATH_MAX];

__dead void	 usage(void);
char 		*makesocketpath(const char *);

#ifndef HAVE___PROGNAME
char      *__progname = (char *) "tmux";
#endif

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-2CluvV] [-c shell-command] [-f file] [-L socket-name]\n"
	    "            [-S socket-path] [command [flags]]\n",
	    __progname);
	exit(1);
}

void
logfile(const char *name)
{
	char	*path;

	if (debug_level > 0) {
		xasprintf(&path, "tmux-%s-%ld.log", name, (long) getpid());
		log_open(path);
		free(path);
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
	if (shell == NULL || *shell == '\0' || *shell != '/')
		return (0);
	if (areshell(shell))
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
makesocketpath(const char *label)
{
	char		base[PATH_MAX], realbase[PATH_MAX], *path, *s;
	struct stat	sb;
	u_int		uid;

	uid = getuid();
	if ((s = getenv("TMUX_TMPDIR")) != NULL && *s != '\0')
		xsnprintf(base, sizeof base, "%s/tmux-%u", s, uid);
	else
		xsnprintf(base, sizeof base, "%s/tmux-%u", _PATH_TMP, uid);

	if (mkdir(base, S_IRWXU) != 0 && errno != EEXIST)
		return (NULL);

	if (lstat(base, &sb) != 0)
		return (NULL);
	if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return (NULL);
	}
	if (sb.st_uid != uid || (sb.st_mode & S_IRWXO) != 0) {
		errno = EACCES;
		return (NULL);
	}

	if (realpath(base, realbase) == NULL)
		strlcpy(realbase, base, sizeof realbase);

	xasprintf(&path, "%s/%s", realbase, label);
	return (path);
}

void
setblocking(int fd, int state)
{
	int mode;

	if ((mode = fcntl(fd, F_GETFL)) != -1) {
		if (!state)
			mode |= O_NONBLOCK;
		else
			mode &= ~O_NONBLOCK;
		fcntl(fd, F_SETFL, mode);
	}
}

const char *
find_home(void)
{
	struct passwd		*pw;
	static const char	*home;

	if (home != NULL)
		return (home);

	home = getenv("HOME");
	if (home == NULL || *home == '\0') {
		pw = getpwuid(getuid());
		if (pw != NULL)
			home = pw->pw_dir;
		else
			home = NULL;
	}

	return (home);
}

int
main(int argc, char **argv)
{
	char	*s, *path, *label, **var, tmp[PATH_MAX];
	int	 opt, flags, keys;

#if defined(DEBUG) && defined(__OpenBSD__)
	malloc_options = (char *) "AFGJPX";
#endif

	setlocale(LC_TIME, "");
	tzset();

	if (**argv == '-')
		flags = CLIENT_LOGIN;
	else
		flags = 0;

	label = path = NULL;
	while ((opt = getopt(argc, argv, "2c:Cdf:lL:qS:uUVv")) != -1) {
		switch (opt) {
		case '2':
			flags |= CLIENT_256COLOURS;
			break;
		case 'c':
			free(shell_cmd);
			shell_cmd = xstrdup(optarg);
			break;
		case 'C':
			if (flags & CLIENT_CONTROL)
				flags |= CLIENT_CONTROLCONTROL;
			else
				flags |= CLIENT_CONTROL;
			break;
		case 'V':
			printf("%s %s\n", __progname, VERSION);
			exit(0);
		case 'f':
			set_cfg_file(optarg);
			break;
		case 'l':
			flags |= CLIENT_LOGIN;
			break;
		case 'L':
			free(label);
			label = xstrdup(optarg);
			break;
		case 'q':
			break;
		case 'S':
			free(path);
			path = xstrdup(optarg);
			break;
		case 'u':
			flags |= CLIENT_UTF8;
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

#ifdef __OpenBSD__
	if (pledge("stdio rpath wpath cpath flock fattr unix sendfd recvfd "
	    "proc exec tty ps", NULL) != 0)
		err(1, "pledge");
#endif

	if (!(flags & CLIENT_UTF8)) {
		/*
		 * If the user has set whichever of LC_ALL, LC_CTYPE or LANG
		 * exist (in that order) to contain UTF-8, it is a safe
		 * assumption that either they are using a UTF-8 terminal, or
		 * if not they know that output from UTF-8-capable programs may
		 * be wrong.
		 */
		if ((s = getenv("LC_ALL")) == NULL || *s == '\0') {
			if ((s = getenv("LC_CTYPE")) == NULL || *s == '\0')
				s = getenv("LANG");
		}
		if (s != NULL && (strcasestr(s, "UTF-8") != NULL ||
		    strcasestr(s, "UTF8") != NULL))
			flags |= CLIENT_UTF8;
	}

	environ_init(&global_environ);
	for (var = environ; *var != NULL; var++)
		environ_put(&global_environ, *var);
	if (getcwd(tmp, sizeof tmp) != NULL)
		environ_set(&global_environ, "PWD", tmp);

	global_options = options_create(NULL);
	options_table_populate_tree(server_options_table, global_options);

	global_s_options = options_create(NULL);
	options_table_populate_tree(session_options_table, global_s_options);
	options_set_string(global_s_options, "default-shell", "%s", getshell());

	global_w_options = options_create(NULL);
	options_table_populate_tree(window_options_table, global_w_options);

	/* Enable UTF-8 if the first client is on UTF-8 terminal. */
	if (flags & CLIENT_UTF8) {
		options_set_number(global_s_options, "status-utf8", 1);
		options_set_number(global_s_options, "mouse-utf8", 1);
		options_set_number(global_w_options, "utf8", 1);
	}

	/* Override keys to vi if VISUAL or EDITOR are set. */
	if ((s = getenv("VISUAL")) != NULL || (s = getenv("EDITOR")) != NULL) {
		if (strrchr(s, '/') != NULL)
			s = strrchr(s, '/') + 1;
		if (strstr(s, "vi") != NULL)
			keys = MODEKEY_VI;
		else
			keys = MODEKEY_EMACS;
		options_set_number(global_s_options, "status-keys", keys);
		options_set_number(global_w_options, "mode-keys", keys);
	}

	/*
	 * Figure out the socket path. If specified on the command-line with -S
	 * or -L, use it, otherwise try $TMUX or assume -L default.
	 */
	if (path == NULL) {
		/* If no -L, use the environment. */
		if (label == NULL) {
			s = getenv("TMUX");
			if (s != NULL) {
				path = xstrdup(s);
				path[strcspn (path, ",")] = '\0';
				if (*path == '\0') {
					free(path);
					label = xstrdup("default");
				}
			} else
				label = xstrdup("default");
		}

		/* -L or default set. */
		if (label != NULL) {
			if ((path = makesocketpath(label)) == NULL) {
				fprintf(stderr, "can't create socket: %s\n",
				    strerror(errno));
				exit(1);
			}
		}
	}
	free(label);

	if (strlcpy(socket_path, path, sizeof socket_path) >=
	    sizeof socket_path) {
		fprintf(stderr, "socket path too long: %s\n", path);
		exit(1);
	}
	free(path);

	/* Pass control to the client. */
	exit(client_main(osdep_event_init(), argc, argv, flags));
}
