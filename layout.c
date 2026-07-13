/* $OpenBSD: layout.c,v 1.94 2026/07/13 10:03:27 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2016 Stephen Kent <smkent@smkent.net>
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
 * The window layout is a tree of cells each of which can be one of: a
 * left-right container for a list of cells, a top-bottom container for a list
 * of cells, or a container for a window pane. 'Node' will be used to refer to
 * a cell which contains a list of cells, and 'leaf' to refer to a cell that
 * contains a window pane. A leaf is considered to be 'tiled' if it is to be
 * drawn as a part of the tiled layout. A 'neighbour' is a sibling that is also
 * tiled or a node that contains a tiled leaf in a subtree. A cell's 'split'
 * size refers to the side that is shortened when splitting it, determined by
 * the parent's type.
 *
 * Each window has a pointer to the root of its layout tree (containing its
 * panes), every pane has a pointer back to the cell containing it, and each
 * cell a pointer to its parent cell. Every cell has a position in the root
 * layout tree. This position is retained through cell state changes such as
 * floating or hiding.
 */

static u_int	layout_resize_check(struct window *, struct layout_cell *,
		    enum layout_type);
static int	layout_resize_pane_grow(struct window *, struct layout_cell *,
		    enum layout_type, int, int);
static int	layout_resize_pane_shrink(struct window *, struct layout_cell *,
		    enum layout_type, int);
static u_int	layout_new_pane_size(struct window *, u_int,
		    struct layout_cell *, enum layout_type, u_int, u_int,
		    u_int);
static int	layout_set_size_check(struct window *, struct layout_cell *,
		    enum layout_type, int);
static void	layout_resize_child_cells(struct window *,
		    struct layout_cell *);

/* Initializes cell geometry to sentinel values. */
static void
layout_geometry_init(struct layout_geometry *lg)
{
	lg->sx = UINT_MAX;
	lg->sy = UINT_MAX;
	lg->xoff = INT_MAX;
	lg->yoff = INT_MAX;
}

/* Create a new layout cell. */
struct layout_cell *
layout_create_cell(struct layout_cell *lcparent)
{
	struct layout_cell	*lc;

	lc = xcalloc(1, sizeof *lc);
	lc->type = LAYOUT_WINDOWPANE;
	lc->parent = lcparent;
	TAILQ_INIT(&lc->cells);

	layout_geometry_init(&lc->g);
	layout_geometry_init(&lc->fg);

	return (lc);
}

/* Free a layout cell. */
void
layout_free_cell(struct layout_cell *lc, int only_nodes)
{
	struct layout_cell	*lcchild, *lcnext;

	if (lc == NULL || (only_nodes && lc->type == LAYOUT_WINDOWPANE))
		return;

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		lcchild = TAILQ_FIRST(&lc->cells);
		while (lcchild != NULL) {
			lcnext = TAILQ_NEXT(lcchild, entry);
			if (!only_nodes || lcchild->type != LAYOUT_WINDOWPANE) {
				TAILQ_REMOVE(&lc->cells, lcchild, entry);
				layout_free_cell(lcchild, only_nodes);
			}
			lcchild = lcnext;
		}
		break;
	case LAYOUT_WINDOWPANE:
		if (lc->wp != NULL) {
			lc->wp->layout_cell->parent = NULL;
			lc->wp->layout_cell = NULL;
		}
		break;
	}

	free(lc);
}

/* Log a cell. */
void
layout_print_cell(struct layout_cell *lc, const char *hdr, u_int n)
{
	struct layout_cell	*lcchild;
	const char		*type;

	if (lc == NULL)
		return;

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		type = "LEFTRIGHT";
		break;
	case LAYOUT_TOPBOTTOM:
		type = "TOPBOTTOM";
		break;
	case LAYOUT_WINDOWPANE:
		type = "WINDOWPANE";
		break;
	default:
		type = "UNKNOWN";
		break;
	}
	log_debug("%s:%*s%p type %s [parent %p] wp=%p [%d,%d %ux%u]", hdr, n,
	    " ", lc, type, lc->parent, lc->wp, lc->g.xoff, lc->g.yoff, lc->g.sx,
	    lc->g.sy);
	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_print_cell(lcchild, hdr, n + 1);
		break;
	case LAYOUT_WINDOWPANE:
		break;
	}
}

/* Search for a cell by the border position. */
struct layout_cell *
layout_search_by_border(struct layout_cell *lc, u_int x, u_int y)
{
	struct layout_cell	*lcchild, *last = NULL;

	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if ((int)x >= lcchild->g.xoff &&
		    (int)x < lcchild->g.xoff + (int)lcchild->g.sx &&
		    (int)y >= lcchild->g.yoff &&
		    (int)y < lcchild->g.yoff + (int)lcchild->g.sy) {
			/* Inside the cell - recurse. */
			return (layout_search_by_border(lcchild, x, y));
		}

		if (last == NULL) {
			last = lcchild;
			continue;
		}

		switch (lc->type) {
		case LAYOUT_LEFTRIGHT:
			if ((int)x < lcchild->g.xoff &&
			    (int)x >= last->g.xoff + (int)last->g.sx)
				return (last);
			break;
		case LAYOUT_TOPBOTTOM:
			if ((int)y < lcchild->g.yoff &&
			    (int)y >= last->g.yoff + (int)last->g.sy)
				return (last);
			break;
		case LAYOUT_WINDOWPANE:
			break;
		}

		last = lcchild;
	}

	return (NULL);
}

/* Set cell size. */
void
layout_set_size(struct layout_cell *lc, u_int sx, u_int sy, int xoff, int yoff)
{
	lc->g.sx = sx;
	lc->g.sy = sy;

	lc->g.xoff = xoff;
	lc->g.yoff = yoff;
}

/* Make a cell a leaf cell. */
void
layout_make_leaf(struct layout_cell *lc, struct window_pane *wp)
{
	lc->type = LAYOUT_WINDOWPANE;

	TAILQ_INIT(&lc->cells);

	wp->layout_cell = lc;
	lc->wp = wp;
}

/* Make a cell a node cell. */
void
layout_make_node(struct layout_cell *lc, enum layout_type type)
{
	if (type == LAYOUT_WINDOWPANE)
		fatalx("bad layout type");
	lc->type = type;

	TAILQ_INIT(&lc->cells);

	if (lc->wp != NULL)
		lc->wp->layout_cell = NULL;
	lc->wp = NULL;
}

/* Fix z-indexes. */
void
layout_fix_zindexes(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

	if (lc == NULL)
		return;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		TAILQ_INSERT_TAIL(&w->z_index, lc->wp, zentry);
		break;
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_fix_zindexes(w, lcchild);
		return;
	default:
		fatalx("bad layout type");
	}
}

int
layout_cell_is_tiled(struct layout_cell *lc)
{
	int	is_leaf = lc->type == LAYOUT_WINDOWPANE;
	int	is_floating = lc->flags & LAYOUT_CELL_FLOATING;

	return is_leaf && !is_floating;
}

static int
layout_cell_has_tiled_child(struct layout_cell *lc)
{
	struct layout_cell      *lcchild;

	if (lc->type == LAYOUT_WINDOWPANE)
		return (0);

	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if (layout_cell_is_tiled(lcchild) ||
		    layout_cell_has_tiled_child(lcchild))
			return (1);
	}
	return (0);
}

static int
layout_cell_is_first_tiled(struct layout_cell *lc)
{
	struct layout_cell      *lcchild, *lcparent = lc->parent;

	if (lcparent == NULL)
		return (layout_cell_is_tiled(lc));

	TAILQ_FOREACH(lcchild, &lcparent->cells, entry) {
		if (layout_cell_is_tiled(lcchild) ||
		    layout_cell_has_tiled_child(lcchild))
			break;
	}

	return (lcchild == lc);
}

static struct layout_cell *
layout_cell_get_first_tiled(struct layout_cell *lc)
{
	struct layout_cell	*lcchild, *lcchild2;

	if (layout_cell_is_tiled(lc))
		return (lc);
	if (lc->type == LAYOUT_WINDOWPANE)
		return (NULL);

	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if (layout_cell_is_tiled(lcchild))
			return (lcchild);
		if (lcchild->type != LAYOUT_WINDOWPANE) {
			lcchild2 = layout_cell_get_first_tiled(lcchild);
			if (lcchild2 != NULL)
				return (lcchild2);
		}
	}
	return (NULL);
}

/* Fix cell offsets for a child cell. */
static void
layout_fix_offsets1(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	int			 xoff, yoff;

	if (lc->type == LAYOUT_LEFTRIGHT) {
		xoff = lc->g.xoff;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			lcchild->g.xoff = xoff;
			lcchild->g.yoff = lc->g.yoff;
			if (lcchild->type != LAYOUT_WINDOWPANE)
				layout_fix_offsets1(lcchild);
			xoff += lcchild->g.sx + 1;
		}
	} else {
		yoff = lc->g.yoff;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			lcchild->g.xoff = lc->g.xoff;
			lcchild->g.yoff = yoff;
			if (lcchild->type != LAYOUT_WINDOWPANE)
				layout_fix_offsets1(lcchild);
			yoff += lcchild->g.sy + 1;
		}
	}
}

/* Update cell offsets based on their sizes. */
void
layout_fix_offsets(struct window *w)
{
	struct layout_cell	*lc = w->layout_root;

	/* Root consists of a single floating cell */
	if (lc->flags & LAYOUT_CELL_FLOATING)
		return;

	lc->g.xoff = 0;
	lc->g.yoff = 0;

	layout_fix_offsets1(lc);
}

/* Is this a top cell? */
static int
layout_cell_is_top(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*next;

	while (lc != w->layout_root) {
		next = lc->parent;
		if (next == NULL)
			return (0);
		if (next->type == LAYOUT_TOPBOTTOM &&
		    !layout_cell_is_first_tiled(lc))
			return (0);
		lc = next;
	}
	return (1);
}

/* Is this a bottom cell? */
static int
layout_cell_is_bottom(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*next, *edge;

	while (lc != w->layout_root) {
		next = lc->parent;
		if (next == NULL)
			return (0);
		if (next->type == LAYOUT_TOPBOTTOM) {
			edge = TAILQ_LAST(&next->cells, layout_cells);
			while (edge != NULL) {
				if (~edge->flags & LAYOUT_CELL_FLOATING)
					break;
				edge = TAILQ_PREV(edge, layout_cells, entry);
			}
			if (lc != edge)
				return (0);
		}
		lc = next;
	}
	return (1);
}

/*
 * Returns 1 if we need to add an extra line for the pane status line. This is
 * the case for the most upper or lower panes only.
 */
static int
layout_add_horizontal_border(struct window *w, struct layout_cell *lc,
    int status)
{
	if (status == PANE_STATUS_TOP)
		return (layout_cell_is_top(w, lc));
	if (status == PANE_STATUS_BOTTOM)
		return (layout_cell_is_bottom(w, lc));
	return (0);
}

/* Update pane offsets and sizes based on their cells. */
void
layout_fix_panes(struct window *w, struct window_pane *skip)
{
	struct window_pane	*wp;
	struct layout_cell	*lc;
	int			 status, sb_w, sb_pad;
	int			 old_xoff, old_yoff, changed = 0;
	u_int			 sx, sy, old_sx, old_sy;

	status = window_get_pane_status(w);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if ((lc = wp->layout_cell) == NULL || wp == skip)
			continue;

		old_xoff = wp->xoff;
		old_yoff = wp->yoff;
		old_sx = wp->sx;
		old_sy = wp->sy;

		wp->xoff = lc->g.xoff;
		wp->yoff = lc->g.yoff;
		sx = lc->g.sx;
		sy = lc->g.sy;

		if (!window_pane_is_floating(wp) &&
		    layout_add_horizontal_border(w, lc, status)) {
			if (status == PANE_STATUS_TOP)
				wp->yoff++;
			if (sy > 1)
				sy--;
		}

		if (window_pane_scrollbar_reserve(wp)) {
			sb_w = wp->scrollbar_style.width;
			sb_pad = wp->scrollbar_style.pad;
			if (sb_w < 1)
				sb_w = 1;
			if (sb_pad < 0)
				sb_pad = 0;
			if (w->sb_pos == PANE_SCROLLBARS_LEFT) {
				if ((int)sx - sb_w - sb_pad < PANE_MINIMUM) {
					wp->xoff = wp->xoff +
					    (int)sx - PANE_MINIMUM;
					sx = PANE_MINIMUM;
				} else {
					sx = sx - sb_w - sb_pad;
					wp->xoff = wp->xoff + sb_w + sb_pad;
				}
			} else /* sb_pos == PANE_SCROLLBARS_RIGHT */
				if ((int)sx - sb_w - sb_pad < PANE_MINIMUM)
					sx = PANE_MINIMUM;
				else
					sx = sx - sb_w - sb_pad;
			wp->flags |= PANE_REDRAWSCROLLBAR;
		}

		window_pane_resize(wp, sx, sy);

		if (wp->xoff != old_xoff ||
		    wp->yoff != old_yoff ||
		    wp->sx != old_sx ||
		    wp->sy != old_sy)
			changed = 1;
	}
	if (changed)
		redraw_invalidate_scene(w);
}

/* Count the number of available cells in a layout. */
u_int
layout_count_cells(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 count = 0;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		return (1);
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			count += layout_count_cells(lcchild);
		return (count);
	default:
		fatalx("bad layout type");
	}
}

/* Calculate how much size is available to be removed from a cell. */
static u_int
layout_resize_check(struct window *w, struct layout_cell *lc,
    enum layout_type type)
{
	struct layout_cell	*lcchild;
	struct style		*sb_style = &w->active->scrollbar_style;
	u_int			 available, minimum;
	int			 status;

	status = window_get_pane_status(w);

	if (lc->type == LAYOUT_WINDOWPANE) {
		/* Space available in this cell only. */
		if (type == LAYOUT_LEFTRIGHT) {
			available = lc->g.sx;
			if (w->sb == PANE_SCROLLBARS_ALWAYS)
				minimum = PANE_MINIMUM + sb_style->width +
				    sb_style->pad;
			else
				minimum = PANE_MINIMUM;
		} else {
			available = lc->g.sy;
			if (layout_add_horizontal_border(w, lc, status))
				minimum = PANE_MINIMUM + 1;
			else
				minimum = PANE_MINIMUM;
		}
		if (available > minimum)
			available -= minimum;
		else
			available = 0;
	} else if (lc->type == type) {
		/* Same type: total of available space in all child cells. */
		available = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			available += layout_resize_check(w, lcchild, type);
	} else {
		/* Different type: minimum of available space in child cells. */
		minimum = UINT_MAX;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			available = layout_resize_check(w, lcchild, type);
			if (available < minimum)
				minimum = available;
		}
		available = minimum;
	}

	return (available);
}

/*
 * Adjust cell size evenly, including altering its children. This function
 * expects the change to have already been bounded to the space available.
 */
void
layout_resize_adjust(struct window *w, struct layout_cell *lc,
    enum layout_type type, int change)
{
	struct layout_cell	*lcchild;
	int			 changed;

	/* Adjust the cell size. */
	if (type == LAYOUT_LEFTRIGHT)
		lc->g.sx += change;
	else
		lc->g.sy += change;

	/* If this is a leaf cell, that is all that is necessary. */
	if (type == LAYOUT_WINDOWPANE)
		return;

	/* Child cell runs in a different direction. */
	if (lc->type != type) {
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			layout_resize_adjust(w, lcchild, type, change);
		}
		return;
	}

	/*
	 * If a node doesn't contain any tiled cells, there is nothing to do.
	 */
	if (!layout_cell_has_tiled_child(lc))
		return;

	/*
	 * Child cell runs in the same direction. Adjust each child equally
	 * until no further change is possible.
	 */
	while (change != 0) {
		changed = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (change == 0)
				break;
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			if (change > 0) {
				layout_resize_adjust(w, lcchild, type, 1);
				change--;
				changed = 1;
				continue;
			}
			if (layout_resize_check(w, lcchild, type) > 0) {
				layout_resize_adjust(w, lcchild, type, -1);
				change++;
				changed = 1;
			}
		}
		if (!changed)
			break;
	}
}

/* Resizes a cell to a specified size */
void
layout_resize_set_size(struct window *w, struct layout_cell *lc,
    enum layout_type type, u_int size)
{
	int	change;

	if (type == LAYOUT_LEFTRIGHT)
		change = size - lc->g.sx;
	else
		change = size - lc->g.sy;
	layout_resize_adjust(w, lc, type, change);
}

/* Find and return the nearest neighbour to a cell in a specific direction. */
static struct layout_cell *
layout_cell_get_neighbour_direction(struct layout_cell *lc, int direction)
{
	struct layout_cell	*lcn = lc;

	while (1) {
		if (direction)
			lcn = TAILQ_NEXT(lcn, entry);
		else
			lcn = TAILQ_PREV(lcn, layout_cells, entry);

		if (lcn == NULL ||
		    layout_cell_is_tiled(lcn) ||
		    layout_cell_has_tiled_child(lcn))
			return (lcn);
	}
}

/*
 * Find and return the nearest neighbour. Prefers cells "after" the specified
 * cell. This behavior defines how cell dimensions are redistributed when a cell
 * is hidden/shown and floated/tiled.
 */
struct layout_cell *
layout_cell_get_neighbour(struct layout_cell *lc)
{
	struct layout_cell	*lcother, *lcparent = lc->parent;
	int			 direction = 1;

	if (lcparent == NULL)
		return (NULL);

	if (lc == TAILQ_LAST(&lcparent->cells, layout_cells))
		direction = !direction;

	lcother = layout_cell_get_neighbour_direction(lc, direction);
	if (lcother == NULL)
		lcother = layout_cell_get_neighbour_direction(lc, !direction);

	return (lcother);
}


/* Destroy a cell and redistribute the space. */
void
layout_destroy_cell(struct window *w, struct layout_cell *lc,
    struct layout_cell **lcroot)
{
	struct layout_cell	*lcother = NULL, *lcparent;
	int			 change;

	/* If no parent, this is the last pane in a window. */
	lcparent = lc->parent;
	if (lcparent == NULL) {
		if (lc->wp != NULL)
			*lcroot = NULL;
		layout_free_cell(lc, 0);
		return;
	}

	if (!layout_cell_is_tiled(lc)) {
		TAILQ_REMOVE(&lcparent->cells, lc, entry);
		layout_free_cell(lc, 0);
		goto out;
	}

	lcother = layout_cell_get_neighbour(lc);
	if (lcother != NULL) {
		if (lcparent->type == LAYOUT_LEFTRIGHT)
			change = lc->g.sx + 1;
		else
			change = lc->g.sy + 1;
		layout_resize_adjust(w, lcother, lcparent->type, change);
	} else
		layout_remove_tile(w, lcparent);

	/* Remove this from the parent's list. */
	TAILQ_REMOVE(&lcparent->cells, lc, entry);
	layout_free_cell(lc, 0);

out:
	/*
	 * If the parent now has one cell, remove the parent from the tree and
	 * replace it by that cell.
	 */
	lc = TAILQ_FIRST(&lcparent->cells);
	if (lc != NULL && TAILQ_NEXT(lc, entry) == NULL) {
		TAILQ_REMOVE(&lcparent->cells, lc, entry);

		lc->parent = lcparent->parent;
		if (lc->parent == NULL) {
			if (layout_cell_is_tiled(lc)) {
				lc->g.xoff = 0;
				lc->g.yoff = 0;
			}
			*lcroot = lc;
		} else
			TAILQ_REPLACE(&lc->parent->cells, lcparent, lc, entry);

		layout_free_cell(lcparent, 0);
	}
}

/* Initialize layout for pane. */
void
layout_init(struct window *w, struct window_pane *wp)
{
	struct layout_cell	*lc;

	lc = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lc, w->sx, w->sy, 0, 0);
	layout_make_leaf(lc, wp);
	layout_fix_panes(w, NULL);
}

/* Free layout for pane. */
void
layout_free(struct window *w, int only_nodes)
{
	layout_free_cell(w->layout_root, only_nodes);
}

/* Resize the entire layout after window resize. */
void
layout_resize(struct window *w, u_int sx, u_int sy)
{
	struct layout_cell	*lc = w->layout_root;
	int			 xlimit, ylimit, xchange, ychange;

	/*
	 * Adjust horizontally. Do not attempt to reduce the layout lower than
	 * the minimum (more than the amount returned by layout_resize_check).
	 *
	 * This can mean that the window size is smaller than the total layout
	 * size: redrawing this is handled at a higher level, but it does leave
	 * a problem with growing the window size here: if the current size is
	 * < the minimum, growing proportionately by adding to each pane is
	 * wrong as it would keep the layout size larger than the window size.
	 * Instead, spread the difference between the minimum and the new size
	 * out proportionately - this should leave the layout fitting the new
	 * window size.
	 */
	if (lc->type == LAYOUT_WINDOWPANE && (lc->flags & LAYOUT_CELL_FLOATING))
		return;
	xchange = sx - lc->g.sx;
	xlimit = layout_resize_check(w, lc, LAYOUT_LEFTRIGHT);
	if (xchange < 0 && xchange < -xlimit)
		xchange = -xlimit;
	if (xlimit == 0) {
		if (sx <= lc->g.sx)	/* lc->g.sx is minimum possible */
			xchange = 0;
		else
			xchange = sx - lc->g.sx;
	}
	if (xchange != 0)
		layout_resize_adjust(w, lc, LAYOUT_LEFTRIGHT, xchange);

	/* Adjust vertically in a similar fashion. */
	ychange = sy - lc->g.sy;
	ylimit = layout_resize_check(w, lc, LAYOUT_TOPBOTTOM);
	if (ychange < 0 && ychange < -ylimit)
		ychange = -ylimit;
	if (ylimit == 0) {
		if (sy <= lc->g.sy)	/* lc->g.sy is minimum possible */
			ychange = 0;
		else
			ychange = sy - lc->g.sy;
	}
	if (ychange != 0)
		layout_resize_adjust(w, lc, LAYOUT_TOPBOTTOM, ychange);

	/* Fix cell offsets. */
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
}

/* Resize a pane to an absolute size. */
void
layout_resize_pane_to(struct window_pane *wp, enum layout_type type,
    u_int new_size)
{
	struct layout_cell     *lc, *lcparent;
	int			change, size;

	lc = wp->layout_cell;

	/* Find next parent of the same type. */
	lcparent = lc->parent;
	while (lcparent != NULL && lcparent->type != type) {
		lc = lcparent;
		lcparent = lc->parent;
	}
	if (lcparent == NULL)
		return;

	/* Work out the size adjustment. */
	if (type == LAYOUT_LEFTRIGHT)
		size = lc->g.sx;
	else
		size = lc->g.sy;
	if (lc == TAILQ_LAST(&lcparent->cells, layout_cells))
		change = size - new_size;
	else
		change = new_size - size;

	/* Resize the pane. */
	layout_resize_pane(wp, type, change, 1);
}

/* Resize a floating pane to an absolute size. */
int
layout_resize_floating_pane_to(struct window_pane *wp, enum layout_type type,
    u_int size, char **cause)
{
	struct layout_cell	*lc = wp->layout_cell;

	if (~lc->flags & LAYOUT_CELL_FLOATING) {
		*cause = xstrdup("pane is not floating");
		return (-1);
	}

	if (window_pane_get_pane_lines(wp) != PANE_LINES_NONE &&
	    size >= PANE_MINIMUM + 2)
		size -= 2;
	if (size < PANE_MINIMUM || size > PANE_MAXIMUM) {
		*cause = xstrdup("size is too big or too small");
		return (-1);
	}

	if (type == LAYOUT_TOPBOTTOM) {
		if (lc->g.sy == size)
			return (0);
		lc->g.sy = size;
	} else {
		if (lc->g.sx == size)
			return (0);
		lc->g.sx = size;
	}
	redraw_invalidate_scene(wp->window);
	return (0);
}

/* Resize a floating pane relative to its current size. */
int
layout_resize_floating_pane(struct window_pane *wp, enum layout_type type,
    int change, int opposite, char **cause)
{
	struct layout_cell	*lc = wp->layout_cell;
	u_int			 size;

	if (~lc->flags & LAYOUT_CELL_FLOATING) {
		*cause = xstrdup("pane is not floating");
		return (-1);
	}
	if (change == 0)
		return (0);

	if (type == LAYOUT_TOPBOTTOM) {
		size = lc->g.sy + change;
		if (size < PANE_MINIMUM || size > PANE_MAXIMUM) {
			*cause = xstrdup("change is too big or too small");
			return (-1);
		}
		lc->g.sy = size;
		if (opposite)
			lc->g.yoff -= change;
	} else {
		size = lc->g.sx + change;
		if (size < PANE_MINIMUM || size > PANE_MAXIMUM) {
			*cause = xstrdup("change is too big or too small");
			return (-1);
		}
		lc->g.sx = size;
		if (opposite)
			lc->g.xoff -= change;
	}
	redraw_invalidate_scene(wp->window);
	return (0);
}

/* Resize a layout cell. */
void
layout_resize_layout(struct window *w, struct layout_cell *lc,
    enum layout_type type, int change, int opposite)
{
	int	needed, size;

	/* Grow or shrink the cell. */
	needed = change;
	while (needed != 0) {
		if (change > 0) {
			size = layout_resize_pane_grow(w, lc, type, needed,
			    opposite);
			needed -= size;
		} else {
			size = layout_resize_pane_shrink(w, lc, type, needed);
			needed += size;
		}

		if (size == 0)	/* no more change possible */
			break;
	}

	/* Fix cell offsets. */
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	events_fire_window("window-layout-changed", w);
}

/* Resize a single pane within the layout. */
void
layout_resize_pane(struct window_pane *wp, enum layout_type type, int change,
    int opposite)
{
	struct layout_cell	*lc = wp->layout_cell, *lcparent;

	/* Find next parent of the same type. */
	lcparent = lc->parent;
	while (lcparent != NULL && lcparent->type != type) {
		lc = lcparent;
		lcparent = lc->parent;
	}
	if (lcparent == NULL)
		return;

	/* If this is the last cell, move back one. */
	if (lc == TAILQ_LAST(&lcparent->cells, layout_cells)) {
		do
			lc = TAILQ_PREV(lc, layout_cells, entry);
		while (lc->flags & LAYOUT_CELL_FLOATING);
	}

	layout_resize_layout(wp->window, lc, type, change, opposite);
}

/* Helper function to grow pane. */
static int
layout_resize_pane_grow(struct window *w, struct layout_cell *lc,
    enum layout_type type, int needed, int opposite)
{
	struct layout_cell	*lcadd, *lcremove;
	u_int			 size = 0;

	/* Growing. Always add to the current cell. */
	lcadd = lc;

	/* Look towards the tail for a suitable cell for reduction. */
	lcremove = TAILQ_NEXT(lc, entry);
	while (lcremove != NULL) {
		size = layout_resize_check(w, lcremove, type);
		if (size > 0)
			break;
		lcremove = TAILQ_NEXT(lcremove, entry);
	}

	/* If none found, look towards the head. */
	if (opposite && lcremove == NULL) {
		lcremove = TAILQ_PREV(lc, layout_cells, entry);
		while (lcremove != NULL) {
			size = layout_resize_check(w, lcremove, type);
			if (size > 0)
				break;
			lcremove = TAILQ_PREV(lcremove, layout_cells, entry);
		}
	}
	if (lcremove == NULL)
		return (0);

	/* Change the cells. */
	if (size > (u_int) needed)
		size = needed;
	layout_resize_adjust(w, lcadd, type, size);
	layout_resize_adjust(w, lcremove, type, -size);
	return (size);
}

/* Helper function to shrink pane. */
static int
layout_resize_pane_shrink(struct window *w, struct layout_cell *lc,
    enum layout_type type, int needed)
{
	struct layout_cell	*lcadd, *lcremove;
	u_int			 size;

	/* Shrinking. Find cell to remove from by walking towards head. */
	lcremove = lc;
	do {
		size = layout_resize_check(w, lcremove, type);
		if (size != 0)
			break;
		lcremove = TAILQ_PREV(lcremove, layout_cells, entry);
	} while (lcremove != NULL);
	if (lcremove == NULL)
		return (0);

	/* And add onto the next cell (from the original cell). */
	lcadd = TAILQ_NEXT(lc, entry);
	if (lcadd == NULL)
		return (0);

	/* Change the cells. */
	if (size > (u_int) -needed)
		size = -needed;
	layout_resize_adjust(w, lcadd, type, size);
	layout_resize_adjust(w, lcremove, type, -size);
	return (size);
}

/* Assign window pane to new cell. */
void
layout_assign_pane(struct layout_cell *lc, struct window_pane *wp,
    int do_not_resize)
{
	layout_make_leaf(lc, wp);
	if (do_not_resize)
		layout_fix_panes(wp->window, wp);
	else
		layout_fix_panes(wp->window, NULL);
}

/* Calculate the new pane size for resized parent. */
static u_int
layout_new_pane_size(struct window *w, u_int previous, struct layout_cell *lc,
    enum layout_type type, u_int size, u_int count_left, u_int size_left)
{
	u_int	new_size, min, max, available;

	/* If this is the last cell, it can take all of the remaining size. */
	if (count_left == 1)
		return (size_left);

	/* How much is available in this parent? */
	available = layout_resize_check(w, lc, type);

	/*
	 * Work out the minimum size of this cell and the new size
	 * proportionate to the previous size.
	 */
	min = (PANE_MINIMUM + 1) * (count_left - 1);
	if (type == LAYOUT_LEFTRIGHT) {
		if (lc->g.sx - available > min)
			min = lc->g.sx - available;
		new_size = (lc->g.sx * size) / previous;
	} else {
		if (lc->g.sy - available > min)
			min = lc->g.sy - available;
		new_size = (lc->g.sy * size) / previous;
	}

	/* Check against the maximum and minimum size. */
	max = size_left - min;
	if (new_size > max)
		new_size = max;
	if (new_size < PANE_MINIMUM)
		new_size = PANE_MINIMUM;
	return (new_size);
}

/* Check if the cell and all its children can be resized to a specific size. */
static int
layout_set_size_check(struct window *w, struct layout_cell *lc,
    enum layout_type type, int size)
{
	struct layout_cell	*lcchild;
	u_int			 new_size, available, previous, count, idx;

	/* Cells with no children must just be bigger than minimum. */
	if (lc->type == LAYOUT_WINDOWPANE)
		return (size >= PANE_MINIMUM);
	available = size;

	/* Count number of children. */
	count = 0;
	TAILQ_FOREACH(lcchild, &lc->cells, entry)
		count++;

	/* Check new size will work for each child. */
	if (lc->type == type) {
		if (available < (count * 2) - 1)
			return (0);

		if (type == LAYOUT_LEFTRIGHT)
			previous = lc->g.sx;
		else
			previous = lc->g.sy;

		idx = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			new_size = layout_new_pane_size(w, previous, lcchild,
			    type, size, count - idx, available);
			if (idx == count - 1) {
				if (new_size > available)
					return (0);
				available -= new_size;
			} else {
				if (new_size + 1 > available)
					return (0);
				available -= new_size + 1;
			}
			if (!layout_set_size_check(w, lcchild, type, new_size))
				return (0);
			idx++;
		}
	} else {
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (lcchild->type == LAYOUT_WINDOWPANE)
				continue;
			if (!layout_set_size_check(w, lcchild, type, size))
				return (0);
		}
	}

	return (1);
}

/* Resize all child cells to fit within the current cell. */
static void
layout_resize_child_cells(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 prev, available, count, idx;

	if (lc->type == LAYOUT_WINDOWPANE)
		return;

	/* What is the current size used? */
	count = 0;
	prev = 0;
	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if (!layout_cell_is_tiled(lcchild) &&
		    !layout_cell_has_tiled_child(lcchild))
			continue;
		count++;
		if (lc->type == LAYOUT_LEFTRIGHT)
			prev += lcchild->g.sx;
		else if (lc->type == LAYOUT_TOPBOTTOM)
			prev += lcchild->g.sy;
	}
	prev += (count - 1);

	/* And how much is available? */
	available = 0;
	if (lc->type == LAYOUT_LEFTRIGHT)
		available = lc->g.sx;
	else if (lc->type == LAYOUT_TOPBOTTOM)
		available = lc->g.sy;

	/* Resize children into the new size. */
	idx = 0;
	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if (!layout_cell_is_tiled(lcchild) &&
		    !layout_cell_has_tiled_child(lcchild))
			continue;
		if (lc->type == LAYOUT_TOPBOTTOM) {
			lcchild->g.sx = lc->g.sx;
			lcchild->g.xoff = lc->g.xoff;
		} else {
			lcchild->g.sx = layout_new_pane_size(w, prev, lcchild,
			    lc->type, lc->g.sx, count - idx, available);
			available -= (lcchild->g.sx + 1);
		}
		if (lc->type == LAYOUT_LEFTRIGHT) {
			lcchild->g.sy = lc->g.sy;
			lcchild->g.yoff = lc->g.yoff;
		} else {
			lcchild->g.sy = layout_new_pane_size(w, prev, lcchild,
			    lc->type, lc->g.sy, count - idx, available);
			available -= (lcchild->g.sy + 1);
		}
		layout_resize_child_cells(w, lcchild);
		idx++;
	}
}

/*
 * Replaces the provided layout cell with a new node of the specified type and
 * inserts the cell into it. Used when creating new cells requires a different
 * layout type, or when the root layout is a window pane.
 */
struct layout_cell *
layout_replace_with_node(struct window *w, struct layout_cell *lc,
    enum layout_type type)
{
	struct layout_cell	*lcparent;

	lcparent = layout_create_cell(lc->parent);
	layout_make_node(lcparent, type);
	layout_set_size(lcparent, lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff);
	if (lc->parent == NULL)
		w->layout_root = lcparent;
	else
		TAILQ_REPLACE(&lc->parent->cells, lc, lcparent, entry);

	/* Insert the old cell. */
	lc->parent = lcparent;
	TAILQ_INSERT_HEAD(&lcparent->cells, lc, entry);

	return (lcparent);
}

/* Checks if there is enough space for two new panes. */
int
layout_split_check_space(struct window_pane *wp, struct layout_cell *lc,
   enum layout_type type)
{
	struct style	*sb_style = &wp->scrollbar_style;
	u_int		 minimum, sx = lc->g.sx, sy = lc->g.sy;
	int		 status;

	if (lc->flags & LAYOUT_CELL_FLOATING)
		fatalx("floating cells cannot be split");

	status = window_get_pane_status(wp->window);

	switch (type) {
	case LAYOUT_LEFTRIGHT:
		if (wp->window->sb == PANE_SCROLLBARS_ALWAYS) {
			minimum = PANE_MINIMUM * 2 + sb_style->width +
			    sb_style->pad;
		} else
			minimum = PANE_MINIMUM * 2 + 1;
		if (sx < minimum)
			return (0);
		break;
	case LAYOUT_TOPBOTTOM:
		if (layout_add_horizontal_border(wp->window, lc, status))
			minimum = PANE_MINIMUM * 2 + 2;
		else
			minimum = PANE_MINIMUM * 2 + 1;
		if (sy < minimum)
			return (0);
		break;
	default:
		fatalx("bad layout type");
	}

	return (1);
}

/* Calculates the new cell sizes when splitting a pane. */
void
layout_split_sizes(struct layout_cell *lc, int size, int before,
    enum layout_type type, u_int *size1, u_int *size2, u_int *saved_size)
{
	u_int	s1, s2, ss;
	u_int	sx = lc->g.sx, sy = lc->g.sy;

	if (type == LAYOUT_LEFTRIGHT)
		ss = sx;
	else
		ss = sy;
	if (size < 0)
		s2 = ((ss + 1) / 2) - 1;
	else if (before)
		s2 = ss - size - 1;
	else
		s2 = size;
	if (s2 < PANE_MINIMUM)
		s2 = PANE_MINIMUM;
	else if (s2 > ss - 2)
		s2 = ss - 2;
	s1 = ss - 1 - s2;

	*size1 = s1;
	*size2 = s2;
	*saved_size = ss;
}

/*
 * Split a pane into two. size is a hint, or -1 for default half/half
 * split. This must be followed by layout_assign_pane before much else happens!
 */
struct layout_cell *
layout_split_pane(struct window_pane *wp, enum layout_type type, int size,
    int flags)
{
	struct layout_cell	*lc, *lcparent, *lcnew, *lc1, *lc2;
	u_int			 sx, sy, xoff, yoff, size1, size2;
	u_int			 new_size, saved_size, resize_first = 0;
	int			 full_size = (flags & SPAWN_FULLSIZE);
	int			 before = (flags & SPAWN_BEFORE);

	/*
	 * If full_size is specified, add a new cell at the top of the window
	 * layout. Otherwise, split the cell for the current pane.
	 */
	if (full_size)
		lc = wp->window->layout_root;
	else
		lc = wp->layout_cell;

	/* Copy the old cell size. */
	sx = lc->g.sx;
	sy = lc->g.sy;
	xoff = lc->g.xoff;
	yoff = lc->g.yoff;

	/* Check there is enough space for the two new panes. */
	if (!layout_split_check_space(wp, lc, type))
		return (NULL);

	/*
	 * Calculate new cell sizes. size is the target size or -1 for middle
	 * split, size1 is the size of the top/left and size2 the bottom/right.
	 */
	layout_split_sizes(lc, size, before, type, &size1, &size2, &saved_size);

	/* Which size are we using? */
	if (flags & SPAWN_BEFORE)
		new_size = size2;
	else
		new_size = size1;

	/* Confirm there is enough space for full size pane. */
	if (full_size && !layout_set_size_check(wp->window, lc, type, new_size))
		return (NULL);

	if (lc->parent != NULL && lc->parent->type == type) {
		/*
		 * If the parent exists and is of the same type as the split,
		 * create a new cell and insert it after this one.
		 */
		lcparent = lc->parent;
		lcnew = layout_create_cell(lcparent);
		if (flags & SPAWN_BEFORE)
			TAILQ_INSERT_BEFORE(lc, lcnew, entry);
		else
			TAILQ_INSERT_AFTER(&lcparent->cells, lc, lcnew, entry);
	} else if (full_size && lc->parent == NULL && lc->type == type) {
		/*
		 * If the new full size pane is the same type as the root
		 * split, insert the new pane under the existing root cell
		 * instead of creating a new root cell. The existing layout
		 * must be resized before inserting the new cell.
		 */
		if (lc->type == LAYOUT_LEFTRIGHT) {
			lc->g.sx = new_size;
			layout_resize_child_cells(wp->window, lc);
			lc->g.sx = saved_size;
		} else if (lc->type == LAYOUT_TOPBOTTOM) {
			lc->g.sy = new_size;
			layout_resize_child_cells(wp->window, lc);
			lc->g.sy = saved_size;
		}
		resize_first = 1;

		/* Create the new cell. */
		lcnew = layout_create_cell(lc);
		size = saved_size - 1 - new_size;
		if (lc->type == LAYOUT_LEFTRIGHT)
			layout_set_size(lcnew, size, sy, 0, 0);
		else if (lc->type == LAYOUT_TOPBOTTOM)
			layout_set_size(lcnew, sx, size, 0, 0);
		if (flags & SPAWN_BEFORE)
			TAILQ_INSERT_HEAD(&lc->cells, lcnew, entry);
		else
			TAILQ_INSERT_TAIL(&lc->cells, lcnew, entry);
	} else {
		/*
		 * Otherwise create a new parent and insert it.
		 */

		/* Create and insert the replacement parent. */
		lcparent = layout_replace_with_node(wp->window, lc, type);

		/* Create the new child cell. */
		lcnew = layout_create_cell(lcparent);
		if (flags & SPAWN_BEFORE)
			TAILQ_INSERT_HEAD(&lcparent->cells, lcnew, entry);
		else
			TAILQ_INSERT_TAIL(&lcparent->cells, lcnew, entry);
	}
	if (flags & SPAWN_BEFORE) {
		lc1 = lcnew;
		lc2 = lc;
	} else {
		lc1 = lc;
		lc2 = lcnew;
	}

	/*
	 * Set new cell sizes. size1 is the size of the top/left and size2 the
	 * bottom/right.
	 */
	if (!resize_first && type == LAYOUT_LEFTRIGHT) {
		layout_set_size(lc1, size1, sy, xoff, yoff);
		layout_set_size(lc2, size2, sy, xoff + lc1->g.sx + 1, yoff);
	} else if (!resize_first && type == LAYOUT_TOPBOTTOM) {
		layout_set_size(lc1, sx, size1, xoff, yoff);
		layout_set_size(lc2, sx, size2, xoff, yoff + lc1->g.sy + 1);
	}
	if (full_size) {
		if (!resize_first)
			layout_resize_child_cells(wp->window, lc);
		layout_fix_offsets(wp->window);
	} else
		layout_make_leaf(lc, wp);

	return (lcnew);
}

/*
 * Creates a cell for a new floating pane. This must be followed by
 * layout_assign_pane before much else happens!
 */
struct layout_cell *
layout_floating_pane(struct window *w, struct window_pane *wp,
    struct layout_geometry *lg)
{
	struct layout_cell	*lc, *lcnew, *lcparent;

	if (wp == NULL)
		lc = w->layout_root;
	else
		lc = wp->layout_cell;
	lcparent = lc->parent;

	if (lcparent == NULL) {
		/*
		 * Adding a pane to a root that isn't a node. Must create and
		 * insert a new root.
		 */
		lcparent = layout_replace_with_node(w, lc, LAYOUT_TOPBOTTOM);
	}

	lcnew = layout_create_cell(lcparent);
	TAILQ_INSERT_AFTER(&lcparent->cells, lc, lcnew, entry);
	lcnew->flags |= LAYOUT_CELL_FLOATING;
	layout_set_size(lcnew, lg->sx, lg->sy, lg->xoff, lg->yoff);

	return (lcnew);
}

/* Destroy the cell associated with a pane. */
void
layout_close_pane(struct window_pane *wp)
{
	struct window	*w = wp->window;

	if (wp->layout_cell == NULL)
		return;

	/* Remove the cell. */
	layout_destroy_cell(w, wp->layout_cell, &w->layout_root);
	wp->layout_cell = NULL;

	/* Fix pane offsets and sizes. */
	if (w->layout_root != NULL) {
		layout_fix_offsets(w);
		layout_fix_panes(w, NULL);
	}
	events_fire_window("window-layout-changed", w);
}

/* Spread out cells inside a parent cell. */
int
layout_spread_cell(struct window *w, struct layout_cell *parent)
{
	struct layout_cell	*lc;
	u_int			 number, each, size, this, remainder;
	int			 change, changed, status;

	number = 0;
	TAILQ_FOREACH (lc, &parent->cells, entry)
		if (layout_cell_is_tiled(lc))
			number++;
	if (number <= 1)
		return (0);
	status = window_get_pane_status(w);

	if (parent->type == LAYOUT_LEFTRIGHT)
		size = parent->g.sx;
	else if (parent->type == LAYOUT_TOPBOTTOM) {
		if (layout_add_horizontal_border(w, parent, status))
			size = parent->g.sy - 1;
		else
			size = parent->g.sy;
	} else
		return (0);
	if (size < number - 1)
		return (0);
	each = (size - (number - 1)) / number;
	if (each == 0)
		return (0);

	/*
	 * Remaining space after assigning that which can be evenly
	 * distributed.
	 */
	remainder = size - (number * (each + 1)) + 1;

	changed = 0;
	TAILQ_FOREACH (lc, &parent->cells, entry) {
		if (!layout_cell_is_tiled(lc))
			continue;
		change = 0;
		if (parent->type == LAYOUT_LEFTRIGHT) {
			change = each - (int)lc->g.sx;
			if (remainder > 0) {
				change++;
				remainder--;
			}
			layout_resize_adjust(w, lc, LAYOUT_LEFTRIGHT, change);
		} else if (parent->type == LAYOUT_TOPBOTTOM) {
			if (layout_add_horizontal_border(w, lc, status))
				this = each + 1;
			else
				this = each;
			if (remainder > 0) {
				this++;
				remainder--;
			}
			change = this - (int)lc->g.sy;
			layout_resize_adjust(w, lc, LAYOUT_TOPBOTTOM, change);
		}
		if (change != 0)
			changed = 1;
	}
	return (changed);
}

/* Spread out cells evenly. */
void
layout_spread_out(struct window_pane *wp)
{
	struct layout_cell	*parent;
	struct window		*w = wp->window;

	parent = wp->layout_cell->parent;
	if (parent == NULL)
		return;

	do {
		if (layout_spread_cell(w, parent)) {
			layout_fix_offsets(w);
			layout_fix_panes(w, NULL);
			break;
		}
	} while ((parent = parent->parent) != NULL);
}

/* Get a new tiled cell. */
struct layout_cell *
layout_get_tiled_cell(struct cmdq_item *item, struct args *args,
    struct window *w, struct window_pane *wp, int flags, char **cause)
{
	struct layout_cell	*lc;
	enum layout_type	 type = LAYOUT_TOPBOTTOM;
	u_int			 curval;
	int			 size = -1;
	char			*error = NULL;

	if (window_pane_is_floating(wp)) {
		*cause = xstrdup("can't split a floating pane");
		return (NULL);
	}

	if (flags & SPAWN_HORIZONTAL)
		type = LAYOUT_LEFTRIGHT;

	if (args_has(args, 'l') || args_has(args, 'p')) {
		if (flags & SPAWN_FULLSIZE) {
			if (type == LAYOUT_TOPBOTTOM)
				curval = w->sy;
			else
				curval = w->sx;
		} else {
			if (type == LAYOUT_TOPBOTTOM)
				curval = wp->sy;
			else
				curval = wp->sx;
		}
	}

	if (args_has(args, 'l')) {
		size = args_percentage_and_expand(args, 'l', 0, INT_MAX, curval,
		    item, &error);
	} else if (args_has(args, 'p')) {
		size = args_strtonum_and_expand(args, 'p', 0, 100, item,
		    &error);
		if (error == NULL)
			size = curval * size / 100;
	}
	if (error != NULL) {
		xasprintf(cause, "invalid tiled geometry %s", error);
		free(error);
		return (NULL);
	}

	window_push_zoom(wp->window, 1, (flags & SPAWN_ZOOM));
	lc = layout_split_pane(wp, type, size, flags);
	if (lc == NULL)
		*cause = xstrdup("no space for a new pane");

	return (lc);
}

struct layout_cell *
layout_get_floating_cell(struct cmdq_item *item, struct args *args,
    enum pane_lines lines, struct window *w, struct window_pane *wp, int flags,
    char **cause)
{
	struct layout_cell	*lcnew, *lc = wp->layout_cell;
	struct layout_geometry	 fg;

	layout_geometry_init(&fg);
	if (flags & SPAWN_SPLIT) {
		if (layout_split_floating_cell(lc, w, &fg, lines, flags, cause)
		    != 0)
			return (NULL);
	} else {
		if (layout_floating_args_parse(item, args, lines, w, &fg, cause)
		    != 0)
			return (NULL);
	}

	window_push_zoom(wp->window, 1, (flags & SPAWN_ZOOM));
	lcnew = layout_floating_pane(w, wp, &fg);
	return (lcnew);
}

int
layout_floating_args_parse(struct cmdq_item *item, struct args *args,
    enum pane_lines lines, struct window *w, struct layout_geometry *lg,
    char **cause)
{
	int	 sx, sy, ox, oy;
	char	*error = NULL;

	sx = lg->sx == UINT_MAX ? w->sx / 2 : lg->sx;
	sy = lg->sy == UINT_MAX ? w->sy / 4 : lg->sy;
	ox = lg->xoff;
	oy = lg->yoff;

	if (args_has(args, 'x')) {
		sx = args_percentage_and_expand(args, 'x', 0, PANE_MAXIMUM,
		    w->sx, item, &error);
		if (error != NULL) {
			xasprintf(cause, "position %s", error);
			free(error);
			return (-1);
		}
		if (lines != PANE_LINES_NONE)
			sx -= 2;
	}
	if (args_has(args, 'y')) {
		sy = args_percentage_and_expand(args, 'y', 0, PANE_MAXIMUM,
		    w->sy, item, &error);
		if (error != NULL) {
			xasprintf(cause, "position %s", error);
			free(error);
			return (-1);
		}
		if (lines != PANE_LINES_NONE)
			sy -= 2;
	}
	if (args_has(args, 'X')) {
		ox = args_percentage_and_expand(args, 'X', -sx, w->sx,
		    w->sx, item, &error);
		if (error != NULL) {
			xasprintf(cause, "position %s", error);
			free(error);
			return (-1);
		}
	}
	if (args_has(args, 'Y')) {
		oy = args_percentage_and_expand(args, 'Y', -sy, w->sy,
		    w->sy, item, &error);
		if (error != NULL) {
			xasprintf(cause, "position %s", error);
			free(error);
			return (-1);
		}
	}

	if (ox == INT_MAX) {
		if (w->last_new_pane_x == 0)
			ox = 4;
		else {
			ox = w->last_new_pane_x + 4;
			if (w->last_new_pane_x > w->sx)
				ox = 4;
		}
		w->last_new_pane_x = ox;
	} else if (args_has(args, 'X'))
		if (lines != PANE_LINES_NONE)
			ox += 1;
	if (oy == INT_MAX) {
		if (w->last_new_pane_y == 0)
			oy = 2;
		else {
			oy = w->last_new_pane_y + 2;
			if (w->last_new_pane_y > w->sy)
				oy = 2;
		}
		w->last_new_pane_y = oy;
	} else if (args_has(args, 'Y'))
		if (lines != PANE_LINES_NONE)
			oy += 1;

	if (sx < PANE_MINIMUM || sx > PANE_MAXIMUM) {
		*cause = xstrdup("invalid width");
		return (-1);
	}
	if (sy < PANE_MINIMUM || sy > PANE_MAXIMUM) {
		*cause = xstrdup("invalid height");
		return (-1);
	}

	lg->sx = sx;
	lg->sy = sy;
	lg->xoff = ox;
	lg->yoff = oy;
	return (0);
}

int
layout_split_floating_cell(struct layout_cell *lc, struct window *w,
    struct layout_geometry *out, enum pane_lines lines, int flags,
    char **cause)
{
	struct layout_geometry	 old, new;
	int			 tborder = 1, bborder = w->sy - 1;
	int			 lborder = 3, rborder = w->sx - 3;
	int			 border = lines != PANE_LINES_NONE ? 1 : 0;
	int			 size, space;

	/* First, move the target cell in-bounds. */
	memcpy(&old, &lc->g, sizeof old);
	if (lborder > old.xoff - border)
		old.xoff = lborder + border;
	if (rborder < old.xoff + (int)old.sx + border)
		old.xoff = rborder - (int)old.sx - border;
	if (tborder > old.yoff - border)
		old.yoff = tborder + border;
	if (bborder < old.yoff + (int)old.sy + border)
		old.yoff = bborder - (int)old.sy - border;

	/* Move the new cell to its ideal position. */
	memcpy(&new, &old, sizeof new);
	if (flags & SPAWN_HORIZONTAL) {
		if (flags & SPAWN_BEFORE)
			new.xoff -= old.sx + 2 * border;
		else
			new.xoff += old.sx + 2 * border;
	} else {
		if (flags & SPAWN_BEFORE)
			new.yoff -= old.sy + 2 * border;
		else
			new.yoff += old.sy + 2 * border;
	}

	/*
	 * The position of the new cell is checked to see if it is in bounds.
	 * If it isn't, the availible space is split and equally given to both
	 * cells. Only one border is check because the target cell is in bounds
	 * already.
	 */
	if (lborder > new.xoff - border) {
		/*
		 * The space for both panes is calculated. Since the offsets are
		 * associated to where pane contents start, we remove pane
		 * borders from the space. '1' is added in case the space is
		 * odd.
		 */
		space = old.xoff + old.sx - lborder - 3 * border + 1;
		size = space / 2;
		new.sx = size;
		old.sx = size;
		new.xoff = lborder + border;
		old.xoff = new.xoff + new.sx + 2 * border;
		/*
		 * If the original space was to be odd (now even), subtract 1
		 * from the rightmost cell
		 */
		if (space % 2 == 0)
			old.sx -= 1;
	} else if (rborder < new.xoff + (int)new.sx + border) {
		space = rborder - old.xoff - 3 * border + 1;
		size = space / 2;
		new.sx = size;
		old.sx = size;
		new.xoff = old.xoff + old.sx + 2 * border;
		if (space % 2 == 0)
			new.sx -= 1;
	} else if (tborder > new.yoff - border) {
		space = old.sy + old.yoff - tborder - 3 * border + 1;
		size = space / 2;
		new.sy = size;
		old.sy = size;
		new.yoff = tborder + border;
		old.yoff = new.yoff + new.sy + 2 * border;
		if (space % 2 == 0)
			old.sy -= 1;
	} else if (bborder < new.yoff + (int)new.sy + border) {
		space = bborder - old.yoff - 3 * border + 1;
		size = space / 2;
		new.sy = size;
		old.sy = size;
		new.yoff = old.yoff + old.sy + 2 * border;
		if (space % 2 == 0)
			new.sy -= 1;
	}

	/*
	 * Expand the cell to occupy the whole availible space where it was
	 * spawned.
	 */
	if (flags & SPAWN_FULLSIZE) {
		if (flags & SPAWN_HORIZONTAL) {
			new.yoff = tborder + border;
			new.sy = bborder - tborder - 2 * border;
			if (flags & SPAWN_BEFORE) {
				new.xoff = lborder + border;
				new.sx = old.xoff - new.xoff - 2 * border;
			} else {
				new.sx = rborder - new.xoff - border;
			}
		} else {
			new.xoff = lborder + border;
			new.sx = rborder - lborder - 2 * border;
			if (flags & SPAWN_BEFORE) {
				new.yoff = tborder + border;
				new.sy = old.yoff - new.yoff - 2 * border;
			} else {
				new.sy = bborder - new.yoff - border;
			}
		}
	}

	if (new.sx < PANE_MINIMUM || new.sy < PANE_MINIMUM ||
	    old.sx < PANE_MINIMUM || old.sy < PANE_MINIMUM) {
		*cause = xstrdup("no space for a new pane");
		return (-1);
	}

	layout_set_size(lc, old.sx, old.sy, old.xoff, old.yoff);
	memcpy(out, &new, sizeof *out);
	return (0);
}

/*
 * Removes a cell from the tiled layout by giving the cell's space to the
 * nearest neighbour.
 */
int
layout_remove_tile(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*lcneighbour, *lcparent;
	enum layout_type	 type;
	int			 change;

	if (lc->flags & LAYOUT_CELL_FLOATING)
		return (-1);

	lcneighbour = layout_cell_get_neighbour(lc);
	if (lcneighbour == NULL) {
		if (lc->parent != NULL)
			layout_remove_tile(w, lc->parent);
	} else if ((lcparent = lcneighbour->parent) != NULL) {
		type = lcparent->type;
		/*
		 * Adding the size of the layout cell plus its border to the
		 * neighbour.
		 */
		if (type == LAYOUT_TOPBOTTOM)
			change = lc->g.sy + 1;
		else
			change = lc->g.sx + 1;
		layout_resize_adjust(w, lcneighbour, type, change);
	}

	/*
	 * Zeroing out the cell geometry until the cell is retiled unless this
	 * is the top level node.
	 */
	if (lc->parent != NULL)
		layout_set_size(lc, 0, 0, 0, 0);
	return (0);
}

/*
 * Inserts a cell back into the tiled layout by taking half the space from its
 * nearest neighbour.
 */
int
layout_insert_tile(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*lcneighbour, *lctiled, *lcparent;
	enum layout_type	 type;
	u_int			 size1, size2, saved_size;

	if (lc == NULL)
		fatalx("layout cell cannot be null when tiling");

	if (layout_cell_is_tiled(lc))
		return (-1);

	lcparent = lc->parent;
	if (lcparent == NULL) {
		/* Only pane in the layout. */
		layout_set_size(lc, w->sx, w->sy, 0, 0);
		return (0);
	}

	type = lcparent->type;
	lcneighbour = layout_cell_get_neighbour(lc);
	if (lcneighbour == NULL) {
		/*
		 * This will become the only visible cell in the parent.
		 * Tile the parent, then set the child's 'split' size.
		 */
		layout_insert_tile(w, lcparent);
		if (type == LAYOUT_LEFTRIGHT)
			size1 = lcparent->g.sx;
		else
			size1 = lcparent->g.sy;
		layout_resize_set_size(w, lc, type, size1);
	} else {
		/*
		 * If the neighbour is a node, a tiled child in the subtree of
		 * the neighbour is needed to check for space.
		 */
		lctiled = layout_cell_get_first_tiled(lcneighbour);
		if (!layout_split_check_space(lctiled->wp, lcneighbour, type))
			return (-1);
		layout_split_sizes(lcneighbour, -1, 0, type, &size1, &size2,
		    &saved_size);
		layout_resize_set_size(w, lc, type, size1);
		layout_resize_set_size(w, lcneighbour, type, size2);
	}

	/* Setting opposite of the 'split' size to that of the parent. */
	if (lcparent->type == LAYOUT_LEFTRIGHT) {
		size1 = lcparent->g.sy;
		type = LAYOUT_TOPBOTTOM;
	} else {
		size1 = lcparent->g.sx;
		type = LAYOUT_LEFTRIGHT;
	}
	layout_resize_set_size(w, lc, type, size1);

	return (0);
}
