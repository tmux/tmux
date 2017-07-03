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
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <vis.h>

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
static u_int	next_window_pane_id;
static u_int	next_window_id;
static u_int	next_active_point;

static void	window_destroy(struct window *);

static struct window_pane *window_pane_create(struct window *, u_int, u_int,
		    u_int);
static void	window_pane_destroy(struct window_pane *);

static void	window_pane_read_callback(struct bufferevent *, void *);
static void	window_pane_error_callback(struct bufferevent *, short, void *);

static int	winlink_next_index(struct winlinks *, int);

static struct window_pane *window_pane_choose_best(struct window_pane **,
		    u_int);

RB_GENERATE(windows, window, entry, window_cmp);
RB_GENERATE(winlinks, winlink, entry, winlink_cmp);
RB_GENERATE(window_pane_tree, window_pane, tree_entry, window_pane_cmp);

int
window_cmp(struct window *w1, struct window *w2)
{
	return (w1->id - w2->id);
}

int
winlink_cmp(struct winlink *wl1, struct winlink *wl2)
{
	return (wl1->idx - wl2->idx);
}

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

static int
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
	if (wl->window != NULL) {
		TAILQ_REMOVE(&wl->window->winlinks, wl, wentry);
		window_remove_ref(wl->window, __func__);
	}
	TAILQ_INSERT_TAIL(&w->winlinks, wl, wentry);
	wl->window = w;
	window_add_ref(w, __func__);
}

void
winlink_remove(struct winlinks *wwl, struct winlink *wl)
{
	struct window	*w = wl->window;

	if (w != NULL) {
		TAILQ_REMOVE(&w->winlinks, wl, wentry);
		window_remove_ref(w, __func__);
	}

	RB_REMOVE(winlinks, wwl, wl);
	free(wl->status_text);
	free(wl);
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

struct window *
window_find_by_id_str(const char *s)
{
	const char	*errstr;
	u_int		 id;

	if (*s != '@')
		return (NULL);

	id = strtonum(s + 1, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return (NULL);
	return (window_find_by_id(id));
}

struct window *
window_find_by_id(u_int id)
{
	struct window	w;

	w.id = id;
	return (RB_FIND(windows, &windows, &w));
}

void
window_update_activity(struct window *w)
{
	gettimeofday(&w->activity_time, NULL);
	alerts_queue(w, WINDOW_ACTIVITY);
}

struct window *
window_create(u_int sx, u_int sy)
{
	struct window	*w;

	w = xcalloc(1, sizeof *w);
	w->name = NULL;
	w->flags = WINDOW_STYLECHANGED;

	TAILQ_INIT(&w->panes);
	w->active = NULL;

	w->lastlayout = -1;
	w->layout_root = NULL;

	w->sx = sx;
	w->sy = sy;

	w->options = options_create(global_w_options);

	w->references = 0;
	TAILQ_INIT(&w->winlinks);

	w->id = next_window_id++;
	RB_INSERT(windows, &windows, w);

	window_update_activity(w);

	return (w);
}

struct window *
window_create_spawn(const char *name, int argc, char **argv, const char *path,
    const char *shell, const char *cwd, struct environ *env,
    struct termios *tio, u_int sx, u_int sy, u_int hlimit, char **cause)
{
	struct window		*w;
	struct window_pane	*wp;

	w = window_create(sx, sy);
	wp = window_add_pane(w, NULL, 0, hlimit);
	layout_init(w, wp);

	if (window_pane_spawn(wp, argc, argv, path, shell, cwd,
	    env, tio, cause) != 0) {
		window_destroy(w);
		return (NULL);
	}

	w->active = TAILQ_FIRST(&w->panes);
	if (name != NULL) {
		w->name = xstrdup(name);
		options_set_number(w->options, "automatic-rename", 0);
	} else
		w->name = default_window_name(w);

	notify_window("window-pane-changed", w);

	return (w);
}

static void
window_destroy(struct window *w)
{
	log_debug("window @%u destroyed (%d references)", w->id, w->references);

	RB_REMOVE(windows, &windows, w);

	if (w->layout_root != NULL)
		layout_free_cell(w->layout_root);
	if (w->saved_layout_root != NULL)
		layout_free_cell(w->saved_layout_root);
	free(w->old_layout);

	if (event_initialized(&w->name_event))
		evtimer_del(&w->name_event);

	if (event_initialized(&w->alerts_timer))
		evtimer_del(&w->alerts_timer);

	options_free(w->options);

	window_destroy_panes(w);

	free(w->name);
	free(w);
}

int
window_pane_destroy_ready(struct window_pane *wp)
{
	int	n;

	if (wp->pipe_fd != -1) {
		if (EVBUFFER_LENGTH(wp->pipe_event->output) != 0)
			return (0);
		if (ioctl(wp->fd, FIONREAD, &n) != -1 && n > 0)
			return (0);
	}

	if (~wp->flags & PANE_EXITED)
		return (0);
	return (1);
}

void
window_add_ref(struct window *w, const char *from)
{
	w->references++;
	log_debug("%s: @%u %s, now %d", __func__, w->id, from, w->references);
}

void
window_remove_ref(struct window *w, const char *from)
{
	w->references--;
	log_debug("%s: @%u %s, now %d", __func__, w->id, from, w->references);

	if (w->references == 0)
		window_destroy(w);
}

void
window_set_name(struct window *w, const char *new_name)
{
	free(w->name);
	utf8_stravis(&w->name, new_name, VIS_OCTAL|VIS_CSTYLE|VIS_TAB|VIS_NL);
	notify_window("window-renamed", w);
}

void
window_resize(struct window *w, u_int sx, u_int sy)
{
	w->sx = sx;
	w->sy = sy;
}

int
window_has_pane(struct window *w, struct window_pane *wp)
{
	struct window_pane	*wp1;

	TAILQ_FOREACH(wp1, &w->panes, entry) {
		if (wp1 == wp)
			return (1);
	}
	return (0);
}

int
window_set_active_pane(struct window *w, struct window_pane *wp)
{
	log_debug("%s: pane %%%u (was %%%u)", __func__, wp->id, w->active->id);
	if (wp == w->active)
		return (0);
	w->last = w->active;
	w->active = wp;
	while (!window_pane_visible(w->active)) {
		w->active = TAILQ_PREV(w->active, window_panes, entry);
		if (w->active == NULL)
			w->active = TAILQ_LAST(&w->panes, window_panes);
		if (w->active == wp) {
			notify_window("window-pane-changed", w);
			return (1);
		}
	}
	w->active->active_point = next_active_point++;
	w->active->flags |= PANE_CHANGED;
	notify_window("window-pane-changed", w);
	return (1);
}

void
window_redraw_active_switch(struct window *w, struct window_pane *wp)
{
	const struct grid_cell	*gc;

	if (wp == w->active)
		return;

	/*
	 * If window-style and window-active-style are the same, we don't need
	 * to redraw panes when switching active panes.
	 */
	gc = options_get_style(w->options, "window-active-style");
	if (style_equal(gc, options_get_style(w->options, "window-style")))
		return;

	/*
	 * If the now active or inactive pane do not have a custom style or if
	 * the palette is different, they need to be redrawn.
	 */
	if (window_pane_get_palette(w->active, w->active->colgc.fg) != -1 ||
	    window_pane_get_palette(w->active, w->active->colgc.bg) != -1 ||
	    style_equal(&grid_default_cell, &w->active->colgc))
		w->active->flags |= PANE_REDRAW;
	if (window_pane_get_palette(wp, wp->colgc.fg) != -1 ||
	    window_pane_get_palette(wp, wp->colgc.bg) != -1 ||
	    style_equal(&grid_default_cell, &wp->colgc))
		wp->flags |= PANE_REDRAW;
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
	notify_window("window-layout-changed", w);

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
	w->saved_layout_root = NULL;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		wp->layout_cell = wp->saved_layout_cell;
		wp->saved_layout_cell = NULL;
	}
	layout_fix_panes(w, w->sx, w->sy);
	notify_window("window-layout-changed", w);

	return (0);
}

struct window_pane *
window_add_pane(struct window *w, struct window_pane *other, int before,
    u_int hlimit)
{
	struct window_pane	*wp;

	if (other == NULL)
		other = w->active;

	wp = window_pane_create(w, w->sx, w->sy, hlimit);
	if (TAILQ_EMPTY(&w->panes)) {
		log_debug("%s: @%u at start", __func__, w->id);
		TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	} else if (before) {
		log_debug("%s: @%u before %%%u", __func__, w->id, wp->id);
		TAILQ_INSERT_BEFORE(other, wp, entry);
	} else {
		log_debug("%s: @%u after %%%u", __func__, w->id, wp->id);
		TAILQ_INSERT_AFTER(&w->panes, other, wp, entry);
	}
	return (wp);
}

void
window_lost_pane(struct window *w, struct window_pane *wp)
{
	if (wp == marked_pane.wp)
		server_clear_marked();

	if (wp == w->active) {
		w->active = w->last;
		w->last = NULL;
		if (w->active == NULL) {
			w->active = TAILQ_PREV(wp, window_panes, entry);
			if (w->active == NULL)
				w->active = TAILQ_NEXT(wp, entry);
		}
		if (w->active != NULL) {
			w->active->flags |= PANE_CHANGED;
			notify_window("window-pane-changed", w);
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

	n = options_get_number(w->options, "pane-base-index");
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

	*i = options_get_number(w->options, "pane-base-index");
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

const char *
window_printable_flags(struct winlink *wl)
{
	struct session	*s = wl->session;
	static char	 flags[32];
	int		 pos;

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
	if (server_check_marked() && wl == marked_pane.wl)
		flags[pos++] = 'M';
	if (wl->window->flags & WINDOW_ZOOMED)
		flags[pos++] = 'Z';
	flags[pos] = '\0';
	return (flags);
}

struct window_pane *
window_pane_find_by_id_str(const char *s)
{
	const char	*errstr;
	u_int		 id;

	if (*s != '%')
		return (NULL);

	id = strtonum(s + 1, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return (NULL);
	return (window_pane_find_by_id(id));
}

struct window_pane *
window_pane_find_by_id(u_int id)
{
	struct window_pane	wp;

	wp.id = id;
	return (RB_FIND(window_pane_tree, &all_window_panes, &wp));
}

static struct window_pane *
window_pane_create(struct window *w, u_int sx, u_int sy, u_int hlimit)
{
	struct window_pane	*wp;
	char			 host[HOST_NAME_MAX + 1];

	wp = xcalloc(1, sizeof *wp);
	wp->window = w;

	wp->id = next_window_pane_id++;
	RB_INSERT(window_pane_tree, &all_window_panes, wp);

	wp->argc = 0;
	wp->argv = NULL;
	wp->shell = NULL;
	wp->cwd = NULL;

	wp->fd = -1;
	wp->event = NULL;

	wp->mode = NULL;
	wp->modeprefix = 1;

	wp->layout_cell = NULL;

	wp->xoff = 0;
	wp->yoff = 0;

	wp->sx = wp->osx = sx;
	wp->sy = wp->osx = sy;

	wp->pipe_fd = -1;
	wp->pipe_off = 0;
	wp->pipe_event = NULL;

	wp->saved_grid = NULL;

	memcpy(&wp->colgc, &grid_default_cell, sizeof wp->colgc);

	screen_init(&wp->base, sx, sy, hlimit);
	wp->screen = &wp->base;

	screen_init(&wp->status_screen, 1, 1, 0);

	if (gethostname(host, sizeof host) == 0)
		screen_set_title(&wp->base, host);

	input_init(wp);

	return (wp);
}

static void
window_pane_destroy(struct window_pane *wp)
{
	window_pane_reset_mode(wp);
	free(wp->searchstr);

	if (wp->fd != -1) {
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

	if (event_initialized(&wp->resize_timer))
		event_del(&wp->resize_timer);

	RB_REMOVE(window_pane_tree, &all_window_panes, wp);

	free((void *)wp->cwd);
	free(wp->shell);
	cmd_free_argv(wp->argc, wp->argv);
	free(wp->palette);
	free(wp);
}

int
window_pane_spawn(struct window_pane *wp, int argc, char **argv,
    const char *path, const char *shell, const char *cwd, struct environ *env,
    struct termios *tio, char **cause)
{
	struct winsize	 ws;
	char		*argv0, *cmd, **argvp;
	const char	*ptr, *first, *home;
	struct termios	 tio2;
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
	if (cwd != NULL) {
		free((void *)wp->cwd);
		wp->cwd = xstrdup(cwd);
	}

	cmd = cmd_stringify_argv(wp->argc, wp->argv);
	log_debug("spawn: %s -- %s", wp->shell, cmd);
	for (i = 0; i < wp->argc; i++)
		log_debug("spawn: argv[%d] = %s", i, wp->argv[i]);
	environ_log(env, "spawn: ");

	memset(&ws, 0, sizeof ws);
	ws.ws_col = screen_size_x(&wp->base);
	ws.ws_row = screen_size_y(&wp->base);

	wp->pid = fdforkpty(ptm_fd, &wp->fd, wp->tty, NULL, &ws);
	switch (wp->pid) {
	case -1:
		wp->fd = -1;
		xasprintf(cause, "%s: %s", cmd, strerror(errno));
		free(cmd);
		return (-1);
	case 0:
		if (chdir(wp->cwd) != 0) {
			if ((home = find_home()) == NULL || chdir(home) != 0)
				chdir("/");
		}

		if (tcgetattr(STDIN_FILENO, &tio2) != 0)
			fatal("tcgetattr failed");
		if (tio != NULL)
			memcpy(tio2.c_cc, tio->c_cc, sizeof tio2.c_cc);
		tio2.c_cc[VERASE] = '\177';
		if (tcsetattr(STDIN_FILENO, TCSANOW, &tio2) != 0)
			fatal("tcgetattr failed");

		closefrom(STDERR_FILENO + 1);

		if (path != NULL)
			environ_set(env, "PATH", "%s", path);
		environ_set(env, "TMUX_PANE", "%%%u", wp->id);
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

	setblocking(wp->fd, 0);

	wp->event = bufferevent_new(wp->fd, window_pane_read_callback, NULL,
	    window_pane_error_callback, wp);

	bufferevent_setwatermark(wp->event, EV_READ, 0, READ_SIZE);
	bufferevent_enable(wp->event, EV_READ|EV_WRITE);

	free(cmd);
	return (0);
}

static void
window_pane_read_callback(__unused struct bufferevent *bufev, void *data)
{
	struct window_pane	*wp = data;
	struct evbuffer		*evb = wp->event->input;
	size_t			 size = EVBUFFER_LENGTH(evb);
	char			*new_data;
	size_t			 new_size;

	new_size = size - wp->pipe_off;
	if (wp->pipe_fd != -1 && new_size > 0) {
		new_data = EVBUFFER_DATA(evb) + wp->pipe_off;
		bufferevent_write(wp->pipe_event, new_data, new_size);
	}

	log_debug("%%%u has %zu bytes", wp->id, size);
	input_parse(wp);

	wp->pipe_off = EVBUFFER_LENGTH(evb);
}

static void
window_pane_error_callback(__unused struct bufferevent *bufev,
    __unused short what, void *data)
{
	struct window_pane *wp = data;

	log_debug("%%%u error", wp->id);
	wp->flags |= PANE_EXITED;

	if (window_pane_destroy_ready(wp))
		server_destroy_pane(wp, 1);
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
	if (!options_get_number(wp->window->options, "alternate-screen"))
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

	grid_view_clear(s->grid, 0, 0, sx, sy, 8);

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
	if (!options_get_number(wp->window->options, "alternate-screen"))
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

void
window_pane_set_palette(struct window_pane *wp, u_int n, int colour)
{
	if (n > 0xff)
		return;

	if (wp->palette == NULL)
		wp->palette = xcalloc(0x100, sizeof *wp->palette);

	wp->palette[n] = colour;
	wp->flags |= PANE_REDRAW;
}

void
window_pane_unset_palette(struct window_pane *wp, u_int n)
{
	if (n > 0xff || wp->palette == NULL)
		return;

	wp->palette[n] = 0;
	wp->flags |= PANE_REDRAW;
}

void
window_pane_reset_palette(struct window_pane *wp)
{
	if (wp->palette == NULL)
		return;

	free(wp->palette);
	wp->palette = NULL;
	wp->flags |= PANE_REDRAW;
}

int
window_pane_get_palette(const struct window_pane *wp, int c)
{
	int	new;

	if (wp == NULL || wp->palette == NULL)
		return (-1);

	new = -1;
	if (c < 8)
		new = wp->palette[c];
	else if (c >= 90 && c <= 97)
		new = wp->palette[8 + c - 90];
	else if (c & COLOUR_FLAG_256)
		new = wp->palette[c & ~COLOUR_FLAG_256];
	if (new == 0)
		return (-1);
	return (new);
}

static void
window_pane_mode_timer(__unused int fd, __unused short events, void *arg)
{
	struct window_pane	*wp = arg;
	struct timeval		 tv = { .tv_sec = 10 };
	int			 n = 0;

	evtimer_del(&wp->modetimer);
	evtimer_add(&wp->modetimer, &tv);

	log_debug("%%%u in mode: last=%ld", wp->id, (long)wp->modelast);

	if (wp->modelast < time(NULL) - WINDOW_MODE_TIMEOUT) {
		if (ioctl(wp->fd, FIONREAD, &n) == -1 || n > 0)
			window_pane_reset_mode(wp);
	}
}

int
window_pane_set_mode(struct window_pane *wp, const struct window_mode *mode,
    struct cmd_find_state *fs, struct args *args)
{
	struct screen	*s;
	struct timeval	 tv = { .tv_sec = 10 };

	if (wp->mode != NULL)
		return (1);
	wp->mode = mode;

	wp->modelast = time(NULL);
	evtimer_set(&wp->modetimer, window_pane_mode_timer, wp);
	evtimer_add(&wp->modetimer, &tv);

	if ((s = wp->mode->init(wp, fs, args)) != NULL)
		wp->screen = s;
	wp->flags |= (PANE_REDRAW|PANE_CHANGED);

	server_status_window(wp->window);
	notify_pane("pane-mode-changed", wp);
	return (0);
}

void
window_pane_reset_mode(struct window_pane *wp)
{
	if (wp->mode == NULL)
		return;

	evtimer_del(&wp->modetimer);

	wp->mode->free(wp);
	wp->mode = NULL;
	wp->modeprefix = 1;

	wp->screen = &wp->base;
	wp->flags |= (PANE_REDRAW|PANE_CHANGED);

	server_status_window(wp->window);
	notify_pane("pane-mode-changed", wp);
}

void
window_pane_key(struct window_pane *wp, struct client *c, struct session *s,
    key_code key, struct mouse_event *m)
{
	struct window_pane	*wp2;

	if (KEYC_IS_MOUSE(key) && m == NULL)
		return;

	if (wp->mode != NULL) {
		wp->modelast = time(NULL);
		if (wp->mode->key != NULL)
			wp->mode->key(wp, c, s, (key & ~KEYC_XTERM), m);
		return;
	}

	if (wp->fd == -1 || wp->flags & PANE_INPUTOFF)
		return;

	input_key(wp, key, m);

	if (KEYC_IS_MOUSE(key))
		return;
	if (options_get_number(wp->window->options, "synchronize-panes")) {
		TAILQ_FOREACH(wp2, &wp->window->panes, entry) {
			if (wp2 == wp || wp2->mode != NULL)
				continue;
			if (wp2->fd == -1 || wp2->flags & PANE_INPUTOFF)
				continue;
			if (window_pane_visible(wp2))
				input_key(wp2, key, NULL);
		}
	}
}

int
window_pane_outside(struct window_pane *wp)
{
	struct window	*w = wp->window;

	if (wp->xoff >= w->sx || wp->yoff >= w->sy)
		return (1);
	if (wp->xoff + wp->sx > w->sx || wp->yoff + wp->sy > w->sy)
		return (1);
	return (0);
}

int
window_pane_visible(struct window_pane *wp)
{
	if (wp->layout_cell == NULL)
		return (0);
	return (!window_pane_outside(wp));
}

u_int
window_pane_search(struct window_pane *wp, const char *searchstr)
{
	struct screen	*s = &wp->base;
	char		*newsearchstr, *line;
	u_int		 i;

	xasprintf(&newsearchstr, "*%s*", searchstr);

	for (i = 0; i < screen_size_y(s); i++) {
		line = grid_view_string_cells(s->grid, 0, i, screen_size_x(s));
		if (fnmatch(newsearchstr, line, 0) == 0) {
			free(line);
			break;
		}
		free(line);
	}

	free(newsearchstr);
	if (i == screen_size_y(s))
		return (0);
	return (i + 1);
}

/* Get MRU pane from a list. */
static struct window_pane *
window_pane_choose_best(struct window_pane **list, u_int size)
{
	struct window_pane	*next, *best;
	u_int			 i;

	if (size == 0)
		return (NULL);

	best = list[0];
	for (i = 1; i < size; i++) {
		next = list[i];
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
	struct window_pane	*next, *best, **list;
	u_int			 edge, left, right, end, size;
	int			 status, found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);
	status = options_get_number(wp->window->options, "pane-border-status");

	list = NULL;
	size = 0;

	edge = wp->yoff;
	if (edge == (status == 1 ? 1 : 0))
		edge = wp->window->sy + 1 - (status == 2 ? 1 : 0);

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
		if (!found)
			continue;
		list = xreallocarray(list, size + 1, sizeof *list);
		list[size++] = next;
	}

	best = window_pane_choose_best(list, size);
	free(list);
	return (best);
}

/* Find the pane directly below another. */
struct window_pane *
window_pane_find_down(struct window_pane *wp)
{
	struct window_pane	*next, *best, **list;
	u_int			 edge, left, right, end, size;
	int			 status, found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);
	status = options_get_number(wp->window->options, "pane-border-status");

	list = NULL;
	size = 0;

	edge = wp->yoff + wp->sy + 1;
	if (edge >= wp->window->sy - (status == 2 ? 1 : 0))
		edge = (status == 1 ? 1 : 0);

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
		if (!found)
			continue;
		list = xreallocarray(list, size + 1, sizeof *list);
		list[size++] = next;
	}

	best = window_pane_choose_best(list, size);
	free(list);
	return (best);
}

/* Find the pane directly to the left of another. */
struct window_pane *
window_pane_find_left(struct window_pane *wp)
{
	struct window_pane	*next, *best, **list;
	u_int			 edge, top, bottom, end, size;
	int			 found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);

	list = NULL;
	size = 0;

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
		if (!found)
			continue;
		list = xreallocarray(list, size + 1, sizeof *list);
		list[size++] = next;
	}

	best = window_pane_choose_best(list, size);
	free(list);
	return (best);
}

/* Find the pane directly to the right of another. */
struct window_pane *
window_pane_find_right(struct window_pane *wp)
{
	struct window_pane	*next, *best, **list;
	u_int			 edge, top, bottom, end, size;
	int			 found;

	if (wp == NULL || !window_pane_visible(wp))
		return (NULL);

	list = NULL;
	size = 0;

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
		if (!found)
			continue;
		list = xreallocarray(list, size + 1, sizeof *list);
		list[size++] = next;
	}

	best = window_pane_choose_best(list, size);
	free(list);
	return (best);
}

/* Clear alert flags for a winlink */
void
winlink_clear_flags(struct winlink *wl)
{
	struct winlink	*loop;

	wl->window->flags &= ~WINDOW_ALERTFLAGS;
	TAILQ_FOREACH(loop, &wl->window->winlinks, wentry) {
		if ((loop->flags & WINLINK_ALERTFLAGS) != 0) {
			loop->flags &= ~WINLINK_ALERTFLAGS;
			server_status_session(loop->session);
		}
	}
}

/* Shuffle window indexes up. */
int
winlink_shuffle_up(struct session *s, struct winlink *wl)
{
	int	 idx, last;

	idx = wl->idx + 1;

	/* Find the next free index. */
	for (last = idx; last < INT_MAX; last++) {
		if (winlink_find_by_index(&s->windows, last) == NULL)
			break;
	}
	if (last == INT_MAX)
		return (-1);

	/* Move everything from last - 1 to idx up a bit. */
	for (; last > idx; last--) {
		wl = winlink_find_by_index(&s->windows, last - 1);
		server_link_window(s, wl, s, last, 0, 0, NULL);
		server_unlink_window(s, wl);
	}

	return (idx);
}
