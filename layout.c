/* $Id: layout.c,v 1.7 2009-05-16 11:48:47 nicm Exp $ */

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

#include <string.h>

#include "tmux.h"

/*
 * Layout functions: second argument (int) is 1 if definitely the /only/ change
 * has been the active pane has changed. If 0 then panes, active pane or both
 * may have changed.
 */
void	layout_manual(struct window *, int);
void	layout_active_only(struct window *, int);
void	layout_even_horizontal(struct window *, int);
void	layout_even_vertical(struct window *, int);
void	layout_left_vertical(struct window *, int);

const struct {
	const char	*name;
	void		(*fn)(struct window *, int);
} layouts[] = {
	{ "manual", layout_manual },
	{ "active-only", layout_active_only },
	{ "even-horizontal", layout_even_horizontal },
	{ "even-vertical", layout_even_vertical },
	{ "left-vertical", layout_left_vertical },
};

const char *
layout_name(struct window *w)
{
	return (layouts[w->layout].name);
}

int
layout_lookup(const char *name)
{
	u_int	i;
	int	matched = -1;

	for (i = 0; i < nitems(layouts); i++) {
		if (strncmp(layouts[i].name, name, strlen(name)) == 0) {
			if (matched != -1)	/* ambiguous */
				return (-1);
			matched = i;
		}
	}

	return (matched);
}

int
layout_select(struct window *w, u_int layout)
{
	if (layout > nitems(layouts) - 1 || layout == w->layout)
		return (-1);
	w->layout = layout;

	if (w->layout == 0) {
		/* XXX Special-case manual. */
		window_fit_panes(w);
		window_update_panes(w);
	}
	layout_refresh(w, 0);
	return (0);
}

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
	layout_refresh(w, 0);
}

void
layout_previous(struct window *w)
{
	if (w->layout == 0)
		w->layout = nitems(layouts) - 1;
	else
		w->layout--;
	if (w->layout == 0) {
		/* XXX Special-case manual. */
		window_fit_panes(w);
		window_update_panes(w);
	}
	layout_refresh(w, 0);
}

void
layout_refresh(struct window *w, unused int active_changed)
{
	layouts[w->layout].fn(w, active_changed);
	server_redraw_window(w);
}

void
layout_manual(unused struct window *w, unused int active_changed)
{
}

void
layout_active_only(struct window *w, unused int active_changed)
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
layout_even_horizontal(struct window *w, int active_changed)
{
	struct window_pane	*wp;
	u_int			 i, n, width, xoff;

	if (active_changed)
		return;

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n == 0)
		return;

	/* How many can we fit? */
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
layout_even_vertical(struct window *w, int active_changed)
{
	struct window_pane	*wp;
	u_int			 i, n, height, yoff;

	if (active_changed)
		return;

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n == 0)
		return;

	/* How many can we fit? */
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
layout_left_vertical(struct window *w, int active_changed)
{
	struct window_pane	*wp;
	u_int			 i, n, height, yoff;

	if (active_changed)
		return;

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n == 0)
		return;

	/* Need >1 pane and minimum columns; if fewer, display active only. */
	if (n == 1 || w->sx < 82 + PANE_MINIMUM) {
		layout_active_only(w, active_changed);
		return;
	}
	n--;

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
