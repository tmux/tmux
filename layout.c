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
static int	layout_need_status(struct layout_cell *, int);
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

/*
 * Returns 1 if we need to reserve space for the pane status line. This is the
 * case for the most upper panes only.
 */
static int
layout_need_status(struct layout_cell *lc, int at_top)
{
	struct layout_cell	*first_lc;

	if (lc->parent) {
		if (lc->parent->type == LAYOUT_LEFTRIGHT)
			return (layout_need_status(lc->parent, at_top));

		if (at_top)
			first_lc = TAILQ_FIRST(&lc->parent->cells);
		else
			first_lc = TAILQ_LAST(&lc->parent->cells,layout_cells);
		if (lc == first_lc)
			return (layout_need_status(lc->parent, at_top));
		return (0);
	}
	return (1);
}

/* Update pane offsets and sizes based on their cells. */
void
layout_fix_panes(struct window *w, u_int wsx, u_int wsy)
{
	struct window_pane	*wp;
	struct layout_cell	*lc;
	u_int			 sx, sy;
	int			 shift, status, at_top;

	status = options_get_number(w->options, "pane-border-status");
	at_top = (status == 1);
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if ((lc = wp->layout_cell) == NULL)
			continue;

		if (status != 0)
			shift = layout_need_status(lc, at_top);
		else
			shift = 0;

		wp->xoff = lc->xoff;
		wp->yoff = lc->yoff;

		if (shift && at_top)
			wp->yoff += 1;

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

		if (shift)
			sy -= 1;

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
	u_int			 available, minimum;

	if (lc->type == LAYOUT_WINDOWPANE) {
		/* Space available in this cell only. */
		minimum = PANE_MINIMUM;
		if (type == LAYOUT_LEFTRIGHT)
			available = lc->sx;
		else {
			available = lc->sy;
			minimum += layout_need_status(lc,
			    options_get_number(w->options,
			    "pane-border-status") == 1);
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
	if (lcparent->type == LAYOUT_LEFTRIGHT)
		layout_resize_adjust(w, lcother, lcparent->type, lc->sx + 1);
	else
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
	ychange = sy - w->sy;
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
	layout_fix_offsets(w->layout_root);
	layout_fix_panes(w, w->sx, w->sy);
	notify_window("window-layout-changed", w);
}

struct layout_map {
	u_int sx;
	u_int sy;
	struct layout_cell **screen_map; /* [sx][sy]; */
	struct layout_cell **cells_array;
	u_int cells_array_len;
	u_int cells_len;
};

#define LAYOUT_MAP_CELL(_x_, _y_) map->screen_map[(_y_) * map->sx + (_x_)]

/*
 * Build a screen map of the current layout from the windowpane leafs. The
 * borders will be NULL pointers.
 */
static void
layout_map_build(struct layout_map *map, struct layout_cell *lc)
{
	struct layout_cell	*lcchild, *lc_copy;
	struct layout_cell	**cell;
	u_int			x, y;

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_map_build(map, lcchild);
		break;

	case LAYOUT_WINDOWPANE:
		lc_copy = layout_create_cell(NULL);
		lc_copy->sx = lc->sx;
		lc_copy->sy = lc->sy;
		lc_copy->xoff = lc->xoff;
		lc_copy->yoff = lc->yoff;
		lc_copy->wp = lc->wp; /* One-sided, until the entire
					 layout works out */

		for (x = 0; x < lc->sx; x++)
			for (y = 0; y < lc->sy; y++) {
				cell = &LAYOUT_MAP_CELL(x + lc->xoff,
							y + lc->yoff);
				*cell = lc_copy;
			}

		map->cells_array =
			xreallocarray(map->cells_array,
				      sizeof(struct layout_cell *),
				      map->cells_len + 1);
		map->cells_array[map->cells_len] = lc_copy;
		map->cells_len += 1;

		break;
	}
}

static void
layout_map_init(struct window *w, struct layout_map *map)
{
	map->sx = w->layout_root->sx;
	map->sy = w->layout_root->sy;
	map->screen_map = xcalloc(map->sx * map->sy, sizeof(struct layout_cell *));
	map->cells_len = 0;
	map->cells_array = NULL;

	layout_map_build(map, w->layout_root);
}

/*
 * Free all non-leafs.
 */
static void
layout_clear_non_leafs(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		while (!TAILQ_EMPTY(&lc->cells)) {
			lcchild = TAILQ_FIRST(&lc->cells);
			lcchild->parent = NULL;
			TAILQ_REMOVE(&lc->cells, lcchild, entry);

			layout_clear_non_leafs(lcchild);
		}

		layout_free_cell(lc);
		break;
	case LAYOUT_WINDOWPANE:
		break;
	}
}

/*
 * Shift the window to the new layout given by the layout map.
 */
static void
layout_map_use(struct window *w, struct layout_map *map, struct layout_cell *lc)
{
	u_int			i;
	struct layout_cell	*lc_leaf;
	struct window_pane      *wp;

	/* Clear all non-leafs of the old layout */
	layout_clear_non_leafs(w->layout_root);
	w->layout_root = NULL;

	/* Set all window panes to use the new layout, and free the old leafs */
	for (i = 0; i < map->cells_len; i++) {
		lc_leaf = map->cells_array[i];
		wp = lc_leaf->wp;
		layout_free_cell(wp->layout_cell);
		wp->layout_cell = lc_leaf;
	}

	/* Use the new leayout */
	w->layout_root = lc;

	/* Prevent freeing the new leafs */
	map->cells_len = 0;
}

static void
layout_map_free(struct layout_map *map)
{
	u_int i;

	for (i = 0; i < map->cells_len; i++) {
		map->cells_array[i]->wp = NULL;
		layout_free_cell(map->cells_array[i]);
	}

	free(map->screen_map);
	free(map->cells_array);
}

static int
layout_rect_expand(const struct layout_cell *lc, u_int *bx, u_int *by, u_int
	*ex, u_int *ey, u_int sx, u_int sy)
{
	u_int			expanse = 0;
	u_int                   b_xoff_border = lc->xoff;
	u_int                   b_yoff_border = lc->yoff;
	u_int                   e_xoff_border = lc->xoff + lc->sx;
	u_int                   e_yoff_border = lc->yoff + lc->sy;

	if (b_xoff_border > 0)
		b_xoff_border--;
	if (b_yoff_border > 0)
		b_yoff_border--;
	if (e_xoff_border < sx - 1)
		e_xoff_border++;
	if (e_yoff_border < sy - 1)
		e_yoff_border++;

	if (*ex == *bx && *ey == *by) {
		/* First cell determines the bounding rect: */

		*bx = b_xoff_border;
		*by = b_yoff_border;
		*ex = e_xoff_border;
		*ey = e_yoff_border;
		return (1);
	}

	/* An additional cell expands the bounding rect: */

	if (b_xoff_border < *bx) {
		*bx = b_xoff_border;
		expanse++;
	}

	if (b_yoff_border < *by) {
		*by = b_yoff_border;
		expanse++;
	}

	if (*ex < e_xoff_border) {
		*ex = e_xoff_border;
		expanse++;
	}

	if (*ey < e_yoff_border) {
		*ey = e_yoff_border;
		expanse++;
	}

	return (expanse);
}

static struct layout_cell *
layout_join_splits(struct layout_cell *lc1, struct layout_cell *lc2, u_int
	layout_type, u_int bx, u_int by, u_int ex, u_int ey)
{
	struct layout_cell *lc = NULL, *lcchild;

	if (lc1->type == layout_type) {
		if (lc2->type == layout_type) {
			log_debug("layout: combining %p and %p",
				  lc1, lc2);
			TAILQ_FOREACH(lcchild, &lc2->cells, entry) {
				lcchild->parent = lc1;
			}
			TAILQ_CONCAT(&lc1->cells, &lc2->cells, entry);
			layout_free_cell(lc2);
		} else {
			log_debug("layout: inserting tail %p into %p",
				  lc2, lc1);
			TAILQ_INSERT_TAIL(&lc1->cells, lc2, entry);
			lc2->parent = lc1;
		}
		lc = lc1;
	} else {
		if (lc2->type == layout_type) {
			log_debug("layout: inserting head %p into %p",
				  lc1, lc2);
			TAILQ_INSERT_HEAD(&lc2->cells, lc1, entry);
			lc1->parent = lc2;
			lc = lc2;
		} else {
			lc = layout_create_cell(NULL);
			log_debug("layout: creating cell: %p", lc);
			lc->type = layout_type;

			TAILQ_INSERT_TAIL(&lc->cells, lc1, entry);
			lc1->parent = lc;
			TAILQ_INSERT_TAIL(&lc->cells, lc2, entry);
			lc2->parent = lc;
		}
	}

	lc->xoff = bx;
	lc->yoff = by;
	lc->sx = ex - bx;
	lc->sy = ey - by;

	return (lc);
}

static struct layout_cell *
layout_recreate_from_map(u_int sx, uint sy, struct layout_map *map,
	struct layout_cell *starting_cell, u_int bx, u_int by, u_int ex,
	u_int ey, u_int nesting, u_int *failures)
{
	struct layout_cell	*lc, *joining_lc;
	u_int			dbx, dby, dex, dey, x, y, is_border, save, joins = 0;

	log_debug("%s[%d]: b:%d,%d e:%d,%d", __func__, nesting, bx, by, ex, ey);

	if (!starting_cell) {
		starting_cell = LAYOUT_MAP_CELL(bx, by);
		if (!starting_cell)
			fatal("expected some cell at (%d, %d)", by, by);
	}

	lc = starting_cell;
	dbx = lc->xoff;
	dby = lc->yoff;
	dex = lc->xoff + lc->sx;
	dey = lc->yoff + lc->sy;

	log_debug("%s[%d]: db:%d,%d de:%d,%d", __func__, nesting, dbx, dby, dex, dey);

	do {
		if (dbx == bx && dby == by && dex == ex && dey == ey) {
			log_debug("%s[%d]: bounding rect reached: "
				  "db:%d,%d de:%d,%d", __func__, nesting, dbx, dby, dex, dey);
			/* Reached bounding rect */
			return (lc);
		}

		joins = 0;
		if (dbx > bx) {
			log_debug("%s[%d]: seeking left at dbx=%d",
				  __func__, nesting, dbx);
			save = dbx;
			dbx--;

			do {
				/* Is there a new border? */
				is_border = 1;
				if (dbx != bx) {
					/* Track along the continuation of the border */
					if (dby != by && LAYOUT_MAP_CELL(dbx - 1, dby - 1))
						break;

					if (dey != ey && LAYOUT_MAP_CELL(dbx - 1, dey))
						break;

					for (y = dby; y < dey; y++)
						if (LAYOUT_MAP_CELL(dbx - 1, y)) {
							is_border = 0;
							break;
						}
				}

				if (is_border) {
					log_debug("%s[%d]: joining at dbx=%d, save=%d",
						  __func__, nesting, dbx, save);

					joining_lc =
						layout_recreate_from_map(sx, sy, map,
						      NULL, dbx, dby, save - 1, dey,
						      nesting + 1, failures);
					lc = layout_join_splits(joining_lc, lc,
						LAYOUT_LEFTRIGHT, dbx, dby, dex, dey);
					if (*failures)
						return (lc);
					save = dbx;
					joins++;
				}

				if (dbx == bx)
					break;

				dbx--;
			} while (1);

			dbx = save;

			log_debug("%s[%d]: done seeking left at dbx=%d", __func__, nesting, dbx);
		}

		if (dex < ex) {
			log_debug("%s[%d]: seeking right at dex=%d", __func__, nesting, dex);
			dex++;
			save = dex;

			do {
				/* Is there a new border? */
				is_border = 1;
				if (dex != ex) {
					/* Track along the continuation of the border */
					if (dby != by && LAYOUT_MAP_CELL(dex, dby - 1))
						break;

					if (dey != ey && LAYOUT_MAP_CELL(dex, dey))
						break;

					for (y = dby; y < dey; y++)
						if (LAYOUT_MAP_CELL(dex, y)) {
							is_border = 0;
							break;
						}
				}

				if (is_border) {
					log_debug("%s[%d]:   joining at dex=%d, save=%d",
						  __func__, nesting, dex, save);

					joining_lc =
						layout_recreate_from_map(sx, sy, map,
						      NULL, save, dby, dex, dey,
						      nesting + 1, failures);
					lc = layout_join_splits(lc, joining_lc,
						LAYOUT_LEFTRIGHT, dbx, dby, dex, dey);
					if (*failures)
						return (lc);
					save = dex + 1;
					joins++;
				}

				if (dex == ex)
					break;

				dex++;
			} while (1);

			dex = save - 1;

			log_debug("%s[%d]: done seeking right at dex=%d",
				  __func__, nesting, dex);
		}

		if (dby > by) {
			log_debug("%s[%d]: seeking up at dby=%d",
				  __func__, nesting, dby);
			save = dby;
			dby--;

			do {
				/* Is there a new border? */
				is_border = 1;
				if (dby != by) {
					/* Track along the continuation of the border */
					if (dbx != bx && LAYOUT_MAP_CELL(dbx - 1, dby - 1))
						break;

					if (dex != ex && LAYOUT_MAP_CELL(dex, dby - 1))
						break;

					for (x = dbx; x < dex; x++)
						if (LAYOUT_MAP_CELL(x, dby - 1)) {
							is_border = 0;
							break;
						}
				}

				if (is_border) {
					log_debug("%s[%d]: joining at dby=%d, save=%d",
						  __func__, nesting, dby, save);

					joining_lc =
						layout_recreate_from_map(sx, sy, map,
						      NULL, dbx, dby, dex, save - 1,
						      nesting + 1, failures);
					lc = layout_join_splits(joining_lc, lc,
						LAYOUT_TOPBOTTOM, dbx, dby, dex, dey);
					if (*failures)
						return (lc);
					save = dby;
					joins++;
				}

				if (dby == by)
					break;

				dby--;
			} while (1);

			dby = save;

			log_debug("%s[%d]: done seeking up at dby=%d", __func__, nesting, dby);
		}

		if (dey < ey) {
			log_debug("%s[%d]: seeking down at dey=%d", __func__, nesting, dey);
			dey++;
			save = dey;

			do {
				/* Is there a new border? */
				is_border = 1;
				if (dey != ey) {
					/* Track along the continuation of the border */
					if (dbx != bx && LAYOUT_MAP_CELL(dbx - 1, dey))
						break;

					if (dex != ex && LAYOUT_MAP_CELL(dex, dey))
						break;

					for (x = dbx; x < dex; x++)
						if (LAYOUT_MAP_CELL(x, dey)) {
							is_border = 0;
							break;
						}
				}

				if (is_border) {
					log_debug("%s[%d]:   joining at dey=%d, save=%d",
						  __func__, nesting, dey, save);

					joining_lc =
						layout_recreate_from_map(sx, sy, map,
						      NULL, dbx, save, dex, dey,
						      nesting + 1, failures);
					lc = layout_join_splits(lc, joining_lc,
						LAYOUT_TOPBOTTOM, dbx, dby, dex, dey);
					if (*failures)
						return (lc);
					save = dey + 1;
					joins++;
				}

				if (dey == ey)
					break;

				dey++;
			} while (1);

			dey = save - 1;

			log_debug("%s[%d]: done seeking down at dey=%d",
				  __func__, nesting, dey);

		}

		if (joins == 0) {
			log_debug("no progress in %s", __func__);
			*failures += 1;
			return (lc);
		}
	} while (1);
}

static int
layout_border_bounding_rect(struct window *w, struct layout_map
	*map, u_int x, u_int y, u_int *vertical, u_int *horizontal,
	uint *o_bx, uint *o_by, uint *o_ex, uint *o_ey)
{
	u_int			sx = w->layout_root->sx;
	u_int			sy = w->layout_root->sy;
	struct layout_cell	*lc;
	u_int			bx = 0, by = 0, fx, fy;
	u_int			ex = 0, ey = 0, ix, iy;
	u_int			tx, ty, expanses;

	/*
	 * Determine initial bounding rect from the specified border, by
	 * enclosing it in the cells that are adjacent to (x, y).
	 */

	*horizontal = ((x == 0 || !LAYOUT_MAP_CELL(x - 1, y)) &&
		       (x == sx - 1 || !LAYOUT_MAP_CELL(x + 1, y)));
	*vertical = ((y == 0 || !LAYOUT_MAP_CELL(x, y - 1)) &&
		     (y == sy - 1 || !LAYOUT_MAP_CELL(x, y + 1)));

	for (ix = 0; ix < 3; ix++) {
		if (x == 0 && ix == 0)
			continue;
		if (x == sx - 1 && ix == 2)
			continue;
		for (iy = 0; iy < 3; iy++) {
			if (y == 0 && iy == 0)
				continue;
			if (y == sy - 1 && iy == 2)
				continue;
			if (ix == 1 && iy == 1)
				continue;

			fx = x + ix - 1;
			fy = y + iy - 1;

			lc = LAYOUT_MAP_CELL(fx, fy);
			if (!lc)
				continue;

			layout_rect_expand(lc, &bx, &by, &ex, &ey, sx, sy);
		}
	}

	if (ex == bx && ey == by)
		return (0);

	/*
	 * Expand bounding rect from cells that it contains until we no longer
	 * need can.
	 */
	do {
		expanses = 0;

		for (tx = bx; tx < ex; tx++) {
			for (ty = by; ty < ey; ty++) {
				lc = LAYOUT_MAP_CELL(tx, ty);
				if (!lc)
					continue;

				expanses +=
					layout_rect_expand(lc, &bx, &by,
							   &ex, &ey, sx, sy);
			}
		}
	} while (expanses > 0);

	if (bx > 0)
		bx++;
	if (by > 0)
		by++;
	if (ex < sx - 1)
		ex--;
	if (ey < sy - 1)
		ey--;

	log_debug("%s: bound rect: x=%d:%d, y=%d:%d",__func__,
		  bx, ex, by, ey);

	*o_ex = ex;
	*o_ey = ey;
	*o_bx = bx;
	*o_by = by;

	return (1);
}

/*
 * Perform a re-organization of the layout so that at the border given by 'x'
 * and 'y' will yield the smallest amount of panes changing their size too, on
 * most cases.
 */
int
layout_resplit(struct window *w, u_int x, u_int y)
{
	u_int			sx = w->layout_root->sx;
	u_int			sy = w->layout_root->sy;
	u_int                   failures = 0;
	struct layout_map	map;
	struct layout_cell	*lc1, *lc2, *lc = NULL;
	u_int 			vertical, horizontal, bx, by, ex, ey;

	layout_map_init(w, &map);

	if (!layout_border_bounding_rect(w, &map, x, y, &vertical, &horizontal,
					 &bx, &by, &ex, &ey))
		return 0;

	if (!vertical && horizontal) {
		log_debug("%s: inner horizontal reconstruction", __func__);

		lc1 = layout_recreate_from_map(sx, sy, &map, NULL,
					       bx, by, ex, y, 0, &failures);
		lc2 = layout_recreate_from_map(sx, sy, &map, NULL,
					       bx, y+1, ex, ey, 0, &failures);

		lc = layout_join_splits(lc1, lc2, LAYOUT_TOPBOTTOM,
				bx, by, ex, ey);
	} else if (vertical && !horizontal) {
		log_debug("%s: inner vertical reconstruction", __func__);

		lc1 = layout_recreate_from_map(sx, sy, &map, NULL,
					       bx, by, x, ey, 0, &failures);
		lc2 = layout_recreate_from_map(sx, sy, &map, NULL,
					       x+1, by, ex, ey, 0, &failures);

		lc = layout_join_splits(lc1, lc2, LAYOUT_LEFTRIGHT,
				bx, by, ex, ey);
	} else {
		log_debug("%s: vertical = %d, horizontal = %d",
			  __func__, vertical, horizontal);
		failures = 1;
	}

	if (lc && failures == 0) {
		log_debug("%s: outer reconstruction", __func__);

		lc = layout_recreate_from_map(sx, sy, &map, lc, 0, 0, sx, sy, 0,
					      &failures);
		if (failures == 0) {
			layout_print_cell(lc, "layout", 1);
			layout_map_use(w, &map, lc);
			lc = NULL;
		}
	}

	if (lc)
		layout_clear_non_leafs(lc);

	layout_map_free(&map);
	return (0);
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
layout_assign_pane(struct layout_cell *lc, struct window_pane *wp)
{
	layout_make_leaf(lc, wp);
	layout_fix_panes(wp->window, wp->window->sx, wp->window->sy);
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
	u_int			new_size, available, previous, count, idx;

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
		if (type == LAYOUT_LEFTRIGHT)
			previous = lc->sx;
		else
			previous = lc->sy;

		idx = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			new_size = layout_new_pane_size(w, previous, lcchild,
			    type, size, count - idx, available);
			if (new_size > available)
				return (0);

			available -= (new_size + 1);
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
    int insert_before, int full_size)
{
	struct layout_cell     *lc, *lcparent, *lcnew, *lc1, *lc2;
	u_int			sx, sy, xoff, yoff, size1, size2;
	u_int			new_size, saved_size, resize_first = 0;

	/*
	 * If full_size is specified, add a new cell at the top of the window
	 * layout. Otherwise, split the cell for the current pane.
	 */
	if (full_size)
		lc = wp->window->layout_root;
	else
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
	else if (insert_before)
		size2 = saved_size - size - 1;
	else
		size2 = size;
	if (size2 < PANE_MINIMUM)
		size2 = PANE_MINIMUM;
	else if (size2 > saved_size - 2)
		size2 = saved_size - 2;
	size1 = saved_size - 1 - size2;

	/* Which size are we using? */
	if (insert_before)
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
		if (insert_before)
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
		if (insert_before)
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
		layout_fix_offsets(wp->window->layout_root);
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
		layout_fix_offsets(w->layout_root);
		layout_fix_panes(w, w->sx, w->sy);
	}
	notify_window("window-layout-changed", w);
}

int
layout_spread_cell(struct window *w, struct layout_cell *parent)
{
	struct layout_cell	*lc;
	u_int			 number, each, size;
	int			 change, changed;

	number = 0;
	TAILQ_FOREACH (lc, &parent->cells, entry)
	    number++;
	if (number <= 1)
		return (0);

	if (parent->type == LAYOUT_LEFTRIGHT)
		size = parent->sx;
	else if (parent->type == LAYOUT_TOPBOTTOM)
		size = parent->sy;
	else
		return (0);
	each = (size - (number - 1)) / number;

	changed = 0;
	TAILQ_FOREACH (lc, &parent->cells, entry) {
		if (TAILQ_NEXT(lc, entry) == NULL)
			each = size - ((each + 1) * (number - 1));
		change = 0;
		if (parent->type == LAYOUT_LEFTRIGHT) {
			change = each - (int)lc->sx;
			layout_resize_adjust(w, lc, LAYOUT_LEFTRIGHT, change);
		} else if (parent->type == LAYOUT_TOPBOTTOM) {
			change = each - (int)lc->sy;
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
			layout_fix_offsets(parent);
			layout_fix_panes(w, w->sx, w->sy);
			break;
		}
	} while ((parent = parent->parent) != NULL);
}
