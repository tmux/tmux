/* $OpenBSD: layout-set.c,v 1.40 2026/07/13 09:42:12 nicm Exp $ */

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
#include <string.h>

#include "tmux.h"

/*
 * Set window layouts - predefined methods to arrange windows. These are
 * one-off and generate a layout tree.
 */

static void	layout_set_even_h(struct window *);
static void	layout_set_even_v(struct window *);
static void	layout_set_main_h(struct window *);
static void	layout_set_main_h_mirrored(struct window *);
static void	layout_set_main_v(struct window *);
static void	layout_set_main_v_mirrored(struct window *);
static void	layout_set_tiled(struct window *);

static const struct {
	const char	*name;
	void	      	(*arrange)(struct window *);
} layout_sets[] = {
	{ "even-horizontal", layout_set_even_h },
	{ "even-vertical", layout_set_even_v },
	{ "main-horizontal", layout_set_main_h },
	{ "main-horizontal-mirrored", layout_set_main_h_mirrored },
	{ "main-vertical", layout_set_main_v },
	{ "main-vertical-mirrored", layout_set_main_v_mirrored },
	{ "tiled", layout_set_tiled },
};

int
layout_set_lookup(const char *name)
{
	u_int	i;
	int	matched = -1;

	for (i = 0; i < nitems(layout_sets); i++) {
		if (strcmp(layout_sets[i].name, name) == 0)
			return (i);
	}
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

static struct window_pane *
layout_set_first_tiled(struct window *w)
{
	struct window_pane	*wp;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp->layout_cell && layout_cell_is_tiled(wp->layout_cell))
			return (wp);
	}
	return (NULL);
}

static void
layout_set_link_floating(struct window *w, struct layout_cell *lcroot)
{
	struct window_pane	*wp;
	struct layout_cell	*lc;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		lc = wp->layout_cell;
		if (!layout_cell_is_tiled(lc)) {
			TAILQ_INSERT_TAIL(&lcroot->cells, lc, entry);
			lc->parent = lcroot;
		}
	}
}

static void
layout_set_even(struct window *w, enum layout_type type)
{
	struct window_pane	*wp;
	struct layout_cell	*lcroot, *lcchild;
	u_int			 n, sx, sy;

	layout_print_cell(w->layout_root, __func__, 1);

	n = window_count_panes(w, 0);
	if (n <= 1)
		return;

	if (type == LAYOUT_LEFTRIGHT) {
		sx = (n * (PANE_MINIMUM + 1)) - 1;
		if (sx < w->sx)
			sx = w->sx;
		sy = w->sy;
	} else {
		sy = (n * (PANE_MINIMUM + 1)) - 1;
		if (sy < w->sy)
			sy = w->sy;
		sx = w->sx;
	}

	layout_free(w, 1);
	lcroot = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lcroot, sx, sy, 0, 0);
	layout_make_node(lcroot, type);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		lcchild = wp->layout_cell;
		TAILQ_INSERT_TAIL(&lcroot->cells, lcchild, entry);
		lcchild->parent = lcroot;
		if (layout_cell_is_tiled(lcchild)) {
			lcchild->g.sx = w->sx;
			lcchild->g.sy = w->sy;
		}
	}

	layout_spread_cell(w, lcroot);

	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);

	layout_print_cell(w->layout_root, __func__, 1);

	window_resize(w, lcroot->g.sx, lcroot->g.sy, -1, -1);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_even_h(struct window *w)
{
	layout_set_even(w, LAYOUT_LEFTRIGHT);
}

static void
layout_set_even_v(struct window *w)
{
	layout_set_even(w, LAYOUT_TOPBOTTOM);
}

static void
layout_set_main_h(struct window *w)
{
	struct window_pane	*wp, *wpmain;
	struct layout_cell	*lcroot, *lcmain, *lcother, *lcchild;
	u_int			 n, mainh, otherh, sx, sy;
	char			*cause;
	const char		*s;

	layout_print_cell(w->layout_root, __func__, 1);

	n = window_count_panes(w, 0);
	if (n <= 1)
		return;
	n--;	/* take off main pane */

	/* Find available height - take off one line for the border. */
	sy = w->sy - 1;

	/* Get the main pane height. */
	s = options_get_string(w->options, "main-pane-height");
	mainh = args_string_percentage(s, 0, sy, sy, &cause);
	if (cause != NULL) {
		mainh = 24;
		free(cause);
	}

	/* Work out the other pane height. */
	if (mainh + PANE_MINIMUM >= sy) {
		if (sy <= PANE_MINIMUM + PANE_MINIMUM)
			mainh = PANE_MINIMUM;
		else
			mainh = sy - PANE_MINIMUM;
		otherh = PANE_MINIMUM;
	} else {
		s = options_get_string(w->options, "other-pane-height");
		otherh = args_string_percentage(s, 0, sy, sy, &cause);
		if (cause != NULL || otherh == 0) {
			otherh = sy - mainh;
			free(cause);
		} else if (otherh > sy || sy - otherh < mainh)
			otherh = sy - mainh;
		else
			mainh = sy - otherh;
	}

	/* Work out what width is needed. */
	sx = (n * (PANE_MINIMUM + 1)) - 1;
	if (sx < w->sx)
		sx = w->sx;

	layout_free(w, 1);
	lcroot = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lcroot, sx, mainh + otherh + 1, 0, 0);
	layout_make_node(lcroot, LAYOUT_TOPBOTTOM);

	wpmain = layout_set_first_tiled(w);
	lcmain = wpmain->layout_cell;
	lcmain->parent = lcroot;
	layout_set_size(lcmain, sx, mainh, 0, 0);
	TAILQ_INSERT_TAIL(&lcroot->cells, lcmain, entry);

	if (n == 1) {
		wp = TAILQ_NEXT(wpmain, entry);
		while (wp != NULL && !layout_cell_is_tiled(wp->layout_cell))
			wp = TAILQ_NEXT(wp, entry);
		TAILQ_INSERT_TAIL(&lcroot->cells, wp->layout_cell, entry);
		wp->layout_cell->parent = lcroot;
		layout_set_size(wp->layout_cell, sx, otherh, 0, 0);
		layout_set_link_floating(w, lcroot);
	} else {
		lcother = layout_create_cell(lcroot);
		layout_set_size(lcother, sx, otherh, 0, 0);
		layout_make_node(lcother, LAYOUT_LEFTRIGHT);
		TAILQ_INSERT_TAIL(&lcroot->cells, lcother, entry);

		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp == wpmain)
				continue;
			lcchild = wp->layout_cell;
			TAILQ_INSERT_TAIL(&lcother->cells, lcchild, entry);
			lcchild->parent = lcother;
			if (layout_cell_is_tiled(lcchild))
				layout_set_size(lcchild, PANE_MINIMUM, otherh,
				    0, 0);
		}
		layout_spread_cell(w, lcother);
	}

	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);

	layout_print_cell(w->layout_root, __func__, 1);

	window_resize(w, lcroot->g.sx, lcroot->g.sy, -1, -1);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_main_h_mirrored(struct window *w)
{
	struct window_pane	*wp, *wpmain;
	struct layout_cell	*lcroot, *lcmain, *lcother, *lcchild;
	u_int			 n, mainh, otherh, sx, sy;
	char			*cause;
	const char		*s;

	layout_print_cell(w->layout_root, __func__, 1);

	n = window_count_panes(w, 0);
	if (n <= 1)
		return;
	n--;	/* take off main pane */

	/* Find available height - take off one line for the border. */
	sy = w->sy - 1;

	/* Get the main pane height. */
	s = options_get_string(w->options, "main-pane-height");
	mainh = args_string_percentage(s, 0, sy, sy, &cause);
	if (cause != NULL) {
		mainh = 24;
		free(cause);
	}

	/* Work out the other pane height. */
	if (mainh + PANE_MINIMUM >= sy) {
		if (sy <= PANE_MINIMUM + PANE_MINIMUM)
			mainh = PANE_MINIMUM;
		else
			mainh = sy - PANE_MINIMUM;
		otherh = PANE_MINIMUM;
	} else {
		s = options_get_string(w->options, "other-pane-height");
		otherh = args_string_percentage(s, 0, sy, sy, &cause);
		if (cause != NULL || otherh == 0) {
			otherh = sy - mainh;
			free(cause);
		} else if (otherh > sy || sy - otherh < mainh)
			otherh = sy - mainh;
		else
			mainh = sy - otherh;
	}

	/* Work out what width is needed. */
	sx = (n * (PANE_MINIMUM + 1)) - 1;
	if (sx < w->sx)
		sx = w->sx;

	layout_free(w, 1);
	lcroot = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lcroot, sx, mainh + otherh + 1, 0, 0);
	layout_make_node(lcroot, LAYOUT_TOPBOTTOM);

	wpmain = layout_set_first_tiled(w);
	lcmain = wpmain->layout_cell;
	lcmain->parent = lcroot;
	layout_set_size(lcmain, sx, mainh, 0, 0);
	TAILQ_INSERT_TAIL(&lcroot->cells, lcmain, entry);

	if (n == 1) {
		wp = TAILQ_NEXT(wpmain, entry);
		while (wp != NULL && !layout_cell_is_tiled(wp->layout_cell))
			wp = TAILQ_NEXT(wp, entry);
		TAILQ_INSERT_HEAD(&lcroot->cells, wp->layout_cell, entry);
		wp->layout_cell->parent = lcroot;
		layout_set_size(wp->layout_cell, sx, otherh, 0, 0);
		layout_set_link_floating(w, lcroot);
	} else {
		lcother = layout_create_cell(lcroot);
		layout_set_size(lcother, sx, otherh, 0, 0);
		layout_make_node(lcother, LAYOUT_LEFTRIGHT);
		TAILQ_INSERT_HEAD(&lcroot->cells, lcother, entry);

		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp == wpmain)
				continue;
			lcchild = wp->layout_cell;
			TAILQ_INSERT_TAIL(&lcother->cells, lcchild, entry);
			lcchild->parent = lcother;
			if (layout_cell_is_tiled(lcchild))
				layout_set_size(lcchild, PANE_MINIMUM, otherh,
				    0, 0);
		}
		layout_spread_cell(w, lcother);
	}

	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);

	layout_print_cell(w->layout_root, __func__, 1);

	window_resize(w, lcroot->g.sx, lcroot->g.sy, -1, -1);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_main_v(struct window *w)
{
	struct window_pane	*wp, *wpmain;
	struct layout_cell	*lcroot, *lcmain, *lcother, *lcchild;
	u_int			 n, mainw, otherw, sx, sy;
	char			*cause;
	const char		*s;

	layout_print_cell(w->layout_root, __func__, 1);

	n = window_count_panes(w, 0);
	if (n <= 1)
		return;
	n--;	/* take off main pane */

	/* Find available width - take off one column for the border. */
	sx = w->sx - 1;

	/* Get the main pane width. */
	s = options_get_string(w->options, "main-pane-width");
	mainw = args_string_percentage(s, 0, sx, sx, &cause);
	if (cause != NULL) {
		mainw = 80;
		free(cause);
	}

	/* Work out the other pane width. */
	if (mainw + PANE_MINIMUM >= sx) {
		if (sx <= PANE_MINIMUM + PANE_MINIMUM)
			mainw = PANE_MINIMUM;
		else
			mainw = sx - PANE_MINIMUM;
		otherw = PANE_MINIMUM;
	} else {
		s = options_get_string(w->options, "other-pane-width");
		otherw = args_string_percentage(s, 0, sx, sx, &cause);
		if (cause != NULL || otherw == 0) {
			otherw = sx - mainw;
			free(cause);
		} else if (otherw > sx || sx - otherw < mainw)
			otherw = sx - mainw;
		else
			mainw = sx - otherw;
	}

	/* Work out what height is needed. */
	sy = (n * (PANE_MINIMUM + 1)) - 1;
	if (sy < w->sy)
		sy = w->sy;

	layout_free(w, 1);
	lcroot = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lcroot, mainw + otherw + 1, sy, 0, 0);
	layout_make_node(lcroot, LAYOUT_LEFTRIGHT);

	wpmain = layout_set_first_tiled(w);
	lcmain = wpmain->layout_cell;
	lcmain->parent = lcroot;
	layout_set_size(lcmain, mainw, sy, 0, 0);
	TAILQ_INSERT_TAIL(&lcroot->cells, lcmain, entry);

	if (n == 1) {
		wp = TAILQ_NEXT(wpmain, entry);
		while (wp != NULL && !layout_cell_is_tiled(wp->layout_cell))
			wp = TAILQ_NEXT(wp, entry);
		TAILQ_INSERT_TAIL(&lcroot->cells, wp->layout_cell, entry);
		wp->layout_cell->parent = lcroot;
		layout_set_size(wp->layout_cell, otherw, sy, 0, 0);
		layout_set_link_floating(w, lcroot);
	} else {
		lcother = layout_create_cell(lcroot);
		layout_make_node(lcother, LAYOUT_TOPBOTTOM);
		layout_set_size(lcother, otherw, sy, 0, 0);
		TAILQ_INSERT_TAIL(&lcroot->cells, lcother, entry);

		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp == wpmain)
				continue;
			lcchild = wp->layout_cell;
			TAILQ_INSERT_TAIL(&lcother->cells, lcchild, entry);
			lcchild->parent = lcother;
			if (layout_cell_is_tiled(lcchild))
				layout_set_size(lcchild, otherw, PANE_MINIMUM,
				    0, 0);
		}
		layout_spread_cell(w, lcother);
	}

	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);

	layout_print_cell(w->layout_root, __func__, 1);

	window_resize(w, lcroot->g.sx, lcroot->g.sy, -1, -1);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_main_v_mirrored(struct window *w)
{
	struct window_pane	*wp, *wpmain;
	struct layout_cell	*lcroot, *lcmain, *lcother, *lcchild;
	u_int			 n, mainw, otherw, sx, sy;
	char			*cause;
	const char		*s;

	layout_print_cell(w->layout_root, __func__, 1);

	/* Get number of panes. */
	n = window_count_panes(w, 0);
	if (n <= 1)
		return;
	n--;	/* take off main pane */

	/* Find available width - take off one column for the border. */
	sx = w->sx - 1;

	/* Get the main pane width. */
	s = options_get_string(w->options, "main-pane-width");
	mainw = args_string_percentage(s, 0, sx, sx, &cause);
	if (cause != NULL) {
		mainw = 80;
		free(cause);
	}

	/* Work out the other pane width. */
	if (mainw + PANE_MINIMUM >= sx) {
		if (sx <= PANE_MINIMUM + PANE_MINIMUM)
			mainw = PANE_MINIMUM;
		else
			mainw = sx - PANE_MINIMUM;
		otherw = PANE_MINIMUM;
	} else {
		s = options_get_string(w->options, "other-pane-width");
		otherw = args_string_percentage(s, 0, sx, sx, &cause);
		if (cause != NULL || otherw == 0) {
			otherw = sx - mainw;
			free(cause);
		} else if (otherw > sx || sx - otherw < mainw)
			otherw = sx - mainw;
		else
			mainw = sx - otherw;
	}

	/* Work out what height is needed. */
	sy = (n * (PANE_MINIMUM + 1)) - 1;
	if (sy < w->sy)
		sy = w->sy;

	layout_free(w, 1);
	lcroot = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lcroot, mainw + otherw + 1, sy, 0, 0);
	layout_make_node(lcroot, LAYOUT_LEFTRIGHT);

	wpmain = layout_set_first_tiled(w);
	lcmain = wpmain->layout_cell;
	lcmain->parent = lcroot;
	layout_set_size(lcmain, mainw, sy, 0, 0);
	TAILQ_INSERT_TAIL(&lcroot->cells, lcmain, entry);

	if (n == 1) {
		wp = TAILQ_NEXT(wpmain, entry);
		while (wp != NULL && !layout_cell_is_tiled(wp->layout_cell))
			wp = TAILQ_NEXT(wp, entry);
		TAILQ_INSERT_HEAD(&lcroot->cells, wp->layout_cell, entry);
		wp->layout_cell->parent = lcroot;
		layout_set_size(wp->layout_cell, otherw, sy, 0, 0);
		layout_set_link_floating(w, lcroot);
	} else {
		lcother = layout_create_cell(lcroot);
		layout_make_node(lcother, LAYOUT_TOPBOTTOM);
		layout_set_size(lcother, otherw, sy, 0, 0);
		TAILQ_INSERT_HEAD(&lcroot->cells, lcother, entry);

		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp == wpmain)
				continue;
			lcchild = wp->layout_cell;
			TAILQ_INSERT_TAIL(&lcother->cells, lcchild, entry);
			lcchild->parent = lcother;
			if (layout_cell_is_tiled(lcchild))
				layout_set_size(lcchild, otherw, PANE_MINIMUM,
				    0, 0);
		}
		layout_spread_cell(w, lcother);
	}

	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);

	layout_print_cell(w->layout_root, __func__, 1);

	window_resize(w, lcroot->g.sx, lcroot->g.sy, -1, -1);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);
}

static void
layout_set_tiled(struct window *w)
{
	struct options		*oo = w->options;
	struct window_pane	*wp;
	struct layout_cell	*lcroot, *lcrow, *lcchild;
	u_int			 n, width, height, used, sx, sy;
	u_int			 i, j, columns, rows, max_columns;

	layout_print_cell(w->layout_root, __func__, 1);

	/* Get number of panes. */
	n = window_count_panes(w, 0);
	if (n <= 1)
		return;

	/* Get maximum columns from window option. */
	max_columns = options_get_number(oo, "tiled-layout-max-columns");

	/* How many rows and columns are wanted? */
	rows = columns = 1;
	while (rows * columns < n) {
		rows++;
		if (rows * columns < n &&
		    (max_columns == 0 || columns < max_columns))
			columns++;
	}

	/* What width and height should they be? */
	width = (w->sx - (columns - 1)) / columns;
	if (width < PANE_MINIMUM)
		width = PANE_MINIMUM;
	height = (w->sy - (rows - 1)) / rows;
	if (height < PANE_MINIMUM)
		height = PANE_MINIMUM;

	sx = ((width + 1) * columns) - 1;
	if (sx < w->sx)
		sx = w->sx;
	sy = ((height + 1) * rows) - 1;
	if (sy < w->sy)
		sy = w->sy;

	layout_free(w, 1);
	lcroot = w->layout_root = layout_create_cell(NULL);
	layout_set_size(lcroot, sx, sy, 0, 0);
	layout_make_node(lcroot, LAYOUT_TOPBOTTOM);

	/* Create a grid of the tiled cells. */
	wp = TAILQ_FIRST(&w->panes);
	for (j = 0; j < rows; j++) {
		while (wp != NULL && !layout_cell_is_tiled(wp->layout_cell))
			wp = TAILQ_NEXT(wp, entry);
		/* If this is the last cell, all done. */
		if (wp == NULL)
			break;

		lcchild = wp->layout_cell;

		/* If only one column, just use the row directly. */
		if (n - (j * columns) == 1 || columns == 1) {
			lcchild->parent = lcroot;
			TAILQ_INSERT_TAIL(&lcroot->cells, lcchild, entry);
			layout_set_size(lcchild, w->sx, height, 0, 0);
			wp = TAILQ_NEXT(wp, entry);
			continue;
		}

		/* Create the new row. */
		lcrow = layout_create_cell(lcroot);
		layout_make_node(lcrow, LAYOUT_LEFTRIGHT);
		layout_set_size(lcrow, w->sx, height, 0, 0);
		TAILQ_INSERT_TAIL(&lcroot->cells, lcrow, entry);

		/* Add in the columns. */
		for (i = 0; i < columns; i++) {
			/* Create and add a pane cell. */
			lcchild->parent = lcrow;
			TAILQ_INSERT_TAIL(&lcrow->cells, lcchild, entry);
			layout_set_size(lcchild, width, height, 0, 0);

			/* Move to the next non-floating cell. */
			wp = TAILQ_NEXT(wp, entry);
			while (wp != NULL &&
			    !layout_cell_is_tiled(wp->layout_cell))
				wp = TAILQ_NEXT(wp, entry);
			if (wp == NULL)
				break;
			lcchild = wp->layout_cell;
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

	used = (rows * height) + rows - 1;
	if (w->sy > used) {
		lcrow = TAILQ_LAST(&lcroot->cells, layout_cells);
		layout_resize_adjust(w, lcrow, LAYOUT_TOPBOTTOM,
		    w->sy - used);
	}

	layout_set_link_floating(w, lcroot);
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);

	layout_print_cell(w->layout_root, __func__, 1);

	window_resize(w, lcroot->g.sx, lcroot->g.sy, -1, -1);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);
}
