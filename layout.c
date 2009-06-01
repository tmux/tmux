/* $OpenBSD$ */

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
 * Each layout has two functions, _refresh to relayout the panes and _resize to
 * resize a single pane.
 *
 * Second argument (int) to _refresh is 1 if the only change has been that the
 * active pane has changed. If 0 then panes, active pane or both may have
 * changed.
 */

void	layout_active_only_refresh(struct window *, int);
void	layout_even_h_refresh(struct window *, int);
void	layout_even_v_refresh(struct window *, int);
void	layout_main_h_refresh(struct window *, int);
void	layout_main_v_refresh(struct window *, int);

const struct {
	const char     *name;
	void		(*refresh)(struct window *, int);
	void		(*resize)(struct window_pane *, int);
} layouts[] = {
	{ "manual-vertical", layout_manual_v_refresh, layout_manual_v_resize },
	{ "active-only", layout_active_only_refresh, NULL },
	{ "even-horizontal", layout_even_h_refresh, NULL },
	{ "even-vertical", layout_even_v_refresh, NULL },
	{ "main-horizontal", layout_main_h_refresh, NULL },
	{ "main-vertical", layout_main_v_refresh, NULL },
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

	layout_refresh(w, 0);
	return (0);
}

void
layout_next(struct window *w)
{
	w->layout++;
	if (w->layout > nitems(layouts) - 1)
		w->layout = 0;
	layout_refresh(w, 0);
}

void
layout_previous(struct window *w)
{
	if (w->layout == 0)
		w->layout = nitems(layouts) - 1;
	else
		w->layout--;
	layout_refresh(w, 0);
}

void
layout_refresh(struct window *w, int active_only)
{
	layouts[w->layout].refresh(w, active_only);
	server_redraw_window(w);
}

int
layout_resize(struct window_pane *wp, int adjust)
{
	struct window	*w = wp->window;

	if (layouts[w->layout].resize == NULL)
		return (-1);
	layouts[w->layout].resize(wp, adjust);
	return (0);
}

void
layout_active_only_refresh(struct window *w, unused int active_only)
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
layout_even_h_refresh(struct window *w, int active_only)
{
	struct window_pane	*wp;
	u_int			 i, n, width, xoff;

	if (active_only)
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
layout_even_v_refresh(struct window *w, int active_only)
{
	struct window_pane	*wp;
	u_int			 i, n, height, yoff;

	if (active_only)
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
layout_main_v_refresh(struct window *w, int active_only)
{
	struct window_pane	*wp;
	u_int			 i, n, mainwidth, height, yoff;

	if (active_only)
		return;

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n == 0)
		return;

	/* Get the main pane width and add one for separator line. */
	mainwidth = options_get_number(&w->options, "main-pane-width") + 1;

	/* Need >1 pane and minimum columns; if fewer, display active only. */
	if (n == 1 || w->sx < mainwidth + PANE_MINIMUM) {
		layout_active_only_refresh(w, active_only);
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
			window_pane_resize(wp, mainwidth - 1, w->sy);
			wp->flags &= ~PANE_HIDDEN;
			continue;
		}

		if (i > n) {
			wp->flags |= PANE_HIDDEN;
			continue;
		}
		wp->flags &= ~PANE_HIDDEN;

		wp->xoff = mainwidth;
		wp->yoff = yoff;
		if (i != n - 1)
 			window_pane_resize(wp, w->sx - mainwidth, height - 1);
		else
 			window_pane_resize(wp, w->sx - mainwidth, height);

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

void
layout_main_h_refresh(struct window *w, int active_only)
{
	struct window_pane	*wp;
	u_int			 i, n, mainheight, width, xoff;

	if (active_only)
		return;

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n == 0)
		return;

	/* Get the main pane height and add one for separator line. */
	mainheight = options_get_number(&w->options, "main-pane-height") + 1;

	/* Need >1 pane and minimum rows; if fewer, display active only. */
	if (n == 1 || w->sy < mainheight + PANE_MINIMUM) {
		layout_active_only_refresh(w, active_only);
		return;
	}
	n--;

	/* How many can we fit, not including first? */
	if (w->sx / n < PANE_MINIMUM) {
		width = PANE_MINIMUM;
		n = w->sx / PANE_MINIMUM;
	} else
		width = w->sx / n;

	/* Fit the panes. */
	i = xoff = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp == TAILQ_FIRST(&w->panes)) {
			wp->xoff = 0;
			wp->yoff = 0;
			window_pane_resize(wp, w->sx, mainheight - 1);
			wp->flags &= ~PANE_HIDDEN;
			continue;
		}

		if (i > n) {
			wp->flags |= PANE_HIDDEN;
			continue;
		}
		wp->flags &= ~PANE_HIDDEN;

		wp->xoff = xoff;
		wp->yoff = mainheight;
		if (i != n - 1)
 			window_pane_resize(wp, width - 1, w->sy - mainheight);
		else
 			window_pane_resize(wp, width - 1, w->sy - mainheight);

		i++;
		xoff += width;
	}

	/* Any space left? */
	while (xoff++ < w->sx + 1) {
		wp = TAILQ_LAST(&w->panes, window_panes);
		while (wp != NULL && wp == TAILQ_FIRST(&w->panes))
			wp = TAILQ_PREV(wp, window_panes, entry);
		if (wp == NULL)
			break;
		window_pane_resize(wp, wp->sx + 1, wp->sy);
	}
}
