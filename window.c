/* $Id$ */

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

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Each window is attached to a number of panes, each of which is a pty. This
 * file contains code to handle them.
 *
 * A pane has two buffers attached, these are filled and emptied by the main
 * server poll loop. Output data is received from pty's in screen format,
 * translated and returned as a series of escape sequences and strings via
 * input_parse (in input.c). Input data is received as key codes and written
 * directly via input_key.
 *
 * Each pane also has a "virtual" screen (screen.c) which contains the current
 * state and is redisplayed when the window is reattached to a client.
 *
 * Windows are stored directly on a global array and wrapped in any number of
 * winlink structs to be linked onto local session RB trees. A reference count
 * is maintained and a window removed from the global list and destroyed when
 * it reaches zero.
 */

/* Global window list. */
struct windows windows;

/* Global panes tree. */
struct window_pane_tree all_window_panes;
u_int	next_window_pane_id;
u_int	next_window_id;
u_int	next_active_point;

void	window_pane_timer_callback(int, short, void *);
void	window_pane_read_callback(struct bufferevent *, void *);
void	window_pane_error_callback(struct bufferevent *, short, void *);

struct window_pane *window_pane_choose_best(struct window_pane_list *);

RB_GENERATE(winlinks, winlink, entry, winlink_cmp);

int
winlink_cmp(struct winlink *wl1, struct winlink *wl2)
{
	return (wl1->idx - wl2->idx);
}

RB_GENERATE(window_pane_tree, window_pane, tree_entry, window_pane_cmp);

int
window_pane_cmp(struct window_pane *wp1, struct window_pane *wp2)
{
	return (wp1->id - wp2->id);
}

struct winlink *
winlink_find_by_window(struct winlinks *wwl, struct window *w)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, wwl) {
		if (wl->window == w)
			return (wl);
	}

	return (NULL);
}

struct winlink *
winlink_find_by_index(struct winlinks *wwl, int idx)
{
	struct winlink	wl;

	if (idx < 0)
		fatalx("bad index");

	wl.idx = idx;
	return (RB_FIND(winlinks, wwl, &wl));
}

struct winlink *
winlink_find_by_window_id(struct winlinks *wwl, u_int id)
{
	struct winlink *wl;

	RB_FOREACH(wl, winlinks, wwl) {
		if (wl->window->id == id)
			return (wl);
	}
	return (NULL);
}

int
winlink_next_index(struct winlinks *wwl, int idx)
{
	int	i;

	i = idx;
	do {
		if (winlink_find_by_index(wwl, i) == NULL)
			return (i);
		if (i == INT_MAX)
			i = 0;
		else
			i++;
	} while (i != idx);
	return (-1);
}

u_int
winlink_count(struct winlinks *wwl)
{
	struct winlink	*wl;
	u_int		 n;

	n = 0;
	RB_FOREACH(wl, winlinks, wwl)
		n++;

	return (n);
}

struct winlink *
winlink_add(struct winlinks *wwl, int idx)
{
	struct winlink	*wl;

	if (idx < 0) {
		if ((idx = winlink_next_index(wwl, -idx - 1)) == -1)
			return (NULL);
	} else if (winlink_find_by_index(wwl, idx) != NULL)
		return (NULL);

	wl = xcalloc(1, sizeof *wl);
	wl->idx = idx;
	RB_INSERT(winlinks, wwl, wl);

	return (wl);
}

void
winlink_set_window(struct winlink *wl, struct window *w)
{
	wl->window = w;
	w->references++;
}

void
winlink_remove(struct winlinks *wwl, struct winlink *wl)
{
	struct window	*w = wl->window;

	RB_REMOVE(winlinks, wwl, wl);
	free(wl->status_text);
	free(wl);

	if (w != NULL)
		window_remove_ref(w);
}

struct winlink *
winlink_next(struct winlink *wl)
{
	return (RB_NEXT(winlinks, wwl, wl));
}

struct winlink *
winlink_previous(struct winlink *wl)
{
	return (RB_PREV(winlinks, wwl, wl));
}

struct winlink *
winlink_next_by_number(struct winlink *wl, struct session *s, int n)
{
	for (; n > 0; n--) {
		if ((wl = RB_NEXT(winlinks, wwl, wl)) == NULL)
			wl = RB_MIN(winlinks, &s->windows);
	}

	return (wl);
}

struct winlink *
winlink_previous_by_number(struct winlink *wl, struct session *s, int n)
{
	for (; n > 0; n--) {
		if ((wl = RB_PREV(winlinks, wwl, wl)) == NULL)
			wl = RB_MAX(winlinks, &s->windows);
	}

	return (wl);
}

void
winlink_stack_push(struct winlink_stack *stack, struct winlink *wl)
{
	if (wl == NULL)
		return;

	winlink_stack_remove(stack, wl);
	TAILQ_INSERT_HEAD(stack, wl, sentry);
}

void
winlink_stack_remove(struct winlink_stack *stack, struct winlink *wl)
{
	struct winlink	*wl2;

	if (wl == NULL)
		return;

	TAILQ_FOREACH(wl2, stack, sentry) {
		if (wl2 == wl) {
			TAILQ_REMOVE(stack, wl, sentry);
			return;
		}
	}
}

int
window_index(struct window *s, u_int *i)
{
	for (*i = 0; *i < ARRAY_LENGTH(&windows); (*i)++) {
		if (s == ARRAY_ITEM(&windows, *i))
			return (0);
	}
	return (-1);
}

struct window *
window_find_by_id(u_int id)
{
	struct window	*w;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w->id == id)
			return (w);
	}
	return (NULL);
}

struct window *
window_create1(u_int sx, u_int sy)
{
	struct window	*w;
	u_int		 i;

	w = xcalloc(1, sizeof *w);
	w->id = next_window_id++;
	w->name = NULL;
	w->flags = 0;

	TAILQ_INIT(&w->panes);
	w->active = NULL;

	w->lastlayout = -1;
	w->layout_root = NULL;

	w->sx = sx;
	w->sy = sy;

	options_init(&w->options, &global_w_options);
	if (options_get_number(&w->options, "automatic-rename"))
		queue_window_name(w);

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if (ARRAY_ITEM(&windows, i) == NULL) {
			ARRAY_SET(&windows, i, w);
			break;
		}
	}
	if (i == ARRAY_LENGTH(&windows))
		ARRAY_ADD(&windows, w);
	w->references = 0;

	return (w);
}

struct window *
window_create(const char *name, int argc, char **argv, const char *path,
    const char *shell, int cwd, struct environ *env, struct termios *tio,
    u_int sx, u_int sy, u_int hlimit, char **cause)
{
	struct window		*w;
	struct window_pane	*wp;

	w = window_create1(sx, sy);
	wp = window_add_pane(w, hlimit);
	layout_init(w, wp);

	if (window_pane_spawn(wp, argc, argv, path, shell, cwd, env, tio,
	    cause) != 0) {
		window_destroy(w);
		return (NULL);
	}

	w->active = TAILQ_FIRST(&w->panes);
	if (name != NULL) {
		w->name = xstrdup(name);
		options_set_number(&w->options, "automatic-rename", 0);
	} else
		w->name = default_window_name(w);

	return (w);
}

void
window_destroy(struct window *w)
{
	u_int	i;

	window_unzoom(w);

	if (window_index(w, &i) != 0)
		fatalx("index not found");
	ARRAY_SET(&windows, i, NULL);
	while (!ARRAY_EMPTY(&windows) && ARRAY_LAST(&windows) == NULL)
		ARRAY_TRUNC(&windows, 1);

	if (w->layout_root != NULL)
		layout_free(w);

	if (event_initialized(&w->name_timer))
		evtimer_del(&w->name_timer);

	options_free(&w->options);

	window_destroy_panes(w);

	free(w->name);
	free(w);
}

void
window_remove_ref(struct window *w)
{
	if (w->references == 0)
		fatal("bad reference count");
	w->references--;
	if (w->references == 0)
		window_destroy(w);
}

void
window_set_name(struct window *w, const char *new_name)
{
	free(w->name);
	w->name = xstrdup(new_name);
	notify_window_renamed(w);
}

void
window_resize(struct window *w, u_int sx, u_int sy)
{
	w->sx = sx;
	w->sy = sy;
}

void
window_set_active_pane(struct window *w, struct window_pane *wp)
{
	if (wp == w->active)
		return;
	w->last = w->active;
	w->active = wp;
	while (!window_pane_visible(w->active)) {
		w->active = TAILQ_PREV(w->active, window_panes, entry);
		if (w->active == NULL)
			w->active = TAILQ_LAST(&w->panes, window_panes);
		if (w->active == wp)
			return;
	}
	w->active->active_point = next_active_point++;
}

struct window_pane *
window_get_active_at(struct window *w, u_int x, u_int y)
{
	struct window_pane	*wp;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		if (x < wp->xoff || x > wp->xoff + wp->sx)
			continue;
		if (y < wp->yoff || y > wp->yoff + wp->sy)
			continue;
		return (wp);
	}
	return (NULL);
}

void
window_set_active_at(struct window *w, u_int x, u_int y)
{
	struct window_pane	*wp;

	wp = window_get_active_at(w, x, y);
	if (wp != NULL && wp != w->active)
		window_set_active_pane(w, wp);
}

struct window_pane *
window_find_string(struct window *w, const char *s)
{
	u_int	x, y;

	x = w->sx / 2;
	y = w->sy / 2;

	if (strcasecmp(s, "top") == 0)
		y = 0;
	else if (strcasecmp(s, "bottom") == 0)
		y = w->sy - 1;
	else if (strcasecmp(s, "left") == 0)
		x = 0;
	else if (strcasecmp(s, "right") == 0)
		x = w->sx - 1;
	else if (strcasecmp(s, "top-left") == 0) {
		x = 0;
		y = 0;
	} else if (strcasecmp(s, "top-right") == 0) {
		x = w->sx - 1;
		y = 0;
	} else if (strcasecmp(s, "bottom-left") == 0) {
		x = 0;
		y = w->sy - 1;
	} else if (strcasecmp(s, "bottom-right") == 0) {
		x = w->sx - 1;
		y = w->sy - 1;
	} else
		return (NULL);

	return (window_get_active_at(w, x, y));
}

int
window_zoom(struct window_pane *wp)
{
	struct window		*w = wp->window;
	struct window_pane	*wp1;

	if (w->flags & WINDOW_ZOOMED)
		return (-1);

	if (!window_pane_visible(wp))
		return (-1);

	if (window_count_panes(w) == 1)
		return (-1);

	if (w->active != wp)
		window_set_active_pane(w, wp);

	TAILQ_FOREACH(wp1, &w->panes, entry) {
		wp1->saved_layout_cell = wp1->layout_cell;
		wp1->layout_cell = NULL;
	}

	w->saved_layout_root = w->layout_root;
	layout_init(w, wp);
	w->flags |= WINDOW_ZOOMED;

	return (0);
}

int
window_unzoom(struct window *w)
{
	struct window_pane	*wp;

	if (!(w->flags & WINDOW_ZOOMED))
		return (-1);

	w->flags &= ~WINDOW_ZOOMED;
	layout_free(w);
	w->layout_root = w->saved_layout_root;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		wp->layout_cell = wp->saved_layout_cell;
		wp->saved_layout_cell = NULL;
	}
	layout_fix_panes(w, w->sx, w->sy);

	return (0);
}

struct window_pane *
window_add_pane(struct window *w, u_int hlimit)
{
	struct window_pane	*wp;

	wp = window_pane_create(w, w->sx, w->sy, hlimit);
	if (TAILQ_EMPTY(&w->panes))
		TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	else
		TAILQ_INSERT_AFTER(&w->panes, w->active, wp, entry);
	return (wp);
}

void
window_lost_pane(struct window *w, struct window_pane *wp)
{
	if (wp == w->active) {
		w->active = w->last;
		w->last = NULL;
		if (w->active == NULL) {
			w->active = TAILQ_PREV(wp, window_panes, entry);
			if (w->active == NULL)
				w->active = TAILQ_NEXT(wp, entry);
		}
	} else if (wp == w->last)
		w->last = NULL;
}

void
window_remove_pane(struct window *w, struct window_pane *wp)
{
	window_lost_pane(w, wp);

	TAILQ_REMOVE(&w->panes, wp, entry);
	window_pane_destroy(wp);
}

struct window_pane *
window_pane_at_index(struct window *w, u_int idx)
{
	struct window_pane	*wp;
	u_int			 n;

	n = options_get_number(&w->options, "pane-base-index");
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (n == idx)
			return (wp);
		n++;
	}
	return (NULL);
}

struct window_pane *
window_pane_next_by_number(struct window *w, struct window_pane *wp, u_int n)
{
	for (; n > 0; n--) {
		if ((wp = TAILQ_NEXT(wp, entry)) == NULL)
			wp = TAILQ_FIRST(&w->panes);
	}

	return (wp);
}

struct window_pane *
window_pane_previous_by_number(struct window *w, struct window_pane *wp,
    u_int n)
{
	for (; n > 0; n--) {
		if ((wp = TAILQ_PREV(wp, window_panes, entry)) == NULL)
			wp = TAILQ_LAST(&w->panes, window_panes);
	}

	return (wp);
}

int
window_pane_index(struct window_pane *wp, u_int *i)
{
	struct window_pane	*wq;
	struct window		*w = wp->window;

	*i = options_get_number(&w->options, "pane-base-index");
	TAILQ_FOREACH(wq, &w->panes, entry) {
		if (wp == wq) {
			return (0);
		}
		(*i)++;
	}

	return (-1);
}

u_int
window_count_panes(struct window *w)
{
	struct window_pane	*wp;
	u_int			 n;

	n = 0;
	TAILQ_FOREACH(wp, &w->panes, entry)
		n++;
	return (n);
}

void
window_destroy_panes(struct window *w)
{
	struct window_pane	*wp;

	while (!TAILQ_EMPTY(&w->panes)) {
		wp = TAILQ_FIRST(&w->panes);
		TAILQ_REMOVE(&w->panes, wp, entry);
		window_pane_destroy(wp);
	}
}

/* Return list of printable window flag symbols. No flags is just a space. */
char *
window_printable_flags(struct session *s, struct winlink *wl)
{
	char	flags[BUFSIZ];
	int	pos;

	pos = 0;
	if (wl->flags & WINLINK_ACTIVITY)
		flags[pos++] = '#';
	if (wl->flags & WINLINK_BELL)
		flags[pos++] = '!';
	if (wl->flags & WINLINK_SILENCE)
		flags[pos++] = '~';
	if (wl == s->curw)
		flags[pos++] = '*';
	if (wl == TAILQ_FIRST(&s->lastw))
		flags[pos++] = '-';
	if (wl->window->flags & WINDOW_ZOOMED)
		flags[pos++] = 'Z';
	if (pos == 0)
		flags[pos++] = ' ';
	flags[pos] = '\0';
	return (xstrdup(flags));
}

/* Find pane in global tree by id. */
struct window_pane *
window_pane_find_by_id(u_int id)
{
	struct window_pane	wp;

	wp.id = id;
	return (RB_FIND(window_pane_tree, &all_window_panes, &wp));
}

struct window_pane *
window_pane_create(struct window *w, u_int sx, u_int sy, u_int hlimit)
{
	struct window_pane	*wp;

	wp = xcalloc(1, sizeof *wp);
	wp->window = w;

	wp->id = next_window_pane_id++;
	RB_INSERT(window_pane_tree, &all_window_panes, wp);

	wp->argc = 0;
	wp->argv = NULL;
	wp->shell = NULL;
	wp->cwd = -1;

	wp->fd = -1;
	wp->event = NULL;

	wp->mode = NULL;

	wp->layout_cell = NULL;

	wp->xoff = 0;
	wp->yoff = 0;

	wp->sx = sx;
	wp->sy = sy;

	wp->pipe_fd = -1;
	wp->pipe_off = 0;
	wp->pipe_event = NULL;

	wp->saved_grid = NULL;

	screen_init(&wp->base, sx, sy, hlimit);
	wp->screen = &wp->base;

	input_init(wp);

	return (wp);
}

void
window_pane_destroy(struct window_pane *wp)
{
	window_pane_reset_mode(wp);

	if (event_initialized(&wp->changes_timer))
		evtimer_del(&wp->changes_timer);

	if (wp->fd != -1) {
#ifdef HAVE_UTEMPTER
		utempter_remove_record(wp->fd);
#endif
		bufferevent_free(wp->event);
		close(wp->fd);
	}

	input_free(wp);

	screen_free(&wp->base);
	if (wp->saved_grid != NULL)
		grid_destroy(wp->saved_grid);

	if (wp->pipe_fd != -1) {
		bufferevent_free(wp->pipe_event);
		close(wp->pipe_fd);
	}

	RB_REMOVE(window_pane_tree, &all_window_panes, wp);

	close(wp->cwd);
	free(wp->shell);
	cmd_free_argv(wp->argc, wp->argv);
	free(wp);
}

int
window_pane_spawn(struct window_pane *wp, int argc, char **argv,
    const char *path, const char *shell, int cwd, struct environ *env,
    struct termios *tio, char **cause)
{
	struct winsize	 ws;
	char		*argv0, *cmd, **argvp, paneid[16];
	const char	*ptr, *first;
	struct termios	 tio2;
#ifdef HAVE_UTEMPTER
	char		 s[32];
#endif
	int		 i;

	if (wp->fd != -1) {
		bufferevent_free(wp->event);
		close(wp->fd);
	}
	if (argc > 0) {
		cmd_free_argv(wp->argc, wp->argv);
		wp->argc = argc;
		wp->argv = cmd_copy_argv(argc, argv);
	}
	if (shell != NULL) {
		free(wp->shell);
		wp->shell = xstrdup(shell);
	}
	if (cwd != -1) {
		close(wp->cwd);
		wp->cwd = dup(cwd);
	}

	cmd = cmd_stringify_argv(wp->argc, wp->argv);
	log_debug("spawn: %s -- %s", wp->shell, cmd);
	for (i = 0; i < wp->argc; i++)
		log_debug("spawn: argv[%d] = %s", i, wp->argv[i]);

	memset(&ws, 0, sizeof ws);
	ws.ws_col = screen_size_x(&wp->base);
	ws.ws_row = screen_size_y(&wp->base);

	switch (wp->pid = forkpty(&wp->fd, wp->tty, NULL, &ws)) {
	case -1:
		wp->fd = -1;
		xasprintf(cause, "%s: %s", cmd, strerror(errno));
		free(cmd);
		return (-1);
	case 0:
		if (fchdir(wp->cwd) != 0)
			chdir("/");

		if (tcgetattr(STDIN_FILENO, &tio2) != 0)
			fatal("tcgetattr failed");
		if (tio != NULL)
			memcpy(tio2.c_cc, tio->c_cc, sizeof tio2.c_cc);
		tio2.c_cc[VERASE] = '\177';
#ifdef IUTF8
		if (options_get_number(&wp->window->options, "utf8"))
			tio2.c_iflag |= IUTF8;
#endif
		if (tcsetattr(STDIN_FILENO, TCSANOW, &tio2) != 0)
			fatal("tcgetattr failed");

		closefrom(STDERR_FILENO + 1);

		if (path != NULL)
			environ_set(env, "PATH", path);
		xsnprintf(paneid, sizeof paneid, "%%%u", wp->id);
		environ_set(env, "TMUX_PANE", paneid);
		environ_push(env);

		clear_signals(1);
		log_close();

		setenv("SHELL", wp->shell, 1);
		ptr = strrchr(wp->shell, '/');

		/*
		 * If given one argument, assume it should be passed to sh -c;
		 * with more than one argument, use execvp(). If there is no
		 * arguments, create a login shell.
		 */
		if (wp->argc > 0) {
			if (wp->argc != 1) {
				/* Copy to ensure argv ends in NULL. */
				argvp = cmd_copy_argv(wp->argc, wp->argv);
				execvp(argvp[0], argvp);
				fatal("execvp failed");
			}
			first = wp->argv[0];

			if (ptr != NULL && *(ptr + 1) != '\0')
				xasprintf(&argv0, "%s", ptr + 1);
			else
				xasprintf(&argv0, "%s", wp->shell);
			execl(wp->shell, argv0, "-c", first, (char *)NULL);
			fatal("execl failed");
		}
		if (ptr != NULL && *(ptr + 1) != '\0')
			xasprintf(&argv0, "-%s", ptr + 1);
		else
			xasprintf(&argv0, "-%s", wp->shell);
		execl(wp->shell, argv0, (char *)NULL);
		fatal("execl failed");
	}

#ifdef HAVE_UTEMPTER
	xsnprintf(s, sizeof s, "tmux(%lu).%%%u", (long) getpid(), wp->id);
	utempter_add_record(wp->fd, s);
#endif

	setblocking(wp->fd, 0);

	wp->event = bufferevent_new(wp->fd, window_pane_read_callback, NULL,
	    window_pane_error_callback, wp);
	bufferevent_enable(wp->event, EV_READ|EV_WRITE);

	free(cmd);
	return (0);
}

void
window_pane_timer_start(struct window_pane *wp)
{
	struct timeval	tv;

	tv.tv_sec = 0;
	tv.tv_usec = 1000;

	evtimer_del(&wp->changes_timer);
	evtimer_set(&wp->changes_timer, window_pane_timer_callback, wp);
	evtimer_add(&wp->changes_timer, &tv);
}

void
window_pane_timer_callback(unused int fd, unused short events, void *data)
{
	struct window_pane	*wp = data;
	struct window		*w = wp->window;
	u_int			 interval, trigger;

	interval = options_get_number(&w->options, "c0-change-interval");
	trigger = options_get_number(&w->options, "c0-change-trigger");

	if (wp->changes_redraw++ == interval) {
		wp->flags |= PANE_REDRAW;
		wp->changes_redraw = 0;

	}

	if (trigger == 0 || wp->changes < trigger) {
		wp->flags |= PANE_REDRAW;
		wp->flags &= ~PANE_DROP;
	} else
		window_pane_timer_start(wp);
	wp->changes = 0;
}

void
window_pane_read_callback(unused struct bufferevent *bufev, void *data)
{
	struct window_pane     *wp = data;
	char   		       *new_data;
	size_t			new_size;

	new_size = EVBUFFER_LENGTH(wp->event->input) - wp->pipe_off;
	if (wp->pipe_fd != -1 && new_size > 0) {
		new_data = EVBUFFER_DATA(wp->event->input);
		bufferevent_write(wp->pipe_event, new_data, new_size);
	}

	input_parse(wp);

	wp->pipe_off = EVBUFFER_LENGTH(wp->event->input);

	/*
	 * If we get here, we're not outputting anymore, so set the silence
	 * flag on the window.
	 */
	wp->window->flags |= WINDOW_SILENCE;
	if (gettimeofday(&wp->window->silence_timer, NULL) != 0)
		fatal("gettimeofday failed.");
}

void
window_pane_error_callback(
    unused struct bufferevent *bufev, unused short what, void *data)
{
	struct window_pane *wp = data;

	server_destroy_pane(wp);
}

void
window_pane_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	if (sx == wp->sx && sy == wp->sy)
		return;
	wp->sx = sx;
	wp->sy = sy;

	screen_resize(&wp->base, sx, sy, wp->saved_grid == NULL);
	if (wp->mode != NULL)
		wp->mode->resize(wp, sx, sy);

	wp->flags |= PANE_RESIZE;
}

/*
 * Enter alternative screen mode. A copy of the visible screen is saved and the
 * history is not updated
 */
void
window_pane_alternate_on(struct window_pane *wp, struct grid_cell *gc,
    int cursor)
{
	struct screen	*s = &wp->base;
	u_int		 sx, sy;

	if (wp->saved_grid != NULL)
		return;
	if (!options_get_number(&wp->window->options, "alternate-screen"))
		return;
	sx = screen_size_x(s);
	sy = screen_size_y(s);

	wp->saved_grid = grid_create(sx, sy, 0);
	grid_duplicate_lines(wp->saved_grid, 0, s->grid, screen_hsize(s), sy);
	if (cursor) {
		wp->saved_cx = s->cx;
		wp->saved_cy = s->cy;
	}
	memcpy(&wp->saved_cell, gc, sizeof wp->saved_cell);

	grid_view_clear(s->grid, 0, 0, sx, sy);

	wp->base.grid->flags &= ~GRID_HISTORY;

	wp->flags |= PANE_REDRAW;
}

/* Exit alternate screen mode and restore the copied grid. */
void
window_pane_alternate_off(struct window_pane *wp, struct grid_cell *gc,
    int cursor)
{
	struct screen	*s = &wp->base;
	u_int		 sx, sy;

	if (wp->saved_grid == NULL)
		return;
	if (!options_get_number(&wp->window->options, "alternate-screen"))
		return;
	sx = screen_size_x(s);
	sy = screen_size_y(s);

	/*
	 * If the current size is bigger, temporarily resize to the old size
	 * before copying back.
	 */
	if (sy > wp->saved_grid->sy)
		screen_resize(s, sx, wp->saved_grid->sy, 1);

	/* Restore the grid, cursor position and cell. */
	grid_duplicate_lines(s->grid, screen_hsize(s), wp->saved_grid, 0, sy);
	if (cursor)
		s->cx = wp->saved_cx;
	if (s->cx > screen_size_x(s) - 1)
		s->cx = screen_size_x(s) - 1;
	if (cursor)
		s->cy = wp->saved_cy;
	if (s->cy > screen_size_y(s) - 1)
		s->cy = screen_size_y(s) - 1;
	memcpy(gc, &wp->saved_cell, sizeof *gc);

	/*
	 * Turn history back on (so resize can use it) and then resize back to
	 * the current size.
	 */
	wp->base.grid->flags |= GRID_HISTORY;
	if (sy > wp->saved_grid->sy || sx != wp->saved_grid->sx)
		screen_resize(s, sx, sy, 1);

	grid_destroy(wp->saved_grid);
	wp->saved_grid = NULL;

	wp->flags |= PANE_REDRAW;
}

int
window_pane_set_mode(struct window_pane *wp, const struct window_mode *mode)
{
	struct screen	*s;

	if (wp->mode != NULL)
		return (1);
	wp->mode = mode;

	if ((s = wp->mode->init(wp)) != NULL)
		wp->screen = s;
	wp->flags |= PANE_REDRAW;
	return (0);
}

void
window_pane_reset_mode(struct window_pane *wp)
{
	if (wp->mode == NULL)
		return;

	wp->mode->free(wp);
	wp->mode = NULL;

	wp->screen = &wp->base;
	wp->flags |= PANE_REDRAW;
}

void
window_pane_key(struct window_pane *wp, struct session *sess, int key)
{
	struct window_pane	*wp2;

	if (!window_pane_visible(wp))
		return;

	if (wp->mode != NULL) {
		if (wp->mode->key != NULL)
			wp->mode->key(wp, sess, key);
		return;
	}

	if (wp->fd == -1)
		return;
	input_key(wp, key);
	if (options_get_number(&wp->window->options, "synchronize-panes")) {
		TAILQ_FOREACH(wp2, &wp->window->panes, entry) {
			if (wp2 == wp || wp2->mode != NULL)
				continue;
			if (wp2->fd != -1 && window_pane_visible(wp2))
				input_key(wp2, key);
		}
	}
}

void
window_pane_mouse(
    struct window_pane *wp, struct session *sess, struct mouse_event *m)
{
	if (!window_pane_visible(wp))
		return;

	if (m->x < wp->xoff || m->x >= wp->xoff + wp->sx)
		return;
	if (m->y < wp->yoff || m->y >= wp->yoff + wp->sy)
		return;
	m->x -= wp->xoff;
	m->y -= wp->yoff;

	if (wp->mode != NULL) {
		if (wp->mode->mouse != NULL &&
		    options_get_number(&wp->window->options, "mode-mouse"))
			wp->mode->mouse(wp, sess, m);
	} else if (wp->fd != -1)
		input_mouse(wp, sess, m);
}

int
window_pane_visible(struct window_pane *wp)
{
	struct window	*w = wp->window;

	if (wp->layout_cell == NULL)
		return (0);
	if (wp->xoff >= w->sx || wp->yoff >= w->sy)
		return (0);
	if (wp->xoff + wp->sx > w->sx || wp->yoff + wp->sy > w->sy)
		return (0);
	return (1);
}

char *
window_pane_search(struct window_pane *wp, const char *searchstr,
    u_int *lineno)
{
	struct screen	*s = &wp->base;
	char		*newsearchstr, *line, *msg;
	u_int	 	 i;

	msg = NULL;
	xasprintf(&newsearchstr, "*%s*", searchstr);

	for (i = 0; i < screen_size_y(s); i++) {
		line = grid_view_string_cells(s->grid, 0, i, screen_size_x(s));
		if (fnmatch(newsearchstr, line, 0) == 0) {
			msg = line;
			if (lineno != NULL)
				*lineno = i;
			break;
		}
		free(line);
	}

	free(newsearchstr);
	return (msg);
}

/* Get MRU pane from a list. */
struct window_pane *
window_pane_choose_best(struct window_pane_list *list)
{
	struct window_pane	*next, *best;
	u_int			 i;

	if (ARRAY_LENGTH(list) == 0)
		return (NULL);

	best = ARRAY_FIRST(list);
	for (i = 1; i < ARRAY_LENGTH(list); i++) {
		next = ARRAY_ITEM(list, i);
		if (next->active_point > best->active_point)
			best = next;
	}
	return (best);
}

/*
 * Find the pane directly above another. We build a list of those adjacent to
 * top edge and then choose the best.
 */
struct window_pane *
window_pane_find_up(struct window_pane *wp)
{
	struct window_pane	*next, *best;
	u_int			 edge, left, right, end;
	struct window_pane_list	 list;
	int			 found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);
	ARRAY_INIT(&list);

	edge = wp->yoff;
	if (edge == 0)
		edge = wp->window->sy + 1;

	left = wp->xoff;
	right = wp->xoff + wp->sx;

	TAILQ_FOREACH(next, &wp->window->panes, entry) {
		if (next == wp || !window_pane_visible(next))
			continue;
		if (next->yoff + next->sy + 1 != edge)
			continue;
		end = next->xoff + next->sx - 1;

		found = 0;
		if (next->xoff < left && end > right)
			found = 1;
		else if (next->xoff >= left && next->xoff <= right)
			found = 1;
		else if (end >= left && end <= right)
			found = 1;
		if (found)
			ARRAY_ADD(&list, next);
	}

	best = window_pane_choose_best(&list);
	ARRAY_FREE(&list);
	return (best);
}

/* Find the pane directly below another. */
struct window_pane *
window_pane_find_down(struct window_pane *wp)
{
	struct window_pane	*next, *best;
	u_int			 edge, left, right, end;
	struct window_pane_list	 list;
	int			 found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);
	ARRAY_INIT(&list);

	edge = wp->yoff + wp->sy + 1;
	if (edge >= wp->window->sy)
		edge = 0;

	left = wp->xoff;
	right = wp->xoff + wp->sx;

	TAILQ_FOREACH(next, &wp->window->panes, entry) {
		if (next == wp || !window_pane_visible(next))
			continue;
		if (next->yoff != edge)
			continue;
		end = next->xoff + next->sx - 1;

		found = 0;
		if (next->xoff < left && end > right)
			found = 1;
		else if (next->xoff >= left && next->xoff <= right)
			found = 1;
		else if (end >= left && end <= right)
			found = 1;
		if (found)
			ARRAY_ADD(&list, next);
	}

	best = window_pane_choose_best(&list);
	ARRAY_FREE(&list);
	return (best);
}

/* Find the pane directly to the left of another. */
struct window_pane *
window_pane_find_left(struct window_pane *wp)
{
	struct window_pane	*next, *best;
	u_int			 edge, top, bottom, end;
	struct window_pane_list	 list;
	int			 found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);
	ARRAY_INIT(&list);

	edge = wp->xoff;
	if (edge == 0)
		edge = wp->window->sx + 1;

	top = wp->yoff;
	bottom = wp->yoff + wp->sy;

	TAILQ_FOREACH(next, &wp->window->panes, entry) {
		if (next == wp || !window_pane_visible(next))
			continue;
		if (next->xoff + next->sx + 1 != edge)
			continue;
		end = next->yoff + next->sy - 1;

		found = 0;
		if (next->yoff < top && end > bottom)
			found = 1;
		else if (next->yoff >= top && next->yoff <= bottom)
			found = 1;
		else if (end >= top && end <= bottom)
			found = 1;
		if (found)
			ARRAY_ADD(&list, next);
	}

	best = window_pane_choose_best(&list);
	ARRAY_FREE(&list);
	return (best);
}

/* Find the pane directly to the right of another. */
struct window_pane *
window_pane_find_right(struct window_pane *wp)
{
	struct window_pane	*next, *best;
	u_int			 edge, top, bottom, end;
	struct window_pane_list	 list;
	int			 found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);
	ARRAY_INIT(&list);

	edge = wp->xoff + wp->sx + 1;
	if (edge >= wp->window->sx)
		edge = 0;

	top = wp->yoff;
	bottom = wp->yoff + wp->sy;

	TAILQ_FOREACH(next, &wp->window->panes, entry) {
		if (next == wp || !window_pane_visible(next))
			continue;
		if (next->xoff != edge)
			continue;
		end = next->yoff + next->sy - 1;

		found = 0;
		if (next->yoff < top && end > bottom)
			found = 1;
		else if (next->yoff >= top && next->yoff <= bottom)
			found = 1;
		else if (end >= top && end <= bottom)
			found = 1;
		if (found)
			ARRAY_ADD(&list, next);
	}

	best = window_pane_choose_best(&list);
	ARRAY_FREE(&list);
	return (best);
}

/* Clear alert flags for a winlink */
void
winlink_clear_flags(struct winlink *wl)
{
	struct winlink	*wm;
	struct session	*s;
	struct window	*w;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if ((w = ARRAY_ITEM(&windows, i)) == NULL)
			continue;

		RB_FOREACH(s, sessions, &sessions) {
			if ((wm = session_has(s, w)) == NULL)
				continue;

			if (wm->window != wl->window)
				continue;
			if ((wm->flags & WINLINK_ALERTFLAGS) == 0)
				continue;

			wm->flags &= ~WINLINK_ALERTFLAGS;
			wm->window->flags &= ~WINDOW_ALERTFLAGS;
			server_status_session(s);
		}
	}
}
