/* $Id: layout.c,v 1.1 2009-04-01 18:21:32 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include "tmux.h"

void	layout_manual(struct window *);
void	layout_active_only(struct window *);
void	layout_even_horizontal(struct window *);
void	layout_even_vertical(struct window *);
void	layout_left_vertical(struct window *);

const struct {
	const char	*name;
	void		(*fn)(struct window *);
} layouts[] = {
	{ "manual", layout_manual },
	{ "active-only", layout_active_only },
	{ "even-horizontal", layout_even_horizontal },
	{ "even-vertical", layout_even_vertical },
	{ "left-vertical", layout_left_vertical },
}; 

void
layout_next(struct window *w)
{
	w->layout++;
	if (w->layout > nitems(layouts) - 1) {
		w->layout = 0;
		/* XXX Special-case manual. */
		window_fit_panes(w);
		window_update_panes(w);
	}
	layout_refresh(w);
}

void
layout_refresh(struct window *w)
{
	layouts[w->layout].fn(w);
	server_redraw_window(w);
}

void
layout_manual(unused struct window *w)
{
}

void
layout_active_only(struct window *w)
{
	struct window_pane	*wp;
	
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp == w->active) { 
			wp->flags &= ~PANE_HIDDEN;
			wp->xoff = wp->yoff = 0;
			window_pane_resize(wp, w->sx, w->sy);
		} else
			wp->flags |= PANE_HIDDEN;
	}
}

void
layout_even_horizontal(struct window *w)
{
	struct window_pane	*wp;
	u_int			 i, n, width, xoff;

	/* How many can we fit? */
	n = window_count_panes(w);
	if (w->sx / n < PANE_MINIMUM) {
		width = PANE_MINIMUM;
		n = w->sx / PANE_MINIMUM;
	} else
		width = w->sx / n;
	
	/* Fit the panes. */
	i = xoff = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (i > n) {
			wp->flags |= PANE_HIDDEN;
			continue;
		}
		wp->flags &= ~PANE_HIDDEN;

		wp->xoff = xoff;
		wp->yoff = 0;
		if (i != n - 1)
 			window_pane_resize(wp, width - 1, w->sy);
		else
 			window_pane_resize(wp, width, w->sy);

		i++;
		xoff += width;
	}

	/* Any space left? */
	while (xoff++ < w->sx) {
		wp = TAILQ_LAST(&w->panes, window_panes);
		window_pane_resize(wp, wp->sx + 1, wp->sy);
	}
}

void
layout_even_vertical(struct window *w)
{
	struct window_pane	*wp;
	u_int			 i, n, height, yoff;

	/* How many can we fit? */
	n = window_count_panes(w);
	if (w->sy / n < PANE_MINIMUM) {
		height = PANE_MINIMUM;
		n = w->sy / PANE_MINIMUM;
	} else
		height = w->sy / n;
	
	/* Fit the panes. */
	i = yoff = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (i > n) {
			wp->flags |= PANE_HIDDEN;
			continue;
		}
		wp->flags &= ~PANE_HIDDEN;

		wp->xoff = 0;
		wp->yoff = yoff;
		if (i != n - 1)
 			window_pane_resize(wp, w->sx, height - 1);
		else
 			window_pane_resize(wp, w->sx, height);

		i++;
		yoff += height;
	}

	/* Any space left? */
	while (yoff++ < w->sy) {
		wp = TAILQ_LAST(&w->panes, window_panes);
		window_pane_resize(wp, wp->sx, wp->sy + 1);
	}
}

void
layout_left_vertical(struct window *w)
{
	struct window_pane	*wp;
	u_int			 i, n, height, yoff;

	/* Need >1 pane and minimum columns; if fewer, display active only. */
	n = window_count_panes(w) - 1;
	if (n == 0 || w->sx < 82 + PANE_MINIMUM) {
		layout_active_only(w);
		return;
	}

	/* How many can we fit, not including first? */
	if (w->sy / n < PANE_MINIMUM) {
		height = PANE_MINIMUM;
		n = w->sy / PANE_MINIMUM;
	} else
		height = w->sy / n;
	
	/* Fit the panes. */
	i = yoff = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp == TAILQ_FIRST(&w->panes)) {
			wp->xoff = 0;
			wp->yoff = 0;
			window_pane_resize(wp, 81, w->sy);
			wp->flags &= ~PANE_HIDDEN;
			continue;
		}

		if (i > n) {
			wp->flags |= PANE_HIDDEN;
			continue;
		}
		wp->flags &= ~PANE_HIDDEN;

		wp->xoff = 82;
		wp->yoff = yoff;
		if (i != n - 1)
 			window_pane_resize(wp, w->sx - 82, height - 1);
		else
 			window_pane_resize(wp, w->sx - 82, height);

		i++;
		yoff += height;
	}

	/* Any space left? */
	while (yoff++ < w->sy) {
		wp = TAILQ_LAST(&w->panes, window_panes);
		while (wp != NULL && wp == TAILQ_FIRST(&w->panes))
			wp = TAILQ_PREV(wp, window_panes, entry);
		if (wp == NULL)
			break;
		window_pane_resize(wp, wp->sx, wp->sy + 1);
	}
}
