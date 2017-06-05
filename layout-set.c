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

#include <string.h>

#include "tmux.h"

/*
 * Set window layouts - predefined methods to arrange windows. These are
 * one-off and generate a layout tree.
 */

static void	layout_set_even_h(struct window *);
static void	layout_set_even_v(struct window *);
static void	layout_set_main_h(struct window *);
static void	layout_set_main_v(struct window *);
static void	layout_set_tiled(struct window *);

static const struct {
	const char	*name;
	void	      	(*arrange)(struct window *);
} layout_sets[] = {
	{ "even-horizontal", layout_set_even_h },
	{ "even-vertical", layout_set_even_v },
	{ "main-horizontal", layout_set_main_h },
	{ "main-vertical", layout_set_main_v },
	{ "tiled", layout_set_tiled },
};

int
layout_set_lookup(const char *name)
{
	u_int	i;
	int	matched = -1;

	for (i = 0; i < nitems(layout_sets); i++) {
		if (strncmp(layout_sets[i].name, name, strlen(name)) == 0) {
			if (matched != -1)	/* ambiguous */
				return (-1);
			matched = i;
		}
	}

	return (matched);
}

u_int
layout_set_select(struct window *w, u_int layout)
{
	if (layout > nitems(layout_sets) - 1)
		layout = nitems(layout_sets) - 1;

	if (layout_sets[layout].arrange != NULL)
		layout_sets[layout].arrange(w);

	w->lastlayout = layout;
	return (layout);
}

u_int
layout_set_next(struct window *w)
{
	u_int	layout;

	if (w->lastlayout == -1)
		layout = 0;
	else {
		layout = w->lastlayout + 1;
		if (layout > nitems(layout_sets) - 1)
			layout = 0;
	}

	if (layout_sets[layout].arrange != NULL)
		layout_sets[layout].arrange(w);
	w->lastlayout = layout;
	return (layout);
}

u_int
layout_set_previous(struct window *w)
{
	u_int	layout;

	if (w->lastlayout == -1)
		layout = nitems(layout_sets) - 1;
	else {
		layout = w->lastlayout;
		if (layout == 0)
			layout = nitems(layout_sets) - 1;
		else
			layout--;
	}

	if (layout_sets[layout].arrange != NULL)
		layout_sets[layout].arrange(w);
	w->lastlayout = layout;
	return (layout);
}

static void
layout_set_even_h(struct window *w)
{
	struct window_pane	*wp;
	struct layout_cell	*lc, *lcnew;
	u_int			 i, n, width, xoff;

	layout_print_cell(w->layout_root, __func__, 1);

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n <= 1)
		return;

	/* How many can we fit? */
	width = (w->sx - (n - 1)) / n;
	if (width < PANE_MINIMUM)
		width = PANE_MINIMUM;

	/* Free the old root and construct a new. */
	layout_free(w);
	lc = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lc, w->sx, w->sy, 0, 0);
	layout_make_node(lc, LAYOUT_LEFTRIGHT);

	/* Build new leaf cells. */
	i = xoff = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		/* Create child cell. */
		lcnew = layout_create_cell(lc);
		layout_set_size(lcnew, width, w->sy, xoff, 0);
		layout_make_leaf(lcnew, wp);
		TAILQ_INSERT_TAIL(&lc->cells, lcnew, entry);

		i++;
		xoff += width + 1;
	}

	/* Allocate any remaining space. */
	if (w->sx > xoff - 1) {
		lc = TAILQ_LAST(&lc->cells, layout_cells);
		layout_resize_adjust(w, lc, LAYOUT_LEFTRIGHT,
		    w->sx - (xoff - 1));
	}

	/* Fix cell offsets. */
	layout_fix_offsets(lc);
	layout_fix_panes(w, w->sx, w->sy);

	layout_print_cell(w->layout_root, __func__, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_even_v(struct window *w)
{
	struct window_pane	*wp;
	struct layout_cell	*lc, *lcnew;
	u_int			 i, n, height, yoff;

	layout_print_cell(w->layout_root, __func__, 1);

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n <= 1)
		return;

	/* How many can we fit? */
	height = (w->sy - (n - 1)) / n;
	if (height < PANE_MINIMUM)
		height = PANE_MINIMUM;

	/* Free the old root and construct a new. */
	layout_free(w);
	lc = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lc, w->sx, w->sy, 0, 0);
	layout_make_node(lc, LAYOUT_TOPBOTTOM);

	/* Build new leaf cells. */
	i = yoff = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		/* Create child cell. */
		lcnew = layout_create_cell(lc);
		layout_set_size(lcnew, w->sx, height, 0, yoff);
		layout_make_leaf(lcnew, wp);
		TAILQ_INSERT_TAIL(&lc->cells, lcnew, entry);

		i++;
		yoff += height + 1;
	}

	/* Allocate any remaining space. */
	if (w->sy > yoff - 1) {
		lc = TAILQ_LAST(&lc->cells, layout_cells);
		layout_resize_adjust(w, lc, LAYOUT_TOPBOTTOM,
		    w->sy - (yoff - 1));
	}

	/* Fix cell offsets. */
	layout_fix_offsets(lc);
	layout_fix_panes(w, w->sx, w->sy);

	layout_print_cell(w->layout_root, __func__, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_main_h(struct window *w)
{
	struct window_pane	*wp;
	struct layout_cell	*lc, *lcmain, *lcrow, *lcchild;
	u_int			 n, mainheight, otherheight, width, height;
	u_int			 used, i, j, columns, rows, totalrows;

	layout_print_cell(w->layout_root, __func__, 1);

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n <= 1)
		return;
	n--;	/* take off main pane */

	/* How many rows and columns will be needed, not counting main? */
	columns = (w->sx + 1) / (PANE_MINIMUM + 1);	/* maximum columns */
	if (columns == 0)
		columns = 1;
	rows = 1 + (n - 1) / columns;
	columns = 1 + (n - 1) / rows;
	width = (w->sx - (n - 1)) / columns;

	/* Get the main pane height and add one for separator line. */
	mainheight = options_get_number(w->options, "main-pane-height") + 1;

	/* Get the optional other pane height and add one for separator line. */
	otherheight = options_get_number(w->options, "other-pane-height") + 1;

	/*
	 * If an other pane height was specified, honour it so long as it
	 * doesn't shrink the main height to less than the main-pane-height
	 */
	if (otherheight > 1 && w->sy - otherheight > mainheight)
		mainheight = w->sy - otherheight;
	if (mainheight < PANE_MINIMUM + 1)
		mainheight = PANE_MINIMUM + 1;

	/* Try and make everything fit. */
	totalrows = rows * (PANE_MINIMUM + 1) - 1;
	if (mainheight + totalrows > w->sy) {
		if (totalrows + PANE_MINIMUM + 1 > w->sy)
			mainheight = PANE_MINIMUM + 2;
		else
			mainheight = w->sy - totalrows;
		height = PANE_MINIMUM;
	} else
		height = (w->sy - mainheight - (rows - 1)) / rows;

	/* Free old tree and create a new root. */
	layout_free(w);
	lc = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lc, w->sx, mainheight + rows * (height + 1) - 1, 0, 0);
	layout_make_node(lc, LAYOUT_TOPBOTTOM);

	/* Create the main pane. */
	lcmain = layout_create_cell(lc);
	layout_set_size(lcmain, w->sx, mainheight - 1, 0, 0);
	layout_make_leaf(lcmain, TAILQ_FIRST(&w->panes));
	TAILQ_INSERT_TAIL(&lc->cells, lcmain, entry);

	/* Create a grid of the remaining cells. */
	wp = TAILQ_NEXT(TAILQ_FIRST(&w->panes), entry);
	for (j = 0; j < rows; j++) {
		/* If this is the last cell, all done. */
		if (wp == NULL)
			break;

		/* Create the new row. */
		lcrow = layout_create_cell(lc);
		layout_set_size(lcrow, w->sx, height, 0, 0);
		TAILQ_INSERT_TAIL(&lc->cells, lcrow, entry);

		/* If only one column, just use the row directly. */
		if (columns == 1) {
			layout_make_leaf(lcrow, wp);
			wp = TAILQ_NEXT(wp, entry);
			continue;
		}

		/* Add in the columns. */
		layout_make_node(lcrow, LAYOUT_LEFTRIGHT);
		for (i = 0; i < columns; i++) {
			/* Create and add a pane cell. */
			lcchild = layout_create_cell(lcrow);
			layout_set_size(lcchild, width, height, 0, 0);
			layout_make_leaf(lcchild, wp);
			TAILQ_INSERT_TAIL(&lcrow->cells, lcchild, entry);

			/* Move to the next cell. */
			if ((wp = TAILQ_NEXT(wp, entry)) == NULL)
				break;
		}

		/* Adjust the row to fit the full width if necessary. */
		if (i == columns)
			i--;
		used = ((i + 1) * (width + 1)) - 1;
		if (w->sx <= used)
			continue;
		lcchild = TAILQ_LAST(&lcrow->cells, layout_cells);
		layout_resize_adjust(w, lcchild, LAYOUT_LEFTRIGHT,
		    w->sx - used);
	}

	/* Adjust the last row height to fit if necessary. */
	used = mainheight + (rows * height) + rows - 1;
	if (w->sy > used) {
		lcrow = TAILQ_LAST(&lc->cells, layout_cells);
		layout_resize_adjust(w, lcrow, LAYOUT_TOPBOTTOM,
		    w->sy - used);
	}

	/* Fix cell offsets. */
	layout_fix_offsets(lc);
	layout_fix_panes(w, w->sx, w->sy);

	layout_print_cell(w->layout_root, __func__, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_main_v(struct window *w)
{
	struct window_pane	*wp;
	struct layout_cell	*lc, *lcmain, *lccolumn, *lcchild;
	u_int			 n, mainwidth, otherwidth, width, height;
	u_int			 used, i, j, columns, rows, totalcolumns;

	layout_print_cell(w->layout_root, __func__, 1);

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n <= 1)
		return;
	n--;	/* take off main pane */

	/* How many rows and columns will be needed, not counting main? */
	rows = (w->sy + 1) / (PANE_MINIMUM + 1);	/* maximum rows */
	if (rows == 0)
		rows = 1;
	columns = 1 + (n - 1) / rows;
	rows = 1 + (n - 1) / columns;
	height = (w->sy - (n - 1)) / rows;

	/* Get the main pane width and add one for separator line. */
	mainwidth = options_get_number(w->options, "main-pane-width") + 1;

	/* Get the optional other pane width and add one for separator line. */
	otherwidth = options_get_number(w->options, "other-pane-width") + 1;

	/*
	 * If an other pane width was specified, honour it so long as it
	 * doesn't shrink the main width to less than the main-pane-width
	 */
	if (otherwidth > 1 && w->sx - otherwidth > mainwidth)
		mainwidth = w->sx - otherwidth;
	if (mainwidth < PANE_MINIMUM + 1)
		mainwidth = PANE_MINIMUM + 1;

	/* Try and make everything fit. */
	totalcolumns = columns * (PANE_MINIMUM + 1) - 1;
	if (mainwidth + totalcolumns > w->sx) {
		if (totalcolumns + PANE_MINIMUM + 1 > w->sx)
			mainwidth = PANE_MINIMUM + 2;
		else
			mainwidth = w->sx - totalcolumns;
		width = PANE_MINIMUM;
	} else
		width = (w->sx - mainwidth - (columns - 1)) / columns;

	/* Free old tree and create a new root. */
	layout_free(w);
	lc = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lc, mainwidth + columns * (width + 1) - 1, w->sy, 0, 0);
	layout_make_node(lc, LAYOUT_LEFTRIGHT);

	/* Create the main pane. */
	lcmain = layout_create_cell(lc);
	layout_set_size(lcmain, mainwidth - 1, w->sy, 0, 0);
	layout_make_leaf(lcmain, TAILQ_FIRST(&w->panes));
	TAILQ_INSERT_TAIL(&lc->cells, lcmain, entry);

	/* Create a grid of the remaining cells. */
	wp = TAILQ_NEXT(TAILQ_FIRST(&w->panes), entry);
	for (j = 0; j < columns; j++) {
		/* If this is the last cell, all done. */
		if (wp == NULL)
			break;

		/* Create the new column. */
		lccolumn = layout_create_cell(lc);
		layout_set_size(lccolumn, width, w->sy, 0, 0);
		TAILQ_INSERT_TAIL(&lc->cells, lccolumn, entry);

		/* If only one row, just use the row directly. */
		if (rows == 1) {
			layout_make_leaf(lccolumn, wp);
			wp = TAILQ_NEXT(wp, entry);
			continue;
		}

		/* Add in the rows. */
		layout_make_node(lccolumn, LAYOUT_TOPBOTTOM);
		for (i = 0; i < rows; i++) {
			/* Create and add a pane cell. */
			lcchild = layout_create_cell(lccolumn);
			layout_set_size(lcchild, width, height, 0, 0);
			layout_make_leaf(lcchild, wp);
			TAILQ_INSERT_TAIL(&lccolumn->cells, lcchild, entry);

			/* Move to the next cell. */
			if ((wp = TAILQ_NEXT(wp, entry)) == NULL)
				break;
		}

		/* Adjust the column to fit the full height if necessary. */
		if (i == rows)
			i--;
		used = ((i + 1) * (height + 1)) - 1;
		if (w->sy <= used)
			continue;
		lcchild = TAILQ_LAST(&lccolumn->cells, layout_cells);
		layout_resize_adjust(w, lcchild, LAYOUT_TOPBOTTOM,
		    w->sy - used);
	}

	/* Adjust the last column width to fit if necessary. */
	used = mainwidth + (columns * width) + columns - 1;
	if (w->sx > used) {
		lccolumn = TAILQ_LAST(&lc->cells, layout_cells);
		layout_resize_adjust(w, lccolumn, LAYOUT_LEFTRIGHT,
		    w->sx - used);
	}

	/* Fix cell offsets. */
	layout_fix_offsets(lc);
	layout_fix_panes(w, w->sx, w->sy);

	layout_print_cell(w->layout_root, __func__, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);
}

void
layout_set_tiled(struct window *w)
{
	struct window_pane	*wp;
	struct layout_cell	*lc, *lcrow, *lcchild;
	u_int			 n, width, height, used;
	u_int			 i, j, columns, rows;

	layout_print_cell(w->layout_root, __func__, 1);

	/* Get number of panes. */
	n = window_count_panes(w);
	if (n <= 1)
		return;

	/* How many rows and columns are wanted? */
	rows = columns = 1;
	while (rows * columns < n) {
		rows++;
		if (rows * columns < n)
			columns++;
	}

	/* What width and height should they be? */
	width = (w->sx - (columns - 1)) / columns;
	if (width < PANE_MINIMUM)
		width = PANE_MINIMUM;
	height = (w->sy - (rows - 1)) / rows;
	if (height < PANE_MINIMUM)
		height = PANE_MINIMUM;

	/* Free old tree and create a new root. */
	layout_free(w);
	lc = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lc, (width + 1) * columns - 1,
	    (height + 1) * rows - 1, 0, 0);
	layout_make_node(lc, LAYOUT_TOPBOTTOM);

	/* Create a grid of the cells. */
	wp = TAILQ_FIRST(&w->panes);
	for (j = 0; j < rows; j++) {
		/* If this is the last cell, all done. */
		if (wp == NULL)
			break;

		/* Create the new row. */
		lcrow = layout_create_cell(lc);
		layout_set_size(lcrow, w->sx, height, 0, 0);
		TAILQ_INSERT_TAIL(&lc->cells, lcrow, entry);

		/* If only one column, just use the row directly. */
		if (n - (j * columns) == 1 || columns == 1) {
			layout_make_leaf(lcrow, wp);
			wp = TAILQ_NEXT(wp, entry);
			continue;
		}

		/* Add in the columns. */
		layout_make_node(lcrow, LAYOUT_LEFTRIGHT);
		for (i = 0; i < columns; i++) {
			/* Create and add a pane cell. */
			lcchild = layout_create_cell(lcrow);
			layout_set_size(lcchild, width, height, 0, 0);
			layout_make_leaf(lcchild, wp);
			TAILQ_INSERT_TAIL(&lcrow->cells, lcchild, entry);

			/* Move to the next cell. */
			if ((wp = TAILQ_NEXT(wp, entry)) == NULL)
				break;
		}

		/*
		 * Adjust the row and columns to fit the full width if
		 * necessary.
		 */
		if (i == columns)
			i--;
		used = ((i + 1) * (width + 1)) - 1;
		if (w->sx <= used)
			continue;
		lcchild = TAILQ_LAST(&lcrow->cells, layout_cells);
		layout_resize_adjust(w, lcchild, LAYOUT_LEFTRIGHT,
		    w->sx - used);
	}

	/* Adjust the last row height to fit if necessary. */
	used = (rows * height) + rows - 1;
	if (w->sy > used) {
		lcrow = TAILQ_LAST(&lc->cells, layout_cells);
		layout_resize_adjust(w, lcrow, LAYOUT_TOPBOTTOM,
		    w->sy - used);
	}

	/* Fix cell offsets. */
	layout_fix_offsets(lc);
	layout_fix_panes(w, w->sx, w->sy);

	layout_print_cell(w->layout_root, __func__, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);
}
