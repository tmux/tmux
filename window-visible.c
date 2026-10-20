/* $OpenBSD: window-visible.c,v 1.6 2026/10/02 15:20:41 nicm Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Check if a single character is within a visible range (not obscured by a
 * menu or floating pane).
 */
int
window_position_is_visible(struct visible_ranges *r, u_int px)
{
	u_int			 i;
	struct visible_range	*ri;

	if (r == NULL)
		return (1);
	for (i = 0; i < r->used; i++) {
		ri = &r->ranges[i];
		if (ri->nx != 0 && px >= ri->px && px < ri->px + ri->nx)
			return (1);
	}
	return (0);
}

/* Remove width cells starting at px from the visible ranges. */
static void
window_visible_ranges_remove(struct visible_ranges *r, int px, u_int width)
{
	struct visible_range	*ri;
	u_int			 i;
	int			 start, end, right = px + width;

	if (width == 0)
		return;

	for (i = 0; i < r->used; i++) {
		ri = &r->ranges[i];
		start = ri->px;
		end = start + ri->nx;
		if (ri->nx == 0 || right <= start || px >= end)
			continue;
		if (px <= start) {
			/* Remove the left part or the whole range. */
			ri->px = right < end ? right : end;
			ri->nx = end - ri->px;
		} else {
			/* Keep the left part, and split off any right part. */
			ri->nx = px - start;
			if (right < end) {
				server_client_ensure_ranges(r, r->used + 1);
				memmove(&r->ranges[i + 2], &r->ranges[i + 1],
				    (r->used - i - 1) * sizeof *r->ranges);
				r->ranges[i + 1].px = right;
				r->ranges[i + 1].nx = end - right;
				r->used++;
			}
		}
	}
}

/*
 * Construct ranges array for the line at starting at px,py of width cells of
 * base_wp that are unobstructed. All ranges are in window coordinates.
 */
struct visible_ranges *
window_visible_ranges(struct window_pane *base_wp, int px, int py, u_int width,
    struct visible_ranges *r)
{
	struct window_pane		*wp;
	struct window			*w;
	struct menu_data		*md;
	static struct visible_ranges	 sr = { NULL, 0, 0 };
	int				 found_self, sb_w, sb_pos;
	int				 lb, rb, tb, bb, no_border;

	if (py < 0 || width == 0)
		goto empty;
	if (px < 0) {
		if ((u_int)-px >= width)
			goto empty;
		width -= (u_int)-px;
		px = 0;
	}

	if (base_wp == NULL) {
		if (r != NULL)
			return (r);
		if (sr.ranges == NULL)
			sr.ranges = xcalloc(1, sizeof *sr.ranges);
		sr.ranges[0].px = px;
		sr.ranges[0].nx = width;
		sr.size = 1;
		sr.used = 1;
		return (&sr);
	}

	w = base_wp->window;
	if ((u_int)py >= w->sy || (u_int)px >= w->sx)
		goto empty;
	if (px + width > w->sx)
		width = w->sx - px;

	if (r == NULL) {
		/* Start with the entire width of the range. */
		server_client_ensure_ranges(&base_wp->r, 1);
		r = &base_wp->r;
		r->ranges[0].px = px;
		r->ranges[0].nx = width;
		r->used = 1;
	}

	/* The menu is above every pane in the window. */
	md = w->menu;
	if (md != NULL &&
	    (u_int)py >= menu_y(md) &&
	    (u_int)py - menu_y(md) < menu_height(md)) {
		window_visible_ranges_remove(r, menu_x(md), menu_width(md));
	}

	found_self = 0;
	TAILQ_FOREACH_REVERSE(wp, &w->z_index, window_panes_zindex, zentry) {
		if (wp == base_wp) {
			found_self = 1;
			continue;
		}

		if (window_pane_is_floating(wp) &&
		    window_pane_get_pane_lines(wp) == PANE_LINES_NONE)
			no_border = 1;
		else
			no_border = 0;
		if (no_border) {
			tb = wp->yoff;
			bb = wp->yoff + (int)wp->sy - 1;
		} else {
			tb = wp->yoff > 0 ? wp->yoff - 1 : 0;
			bb = wp->yoff + (int)wp->sy;
		}
		if (!found_self ||
		    !window_pane_is_visible(wp) ||
		    py < tb ||
		    py > bb)
			continue;
		if (!window_pane_is_floating(wp) && (py == tb || py == bb))
			continue;

		sb_w = wp->scrollbar_style.width + wp->scrollbar_style.pad;
		if (window_pane_scrollbar_reserve(wp))
			sb_pos = w->sb_pos;
		else
			sb_w = sb_pos = 0;

		if (no_border) {
			lb = wp->xoff;
			rb = wp->xoff + (int)wp->sx - 1;
		} else if (sb_pos == PANE_SCROLLBARS_LEFT) {
			if (wp->xoff > sb_w)
				lb = wp->xoff - 1 - sb_w;
			else
				lb = 0;
		} else { /* PANE_SCROLLBARS_RIGHT or none. */
			if (wp->xoff > 0)
				lb = wp->xoff - 1;
			else
				lb = 0;
		}
		if (!no_border) {
			if (sb_pos == PANE_SCROLLBARS_LEFT)
				rb = wp->xoff + (int)wp->sx;
			else /* PANE_SCROLLBARS_RIGHT or none. */
				rb = wp->xoff + (int)wp->sx + sb_w;
		}
		if (lb < 0)
			lb = 0;
		if (rb < 0)
			continue;
		if (no_border && rb >= (int)w->sx)
			rb = w->sx - 1;
		else if (!no_border && rb > (int)w->sx)
			rb = w->sx - 1;
		if (lb <= rb)
			window_visible_ranges_remove(r, lb, rb - lb + 1);
	}
	return (r);

empty:
	if (r == NULL) {
		if (sr.ranges == NULL)
			sr.ranges = xcalloc(1, sizeof *sr.ranges);
		sr.size = 1;
		sr.used = 0;
		return (&sr);
	}
	r->used = 0;
	return (r);
}
