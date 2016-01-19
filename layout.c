/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include "tmux.h"

/*
 * The window layout is a tree of cells each of which can be one of: a
 * left-right container for a list of cells, a top-bottom container for a list
 * of cells, or a container for a window pane.
 *
 * Each window has a pointer to the root of its layout tree (containing its
 * panes), every pane has a pointer back to the cell containing it, and each
 * cell a pointer to its parent cell.
 */

int	layout_resize_pane_grow(struct layout_cell *, enum layout_type, int);
int	layout_resize_pane_shrink(struct layout_cell *, enum layout_type, int);

struct layout_cell *
layout_create_cell(struct layout_cell *lcparent)
{
	struct layout_cell	*lc;

	lc = xmalloc(sizeof *lc);
	lc->type = LAYOUT_WINDOWPANE;
	lc->parent = lcparent;

	TAILQ_INIT(&lc->cells);

	lc->sx = UINT_MAX;
	lc->sy = UINT_MAX;

	lc->xoff = UINT_MAX;
	lc->yoff = UINT_MAX;

	lc->wp = NULL;

	return (lc);
}

void
layout_free_cell(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		while (!TAILQ_EMPTY(&lc->cells)) {
			lcchild = TAILQ_FIRST(&lc->cells);
			TAILQ_REMOVE(&lc->cells, lcchild, entry);
			layout_free_cell(lcchild);
		}
		break;
	case LAYOUT_WINDOWPANE:
		if (lc->wp != NULL)
			lc->wp->layout_cell = NULL;
		break;
	}

	free(lc);
}

void
layout_print_cell(struct layout_cell *lc, const char *hdr, u_int n)
{
	struct layout_cell	*lcchild;

	log_debug("%s:%*s%p type %u [parent %p] wp=%p [%u,%u %ux%u]", hdr, n,
	    " ", lc, lc->type, lc->parent, lc->wp, lc->xoff, lc->yoff, lc->sx,
	    lc->sy);
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

void
layout_set_size(struct layout_cell *lc, u_int sx, u_int sy, u_int xoff,
    u_int yoff)
{
	lc->sx = sx;
	lc->sy = sy;

	lc->xoff = xoff;
	lc->yoff = yoff;
}

void
layout_make_leaf(struct layout_cell *lc, struct window_pane *wp)
{
	lc->type = LAYOUT_WINDOWPANE;

	TAILQ_INIT(&lc->cells);

	wp->layout_cell = lc;
	lc->wp = wp;
}

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

/* Fix cell offsets based on their sizes. */
void
layout_fix_offsets(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 xoff, yoff;

	if (lc->type == LAYOUT_LEFTRIGHT) {
		xoff = lc->xoff;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			lcchild->xoff = xoff;
			lcchild->yoff = lc->yoff;
			if (lcchild->type != LAYOUT_WINDOWPANE)
				layout_fix_offsets(lcchild);
			xoff += lcchild->sx + 1;
		}
	} else {
		yoff = lc->yoff;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			lcchild->xoff = lc->xoff;
			lcchild->yoff = yoff;
			if (lcchild->type != LAYOUT_WINDOWPANE)
				layout_fix_offsets(lcchild);
			yoff += lcchild->sy + 1;
		}
	}
}

/* Update pane offsets and sizes based on their cells. */
void
layout_fix_panes(struct window *w, u_int wsx, u_int wsy)
{
	struct window_pane	*wp;
	struct layout_cell	*lc;
	u_int			 sx, sy;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if ((lc = wp->layout_cell) == NULL)
			continue;
		wp->xoff = lc->xoff;
		wp->yoff = lc->yoff;

		/*
		 * Layout cells are limited by the smallest size of other cells
		 * within the same row or column; if this isn't the case
		 * resizing becomes difficult.
		 *
		 * However, panes do not have to take up their entire cell, so
		 * they can be cropped to the window edge if the layout
		 * overflows and they are partly visible.
		 *
		 * This stops cells being hidden unnecessarily.
		 */

		/*
		 * Work out the horizontal size. If the pane is actually
		 * outside the window or the entire pane is already visible,
		 * don't crop.
		 */
		if (lc->xoff >= wsx || lc->xoff + lc->sx < wsx)
			sx = lc->sx;
		else {
			sx = wsx - lc->xoff;
			if (sx < 1)
				sx = lc->sx;
		}

		/*
		 * Similarly for the vertical size; the minimum vertical size
		 * is two because scroll regions cannot be one line.
		 */
		if (lc->yoff >= wsy || lc->yoff + lc->sy < wsy)
			sy = lc->sy;
		else {
			sy = wsy - lc->yoff;
			if (sy < 2)
				sy = lc->sy;
		}

		window_pane_resize(wp, sx, sy);
	}
}

/* Count the number of available cells in a layout. */
u_int
layout_count_cells(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 n;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		return (1);
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		n = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			n += layout_count_cells(lcchild);
		return (n);
	default:
		fatalx("bad layout type");
	}
}

/* Calculate how much size is available to be removed from a cell. */
u_int
layout_resize_check(struct layout_cell *lc, enum layout_type type)
{
	struct layout_cell	*lcchild;
	u_int			 available, minimum;

	if (lc->type == LAYOUT_WINDOWPANE) {
		/* Space available in this cell only. */
		if (type == LAYOUT_LEFTRIGHT)
			available = lc->sx;
		else
			available = lc->sy;

		if (available > PANE_MINIMUM)
			available -= PANE_MINIMUM;
		else
			available = 0;
	} else if (lc->type == type) {
		/* Same type: total of available space in all child cells. */
		available = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			available += layout_resize_check(lcchild, type);
	} else {
		/* Different type: minimum of available space in child cells. */
		minimum = UINT_MAX;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			available = layout_resize_check(lcchild, type);
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
layout_resize_adjust(struct layout_cell *lc, enum layout_type type, int change)
{
	struct layout_cell	*lcchild;

	/* Adjust the cell size. */
	if (type == LAYOUT_LEFTRIGHT)
		lc->sx += change;
	else
		lc->sy += change;

	/* If this is a leaf cell, that is all that is necessary. */
	if (type == LAYOUT_WINDOWPANE)
		return;

	/* Child cell runs in a different direction. */
	if (lc->type != type) {
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_resize_adjust(lcchild, type, change);
		return;
	}

	/*
	 * Child cell runs in the same direction. Adjust each child equally
	 * until no further change is possible.
	 */
	while (change != 0) {
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (change == 0)
				break;
			if (change > 0) {
				layout_resize_adjust(lcchild, type, 1);
				change--;
				continue;
			}
			if (layout_resize_check(lcchild, type) > 0) {
				layout_resize_adjust(lcchild, type, -1);
				change++;
			}
		}
	}
}

/* Destroy a cell and redistribute the space. */
void
layout_destroy_cell(struct layout_cell *lc, struct layout_cell **lcroot)
{
	struct layout_cell     *lcother, *lcparent;

	/*
	 * If no parent, this is the last pane so window close is imminent and
	 * there is no need to resize anything.
	 */
	lcparent = lc->parent;
	if (lcparent == NULL) {
		layout_free_cell(lc);
		*lcroot = NULL;
		return;
	}

	/* Merge the space into the previous or next cell. */
	if (lc == TAILQ_FIRST(&lcparent->cells))
		lcother = TAILQ_NEXT(lc, entry);
	else
		lcother = TAILQ_PREV(lc, layout_cells, entry);
	if (lcparent->type == LAYOUT_LEFTRIGHT)
		layout_resize_adjust(lcother, lcparent->type, lc->sx + 1);
	else
		layout_resize_adjust(lcother, lcparent->type, lc->sy + 1);

	/* Remove this from the parent's list. */
	TAILQ_REMOVE(&lcparent->cells, lc, entry);
	layout_free_cell(lc);

	/*
	 * If the parent now has one cell, remove the parent from the tree and
	 * replace it by that cell.
	 */
	lc = TAILQ_FIRST(&lcparent->cells);
	if (TAILQ_NEXT(lc, entry) == NULL) {
		TAILQ_REMOVE(&lcparent->cells, lc, entry);

		lc->parent = lcparent->parent;
		if (lc->parent == NULL) {
			lc->xoff = 0; lc->yoff = 0;
			*lcroot = lc;
		} else
			TAILQ_REPLACE(&lc->parent->cells, lcparent, lc, entry);

		layout_free_cell(lcparent);
	}
}

void
layout_init(struct window *w, struct window_pane *wp)
{
	struct layout_cell	*lc;

	lc = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lc, w->sx, w->sy, 0, 0);
	layout_make_leaf(lc, wp);

	layout_fix_panes(w, w->sx, w->sy);
}

void
layout_free(struct window *w)
{
	layout_free_cell(w->layout_root);
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
	xchange = sx - w->sx;
	xlimit = layout_resize_check(lc, LAYOUT_LEFTRIGHT);
	if (xchange < 0 && xchange < -xlimit)
		xchange = -xlimit;
	if (xlimit == 0) {
		if (sx <= lc->sx)	/* lc->sx is minimum possible */
			xchange = 0;
		else
			xchange = sx - lc->sx;
	}
	if (xchange != 0)
		layout_resize_adjust(lc, LAYOUT_LEFTRIGHT, xchange);

	/* Adjust vertically in a similar fashion. */
	ychange = sy - w->sy;
	ylimit = layout_resize_check(lc, LAYOUT_TOPBOTTOM);
	if (ychange < 0 && ychange < -ylimit)
		ychange = -ylimit;
	if (ylimit == 0) {
		if (sy <= lc->sy)	/* lc->sy is minimum possible */
			ychange = 0;
		else
			ychange = sy - lc->sy;
	}
	if (ychange != 0)
		layout_resize_adjust(lc, LAYOUT_TOPBOTTOM, ychange);

	/* Fix cell offsets. */
	layout_fix_offsets(lc);
	layout_fix_panes(w, sx, sy);
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
		size = lc->sx;
	else
		size = lc->sy;
	if (lc == TAILQ_LAST(&lcparent->cells, layout_cells))
		change = size - new_size;
	else
		change = new_size - size;

	/* Resize the pane. */
	layout_resize_pane(wp, type, change);
}

/* Resize a single pane within the layout. */
void
layout_resize_pane(struct window_pane *wp, enum layout_type type, int change)
{
	struct layout_cell     *lc, *lcparent;
	int			needed, size;

	lc = wp->layout_cell;

	/* Find next parent of the same type. */
	lcparent = lc->parent;
	while (lcparent != NULL && lcparent->type != type) {
		lc = lcparent;
		lcparent = lc->parent;
	}
	if (lcparent == NULL)
		return;

	/* If this is the last cell, move back one. */
	if (lc == TAILQ_LAST(&lcparent->cells, layout_cells))
		lc = TAILQ_PREV(lc, layout_cells, entry);

	/* Grow or shrink the cell. */
	needed = change;
	while (needed != 0) {
		if (change > 0) {
			size = layout_resize_pane_grow(lc, type, needed);
			needed -= size;
		} else {
			size = layout_resize_pane_shrink(lc, type, needed);
			needed += size;
		}

		if (size == 0)	/* no more change possible */
			break;
	}

	/* Fix cell offsets. */
	layout_fix_offsets(wp->window->layout_root);
	layout_fix_panes(wp->window, wp->window->sx, wp->window->sy);
	notify_window_layout_changed(wp->window);
}

/* Helper function to grow pane. */
int
layout_resize_pane_grow(struct layout_cell *lc, enum layout_type type,
    int needed)
{
	struct layout_cell	*lcadd, *lcremove;
	u_int			 size;

	/* Growing. Always add to the current cell. */
	lcadd = lc;

	/* Look towards the tail for a suitable cell for reduction. */
	lcremove = TAILQ_NEXT(lc, entry);
	while (lcremove != NULL) {
		size = layout_resize_check(lcremove, type);
		if (size > 0)
			break;
		lcremove = TAILQ_NEXT(lcremove, entry);
	}

	/* If none found, look towards the head. */
	if (lcremove == NULL) {
		lcremove = TAILQ_PREV(lc, layout_cells, entry);
		while (lcremove != NULL) {
			size = layout_resize_check(lcremove, type);
			if (size > 0)
				break;
			lcremove = TAILQ_PREV(lcremove, layout_cells, entry);
		}
		if (lcremove == NULL)
			return (0);
	}

	/* Change the cells. */
	if (size > (u_int) needed)
		size = needed;
	layout_resize_adjust(lcadd, type, size);
	layout_resize_adjust(lcremove, type, -size);
	return (size);
}

/* Helper function to shrink pane. */
int
layout_resize_pane_shrink(struct layout_cell *lc, enum layout_type type,
    int needed)
{
	struct layout_cell	*lcadd, *lcremove;
	u_int			 size;

	/* Shrinking. Find cell to remove from by walking towards head. */
	lcremove = lc;
	do {
		size = layout_resize_check(lcremove, type);
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
	layout_resize_adjust(lcadd, type, size);
	layout_resize_adjust(lcremove, type, -size);
	return (size);
}

/* Assign window pane to newly split cell. */
void
layout_assign_pane(struct layout_cell *lc, struct window_pane *wp)
{
	layout_make_leaf(lc, wp);
	layout_fix_panes(wp->window, wp->window->sx, wp->window->sy);
}

/*
 * Split a pane into two. size is a hint, or -1 for default half/half
 * split. This must be followed by layout_assign_pane before much else happens!
 **/
struct layout_cell *
layout_split_pane(struct window_pane *wp, enum layout_type type, int size,
    int insert_before)
{
	struct layout_cell     *lc, *lcparent, *lcnew, *lc1, *lc2;
	u_int			sx, sy, xoff, yoff, size1, size2;

	lc = wp->layout_cell;

	/* Copy the old cell size. */
	sx = lc->sx;
	sy = lc->sy;
	xoff = lc->xoff;
	yoff = lc->yoff;

	/* Check there is enough space for the two new panes. */
	switch (type) {
	case LAYOUT_LEFTRIGHT:
		if (sx < PANE_MINIMUM * 2 + 1)
			return (NULL);
		break;
	case LAYOUT_TOPBOTTOM:
		if (sy < PANE_MINIMUM * 2 + 1)
			return (NULL);
		break;
	default:
		fatalx("bad layout type");
	}

	if (lc->parent != NULL && lc->parent->type == type) {
		/*
		 * If the parent exists and is of the same type as the split,
		 * create a new cell and insert it after this one.
		 */

		/* Create the new child cell. */
		lcparent = lc->parent;
		lcnew = layout_create_cell(lcparent);
		if (insert_before)
			TAILQ_INSERT_BEFORE(lc, lcnew, entry);
		else
			TAILQ_INSERT_AFTER(&lcparent->cells, lc, lcnew, entry);
	} else {
		/*
		 * Otherwise create a new parent and insert it.
		 */

		/* Create and insert the replacement parent. */
		lcparent = layout_create_cell(lc->parent);
		layout_make_node(lcparent, type);
		layout_set_size(lcparent, sx, sy, xoff, yoff);
		if (lc->parent == NULL)
			wp->window->layout_root = lcparent;
		else
			TAILQ_REPLACE(&lc->parent->cells, lc, lcparent, entry);

		/* Insert the old cell. */
		lc->parent = lcparent;
		TAILQ_INSERT_HEAD(&lcparent->cells, lc, entry);

		/* Create the new child cell. */
		lcnew = layout_create_cell(lcparent);
		if (insert_before)
			TAILQ_INSERT_HEAD(&lcparent->cells, lcnew, entry);
		else
			TAILQ_INSERT_TAIL(&lcparent->cells, lcnew, entry);
	}
	if (insert_before) {
		lc1 = lcnew;
		lc2 = lc;
	} else {
		lc1 = lc;
		lc2 = lcnew;
	}

	/* Set new cell sizes.  size is the target size or -1 for middle split,
	 * size1 is the size of the top/left and size2 the bottom/right.
	 */
	switch (type) {
	case LAYOUT_LEFTRIGHT:
		if (size < 0)
			size2 = ((sx + 1) / 2) - 1;
		else if (insert_before)
			size2 = sx - size - 1;
		else
			size2 = size;
		if (size2 < PANE_MINIMUM)
			size2 = PANE_MINIMUM;
		else if (size2 > sx - 2)
			size2 = sx - 2;
		size1 = sx - 1 - size2;
		layout_set_size(lc1, size1, sy, xoff, yoff);
		layout_set_size(lc2, size2, sy, xoff + lc1->sx + 1, yoff);
		break;
	case LAYOUT_TOPBOTTOM:
		if (size < 0)
			size2 = ((sy + 1) / 2) - 1;
		else if (insert_before)
			size2 = sy - size - 1;
		else
			size2 = size;
		if (size2 < PANE_MINIMUM)
			size2 = PANE_MINIMUM;
		else if (size2 > sy - 2)
			size2 = sy - 2;
		size1 = sy - 1 - size2;
		layout_set_size(lc1, sx, size1, xoff, yoff);
		layout_set_size(lc2, sx, size2, xoff, yoff + lc1->sy + 1);
		break;
	default:
		fatalx("bad layout type");
	}

	/* Assign the panes. */
	layout_make_leaf(lc, wp);

	return (lcnew);
}

/* Destroy the cell associated with a pane. */
void
layout_close_pane(struct window_pane *wp)
{
	/* Remove the cell. */
	layout_destroy_cell(wp->layout_cell, &wp->window->layout_root);

	/* Fix pane offsets and sizes. */
	if (wp->window->layout_root != NULL) {
		layout_fix_offsets(wp->window->layout_root);
		layout_fix_panes(wp->window, wp->window->sx, wp->window->sy);
	}
	notify_window_layout_changed(wp->window);
}
