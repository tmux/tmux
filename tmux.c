/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/utsname.h>

#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

struct options	*global_options;	/* server options */
struct options	*global_s_options;	/* session options */
struct options	*global_w_options;	/* window options */
struct environ	*global_environ;

struct timeval	 start_time;
const char	*socket_path;
int		 ptm_fd = -1;
const char	*shell_command;

static __dead void	 usage(void);
static char		*make_label(const char *, char **);

static int		 areshell(const char *);
static const char	*getshell(void);

static __dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-2CDlNuvV] [-c shell-command] [-f file] [-L socket-name]\n"
	    "            [-S socket-path] [-T features] [command [flags]]\n",
	    getprogname());
	exit(1);
}

static const char *
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
	if (shell == NULL || *shell != '/')
		return (0);
	if (areshell(shell))
		return (0);
	if (access(shell, X_OK) != 0)
		return (0);
	return (1);
}

static int
areshell(const char *shell)
{
	const char	*progname, *ptr;

	if ((ptr = strrchr(shell, '/')) != NULL)
		ptr++;
	else
		ptr = shell;
	progname = getprogname();
	if (*progname == '-')
		progname++;
	if (strcmp(ptr, progname) == 0)
		return (1);
	return (0);
}

static char *
expand_path(const char *path, const char *home)
{
	char			*expanded, *name;
	const char		*end;
	struct environ_entry	*value;

	if (strncmp(path, "~/", 2) == 0) {
		if (home == NULL)
			return (NULL);
		xasprintf(&expanded, "%s%s", home, path + 1);
		return (expanded);
	}

	if (*path == '$') {
		end = strchr(path, '/');
		if (end == NULL)
			name = xstrdup(path + 1);
		else
			name = xstrndup(path + 1, end - path - 1);
		value = environ_find(global_environ, name);
		free(name);
		if (value == NULL)
			return (NULL);
		if (end == NULL)
			end = "";
		xasprintf(&expanded, "%s%s", value->value, end);
		return (expanded);
	}

	return (xstrdup(path));
}

static void
expand_paths(const char *s, char ***paths, u_int *n, int ignore_errors)
{
	const char	*home = find_home();
	char		*copy, *next, *tmp, resolved[PATH_MAX], *expanded;
	char		*path;
	u_int		 i;

	*paths = NULL;
	*n = 0;

	copy = tmp = xstrdup(s);
	while ((next = strsep(&tmp, ":")) != NULL) {
		expanded = expand_path(next, home);
		if (expanded == NULL) {
			log_debug("%s: invalid path: %s", __func__, next);
			continue;
		}
		if (realpath(expanded, resolved) == NULL) {
			log_debug("%s: realpath(\"%s\") failed: %s", __func__,
			    expanded, strerror(errno));
			if (ignore_errors) {
				free(expanded);
				continue;
			}
			path = expanded;
		} else {
			path = xstrdup(resolved);
			free(expanded);
		}
		for (i = 0; i < *n; i++) {
			if (strcmp(path, (*paths)[i]) == 0)
				break;
		}
		if (i != *n) {
			log_debug("%s: duplicate path: %s", __func__, path);
			free(path);
			continue;
		}
		*paths = xreallocarray(*paths, (*n) + 1, sizeof *paths);
		(*paths)[(*n)++] = path;
	}
	free(copy);
}

static char *
make_label(const char *label, char **cause)
{
	char		**paths, *path, *base;
	u_int		  i, n;
	struct stat	  sb;
	uid_t		  uid;

	*cause = NULL;
	if (label == NULL)
		label = "default";
	uid = getuid();

	expand_paths(TMUX_SOCK, &paths, &n, 1);
	if (n == 0) {
		xasprintf(cause, "no suitable socket path");
		return (NULL);
	}
	path = paths[0]; /* can only have one socket! */
	for (i = 1; i < n; i++)
		free(paths[i]);
	free(paths);

	xasprintf(&base, "%s/tmux-%ld", path, (long)uid);
	if (mkdir(base, S_IRWXU) != 0 && errno != EEXIST)
		goto fail;
	if (lstat(base, &sb) != 0)
		goto fail;
	if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		goto fail;
	}
	if (sb.st_uid != uid || (sb.st_mode & S_IRWXO) != 0) {
		errno = EACCES;
		goto fail;
	}
	xasprintf(&path, "%s/%s", base, label);
	free(base);
	return (path);

fail:
	xasprintf(cause, "error creating %s (%s)", base, strerror(errno));
	free(base);
	return (NULL);
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

uint64_t
get_timer(void)
{
	struct timespec	ts;

	/*
	 * We want a timestamp in milliseconds suitable for time measurement,
	 * so prefer the monotonic clock.
	 */
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		clock_gettime(CLOCK_REALTIME, &ts);
	return ((ts.tv_sec * 1000ULL) + (ts.tv_nsec / 1000000ULL));
}

const char *
sig2name(int signo)
{
     static char	s[11];

#ifdef HAVE_SYS_SIGNAME
     if (signo > 0 && signo < NSIG)
	     return (sys_signame[signo]);
#endif
     xsnprintf(s, sizeof s, "%d", signo);
     return (s);
}

const char *
find_cwd(void)
{
	char		 resolved1[PATH_MAX], resolved2[PATH_MAX];
	static char	 cwd[PATH_MAX];
	const char	*pwd;

	if (getcwd(cwd, sizeof cwd) == NULL)
		return (NULL);
	if ((pwd = getenv("PWD")) == NULL || *pwd == '\0')
		return (cwd);

	/*
	 * We want to use PWD so that symbolic links are maintained,
	 * but only if it matches the actual working directory.
	 */
	if (realpath(pwd, resolved1) == NULL)
		return (cwd);
	if (realpath(cwd, resolved2) == NULL)
		return (cwd);
	if (strcmp(resolved1, resolved2) != 0)
		return (cwd);
	return (pwd);
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

const char *
getversion(void)
{
	return TMUX_VERSION;
}

int
main(int argc, char **argv)
{
	char					*path = NULL, *label = NULL;
	char					*cause, **var;
	const char				*s, *cwd;
	int					 opt, keys, feat = 0, fflag = 0;
	uint64_t				 flags = 0;
	const struct options_table_entry	*oe;
	u_int					 i;

	if (setlocale(LC_CTYPE, "en_US.UTF-8") == NULL &&
	    setlocale(LC_CTYPE, "C.UTF-8") == NULL) {
		if (setlocale(LC_CTYPE, "") == NULL)
			errx(1, "invalid LC_ALL, LC_CTYPE or LANG");
		s = nl_langinfo(CODESET);
		if (strcasecmp(s, "UTF-8") != 0 && strcasecmp(s, "UTF8") != 0)
			errx(1, "need UTF-8 locale (LC_CTYPE) but have %s", s);
	}

	setlocale(LC_TIME, "");
	tzset();

	if (**argv == '-')
		flags = CLIENT_LOGIN;

	global_environ = environ_create();
	for (var = environ; *var != NULL; var++)
		environ_put(global_environ, *var, 0);
	if ((cwd = find_cwd()) != NULL)
		environ_set(global_environ, "PWD", 0, "%s", cwd);
	expand_paths(TMUX_CONF, &cfg_files, &cfg_nfiles, 1);

	while ((opt = getopt(argc, argv, "2c:CDdf:lL:NqS:T:uUvV")) != -1) {
		switch (opt) {
		case '2':
			tty_add_features(&feat, "256", ":,");
			break;
		case 'c':
			shell_command = optarg;
			break;
		case 'D':
			flags |= CLIENT_NOFORK;
			break;
		case 'C':
			if (flags & CLIENT_CONTROL)
				flags |= CLIENT_CONTROLCONTROL;
			else
				flags |= CLIENT_CONTROL;
			break;
		case 'f':
			if (!fflag) {
				fflag = 1;
				for (i = 0; i < cfg_nfiles; i++)
					free(cfg_files[i]);
				cfg_nfiles = 0;
			}
			cfg_files = xreallocarray(cfg_files, cfg_nfiles + 1,
			    sizeof *cfg_files);
			cfg_files[cfg_nfiles++] = xstrdup(optarg);
			cfg_quiet = 0;
			break;
 		case 'V':
			printf("%s %s\n", getprogname(), getversion());
 			exit(0);
		case 'l':
			flags |= CLIENT_LOGIN;
			break;
		case 'L':
			free(label);
			label = xstrdup(optarg);
			break;
		case 'N':
			flags |= CLIENT_NOSTARTSERVER;
			break;
		case 'q':
			break;
		case 'S':
			free(path);
			path = xstrdup(optarg);
			break;
		case 'T':
			tty_add_features(&feat, optarg, ":,");
			break;
		case 'u':
			flags |= CLIENT_UTF8;
			break;
		case 'v':
			log_add_level();
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (shell_command != NULL && argc != 0)
		usage();
	if ((flags & CLIENT_NOFORK) && argc != 0)
		usage();

	if ((ptm_fd = getptmfd()) == -1)
		err(1, "getptmfd");
	if (pledge("stdio rpath wpath cpath flock fattr unix getpw sendfd "
	    "recvfd proc exec tty ps", NULL) != 0)
		err(1, "pledge");

	/*
	 * tmux is a UTF-8 terminal, so if TMUX is set, assume UTF-8.
	 * Otherwise, if the user has set LC_ALL, LC_CTYPE or LANG to contain
	 * UTF-8, it is a safe assumption that either they are using a UTF-8
	 * terminal, or if not they know that output from UTF-8-capable
	 * programs may be wrong.
	 */
	if (getenv("TMUX") != NULL)
		flags |= CLIENT_UTF8;
	else {
		s = getenv("LC_ALL");
		if (s == NULL || *s == '\0')
			s = getenv("LC_CTYPE");
		if (s == NULL || *s == '\0')
			s = getenv("LANG");
		if (s == NULL || *s == '\0')
			s = "";
		if (strcasestr(s, "UTF-8") != NULL ||
		    strcasestr(s, "UTF8") != NULL)
			flags |= CLIENT_UTF8;
	}

	global_options = options_create(NULL);
	global_s_options = options_create(NULL);
	global_w_options = options_create(NULL);
	for (oe = options_table; oe->name != NULL; oe++) {
		if (oe->scope & OPTIONS_TABLE_SERVER)
			options_default(global_options, oe);
		if (oe->scope & OPTIONS_TABLE_SESSION)
			options_default(global_s_options, oe);
		if (oe->scope & OPTIONS_TABLE_WINDOW)
			options_default(global_w_options, oe);
	}

	/*
	 * The default shell comes from SHELL or from the user's passwd entry
	 * if available.
	 */
	options_set_string(global_s_options, "default-shell", 0, "%s",
	    getshell());

	/* Override keys to vi if VISUAL or EDITOR are set. */
	if ((s = getenv("VISUAL")) != NULL || (s = getenv("EDITOR")) != NULL) {
		options_set_string(global_options, "editor", 0, "%s", s);
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
	 * If socket is specified on the command-line with -S or -L, it is
	 * used. Otherwise, $TMUX is checked and if that fails "default" is
	 * used.
	 */
	if (path == NULL && label == NULL) {
		s = getenv("TMUX");
		if (s != NULL && *s != '\0' && *s != ',') {
			path = xstrdup(s);
			path[strcspn(path, ",")] = '\0';
		}
	}
	if (path == NULL) {
		if ((path = make_label(label, &cause)) == NULL) {
			if (cause != NULL) {
				fprintf(stderr, "%s\n", cause);
				free(cause);
			}
			exit(1);
		}
		flags |= CLIENT_DEFAULTSOCKET;
	}
	socket_path = path;
	free(label);

	/* Pass control to the client. */
	exit(client_main(osdep_event_init(), argc, argv, flags, feat));
}
