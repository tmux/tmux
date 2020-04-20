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

#include "tmux.h"

static void	screen_write_collect_clear(struct screen_write_ctx *, u_int,
		    u_int);
static int	screen_write_collect_clear_end(struct screen_write_ctx *, u_int,
		    u_int, u_int);
static int	screen_write_collect_clear_start(struct screen_write_ctx *, u_int,
		    u_int, u_int);
static void	screen_write_collect_scroll(struct screen_write_ctx *);
static void	screen_write_collect_flush(struct screen_write_ctx *, int,
		    const char *);

static int	screen_write_overwrite(struct screen_write_ctx *,
		    struct grid_cell *, u_int);
static const struct grid_cell *screen_write_combine(struct screen_write_ctx *,
		    const struct utf8_data *, u_int *);

static const struct grid_cell screen_write_pad_cell = {
	{ { 0 }, 0, 0, 0 }, 0, GRID_FLAG_PADDING, 0, 8, 8
};

struct screen_write_collect_item {
	u_int			 x;
	int			 wrapped;

	enum { TEXT, CLEAR_END, CLEAR_START } type;
	u_int			 used;
	u_int			 bg;

	struct grid_cell	 gc;

	TAILQ_ENTRY(screen_write_collect_item) entry;
};
struct screen_write_collect_line {
	u_int					 bg;
	char					*data;
	TAILQ_HEAD(, screen_write_collect_item)  items;
};

static void
screen_write_offset_timer(__unused int fd, __unused short events, void *data)
{
	struct window	*w = data;

	tty_update_window_offset(w);
}

/* Set cursor position. */
static void
screen_write_set_cursor(struct screen_write_ctx *ctx, int cx, int cy)
{
	struct window_pane	*wp = ctx->wp;
	struct window		*w;
	struct screen		*s = ctx->s;
	struct timeval		 tv = { .tv_usec = 10000 };

	if (cx != -1 && (u_int)cx == s->cx && cy != -1 && (u_int)cy == s->cy)
		return;

	if (cx != -1) {
		if ((u_int)cx > screen_size_x(s)) /* allow last column */
			cx = screen_size_x(s) - 1;
		s->cx = cx;
	}
	if (cy != -1) {
		if ((u_int)cy > screen_size_y(s) - 1)
			cy = screen_size_y(s) - 1;
		s->cy = cy;
	}

	if (wp == NULL)
		return;
	w = wp->window;

	if (!event_initialized(&w->offset_timer))
		evtimer_set(&w->offset_timer, screen_write_offset_timer, w);
	if (!evtimer_pending(&w->offset_timer, NULL))
		evtimer_add(&w->offset_timer, &tv);
}

/* Set up context for TTY command. */
static void
screen_write_initctx(struct screen_write_ctx *ctx, struct tty_ctx *ttyctx,
    int sync)
{
	struct screen	*s = ctx->s;

	memset(ttyctx, 0, sizeof *ttyctx);

	ttyctx->wp = ctx->wp;

	ttyctx->ocx = s->cx;
	ttyctx->ocy = s->cy;

	ttyctx->orlower = s->rlower;
	ttyctx->orupper = s->rupper;

	if (ctx->wp != NULL &&
	    !ctx->sync &&
	    (sync || ctx->wp != ctx->wp->window->active)) {
		tty_write(tty_cmd_syncstart, ttyctx);
		ctx->sync = 1;
	}
}

/* Make write list. */
void
screen_write_make_list(struct screen *s)
{
	u_int	y;

	s->write_list = xcalloc(screen_size_y(s), sizeof *s->write_list);
	for (y = 0; y < screen_size_y(s); y++)
		TAILQ_INIT(&s->write_list[y].items);
}

/* Free write list. */
void
screen_write_free_list(struct screen *s)
{
	u_int	y;

	for (y = 0; y < screen_size_y(s); y++)
		free(s->write_list[y].data);
	free(s->write_list);
}

/* Initialize writing with a window. */
void
screen_write_start(struct screen_write_ctx *ctx, struct window_pane *wp,
    struct screen *s)
{
	memset(ctx, 0, sizeof *ctx);

	ctx->wp = wp;
	if (wp != NULL && s == NULL)
		ctx->s = wp->screen;
	else
		ctx->s = s;

	if (ctx->s->write_list == NULL)
		screen_write_make_list(ctx->s);
	ctx->item = xcalloc(1, sizeof *ctx->item);

	ctx->scrolled = 0;
	ctx->bg = 8;

	if (log_get_level() != 0) {
		if (wp != NULL) {
			log_debug("%s: size %ux%u, pane %%%u (at %u,%u)",
			    __func__, screen_size_x(ctx->s),
			    screen_size_y(ctx->s), wp->id, wp->xoff, wp->yoff);
		} else {
			log_debug("%s: size %ux%u, no pane",
			    __func__, screen_size_x(ctx->s),
			    screen_size_y(ctx->s));
		}
	}
}

/* Finish writing. */
void
screen_write_stop(struct screen_write_ctx *ctx)
{
	screen_write_collect_end(ctx);
	screen_write_collect_flush(ctx, 0, __func__);

	log_debug("%s: %u cells (%u written, %u skipped)", __func__,
	    ctx->cells, ctx->written, ctx->skipped);
	if (ctx->wp != NULL) {
		ctx->wp->written += ctx->written;
		ctx->wp->skipped += ctx->skipped;
	}

	free(ctx->item);
}

/* Reset screen state. */
void
screen_write_reset(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	screen_reset_tabs(s);
	screen_write_scrollregion(ctx, 0, screen_size_y(s) - 1);

	s->mode = MODE_CURSOR | MODE_WRAP;

	screen_write_clearscreen(ctx, 8);
	screen_write_set_cursor(ctx, 0, 0);
}

/* Write character. */
void
screen_write_putc(struct screen_write_ctx *ctx, const struct grid_cell *gcp,
    u_char ch)
{
	struct grid_cell	gc;

	memcpy(&gc, gcp, sizeof gc);

	utf8_set(&gc.data, ch);
	screen_write_cell(ctx, &gc);
}

/* Calculate string length. */
size_t
screen_write_strlen(const char *fmt, ...)
{
	va_list			ap;
	char   	       	       *msg;
	struct utf8_data	ud;
	u_char 	      	       *ptr;
	size_t			left, size = 0;
	enum utf8_state		more;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (*ptr > 0x7f && utf8_open(&ud, *ptr) == UTF8_MORE) {
			ptr++;

			left = strlen(ptr);
			if (left < (size_t)ud.size - 1)
				break;
			while ((more = utf8_append(&ud, *ptr)) == UTF8_MORE)
				ptr++;
			ptr++;

			if (more == UTF8_DONE)
				size += ud.width;
		} else {
			if (*ptr > 0x1f && *ptr < 0x7f)
				size++;
			ptr++;
		}
	}

	free(msg);
	return (size);
}

/* Write simple string (no UTF-8 or maximum length). */
void
screen_write_puts(struct screen_write_ctx *ctx, const struct grid_cell *gcp,
    const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	screen_write_vnputs(ctx, -1, gcp, fmt, ap);
	va_end(ap);
}

/* Write string with length limit (-1 for unlimited). */
void
screen_write_nputs(struct screen_write_ctx *ctx, ssize_t maxlen,
    const struct grid_cell *gcp, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	screen_write_vnputs(ctx, maxlen, gcp, fmt, ap);
	va_end(ap);
}

void
screen_write_vnputs(struct screen_write_ctx *ctx, ssize_t maxlen,
    const struct grid_cell *gcp, const char *fmt, va_list ap)
{
	struct grid_cell	gc;
	struct utf8_data       *ud = &gc.data;
	char   		       *msg;
	u_char 		       *ptr;
	size_t		 	left, size = 0;
	enum utf8_state		more;

	memcpy(&gc, gcp, sizeof gc);
	xvasprintf(&msg, fmt, ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (*ptr > 0x7f && utf8_open(ud, *ptr) == UTF8_MORE) {
			ptr++;

			left = strlen(ptr);
			if (left < (size_t)ud->size - 1)
				break;
			while ((more = utf8_append(ud, *ptr)) == UTF8_MORE)
				ptr++;
			ptr++;

			if (more != UTF8_DONE)
				continue;
			if (maxlen > 0 && size + ud->width > (size_t)maxlen) {
				while (size < (size_t)maxlen) {
					screen_write_putc(ctx, &gc, ' ');
					size++;
				}
				break;
			}
			size += ud->width;
			screen_write_cell(ctx, &gc);
		} else {
			if (maxlen > 0 && size + 1 > (size_t)maxlen)
				break;

			if (*ptr == '\001')
				gc.attr ^= GRID_ATTR_CHARSET;
			else if (*ptr > 0x1f && *ptr < 0x7f) {
				size++;
				screen_write_putc(ctx, &gc, *ptr);
			}
			ptr++;
		}
	}

	free(msg);
}

/* Copy from another screen. Assumes target region is big enough. */
void
screen_write_copy(struct screen_write_ctx *ctx, struct screen *src, u_int px,
    u_int py, u_int nx, u_int ny, bitstr_t *mbs, const struct grid_cell *mgc)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = src->grid;
	struct grid_cell	 gc;
	u_int		 	 xx, yy, cx, cy, b;

	if (nx == 0 || ny == 0)
		return;

	cx = s->cx;
	cy = s->cy;

	for (yy = py; yy < py + ny; yy++) {
		for (xx = px; xx < px + nx; xx++) {
			grid_get_cell(gd, xx, yy, &gc);
			if (mbs != NULL) {
				b = (yy * screen_size_x(src)) + xx;
				if (bit_test(mbs, b)) {
					gc.attr = mgc->attr;
					gc.fg = mgc->fg;
					gc.bg = mgc->bg;
				}
			}
			if (xx + gc.data.width <= px + nx)
				screen_write_cell(ctx, &gc);
		}
		cy++;
		screen_write_set_cursor(ctx, cx, cy);
	}
}

/*
 * Copy from another screen but without the selection stuff. Also assumes the
 * target region is already big enough.
 */
void
screen_write_fast_copy(struct screen_write_ctx *ctx, struct screen *src,
    u_int px, u_int py, u_int nx, u_int ny)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = src->grid;
	struct grid_cell	 gc;
	u_int		 	 xx, yy, cx, cy;

	if (nx == 0 || ny == 0)
		return;

	cy = s->cy;
	for (yy = py; yy < py + ny; yy++) {
		if (yy >= gd->hsize + gd->sy)
			break;
		cx = s->cx;
		for (xx = px; xx < px + nx; xx++) {
			if (xx >= grid_get_line(gd, yy)->cellsize)
				break;
			grid_get_cell(gd, xx, yy, &gc);
			if (xx + gc.data.width > px + nx)
				break;
			grid_view_set_cell(ctx->s->grid, cx, cy, &gc);
			cx++;
		}
		cy++;
	}
}

/* Draw a horizontal line on screen. */
void
screen_write_hline(struct screen_write_ctx *ctx, u_int nx, int left, int right)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	u_int			 cx, cy, i;

	cx = s->cx;
	cy = s->cy;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.attr |= GRID_ATTR_CHARSET;

	screen_write_putc(ctx, &gc, left ? 't' : 'q');
	for (i = 1; i < nx - 1; i++)
		screen_write_putc(ctx, &gc, 'q');
	screen_write_putc(ctx, &gc, right ? 'u' : 'q');

	screen_write_set_cursor(ctx, cx, cy);
}

/* Draw a horizontal line on screen. */
void
screen_write_vline(struct screen_write_ctx *ctx, u_int ny, int top, int bottom)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	u_int			 cx, cy, i;

	cx = s->cx;
	cy = s->cy;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.attr |= GRID_ATTR_CHARSET;

	screen_write_putc(ctx, &gc, top ? 'w' : 'x');
	for (i = 1; i < ny - 1; i++) {
		screen_write_set_cursor(ctx, cx, cy + i);
		screen_write_putc(ctx, &gc, 'x');
	}
	screen_write_set_cursor(ctx, cx, cy + ny - 1);
	screen_write_putc(ctx, &gc, bottom ? 'v' : 'x');

	screen_write_set_cursor(ctx, cx, cy);
}

/* Draw a menu on screen. */
void
screen_write_menu(struct screen_write_ctx *ctx, struct menu *menu,
    int choice, const struct grid_cell *choice_gc)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 default_gc;
	const struct grid_cell	*gc = &default_gc;
	u_int			 cx, cy, i, j;
	const char		*name;

	cx = s->cx;
	cy = s->cy;

	memcpy(&default_gc, &grid_default_cell, sizeof default_gc);

	screen_write_box(ctx, menu->width + 4, menu->count + 2);
	screen_write_cursormove(ctx, cx + 2, cy, 0);
	format_draw(ctx, &default_gc, menu->width, menu->title, NULL);

	for (i = 0; i < menu->count; i++) {
		name = menu->items[i].name;
		if (name == NULL) {
			screen_write_cursormove(ctx, cx, cy + 1 + i, 0);
			screen_write_hline(ctx, menu->width + 4, 1, 1);
		} else {
			if (choice >= 0 && i == (u_int)choice && *name != '-')
				gc = choice_gc;
			screen_write_cursormove(ctx, cx + 2, cy + 1 + i, 0);
			for (j = 0; j < menu->width; j++)
				screen_write_putc(ctx, gc, ' ');
			screen_write_cursormove(ctx, cx + 2, cy + 1 + i, 0);
			if (*name == '-') {
				name++;
				default_gc.attr |= GRID_ATTR_DIM;
				format_draw(ctx, gc, menu->width, name, NULL);
				default_gc.attr &= ~GRID_ATTR_DIM;
			} else
				format_draw(ctx, gc, menu->width, name, NULL);
			gc = &default_gc;
		}
	}

	screen_write_set_cursor(ctx, cx, cy);
}

/* Draw a box on screen. */
void
screen_write_box(struct screen_write_ctx *ctx, u_int nx, u_int ny)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	u_int			 cx, cy, i;

	cx = s->cx;
	cy = s->cy;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.attr |= GRID_ATTR_CHARSET;

	screen_write_putc(ctx, &gc, 'l');
	for (i = 1; i < nx - 1; i++)
		screen_write_putc(ctx, &gc, 'q');
	screen_write_putc(ctx, &gc, 'k');

	screen_write_set_cursor(ctx, cx, cy + ny - 1);
	screen_write_putc(ctx, &gc, 'm');
	for (i = 1; i < nx - 1; i++)
		screen_write_putc(ctx, &gc, 'q');
	screen_write_putc(ctx, &gc, 'j');

	for (i = 1; i < ny - 1; i++) {
		screen_write_set_cursor(ctx, cx, cy + i);
		screen_write_putc(ctx, &gc, 'x');
	}
	for (i = 1; i < ny - 1; i++) {
		screen_write_set_cursor(ctx, cx + nx - 1, cy + i);
		screen_write_putc(ctx, &gc, 'x');
	}

	screen_write_set_cursor(ctx, cx, cy);
}

/*
 * Write a preview version of a window. Assumes target area is big enough and
 * already cleared.
 */
void
screen_write_preview(struct screen_write_ctx *ctx, struct screen *src, u_int nx,
    u_int ny)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	u_int			 cx, cy, px, py;

	cx = s->cx;
	cy = s->cy;

	/*
	 * If the cursor is on, pick the area around the cursor, otherwise use
	 * the top left.
	 */
	if (src->mode & MODE_CURSOR) {
		px = src->cx;
		if (px < nx / 3)
			px = 0;
		else
			px = px - nx / 3;
		if (px + nx > screen_size_x(src)) {
			if (nx > screen_size_x(src))
				px = 0;
			else
				px = screen_size_x(src) - nx;
		}
		py = src->cy;
		if (py < ny / 3)
			py = 0;
		else
			py = py - ny / 3;
		if (py + ny > screen_size_y(src)) {
			if (ny > screen_size_y(src))
				py = 0;
			else
				py = screen_size_y(src) - ny;
		}
	} else {
		px = 0;
		py = 0;
	}

	screen_write_fast_copy(ctx, src, px, src->grid->hsize + py, nx, ny);

	if (src->mode & MODE_CURSOR) {
		grid_view_get_cell(src->grid, src->cx, src->cy, &gc);
		gc.attr |= GRID_ATTR_REVERSE;
		screen_write_set_cursor(ctx, cx + (src->cx - px),
		    cy + (src->cy - py));
		screen_write_cell(ctx, &gc);
	}
}

/* Set a mode. */
void
screen_write_mode_set(struct screen_write_ctx *ctx, int mode)
{
	struct screen	*s = ctx->s;

	s->mode |= mode;
}

/* Clear a mode. */
void
screen_write_mode_clear(struct screen_write_ctx *ctx, int mode)
{
	struct screen	*s = ctx->s;

	s->mode &= ~mode;
}

/* Cursor up by ny. */
void
screen_write_cursorup(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (ny == 0)
		ny = 1;

	if (cy < s->rupper) {
		/* Above region. */
		if (ny > cy)
			ny = cy;
	} else {
		/* Below region. */
		if (ny > cy - s->rupper)
			ny = cy - s->rupper;
	}
	if (cx == screen_size_x(s))
		cx--;

	cy -= ny;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Cursor down by ny. */
void
screen_write_cursordown(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (ny == 0)
		ny = 1;

	if (cy > s->rlower) {
		/* Below region. */
		if (ny > screen_size_y(s) - 1 - cy)
			ny = screen_size_y(s) - 1 - cy;
	} else {
		/* Above region. */
		if (ny > s->rlower - cy)
			ny = s->rlower - cy;
	}
	if (cx == screen_size_x(s))
	    cx--;
	else if (ny == 0)
		return;

	cy += ny;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Cursor right by nx. */
void
screen_write_cursorright(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - 1 - cx)
		nx = screen_size_x(s) - 1 - cx;
	if (nx == 0)
		return;

	cx += nx;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Cursor left by nx. */
void
screen_write_cursorleft(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (nx == 0)
		nx = 1;

	if (nx > cx)
		nx = cx;
	if (nx == 0)
		return;

	cx -= nx;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Backspace; cursor left unless at start of wrapped line when can move up. */
void
screen_write_backspace(struct screen_write_ctx *ctx)
{
	struct screen		*s = ctx->s;
	struct grid_line	*gl;
	u_int			 cx = s->cx, cy = s->cy;

	if (cx == 0) {
		if (cy == 0)
			return;
		gl = grid_get_line(s->grid, s->grid->hsize + cy - 1);
		if (gl->flags & GRID_LINE_WRAPPED) {
			cy--;
			cx = screen_size_x(s) - 1;
		}
	} else
		cx--;

	screen_write_set_cursor(ctx, cx, cy);
}

/* VT100 alignment test. */
void
screen_write_alignmenttest(struct screen_write_ctx *ctx)
{
	struct screen		*s = ctx->s;
	struct tty_ctx	 	 ttyctx;
	struct grid_cell       	 gc;
	u_int			 xx, yy;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	utf8_set(&gc.data, 'E');

	for (yy = 0; yy < screen_size_y(s); yy++) {
		for (xx = 0; xx < screen_size_x(s); xx++)
			grid_view_set_cell(s->grid, xx, yy, &gc);
	}

	screen_write_set_cursor(ctx, 0, 0);

	s->rupper = 0;
	s->rlower = screen_size_y(s) - 1;

	screen_write_initctx(ctx, &ttyctx, 1);

	screen_write_collect_clear(ctx, 0, screen_size_y(s) - 1);
	tty_write(tty_cmd_alignmenttest, &ttyctx);
}

/* Insert nx characters. */
void
screen_write_insertcharacter(struct screen_write_ctx *ctx, u_int nx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	if (s->cx > screen_size_x(s) - 1)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.bg = bg;

	grid_view_insert_cells(s->grid, s->cx, s->cy, nx, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = nx;
	tty_write(tty_cmd_insertcharacter, &ttyctx);
}

/* Delete nx characters. */
void
screen_write_deletecharacter(struct screen_write_ctx *ctx, u_int nx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	if (s->cx > screen_size_x(s) - 1)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.bg = bg;

	grid_view_delete_cells(s->grid, s->cx, s->cy, nx, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = nx;
	tty_write(tty_cmd_deletecharacter, &ttyctx);
}

/* Clear nx characters. */
void
screen_write_clearcharacter(struct screen_write_ctx *ctx, u_int nx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	if (s->cx > screen_size_x(s) - 1)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.bg = bg;

	grid_view_clear(s->grid, s->cx, s->cy, nx, 1, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = nx;
	tty_write(tty_cmd_clearcharacter, &ttyctx);
}

/* Insert ny lines. */
void
screen_write_insertline(struct screen_write_ctx *ctx, u_int ny, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;

	if (ny == 0)
		ny = 1;

	if (s->cy < s->rupper || s->cy > s->rlower) {
		if (ny > screen_size_y(s) - s->cy)
			ny = screen_size_y(s) - s->cy;
		if (ny == 0)
			return;

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.bg = bg;

		grid_view_insert_lines(gd, s->cy, ny, bg);

		screen_write_collect_flush(ctx, 0, __func__);
		ttyctx.num = ny;
		tty_write(tty_cmd_insertline, &ttyctx);
		return;
	}

	if (ny > s->rlower + 1 - s->cy)
		ny = s->rlower + 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_insert_lines(gd, s->cy, ny, bg);
	else
		grid_view_insert_lines_region(gd, s->rlower, s->cy, ny, bg);

	screen_write_collect_flush(ctx, 0, __func__);

	ttyctx.num = ny;
	tty_write(tty_cmd_insertline, &ttyctx);
}

/* Delete ny lines. */
void
screen_write_deleteline(struct screen_write_ctx *ctx, u_int ny, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;

	if (ny == 0)
		ny = 1;

	if (s->cy < s->rupper || s->cy > s->rlower) {
		if (ny > screen_size_y(s) - s->cy)
			ny = screen_size_y(s) - s->cy;
		if (ny == 0)
			return;

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.bg = bg;

		grid_view_delete_lines(gd, s->cy, ny, bg);

		screen_write_collect_flush(ctx, 0, __func__);
		ttyctx.num = ny;
		tty_write(tty_cmd_deleteline, &ttyctx);
		return;
	}

	if (ny > s->rlower + 1 - s->cy)
		ny = s->rlower + 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_delete_lines(gd, s->cy, ny, bg);
	else
		grid_view_delete_lines_region(gd, s->rlower, s->cy, ny, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = ny;
	tty_write(tty_cmd_deleteline, &ttyctx);
}

/* Clear line at cursor. */
void
screen_write_clearline(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen		 *s = ctx->s;
	struct grid_line	 *gl;
	u_int			  sx = screen_size_x(s);

	gl = grid_get_line(s->grid, s->grid->hsize + s->cy);
	if (gl->cellsize == 0 && COLOUR_DEFAULT(bg))
		return;

	grid_view_clear(s->grid, 0, s->cy, sx, 1, bg);

	screen_write_collect_clear(ctx, s->cy, 1);
	ctx->s->write_list[s->cy].bg = 1 + bg;
	ctx->item->used = 0;
}

/* Clear to end of line from cursor. */
void
screen_write_clearendofline(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen			 *s = ctx->s;
	struct grid_line		 *gl;
	u_int				  sx = screen_size_x(s);
	struct screen_write_collect_item *ci = ctx->item;

	if (s->cx == 0) {
		screen_write_clearline(ctx, bg);
		return;
	}

	gl = grid_get_line(s->grid, s->grid->hsize + s->cy);
	if (s->cx > sx - 1 || (s->cx >= gl->cellsize && COLOUR_DEFAULT(bg)))
		return;

	grid_view_clear(s->grid, s->cx, s->cy, sx - s->cx, 1, bg);

	if (!screen_write_collect_clear_end(ctx, s->cy, s->cx, bg)) {
		ci->x = s->cx;
		ci->type = CLEAR_END;
		ci->bg = bg;
		TAILQ_INSERT_TAIL(&ctx->s->write_list[s->cy].items, ci, entry);
		ctx->item = xcalloc(1, sizeof *ctx->item);
	}
}

/* Clear to start of line from cursor. */
void
screen_write_clearstartofline(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen			 *s = ctx->s;
	u_int				  sx = screen_size_x(s);
	struct screen_write_collect_item *ci = ctx->item;

	if (s->cx >= sx - 1) {
		screen_write_clearline(ctx, bg);
		return;
	}

	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1, bg);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1, bg);

	if (!screen_write_collect_clear_start(ctx, s->cy, s->cx, bg)) {
		ci->x = s->cx;
		ci->type = CLEAR_START;
		ci->bg = bg;
		TAILQ_INSERT_TAIL(&ctx->s->write_list[s->cy].items, ci, entry);
		ctx->item = xcalloc(1, sizeof *ctx->item);
	}
}

/* Move cursor to px,py. */
void
screen_write_cursormove(struct screen_write_ctx *ctx, int px, int py,
    int origin)
{
	struct screen	*s = ctx->s;

	if (origin && py != -1 && (s->mode & MODE_ORIGIN)) {
		if ((u_int)py > s->rlower - s->rupper)
			py = s->rlower;
		else
			py += s->rupper;
	}

	if (px != -1 && (u_int)px > screen_size_x(s) - 1)
		px = screen_size_x(s) - 1;
	if (py != -1 && (u_int)py > screen_size_y(s) - 1)
		py = screen_size_y(s) - 1;

	screen_write_set_cursor(ctx, px, py);
}

/* Reverse index (up with scroll). */
void
screen_write_reverseindex(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (s->cy == s->rupper) {
		grid_view_scroll_region_down(s->grid, s->rupper, s->rlower, bg);
		screen_write_collect_flush(ctx, 0, __func__);

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.bg = bg;

		tty_write(tty_cmd_reverseindex, &ttyctx);
	} else if (s->cy > 0)
		screen_write_set_cursor(ctx, -1, s->cy - 1);

}

/* Set scroll region. */
void
screen_write_scrollregion(struct screen_write_ctx *ctx, u_int rupper,
    u_int rlower)
{
	struct screen	*s = ctx->s;

	if (rupper > screen_size_y(s) - 1)
		rupper = screen_size_y(s) - 1;
	if (rlower > screen_size_y(s) - 1)
		rlower = screen_size_y(s) - 1;
	if (rupper >= rlower)	/* cannot be one line */
		return;

	screen_write_collect_flush(ctx, 0, __func__);

	/* Cursor moves to top-left. */
	screen_write_set_cursor(ctx, 0, 0);

	s->rupper = rupper;
	s->rlower = rlower;
}

/* Line feed. */
void
screen_write_linefeed(struct screen_write_ctx *ctx, int wrapped, u_int bg)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_line	*gl;

	gl = grid_get_line(gd, gd->hsize + s->cy);
	if (wrapped)
		gl->flags |= GRID_LINE_WRAPPED;
	else
		gl->flags &= ~GRID_LINE_WRAPPED;

	log_debug("%s: at %u,%u (region %u-%u)", __func__, s->cx, s->cy,
	    s->rupper, s->rlower);

	if (bg != ctx->bg) {
		screen_write_collect_flush(ctx, 1, __func__);
		ctx->bg = bg;
	}

	if (s->cy == s->rlower) {
		grid_view_scroll_region_up(gd, s->rupper, s->rlower, bg);
		screen_write_collect_scroll(ctx);
		ctx->scrolled++;
	} else if (s->cy < screen_size_y(s) - 1)
		screen_write_set_cursor(ctx, -1, s->cy + 1);
}

/* Scroll up. */
void
screen_write_scrollup(struct screen_write_ctx *ctx, u_int lines, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	u_int		 i;

	if (lines == 0)
		lines = 1;
	else if (lines > s->rlower - s->rupper + 1)
		lines = s->rlower - s->rupper + 1;

	if (bg != ctx->bg) {
		screen_write_collect_flush(ctx, 1, __func__);
		ctx->bg = bg;
	}

	for (i = 0; i < lines; i++) {
		grid_view_scroll_region_up(gd, s->rupper, s->rlower, bg);
		screen_write_collect_scroll(ctx);
	}
	ctx->scrolled += lines;
}

/* Scroll down. */
void
screen_write_scrolldown(struct screen_write_ctx *ctx, u_int lines, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;
	u_int		 i;

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (lines == 0)
		lines = 1;
	else if (lines > s->rlower - s->rupper + 1)
		lines = s->rlower - s->rupper + 1;

	for (i = 0; i < lines; i++)
		grid_view_scroll_region_down(gd, s->rupper, s->rlower, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = lines;
	tty_write(tty_cmd_scrolldown, &ttyctx);
}

/* Carriage return (cursor to start of line). */
void
screen_write_carriagereturn(struct screen_write_ctx *ctx)
{
	screen_write_set_cursor(ctx, 0, -1);
}

/* Clear to end of screen from cursor. */
void
screen_write_clearendofscreen(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;
	u_int		 sx = screen_size_x(s), sy = screen_size_y(s);

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	/* Scroll into history if it is enabled and clearing entire screen. */
	if (s->cx == 0 && s->cy == 0 && (gd->flags & GRID_HISTORY))
		grid_view_clear_history(gd, bg);
	else {
		if (s->cx <= sx - 1)
			grid_view_clear(gd, s->cx, s->cy, sx - s->cx, 1, bg);
		grid_view_clear(gd, 0, s->cy + 1, sx, sy - (s->cy + 1), bg);
	}

	screen_write_collect_clear(ctx, s->cy + 1, sy - (s->cy + 1));
	screen_write_collect_flush(ctx, 0, __func__);
	tty_write(tty_cmd_clearendofscreen, &ttyctx);
}

/* Clear to start of screen. */
void
screen_write_clearstartofscreen(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx = screen_size_x(s);

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (s->cy > 0)
		grid_view_clear(s->grid, 0, 0, sx, s->cy, bg);
	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1, bg);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1, bg);

	screen_write_collect_clear(ctx, 0, s->cy);
	screen_write_collect_flush(ctx, 0, __func__);
	tty_write(tty_cmd_clearstartofscreen, &ttyctx);
}

/* Clear entire screen. */
void
screen_write_clearscreen(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx = screen_size_x(s), sy = screen_size_y(s);

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	/* Scroll into history if it is enabled. */
	if (s->grid->flags & GRID_HISTORY)
		grid_view_clear_history(s->grid, bg);
	else
		grid_view_clear(s->grid, 0, 0, sx, sy, bg);

	screen_write_collect_clear(ctx, 0, sy);
	tty_write(tty_cmd_clearscreen, &ttyctx);
}

/* Clear entire history. */
void
screen_write_clearhistory(struct screen_write_ctx *ctx)
{
	grid_clear_history(ctx->s->grid);
}

/* Clear to start of a collected line. */
static int
screen_write_collect_clear_start(struct screen_write_ctx *ctx, u_int y, u_int x,
    u_int bg)
{
	struct screen_write_collect_item	*ci, *tmp;
	size_t					 size = 0;
	u_int					 items = 0;
	int					 redundant = 0;

	if (TAILQ_EMPTY(&ctx->s->write_list[y].items))
		return (0);
	TAILQ_FOREACH_SAFE(ci, &ctx->s->write_list[y].items, entry, tmp) {
		switch (ci->type) {
		case CLEAR_START:
			if (ci->x >= x) {
				if (ci->bg == bg)
					redundant = 1;
				continue;
			}
			break;
		case CLEAR_END:
			if (ci->x <= x)
				ci->x = x;
			continue;
		case TEXT:
			if (ci->x > x)
				continue;
			break;
		}
		items++;
		size += ci->used;
		TAILQ_REMOVE(&ctx->s->write_list[y].items, ci, entry);
		free(ci);
	}
	ctx->skipped += size;
	log_debug("%s: dropped %u items (%zu bytes) (line %u)", __func__, items,
	    size, y);
	return (redundant);
}

/* Clear to end of a collected line. */
static int
screen_write_collect_clear_end(struct screen_write_ctx *ctx, u_int y, u_int x,
    u_int bg)
{
	struct screen_write_collect_item	*ci, *tmp;
	size_t					 size = 0;
	int					 redundant = 0;
	u_int					 items = 0;

	if (TAILQ_EMPTY(&ctx->s->write_list[y].items))
		return (0);
	TAILQ_FOREACH_SAFE(ci, &ctx->s->write_list[y].items, entry, tmp) {
		switch (ci->type) {
		case CLEAR_START:
			if (ci->x >= x)
				ci->x = x;
			continue;
		case CLEAR_END:
			if (ci->x <= x) {
				if (ci->bg == bg)
					redundant = 1;
				continue;
			}
			break;
		case TEXT:
			if (ci->x < x)
				continue;
			break;
		}
		items++;
		size += ci->used;
		TAILQ_REMOVE(&ctx->s->write_list[y].items, ci, entry);
		free(ci);
	}
	ctx->skipped += size;
	log_debug("%s: dropped %u items (%zu bytes) (line %u)", __func__, items,
	    size, y);
	return (redundant);
}

/* Clear collected lines. */
static void
screen_write_collect_clear(struct screen_write_ctx *ctx, u_int y, u_int n)
{
	struct screen_write_collect_item	*ci, *tmp;
	struct screen_write_collect_line	*cl;
	u_int					 i, items;
	size_t					 size;

	for (i = y; i < y + n; i++) {
		if (TAILQ_EMPTY(&ctx->s->write_list[i].items))
			continue;
		items = 0;
		size = 0;
		cl = &ctx->s->write_list[i];
		TAILQ_FOREACH_SAFE(ci, &cl->items, entry, tmp) {
			items++;
			size += ci->used;
			TAILQ_REMOVE(&cl->items, ci, entry);
			free(ci);
		}
		ctx->skipped += size;
		log_debug("%s: dropped %u items (%zu bytes) (line %u)",
		    __func__, items, size, y);
	}
}

/* Scroll collected lines up. */
static void
screen_write_collect_scroll(struct screen_write_ctx *ctx)
{
	struct screen				*s = ctx->s;
	struct screen_write_collect_line	*cl;
	u_int					 y;
	char					*saved;

	log_debug("%s: at %u,%u (region %u-%u)", __func__, s->cx, s->cy,
	    s->rupper, s->rlower);

	screen_write_collect_clear(ctx, s->rupper, 1);
	saved = ctx->s->write_list[s->rupper].data;
	for (y = s->rupper; y < s->rlower; y++) {
		cl = &ctx->s->write_list[y + 1];
		TAILQ_CONCAT(&ctx->s->write_list[y].items, &cl->items, entry);
		ctx->s->write_list[y].data = cl->data;
	}
	ctx->s->write_list[s->rlower].data = saved;
	ctx->s->write_list[s->rlower].bg = 1 + 8;
}

/* Flush collected lines. */
static void
screen_write_collect_flush(struct screen_write_ctx *ctx, int scroll_only,
    const char *from)
{
	struct screen				*s = ctx->s;
	struct screen_write_collect_item	*ci, *tmp;
	struct screen_write_collect_line	*cl;
	u_int					 y, cx, cy, items = 0;
	struct tty_ctx				 ttyctx;
	size_t					 written = 0;

	if (ctx->scrolled != 0) {
		log_debug("%s: scrolled %u (region %u-%u)", __func__,
		    ctx->scrolled, s->rupper, s->rlower);
		if (ctx->scrolled > s->rlower - s->rupper + 1)
			ctx->scrolled = s->rlower - s->rupper + 1;

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.num = ctx->scrolled;
		ttyctx.bg = ctx->bg;
		tty_write(tty_cmd_scrollup, &ttyctx);
	}
	ctx->scrolled = 0;
	ctx->bg = 8;

	if (scroll_only)
		return;

	cx = s->cx; cy = s->cy;
	for (y = 0; y < screen_size_y(s); y++) {
		cl = &ctx->s->write_list[y];
		if (cl->bg != 0) {
			screen_write_set_cursor(ctx, 0, y);
			screen_write_initctx(ctx, &ttyctx, 1);
			ttyctx.bg = cl->bg - 1;
			tty_write(tty_cmd_clearline, &ttyctx);
		}
		TAILQ_FOREACH_SAFE(ci, &cl->items, entry, tmp) {
			screen_write_set_cursor(ctx, ci->x, y);
			if (ci->type == CLEAR_END) {
				screen_write_initctx(ctx, &ttyctx, 1);
				ttyctx.bg = ci->bg;
				tty_write(tty_cmd_clearendofline, &ttyctx);
			} else if (ci->type == CLEAR_START) {
				screen_write_initctx(ctx, &ttyctx, 1);
				ttyctx.bg = ci->bg;
				tty_write(tty_cmd_clearstartofline, &ttyctx);
			} else {
				screen_write_initctx(ctx, &ttyctx, 0);
				ttyctx.cell = &ci->gc;
				ttyctx.wrapped = ci->wrapped;
				ttyctx.ptr = cl->data + ci->x;
				ttyctx.num = ci->used;
				tty_write(tty_cmd_cells, &ttyctx);
			}

			items++;
			written += ci->used;

			TAILQ_REMOVE(&cl->items, ci, entry);
			free(ci);
		}
		cl->bg = 0;
	}
	s->cx = cx; s->cy = cy;

	log_debug("%s: flushed %u items (%zu bytes) (%s)", __func__, items,
	    written, from);
	ctx->written += written;
}

/* Finish and store collected cells. */
void
screen_write_collect_end(struct screen_write_ctx *ctx)
{
	struct screen				*s = ctx->s;
	struct screen_write_collect_item	*ci = ctx->item;
	struct screen_write_collect_line	*cl = &s->write_list[s->cy];
	struct grid_cell			 gc;
	u_int					 xx;

	if (ci->used == 0)
		return;

	ci->x = s->cx;
	TAILQ_INSERT_TAIL(&cl->items, ci, entry);
	ctx->item = xcalloc(1, sizeof *ctx->item);

	log_debug("%s: %u %.*s (at %u,%u)", __func__, ci->used,
	    (int)ci->used, cl->data + ci->x, s->cx, s->cy);

	if (s->cx != 0) {
		for (xx = s->cx; xx > 0; xx--) {
			grid_view_get_cell(s->grid, xx, s->cy, &gc);
			if (~gc.flags & GRID_FLAG_PADDING)
				break;
			grid_view_set_cell(s->grid, xx, s->cy,
			    &grid_default_cell);
		}
		if (gc.data.width > 1) {
			grid_view_set_cell(s->grid, xx, s->cy,
			    &grid_default_cell);
		}
	}

	grid_view_set_cells(s->grid, s->cx, s->cy, &ci->gc, cl->data + ci->x,
	    ci->used);
	screen_write_set_cursor(ctx, s->cx + ci->used, -1);

	for (xx = s->cx; xx < screen_size_x(s); xx++) {
		grid_view_get_cell(s->grid, xx, s->cy, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
		grid_view_set_cell(s->grid, xx, s->cy, &grid_default_cell);
	}
}

/* Write cell data, collecting if necessary. */
void
screen_write_collect_add(struct screen_write_ctx *ctx,
    const struct grid_cell *gc)
{
	struct screen				*s = ctx->s;
	struct screen_write_collect_item	*ci;
	u_int					 sx = screen_size_x(s);
	int					 collect;

	/*
	 * Don't need to check that the attributes and whatnot are still the
	 * same - input_parse will end the collection when anything that isn't
	 * a plain character is encountered.
	 */

	collect = 1;
	if (gc->data.width != 1 || gc->data.size != 1 || *gc->data.data >= 0x7f)
		collect = 0;
	else if (gc->attr & GRID_ATTR_CHARSET)
		collect = 0;
	else if (~s->mode & MODE_WRAP)
		collect = 0;
	else if (s->mode & MODE_INSERT)
		collect = 0;
	else if (s->sel != NULL)
		collect = 0;
	if (!collect) {
		screen_write_collect_end(ctx);
		screen_write_collect_flush(ctx, 0, __func__);
		screen_write_cell(ctx, gc);
		return;
	}
	ctx->cells++;

	if (s->cx > sx - 1 || ctx->item->used > sx - 1 - s->cx)
		screen_write_collect_end(ctx);
	ci = ctx->item; /* may have changed */

	if (s->cx > sx - 1) {
		log_debug("%s: wrapped at %u,%u", __func__, s->cx, s->cy);
		ci->wrapped = 1;
		screen_write_linefeed(ctx, 1, 8);
		screen_write_set_cursor(ctx, 0, -1);
	}

	if (ci->used == 0)
		memcpy(&ci->gc, gc, sizeof ci->gc);
	if (ctx->s->write_list[s->cy].data == NULL)
		ctx->s->write_list[s->cy].data = xmalloc(screen_size_x(ctx->s));
	ctx->s->write_list[s->cy].data[s->cx + ci->used++] = gc->data.data[0];
}

/* Write cell data. */
void
screen_write_cell(struct screen_write_ctx *ctx, const struct grid_cell *gc)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_line	*gl;
	struct grid_cell_entry	*gce;
	struct grid_cell 	 tmp_gc, now_gc;
	struct tty_ctx		 ttyctx;
	u_int			 sx = screen_size_x(s), sy = screen_size_y(s);
	u_int		 	 width = gc->data.width, xx, last, cx, cy;
	int			 selected, skip = 1;

	/* Ignore padding cells. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;
	ctx->cells++;

	/* If the width is zero, combine onto the previous character. */
	if (width == 0) {
		screen_write_collect_flush(ctx, 0, __func__);
		if ((gc = screen_write_combine(ctx, &gc->data, &xx)) != 0) {
			cx = s->cx; cy = s->cy;
			screen_write_set_cursor(ctx, xx, s->cy);
			screen_write_initctx(ctx, &ttyctx, 0);
			ttyctx.cell = gc;
			tty_write(tty_cmd_cell, &ttyctx);
			s->cx = cx; s->cy = cy;
		}
		return;
	}

	/* Flush any existing scrolling. */
	screen_write_collect_flush(ctx, 1, __func__);

	/* If this character doesn't fit, ignore it. */
	if ((~s->mode & MODE_WRAP) &&
	    width > 1 &&
	    (width > sx || (s->cx != sx && s->cx > sx - width)))
		return;

	/* If in insert mode, make space for the cells. */
	if (s->mode & MODE_INSERT) {
		grid_view_insert_cells(s->grid, s->cx, s->cy, width, 8);
		skip = 0;
	}

	/* Check this will fit on the current line and wrap if not. */
	if ((s->mode & MODE_WRAP) && s->cx > sx - width) {
		log_debug("%s: wrapped at %u,%u", __func__, s->cx, s->cy);
		screen_write_linefeed(ctx, 1, 8);
		screen_write_set_cursor(ctx, 0, -1);
		screen_write_collect_flush(ctx, 1, __func__);
	}

	/* Sanity check cursor position. */
	if (s->cx > sx - width || s->cy > sy - 1)
		return;
	screen_write_initctx(ctx, &ttyctx, 0);

	/* Handle overwriting of UTF-8 characters. */
	gl = grid_get_line(s->grid, s->grid->hsize + s->cy);
	if (gl->flags & GRID_LINE_EXTENDED) {
		grid_view_get_cell(gd, s->cx, s->cy, &now_gc);
		if (screen_write_overwrite(ctx, &now_gc, width))
			skip = 0;
	}

	/*
	 * If the new character is UTF-8 wide, fill in padding cells. Have
	 * already ensured there is enough room.
	 */
	for (xx = s->cx + 1; xx < s->cx + width; xx++) {
		log_debug("%s: new padding at %u,%u", __func__, xx, s->cy);
		grid_view_set_cell(gd, xx, s->cy, &screen_write_pad_cell);
		skip = 0;
	}

	/* If no change, do not draw. */
	if (skip) {
		if (s->cx >= gl->cellsize)
			skip = grid_cells_equal(gc, &grid_default_cell);
		else {
			gce = &gl->celldata[s->cx];
			if (gce->flags & GRID_FLAG_EXTENDED)
				skip = 0;
			else if (gc->flags != gce->flags)
				skip = 0;
			else if (gc->attr != gce->data.attr)
				skip = 0;
			else if (gc->fg != gce->data.fg)
				skip = 0;
			else if (gc->bg != gce->data.bg)
				skip = 0;
			else if (gc->data.width != 1)
				skip = 0;
			else if (gc->data.size != 1)
				skip = 0;
			else if (gce->data.data != gc->data.data[0])
				skip = 0;
		}
	}

	/* Update the selected flag and set the cell. */
	selected = screen_check_selection(s, s->cx, s->cy);
	if (selected && (~gc->flags & GRID_FLAG_SELECTED)) {
		memcpy(&tmp_gc, gc, sizeof tmp_gc);
		tmp_gc.flags |= GRID_FLAG_SELECTED;
		grid_view_set_cell(gd, s->cx, s->cy, &tmp_gc);
	} else if (!selected && (gc->flags & GRID_FLAG_SELECTED)) {
		memcpy(&tmp_gc, gc, sizeof tmp_gc);
		tmp_gc.flags &= ~GRID_FLAG_SELECTED;
		grid_view_set_cell(gd, s->cx, s->cy, &tmp_gc);
	} else if (!skip)
		grid_view_set_cell(gd, s->cx, s->cy, gc);
	if (selected)
		skip = 0;

	/*
	 * Move the cursor. If not wrapping, stick at the last character and
	 * replace it.
	 */
	last = !(s->mode & MODE_WRAP);
	if (s->cx <= sx - last - width)
		screen_write_set_cursor(ctx, s->cx + width, -1);
	else
		screen_write_set_cursor(ctx,  sx - last, -1);

	/* Create space for character in insert mode. */
	if (s->mode & MODE_INSERT) {
		screen_write_collect_flush(ctx, 0, __func__);
		ttyctx.num = width;
		tty_write(tty_cmd_insertcharacter, &ttyctx);
	}

	/* Write to the screen. */
	if (!skip) {
		if (selected) {
			screen_select_cell(s, &tmp_gc, gc);
			ttyctx.cell = &tmp_gc;
		} else
			ttyctx.cell = gc;
		tty_write(tty_cmd_cell, &ttyctx);
		ctx->written++;
	} else
		ctx->skipped++;
}

/* Combine a UTF-8 zero-width character onto the previous. */
static const struct grid_cell *
screen_write_combine(struct screen_write_ctx *ctx, const struct utf8_data *ud,
    u_int *xx)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	static struct grid_cell	 gc;
	u_int			 n;

	/* Can't combine if at 0. */
	if (s->cx == 0)
		return (NULL);

	/* Empty data is out. */
	if (ud->size == 0)
		fatalx("UTF-8 data empty");

	/* Retrieve the previous cell. */
	for (n = 1; n <= s->cx; n++) {
		grid_view_get_cell(gd, s->cx - n, s->cy, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
	}
	if (n > s->cx)
		return (NULL);
	*xx = s->cx - n;

	/* Check there is enough space. */
	if (gc.data.size + ud->size > sizeof gc.data.data)
		return (NULL);

	log_debug("%s: %.*s onto %.*s at %u,%u", __func__, (int)ud->size,
	    ud->data, (int)gc.data.size, gc.data.data, *xx, s->cy);

	/* Append the data. */
	memcpy(gc.data.data + gc.data.size, ud->data, ud->size);
	gc.data.size += ud->size;

	/* Set the new cell. */
	grid_view_set_cell(gd, *xx, s->cy, &gc);

	return (&gc);
}

/*
 * UTF-8 wide characters are a bit of an annoyance. They take up more than one
 * cell on the screen, so following cells must not be drawn by marking them as
 * padding.
 *
 * So far, so good. The problem is, when overwriting a padding cell, or a UTF-8
 * character, it is necessary to also overwrite any other cells which covered
 * by the same character.
 */
static int
screen_write_overwrite(struct screen_write_ctx *ctx, struct grid_cell *gc,
    u_int width)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_cell	 tmp_gc;
	u_int			 xx;
	int			 done = 0;

	if (gc->flags & GRID_FLAG_PADDING) {
		/*
		 * A padding cell, so clear any following and leading padding
		 * cells back to the character. Don't overwrite the current
		 * cell as that happens later anyway.
		 */
		xx = s->cx + 1;
		while (--xx > 0) {
			grid_view_get_cell(gd, xx, s->cy, &tmp_gc);
			if (~tmp_gc.flags & GRID_FLAG_PADDING)
				break;
			log_debug("%s: padding at %u,%u", __func__, xx, s->cy);
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}

		/* Overwrite the character at the start of this padding. */
		log_debug("%s: character at %u,%u", __func__, xx, s->cy);
		grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		done = 1;
	}

	/*
	 * Overwrite any padding cells that belong to any UTF-8 characters
	 * we'll be overwriting with the current character.
	 */
	if (width != 1 ||
	    gc->data.width != 1 ||
	    gc->flags & GRID_FLAG_PADDING) {
		xx = s->cx + width - 1;
		while (++xx < screen_size_x(s)) {
			grid_view_get_cell(gd, xx, s->cy, &tmp_gc);
			if (~tmp_gc.flags & GRID_FLAG_PADDING)
				break;
			log_debug("%s: overwrite at %u,%u", __func__, xx,
			    s->cy);
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
			done = 1;
		}
	}

	return (done);
}

/* Set external clipboard. */
void
screen_write_setselection(struct screen_write_ctx *ctx, u_char *str, u_int len)
{
	struct tty_ctx	ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.ptr = str;
	ttyctx.num = len;

	tty_write(tty_cmd_setselection, &ttyctx);
}

/* Write unmodified string. */
void
screen_write_rawstring(struct screen_write_ctx *ctx, u_char *str, u_int len)
{
	struct tty_ctx	ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.ptr = str;
	ttyctx.num = len;

	tty_write(tty_cmd_rawstring, &ttyctx);
}
