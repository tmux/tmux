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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Figure out the pane position based on a description. Fairly simple right
 * now, just understands a set of strings: left, right, top, bottom, top-left
 * top-right, bottom-left, bottom-right.
 */

struct layout_cell *layout_find_top(struct layout_cell *);
struct layout_cell *layout_find_bottom(struct layout_cell *);
struct layout_cell *layout_find_left(struct layout_cell *);
struct layout_cell *layout_find_right(struct layout_cell *);
struct layout_cell *layout_find_topleft(struct layout_cell *);
struct layout_cell *layout_find_topright(struct layout_cell *);
struct layout_cell *layout_find_bottomleft(struct layout_cell *);
struct layout_cell *layout_find_bottomright(struct layout_cell *);

/* Find the cell; returns NULL if string not understood. */
struct layout_cell *
layout_find_string(struct window *w, const char *s)
{
	struct layout_cell	*lc;

	lc = NULL;

	if (strcasecmp(s, "top") == 0)
		lc = layout_find_top(w->layout_root);
	else if (strcasecmp(s, "bottom") == 0)
		lc = layout_find_bottom(w->layout_root);
	else if (strcasecmp(s, "left") == 0)
		lc = layout_find_left(w->layout_root);
	else if (strcasecmp(s, "right") == 0)
		lc = layout_find_right(w->layout_root);
	else if (strcasecmp(s, "top-left") == 0)
		lc = layout_find_topleft(w->layout_root);
	else if (strcasecmp(s, "top-right") == 0)
		lc = layout_find_topright(w->layout_root);
	else if (strcasecmp(s, "bottom-left") == 0)
		lc = layout_find_bottomleft(w->layout_root);
	else if (strcasecmp(s, "bottom-right") == 0)
		lc = layout_find_bottomright(w->layout_root);

	if (lc == NULL || lc->type != LAYOUT_WINDOWPANE)
		return (NULL);
	return (lc);
}

/*
 * Find the top cell. Because splits in the same direction are stored as a
 * list, this is just the first in the list. Return NULL if no topmost cell.
 * For an unnested cell (not split), the top cell is always itself.
 */
struct layout_cell *
layout_find_top(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	else if (lc->type == LAYOUT_TOPBOTTOM)
		return (TAILQ_FIRST(&lc->cells));
	return (NULL);
}

/*
 * Find the bottom cell. Similarly to the top cell, this is just the last in
 * the list.
 */
struct layout_cell *
layout_find_bottom(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	else if (lc->type == LAYOUT_TOPBOTTOM)
		return (TAILQ_LAST(&lc->cells, layout_cells));
	return (NULL);
}

/* Find the left cell. */
struct layout_cell *
layout_find_left(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	else if (lc->type == LAYOUT_LEFTRIGHT)
		return (TAILQ_FIRST(&lc->cells));
	return (NULL);
}

/* Find the right cell. */
struct layout_cell *
layout_find_right(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	else if (lc->type == LAYOUT_LEFTRIGHT)
		return (TAILQ_LAST(&lc->cells, layout_cells));
	return (NULL);
}

/*
 * Find the top-left cell. This means recursing until there are no more moves
 * to be made.
 */
struct layout_cell *
layout_find_topleft(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	lc = TAILQ_FIRST(&lc->cells);
	return (layout_find_topleft(lc));
}

/* Find the top-right cell. */
struct layout_cell *
layout_find_topright(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	if (lc->type == LAYOUT_LEFTRIGHT)
		lc = TAILQ_LAST(&lc->cells, layout_cells);
	else
		lc = TAILQ_FIRST(&lc->cells);
	return (layout_find_topright(lc));
}

/* Find the bottom-left cell. */
struct layout_cell *
layout_find_bottomleft(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	if (lc->type == LAYOUT_LEFTRIGHT)
		lc = TAILQ_FIRST(&lc->cells);
	else
		lc = TAILQ_LAST(&lc->cells, layout_cells);
	return (layout_find_bottomleft(lc));
}

/* Find the bottom-right cell. */
struct layout_cell *
layout_find_bottomright(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	lc = TAILQ_LAST(&lc->cells, layout_cells);
	return (layout_find_bottomright(lc));
}
