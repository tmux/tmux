/* $OpenBSD$ */

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
	const char		*type;

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
	log_debug("%s:%*s%p type %s [parent %p] wp=%p [%u,%u %ux%u]", hdr, n,
	    " ", lc, type, lc->parent, lc->wp, lc->xoff, lc->yoff, lc->sx,
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

struct layout_cell *
layout_search_by_border(struct layout_cell *lc, u_int x, u_int y)
{
	struct layout_cell	*lcchild, *last = NULL;

	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if (x >= lcchild->xoff && x < lcchild->xoff + lcchild->sx &&
		    y >= lcchild->yoff && y < lcchild->yoff + lcchild->sy) {
			/* Inside the cell - recurse. */
			return (layout_search_by_border(lcchild, x, y));
		}

		if (last == NULL) {
			last = lcchild;
			continue;
		}

		switch (lc->type) {
		case LAYOUT_LEFTRIGHT:
			if (x < lcchild->xoff && x >= last->xoff + last->sx)
				return (last);
			break;
		case LAYOUT_TOPBOTTOM:
			if (y < lcchild->yoff && y >= last->yoff + last->sy)
				return (last);
			break;
		case LAYOUT_WINDOWPANE:
			break;
		}

		last = lcchild;
	}

	return (NULL);
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

/* Fix cell offsets for a child cell. */
static void
layout_fix_offsets1(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 xoff, yoff;

	if (lc->type == LAYOUT_LEFTRIGHT) {
		xoff = lc->xoff;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			lcchild->xoff = xoff;
			lcchild->yoff = lc->yoff;
			if (lcchild->type != LAYOUT_WINDOWPANE)
				layout_fix_offsets1(lcchild);
			xoff += lcchild->sx + 1;
		}
	} else {
		yoff = lc->yoff;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			lcchild->xoff = lc->xoff;
			lcchild->yoff = yoff;
			if (lcchild->type != LAYOUT_WINDOWPANE)
				layout_fix_offsets1(lcchild);
			yoff += lcchild->sy + 1;
		}
	}
}

/* Update cell offsets based on their sizes. */
void
layout_fix_offsets(struct window *w)
{
	struct layout_cell      *lc = w->layout_root;

	lc->xoff = 0;
	lc->yoff = 0;

	layout_fix_offsets1(lc);
}

/* Is this a top cell? */
static int
layout_cell_is_top(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*next;

	while (lc != w->layout_root) {
		next = lc->parent;
		if (next->type == LAYOUT_TOPBOTTOM &&
		    lc != TAILQ_FIRST(&next->cells))
			return (0);
		lc = next;
	}
	return (1);
}

/* Is this a bottom cell? */
static int
layout_cell_is_bottom(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*next;

	while (lc != w->layout_root) {
		next = lc->parent;
		if (next->type == LAYOUT_TOPBOTTOM &&
		    lc != TAILQ_LAST(&next->cells, layout_cells))
			return (0);
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
	int			 status, scrollbars, sb_pos, sb_w, sb_pad;
	u_int			 sx, sy;

	status = options_get_number(w->options, "pane-border-status");
	scrollbars = options_get_number(w->options, "pane-scrollbars");
	sb_pos = options_get_number(w->options, "pane-scrollbars-position");

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if ((lc = wp->layout_cell) == NULL || wp == skip)
			continue;

		wp->xoff = lc->xoff;
		wp->yoff = lc->yoff;
		sx = lc->sx;
		sy = lc->sy;

		if (layout_add_horizontal_border(w, lc, status)) {
			if (status == PANE_STATUS_TOP)
				wp->yoff++;
			sy--;
		}

		if (window_pane_show_scrollbar(wp, scrollbars)) {
			sb_w = wp->scrollbar_style.width;
			sb_pad = wp->scrollbar_style.pad;
			if (sb_w < 1)
				sb_w = 1;
			if (sb_pad < 0)
				sb_pad = 0;
			if (sb_pos == PANE_SCROLLBARS_LEFT) {
				if ((int)sx - sb_w < PANE_MINIMUM) {
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
	}
}

/* Count the number of available cells in a layout. */
u_int
layout_count_cells(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 count;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		return (1);
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		count = 0;
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
	int			 status, scrollbars;

	status = options_get_number(w->options, "pane-border-status");
	scrollbars = options_get_number(w->options, "pane-scrollbars");

	if (lc->type == LAYOUT_WINDOWPANE) {
		/* Space available in this cell only. */
		if (type == LAYOUT_LEFTRIGHT) {
			available = lc->sx;
			if (scrollbars)
				minimum = PANE_MINIMUM + sb_style->width +
				    sb_style->pad;
			else
				minimum = PANE_MINIMUM;
		} else {
			available = lc->sy;
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
			layout_resize_adjust(w, lcchild, type, change);
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
				layout_resize_adjust(w, lcchild, type, 1);
				change--;
				continue;
			}
			if (layout_resize_check(w, lcchild, type) > 0) {
				layout_resize_adjust(w, lcchild, type, -1);
				change++;
			}
		}
	}
}

/* Destroy a cell and redistribute the space. */
void
layout_destroy_cell(struct window *w, struct layout_cell *lc,
    struct layout_cell **lcroot)
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
	if (lcother != NULL && lcparent->type == LAYOUT_LEFTRIGHT)
		layout_resize_adjust(w, lcother, lcparent->type, lc->sx + 1);
	else if (lcother != NULL)
		layout_resize_adjust(w, lcother, lcparent->type, lc->sy + 1);

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
	layout_fix_panes(w, NULL);
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
	xchange = sx - lc->sx;
	xlimit = layout_resize_check(w, lc, LAYOUT_LEFTRIGHT);
	if (xchange < 0 && xchange < -xlimit)
		xchange = -xlimit;
	if (xlimit == 0) {
		if (sx <= lc->sx)	/* lc->sx is minimum possible */
			xchange = 0;
		else
			xchange = sx - lc->sx;
	}
	if (xchange != 0)
		layout_resize_adjust(w, lc, LAYOUT_LEFTRIGHT, xchange);

	/* Adjust vertically in a similar fashion. */
	ychange = sy - lc->sy;
	ylimit = layout_resize_check(w, lc, LAYOUT_TOPBOTTOM);
	if (ychange < 0 && ychange < -ylimit)
		ychange = -ylimit;
	if (ylimit == 0) {
		if (sy <= lc->sy)	/* lc->sy is minimum possible */
			ychange = 0;
		else
			ychange = sy - lc->sy;
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
		size = lc->sx;
	else
		size = lc->sy;
	if (lc == TAILQ_LAST(&lcparent->cells, layout_cells))
		change = size - new_size;
	else
		change = new_size - size;

	/* Resize the pane. */
	layout_resize_pane(wp, type, change, 1);
}

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
	notify_window("window-layout-changed", w);
}

/* Resize a single pane within the layout. */
void
layout_resize_pane(struct window_pane *wp, enum layout_type type, int change,
    int opposite)
{
	struct layout_cell	*lc, *lcparent;

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

/* Assign window pane to newly split cell. */
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
		if (lc->sx - available > min)
			min = lc->sx - available;
		new_size = (lc->sx * size) / previous;
	} else {
		if (lc->sy - available > min)
			min = lc->sy - available;
		new_size = (lc->sy * size) / previous;
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
			previous = lc->sx;
		else
			previous = lc->sy;

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
	u_int			 previous, available, count, idx;

	if (lc->type == LAYOUT_WINDOWPANE)
		return;

	/* What is the current size used? */
	count = 0;
	previous = 0;
	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		count++;
		if (lc->type == LAYOUT_LEFTRIGHT)
			previous += lcchild->sx;
		else if (lc->type == LAYOUT_TOPBOTTOM)
			previous += lcchild->sy;
	}
	previous += (count - 1);

	/* And how much is available? */
	available = 0;
	if (lc->type == LAYOUT_LEFTRIGHT)
		available = lc->sx;
	else if (lc->type == LAYOUT_TOPBOTTOM)
		available = lc->sy;

	/* Resize children into the new size. */
	idx = 0;
	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if (lc->type == LAYOUT_TOPBOTTOM) {
			lcchild->sx = lc->sx;
			lcchild->xoff = lc->xoff;
		} else {
			lcchild->sx = layout_new_pane_size(w, previous, lcchild,
			    lc->type, lc->sx, count - idx, available);
			available -= (lcchild->sx + 1);
		}
		if (lc->type == LAYOUT_LEFTRIGHT)
			lcchild->sy = lc->sy;
		else {
			lcchild->sy = layout_new_pane_size(w, previous, lcchild,
			    lc->type, lc->sy, count - idx, available);
			available -= (lcchild->sy + 1);
		}
		layout_resize_child_cells(w, lcchild);
		idx++;
	}
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
	struct style		*sb_style = &wp->scrollbar_style;
	u_int			 sx, sy, xoff, yoff, size1, size2, minimum;
	u_int			 new_size, saved_size, resize_first = 0;
	int			 full_size = (flags & SPAWN_FULLSIZE), status;
	int			 scrollbars;

	/*
	 * If full_size is specified, add a new cell at the top of the window
	 * layout. Otherwise, split the cell for the current pane.
	 */
	if (full_size)
		lc = wp->window->layout_root;
	else
		lc = wp->layout_cell;
	status = options_get_number(wp->window->options, "pane-border-status");
	scrollbars = options_get_number(wp->window->options, "pane-scrollbars");

	/* Copy the old cell size. */
	sx = lc->sx;
	sy = lc->sy;
	xoff = lc->xoff;
	yoff = lc->yoff;

	/* Check there is enough space for the two new panes. */
	switch (type) {
	case LAYOUT_LEFTRIGHT:
		if (scrollbars) {
			minimum = PANE_MINIMUM * 2 + sb_style->width +
			    sb_style->pad;
		} else
			minimum = PANE_MINIMUM * 2 + 1;
		if (sx < minimum)
			return (NULL);
		break;
	case LAYOUT_TOPBOTTOM:
		if (layout_add_horizontal_border(wp->window, lc, status))
			minimum = PANE_MINIMUM * 2 + 2;
		else
			minimum = PANE_MINIMUM * 2 + 1;
		if (sy < minimum)
			return (NULL);
		break;
	default:
		fatalx("bad layout type");
	}

	/*
	 * Calculate new cell sizes. size is the target size or -1 for middle
	 * split, size1 is the size of the top/left and size2 the bottom/right.
	 */
	if (type == LAYOUT_LEFTRIGHT)
		saved_size = sx;
	else
		saved_size = sy;
	if (size < 0)
		size2 = ((saved_size + 1) / 2) - 1;
	else if (flags & SPAWN_BEFORE)
		size2 = saved_size - size - 1;
	else
		size2 = size;
	if (size2 < PANE_MINIMUM)
		size2 = PANE_MINIMUM;
	else if (size2 > saved_size - 2)
		size2 = saved_size - 2;
	size1 = saved_size - 1 - size2;

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
			lc->sx = new_size;
			layout_resize_child_cells(wp->window, lc);
			lc->sx = saved_size;
		} else if (lc->type == LAYOUT_TOPBOTTOM) {
			lc->sy = new_size;
			layout_resize_child_cells(wp->window, lc);
			lc->sy = saved_size;
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
		layout_set_size(lc2, size2, sy, xoff + lc1->sx + 1, yoff);
	} else if (!resize_first && type == LAYOUT_TOPBOTTOM) {
		layout_set_size(lc1, sx, size1, xoff, yoff);
		layout_set_size(lc2, sx, size2, xoff, yoff + lc1->sy + 1);
	}
	if (full_size) {
		if (!resize_first)
			layout_resize_child_cells(wp->window, lc);
		layout_fix_offsets(wp->window);
	} else
		layout_make_leaf(lc, wp);

	return (lcnew);
}

/* Destroy the cell associated with a pane. */
void
layout_close_pane(struct window_pane *wp)
{
	struct window	*w = wp->window;

	/* Remove the cell. */
	layout_destroy_cell(w, wp->layout_cell, &w->layout_root);

	/* Fix pane offsets and sizes. */
	if (w->layout_root != NULL) {
		layout_fix_offsets(w);
		layout_fix_panes(w, NULL);
	}
	notify_window("window-layout-changed", w);
}

int
layout_spread_cell(struct window *w, struct layout_cell *parent)
{
	struct layout_cell	*lc;
	struct style		*sb_style = &w->active->scrollbar_style;
	u_int			 number, each, size, this, remainder;
	int			 change, changed, status, scrollbars;

	number = 0;
	TAILQ_FOREACH (lc, &parent->cells, entry)
		number++;
	if (number <= 1)
		return (0);
	status = options_get_number(w->options, "pane-border-status");
	scrollbars = options_get_number(w->options, "pane-scrollbars");

	if (parent->type == LAYOUT_LEFTRIGHT) {
		if (scrollbars)
			size = parent->sx - sb_style->width + sb_style->pad;
		else
			size = parent->sx;
	}
	else if (parent->type == LAYOUT_TOPBOTTOM) {
		if (layout_add_horizontal_border(w, parent, status))
			size = parent->sy - 1;
		else
			size = parent->sy;
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
		change = 0;
		if (parent->type == LAYOUT_LEFTRIGHT) {
			change = each - (int)lc->sx;
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
			change = this - (int)lc->sy;
			layout_resize_adjust(w, lc, LAYOUT_TOPBOTTOM, change);
		}
		if (change != 0)
			changed = 1;
	}
	return (changed);
}

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
