/* $OpenBSD$ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Set up the environment and create a new window and pane or a new pane.
 *
 * We need to set up the following items:
 *
 * - history limit, comes from the session;
 *
 * - base index, comes from the session;
 *
 * - current working directory, may be specified - if it isn't it comes from
 *   either the client or the session;
 *
 * - PATH variable, comes from the client if any, otherwise from the session
 *   environment;
 *
 * - shell, comes from default-shell;
 *
 * - termios, comes from the session;
 *
 * - remaining environment, comes from the session.
 */

static void
spawn_log(const char *from, struct spawn_context *sc)
{
	struct session		*s = sc->s;
	struct winlink		*wl = sc->wl;
	struct window_pane	*wp0 = sc->wp0;
	const char		*name = cmdq_get_name(sc->item);
	char			 tmp[128];

	log_debug("%s: %s, flags=%#x", from, name, sc->flags);

	if (wl != NULL && wp0 != NULL)
		xsnprintf(tmp, sizeof tmp, "wl=%d wp0=%%%u", wl->idx, wp0->id);
	else if (wl != NULL)
		xsnprintf(tmp, sizeof tmp, "wl=%d wp0=none", wl->idx);
	else if (wp0 != NULL)
		xsnprintf(tmp, sizeof tmp, "wl=none wp0=%%%u", wp0->id);
	else
		xsnprintf(tmp, sizeof tmp, "wl=none wp0=none");
	log_debug("%s: s=$%u %s idx=%d", from, s->id, tmp, sc->idx);
	log_debug("%s: name=%s", from, sc->name == NULL ? "none" : sc->name);
}

struct winlink *
spawn_window(struct spawn_context *sc, char **cause)
{
	struct cmdq_item	*item = sc->item;
	struct client		*c = cmdq_get_client(item);
	struct session		*s = sc->s;
	struct window		*w;
	struct window_pane	*wp;
	struct winlink		*wl;
	int			 idx = sc->idx;
	u_int			 sx, sy, xpixel, ypixel;

	spawn_log(__func__, sc);

	/*
	 * If the window already exists, we are respawning, so destroy all the
	 * panes except one.
	 */
	if (sc->flags & SPAWN_RESPAWN) {
		w = sc->wl->window;
		if (~sc->flags & SPAWN_KILL) {
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (wp->fd != -1)
					break;
			}
			if (wp != NULL) {
				xasprintf(cause, "window %s:%d still active",
				    s->name, sc->wl->idx);
				return (NULL);
			}
		}

		sc->wp0 = TAILQ_FIRST(&w->panes);
		TAILQ_REMOVE(&w->panes, sc->wp0, entry);

		layout_free(w);
		window_destroy_panes(w);

		TAILQ_INSERT_HEAD(&w->panes, sc->wp0, entry);
		window_pane_resize(sc->wp0, w->sx, w->sy);

		layout_init(w, sc->wp0);
		w->active = NULL;
		window_set_active_pane(w, sc->wp0, 0);
	}

	/*
	 * Otherwise we have no window so we will need to create one. First
	 * check if the given index already exists and destroy it if so.
	 */
	if ((~sc->flags & SPAWN_RESPAWN) && idx != -1) {
		wl = winlink_find_by_index(&s->windows, idx);
		if (wl != NULL && (~sc->flags & SPAWN_KILL)) {
			xasprintf(cause, "index %d in use", idx);
			return (NULL);
		}
		if (wl != NULL) {
			/*
			 * Can't use session_detach as it will destroy session
			 * if this makes it empty.
			 */
			wl->flags &= ~WINLINK_ALERTFLAGS;
			notify_session_window("window-unlinked", s, wl->window);
			winlink_stack_remove(&s->lastw, wl);
			winlink_remove(&s->windows, wl);

			if (s->curw == wl) {
				s->curw = NULL;
				sc->flags &= ~SPAWN_DETACHED;
			}
		}
	}

	/* Then create a window if needed. */
	if (~sc->flags & SPAWN_RESPAWN) {
		if (idx == -1)
			idx = -1 - options_get_number(s->options, "base-index");
		if ((sc->wl = winlink_add(&s->windows, idx)) == NULL) {
			xasprintf(cause, "couldn't add window %d", idx);
			return (NULL);
		}
		default_window_size(sc->tc, s, NULL, &sx, &sy, &xpixel, &ypixel,
		    -1);
		if ((w = window_create(sx, sy, xpixel, ypixel)) == NULL) {
			winlink_remove(&s->windows, sc->wl);
			xasprintf(cause, "couldn't create window %d", idx);
			return (NULL);
		}
		if (s->curw == NULL)
			s->curw = sc->wl;
		sc->wl->session = s;
		w->latest = sc->tc;
		winlink_set_window(sc->wl, w);
	} else
		w = NULL;
	sc->flags |= SPAWN_NONOTIFY;

	/* Spawn the pane. */
	wp = spawn_pane(sc, cause);
	if (wp == NULL) {
		if (~sc->flags & SPAWN_RESPAWN)
			winlink_remove(&s->windows, sc->wl);
		return (NULL);
	}

	/* Set the name of the new window. */
	if (~sc->flags & SPAWN_RESPAWN) {
		free(w->name);
		if (sc->name != NULL) {
			w->name = format_single(item, sc->name, c, s, NULL,
			    NULL);
			options_set_number(w->options, "automatic-rename", 0);
		} else
			w->name = default_window_name(w);
	}

	/* Switch to the new window if required. */
	if (~sc->flags & SPAWN_DETACHED)
		session_select(s, sc->wl->idx);

	/* Fire notification if new window. */
	if (~sc->flags & SPAWN_RESPAWN)
		notify_session_window("window-linked", s, w);

	session_group_synchronize_from(s);
	return (sc->wl);
}

struct window_pane *
spawn_pane(struct spawn_context *sc, char **cause)
{
	struct cmdq_item	 *item = sc->item;
	struct cmd_find_state	 *target = cmdq_get_target(item);
	struct client		 *c = cmdq_get_client(item);
	struct session		 *s = sc->s;
	struct window		 *w = sc->wl->window;
	struct window_pane	 *new_wp;
	struct environ		 *child;
	struct environ_entry	 *ee;
	char			**argv, *cp, **argvp, *argv0, *cwd, *new_cwd;
	const char		 *cmd, *tmp;
	int			  argc;
	u_int			  idx;
	struct termios		  now;
	u_int			  hlimit;
	struct winsize		  ws;
	sigset_t		  set, oldset;
	key_code		  key;

	spawn_log(__func__, sc);

	/*
	 * Work out the current working directory. If respawning, use
	 * the pane's stored one unless specified.
	 */
	if (sc->cwd != NULL) {
		cwd = format_single(item, sc->cwd, c, target->s, NULL, NULL);
		if (*cwd != '/') {
			xasprintf(&new_cwd, "%s/%s", server_client_get_cwd(c,
			    target->s), cwd);
			free(cwd);
			cwd = new_cwd;
		}
	} else if (~sc->flags & SPAWN_RESPAWN)
		cwd = xstrdup(server_client_get_cwd(c, target->s));
	else
		cwd = NULL;

	/*
	 * If we are respawning then get rid of the old process. Otherwise
	 * either create a new cell or assign to the one we are given.
	 */
	hlimit = options_get_number(s->options, "history-limit");
	if (sc->flags & SPAWN_RESPAWN) {
		if (sc->wp0->fd != -1 && (~sc->flags & SPAWN_KILL)) {
			window_pane_index(sc->wp0, &idx);
			xasprintf(cause, "pane %s:%d.%u still active",
			    s->name, sc->wl->idx, idx);
			free(cwd);
			return (NULL);
		}
		if (sc->wp0->fd != -1) {
			bufferevent_free(sc->wp0->event);
			close(sc->wp0->fd);
		}
		window_pane_reset_mode_all(sc->wp0);
		screen_reinit(&sc->wp0->base);
		input_free(sc->wp0->ictx);
		sc->wp0->ictx = NULL;
		new_wp = sc->wp0;
		new_wp->flags &= ~(PANE_STATUSREADY|PANE_STATUSDRAWN);
	} else if (sc->lc == NULL) {
		new_wp = window_add_pane(w, NULL, hlimit, sc->flags);
		layout_init(w, new_wp);
	} else {
		new_wp = window_add_pane(w, sc->wp0, hlimit, sc->flags);
		if (sc->flags & SPAWN_ZOOM)
			layout_assign_pane(sc->lc, new_wp, 1);
		else
			layout_assign_pane(sc->lc, new_wp, 0);
	}

	/*
	 * Now we have a pane with nothing running in it ready for the new
	 * process. Work out the command and arguments and store the working
	 * directory.
	 */
	if (sc->argc == 0 && (~sc->flags & SPAWN_RESPAWN)) {
		cmd = options_get_string(s->options, "default-command");
		if (cmd != NULL && *cmd != '\0') {
			argc = 1;
			argv = (char **)&cmd;
		} else {
			argc = 0;
			argv = NULL;
		}
	} else {
		argc = sc->argc;
		argv = sc->argv;
	}
	if (cwd != NULL) {
		free(new_wp->cwd);
		new_wp->cwd = cwd;
	}

	/*
	 * Replace the stored arguments if there are new ones. If not, the
	 * existing ones will be used (they will only exist for respawn).
	 */
	if (argc > 0) {
		cmd_free_argv(new_wp->argc, new_wp->argv);
		new_wp->argc = argc;
		new_wp->argv = cmd_copy_argv(argc, argv);
	}

	/* Create an environment for this pane. */
	child = environ_for_session(s, 0);
	if (sc->environ != NULL)
		environ_copy(sc->environ, child);
	environ_set(child, "TMUX_PANE", 0, "%%%u", new_wp->id);

	/*
	 * Then the PATH environment variable. The session one is replaced from
	 * the client if there is one because otherwise running "tmux new
	 * myprogram" wouldn't work if myprogram isn't in the session's path.
	 */
	if (c != NULL && c->session == NULL) { /* only unattached clients */
		ee = environ_find(c->environ, "PATH");
		if (ee != NULL)
			environ_set(child, "PATH", 0, "%s", ee->value);
	}
	if (environ_find(child, "PATH") == NULL)
		environ_set(child, "PATH", 0, "%s", _PATH_DEFPATH);

	/* Then the shell. If respawning, use the old one. */
	if (~sc->flags & SPAWN_RESPAWN) {
		tmp = options_get_string(s->options, "default-shell");
		if (!checkshell(tmp))
			tmp = _PATH_BSHELL;
		free(new_wp->shell);
		new_wp->shell = xstrdup(tmp);
	}
	environ_set(child, "SHELL", 0, "%s", new_wp->shell);

	/* Log the arguments we are going to use. */
	log_debug("%s: shell=%s", __func__, new_wp->shell);
	if (new_wp->argc != 0) {
		cp = cmd_stringify_argv(new_wp->argc, new_wp->argv);
		log_debug("%s: cmd=%s", __func__, cp);
		free(cp);
	}
	log_debug("%s: cwd=%s", __func__, new_wp->cwd);
	cmd_log_argv(new_wp->argc, new_wp->argv, "%s", __func__);
	environ_log(child, "%s: environment ", __func__);

	/* Initialize the window size. */
	memset(&ws, 0, sizeof ws);
	ws.ws_col = screen_size_x(&new_wp->base);
	ws.ws_row = screen_size_y(&new_wp->base);
	ws.ws_xpixel = w->xpixel * ws.ws_col;
	ws.ws_ypixel = w->ypixel * ws.ws_row;

	/* Block signals until fork has completed. */
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oldset);

	/* If the command is empty, don't fork a child process. */
	if (sc->flags & SPAWN_EMPTY) {
		new_wp->flags |= PANE_EMPTY;
		new_wp->base.mode &= ~MODE_CURSOR;
		new_wp->base.mode |= MODE_CRLF;
		goto complete;
	}

	/* Fork the new process. */
	new_wp->pid = fdforkpty(ptm_fd, &new_wp->fd, new_wp->tty, NULL, &ws);
	if (new_wp->pid == -1) {
		xasprintf(cause, "fork failed: %s", strerror(errno));
		new_wp->fd = -1;
		if (~sc->flags & SPAWN_RESPAWN) {
			server_client_remove_pane(new_wp);
			layout_close_pane(new_wp);
			window_remove_pane(w, new_wp);
		}
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		environ_free(child);
		return (NULL);
	}

	/* In the parent process, everything is done now. */
	if (new_wp->pid != 0) {
#if defined(HAVE_SYSTEMD) && defined(ENABLE_CGROUPS)
		/*
		 * Move the child process into a new cgroup for systemd-oomd
		 * isolation.
		 */
		if (systemd_move_pid_to_new_cgroup(new_wp->pid, cause) < 0) {
			log_debug("%s: moving pane to new cgroup failed: %s",
			    __func__, *cause);
			free (*cause);
		}
#endif
		goto complete;
	}

	/*
	 * Child process. Change to the working directory or home if that
	 * fails.
	 */
	if (chdir(new_wp->cwd) == 0)
		environ_set(child, "PWD", 0, "%s", new_wp->cwd);
	else if ((tmp = find_home()) != NULL && chdir(tmp) == 0)
		environ_set(child, "PWD", 0, "%s", tmp);
	else if (chdir("/") == 0)
		environ_set(child, "PWD", 0, "/");
	else
		fatal("chdir failed");

	/*
	 * Update terminal escape characters from the session if available and
	 * force VERASE to tmux's backspace.
	 */
	if (tcgetattr(STDIN_FILENO, &now) != 0)
		_exit(1);
	if (s->tio != NULL)
		memcpy(now.c_cc, s->tio->c_cc, sizeof now.c_cc);
	key = options_get_number(global_options, "backspace");
	if (key >= 0x7f)
		now.c_cc[VERASE] = '\177';
	else
		now.c_cc[VERASE] = key;
#ifdef IUTF8
	now.c_iflag |= IUTF8;
#endif
	if (tcsetattr(STDIN_FILENO, TCSANOW, &now) != 0)
		_exit(1);

	/* Clean up file descriptors and signals and update the environment. */
	proc_clear_signals(server_proc, 1);
	closefrom(STDERR_FILENO + 1);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	log_close();
	environ_push(child);

	/*
	 * If given multiple arguments, use execvp(). Copy the arguments to
	 * ensure they end in a NULL.
	 */
	if (new_wp->argc != 0 && new_wp->argc != 1) {
		argvp = cmd_copy_argv(new_wp->argc, new_wp->argv);
		execvp(argvp[0], argvp);
		_exit(1);
	}

	/*
	 * If one argument, pass it to $SHELL -c. Otherwise create a login
	 * shell.
	 */
	cp = strrchr(new_wp->shell, '/');
	if (new_wp->argc == 1) {
		tmp = new_wp->argv[0];
		if (cp != NULL && cp[1] != '\0')
			xasprintf(&argv0, "%s", cp + 1);
		else
			xasprintf(&argv0, "%s", new_wp->shell);
		execl(new_wp->shell, argv0, "-c", tmp, (char *)NULL);
		_exit(1);
	}
	if (cp != NULL && cp[1] != '\0')
		xasprintf(&argv0, "-%s", cp + 1);
	else
		xasprintf(&argv0, "-%s", new_wp->shell);
	execl(new_wp->shell, argv0, (char *)NULL);
	_exit(1);

complete:
#ifdef HAVE_UTEMPTER
	if (~new_wp->flags & PANE_EMPTY) {
		xasprintf(&cp, "tmux(%lu).%%%u", (long)getpid(), new_wp->id);
		utempter_add_record(new_wp->fd, cp);
		kill(getpid(), SIGCHLD);
		free(cp);
	}
#endif

	new_wp->flags &= ~PANE_EXITED;

	sigprocmask(SIG_SETMASK, &oldset, NULL);
	window_pane_set_event(new_wp);

	environ_free(child);

	if (sc->flags & SPAWN_RESPAWN)
		return (new_wp);
	if ((~sc->flags & SPAWN_DETACHED) || w->active == NULL) {
		if (sc->flags & SPAWN_NONOTIFY)
			window_set_active_pane(w, new_wp, 0);
		else
			window_set_active_pane(w, new_wp, 1);
	}
	if (~sc->flags & SPAWN_NONOTIFY)
		notify_window("window-layout-changed", w);
	return (new_wp);
}
