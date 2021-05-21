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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <regex.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
static u_int	next_window_pane_id;
static u_int	next_window_id;
static u_int	next_active_point;

struct window_pane_input_data {
	struct cmdq_item	*item;
	u_int			 wp;
};

static struct window_pane *window_pane_create(struct window *, u_int, u_int,
		    u_int);
static void	window_pane_destroy(struct window_pane *);

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
window_create(u_int sx, u_int sy, u_int xpixel, u_int ypixel)
{
	struct window	*w;

	if (xpixel == 0)
		xpixel = DEFAULT_XPIXEL;
	if (ypixel == 0)
		ypixel = DEFAULT_YPIXEL;

	w = xcalloc(1, sizeof *w);
	w->name = xstrdup("");
	w->flags = 0;

	TAILQ_INIT(&w->panes);
	w->active = NULL;

	w->lastlayout = -1;
	w->layout_root = NULL;

	w->sx = sx;
	w->sy = sy;
	w->xpixel = xpixel;
	w->ypixel = ypixel;

	w->options = options_create(global_w_options);

	w->references = 0;
	TAILQ_INIT(&w->winlinks);

	w->id = next_window_id++;
	RB_INSERT(windows, &windows, w);

	window_update_activity(w);

	log_debug("%s: @%u create %ux%u (%ux%u)", __func__, w->id, sx, sy,
	    w->xpixel, w->ypixel);
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

	window_destroy_panes(w);

	if (event_initialized(&w->name_event))
		evtimer_del(&w->name_event);

	if (event_initialized(&w->alerts_timer))
		evtimer_del(&w->alerts_timer);
	if (event_initialized(&w->offset_timer))
		event_del(&w->offset_timer);

	options_free(w->options);

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
window_resize(struct window *w, u_int sx, u_int sy, int xpixel, int ypixel)
{
	if (xpixel == 0)
		xpixel = DEFAULT_XPIXEL;
	if (ypixel == 0)
		ypixel = DEFAULT_YPIXEL;

	log_debug("%s: @%u resize %ux%u (%ux%u)", __func__, w->id, sx, sy,
	    xpixel == -1 ? w->xpixel : (u_int)xpixel,
	    ypixel == -1 ? w->ypixel : (u_int)ypixel);
	w->sx = sx;
	w->sy = sy;
	if (xpixel != -1)
		w->xpixel = xpixel;
	if (ypixel != -1)
		w->ypixel = ypixel;
}

void
window_pane_send_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window	*w = wp->window;
	struct winsize	 ws;

	if (wp->fd == -1)
		return;

	log_debug("%s: %%%u resize to %u,%u", __func__, wp->id, sx, sy);

	memset(&ws, 0, sizeof ws);
	ws.ws_col = sx;
	ws.ws_row = sy;
	ws.ws_xpixel = w->xpixel * ws.ws_col;
	ws.ws_ypixel = w->ypixel * ws.ws_row;
	if (ioctl(wp->fd, TIOCSWINSZ, &ws) == -1)
#ifdef __sun
		/*
		 * Some versions of Solaris apparently can return an error when
		 * resizing; don't know why this happens, can't reproduce on
		 * other platforms and ignoring it doesn't seem to cause any
		 * issues.
		 */
		if (errno != EINVAL && errno != ENXIO)
#endif
		fatal("ioctl failed");
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
window_set_active_pane(struct window *w, struct window_pane *wp, int notify)
{
	log_debug("%s: pane %%%u", __func__, wp->id);

	if (wp == w->active)
		return (0);
	w->last = w->active;

	w->active = wp;
	w->active->active_point = next_active_point++;
	w->active->flags |= PANE_CHANGED;

	tty_update_window_offset(w);

	if (notify)
		notify_window("window-pane-changed", w);
	return (1);
}

void
window_redraw_active_switch(struct window *w, struct window_pane *wp)
{
	struct grid_cell	*gc1, *gc2;
	int			 c1, c2;

	if (wp == w->active)
		return;

	for (;;) {
		/*
		 * If the active and inactive styles or palettes are different,
		 * need to redraw the panes.
		 */
		gc1 = &wp->cached_gc;
		gc2 = &wp->cached_active_gc;
		if (!grid_cells_look_equal(gc1, gc2))
			wp->flags |= PANE_REDRAW;
		else {
			c1 = window_pane_get_palette(wp, gc1->fg);
			c2 = window_pane_get_palette(wp, gc2->fg);
			if (c1 != c2)
				wp->flags |= PANE_REDRAW;
			else {
				c1 = window_pane_get_palette(wp, gc1->bg);
				c2 = window_pane_get_palette(wp, gc2->bg);
				if (c1 != c2)
					wp->flags |= PANE_REDRAW;
			}
		}
		if (wp == w->active)
			break;
		wp = w->active;
	}
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
	u_int	x, y, top = 0, bottom = w->sy - 1;
	int	status;

	x = w->sx / 2;
	y = w->sy / 2;

	status = options_get_number(w->options, "pane-border-status");
	if (status == PANE_STATUS_TOP)
		top++;
	else if (status == PANE_STATUS_BOTTOM)
		bottom--;

	if (strcasecmp(s, "top") == 0)
		y = top;
	else if (strcasecmp(s, "bottom") == 0)
		y = bottom;
	else if (strcasecmp(s, "left") == 0)
		x = 0;
	else if (strcasecmp(s, "right") == 0)
		x = w->sx - 1;
	else if (strcasecmp(s, "top-left") == 0) {
		x = 0;
		y = top;
	} else if (strcasecmp(s, "top-right") == 0) {
		x = w->sx - 1;
		y = top;
	} else if (strcasecmp(s, "bottom-left") == 0) {
		x = 0;
		y = bottom;
	} else if (strcasecmp(s, "bottom-right") == 0) {
		x = w->sx - 1;
		y = bottom;
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

	if (window_count_panes(w) == 1)
		return (-1);

	if (w->active != wp)
		window_set_active_pane(w, wp, 1);

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
	layout_fix_panes(w, NULL);
	notify_window("window-layout-changed", w);

	return (0);
}

int
window_push_zoom(struct window *w, int always, int flag)
{
	log_debug("%s: @%u %d", __func__, w->id,
	    flag && (w->flags & WINDOW_ZOOMED));
	if (flag && (always || (w->flags & WINDOW_ZOOMED)))
		w->flags |= WINDOW_WASZOOMED;
	else
		w->flags &= ~WINDOW_WASZOOMED;
	return (window_unzoom(w) == 0);
}

int
window_pop_zoom(struct window *w)
{
	log_debug("%s: @%u %d", __func__, w->id,
	    !!(w->flags & WINDOW_WASZOOMED));
	if (w->flags & WINDOW_WASZOOMED)
		return (window_zoom(w->active) == 0);
	return (0);
}

struct window_pane *
window_add_pane(struct window *w, struct window_pane *other, u_int hlimit,
    int flags)
{
	struct window_pane	*wp;

	if (other == NULL)
		other = w->active;

	wp = window_pane_create(w, w->sx, w->sy, hlimit);
	if (TAILQ_EMPTY(&w->panes)) {
		log_debug("%s: @%u at start", __func__, w->id);
		TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	} else if (flags & SPAWN_BEFORE) {
		log_debug("%s: @%u before %%%u", __func__, w->id, wp->id);
		if (flags & SPAWN_FULLSIZE)
			TAILQ_INSERT_HEAD(&w->panes, wp, entry);
		else
			TAILQ_INSERT_BEFORE(other, wp, entry);
	} else {
		log_debug("%s: @%u after %%%u", __func__, w->id, wp->id);
		if (flags & SPAWN_FULLSIZE)
			TAILQ_INSERT_TAIL(&w->panes, wp, entry);
		else
			TAILQ_INSERT_AFTER(&w->panes, other, wp, entry);
	}
	return (wp);
}

void
window_lost_pane(struct window *w, struct window_pane *wp)
{
	log_debug("%s: @%u pane %%%u", __func__, w->id, wp->id);

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
window_printable_flags(struct winlink *wl, int escape)
{
	struct session	*s = wl->session;
	static char	 flags[32];
	int		 pos;

	pos = 0;
	if (wl->flags & WINLINK_ACTIVITY) {
		flags[pos++] = '#';
		if (escape)
			flags[pos++] = '#';
	}
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
	wp->options = options_create(w->options);
	wp->flags = PANE_STYLECHANGED;

	wp->id = next_window_pane_id++;
	RB_INSERT(window_pane_tree, &all_window_panes, wp);

	wp->fd = -1;

	wp->fg = 8;
	wp->bg = 8;

	TAILQ_INIT(&wp->modes);

	TAILQ_INIT (&wp->resize_queue);

	wp->sx = sx;
	wp->sy = sy;

	wp->pipe_fd = -1;

	screen_init(&wp->base, sx, sy, hlimit);
	wp->screen = &wp->base;

	screen_init(&wp->status_screen, 1, 1, 0);

	if (gethostname(host, sizeof host) == 0)
		screen_set_title(&wp->base, host);

	return (wp);
}

static void
window_pane_destroy(struct window_pane *wp)
{
	struct window_pane_resize	*r;
	struct window_pane_resize	*r1;

	window_pane_reset_mode_all(wp);
	free(wp->searchstr);

	if (wp->fd != -1) {
#ifdef HAVE_UTEMPTER
		utempter_remove_record(wp->fd);
#endif
		bufferevent_free(wp->event);
		close(wp->fd);
	}
	if (wp->ictx != NULL)
		input_free(wp->ictx);

	screen_free(&wp->status_screen);

	screen_free(&wp->base);

	if (wp->pipe_fd != -1) {
		bufferevent_free(wp->pipe_event);
		close(wp->pipe_fd);
	}

	if (event_initialized(&wp->resize_timer))
		event_del(&wp->resize_timer);
	TAILQ_FOREACH_SAFE(r, &wp->resize_queue, entry, r1) {
		TAILQ_REMOVE(&wp->resize_queue, r, entry);
		free(r);
	}

	RB_REMOVE(window_pane_tree, &all_window_panes, wp);

	options_free(wp->options);
	free((void *)wp->cwd);
	free(wp->shell);
	cmd_free_argv(wp->argc, wp->argv);
	free(wp->palette);
	free(wp);
}

static void
window_pane_read_callback(__unused struct bufferevent *bufev, void *data)
{
	struct window_pane		*wp = data;
	struct evbuffer			*evb = wp->event->input;
	struct window_pane_offset	*wpo = &wp->pipe_offset;
	size_t				 size = EVBUFFER_LENGTH(evb);
	char				*new_data;
	size_t				 new_size;
	struct client			*c;

	if (wp->pipe_fd != -1) {
		new_data = window_pane_get_new_data(wp, wpo, &new_size);
		if (new_size > 0) {
			bufferevent_write(wp->pipe_event, new_data, new_size);
			window_pane_update_used_data(wp, wpo, new_size);
		}
	}

	log_debug("%%%u has %zu bytes", wp->id, size);
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL && (c->flags & CLIENT_CONTROL))
			control_write_output(c, wp);
	}
	input_parse_pane(wp);
	bufferevent_disable(wp->event, EV_READ);
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
window_pane_set_event(struct window_pane *wp)
{
	setblocking(wp->fd, 0);

	wp->event = bufferevent_new(wp->fd, window_pane_read_callback,
	    NULL, window_pane_error_callback, wp);
	wp->ictx = input_init(wp, wp->event);

	bufferevent_enable(wp->event, EV_READ|EV_WRITE);
}

void
window_pane_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_mode_entry	*wme;
	struct window_pane_resize	*r;

	if (sx == wp->sx && sy == wp->sy)
		return;

	r = xmalloc (sizeof *r);
	r->sx = sx;
	r->sy = sy;
	r->osx = wp->sx;
	r->osy = wp->sy;
	TAILQ_INSERT_TAIL (&wp->resize_queue, r, entry);

	wp->sx = sx;
	wp->sy = sy;

	log_debug("%s: %%%u resize %ux%u", __func__, wp->id, sx, sy);
	screen_resize(&wp->base, sx, sy, wp->base.saved_grid == NULL);

	wme = TAILQ_FIRST(&wp->modes);
	if (wme != NULL && wme->mode->resize != NULL)
		wme->mode->resize(wme, sx, sy);
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
window_pane_get_palette(struct window_pane *wp, int c)
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

int
window_pane_set_mode(struct window_pane *wp, struct window_pane *swp,
    const struct window_mode *mode, struct cmd_find_state *fs,
    struct args *args)
{
	struct window_mode_entry	*wme;

	if (!TAILQ_EMPTY(&wp->modes) && TAILQ_FIRST(&wp->modes)->mode == mode)
		return (1);

	TAILQ_FOREACH(wme, &wp->modes, entry) {
		if (wme->mode == mode)
			break;
	}
	if (wme != NULL) {
		TAILQ_REMOVE(&wp->modes, wme, entry);
		TAILQ_INSERT_HEAD(&wp->modes, wme, entry);
	} else {
		wme = xcalloc(1, sizeof *wme);
		wme->wp = wp;
		wme->swp = swp;
		wme->mode = mode;
		wme->prefix = 1;
		TAILQ_INSERT_HEAD(&wp->modes, wme, entry);
		wme->screen = wme->mode->init(wme, fs, args);
	}

	wp->screen = wme->screen;
	wp->flags |= (PANE_REDRAW|PANE_CHANGED);

	server_redraw_window_borders(wp->window);
	server_status_window(wp->window);
	notify_pane("pane-mode-changed", wp);

	return (0);
}

void
window_pane_reset_mode(struct window_pane *wp)
{
	struct window_mode_entry	*wme, *next;

	if (TAILQ_EMPTY(&wp->modes))
		return;

	wme = TAILQ_FIRST(&wp->modes);
	TAILQ_REMOVE(&wp->modes, wme, entry);
	wme->mode->free(wme);
	free(wme);

	next = TAILQ_FIRST(&wp->modes);
	if (next == NULL) {
		log_debug("%s: no next mode", __func__);
		wp->screen = &wp->base;
	} else {
		log_debug("%s: next mode is %s", __func__, next->mode->name);
		wp->screen = next->screen;
		if (next->mode->resize != NULL)
			next->mode->resize(next, wp->sx, wp->sy);
	}
	wp->flags |= (PANE_REDRAW|PANE_CHANGED);

	server_redraw_window_borders(wp->window);
	server_status_window(wp->window);
	notify_pane("pane-mode-changed", wp);
}

void
window_pane_reset_mode_all(struct window_pane *wp)
{
	while (!TAILQ_EMPTY(&wp->modes))
		window_pane_reset_mode(wp);
}

static void
window_pane_copy_key(struct window_pane *wp, key_code key)
{
 	struct window_pane	*loop;

	TAILQ_FOREACH(loop, &wp->window->panes, entry) {
		if (loop != wp &&
		    TAILQ_EMPTY(&loop->modes) &&
		    loop->fd != -1 &&
		    (~loop->flags & PANE_INPUTOFF) &&
		    window_pane_visible(loop) &&
		    options_get_number(loop->options, "synchronize-panes"))
			input_key_pane(loop, key, NULL);
	}
}

int
window_pane_key(struct window_pane *wp, struct client *c, struct session *s,
    struct winlink *wl, key_code key, struct mouse_event *m)
{
	struct window_mode_entry	*wme;

	if (KEYC_IS_MOUSE(key) && m == NULL)
		return (-1);

	wme = TAILQ_FIRST(&wp->modes);
	if (wme != NULL) {
		if (wme->mode->key != NULL && c != NULL) {
			key &= ~KEYC_MASK_FLAGS;
			wme->mode->key(wme, c, s, wl, key, m);
		}
		return (0);
	}

	if (wp->fd == -1 || wp->flags & PANE_INPUTOFF)
		return (0);

	if (input_key_pane(wp, key, m) != 0)
		return (-1);

	if (KEYC_IS_MOUSE(key))
		return (0);
	if (options_get_number(wp->options, "synchronize-panes"))
		window_pane_copy_key(wp, key);
	return (0);
}

int
window_pane_visible(struct window_pane *wp)
{
	if (~wp->window->flags & WINDOW_ZOOMED)
		return (1);
	return (wp == wp->window->active);
}

u_int
window_pane_search(struct window_pane *wp, const char *term, int regex,
    int ignore)
{
	struct screen	*s = &wp->base;
	regex_t		 r;
	char		*new = NULL, *line;
	u_int		 i;
	int		 flags = 0, found;
	size_t		 n;

	if (!regex) {
		if (ignore)
			flags |= FNM_CASEFOLD;
		xasprintf(&new, "*%s*", term);
	} else {
		if (ignore)
			flags |= REG_ICASE;
		if (regcomp(&r, term, flags|REG_EXTENDED) != 0)
			return (0);
	}

	for (i = 0; i < screen_size_y(s); i++) {
		line = grid_view_string_cells(s->grid, 0, i, screen_size_x(s));
		for (n = strlen(line); n > 0; n--) {
			if (!isspace((u_char)line[n - 1]))
				break;
			line[n - 1] = '\0';
		}
		log_debug("%s: %s", __func__, line);
		if (!regex)
			found = (fnmatch(new, line, flags) == 0);
		else
			found = (regexec(&r, line, 0, NULL, 0) == 0);
		free(line);
		if (found)
			break;
	}
	if (!regex)
		free(new);
	else
		regfree(&r);

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
	struct window		*w;
	struct window_pane	*next, *best, **list;
	u_int			 edge, left, right, end, size;
	int			 status, found;

	if (wp == NULL)
		return (NULL);
	w = wp->window;
	status = options_get_number(w->options, "pane-border-status");

	list = NULL;
	size = 0;

	edge = wp->yoff;
	if (status == PANE_STATUS_TOP) {
		if (edge == 1)
			edge = w->sy + 1;
	} else if (status == PANE_STATUS_BOTTOM) {
		if (edge == 0)
			edge = w->sy;
	} else {
		if (edge == 0)
			edge = w->sy + 1;
	}

	left = wp->xoff;
	right = wp->xoff + wp->sx;

	TAILQ_FOREACH(next, &w->panes, entry) {
		if (next == wp)
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
	struct window		*w;
	struct window_pane	*next, *best, **list;
	u_int			 edge, left, right, end, size;
	int			 status, found;

	if (wp == NULL)
		return (NULL);
	w = wp->window;
	status = options_get_number(w->options, "pane-border-status");

	list = NULL;
	size = 0;

	edge = wp->yoff + wp->sy + 1;
	if (status == PANE_STATUS_TOP) {
		if (edge >= w->sy)
			edge = 1;
	} else if (status == PANE_STATUS_BOTTOM) {
		if (edge >= w->sy - 1)
			edge = 0;
	} else {
		if (edge >= w->sy)
			edge = 0;
	}

	left = wp->xoff;
	right = wp->xoff + wp->sx;

	TAILQ_FOREACH(next, &w->panes, entry) {
		if (next == wp)
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
	struct window		*w;
	struct window_pane	*next, *best, **list;
	u_int			 edge, top, bottom, end, size;
	int			 found;

	if (wp == NULL)
		return (NULL);
	w = wp->window;

	list = NULL;
	size = 0;

	edge = wp->xoff;
	if (edge == 0)
		edge = w->sx + 1;

	top = wp->yoff;
	bottom = wp->yoff + wp->sy;

	TAILQ_FOREACH(next, &w->panes, entry) {
		if (next == wp)
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
	struct window		*w;
	struct window_pane	*next, *best, **list;
	u_int			 edge, top, bottom, end, size;
	int			 found;

	if (wp == NULL)
		return (NULL);
	w = wp->window;

	list = NULL;
	size = 0;

	edge = wp->xoff + wp->sx + 1;
	if (edge >= w->sx)
		edge = 0;

	top = wp->yoff;
	bottom = wp->yoff + wp->sy;

	TAILQ_FOREACH(next, &w->panes, entry) {
		if (next == wp)
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
winlink_shuffle_up(struct session *s, struct winlink *wl, int before)
{
	int	 idx, last;

	if (wl == NULL)
		return (-1);
	if (before)
		idx = wl->idx;
	else
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
		RB_REMOVE(winlinks, &s->windows, wl);
		wl->idx++;
		RB_INSERT(winlinks, &s->windows, wl);
	}

	return (idx);
}

static void
window_pane_input_callback(struct client *c, __unused const char *path,
    int error, int closed, struct evbuffer *buffer, void *data)
{
	struct window_pane_input_data	*cdata = data;
	struct window_pane		*wp;
	u_char				*buf = EVBUFFER_DATA(buffer);
	size_t				 len = EVBUFFER_LENGTH(buffer);

	wp = window_pane_find_by_id(cdata->wp);
	if (wp == NULL || closed || error != 0 || c->flags & CLIENT_DEAD) {
		if (wp == NULL)
			c->flags |= CLIENT_EXIT;

		evbuffer_drain(buffer, len);
		cmdq_continue(cdata->item);

		server_client_unref(c);
		free(cdata);
		return;
	}
	input_parse_buffer(wp, buf, len);
	evbuffer_drain(buffer, len);
}

int
window_pane_start_input(struct window_pane *wp, struct cmdq_item *item,
    char **cause)
{
	struct client			*c = cmdq_get_client(item);
	struct window_pane_input_data	*cdata;

	if (~wp->flags & PANE_EMPTY) {
		*cause = xstrdup("pane is not empty");
		return (-1);
	}

	cdata = xmalloc(sizeof *cdata);
	cdata->item = item;
	cdata->wp = wp->id;

	c->references++;
	file_read(c, "-", window_pane_input_callback, cdata);

	return (0);
}

void *
window_pane_get_new_data(struct window_pane *wp,
    struct window_pane_offset *wpo, size_t *size)
{
	size_t	used = wpo->used - wp->base_offset;

	*size = EVBUFFER_LENGTH(wp->event->input) - used;
	return (EVBUFFER_DATA(wp->event->input) + used);
}

void
window_pane_update_used_data(struct window_pane *wp,
    struct window_pane_offset *wpo, size_t size)
{
	size_t	used = wpo->used - wp->base_offset;

	if (size > EVBUFFER_LENGTH(wp->event->input) - used)
		size = EVBUFFER_LENGTH(wp->event->input) - used;
	wpo->used += size;
}
