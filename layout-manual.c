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

#include "tmux.h"

void	layout_manual_v_update_offsets(struct window *);

void
layout_manual_v_refresh(struct window *w, unused int active_only)
{
	struct window_pane	*wp;
	u_int			 npanes, canfit, total;
	int			 left;

	if (active_only)
		return;

	if (TAILQ_EMPTY(&w->panes))
		return;

	/* Clear hidden flags. */
	TAILQ_FOREACH(wp, &w->panes, entry)
	    	wp->flags &= ~PANE_HIDDEN;

	/* Check the new size. */
	npanes = window_count_panes(w);
	if (w->sy <= PANE_MINIMUM * npanes) {
		/* How many can we fit? */
		canfit = w->sy / PANE_MINIMUM;
		if (canfit == 0) {
			/* None. Just use this size for the first. */
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (wp == TAILQ_FIRST(&w->panes))
					wp->sy = w->sy;
				else
					wp->flags |= PANE_HIDDEN;
			}
		} else {
			/* >=1, set minimum for them all. */
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (canfit-- > 0)
					wp->sy = PANE_MINIMUM - 1;
				else
					wp->flags |= PANE_HIDDEN;
			}
			/* And increase the first by the rest. */
			TAILQ_FIRST(&w->panes)->sy += 1 + w->sy % PANE_MINIMUM;
		}
	} else {
		/* In theory they will all fit. Find the current total. */
		total = 0;
		TAILQ_FOREACH(wp, &w->panes, entry)
			total += wp->sy;
		total += npanes - 1;

		/* Growing or shrinking? */
		left = w->sy - total;
		if (left > 0) {
			/* Growing. Expand evenly. */
			while (left > 0) {
				TAILQ_FOREACH(wp, &w->panes, entry) {
					wp->sy++;
					if (--left == 0)
						break;
				}
			}
		} else {
			/* Shrinking. Reduce evenly down to minimum. */
			while (left < 0) {
				TAILQ_FOREACH(wp, &w->panes, entry) {
					if (wp->sy <= PANE_MINIMUM - 1)
						continue;
					wp->sy--;
					if (++left == 0)
						break;
				}
			}
		}
	}

	/* Now do the resize. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		wp->sy--;
	    	window_pane_resize(wp, w->sx, wp->sy + 1);
	}

	/* Fill in the offsets. */
	layout_manual_v_update_offsets(w);

	/* Switch the active window if necessary. */
	window_set_active_pane(w, w->active);
}

void
layout_manual_v_resize(struct window_pane *wp, int adjust)
{
	struct window		*w = wp->window;
	struct window_pane	*wq;

	if (adjust > 0) {
		/*
		 * If this is not the last pane, keep trying to increase size
		 * and remove it from the next panes. If it is the last, do
		 * so on the previous pane.
		 */
		if (TAILQ_NEXT(wp, entry) == NULL) {
			if (wp == TAILQ_FIRST(&w->panes)) {
				/* Only one pane. */
				return;
			}
			wp = TAILQ_PREV(wp, window_panes, entry);
		}
		while (adjust-- > 0) {
			wq = wp;
			while ((wq = TAILQ_NEXT(wq, entry)) != NULL) {
				if (wq->sy <= PANE_MINIMUM)
					continue;
				window_pane_resize(wq, wq->sx, wq->sy - 1);
				break;
			}
			if (wq == NULL)
				break;
			window_pane_resize(wp, wp->sx, wp->sy + 1);
		}
	} else {
		adjust = -adjust;
		/*
		 * If this is not the last pane, keep trying to reduce size
		 * and add to the following pane. If it is the last, do so on
		 * the previous pane.
		 */
		wq = TAILQ_NEXT(wp, entry);
		if (wq == NULL) {
			if (wp == TAILQ_FIRST(&w->panes)) {
				/* Only one pane. */
				return;
			}
			wq = wp;
			wp = TAILQ_PREV(wq, window_panes, entry);
		}
		while (adjust-- > 0) {
			if (wp->sy <= PANE_MINIMUM)
				break;
			window_pane_resize(wq, wq->sx, wq->sy + 1);
			window_pane_resize(wp, wp->sx, wp->sy - 1);
		}
	}

	layout_manual_v_update_offsets(w);
}

void
layout_manual_v_update_offsets(struct window *w)
{
	struct window_pane     *wp;
	u_int			yoff;

	yoff = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp->flags & PANE_HIDDEN)
			continue;
		wp->xoff = 0;
		wp->yoff = yoff;
		yoff += wp->sy + 1;
	}
}
