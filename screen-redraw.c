/* $OpenBSD: screen-redraw.c,v 1.153 2026/07/17 12:42:51 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Draw the visible area of a window to a client.
 *
 * A scene is built for the client and cached (in struct client). When the
 * offset or size of the visible part of the window changes, the scene is
 * invalidated. It is also invalidated when the generation number is increased;
 * this is done at various points, such as when a pane is moved or resized. The
 * scene only includes the part of the client used for the window: panes, pane
 * status lines, borders, scrollbars, and any area outside the window. The
 * client status line and overlay are not included.
 *
 * A scene is made from spans. A span is a horizontal run of cells on one
 * visible line that can be drawn in the same way. Each span has a type, for
 * example: pane content or pane border cells or pane status line. A span also
 * includes enough additional data to draw it, but does not include items such
 * as the style and content - those are determined when it is drawn.
 *
 * Scenes are built in two stages:
 *
 * 1) redraw_build_cells fills a temporary grid of struct redraw_build_cell
 *    objects, one per visible cell. It marks pane contents, scrollbars,
 *    borders, pane status lines and any cells outside the window. Border cells
 *    may belong to multiple panes, so they may be marked multiple times, with
 *    each pane adding its own state.
 *
 * 2) redraw_make_scene takes the grid of struct redraw_build_cell objects and
 *    converts them into spans by joining adjacent cells that have the same
 *    type and data. These spans together make up the scene (struct
 *    redraw_scene).
 *
 * Once generated, a scene is redrawn by looping over some or all of the spans
 * (in redraw_draw), working out the style and content, and writing to the
 * client terminal. Until it is invalidated, the scene may be redrawn multiple
 * times without being rebuilt.
 */

/* Type of span in the scene. */
enum redraw_span_type {
	REDRAW_SPAN_PANE,	/* inside a pane */
	REDRAW_SPAN_OUTSIDE,	/* outside the window */
	REDRAW_SPAN_EMPTY,	/* inside the window but nothing visible */
	REDRAW_SPAN_STATUS,	/* pane status line */
	REDRAW_SPAN_BORDER,	/* pane border */
	REDRAW_SPAN_SCROLLBAR,	/* pane scrollbar */
	REDRAW_SPAN_MENU,	/* window menu */
};
#define REDRAW_SPAN_TYPES 7

/* Border connections to adjacent cells. */
#define REDRAW_BORDER_L 0x1
#define REDRAW_BORDER_R 0x2
#define REDRAW_BORDER_U 0x4
#define REDRAW_BORDER_D 0x8

/* Span flags. */
#define REDRAW_BORDER_IS_ARROW 0x1
#define REDRAW_SCROLLBAR_LEFT 0x2
#define REDRAW_SCROLLBAR_RIGHT 0x4
#define REDRAW_SCROLLBAR_OVERLAY 0x8

/* Draw operations. */
#define REDRAW_PANE 0x1
#define REDRAW_OUTSIDE 0x2
#define REDRAW_EMPTY 0x4
#define REDRAW_PANE_BORDER 0x8
#define REDRAW_PANE_STATUS 0x10
#define REDRAW_PANE_SCROLLBAR 0x20
#define REDRAW_STATUS 0x40
#define REDRAW_MENU 0x80
#define REDRAW_OVERLAY 0x100

/* Draw everything. */
#define REDRAW_ALL 0x7fffffff
#define REDRAW_IS_ALL(flags) ((flags) == REDRAW_ALL)

/* UTF-8 isolate characters. */
#define REDRAW_START_ISOLATE "\342\201\246"
#define REDRAW_END_ISOLATE "\342\201\251"

/* Data for a span. */
struct redraw_span_data {
	enum redraw_span_type	type;

	union {
		struct {
			/* Pane this span belongs to. */
			struct window_pane	*wp;

			/* Position of span inside the pane. */
			u_int			 px;
			u_int			 py;
		} p; /* pane */

		struct {
			/* Adjacent panes on each side. */
			struct window_pane	*top_wp;
			struct window_pane	*bottom_wp;
			struct window_pane	*left_wp;
			struct window_pane	*right_wp;

			/*
			 * The pane this span belongs if that is known when the
			 * scene is built. This is used for the half coloured
			 * active pane indicator.
			 */
			struct window_pane	*style_wp;

			/* Cell type for this span. */
			int			 cell_type;
			int			 cell_mask;

			/* Line styles for this span. */
			enum pane_lines		 top_lines;
			enum pane_lines		 bottom_lines;
			enum pane_lines		 left_lines;
			enum pane_lines		 right_lines;

			/* Flags for this span. */
			int			 flags;
		} b; /* pane border */

		struct {
			/* The pane and the offset into the status line. */
			struct window_pane	*wp;
			u_int			 offset;
			int			 cell_type;
		} st; /* pane status line */

		struct {
			/* Pane this span belongs to. */
			struct window_pane	*wp;

			/* Line within the scrollbar. */
			u_int			 y;

			/* Full height of scrollbar. */
			u_int			 height;

			/* Flags for this span. */
			int			 flags;
		} sb; /* pane scrollbar */

		struct {
			/* Menu this span belongs to. */
			struct menu_data	*md;

			/* Position of span inside the menu. */
			u_int			 px;
			u_int			 py;
		} m; /* menu */
	};
};

/* A span of cells of the same type inside a line. */
struct redraw_span {
	u_int				x;
	u_int				width;
	struct redraw_span_data		data;

	TAILQ_ENTRY(redraw_span)	entry;
};
TAILQ_HEAD(redraw_spans, redraw_span);

/* A visible line on the client. */
struct redraw_line {
	struct redraw_spans	spans[REDRAW_SPAN_TYPES];
};

/* A scene representing all the spans on the client. */
struct redraw_scene {
	struct client		*c;
	struct window		*w;
	struct redraw_line	*lines;

	uint64_t		 generation;
	u_int			 sx;
	u_int			 sy;
	u_int			 ox;
	u_int			 oy;
};

/* Cell for building the scene. */
struct redraw_build_cell {
	struct redraw_span_data	 data;
};

static struct redraw_build_cell	*redraw_cells;
static size_t			 redraw_ncells;

/* Context for building the scene. */
struct redraw_build_ctx {
	struct client				*c;
	struct window				*w;

	u_int					 ox;
	u_int					 oy;
	u_int					 sx;
	u_int					 sy;

	int					 ind;

	struct redraw_build_cell		*cells;
};

/* Context for redrawing. */
struct redraw_draw_ctx {
	struct redraw_scene	*scene;

	struct window_pane	*active;
	struct window_pane	*marked;

	u_int			 status_lines;
	enum pane_lines		 pane_lines;
	struct grid_cell	 default_gc;

	int			 flags;
#define REDRAW_ISOLATES 0x1
#define REDRAW_DEFAULT_SET 0x2
#define REDRAW_STATUS_TOP 0x4
};

/* Make redraw flags into a string. */
static const char *
redraw_flags_to_string(int flags)
{
	static char	s[128];

	*s = '\0';
	if (flags & REDRAW_STATUS)
		strlcat(s, "status ", sizeof s);
	if (flags & REDRAW_PANE)
		strlcat(s, "pane ", sizeof s);
	if (flags & REDRAW_PANE_BORDER)
		strlcat(s, "border ", sizeof s);
	if (flags & REDRAW_PANE_STATUS)
		strlcat(s, "pane-status ", sizeof s);
	if (flags & REDRAW_PANE_SCROLLBAR)
		strlcat(s, "scrollbar ", sizeof s);
	if (flags & REDRAW_MENU)
		strlcat(s, "menu ", sizeof s);
	if (flags & REDRAW_OVERLAY)
		strlcat(s, "overlay ", sizeof s);
	if (REDRAW_IS_ALL(flags))
		strlcat(s, "all ", sizeof s);
	if (*s != '\0')
		s[strlen(s) - 1] = '\0';
	return (s);
}

/* Get window offset and expand size to cover any part outside the window. */
static void
redraw_get_window_offset(struct client *c, u_int *ox, u_int *oy, u_int *sx,
    u_int *sy)
{
	u_int	tty_sx, tty_sy;

	tty_window_offset(&c->tty, ox, oy, sx, sy);

	tty_sx = c->tty.sx;
	tty_sy = c->tty.sy - status_line_size(c);
	if (*sx < tty_sx)
		*sx = tty_sx;
	if (*sy < tty_sy)
		*sy = tty_sy;
}

/* Initialize the context for building scene. */
static void
redraw_set_context(struct client *c, struct redraw_build_ctx *bctx)
{
	struct session	*s = c->session;
	struct window	*w = s->curw->window;

	memset(bctx, 0, sizeof *bctx);
	bctx->c = c;
	bctx->w = w;
	redraw_get_window_offset(c, &bctx->ox, &bctx->oy, &bctx->sx, &bctx->sy);

	bctx->ind = options_get_number(w->options, "pane-border-indicators");
}

/* Return a cell. */
static struct redraw_build_cell *
redraw_get_build_cell(struct redraw_build_ctx *bctx, u_int x, u_int y)
{
	return (&bctx->cells[(y * bctx->sx) + x]);
}

/* Reset cell to either empty or outside the window. */
static void
redraw_reset_cell(struct redraw_build_ctx *bctx, u_int x, u_int y)
{
	struct redraw_build_cell	*bc = redraw_get_build_cell(bctx, x, y);
	struct window			*w = bctx->w;

	memset(bc, 0, sizeof *bc);
	if (bctx->ox + x <= w->sx && bctx->oy + y <= w->sy)
		bc->data.type = REDRAW_SPAN_EMPTY;
	else
		bc->data.type = REDRAW_SPAN_OUTSIDE;
}

/* Convert window position to scene position. Return 0 if outside the scene. */
static int
redraw_window_to_scene(struct redraw_build_ctx *bctx, int wx, int wy,
    u_int *x, u_int *y)
{
	int	sx, sy;

	if (wx < 0 || wy < 0)
		return (0);
	if ((u_int)wx > bctx->w->sx || (u_int)wy > bctx->w->sy)
		return (0);
	if (wx < (int)bctx->ox || wy < (int)bctx->oy)
		return (0);

	sx = wx - (int)bctx->ox;
	sy = wy - (int)bctx->oy;

	if ((u_int)sx >= bctx->sx || (u_int)sy >= bctx->sy)
		return (0);
	*x = sx;
	*y = sy;
	return (1);
}

/*
 * Convert pane position to scene position. Return 0 if outside the scene. A
 * floating pane is clipped to the window edge.
 */
static int
redraw_pane_to_scene(struct redraw_build_ctx *bctx, struct window_pane *wp,
    int px, int py, u_int *x, u_int *y)
{
	int	wx = wp->xoff + px, wy = wp->yoff + py;
	int	left, right, top, bottom;

	if (window_pane_is_floating(wp)) {
		left = wp->xoff - 1;
		right = wp->xoff + wp->sx;
		top = wp->yoff - 1;
		bottom = wp->yoff + wp->sy;

		if (left < 0 && wx < 0)
			return (0);
		if (right > (int)bctx->w->sx && wx >= (int)bctx->w->sx)
			return (0);
		if (top < 0 && wy < 0)
			return (0);
		if (bottom > (int)bctx->w->sy && wy >= (int)bctx->w->sy)
			return (0);
	}
	return (redraw_window_to_scene(bctx, wx, wy, x, y));
}

/* Convert redraw border mask to a border cell type. */
static int
redraw_get_cell_type(int mask)
{
	switch (mask) {
	case REDRAW_BORDER_L|REDRAW_BORDER_R|REDRAW_BORDER_U|REDRAW_BORDER_D:
		return (CELL_LRUD);
	case REDRAW_BORDER_L|REDRAW_BORDER_R|REDRAW_BORDER_U:
		return (CELL_LRU);
	case REDRAW_BORDER_L|REDRAW_BORDER_R|REDRAW_BORDER_D:
		return (CELL_LRD);
	case REDRAW_BORDER_L|REDRAW_BORDER_R:
	case REDRAW_BORDER_L:
	case REDRAW_BORDER_R:
		return (CELL_LR);
	case REDRAW_BORDER_L|REDRAW_BORDER_U|REDRAW_BORDER_D:
		return (CELL_ULD);
	case REDRAW_BORDER_L|REDRAW_BORDER_U:
		return (CELL_LU);
	case REDRAW_BORDER_L|REDRAW_BORDER_D:
		return (CELL_LD);
	case REDRAW_BORDER_R|REDRAW_BORDER_U|REDRAW_BORDER_D:
		return (CELL_URD);
	case REDRAW_BORDER_R|REDRAW_BORDER_U:
		return (CELL_RU);
	case REDRAW_BORDER_R|REDRAW_BORDER_D:
		return (CELL_RD);
	case REDRAW_BORDER_U|REDRAW_BORDER_D:
	case REDRAW_BORDER_U:
	case REDRAW_BORDER_D:
		return (CELL_UD);
	}
	return (CELL_NONE);
}

/* Return if there are two panes for the border colour indicator. */
static int
redraw_check_two_pane_colours(struct window *w, enum layout_type *type)
{
	struct window_pane	*wp;
	u_int			 count = 0;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (window_pane_is_floating(wp) || wp->layout_cell == NULL)
			continue;
		count++;
		if (count > 2 || wp->layout_cell->parent == NULL)
			return (0);
		*type = wp->layout_cell->parent->type;
	}
	return (count == 2);
}

/* Mark pane inside data. */
static void
redraw_mark_pane_inside(struct redraw_build_ctx *bctx, struct window_pane *wp)
{
	struct redraw_build_cell	*bc;
	u_int				 px, py, x, y;

	for (py = 0; py < wp->sy; py++) {
		for (px = 0; px < wp->sx; px++) {
			if (!redraw_pane_to_scene(bctx, wp, px, py, &x, &y))
				continue;
			bc = redraw_get_build_cell(bctx, x, y);
			memset(bc, 0, sizeof *bc);
			bc->data.type = REDRAW_SPAN_PANE;
			bc->data.p.wp = wp;
			bc->data.p.px = px;
			bc->data.p.py = py;
		}
	}
}

/* Mark scrollbar data. */
static void
redraw_mark_pane_scrollbar(struct redraw_build_ctx *bctx,
    struct window_pane *wp, int sb_w, int sb_left, int overlay)
{
	struct redraw_build_cell	*bc;
	u_int				 x, y;
	int				 wx, wy, sx, ex;
	u_int				 sy;

	if (sb_w == 0)
		return;

	if (overlay && sb_left) {
		sx = wp->xoff;
		ex = sx + sb_w - 1;
	} else if (overlay) {
		ex = wp->xoff + (int)wp->sx - 1;
		sx = ex - sb_w + 1;
	} else if (sb_left) {
		sx = wp->xoff - sb_w;
		ex = wp->xoff - 1;
	} else {
		sx = wp->xoff + (int)wp->sx;
		ex = sx + sb_w - 1;
	}

	for (sy = 0; sy < wp->sy; sy++) {
		wy = wp->yoff + (int)sy;
		for (wx = sx; wx <= ex; wx++) {
			if (!redraw_window_to_scene(bctx, wx, wy, &x,
			    &y))
				continue;
			bc = redraw_get_build_cell(bctx, x, y);
			memset(bc, 0, sizeof *bc);
			bc->data.type = REDRAW_SPAN_SCROLLBAR;
			bc->data.sb.wp = wp;
			bc->data.sb.y = sy;
			bc->data.sb.height = wp->sy;
			if (sb_left)
				bc->data.sb.flags |= REDRAW_SCROLLBAR_LEFT;
			else
				bc->data.sb.flags |= REDRAW_SCROLLBAR_RIGHT;
			if (overlay)
				bc->data.sb.flags |= REDRAW_SCROLLBAR_OVERLAY;
		}
	}
}

/*
 * Return if span data belongs to pane, that is: is the cell adjacent to this
 * pane?
 */
static int
redraw_data_has_pane(struct redraw_span_data *data, struct window_pane *wp)
{
	if (data->b.top_wp == wp)
		return (1);
	if (data->b.bottom_wp == wp)
		return (1);
	if (data->b.left_wp == wp)
		return (1);
	if (data->b.right_wp == wp)
		return (1);
	return (0);
}

/*
 * Mark one border cell. If a non-border cell is marked as a border, replace
 * it. If it is already a border and this is not a floating pane, merge the
 * border mask and pane ownership.
 */
static void
redraw_mark_border_cell(struct redraw_build_ctx *bctx, int wx, int wy,
    struct window_pane *wp, int top_owner, int bottom_owner, int mask,
    enum pane_lines pane_lines, int floating)
{
	struct redraw_build_cell	*bc;
	u_int				 x, y;
	int				 reset = 0;

	if (!redraw_window_to_scene(bctx, wx, wy, &x, &y))
		return;
	bc = redraw_get_build_cell(bctx, x, y);

	/*
	 * If this is a tiled pane, only empty and border cells may be marked.
	 * Border cells are merged and empty cells are updated.
	 *
	 * Floating panes may mark any existing cell type. All cells are reset
	 * except borders that already belong to this pane, they need to be
	 * merged.
	 */
	if (!floating) {
		if (bc->data.type == REDRAW_SPAN_EMPTY)
			reset = 1;
		else if (bc->data.type != REDRAW_SPAN_BORDER)
			return;
	} else {
		if (bc->data.type != REDRAW_SPAN_BORDER ||
		    !redraw_data_has_pane(&bc->data, wp))
			reset = 1;
	}
	if (reset) {
		memset(bc, 0, sizeof *bc);
		bc->data.type = REDRAW_SPAN_BORDER;
	}

	if (top_owner) {
		bc->data.b.top_wp = wp;
		bc->data.b.top_lines = pane_lines;
	}
	if (bottom_owner) {
		bc->data.b.bottom_wp = wp;
		bc->data.b.bottom_lines = pane_lines;
	}

	if (mask & (REDRAW_BORDER_U|REDRAW_BORDER_D)) {
		if (wx < wp->xoff) {
			bc->data.b.right_wp = wp;
			bc->data.b.right_lines = pane_lines;
		} else if (wx >= wp->xoff + (int)wp->sx) {
			bc->data.b.left_wp = wp;
			bc->data.b.left_lines = pane_lines;
		}
	}

	mask |= bc->data.b.cell_mask;
	bc->data.b.cell_mask = mask;
	bc->data.b.cell_type = redraw_get_cell_type(mask);
}

/*
 * Mark border cells for a pane status line, keeping the border cell type for
 * drawing.
 */
static void
redraw_mark_border_status(struct redraw_build_ctx *bctx, struct window_pane *wp,
    __unused int left, int right, int top, int bottom)
{
	struct redraw_build_cell	*bc;
	u_int				 x, y, off = 0;
	int				 pane_status, wy, sx, ex, wx, cell_type;

	pane_status = window_pane_get_pane_status(wp);
	if (pane_status == PANE_STATUS_OFF)
		return;
	if (pane_status == PANE_STATUS_TOP)
		wy = top;
	else
		wy = bottom;

	sx = wp->xoff + 2;
	ex = right - 1;
	if (sx > ex)
		return;

	for (wx = sx; wx <= ex; wx++, off++) {
		if (!redraw_window_to_scene(bctx, wx, wy, &x, &y))
			continue;
		bc = redraw_get_build_cell(bctx, x, y);
		if (bc->data.type != REDRAW_SPAN_BORDER)
			continue;
		cell_type = bc->data.b.cell_type;

		bc->data.type = REDRAW_SPAN_STATUS;
		bc->data.st.wp = wp;
		bc->data.st.offset = off;
		bc->data.st.cell_type = cell_type;
	}
}

/* Mark existing border cells where indicator arrows will be drawn. */
static void
redraw_mark_border_arrows(struct redraw_build_ctx *bctx, struct window_pane *wp,
    int left, int right, int top, int bottom)
{
	struct redraw_build_cell	*bc;
	u_int				 x, y;
	int				 wx, wy;

	if (bctx->ind != PANE_BORDER_ARROWS && bctx->ind != PANE_BORDER_BOTH)
		return;

	wx = wp->xoff + 1;
	if (wx >= left && wx <= right) {
		wy = top;
		if (redraw_window_to_scene(bctx, wx, wy, &x, &y)) {
			bc = redraw_get_build_cell(bctx, x, y);
			if (bc->data.type == REDRAW_SPAN_BORDER)
				bc->data.b.flags |= REDRAW_BORDER_IS_ARROW;
		}
		wy = bottom;
		if (redraw_window_to_scene(bctx, wx, wy, &x, &y)) {
			bc = redraw_get_build_cell(bctx, x, y);
			if (bc->data.type == REDRAW_SPAN_BORDER)
				bc->data.b.flags |= REDRAW_BORDER_IS_ARROW;
		}
	}

	wy = wp->yoff + 1;
	if (wy >= top && wy <= bottom) {
		wx = left;
		if (redraw_window_to_scene(bctx, wx, wy, &x, &y)) {
			bc = redraw_get_build_cell(bctx, x, y);
			if (bc->data.type == REDRAW_SPAN_BORDER)
				bc->data.b.flags |= REDRAW_BORDER_IS_ARROW;
		}
		wx = right;
		if (redraw_window_to_scene(bctx, wx, wy, &x, &y)) {
			bc = redraw_get_build_cell(bctx, x, y);
			if (bc->data.type == REDRAW_SPAN_BORDER)
				bc->data.b.flags |= REDRAW_BORDER_IS_ARROW;
		}
	}
}

/* Mark pane borders. */
static void
redraw_mark_pane_borders(struct redraw_build_ctx *bctx, struct window_pane *wp,
    int sb_w, int sb_left)
{
	enum pane_lines pane_lines = window_pane_get_pane_lines(wp);
	int		pane_status, left, right, top, bottom, wx, wy;
	int		mark_top, mark_bottom, mark_left, mark_right, mask = 0;
	int		floating = window_pane_is_floating(wp);

	if (floating && pane_lines == PANE_LINES_NONE)
		return;
	pane_status = window_pane_get_pane_status(wp);

	left = wp->xoff - 1;
	right = wp->xoff + wp->sx;
	if (sb_w != 0) {
		if (sb_left)
			left -= sb_w;
		else
			right += sb_w;
	}
	top = wp->yoff - 1;
	bottom = wp->yoff + wp->sy;

	mark_left = (left >= 0);
	mark_right = (right <= (int)bctx->w->sx);
	mark_top = (top >= 0);
	mark_bottom = (bottom <= (int)bctx->w->sy);

	if (floating) {
		if (left < 0)
			left = 0;
		if (right > (int)bctx->w->sx)
			right = (int)bctx->w->sx - 1;
		if (top < 0)
			top = 0;
		if (bottom > (int)bctx->w->sy)
			bottom = (int)bctx->w->sy - 1;
	} else {
		if (pane_status == PANE_STATUS_TOP)
			mark_bottom = 0;
		else if (pane_status == PANE_STATUS_BOTTOM)
			mark_top = 0;
	}

	if (mark_top) {
		for (wx = left; wx <= right; wx++) {
			mask = 0;
			if (wx > left)
				mask |= REDRAW_BORDER_L;
			if (wx < right)
				mask |= REDRAW_BORDER_R;
			redraw_mark_border_cell(bctx, wx, top, wp, 0, 1, mask,
			    pane_lines, floating);
		}
	}
	if (mark_bottom) {
		for (wx = left; wx <= right; wx++) {
			mask = 0;
			if (wx > left)
				mask |= REDRAW_BORDER_L;
			if (wx < right)
				mask |= REDRAW_BORDER_R;
			redraw_mark_border_cell(bctx, wx, bottom, wp, 1, 0,
			    mask, pane_lines, floating);
		}
	}
	if (mark_left) {
		for (wy = top; wy <= bottom; wy++) {
			mask = 0;
			if (wy > top)
				mask |= REDRAW_BORDER_U;
			if (wy < bottom)
				mask |= REDRAW_BORDER_D;
			redraw_mark_border_cell(bctx, left, wy, wp, 0, 0, mask,
			    pane_lines, floating);
		}
	}
	if (mark_right) {
		for (wy = top; wy <= bottom; wy++) {
			mask = 0;
			if (wy > top)
				mask |= REDRAW_BORDER_U;
			if (wy < bottom)
				mask |= REDRAW_BORDER_D;
			redraw_mark_border_cell(bctx, right, wy, wp, 0, 0, mask,
			    pane_lines, floating);
		}
	}

	redraw_mark_border_status(bctx, wp, left, right, top, bottom);
	redraw_mark_border_arrows(bctx, wp, left, right, top, bottom);
}

/*
 * Mark an entire pane in the build grid. Floating panes overwrite anything
 * already below them.
 */
static void
redraw_mark_pane(struct redraw_build_ctx *bctx, struct window_pane *wp)
{
	int	sb_w = 0, sb_left = 0, overlay = 0;

	if (!window_pane_is_visible(wp))
		return;

	if (window_pane_scrollbar_visible(wp)) {
		overlay = window_pane_scrollbar_overlay(wp);
		if (overlay) {
			sb_w = wp->scrollbar_style.width +
			    wp->scrollbar_style.pad;
			if (sb_w > (int)wp->sx) {
				sb_w = wp->scrollbar_style.width;
				if (sb_w > (int)wp->sx)
					sb_w = wp->sx;
			}
		} else
			sb_w = wp->scrollbar_style.width + wp->scrollbar_style.pad;
	}
	if (sb_w != 0 && bctx->w->sb_pos == PANE_SCROLLBARS_LEFT)
		sb_left = 1;

	redraw_mark_pane_inside(bctx, wp);
	redraw_mark_pane_borders(bctx, wp, overlay ? 0 : sb_w, sb_left);
	redraw_mark_pane_scrollbar(bctx, wp, sb_w, sb_left, overlay);
}

/* Choose the pane that will provide the border style for two-pane layouts. */
static void
redraw_mark_two_pane_colours(struct redraw_build_ctx *bctx)
{
	struct redraw_build_cell	*bc;
	struct redraw_span_data		*sd;
	enum layout_type		 type;
	u_int				 x, y, wx, wy;

	if (bctx->ind != PANE_BORDER_COLOUR && bctx->ind != PANE_BORDER_BOTH)
		return;
	if (!redraw_check_two_pane_colours(bctx->w, &type))
		return;

	for (y = 0; y < bctx->sy; y++) {
		for (x = 0; x < bctx->sx; x++) {
			bc = redraw_get_build_cell(bctx, x, y);
			if (bc->data.type != REDRAW_SPAN_BORDER)
				continue;
			sd = &bc->data;

			wx = bctx->ox + x;
			wy = bctx->oy + y;

			if (type == LAYOUT_LEFTRIGHT &&
			    sd->b.left_wp != NULL &&
			    sd->b.right_wp != NULL) {
				if (wy <= bctx->w->sy / 2)
					sd->b.style_wp = sd->b.left_wp;
				else
					sd->b.style_wp = sd->b.right_wp;
			} else if (type == LAYOUT_TOPBOTTOM &&
			    sd->b.top_wp != NULL &&
			    sd->b.bottom_wp != NULL) {
				if (wx <= bctx->w->sx / 2)
					sd->b.style_wp = sd->b.top_wp;
				else
					sd->b.style_wp = sd->b.bottom_wp;
			}
		}
	}
}

/* Mark the window menu above all panes. */
static void
redraw_mark_menu(struct redraw_build_ctx *bctx)
{
	struct menu_data		*md = bctx->w->menu;
	struct redraw_build_cell	*bc;
	u_int				 px, py, x, y, sx, sy;

	if (md == NULL)
		return;

	sx = menu_width(md);
	sy = menu_height(md);
	for (py = 0; py < sy; py++) {
		for (px = 0; px < sx; px++) {
			if (!redraw_window_to_scene(bctx, menu_x(md) + px,
			    menu_y(md) + py, &x, &y))
				continue;
			bc = redraw_get_build_cell(bctx, x, y);
			memset(bc, 0, sizeof *bc);
			bc->data.type = REDRAW_SPAN_MENU;
			bc->data.m.md = md;
			bc->data.m.px = px;
			bc->data.m.py = py;
		}
	}
}

/* Return true if two adjacent build cells can be joined into one span. */
static int
redraw_compare_data(struct redraw_build_cell *a, struct redraw_build_cell *b)
{
	struct redraw_span_data	*ad = &a->data, *bd = &b->data;

	if (ad->type != bd->type)
		return (0);

	switch (ad->type) {
	case REDRAW_SPAN_PANE:
		if (ad->p.wp != bd->p.wp ||
		    ad->p.py != bd->p.py ||
		    ad->p.px + 1 != bd->p.px)
			return (0);
		return (1);
	case REDRAW_SPAN_BORDER:
		if (ad->b.top_wp != bd->b.top_wp ||
		    ad->b.bottom_wp != bd->b.bottom_wp ||
		    ad->b.left_wp != bd->b.left_wp ||
		    ad->b.right_wp != bd->b.right_wp ||
		    ad->b.style_wp != bd->b.style_wp ||
		    ad->b.top_lines != bd->b.top_lines ||
		    ad->b.bottom_lines != bd->b.bottom_lines ||
		    ad->b.left_lines != bd->b.left_lines ||
		    ad->b.right_lines != bd->b.right_lines ||
		    ad->b.cell_type != bd->b.cell_type ||
		    ad->b.cell_mask != bd->b.cell_mask ||
		    ad->b.flags != bd->b.flags)
			return (0);
		if (ad->b.flags & REDRAW_BORDER_IS_ARROW)
			return (0);
		return (1);
	case REDRAW_SPAN_STATUS:
		if (ad->st.wp != bd->st.wp ||
		    ad->st.offset + 1 != bd->st.offset ||
		    ad->st.cell_type != bd->st.cell_type)
			return (0);
		return (1);
	case REDRAW_SPAN_SCROLLBAR:
		if (ad->sb.wp != bd->sb.wp ||
		    ad->sb.y != bd->sb.y ||
		    ad->sb.height != bd->sb.height ||
		    ad->sb.flags != bd->sb.flags)
			return (0);
		return (1);
	case REDRAW_SPAN_MENU:
		if (ad->m.md != bd->m.md ||
		    ad->m.py != bd->m.py ||
		    ad->m.px + 1 != bd->m.px)
			return (0);
		return (1);
	case REDRAW_SPAN_OUTSIDE:
	case REDRAW_SPAN_EMPTY:
		return (1);
	}
	return (0);
}

/* Build the temporary cells for a redraw scene. */
static void
redraw_build_cells(struct redraw_build_ctx *bctx)
{
	struct window		*w = bctx->w;
	struct window_pane	*wp;
	size_t			 ncells;
	u_int			 x, y;

	if (bctx->sx != 0 && bctx->sy > SIZE_MAX / bctx->sx)
		fatalx("%s: too many cells", __func__);
	ncells = (size_t)bctx->sx * bctx->sy;
	if (ncells > redraw_ncells) {
		redraw_cells = xreallocarray(redraw_cells, ncells,
		    sizeof *redraw_cells);
		redraw_ncells = ncells;
	}

	bctx->cells = redraw_cells;
	for (y = 0; y < bctx->sy; y++) {
		for (x = 0; x < bctx->sx; x++)
			redraw_reset_cell(bctx, x, y);
	}

	TAILQ_FOREACH_REVERSE(wp, &w->z_index, window_panes_zindex, zentry)
		redraw_mark_pane(bctx, wp);
	redraw_mark_two_pane_colours(bctx);
	redraw_mark_menu(bctx);
}

/*
 * Build and return a redraw scene for a client. The caller owns the scene and
 * must free it with redraw_free_scene.
 */
static struct redraw_scene *
redraw_make_scene(struct client *c)
{
	struct session			*s = c->session;
	struct window			*w = s->curw->window;
	struct redraw_build_ctx		 bctx;
	struct redraw_scene		*scene;
	struct redraw_build_cell	*bc, *last;
	struct redraw_line		*line;
	struct redraw_span		*span;
	enum redraw_span_type		 type;
	u_int				 x, y, x0;

	if (c->flags & CLIENT_SUSPENDED)
		return (NULL);

	redraw_set_context(c, &bctx);

	log_debug("%s: building @%u scene (%ux%u %u,%u; generation %llu)",
	    c->name, w->id, bctx.sx, bctx.sy, bctx.ox, bctx.oy,
	    (unsigned long long)w->redraw_scene_generation);

	redraw_build_cells(&bctx);

	scene = xcalloc(1, sizeof *scene);
	scene->c = c;
	scene->w = w;
	scene->lines = xcalloc(bctx.sy, sizeof *scene->lines);
	scene->generation = w->redraw_scene_generation;
	scene->sx = bctx.sx;
	scene->sy = bctx.sy;
	scene->ox = bctx.ox;
	scene->oy = bctx.oy;

	for (y = 0; y < bctx.sy; y++) {
		line = &scene->lines[y];
		for (type = 0; type < REDRAW_SPAN_TYPES; type++)
			TAILQ_INIT(&line->spans[type]);

		x = 0;
		while (x < bctx.sx) {
			x0 = x;
			last = redraw_get_build_cell(&bctx, x, y);
			x++;

			while (x < bctx.sx) {
				bc = redraw_get_build_cell(&bctx, x, y);
				if (!redraw_compare_data(last, bc))
					break;
				last = bc;
				x++;
			}
			bc = redraw_get_build_cell(&bctx, x0, y);
			type = bc->data.type;

			span = xcalloc(1, sizeof *span);
			span->x = x0;
			span->width = x - x0;
			span->data = bc->data;

			TAILQ_INSERT_TAIL(&line->spans[type], span, entry);
		}
	}

	log_debug("%s: finished building @%u scene", c->name, w->id);
	return (scene);
}

/* Free a scene. */
void
redraw_free_scene(struct redraw_scene *scene)
{
	struct redraw_spans	*spans;
	struct redraw_span	*span, *span1;
	u_int			 y, type;

	if (scene == NULL)
		return;

	for (y = 0; y < scene->sy; y++) {
		for (type = 0; type < REDRAW_SPAN_TYPES; type++) {
			spans = &scene->lines[y].spans[type];
			TAILQ_FOREACH_SAFE(span, spans, entry, span1) {
				TAILQ_REMOVE(spans, span, entry);
				free(span);
			}
		}
	}
	free(scene->lines);
	free(scene);
}

/* Mark a window's cached redraw scenes as out of date. */
void
redraw_invalidate_scene(struct window *w)
{
	w->redraw_scene_generation++;
}

/* Mark all cached redraw scenes as out of date. */
void
redraw_invalidate_all_scenes(void)
{
	struct window	*w;

	RB_FOREACH(w, windows, &windows)
		redraw_invalidate_scene(w);
}

/* Get the cached redraw scene, rebuilding it if needed. */
static struct redraw_scene *
redraw_get_scene(struct client *c)
{
	struct redraw_scene	*scene = c->redraw_scene;
	struct window		*w = c->session->curw->window;
	const char		*reason = NULL;
	u_int			 ox, oy, sx, sy;

	redraw_get_window_offset(c, &ox, &oy, &sx, &sy);
	if (scene == NULL)
		reason = "missing";
	else if (scene->w != w)
		reason = "window changed";
	else if (scene->generation != w->redraw_scene_generation)
		reason = "generation changed";
	else if (scene->ox != ox || scene->oy != oy)
		reason = "offset changed";
	else if (scene->sx != sx || scene->sy != sy)
		reason = "size changed";
	if (reason != NULL) {
		log_debug("%s: @%u scene invalid: %s", c->name, w->id, reason);
		redraw_free_scene(scene);
		scene = redraw_make_scene(c);
		c->redraw_scene = scene;
	}
	return (scene);
}

/* Draw a pane span. */
static void
redraw_draw_pane_span(struct redraw_draw_ctx *dctx,
    struct redraw_span *span, u_int x, u_int y, u_int n)
{
	struct redraw_scene	*scene = dctx->scene;
	struct client		*c = scene->c;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp = span->data.p.wp;
	struct screen		*s = wp->screen;
	struct grid_cell	 defaults;
	struct tty_style_ctx	 style_ctx;
	u_int			 px, py;

	tty_default_colours(&defaults, wp, &style_ctx.dim);
	style_ctx.defaults = &defaults;
	style_ctx.palette = &wp->palette;
	style_ctx.hyperlinks = s->hyperlinks;

	px = span->data.p.px + (x - span->x);
	py = span->data.p.py;
	tty_draw_line(tty, s, px, py, n, x, y, &style_ctx);
}

/* Get default border style for spans without a pane. */
static void
redraw_get_default_border_style(struct redraw_draw_ctx *dctx,
    struct grid_cell *gc, enum pane_lines *pane_lines)
{
	struct redraw_scene	*scene = dctx->scene;
	struct client		*c = scene->c;
	struct session		*s = c->session;
	struct options		*oo = scene->w->options;
	struct format_tree	*ft;
	struct grid_cell	*dgc = &dctx->default_gc;

	if (~dctx->flags & REDRAW_DEFAULT_SET) {
		ft = format_create_defaults(NULL, c, s, s->curw, NULL);
		memcpy(dgc, &grid_default_cell, sizeof *dgc);
		style_add(dgc, oo, "pane-border-style", ft);
		format_free(ft);
		dctx->pane_lines = options_get_number(oo, "pane-border-lines");
		dctx->flags |= REDRAW_DEFAULT_SET;
	}
	memcpy(gc, dgc, sizeof *gc);
	*pane_lines = dctx->pane_lines;
}

/*
 * For this border span, pick the pane whose border style should colour it.
 * Prefer an explicitly assigned style owner, then the active adjacent pane,
 * then any adjacent pane.
 */
static struct window_pane *
redraw_get_pane_for_border_style(struct redraw_draw_ctx *dctx,
    struct redraw_span *span)
{
	struct window_pane	*active = dctx->active;

	if (span->data.type != REDRAW_SPAN_BORDER)
		return (NULL);

	if (span->data.b.style_wp != NULL)
		return (span->data.b.style_wp);
	if (active != NULL && redraw_data_has_pane(&span->data, active))
		return (active);

	if (span->data.b.top_wp != NULL)
		return (span->data.b.top_wp);
	if (span->data.b.bottom_wp != NULL)
		return (span->data.b.bottom_wp);
	if (span->data.b.left_wp != NULL)
		return (span->data.b.left_wp);
	if (span->data.b.right_wp != NULL)
		return (span->data.b.right_wp);
	return (NULL);
}

/* Draw arrow indicator if this border span is an arrow cell. */
static void
redraw_draw_border_arrow(struct redraw_draw_ctx *dctx,
    struct redraw_span *span, struct grid_cell *gc)
{
	struct window_pane	*active = dctx->active;
	char			 ch;

	if (span->data.type != REDRAW_SPAN_BORDER || active == NULL)
		return;
	if (~span->data.b.flags & REDRAW_BORDER_IS_ARROW)
		return;

	if (span->data.b.left_wp == active)
		ch = ',';
	else if (span->data.b.right_wp == active)
		ch = '+';
	else if (span->data.b.top_wp == active)
		ch = '-';
	else if (span->data.b.bottom_wp == active)
		ch = '.';
	else
		return;

	utf8_set(&gc->data, ch);
	gc->attr |= GRID_ATTR_CHARSET;
}

/* Draw a border span. */
static void
redraw_draw_border_span(struct redraw_draw_ctx *dctx,
    struct redraw_span *span, u_int x, u_int y, u_int n)
{
	struct redraw_scene	*scene = dctx->scene;
	struct client		*c = scene->c;
	struct tty		*tty = &c->tty;
	struct window		*w = scene->w;
	struct window_pane	*wp = NULL;
	struct grid_cell	 gc;
	enum pane_lines		 pane_lines;
	u_int			 i, cell_type;
	int			 isolates = 0;

	if (span->data.type != REDRAW_SPAN_BORDER)
		cell_type = CELL_NONE;
	else {
		wp = redraw_get_pane_for_border_style(dctx, span);
		cell_type = span->data.b.cell_type;
	}

	if (wp == NULL) {
		redraw_get_default_border_style(dctx, &gc, &pane_lines);
		window_get_border_cell(w, NULL, pane_lines, cell_type, &gc);
	} else {
		window_pane_get_border_style(wp, c, &gc);
		window_pane_get_border_cell(wp, cell_type, &gc);
	}

	if (span->data.type == REDRAW_SPAN_BORDER &&
	    dctx->marked != NULL &&
	    redraw_data_has_pane(&span->data, dctx->marked))
		gc.attr ^= GRID_ATTR_REVERSE;
	redraw_draw_border_arrow(dctx, span, &gc);

	if (cell_type == CELL_UD && (dctx->flags & REDRAW_ISOLATES))
		isolates = 1;
	tty_cursor(tty, x, y);
	if (isolates)
		tty_puts(tty, REDRAW_END_ISOLATE);
	for (i = 0; i < n; i++)
		tty_cell(tty, &gc, NULL);
	if (isolates)
		tty_puts(tty, REDRAW_START_ISOLATE);
}

/* Draw a pane status span. */
static void
redraw_draw_status_span(struct redraw_draw_ctx *dctx,
    struct redraw_span *span, u_int x, u_int y, u_int n)
{
	struct redraw_scene	*scene = dctx->scene;
	struct client		*c = scene->c;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp = span->data.st.wp;
	struct screen		*s = &wp->status_screen;
	u_int			 px, sx = screen_size_x(s);

	px = span->data.st.offset + (x - span->x);
	if (px < sx) {
		if (n > sx - px)
			n = sx - px;
		tty_draw_line(tty, s, px, 0, n, x, y, NULL);
	}
}

/* Draw a scrollbar span. */
static void
redraw_draw_scrollbar_span(struct redraw_draw_ctx *dctx,
    struct redraw_span *span, u_int x, u_int y, u_int n)
{
	struct redraw_scene	*scene = dctx->scene;
	struct window_pane	*wp = span->data.sb.wp;
	struct screen		*s = wp->screen;
	struct tty		*tty = &scene->c->tty;
	struct style		*sb_style = &wp->scrollbar_style;
	struct grid_cell	 gc, slgc, pad_gc, *gcp;
	double			 pct_view;
	u_int			 total_height, slider_h, slider_y;
	u_int			 sb_h = span->data.sb.height;
	u_int			 sb_y = span->data.sb.y;
	u_int			 i, off, sb_w, sb_pad;
	int			 cm_y, cm_size;

	if (window_pane_mode(wp) == WINDOW_PANE_NO_MODE) {
		total_height = screen_size_y(s) + screen_hsize(s);
		if (total_height == 0)
			return;
		pct_view = (double)sb_h / total_height;
		slider_h = (double)sb_h * pct_view;
		slider_y = sb_h - slider_h;
	} else {
		if (TAILQ_FIRST(&wp->modes) == NULL)
			return;
		if (window_copy_get_current_offset(wp, &cm_y, &cm_size) == 0)
			return;
		total_height = cm_size + sb_h;
		if (total_height == 0)
			return;
		pct_view = (double)sb_h / total_height;
		slider_h = (double)sb_h * pct_view;
		slider_y = (sb_h + 1) * ((double)cm_y / total_height);
	}

	if (slider_h < 1)
		slider_h = 1;
	if (slider_y >= sb_h)
		slider_y = sb_h - 1;

	wp->sb_slider_y = slider_y;
	wp->sb_slider_h = slider_h;

	gc = sb_style->gc;
	memcpy(&slgc, &gc, sizeof slgc);
	slgc.fg = gc.bg;
	slgc.bg = gc.fg;
	tty_default_colours(&pad_gc, wp, NULL);

	sb_w = sb_style->width;
	sb_pad = sb_style->pad;
	off = x - span->x;

	tty_cursor(tty, x, y);
	for (i = 0; i < n; i++) {
		if (span->data.sb.flags & REDRAW_SCROLLBAR_LEFT) {
			if (off + i >= sb_w && off + i < sb_w + sb_pad) {
				tty_cell(tty, &pad_gc, NULL);
				continue;
			}
		} else {
			if (off + i < sb_pad) {
				tty_cell(tty, &pad_gc, NULL);
				continue;
			}
		}

		if (sb_y >= slider_y && sb_y < slider_y + slider_h)
			gcp = &slgc;
		else
			gcp = &gc;
		tty_cell(tty, gcp, NULL);
	}
}

/* Draw a menu span. */
static void
redraw_draw_menu_span(struct redraw_draw_ctx *dctx,
    struct redraw_span *span, u_int x, u_int y, u_int n)
{
	struct redraw_scene	*scene = dctx->scene;
	struct tty		*tty = &scene->c->tty;
	struct screen		*s = menu_screen(span->data.m.md);
	u_int			 px;

	px = span->data.m.px + (x - span->x);
	tty_draw_line(tty, s, px, span->data.m.py, n, x, y, NULL);
}

/* Draw a span. */
static void
redraw_draw_span(struct redraw_draw_ctx *dctx, struct redraw_span *span,
    u_int y)
{
	struct redraw_scene	*scene = dctx->scene;
	struct redraw_span_data	*data = &span->data;
	enum redraw_span_type	 type = data->type;
	struct client		*c = scene->c;
	struct tty		*tty = &c->tty;
	struct visible_ranges	*r;
	struct visible_range	*rr;
	u_int			 i, x, n;

	if (type == REDRAW_SPAN_STATUS && ~data->st.wp->flags & PANE_NEWSTATUS)
		return;

	r = tty_check_overlay_range(tty, span->x, y, span->width);
	for (i = 0; i < r->used; i++) {
		rr = &r->ranges[i];
		if (rr->nx == 0)
			continue;
		x = rr->px;
		n = rr->nx;

		switch (span->data.type) {
		case REDRAW_SPAN_PANE:
			redraw_draw_pane_span(dctx, span, x, y, n);
			break;
		case REDRAW_SPAN_BORDER:
		case REDRAW_SPAN_EMPTY:
		case REDRAW_SPAN_OUTSIDE:
			redraw_draw_border_span(dctx, span, x, y, n);
			break;
		case REDRAW_SPAN_STATUS:
			redraw_draw_status_span(dctx, span, x, y, n);
			break;
		case REDRAW_SPAN_SCROLLBAR:
			redraw_draw_scrollbar_span(dctx, span, x, y, n);
			break;
		case REDRAW_SPAN_MENU:
			redraw_draw_menu_span(dctx, span, x, y, n);
			break;
		}
	}
}

/* Draw pane lines. */
static void
redraw_draw_pane_lines(struct redraw_draw_ctx *dctx, struct window_pane *wp,
    int flags)
{
	struct redraw_scene	*scene = dctx->scene;
	struct redraw_line	*line;
	struct redraw_spans	*spans;
	struct redraw_span	*span;
	u_int			 cy;
	int			 y, top, bottom;

	top = wp->yoff - (int)scene->oy;
	if (top < 0)
		top = 0;
	bottom = wp->yoff + (int)wp->sy - (int)scene->oy;
	if (bottom < 0)
		bottom = 0;
	if (bottom > (int)scene->sy)
		bottom = scene->sy;

	for (y = top; y < bottom; y++) {
		line = &scene->lines[y];
		if (dctx->flags & REDRAW_STATUS_TOP)
			cy = dctx->status_lines + y;
		else
			cy = y;
		if (flags & REDRAW_PANE) {
			spans = &line->spans[REDRAW_SPAN_PANE];
			TAILQ_FOREACH(span, spans, entry) {
				if (span->data.p.wp == wp)
					redraw_draw_span(dctx, span, cy);
			}
		}
		if (flags & REDRAW_PANE_SCROLLBAR) {
			spans = &line->spans[REDRAW_SPAN_SCROLLBAR];
			TAILQ_FOREACH(span, spans, entry) {
				if (span->data.sb.wp == wp)
					redraw_draw_span(dctx, span, cy);
			}
		}
	}
}

/* Draw lines. */
static void
redraw_draw_lines(struct redraw_draw_ctx *dctx, int flags)
{
	struct redraw_scene	*scene = dctx->scene;
	struct redraw_line	*line;
	struct redraw_spans	*spans;
	struct redraw_span	*span;
	u_int			 y, cy, type;

	for (y = 0; y < scene->sy; y++) {
		line = &scene->lines[y];
		if (dctx->flags & REDRAW_STATUS_TOP)
			cy = dctx->status_lines + y;
		else
			cy = y;
		for (type = 0; type < REDRAW_SPAN_TYPES; type++) {
			if (!REDRAW_IS_ALL(flags)) {
				switch (type) {
				case REDRAW_SPAN_PANE:
					if (~flags & REDRAW_PANE)
						continue;
					break;
				case REDRAW_SPAN_OUTSIDE:
					if (~flags & REDRAW_OUTSIDE)
						continue;
					break;
				case REDRAW_SPAN_EMPTY:
					if (~flags & REDRAW_EMPTY)
						continue;
					break;
				case REDRAW_SPAN_BORDER:
					if (~flags & REDRAW_PANE_BORDER)
						continue;
					break;
				case REDRAW_SPAN_STATUS:
					if (~flags & REDRAW_PANE_STATUS)
						continue;
					break;
				case REDRAW_SPAN_SCROLLBAR:
					if (~flags & REDRAW_PANE_SCROLLBAR)
						continue;
					break;
				case REDRAW_SPAN_MENU:
					if (~flags & REDRAW_MENU)
						continue;
					break;
				default:
					continue;
				}
			}
			spans = &line->spans[type];
			TAILQ_FOREACH(span, spans, entry)
				redraw_draw_span(dctx, span, cy);
		}
	}
}

/* Draw menu spans. */
static void
redraw_draw_menu_lines(struct redraw_draw_ctx *dctx)
{
	struct redraw_scene	*scene = dctx->scene;
	struct redraw_line	*line;
	struct redraw_span	*span;
	u_int			 y, cy;

	for (y = 0; y < scene->sy; y++) {
		line = &scene->lines[y];
		if (dctx->flags & REDRAW_STATUS_TOP)
			cy = dctx->status_lines + y;
		else
			cy = y;
		TAILQ_FOREACH(span, &line->spans[REDRAW_SPAN_MENU], entry)
			redraw_draw_span(dctx, span, cy);
	}
}

/* Get line for pane status line. */
static int
redraw_pane_status_line(struct redraw_draw_ctx *dctx,
    struct window_pane *wp, u_int *line)
{
	struct redraw_scene	*scene = dctx->scene;
	int			 pane_status, wy;

	pane_status = window_pane_get_pane_status(wp);
	if (pane_status == PANE_STATUS_OFF)
		return (0);

	if (pane_status == PANE_STATUS_TOP)
		wy = (int)wp->yoff - 1;
	else
		wy = (int)wp->yoff + wp->sy;
	if (wy < 0 || wy < (int)scene->oy)
		return (0);
	if ((u_int)wy >= scene->oy + scene->sy)
		return (0);
	*line = wy - scene->oy;
	return (1);
}

/* Get available width for pane status line. */
static u_int
redraw_pane_status_width(struct redraw_draw_ctx *dctx,
    struct window_pane *wp, struct redraw_span **first)
{
	struct redraw_scene	*scene = dctx->scene;
	struct redraw_span	*span;
	u_int			 y, width = 0, end;

	if (!redraw_pane_status_line(dctx, wp, &y))
		return (0);

	*first = NULL;
	TAILQ_FOREACH(span, &scene->lines[y].spans[REDRAW_SPAN_STATUS], entry) {
		if (span->data.st.wp == wp) {
			if (*first == NULL)
			    *first = span;
			end = span->data.st.offset + span->width;
			if (end > width)
				width = end;
		}
	}
	return (width);
}

/* Set up draw context. */
static void
redraw_set_draw_context(struct redraw_draw_ctx *dctx,
    struct redraw_scene *scene)
{
	struct client	*c = scene->c;
	struct session	*s = c->session;
	struct options	*oo = s->options;
	struct tty	*tty = &c->tty;
	u_int		 lines;

	memset(dctx, 0, sizeof *dctx);
	dctx->scene = scene;

	if (server_is_marked(s, s->curw, marked_pane.wp))
		dctx->marked = marked_pane.wp;
	dctx->active = s->curw->window->active;

	lines = status_line_size(c);
	if (options_get_number(oo, "status-position") == 0)
		dctx->flags |= REDRAW_STATUS_TOP;
	dctx->status_lines = lines;

	if ((c->flags & CLIENT_UTF8) && tty_term_has(tty->term, TTYC_BIDI))
		dctx->flags |= REDRAW_ISOLATES;
}

/* Draw a pane's prompt over its content. */
static void
redraw_draw_pane_prompt(struct redraw_draw_ctx *dctx, struct window_pane *wp)
{
	struct redraw_scene	*scene = dctx->scene;
	struct client		*c = scene->c;
	struct tty		*tty = &c->tty;
	struct screen		 screen;
	struct screen_write_ctx	 ctx;
	struct prompt_draw_data	 pdd;
	int			 ox = scene->ox, oy = scene->oy;
	int			 sx = scene->sx, sy = scene->sy;
	int			 line, cy, px, offset, width, wy;

	if (wp->prompt == NULL || wp->sx == 0 || wp->sy == 0)
		return;

	if (~dctx->flags & REDRAW_STATUS_TOP)
		wy = wp->yoff + (int)wp->sy - 1;
	else
		wy = wp->yoff;
	if (wy < oy || wy >= oy + sy)
		return;
	line = wy - oy;
	if (dctx->flags & REDRAW_STATUS_TOP)
		cy = dctx->status_lines + line;
	else
		cy = line;

	if (wp->xoff + (int)wp->sx <= ox || wp->xoff >= ox + sx)
		return;
	if (wp->xoff < ox) {
		offset = ox - wp->xoff;
		px = 0;
	} else {
		offset = 0;
		px = wp->xoff - ox;
	}
	width = wp->sx - offset;
	if (px + width > sx)
		width = sx - px;

	screen_init(&screen, wp->sx, 1, 0);
	screen_write_start(&ctx, &screen);
	pdd.ctx = &ctx;
	pdd.cursor_x = &wp->prompt_cx;
	pdd.area_x = 0;
	pdd.area_width = wp->sx;
	pdd.prompt_line = 0;
	prompt_draw(wp->prompt, &pdd);
	screen_write_stop(&ctx);

	tty_draw_line(tty, &screen, 0, offset, width, px, cy, NULL);
	screen_free(&screen);
}

/* Draw scene to client. */
static void
redraw_draw(struct client *c, struct window_pane *wp, int flags)
{
	struct redraw_draw_ctx	 dctx;
	struct session		*s = c->session;
	struct window		*w = s->curw->window;
	struct tty		*tty = &c->tty;
	struct screen		*sl;
	struct redraw_scene	*scene;
	struct window_pane	*loop;
	u_int			 width, i, y, lines, j;
	struct redraw_span	*first;
	struct visible_ranges	*r;
	struct visible_range	*rr;
	int			 redraw;

	if (c->flags & CLIENT_SUSPENDED)
		return;

	if (flags & REDRAW_STATUS) {
		if (c->message_string != NULL)
			redraw = status_message_redraw(c);
		else if (c->prompt != NULL)
			redraw = status_prompt_redraw(c);
		else
			redraw = status_redraw(c);
		if (!redraw && !REDRAW_IS_ALL(flags)) {
			flags &= ~REDRAW_STATUS;
			if (flags == 0)
				return;
		}
	}

	if (log_get_level() != 0) {
		log_debug("%s: starting @%u redraw (%s)", c->name, w->id,
		    redraw_flags_to_string(flags));
	}

	scene = redraw_get_scene(c);
	if (scene == NULL)
		return;
	redraw_set_draw_context(&dctx, scene);
	if (w->menu != NULL)
		menu_update(w->menu);

	if (flags & (REDRAW_PANE_BORDER|REDRAW_PANE_STATUS)) {
		TAILQ_FOREACH(loop, &scene->w->panes, entry) {
			loop->border_gc_set = 0;
			loop->active_border_gc_set = 0;
		}
	}

	if (flags & REDRAW_PANE_STATUS) {
		redraw = 0;
		TAILQ_FOREACH(loop, &scene->w->panes, entry) {
			if (REDRAW_IS_ALL(flags))
				loop->flags |= PANE_NEWSTATUS;
			else
				loop->flags &= ~PANE_NEWSTATUS;

			width = redraw_pane_status_width(&dctx, loop,
			    &first);
			if (width == 0)
				continue;

			if (window_make_pane_status(loop, c, width, first)) {
				loop->flags |= PANE_NEWSTATUS;
				redraw = 1;
			}
		}
		if (!redraw && !REDRAW_IS_ALL(flags)) {
			flags &= ~REDRAW_PANE_STATUS;
			if (flags == 0)
				return;
		}
	}

	if (flags & REDRAW_PANE) {
		if (wp != NULL) {
			if (wp->base.mode & MODE_SYNC)
				screen_write_stop_sync(wp);
			screen_write_clear_dirty(wp);
		} else {
			TAILQ_FOREACH(loop, &scene->w->panes, entry) {
				if (!window_pane_is_visible(loop))
					continue;
				if (loop->base.mode & MODE_SYNC)
					screen_write_stop_sync(loop);
				screen_write_clear_dirty(loop);
			}
		}
	}
	tty_sync_start(tty);
	tty_update_mode(tty, tty->mode & ~CURSOR_MODES, NULL);

	if (wp != NULL)
		redraw_draw_pane_lines(&dctx, wp, flags);
	else
		redraw_draw_lines(&dctx, flags);

	if (flags & REDRAW_PANE) {
		if (wp != NULL)
			redraw_draw_pane_prompt(&dctx, wp);
		else {
			TAILQ_FOREACH(loop, &scene->w->panes, entry) {
				if (window_pane_is_visible(loop))
					redraw_draw_pane_prompt(&dctx, loop);
			}
		}
	}
	if (w->menu != NULL && (flags & REDRAW_MENU))
		redraw_draw_menu_lines(&dctx);

	if (flags & REDRAW_STATUS) {
		lines = dctx.status_lines;
		if (c->message_string != NULL || c->prompt != NULL)
			lines = (lines == 0 ? 1 : lines);
		if (dctx.flags & REDRAW_STATUS_TOP)
			y = 0;
		else
			y = c->tty.sy - lines;
		sl = c->status.active;
		for (i = 0; i < lines; i++) {
			r = tty_check_overlay_range(tty, 0, y + i, tty->sx);
			for (j = 0; j < r->used; j++) {
				rr = &r->ranges[j];
				if (rr->nx == 0)
					continue;
				tty_draw_line(tty, sl, rr->px, i, rr->nx,
				    rr->px, y + i, NULL);
			}
		}
	}
	if (c->overlay_draw != NULL && (flags & REDRAW_OVERLAY))
		c->overlay_draw(c, c->overlay_data);

	tty_reset(tty);
	tty_sync_end(tty);

	log_debug("%s: finished @%u redraw", c->name, scene->w->id);

}

/* Get border cell type beneath status cell at offset x in pane status line. */
int
redraw_get_status_border_cell_type(struct redraw_span **spanp, u_int x)
{
	struct redraw_span	*span = *spanp;
	struct window_pane	*wp;
	u_int			 start, end;

	if (span == NULL || span->data.type != REDRAW_SPAN_STATUS)
		return (CELL_LR);
	wp = span->data.st.wp;
	for (; span != NULL; span = TAILQ_NEXT(span, entry)) {
		if (span->data.type != REDRAW_SPAN_STATUS)
			continue;
		if (span->data.st.wp != wp)
			continue;

		start = span->data.st.offset;
		end = start + span->width;
		if (x >= start && x < end) {
			*spanp = span;
			return (span->data.st.cell_type);
		}

		if (start > x) {
			*spanp = span;
			break;
		}
	}
	if (span == NULL)
		*spanp = NULL;
	return (CELL_LR);
}

/* Draw screen. */
void
redraw_screen(struct client *c)
{
	int	flags = 0;

	if (c->flags & CLIENT_REDRAWWINDOW) {
		if (c->flags & CLIENT_REDRAWOVERLAY)
			redraw_draw(c, NULL, REDRAW_ALL);
		else
			redraw_draw(c, NULL, REDRAW_ALL & ~REDRAW_OVERLAY);
	} else {
		if (c->flags & CLIENT_REDRAWBORDERS)
			flags |= (REDRAW_PANE_BORDER|REDRAW_PANE_STATUS);
		if (c->flags & CLIENT_REDRAWSTATUS)
			flags |= (REDRAW_STATUS|REDRAW_PANE_STATUS);
		if (c->flags & CLIENT_REDRAWOVERLAY)
			flags |= REDRAW_OVERLAY;
		if (c->flags & CLIENT_REDRAWMENU)
			flags |= REDRAW_MENU;
		if (c->session->curw->window->menu != NULL)
			flags |= REDRAW_MENU;
		if (flags != 0)
			redraw_draw(c, NULL, flags);
	}
}

/* Draw a single pane. */
void
redraw_pane(struct client *c, struct window_pane *wp)
{
	redraw_draw(c, wp, REDRAW_PANE|REDRAW_PANE_SCROLLBAR);
	if (c->session->curw->window->menu != NULL)
		redraw_draw(c, NULL, REDRAW_MENU);
}

/* Draw a pane's scrollbar. */
void
redraw_pane_scrollbar(struct client *c, struct window_pane *wp)
{
	redraw_draw(c, wp, REDRAW_PANE_SCROLLBAR);
}
