/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <unistd.h>

#include "tmux.h"

/* Selected area in screen. */
struct screen_sel {
	int		 hidden;
	int		 rectangle;
	int		 modekeys;

	u_int		 sx;
	u_int		 sy;

	u_int		 ex;
	u_int		 ey;

	struct grid_cell cell;
};

/* Entry on title stack. */
struct screen_title_entry {
	char				*text;

	TAILQ_ENTRY(screen_title_entry)	 entry;
};
TAILQ_HEAD(screen_titles, screen_title_entry);

static void	screen_resize_y(struct screen *, u_int, int, u_int *);
static void	screen_reflow(struct screen *, u_int, u_int *, u_int *, int);

/* Free titles stack. */
static void
screen_free_titles(struct screen *s)
{
	struct screen_title_entry	*title_entry;

	if (s->titles == NULL)
		return;

	while ((title_entry = TAILQ_FIRST(s->titles)) != NULL) {
		TAILQ_REMOVE(s->titles, title_entry, entry);
		free(title_entry->text);
		free(title_entry);
	}

	free(s->titles);
	s->titles = NULL;
}

/* Create a new screen. */
void
screen_init(struct screen *s, u_int sx, u_int sy, u_int hlimit)
{
	s->grid = grid_create(sx, sy, hlimit);
	s->saved_grid = NULL;

	s->title = xstrdup("");
	s->titles = NULL;
	s->path = NULL;

	s->cstyle = 0;
	s->ccolour = xstrdup("");
	s->tabs = NULL;
	s->sel = NULL;

	s->write_list = NULL;

	screen_reinit(s);
}

/* Reinitialise screen. */
void
screen_reinit(struct screen *s)
{
	s->cx = 0;
	s->cy = 0;

	s->rupper = 0;
	s->rlower = screen_size_y(s) - 1;

	s->mode = MODE_CURSOR|MODE_WRAP;
	if (options_get_number(global_options, "extended-keys") == 2)
		s->mode |= MODE_KEXTENDED;

	if (s->saved_grid != NULL)
		screen_alternate_off(s, NULL, 0);
	s->saved_cx = UINT_MAX;
	s->saved_cy = UINT_MAX;

	screen_reset_tabs(s);

	grid_clear_lines(s->grid, s->grid->hsize, s->grid->sy, 8);

	screen_clear_selection(s);
	screen_free_titles(s);
}

/* Destroy a screen. */
void
screen_free(struct screen *s)
{
	free(s->sel);
	free(s->tabs);
	free(s->path);
	free(s->title);
	free(s->ccolour);

	if (s->write_list != NULL)
		screen_write_free_list(s);

	if (s->saved_grid != NULL)
		grid_destroy(s->saved_grid);
	grid_destroy(s->grid);

	screen_free_titles(s);
}

/* Reset tabs to default, eight spaces apart. */
void
screen_reset_tabs(struct screen *s)
{
	u_int	i;

	free(s->tabs);

	if ((s->tabs = bit_alloc(screen_size_x(s))) == NULL)
		fatal("bit_alloc failed");
	for (i = 8; i < screen_size_x(s); i += 8)
		bit_set(s->tabs, i);
}

/* Set screen cursor style. */
void
screen_set_cursor_style(struct screen *s, u_int style)
{
	if (style <= 6) {
		s->cstyle = style;
		s->mode &= ~MODE_BLINKING;
	}
}

/* Set screen cursor colour. */
void
screen_set_cursor_colour(struct screen *s, const char *colour)
{
	free(s->ccolour);
	s->ccolour = xstrdup(colour);
}

/* Set screen title. */
int
screen_set_title(struct screen *s, const char *title)
{
	if (!utf8_isvalid(title))
		return (0);
	free(s->title);
	s->title = xstrdup(title);
	return (1);
}

/* Set screen path. */
void
screen_set_path(struct screen *s, const char *path)
{
	free(s->path);
	utf8_stravis(&s->path, path, VIS_OCTAL|VIS_CSTYLE|VIS_TAB|VIS_NL);
}

/* Push the current title onto the stack. */
void
screen_push_title(struct screen *s)
{
	struct screen_title_entry *title_entry;

	if (s->titles == NULL) {
		s->titles = xmalloc(sizeof *s->titles);
		TAILQ_INIT(s->titles);
	}
	title_entry = xmalloc(sizeof *title_entry);
	title_entry->text = xstrdup(s->title);
	TAILQ_INSERT_HEAD(s->titles, title_entry, entry);
}

/*
 * Pop a title from the stack and set it as the screen title. If the stack is
 * empty, do nothing.
 */
void
screen_pop_title(struct screen *s)
{
	struct screen_title_entry *title_entry;

	if (s->titles == NULL)
		return;

	title_entry = TAILQ_FIRST(s->titles);
	if (title_entry != NULL) {
		screen_set_title(s, title_entry->text);

		TAILQ_REMOVE(s->titles, title_entry, entry);
		free(title_entry->text);
		free(title_entry);
	}
}

/* Resize screen with options. */
void
screen_resize_cursor(struct screen *s, u_int sx, u_int sy, int reflow,
    int eat_empty, int cursor)
{
	u_int	cx = s->cx, cy = s->grid->hsize + s->cy;

	if (s->write_list != NULL)
		screen_write_free_list(s);

	log_debug("%s: new size %ux%u, now %ux%u (cursor %u,%u = %u,%u)",
	    __func__, sx, sy, screen_size_x(s), screen_size_y(s), s->cx, s->cy,
	    cx, cy);

	if (sx < 1)
		sx = 1;
	if (sy < 1)
		sy = 1;

	if (sx != screen_size_x(s)) {
		s->grid->sx = sx;
		screen_reset_tabs(s);
	} else
		reflow = 0;

	if (sy != screen_size_y(s))
		screen_resize_y(s, sy, eat_empty, &cy);

	if (reflow)
		screen_reflow(s, sx, &cx, &cy, cursor);

	if (cy >= s->grid->hsize) {
		s->cx = cx;
		s->cy = cy - s->grid->hsize;
	} else {
		s->cx = 0;
		s->cy = 0;
	}

	log_debug("%s: cursor finished at %u,%u = %u,%u", __func__, s->cx,
	    s->cy, cx, cy);

	if (s->write_list != NULL)
		screen_write_make_list(s);
}

/* Resize screen. */
void
screen_resize(struct screen *s, u_int sx, u_int sy, int reflow)
{
	screen_resize_cursor(s, sx, sy, reflow, 1, 1);
}

static void
screen_resize_y(struct screen *s, u_int sy, int eat_empty, u_int *cy)
{
	struct grid	*gd = s->grid;
	u_int		 needed, available, oldy, i;

	if (sy == 0)
		fatalx("zero size");
	oldy = screen_size_y(s);

	/*
	 * When resizing:
	 *
	 * If the height is decreasing, delete lines from the bottom until
	 * hitting the cursor, then push lines from the top into the history.
	 *
	 * When increasing, pull as many lines as possible from scrolled
	 * history (not explicitly cleared from view) to the top, then fill the
	 * remaining with blanks at the bottom.
	 */

	/* Size decreasing. */
	if (sy < oldy) {
		needed = oldy - sy;

		/* Delete as many lines as possible from the bottom. */
		if (eat_empty) {
			available = oldy - 1 - s->cy;
			if (available > 0) {
				if (available > needed)
					available = needed;
				grid_view_delete_lines(gd, oldy - available,
				    available, 8);
			}
			needed -= available;
		}

		/*
		 * Now just increase the history size, if possible, to take
		 * over the lines which are left. If history is off, delete
		 * lines from the top.
		 */
		available = s->cy;
		if (gd->flags & GRID_HISTORY) {
			gd->hscrolled += needed;
			gd->hsize += needed;
		} else if (needed > 0 && available > 0) {
			if (available > needed)
				available = needed;
			grid_view_delete_lines(gd, 0, available, 8);
			(*cy) -= available;
		}
	}

	/* Resize line array. */
	grid_adjust_lines(gd, gd->hsize + sy);

	/* Size increasing. */
	if (sy > oldy) {
		needed = sy - oldy;

		/*
		 * Try to pull as much as possible out of scrolled history, if
		 * is is enabled.
		 */
		available = gd->hscrolled;
		if (gd->flags & GRID_HISTORY && available > 0) {
			if (available > needed)
				available = needed;
			gd->hscrolled -= available;
			gd->hsize -= available;
		} else
			available = 0;
		needed -= available;

		/* Then fill the rest in with blanks. */
		for (i = gd->hsize + sy - needed; i < gd->hsize + sy; i++)
			grid_empty_line(gd, i, 8);
	}

	/* Set the new size, and reset the scroll region. */
	gd->sy = sy;
	s->rupper = 0;
	s->rlower = screen_size_y(s) - 1;
}

/* Set selection. */
void
screen_set_selection(struct screen *s, u_int sx, u_int sy,
    u_int ex, u_int ey, u_int rectangle, int modekeys, struct grid_cell *gc)
{
	if (s->sel == NULL)
		s->sel = xcalloc(1, sizeof *s->sel);

	memcpy(&s->sel->cell, gc, sizeof s->sel->cell);
	s->sel->hidden = 0;
	s->sel->rectangle = rectangle;
	s->sel->modekeys = modekeys;

	s->sel->sx = sx;
	s->sel->sy = sy;
	s->sel->ex = ex;
	s->sel->ey = ey;
}

/* Clear selection. */
void
screen_clear_selection(struct screen *s)
{
	free(s->sel);
	s->sel = NULL;
}

/* Hide selection. */
void
screen_hide_selection(struct screen *s)
{
	if (s->sel != NULL)
		s->sel->hidden = 1;
}

/* Check if cell in selection. */
int
screen_check_selection(struct screen *s, u_int px, u_int py)
{
	struct screen_sel	*sel = s->sel;
	u_int			 xx;

	if (sel == NULL || sel->hidden)
		return (0);

	if (sel->rectangle) {
		if (sel->sy < sel->ey) {
			/* start line < end line -- downward selection. */
			if (py < sel->sy || py > sel->ey)
				return (0);
		} else if (sel->sy > sel->ey) {
			/* start line > end line -- upward selection. */
			if (py > sel->sy || py < sel->ey)
				return (0);
		} else {
			/* starting line == ending line. */
			if (py != sel->sy)
				return (0);
		}

		/*
		 * Need to include the selection start row, but not the cursor
		 * row, which means the selection changes depending on which
		 * one is on the left.
		 */
		if (sel->ex < sel->sx) {
			/* Cursor (ex) is on the left. */
			if (px < sel->ex)
				return (0);

			if (px > sel->sx)
				return (0);
		} else {
			/* Selection start (sx) is on the left. */
			if (px < sel->sx)
				return (0);

			if (px > sel->ex)
				return (0);
		}
	} else {
		/*
		 * Like emacs, keep the top-left-most character, and drop the
		 * bottom-right-most, regardless of copy direction.
		 */
		if (sel->sy < sel->ey) {
			/* starting line < ending line -- downward selection. */
			if (py < sel->sy || py > sel->ey)
				return (0);

			if (py == sel->sy && px < sel->sx)
				return (0);

			if (sel->modekeys == MODEKEY_EMACS)
				xx = (sel->ex == 0 ? 0 : sel->ex - 1);
			else
				xx = sel->ex;
			if (py == sel->ey && px > xx)
				return (0);
		} else if (sel->sy > sel->ey) {
			/* starting line > ending line -- upward selection. */
			if (py > sel->sy || py < sel->ey)
				return (0);

			if (py == sel->ey && px < sel->ex)
				return (0);

			if (sel->modekeys == MODEKEY_EMACS)
				xx = sel->sx - 1;
			else
				xx = sel->sx;
			if (py == sel->sy && (sel->sx == 0 || px > xx))
				return (0);
		} else {
			/* starting line == ending line. */
			if (py != sel->sy)
				return (0);

			if (sel->ex < sel->sx) {
				/* cursor (ex) is on the left */
				if (sel->modekeys == MODEKEY_EMACS)
					xx = sel->sx - 1;
				else
					xx = sel->sx;
				if (px > xx || px < sel->ex)
					return (0);
			} else {
				/* selection start (sx) is on the left */
				if (sel->modekeys == MODEKEY_EMACS)
					xx = (sel->ex == 0 ? 0 : sel->ex - 1);
				else
					xx = sel->ex;
				if (px < sel->sx || px > xx)
					return (0);
			}
		}
	}

	return (1);
}

/* Get selected grid cell. */
void
screen_select_cell(struct screen *s, struct grid_cell *dst,
    const struct grid_cell *src)
{
	if (s->sel == NULL || s->sel->hidden)
		return;

	memcpy(dst, &s->sel->cell, sizeof *dst);

	utf8_copy(&dst->data, &src->data);
	dst->attr = dst->attr & ~GRID_ATTR_CHARSET;
	dst->attr |= src->attr & GRID_ATTR_CHARSET;
	dst->flags = src->flags;
}

/* Reflow wrapped lines. */
static void
screen_reflow(struct screen *s, u_int new_x, u_int *cx, u_int *cy, int cursor)
{
	u_int	wx, wy;

	if (cursor) {
		grid_wrap_position(s->grid, *cx, *cy, &wx, &wy);
		log_debug("%s: cursor %u,%u is %u,%u", __func__, *cx, *cy, wx,
		    wy);
	}

	grid_reflow(s->grid, new_x);

	if (cursor) {
		grid_unwrap_position(s->grid, cx, cy, wx, wy);
		log_debug("%s: new cursor is %u,%u", __func__, *cx, *cy);
	}
	else {
		*cx = 0;
		*cy = s->grid->hsize;
	}
}

/*
 * Enter alternative screen mode. A copy of the visible screen is saved and the
 * history is not updated.
 */
void
screen_alternate_on(struct screen *s, struct grid_cell *gc, int cursor)
{
	u_int	sx, sy;

	if (s->saved_grid != NULL)
		return;
	sx = screen_size_x(s);
	sy = screen_size_y(s);

	s->saved_grid = grid_create(sx, sy, 0);
	grid_duplicate_lines(s->saved_grid, 0, s->grid, screen_hsize(s), sy);
	if (cursor) {
		s->saved_cx = s->cx;
		s->saved_cy = s->cy;
	}
	memcpy(&s->saved_cell, gc, sizeof s->saved_cell);

	grid_view_clear(s->grid, 0, 0, sx, sy, 8);

	s->saved_flags = s->grid->flags;
	s->grid->flags &= ~GRID_HISTORY;
}

/* Exit alternate screen mode and restore the copied grid. */
void
screen_alternate_off(struct screen *s, struct grid_cell *gc, int cursor)
{
	u_int	sx = screen_size_x(s), sy = screen_size_y(s);

	/*
	 * If the current size is different, temporarily resize to the old size
	 * before copying back.
	 */
	if (s->saved_grid != NULL)
		screen_resize(s, s->saved_grid->sx, s->saved_grid->sy, 1);

	/*
	 * Restore the cursor position and cell. This happens even if not
	 * currently in the alternate screen.
	 */
	if (cursor && s->saved_cx != UINT_MAX && s->saved_cy != UINT_MAX) {
		s->cx = s->saved_cx;
		s->cy = s->saved_cy;
		if (gc != NULL)
			memcpy(gc, &s->saved_cell, sizeof *gc);
	}

	/* If not in the alternate screen, do nothing more. */
	if (s->saved_grid == NULL) {
		if (s->cx > screen_size_x(s) - 1)
			s->cx = screen_size_x(s) - 1;
		if (s->cy > screen_size_y(s) - 1)
			s->cy = screen_size_y(s) - 1;
		return;
	}

	/* Restore the saved grid. */
	grid_duplicate_lines(s->grid, screen_hsize(s), s->saved_grid, 0,
	    s->saved_grid->sy);

	/*
	 * Turn history back on (so resize can use it) and then resize back to
	 * the current size.
	 */
	if (s->saved_flags & GRID_HISTORY)
		s->grid->flags |= GRID_HISTORY;
	screen_resize(s, sx, sy, 1);

	grid_destroy(s->saved_grid);
	s->saved_grid = NULL;

	if (s->cx > screen_size_x(s) - 1)
		s->cx = screen_size_x(s) - 1;
	if (s->cy > screen_size_y(s) - 1)
		s->cy = screen_size_y(s) - 1;
}
