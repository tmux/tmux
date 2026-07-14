/* $OpenBSD: window.c,v 1.362 2026/07/14 17:17:18 nicm Exp $ */

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
#include <sys/wait.h>

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

struct window_pane_input_data {
	struct cmdq_item	*item;
	u_int			 wp;
	struct client_file	*file;
};

static struct window_pane *window_pane_create(struct window *, u_int, u_int,
		    u_int);
static void	window_pane_destroy(struct window_pane *);
static void	window_pane_free(struct window_pane *);
static void	window_pane_scrollbar_timer(int, short, void *);
static void	window_pane_full_size_offset(struct window_pane *, int *, int *,
		    u_int *, u_int *);

RB_GENERATE(windows, window, entry, window_cmp);
RB_GENERATE(winlinks, winlink, entry, winlink_cmp);
RB_GENERATE(window_pane_tree, window_pane, tree_entry, window_pane_cmp);

struct window_pane_prompt {
	u_int			 wp_id;
	struct client		*c;
	status_prompt_input_cb	 inputcb;
	prompt_free_cb		 freecb;
	void			*data;
	enum prompt_type	 type;
};

int
window_cmp(struct window *w1, struct window *w2)
{
	return (w1->id - w2->id);
}

static void
window_fire_renamed(struct window *w, const char *old_name)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_window(&fs, w, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_window(ep, "window", w);
	event_payload_set_string(ep, "old_name", "%s", old_name);
	event_payload_set_string(ep, "new_name", "%s", w->name);
	events_fire("window-renamed", ep);
}

static void
window_fire_pane_changed(struct window *w, struct window_pane *wp,
    struct window_pane *lastwp)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_pane(&fs, wp, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_window(ep, "window", w);
	event_payload_set_pane(ep, "pane", wp);
	event_payload_set_pane(ep, "new_pane", wp);
	if (lastwp != NULL)
		event_payload_set_pane(ep, "old_pane", lastwp);
	events_fire("window-pane-changed", ep);
}

void
window_fire_pane_moved(struct window_pane *wp, struct window *old_w,
    int old_idx, struct window *new_w, int new_idx)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_pane(&fs, wp, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_pane(ep, "pane", wp);
	event_payload_set_window(ep, "window", new_w);
	event_payload_set_window(ep, "old_window", old_w);
	event_payload_set_window(ep, "new_window", new_w);
	if (old_idx != -1)
		event_payload_set_int(ep, "old_window_index", old_idx);
	if (new_idx != -1) {
		event_payload_set_int(ep, "window_index", new_idx);
		event_payload_set_int(ep, "new_window_index", new_idx);
	}
	events_fire("pane-moved", ep);
}

static void
window_fire_pane_mode_changed(const char *name, struct window_pane *wp,
    const char *previous, const char *current, int entered)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_pane(&fs, wp, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_pane(ep, "pane", wp);
	event_payload_set_window(ep, "window", wp->window);

	if (current != NULL)
		event_payload_set_string(ep, "current_mode", "%s", current);
	if (previous != NULL)
		event_payload_set_string(ep, "previous_mode", "%s", previous);
	event_payload_set_int(ep, "mode_entered", entered);

	events_fire(name, ep);
}

static void
window_fire_pane_prompt(const char *name, struct window_pane *wp,
    enum prompt_type type)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;
	const char		*type_string = prompt_type_string(type);

	ep = event_payload_create();
	cmd_find_from_pane(&fs, wp, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_pane(ep, "pane", wp);
	event_payload_set_window(ep, "window", wp->window);
	event_payload_set_string(ep, "prompt_type", "%s", type_string);
	events_fire(name, ep);
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
	wl->flags |= WINLINK_VISITED;
}

void
winlink_stack_remove(struct winlink_stack *stack, struct winlink *wl)
{
	if (wl != NULL && (wl->flags & WINLINK_VISITED)) {
		TAILQ_REMOVE(stack, wl, sentry);
		wl->flags &= ~WINLINK_VISITED;
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
	TAILQ_INIT(&w->z_index);
	TAILQ_INIT(&w->last_panes);
	w->active = NULL;

	w->lastlayout = -1;
	w->layout_root = NULL;

	w->sx = sx;
	w->sy = sy;
	w->manual_sx = sx;
	w->manual_sy = sy;
	w->xpixel = xpixel;
	w->ypixel = ypixel;

	w->options = options_create(global_w_options);
	w->sb = options_get_number(w->options, "pane-scrollbars");
	w->sb_pos = options_get_number(w->options, "pane-scrollbars-position");

	w->references = 0;
	TAILQ_INIT(&w->winlinks);

	w->id = next_window_id++;
	RB_INSERT(windows, &windows, w);

	window_set_fill_character(w);

	if (gettimeofday(&w->creation_time, NULL) != 0)
		fatal("gettimeofday failed");
	window_update_activity(w);

	log_debug("%s: @%u create %ux%u (%ux%u)", __func__, w->id, sx, sy,
	    w->xpixel, w->ypixel);
	return (w);
}

static void
window_destroy(struct window *w)
{
	log_debug("window @%u destroyed (%d references)", w->id, w->references);

	window_unzoom(w, 0);
	RB_REMOVE(windows, &windows, w);

	layout_free_cell(w->layout_root, 0);
	layout_free_cell(w->saved_layout_root, 0);
	free(w->old_layout);

	window_destroy_panes(w);

	if (event_initialized(&w->name_event))
		evtimer_del(&w->name_event);

	if (event_initialized(&w->alerts_timer))
		evtimer_del(&w->alerts_timer);
	if (event_initialized(&w->offset_timer))
		event_del(&w->offset_timer);

	options_free(w->options);
	free(w->fill_character);

	free(w->name);
	free(w);
}

int
window_pane_destroy_ready(struct window_pane *wp)
{
	int	n;

	if (wp->pipe_fd != -1 && EVBUFFER_LENGTH(wp->pipe_event->output) != 0)
		return (0);
	if (ioctl(wp->fd, FIONREAD, &n) != -1 && n > 0)
		return (0);

	if (~wp->flags & PANE_EXITED)
		return (0);

	/*
	 * If a command queue item is blocked on this pane, wait for the
	 * child's exit status before destroying it.
	 */
	if (wp->wait_item != NULL && (~wp->flags & PANE_STATUSREADY))
		return (0);
	if (wp->editor != NULL && (~wp->flags & PANE_STATUSREADY))
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
	if (w->references == 1)
		events_fire_window("window-closed", w);

	w->references--;
	log_debug("%s: @%u %s, now %d", __func__, w->id, from, w->references);

	if (w->references == 0)
		window_destroy(w);
}

void
window_pane_add_ref(struct window_pane *wp, const char *from)
{
	wp->references++;
	log_debug("%s: %%%u %s, now %d", __func__, wp->id, from,
	    wp->references);
}

void
window_pane_remove_ref(struct window_pane *wp, const char *from)
{
	wp->references--;
	log_debug("%s: %%%u %s, now %d", __func__, wp->id, from,
	    wp->references);

	if (wp->references == 0)
		window_pane_free(wp);
}

void
window_set_name(struct window *w, const char *new_name, int untrusted)
{
	char	*last, *name;

	name = clean_name(new_name, untrusted);
	if (name != NULL) {
		last = xstrdup(w->name);
		free(w->name);
		w->name = name;
		window_fire_renamed(w, last);
		free(last);
	}
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
		fatal("ioctl failed");
}

int
window_has_floating_panes(struct window *w)
{
	struct window_pane	*wp;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (window_pane_is_floating(wp))
			return (1);
	}
	return (0);
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

void
window_update_focus(struct window *w)
{
	if (w != NULL) {
		log_debug("%s: @%u", __func__, w->id);
		window_pane_update_focus(w->active);
	}
}

void
window_pane_update_focus(struct window_pane *wp)
{
	struct client	*c;
	int		 focused = 0;

	if (wp != NULL && (~wp->flags & PANE_EXITED)) {
		if (wp != wp->window->active)
			focused = 0;
		else {
			TAILQ_FOREACH(c, &clients, entry) {
				if (c->session != NULL &&
				    c->session->attached != 0 &&
				    (c->flags & CLIENT_FOCUSED) &&
				    c->session->curw->window == wp->window &&
				    c->overlay_draw == NULL) {
					focused = 1;
					break;
				}
			}
		}
		if (!focused && (wp->flags & PANE_FOCUSED)) {
			log_debug("%s: %%%u focus out", __func__, wp->id);
			if (wp->base.mode & MODE_FOCUSON)
				bufferevent_write(wp->event, "\033[O", 3);
			events_fire_pane("pane-focus-out", wp);
			wp->flags &= ~PANE_FOCUSED;
		} else if (focused && (~wp->flags & PANE_FOCUSED)) {
			log_debug("%s: %%%u focus in", __func__, wp->id);
			if (wp->base.mode & MODE_FOCUSON)
				bufferevent_write(wp->event, "\033[I", 3);
			events_fire_pane("pane-focus-in", wp);
			wp->flags |= PANE_FOCUSED;
		} else
			log_debug("%s: %%%u focus unchanged", __func__, wp->id);
	}
}

int
window_set_active_pane(struct window *w, struct window_pane *wp, int notify)
{
	struct window_pane *lastwp;

	log_debug("%s: pane %%%u", __func__, wp->id);

	if (wp == w->active)
		return (0);
	if (w->flags & WINDOW_ZOOMED)
		window_unzoom(w, 1);
	lastwp = w->active;

	window_pane_stack_remove(&w->last_panes, wp);
	window_pane_stack_push(&w->last_panes, lastwp);

	w->active = wp;
	w->active->active_point = next_active_point++;
	w->active->flags |= PANE_CHANGED;

	if (options_get_number(global_options, "focus-events")) {
		window_pane_update_focus(lastwp);
		window_pane_update_focus(w->active);
	}

	tty_update_window_offset(w);
	server_redraw_window(w);

	if (notify)
		window_fire_pane_changed(w, w->active, lastwp);
	return (1);
}

static int
window_pane_get_palette(struct window_pane *wp, int c)
{
	if (wp == NULL)
		return (-1);
	return (colour_palette_get(&wp->palette, c));
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
		else if (wp->cached_dim != wp->cached_active_dim)
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

		/* If the pane is floating, move to the front. */
		if (window_pane_is_floating(wp)) {
			TAILQ_REMOVE(&w->z_index, wp, zentry);
			TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);
			wp->flags |= PANE_REDRAW;
			redraw_invalidate_scene(w);
		}

		wp = w->active;
		if (wp == NULL)
			break;
	}
}

struct window_pane *
window_get_active_at(struct window *w, u_int x, u_int y)
{
	struct window_pane	*wp;
	int			 pane_status, xoff, yoff;
	u_int			 sx, sy;

	pane_status = window_get_pane_status(w);

	if (pane_status == PANE_STATUS_TOP) {
		/*
		 * Prefer a pane's top border status line over the pane above's
		 * bottom border.
		 */
		TAILQ_FOREACH(wp, &w->z_index, zentry) {
			if (!window_pane_is_visible(wp) ||
			    window_pane_is_floating(wp))
				continue;

			window_pane_full_size_offset(wp, &xoff, &yoff, &sx,
			    &sy);
			if ((int)x < xoff || x > xoff + sx)
				continue;
			if ((int)y == yoff - 1)
				return (wp);
		}
	}

	TAILQ_FOREACH(wp, &w->z_index, zentry) {
		if (!window_pane_is_visible(wp))
			continue;
		window_pane_full_size_offset(wp, &xoff, &yoff, &sx, &sy);
		if (!window_pane_is_floating(wp)) {
			/*
			 * Tiled - to and including the right border, excluding
			 * the bottom border.
			 */
			if ((int)x < xoff || x > xoff + sx)
				continue;
			if (pane_status == PANE_STATUS_TOP) {
				if ((int)y < yoff - 1 || y > yoff + sy)
					continue;
			} else {
				if ((int)y < yoff || y > yoff + sy)
					continue;
			}
		} else {
			if (window_pane_get_pane_lines(wp) == PANE_LINES_NONE) {
				if ((int)x < xoff || (int)x >= xoff + (int)sx)
					continue;
				if ((int)y < yoff || (int)y >= yoff + (int)sy)
					continue;
			} else {
				/* Floating - include all borders. */
				if ((int)x < xoff - 1 || x > xoff + sx)
					continue;
				if ((int)y < yoff - 1 || y > yoff + sy)
					continue;
			}
		}
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

	status = window_get_pane_status(w);
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
	if (window_count_panes(w, 1) == 1)
		return (-1);

	if (w->active != wp)
		window_set_active_pane(w, wp, 1);
	wp->flags |= PANE_ZOOMED;

	TAILQ_FOREACH(wp1, &w->panes, entry) {
		wp1->saved_layout_cell = wp1->layout_cell;
		wp1->layout_cell = NULL;
	}

	w->saved_layout_root = w->layout_root;
	layout_init(w, wp);
	w->flags |= WINDOW_ZOOMED;
	events_fire_window("window-zoomed", w);
	events_fire_window("window-layout-changed", w);

	redraw_invalidate_scene(w);
	return (0);
}

int
window_unzoom(struct window *w, int notify)
{
	struct window_pane	*wp;

	if (!(w->flags & WINDOW_ZOOMED))
		return (-1);

	w->flags &= ~WINDOW_ZOOMED;
	layout_free(w, 0);
	w->layout_root = w->saved_layout_root;
	w->saved_layout_root = NULL;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		wp->layout_cell = wp->saved_layout_cell;
		wp->saved_layout_cell = NULL;
		wp->flags &= ~PANE_ZOOMED;
	}
	layout_fix_panes(w, NULL);

	if (notify) {
		events_fire_window("window-unzoomed", w);
		events_fire_window("window-layout-changed", w);
	}

	redraw_invalidate_scene(w);
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
	return (window_unzoom(w, 1) == 0);
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
		if (flags & (SPAWN_FULLSIZE|SPAWN_FLOATING))
			TAILQ_INSERT_TAIL(&w->panes, wp, entry);
		else
			TAILQ_INSERT_AFTER(&w->panes, other, wp, entry);
	}
	if (~flags & SPAWN_FLOATING)
		TAILQ_INSERT_TAIL(&w->z_index, wp, zentry);
	else {
		TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);
	}
	redraw_invalidate_scene(w);
	return (wp);
}

void
window_lost_pane(struct window *w, struct window_pane *wp)
{
	log_debug("%s: @%u pane %%%u", __func__, w->id, wp->id);

	if (wp == marked_pane.wp)
		server_clear_marked();

	window_pane_stack_remove(&w->last_panes, wp);
	if (wp == w->active) {
		w->active = TAILQ_FIRST(&w->last_panes);
		if (w->active == NULL) {
			w->active = TAILQ_PREV(wp, window_panes, entry);
			if (w->active == NULL)
				w->active = TAILQ_NEXT(wp, entry);
		}
		if (w->active != NULL) {
			window_pane_stack_remove(&w->last_panes, w->active);
			w->active->flags |= PANE_CHANGED;
			window_fire_pane_changed(w, w->active, wp);
			window_update_focus(w);
		}
	}
	redraw_invalidate_scene(w);
}

void
window_remove_pane(struct window *w, struct window_pane *wp)
{
	window_lost_pane(w, wp);
	TAILQ_REMOVE(&w->panes, wp, entry);
	TAILQ_REMOVE(&w->z_index, wp, zentry);
	redraw_invalidate_scene(w);
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
	struct window		*w = wp->window;
	struct window_pane	*wq;

	*i = options_get_number(w->options, "pane-base-index");
	TAILQ_FOREACH(wq, &w->panes, entry) {
		if (wp == wq) {
			return (0);
		}
		(*i)++;
	}

	return (-1);
}

int
window_pane_zindex(struct window_pane *wp, u_int *i)
{
	struct window		*w = wp->window;
	struct window_pane	*wq;

	*i = 0;
	TAILQ_FOREACH(wq, &w->z_index, zentry) {
		if (wq == wp) {
			if (!window_pane_is_floating(wp))
				(*i)++;
			return (0);
		}
		if (window_pane_is_floating(wq))
			(*i)++;
	}

	return (-1);
}

u_int
window_count_panes(struct window *w, int with_floating)
{
	struct window_pane	*wp;
	u_int			 n = 0;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (with_floating || !window_pane_is_floating(wp))
			n++;
	}
	return (n);
}

void
window_destroy_panes(struct window *w)
{
	struct window_pane	*wp;

	while (!TAILQ_EMPTY(&w->last_panes)) {
		wp = TAILQ_FIRST(&w->last_panes);
		window_pane_stack_remove(&w->last_panes, wp);
	}

	while (!TAILQ_EMPTY(&w->panes)) {
		wp = TAILQ_FIRST(&w->panes);
		TAILQ_REMOVE(&w->panes, wp, entry);
		TAILQ_REMOVE(&w->z_index, wp, zentry);
		window_pane_destroy(wp);
	}
}

const char *
window_printable_flags(struct winlink *wl, int escape)
{
	struct session	*s = wl->session;
	static char	 flags[32];
	u_int		 pos = 0;

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

const char *
window_pane_printable_flags(struct window_pane *wp)
{
	struct window	*w = wp->window;
	static char	 flags[32];
	int		 pos = 0;

	if (wp == w->active)
		flags[pos++] = '*';
	if (wp == TAILQ_FIRST(&w->last_panes))
		flags[pos++] = '-';
	if (wp->flags & PANE_ZOOMED)
		flags[pos++] = 'Z';
	if (window_pane_is_floating(wp))
		flags[pos++] = 'F';
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
	wp->references = 1;
	wp->window = w;
	wp->options = options_create(w->options);
	wp->flags = PANE_STYLECHANGED;
	wp->cmd_status = -1;

	wp->id = next_window_pane_id++;
	RB_INSERT(window_pane_tree, &all_window_panes, wp);

	wp->fd = -1;

	TAILQ_INIT(&wp->modes);

	TAILQ_INIT (&wp->resize_queue);

	wp->sx = sx;
	wp->sy = sy;

	wp->pipe_fd = -1;

	wp->control_bg = -1;
	wp->control_fg = -1;

	style_set_scrollbar_style_from_option(&wp->scrollbar_style,
	    wp->options);

	colour_palette_init(&wp->palette);
	colour_palette_from_option(&wp->palette, wp->options);

	screen_init(&wp->base, sx, sy, hlimit);
	wp->screen = &wp->base;
	window_pane_default_cursor(wp);

	screen_init(&wp->status_screen, 1, 1, 0);
	style_ranges_init(&wp->border_status_line.ranges);
	evtimer_set(&wp->sb_auto_timer, window_pane_scrollbar_timer, wp);

	if (gethostname(host, sizeof host) == 0)
		screen_set_title(&wp->base, host, 0);

	return (wp);
}

void
window_pane_wait_finish(struct window_pane *wp)
{
	struct cmdq_item	*item = wp->wait_item;
	struct client		*c;
	int			 retval = 0;

	if (item == NULL)
		return;
	wp->wait_item = NULL;

	if (wp->flags & PANE_STATUSREADY) {
		if (WIFEXITED(wp->status))
			retval = WEXITSTATUS(wp->status);
		else if (WIFSIGNALED(wp->status))
			retval = WTERMSIG(wp->status) + 128;
	}

	c = cmdq_get_client(item);
	if (c != NULL && c->session == NULL)
		c->retval = retval;
	cmdq_continue(item);
}

static void
window_pane_free_modes(struct window_pane *wp)
{
	struct window_mode_entry	*wme;

	while (!TAILQ_EMPTY(&wp->modes)) {
		wme = TAILQ_FIRST(&wp->modes);
		TAILQ_REMOVE(&wp->modes, wme, entry);
		wme->mode->free(wme);
		free(wme);
	}

	wp->screen = &wp->base;
}

static void
window_pane_scrollbar_timer(__unused int fd, __unused short events, void *arg)
{
	struct window_pane	*wp = arg;

	wp->sb_auto_hover = 0;
	window_pane_scrollbar_hide(wp);
}

static int
window_pane_scrollbar_auto_hide(struct window_pane *wp)
{
	return (wp->window->sb == PANE_SCROLLBARS_MODAL ||
	    wp->window->sb == PANE_SCROLLBARS_AUTOHIDE);
}

int
window_pane_scrollbar_overlay_visible(struct window_pane *wp)
{
	return (window_pane_scrollbar_overlay(wp) &&
	    window_pane_scrollbar_visible(wp));
}

void
window_pane_scrollbar_redraw(struct window_pane *wp)
{
	if (window_pane_scrollbar_overlay_visible(wp)) {
		wp->flags |= PANE_REDRAW;
		return;
	}
	wp->flags |= PANE_REDRAWSCROLLBAR;
}

static void
window_pane_scrollbar_redraw_visibility(struct window_pane *wp)
{
	redraw_invalidate_scene(wp->window);
	wp->flags |= PANE_REDRAW;
	server_redraw_window(wp->window);
}

static void
window_pane_destroy(struct window_pane *wp)
{
	window_pane_wait_finish(wp);
	spawn_editor_finish(wp);

	RB_REMOVE(window_pane_tree, &all_window_panes, wp);
	wp->flags |= PANE_DESTROYED;
	window_pane_clear_prompt(wp);

	window_pane_free_modes(wp);
	screen_write_clear_dirty(wp);

	if (wp->fd != -1) {
		bufferevent_free(wp->event);
		wp->event = NULL;
		close(wp->fd);
		wp->fd = -1;
	}
	if (wp->ictx != NULL) {
		input_free(wp->ictx);
		wp->ictx = NULL;
	}

	if (wp->pipe_fd != -1) {
		bufferevent_free(wp->pipe_event);
		wp->pipe_event = NULL;
		close(wp->pipe_fd);
		wp->pipe_fd = -1;
	}

	if (event_initialized(&wp->resize_timer))
		event_del(&wp->resize_timer);
	if (event_initialized(&wp->sync_timer))
		event_del(&wp->sync_timer);
	if (event_initialized(&wp->sb_auto_timer))
		event_del(&wp->sb_auto_timer);
	window_pane_clear_resizes(wp, NULL);

	window_pane_remove_ref(wp, __func__);
}

static void
window_pane_free(struct window_pane *wp)
{
	log_debug("pane %%%u freed (%d references)", wp->id, wp->references);

	free(wp->searchstr);

	screen_free(&wp->status_screen);
	screen_free(&wp->base);

	options_free(wp->options);
	free((void *)wp->cwd);
	free(wp->shell);
	cmd_free_argv(wp->argc, wp->argv);
	colour_palette_free(&wp->palette);
	style_ranges_free(&wp->border_status_line.ranges);
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
	if (wp->event == NULL)
		fatalx("out of memory");
	wp->ictx = input_init(wp, wp->event, &wp->palette, NULL);

	bufferevent_enable(wp->event, EV_READ|EV_WRITE);
}

void
window_pane_clear_resizes(struct window_pane *wp,
    struct window_pane_resize *except)
{
	struct window_pane_resize	*r, *r1;

	TAILQ_FOREACH_SAFE(r, &wp->resize_queue, entry, r1) {
		if (r == except)
			continue;
		TAILQ_REMOVE(&wp->resize_queue, r, entry);
		free(r);
	}
}

void
window_pane_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_mode_entry	*wme;
	struct window_pane_resize	*r;
	struct event_payload		*ep;
	struct cmd_find_state		 fs;

	if (sx == wp->sx && sy == wp->sy)
		return;

	screen_write_stop_sync(wp);

	r = xmalloc(sizeof *r);
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

	ep = event_payload_create();
	cmd_find_from_pane(&fs, wp, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_pane(ep, "pane", wp);
	event_payload_set_window(ep, "window", wp->window);
	event_payload_set_uint(ep, "width", sx);
	event_payload_set_uint(ep, "height", sy);
	event_payload_set_uint(ep, "old_width", r->osx);
	event_payload_set_uint(ep, "old_height", r->osy);
	events_fire("pane-resized", ep);
}

int
window_pane_set_mode(struct window_pane *wp, struct window_pane *swp,
    const struct window_mode *mode, struct cmdq_item *item,
    struct cmd_find_state *fs, struct args *args)
{
	struct window_mode_entry	*wme;
	struct window			*w = wp->window;
	const char			*name = mode->name, *oname = NULL;

	if (!TAILQ_EMPTY(&wp->modes)) {
		if (TAILQ_FIRST(&wp->modes)->mode == mode)
			return (1);
		if (TAILQ_FIRST(&wp->modes)->mode->flags & WINDOW_MODE_NO_STACK)
			window_pane_reset_mode(wp);
	}
	if (!TAILQ_EMPTY(&wp->modes))
		oname = TAILQ_FIRST(&wp->modes)->mode->name;

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
		wme->screen = wme->mode->init(wme, item, fs, args);
		if (wme->screen == NULL) {
			TAILQ_REMOVE(&wp->modes, wme, entry);
			free(wme);
			return (1);
		}
	}
	wme->kill = args != NULL ? args_has(args, 'k') : 0;
	wp->screen = wme->screen;

	wp->flags |= (PANE_REDRAW|PANE_REDRAWSCROLLBAR|PANE_CHANGED);
	layout_fix_panes(w, NULL);

	server_redraw_window_borders(wp->window);
	server_status_window(wp->window);

	window_fire_pane_mode_changed("pane-mode-entered", wp, oname, name, 1);
	window_fire_pane_mode_changed("pane-mode-changed", wp, oname, name, 1);

	return (0);
}

void
window_pane_reset_mode(struct window_pane *wp)
{
	struct window_mode_entry	*wme, *next;
	struct window			*w = wp->window;
	int				 kill;
	const char			*name, *p;

	if (TAILQ_EMPTY(&wp->modes))
		return;

	wme = TAILQ_FIRST(&wp->modes);
	p = wme->mode->name;
	kill = wme->kill;
	TAILQ_REMOVE(&wp->modes, wme, entry);
	wme->mode->free(wme);
	free(wme);

	next = TAILQ_FIRST(&wp->modes);
	if (next == NULL) {
		wp->flags &= ~PANE_UNSEENCHANGES;
		log_debug("%s: no next mode", __func__);
		wp->screen = &wp->base;
	} else {
		log_debug("%s: next mode is %s", __func__, next->mode->name);
		wp->screen = next->screen;
		if (next->mode->resize != NULL)
			next->mode->resize(next, wp->sx, wp->sy);
	}
	name = (next == NULL ? NULL : next->mode->name);

	wp->flags |= (PANE_REDRAW|PANE_REDRAWSCROLLBAR|PANE_CHANGED);
	layout_fix_panes(w, NULL);

	server_redraw_window_borders(wp->window);
	server_status_window(wp->window);

	window_fire_pane_mode_changed("pane-mode-exited", wp, p, name, 0);
	window_fire_pane_mode_changed("pane-mode-changed", wp, p, name, 0);

	if (kill)
		server_kill_pane(wp);
}

/* Reset all modes. */
void
window_pane_reset_mode_all(struct window_pane *wp)
{
	while (!TAILQ_EMPTY(&wp->modes))
		window_pane_reset_mode(wp);
}

/* Prompt input callback. */
static enum prompt_result
window_pane_prompt_input_callback(void *data, const char *s,
    enum prompt_key_result key)
{
	struct window_pane_prompt	*wpp = data;

	if (wpp->inputcb != NULL)
		return (wpp->inputcb(wpp->c, wpp->data, s, key));
	return (PROMPT_CLOSE);
}

/* Prompt free callback. */
static void
window_pane_prompt_free_callback(void *data)
{
	struct window_pane_prompt	*wpp = data;
	struct window_pane		*wp;

	wp = window_pane_find_by_id(wpp->wp_id);
	if (wp != NULL && wp->prompt_data == wpp)
		wp->prompt_data = NULL;
	if (wpp->freecb != NULL)
		wpp->freecb(wpp->data);
	free(wpp);
}

/* Open a prompt owned by a pane, drawn over the pane instead of the status. */
void
window_pane_set_prompt(struct window_pane *wp, struct client *c,
    struct cmd_find_state *fs, const char *msg, const char *input,
    status_prompt_input_cb inputcb, prompt_free_cb freecb, void *data,
    int flags, enum prompt_type type)
{
	struct session			*s = NULL;
	struct prompt_create_data	 pd;
	struct window_pane_prompt	*wpp;

	if (c != NULL)
		s = c->session;

	window_pane_clear_prompt(wp);

	wpp = xcalloc(1, sizeof *wpp);
	wpp->wp_id = wp->id;
	wpp->c = c;
	wpp->inputcb = inputcb;
	wpp->freecb = freecb;
	wpp->data = data;
	wpp->type = type;

	memset(&pd, 0, sizeof pd);
	prompt_set_options(&pd, s);
	pd.fs = fs;
	pd.prompt = msg;
	pd.input = input;
	pd.type = type;
	pd.flags = flags;
	pd.inputcb = window_pane_prompt_input_callback;
	pd.freecb = window_pane_prompt_free_callback;
	pd.data = wpp;

	wp->prompt = prompt_create(&pd);
	wp->prompt_data = wpp;
	wp->flags |= PANE_REDRAW;

	prompt_incremental_start(wp->prompt);
	window_fire_pane_prompt("pane-prompt-opened", wp, type);
}

/* Close a pane prompt. */
void
window_pane_clear_prompt(struct window_pane *wp)
{
	struct prompt			*prompt = wp->prompt;
	struct window_pane_prompt	*wpp = wp->prompt_data;
	enum prompt_type		 type = PROMPT_TYPE_INVALID;

	if (prompt != NULL) {
		if (wpp != NULL)
			type = wpp->type;

		wp->prompt = NULL;
		prompt_free(prompt);
		wp->flags |= PANE_REDRAW;

		if (~wp->flags & PANE_DESTROYED)
			window_fire_pane_prompt("pane-prompt-closed", wp, type);
	}
}

/* Does this pane have an open prompt? */
int
window_pane_has_prompt(struct window_pane *wp)
{
	return (wp->prompt != NULL);
}

/* Replace the message and input of an open pane prompt. */
void
window_pane_update_prompt(struct window_pane *wp, const char *msg,
    const char *input)
{
	if (wp->prompt != NULL) {
		prompt_update(wp->prompt, msg, input);
		wp->flags |= PANE_REDRAW;
	}
}

/*
 * Pass a key to a pane prompt. The client is set transiently for the duration
 * of the key in case the prompt or pane is destroyed by the callback.
 */
enum prompt_key_result
window_pane_prompt_key(struct window_pane *wp, struct client *c, key_code key,
    struct mouse_event *m)
{
	struct prompt			*prompt = wp->prompt;
	struct window_pane_prompt	*wpp = wp->prompt_data;
	enum prompt_key_result		 result;
	u_int				 wp_id = wp->id, x, y, py;
	int				 redraw = 0;

	if (prompt == NULL)
		return (PROMPT_KEY_NOT_HANDLED);

	if (wpp != NULL)
		wpp->c = c;
	if (KEYC_IS_MOUSE(key)) {
		if (m == NULL ||
		    MOUSE_BUTTONS(m->b) != MOUSE_BUTTON_1 ||
		    MOUSE_DRAG(m->b) ||
		    MOUSE_RELEASE(m->b) ||
		    cmd_mouse_at(wp, m, &x, &y, 0) != 0)
			result = PROMPT_KEY_NOT_HANDLED;
		else {
			if (c != NULL && status_at_line(c) == 0)
				py = 0;
			else
				py = wp->sy - 1;
			if (y == py) {
				result = prompt_mouse(prompt, x, 0, wp->sx,
				    &redraw);
			} else
				result = PROMPT_KEY_NOT_HANDLED;
		}
	} else
		result = prompt_key(prompt, key, &redraw);

	wp = window_pane_find_by_id(wp_id);
	if (wp == NULL)
		return (result);
	if (wpp != NULL && wp->prompt_data == wpp)
		wpp->c = NULL;

	/*
	 * Only an explicit close or the prompt marking itself closed ends it;
	 * cursor movement and editing keep it open.
	 */
	if (wp->prompt == prompt &&
	    (result == PROMPT_KEY_CLOSE || prompt_closed(prompt)))
		window_pane_clear_prompt(wp);

	if (redraw || wp->prompt != prompt)
		wp->flags |= PANE_REDRAW;

	return (result);
}

static void
window_pane_copy_paste(struct window_pane *wp, char *buf, size_t len)
{
	struct window_pane	*loop;

	TAILQ_FOREACH(loop, &wp->window->panes, entry) {
		if (loop != wp &&
		    TAILQ_EMPTY(&loop->modes) &&
		    loop->fd != -1 &&
		    (~loop->flags & PANE_INPUTOFF) &&
		    window_pane_is_visible(loop) &&
		    options_get_number(loop->options, "synchronize-panes")) {
			log_debug("%s: %.*s", __func__, (int)len, buf);
			bufferevent_write(loop->event, buf, len);
		}
	}
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
		    window_pane_is_visible(loop) &&
		    options_get_number(loop->options, "synchronize-panes"))
			input_key_pane(loop, key, NULL);
	}
}

void
window_pane_paste(struct window_pane *wp, key_code key, char *buf, size_t len)
{
	if (!TAILQ_EMPTY(&wp->modes))
		return;

	if (wp->fd == -1 || wp->flags & PANE_INPUTOFF)
		return;

	if (KEYC_IS_PASTE(key) && (~wp->screen->mode & MODE_BRACKETPASTE))
		return;

	log_debug("%s: %.*s", __func__, (int)len, buf);
	bufferevent_write(wp->event, buf, len);

	if (options_get_number(wp->options, "synchronize-panes"))
		window_pane_copy_paste(wp, buf, len);
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
		/*
		 * No mode uses mouse motion events, so drop them here rather
		 * than passing them on and causing a redraw on every movement.
		 */
		if (KEYC_IS_TYPE(key, KEYC_TYPE_MOUSEMOVE))
			return (0);
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
window_pane_is_visible(struct window_pane *wp)
{
	if (~wp->window->flags & WINDOW_ZOOMED)
		return (1);
	return (wp == wp->window->active);
}

int
window_pane_exited(struct window_pane *wp)
{
	return (wp->fd == -1 || (wp->flags & PANE_EXITED));
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
 * Get full size and offset of a window pane including the area of the
 * scrollbars if they were visible but not including the border(s).
 */
static void
window_pane_full_size_offset(struct window_pane *wp, int *xoff, int *yoff,
    u_int *sx, u_int *sy)
{
	struct window		*w = wp->window;
	u_int			 sb_w;

	if (window_pane_scrollbar_reserve(wp))
		sb_w = wp->scrollbar_style.width + wp->scrollbar_style.pad;
	else
		sb_w = 0;
	if (w->sb_pos == PANE_SCROLLBARS_LEFT) {
		*xoff = wp->xoff - sb_w;
		*sx = wp->sx + sb_w;
	} else { /* sb_pos == PANE_SCROLLBARS_RIGHT */
		*xoff = wp->xoff;
		*sx = wp->sx + sb_w;
	}
	*yoff = wp->yoff;
	*sy = wp->sy;
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
	int			 edge, left, right, end, status, found;
	int			 xoff, yoff;
	u_int			 size, sx, sy;

	if (wp == NULL)
		return (NULL);
	w = wp->window;
	status = window_get_pane_status(w);

	list = NULL;
	size = 0;

	window_pane_full_size_offset(wp, &xoff, &yoff, &sx, &sy);

	edge = yoff;
	if (status == PANE_STATUS_TOP) {
		if (edge == 1)
			edge = (int)w->sy + 1;
	} else if (status == PANE_STATUS_BOTTOM) {
		if (edge == 0)
			edge = (int)w->sy;
	} else {
		if (edge == 0)
			edge = (int)w->sy + 1;
	}

	left = xoff;
	right = xoff + (int)sx;

	TAILQ_FOREACH(next, &w->panes, entry) {
		window_pane_full_size_offset(next, &xoff, &yoff, &sx, &sy);
		if (next == wp)
			continue;
		if (yoff + (int)sy + 1 != edge)
			continue;
		end = xoff + (int)sx - 1;

		found = 0;
		if (xoff < left && end > right)
			found = 1;
		else if (xoff >= left && xoff <= right)
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
	int			 edge, left, right, end, status, found;
	int			 xoff, yoff;
	u_int			 size, sx, sy;

	if (wp == NULL)
		return (NULL);
	w = wp->window;
	status = window_get_pane_status(w);

	list = NULL;
	size = 0;

	window_pane_full_size_offset(wp, &xoff, &yoff, &sx, &sy);

	edge = yoff + (int)sy + 1;
	if (status == PANE_STATUS_TOP) {
		if (edge >= (int)w->sy)
			edge = 1;
	} else if (status == PANE_STATUS_BOTTOM) {
		if (edge >= (int)w->sy - 1)
			edge = 0;
	} else {
		if (edge >= (int)w->sy)
			edge = 0;
	}

	left = wp->xoff;
	right = wp->xoff + (int)wp->sx;

	TAILQ_FOREACH(next, &w->panes, entry) {
		window_pane_full_size_offset(next, &xoff, &yoff, &sx, &sy);
		if (next == wp)
			continue;
		if (yoff != edge)
			continue;
		end = xoff + (int)sx - 1;

		found = 0;
		if (xoff < left && end > right)
			found = 1;
		else if (xoff >= left && xoff <= right)
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
	int			 edge, top, bottom, end, found;
	int			 xoff, yoff;
	u_int			 size, sx, sy;

	if (wp == NULL)
		return (NULL);
	w = wp->window;

	list = NULL;
	size = 0;

	window_pane_full_size_offset(wp, &xoff, &yoff, &sx, &sy);

	edge = xoff;
	if (edge == 0)
		edge = (int)w->sx + 1;

	top = yoff;
	bottom = yoff + (int)sy;

	TAILQ_FOREACH(next, &w->panes, entry) {
		window_pane_full_size_offset(next, &xoff, &yoff, &sx, &sy);
		if (next == wp)
			continue;
		if (xoff + (int)sx + 1 != edge)
			continue;
		end = yoff + (int)sy - 1;

		found = 0;
		if (yoff < top && end > bottom)
			found = 1;
		else if (yoff >= top && yoff <= bottom)
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
	int			 edge, top, bottom, end, found;
	int			 xoff, yoff;
	u_int			 size, sx, sy;

	if (wp == NULL)
		return (NULL);
	w = wp->window;

	list = NULL;
	size = 0;

	window_pane_full_size_offset(wp, &xoff, &yoff, &sx, &sy);

	edge = xoff + (int)sx + 1;
	if (edge >= (int)w->sx)
		edge = 0;

	top = wp->yoff;
	bottom = wp->yoff + (int)wp->sy;

	TAILQ_FOREACH(next, &w->panes, entry) {
		window_pane_full_size_offset(next, &xoff, &yoff, &sx, &sy);
		if (next == wp)
			continue;
		if (xoff != edge)
			continue;
		end = yoff + (int)sy - 1;

		found = 0;
		if (yoff < top && end > bottom)
			found = 1;
		else if (yoff >= top && yoff <= bottom)
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

/* Add window to stack. */
void
window_pane_stack_push(struct window_panes *stack, struct window_pane *wp)
{
	if (wp != NULL) {
		window_pane_stack_remove(stack, wp);
		TAILQ_INSERT_HEAD(stack, wp, sentry);
		wp->flags |= PANE_VISITED;
	}
}

/* Remove window from stack. */
void
window_pane_stack_remove(struct window_panes *stack, struct window_pane *wp)
{
	if (wp != NULL && (wp->flags & PANE_VISITED)) {
		TAILQ_REMOVE(stack, wp, sentry);
		wp->flags &= ~PANE_VISITED;
	}
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
	if (cdata->file != NULL && (wp == NULL || c->flags & CLIENT_DEAD)) {
		if (wp == NULL) {
			c->retval = 1;
			c->flags |= CLIENT_EXIT;
		}
		file_cancel(cdata->file);
	} else if (cdata->file == NULL || closed || error != 0) {
		cmdq_continue(cdata->item);
		server_client_unref(c);
		free(cdata);
	} else
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
	if (c->flags & (CLIENT_DEAD|CLIENT_EXITED))
		return (1);
	if (c->session != NULL)
		return (1);

	cdata = xmalloc(sizeof *cdata);
	cdata->item = item;
	cdata->wp = wp->id;
	cdata->file = file_read(c, "-", window_pane_input_callback, cdata);
	c->references++;

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

void
window_set_fill_character(struct window *w)
{
	const char		*value;
	struct utf8_data	*ud;

	free(w->fill_character);
	w->fill_character = NULL;

	value = options_get_string(w->options, "fill-character");
	if (*value != '\0' && utf8_isvalid(value)) {
		ud = utf8_fromcstr(value);
		if (ud != NULL && ud[0].width == 1)
			w->fill_character = ud;
		else
			free(ud);
	}
}

void
window_pane_default_cursor(struct window_pane *wp)
{
	screen_set_default_cursor(wp->screen, wp->options);
}

int
window_pane_mode(struct window_pane *wp)
{
	if (TAILQ_FIRST(&wp->modes) != NULL) {
		if (TAILQ_FIRST(&wp->modes)->mode == &window_copy_mode)
			return (WINDOW_PANE_COPY_MODE);
		if (TAILQ_FIRST(&wp->modes)->mode == &window_view_mode)
			return (WINDOW_PANE_VIEW_MODE);
	}
	return (WINDOW_PANE_NO_MODE);
}

int
window_pane_show_scrollbar(struct window_pane *wp)
{
	if (SCREEN_IS_ALTERNATE(&wp->base))
		return (0);
	if (wp->window->sb == PANE_SCROLLBARS_ALWAYS ||
	    wp->window->sb == PANE_SCROLLBARS_AUTOHIDE ||
	    (wp->window->sb == PANE_SCROLLBARS_MODAL &&
	    window_pane_mode(wp) != WINDOW_PANE_NO_MODE))
		return (1);
	return (0);
}

int
window_pane_scrollbar_reserve(struct window_pane *wp)
{
	if (!window_pane_show_scrollbar(wp))
		return (0);
	return (wp->window->sb == PANE_SCROLLBARS_ALWAYS);
}

int
window_pane_scrollbar_overlay(struct window_pane *wp)
{
	if (!window_pane_show_scrollbar(wp))
		return (0);
	return (window_pane_scrollbar_auto_hide(wp));
}

int
window_pane_scrollbar_visible(struct window_pane *wp)
{
	if (!window_pane_show_scrollbar(wp))
		return (0);
	if (!window_pane_scrollbar_auto_hide(wp))
		return (1);
	return (wp->sb_auto_visible);
}

void
window_pane_scrollbar_start_timer(struct window_pane *wp)
{
	struct timeval	tv;
	u_int		delay;

	if (!window_pane_scrollbar_auto_hide(wp) || !wp->sb_auto_visible)
		return;

	delay = options_get_number(wp->window->options,
	    "pane-scrollbars-timeout");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;
	evtimer_del(&wp->sb_auto_timer);
	evtimer_add(&wp->sb_auto_timer, &tv);
}

void
window_pane_scrollbar_show(struct window_pane *wp, int start_timer)
{
	int	changed = 0;
	if (!window_pane_scrollbar_auto_hide(wp))
		return;
	if (!window_pane_show_scrollbar(wp))
		return;
	if (!wp->sb_auto_visible) {
		wp->sb_auto_visible = 1;
		changed = 1;
	}
	evtimer_del(&wp->sb_auto_timer);
	if (start_timer)
		window_pane_scrollbar_start_timer(wp);
	if (changed)
		window_pane_scrollbar_redraw_visibility(wp);
}

void
window_pane_scrollbar_hide(struct window_pane *wp)
{
	if (event_initialized(&wp->sb_auto_timer))
		evtimer_del(&wp->sb_auto_timer);
	wp->sb_auto_hover = 0;
	if (!wp->sb_auto_visible)
		return;
	wp->sb_auto_visible = 0;
	window_pane_scrollbar_redraw_visibility(wp);
}

int
window_pane_get_bg(struct window_pane *wp)
{
	int			c;
	struct grid_cell	defaults;

	c = window_pane_get_bg_control_client(wp);
	if (c == -1) {
		tty_default_colours(&defaults, wp, NULL);
		if (COLOUR_DEFAULT(defaults.bg))
			c = window_get_bg_client(wp);
		else
			c = defaults.bg;
	}
	return (c);
}

/* Get a client with a background for the pane. */
int
window_get_bg_client(struct window_pane *wp)
{
	struct window	*w = wp->window;
	struct client	*loop;

	TAILQ_FOREACH(loop, &clients, entry) {
		if (loop->flags & CLIENT_UNATTACHEDFLAGS)
			continue;
		if (loop->session == NULL || !session_has(loop->session, w))
			continue;
		if (loop->tty.bg == -1)
			continue;
		return (loop->tty.bg);
	}
	return (-1);
}

/*
 * If any control mode client exists that has provided a bg color, return it.
 * Otherwise, return -1.
 */
int
window_pane_get_bg_control_client(struct window_pane *wp)
{
	struct client	*c;

	if (wp->control_bg == -1)
		return (-1);

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->flags & CLIENT_CONTROL)
			return (wp->control_bg);
	}
	return (-1);
}

/*
 * Get a client with a foreground for the pane. There isn't much to choose
 * between them so just use the first.
 */
int
window_pane_get_fg(struct window_pane *wp)
{
	struct window	*w = wp->window;
	struct client	*loop;

	TAILQ_FOREACH(loop, &clients, entry) {
		if (loop->flags & CLIENT_UNATTACHEDFLAGS)
			continue;
		if (loop->session == NULL || !session_has(loop->session, w))
			continue;
		if (loop->tty.fg == -1)
			continue;
		return (loop->tty.fg);
	}
	return (-1);
}

/*
 * If any control mode client exists that has provided a fg color, return it.
 * Otherwise, return -1.
 */
int
window_pane_get_fg_control_client(struct window_pane *wp)
{
	struct client	*c;

	if (wp->control_fg == -1)
		return (-1);

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->flags & CLIENT_CONTROL)
			return (wp->control_fg);
	}
	return (-1);
}

enum client_theme
window_pane_get_theme(struct window_pane *wp)
{
	struct window		*w;
	struct client		*loop;
	int			 found_light = 0, found_dark = 0;

	if (wp == NULL)
		return (THEME_UNKNOWN);
	w = wp->window;

	/*
	 * Prefer a theme reported by an attached client with mode 2031 or DSR
	 * 996: the terminal knows its own light or dark mode.
	 */
	TAILQ_FOREACH(loop, &clients, entry) {
		if (loop->flags & CLIENT_UNATTACHEDFLAGS)
			continue;
		if (loop->session == NULL || !session_has(loop->session, w))
			continue;
		switch (loop->theme) {
		case THEME_LIGHT:
			found_light = 1;
			break;
		case THEME_DARK:
			found_dark = 1;
			break;
		case THEME_UNKNOWN:
			break;
		}
	}
	if (found_dark && !found_light)
		return (THEME_DARK);
	if (found_light && !found_dark)
		return (THEME_LIGHT);

	/*
	 * Otherwise guess from the pane background colour, for terminals which
	 * do not report a theme themselves.
	 */
	return (colour_totheme(window_pane_get_bg(wp)));
}

void
window_pane_send_theme_update(struct window_pane *wp)
{
	enum client_theme	theme;

	if (wp == NULL || window_pane_exited(wp))
		return;
	if (~wp->flags & PANE_THEMECHANGED)
		return;
	if (~wp->screen->mode & MODE_THEME_UPDATES)
		return;

	theme = window_pane_get_theme(wp);
	if (theme == wp->last_theme)
		return;
	wp->last_theme = theme;
	wp->flags &= ~PANE_THEMECHANGED;

	switch (theme) {
	case THEME_LIGHT:
		log_debug("%s: %%%u light theme", __func__, wp->id);
		bufferevent_write(wp->event, "\033[?997;2n", 9);
		break;
	case THEME_DARK:
		log_debug("%s: %%%u dark theme", __func__, wp->id);
		bufferevent_write(wp->event, "\033[?997;1n", 9);
		break;
	case THEME_UNKNOWN:
		log_debug("%s: %%%u unknown theme", __func__, wp->id);
		break;
	}
}

struct style_range *
window_pane_status_get_range(struct window_pane *wp, u_int x, u_int y)
{
	struct style_ranges	*srs;
	u_int			 line;
	int			 pane_status;

	if (wp == NULL)
		return (NULL);
	srs = &wp->border_status_line.ranges;

	pane_status = window_pane_get_pane_status(wp);
	if (pane_status == PANE_STATUS_TOP)
		line = wp->yoff - 1;
	else if (pane_status == PANE_STATUS_BOTTOM)
		line = wp->yoff + wp->sy;
	if (pane_status == PANE_STATUS_OFF || line != y)
		return (NULL);

	/*
	 * The border formats start 2 off but that isn't reflected in
	 * the stored bounds of the range.
	 */
	return (style_ranges_get_range(srs, x - wp->xoff - 2));
}

enum pane_lines
window_get_pane_lines(struct window *w)
{
	struct options	*oo;

	oo = w->options;
	return (options_get_number(oo, "pane-border-lines"));
}

enum pane_lines
window_pane_get_pane_lines(struct window_pane *wp)
{
	struct options	*oo;

	if (!window_pane_is_floating(wp))
		oo = wp->window->options;
	else
		oo = wp->options;
	return (options_get_number(oo, "pane-border-lines"));
}

int
window_get_pane_status(struct window *w)
{
	int	status;

	status = options_get_number(w->options, "pane-border-status");
	if (status == PANE_STATUS_TOP_FLOATING ||
	    status == PANE_STATUS_BOTTOM_FLOATING)
		return (PANE_STATUS_OFF);
	return (status);
}

int
window_pane_get_pane_status(struct window_pane *wp)
{
	struct window_mode_entry	*wme;
	int	status;

	wme = TAILQ_FIRST(&wp->modes);
	if (wme != NULL &&
	    (wme->mode->flags & WINDOW_MODE_HIDE_PANE_STATUS) &&
	    (wp->flags & PANE_ZOOMED))
		return (PANE_STATUS_OFF);

	if (!window_pane_is_floating(wp))
		return (window_get_pane_status(wp->window));
	if (window_pane_get_pane_lines(wp) == PANE_LINES_NONE)
		return (PANE_STATUS_OFF);

	status = options_get_number(wp->options, "pane-border-status");
	if (status == PANE_STATUS_TOP_FLOATING)
		return (PANE_STATUS_TOP);
	if (status == PANE_STATUS_BOTTOM_FLOATING)
		return (PANE_STATUS_BOTTOM);
	return (status);
}

int
window_pane_is_floating(struct window_pane *wp)
{
	struct layout_cell	*lc = wp->layout_cell;

	if (lc == NULL || (lc->flags & LAYOUT_CELL_FLOATING) == 0)
		return (0);
	return (1);
}
