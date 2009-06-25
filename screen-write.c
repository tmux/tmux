/* $OpenBSD: screen-write.c,v 1.4 2009/06/03 23:26:56 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

void	screen_write_save(struct screen_write_ctx *);
void	screen_write_overwrite(struct screen_write_ctx *);

/* Initialise writing with a window. */
void
screen_write_start(
    struct screen_write_ctx *ctx, struct window_pane *wp, struct screen *s)
{
	ctx->wp = wp;
	if (wp != NULL && s == NULL)
		ctx->s = wp->screen;
	else
		ctx->s = s;
}

/* Finish writing. */
void
screen_write_stop(unused struct screen_write_ctx *ctx)
{
}

/* Write character. */
void
screen_write_putc(
    struct screen_write_ctx *ctx, struct grid_cell *gc, u_char ch)
{
	gc->data = ch;
	screen_write_cell(ctx, gc, NULL);
}

/* Calculate string length. */
size_t printflike2
screen_write_strlen(int utf8flag, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;
	u_char *ptr, utf8buf[4];
	size_t	left, size = 0;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (utf8flag && *ptr > 0x7f) {
			memset(utf8buf, 0xff, sizeof utf8buf);

			left = strlen(ptr);
			if (*ptr >= 0xc2 && *ptr <= 0xdf && left >= 2) {
				memcpy(utf8buf, ptr, 2);
				ptr += 2;
			} else if (*ptr >= 0xe0 && *ptr <= 0xef && left >= 3) {
				memcpy(utf8buf, ptr, 3);
				ptr += 3;
			} else if (*ptr >= 0xf0 && *ptr <= 0xf4 && left >= 4) {
				memcpy(utf8buf, ptr, 4);
				ptr += 4;
			} else {
				*utf8buf = *ptr;
				ptr++;
			}
			size += utf8_width(utf8buf);
		} else {
			size++;
			ptr++;
		}
	}	

	return (size);
}

/* Write simple string (no UTF-8 or maximum length). */
void printflike3
screen_write_puts(
    struct screen_write_ctx *ctx, struct grid_cell *gc, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	screen_write_vnputs(ctx, -1, gc, 0, fmt, ap);
	va_end(ap);
}

/* Write string with length limit (-1 for unlimited). */
void printflike5
screen_write_nputs(struct screen_write_ctx *ctx,
    ssize_t maxlen, struct grid_cell *gc, int utf8flag, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	screen_write_vnputs(ctx, maxlen, gc, utf8flag, fmt, ap);
	va_end(ap);
}

void
screen_write_vnputs(struct screen_write_ctx *ctx, ssize_t maxlen,
    struct grid_cell *gc, int utf8flag, const char *fmt, va_list ap)
{
	char   *msg;
	u_char *ptr, utf8buf[4];
	size_t	left, size = 0;
	int	width;

	xvasprintf(&msg, fmt, ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (utf8flag && *ptr > 0x7f) {
			memset(utf8buf, 0xff, sizeof utf8buf);

			left = strlen(ptr);
			if (*ptr >= 0xc2 && *ptr <= 0xdf && left >= 2) {
				memcpy(utf8buf, ptr, 2);
				ptr += 2;
			} else if (*ptr >= 0xe0 && *ptr <= 0xef && left >= 3) {
				memcpy(utf8buf, ptr, 3);
				ptr += 3;
			} else if (*ptr >= 0xf0 && *ptr <= 0xf4 && left >= 4) {
				memcpy(utf8buf, ptr, 4);
				ptr += 4;
			} else {
				*utf8buf = *ptr;
				ptr++;
			}
			
			width = utf8_width(utf8buf);
			if (maxlen > 0 && size + width > (size_t) maxlen) {
				while (size < (size_t) maxlen) {
					screen_write_putc(ctx, gc, ' ');
					size++;
				}
				break;
			}
			size += width;

			gc->flags |= GRID_FLAG_UTF8;
			screen_write_cell(ctx, gc, utf8buf);
			gc->flags &= ~GRID_FLAG_UTF8;

		} else {
			if (maxlen > 0 && size > (size_t) maxlen)
				break;

			size++;
			screen_write_putc(ctx, gc, *ptr);
			ptr++;
		}
	}

	xfree(msg);
}

/* Copy from another screen. */
void
screen_write_copy(struct screen_write_ctx *ctx,
    struct screen *src, u_int px, u_int py, u_int nx, u_int ny)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = src->grid;
	const struct grid_cell	*gc;
	struct grid_utf8	*gu;
	u_char			*udata;
	u_int		 	 xx, yy, cx, cy;

	cx = s->cx;
	cy = s->cy;
	for (yy = py; yy < py + ny; yy++) {
		for (xx = px; xx < px + nx; xx++) {
			if (xx >= gd->sx || yy >= gd->hsize + gd->sy)
				gc = &grid_default_cell;
			else
				gc = grid_peek_cell(gd, xx, yy);

			udata = NULL;
			if (gc->flags & GRID_FLAG_UTF8) {
				gu = grid_get_utf8(gd, xx, yy);
				udata = gu->data;
			}

			screen_write_cell(ctx, gc, udata);
		}
		cy++;
		screen_write_cursormove(ctx, cx, cy);
	}
}

/* Save cursor and region positions. */
void
screen_write_save(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	s->old_cx = s->cx;
	s->old_cy = s->cy;

	s->old_rlower = s->rlower;
	s->old_rupper = s->rupper;
}

/* Cursor up by ny. */
void
screen_write_cursorup(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny == 0)
		ny = 1;

	if (ny > s->cy)
		ny = s->cy;
	if (ny == 0)
		return;

	s->cy -= ny;
}

/* Cursor down by ny. */
void
screen_write_cursordown(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny == 0)
		ny = 1;

	if (ny > screen_size_y(s) - 1 - s->cy)
		ny = screen_size_y(s) - 1 - s->cy;
	if (ny == 0)
		return;

	s->cy += ny;
}

/* Cursor right by nx.  */
void
screen_write_cursorright(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - 1 - s->cx)
		nx = screen_size_x(s) - 1 - s->cx;
	if (nx == 0)
		return;

	s->cx += nx;
}

/* Cursor left by nx. */
void
screen_write_cursorleft(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx == 0)
		nx = 1;

	if (nx > s->cx)
		nx = s->cx;
	if (nx == 0)
		return;

	s->cx -= nx;
}

/* Insert nx characters. */
void
screen_write_insertcharacter(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - 1 - s->cx)
		nx = screen_size_x(s) - 1 - s->cx;
	if (nx == 0)
		return;

	screen_write_save(ctx);

	if (s->cx <= screen_size_x(s) - 1)
		grid_view_insert_cells(s->grid, s->cx, s->cy, nx);

	tty_write_cmd(ctx->wp, TTY_INSERTCHARACTER, nx);
}

/* Delete nx characters. */
void
screen_write_deletecharacter(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - 1 - s->cx)
		nx = screen_size_x(s) - 1 - s->cx;
	if (nx == 0)
		return;

	screen_write_save(ctx);

	if (s->cx <= screen_size_x(s) - 1)
		grid_view_delete_cells(s->grid, s->cx, s->cy, nx);

	tty_write_cmd(ctx->wp, TTY_DELETECHARACTER, nx);
}

/* Insert ny lines. */
void
screen_write_insertline(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny == 0)
		ny = 1;

	if (ny > screen_size_y(s) - 1 - s->cy)
		ny = screen_size_y(s) - 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_save(ctx);

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_insert_lines(s->grid, s->cy, ny);
	else {
		grid_view_insert_lines_region(
		    s->grid, s->rupper, s->rlower, s->cy, ny);
	}

	tty_write_cmd(ctx->wp, TTY_INSERTLINE, ny);
}

/* Delete ny lines. */
void
screen_write_deleteline(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny == 0)
		ny = 1;

	if (ny > screen_size_y(s) - 1 - s->cy)
		ny = screen_size_y(s) - 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_save(ctx);

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_delete_lines(s->grid, s->cy, ny);
	else {
		grid_view_delete_lines_region(
		    s->grid, s->rupper, s->rlower, s->cy, ny);
	}

	tty_write_cmd(ctx->wp, TTY_DELETELINE, ny);
}

/* Clear line at cursor. */
void
screen_write_clearline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	screen_write_save(ctx);

	grid_view_clear(s->grid, 0, s->cy, screen_size_x(s), 1);

	tty_write_cmd(ctx->wp, TTY_CLEARLINE);
}

/* Clear to end of line from cursor. */
void
screen_write_clearendofline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx;

	screen_write_save(ctx);

	sx = screen_size_x(s);

	if (s->cx <= sx - 1)
		grid_view_clear(s->grid, s->cx, s->cy, sx - s->cx, 1);

 	tty_write_cmd(ctx->wp, TTY_CLEARENDOFLINE);
}

/* Clear to start of line from cursor. */
void
screen_write_clearstartofline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx;

	screen_write_save(ctx);

	sx = screen_size_x(s);

	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1);

	tty_write_cmd(ctx->wp, TTY_CLEARSTARTOFLINE);
}

/* Move cursor to px,py.  */
void
screen_write_cursormove(struct screen_write_ctx *ctx, u_int px, u_int py)
{
	struct screen	*s = ctx->s;

	if (px > screen_size_x(s) - 1)
		px = screen_size_x(s) - 1;
	if (py > screen_size_y(s) - 1)
		py = screen_size_y(s) - 1;

	s->cx = px;
	s->cy = py;
}

/* Set cursor mode. */
void
screen_write_cursormode(struct screen_write_ctx *ctx, int state)
{
	struct screen	*s = ctx->s;

	if (state)
		s->mode |= MODE_CURSOR;
	else
		s->mode &= ~MODE_CURSOR;
}

/* Reverse index (up with scroll).  */
void
screen_write_reverseindex(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	screen_write_save(ctx);

	if (s->cy == s->rupper)
		grid_view_scroll_region_down(s->grid, s->rupper, s->rlower);
	else if (s->cy > 0)
		s->cy--;

	tty_write_cmd(ctx->wp, TTY_REVERSEINDEX);
}

/* Set scroll region. */
void
screen_write_scrollregion(
    struct screen_write_ctx *ctx, u_int rupper, u_int rlower)
{
	struct screen	*s = ctx->s;

	if (rupper > screen_size_y(s) - 1)
		rupper = screen_size_y(s) - 1;
	if (rlower > screen_size_y(s) - 1)
		rlower = screen_size_y(s) - 1;
	if (rupper > rlower)
		return;

	/* Cursor moves to top-left. */
	s->cx = 0;
	s->cy = 0;

	s->rupper = rupper;
	s->rlower = rlower;
}

/* Set insert mode. */
void
screen_write_insertmode(struct screen_write_ctx *ctx, int state)
{
	struct screen	*s = ctx->s;

	if (state)
		s->mode |= MODE_INSERT;
	else
		s->mode &= ~MODE_INSERT;
}

/* Set mouse mode.  */
void
screen_write_mousemode(struct screen_write_ctx *ctx, int state)
{
	struct screen	*s = ctx->s;

	if (state)
		s->mode |= MODE_MOUSE;
	else
		s->mode &= ~MODE_MOUSE;
}

/* Line feed (down with scroll). */
void
screen_write_linefeed(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	screen_write_save(ctx);

	if (s->cy == s->rlower)
		grid_view_scroll_region_up(s->grid, s->rupper, s->rlower);
	else if (s->cy < screen_size_y(s) - 1)
		s->cy++;

 	tty_write_cmd(ctx->wp, TTY_LINEFEED);
}

/* Carriage return (cursor to start of line). */
void
screen_write_carriagereturn(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	s->cx = 0;
}

/* Set keypad cursor keys mode. */
void
screen_write_kcursormode(struct screen_write_ctx *ctx, int state)
{
	struct screen	*s = ctx->s;

	if (state)
		s->mode |= MODE_KCURSOR;
	else
		s->mode &= ~MODE_KCURSOR;
}

/* Set keypad number keys mode. */
void
screen_write_kkeypadmode(struct screen_write_ctx *ctx, int state)
{
	struct screen	*s = ctx->s;

	if (state)
		s->mode |= MODE_KKEYPAD;
	else
		s->mode &= ~MODE_KKEYPAD;
}

/* Clear to end of screen from cursor. */
void
screen_write_clearendofscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx, sy;

	screen_write_save(ctx);

	sx = screen_size_x(s);
	sy = screen_size_y(s);

	if (s->cx <= sx - 1)
		grid_view_clear(s->grid, s->cx, s->cy, sx - s->cx, 1);
	grid_view_clear(s->grid, 0, s->cy + 1, sx, sy - (s->cy + 1));

	tty_write_cmd(ctx->wp, TTY_CLEARENDOFSCREEN);
}

/* Clear to start of screen. */
void
screen_write_clearstartofscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx;

	screen_write_save(ctx);

	sx = screen_size_x(s);

	if (s->cy > 0)
		grid_view_clear(s->grid, 0, 0, sx, s->cy);
	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1);

	tty_write_cmd(ctx->wp, TTY_CLEARSTARTOFSCREEN);
}

/* Clear entire screen. */
void
screen_write_clearscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	screen_write_save(ctx);

	grid_view_clear(s->grid, 0, 0, screen_size_x(s), screen_size_y(s));

	tty_write_cmd(ctx->wp, TTY_CLEARSCREEN);
}

/* Write cell data. */
void
screen_write_cell(
    struct screen_write_ctx *ctx, const struct grid_cell *gc, u_char *udata)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_utf8	 gu, *tmp_gu;
	u_int		 	 width, xx, i;
	struct grid_cell 	 tmp_gc, *tmp_gc2;
	size_t			 size;

	/* Ignore padding. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;

	/* Find character width. */
	if (gc->flags & GRID_FLAG_UTF8) {
		width = utf8_width(udata);

		gu.width = width;
		memcpy(&gu.data, udata, sizeof gu.data);
 	} else
		width = 1;

	/* If the width is zero, combine onto the previous character. */
	if (width == 0) {
		if (s->cx == 0)
			return;
		tmp_gc2 = grid_view_get_cell(gd, s->cx - 1, s->cy);
		if (!(tmp_gc2->flags & GRID_FLAG_UTF8)) {
			tmp_gc2->flags |= GRID_FLAG_UTF8;
			memset(&gu.data, 0xff, sizeof gu.data);
			*gu.data = tmp_gc2->data;
			gu.width = 1;
			grid_view_set_utf8(gd, s->cx - 1, s->cy, &gu);
		}
		tmp_gu = grid_view_get_utf8(gd, s->cx - 1, s->cy);

		for (i = 0; i < UTF8_SIZE; i++) {
			if (tmp_gu->data[i] == 0xff)
				break;
		}
		memcpy(tmp_gu->data + i, udata, UTF8_SIZE - i);

		/* Assume the previous character has just been input. */
		for (size = 0; size < UTF8_SIZE; size++) {
			if (udata[size] == 0xff)
				break;
		}
		tty_write_cmd(ctx->wp, TTY_RAW, udata, size);
		return;
	}

	/* If the character is wider than the screen, don't print it. */
	if (width > screen_size_x(s)) {
		memcpy(&tmp_gc, gc, sizeof tmp_gc);
		tmp_gc.data = '_';
		width = 1;
		gc = &tmp_gc;
	}

	/* Check this will fit on the current line; scroll if not. */
	if (s->cx > screen_size_x(s) - width) {
		screen_write_carriagereturn(ctx);
		screen_write_linefeed(ctx);
	}

	/* Sanity checks. */
	if (s->cx > screen_size_x(s) - 1 || s->cy > screen_size_y(s) - 1)
		return;

	/* Handle overwriting of UTF-8 characters. */
	screen_write_overwrite(ctx);

	/*
	 * If the new character is UTF-8 wide, fill in padding cells. Have
	 * already ensured there is enough room.
	 */
	for (xx = s->cx + 1; xx < s->cx + width; xx++) {
		tmp_gc2 = grid_view_get_cell(gd, xx, s->cy);
		if (tmp_gc2 != NULL)
			tmp_gc2->flags |= GRID_FLAG_PADDING;
	}

	/* Set the cell. */
	grid_view_set_cell(gd, s->cx, s->cy, gc);
	if (gc->flags & GRID_FLAG_UTF8)
		grid_view_set_utf8(gd, s->cx, s->cy, &gu);

	/* Move the cursor. */
	screen_write_save(ctx);
	s->cx += width;

	/* Draw to the screen if necessary. */
	if (screen_check_selection(s, s->cx - width, s->cy)) {
		s->sel.cell.data = gc->data;
		tty_write_cmd(ctx->wp, TTY_CELL, &s->sel.cell, &gu);
	} else
		tty_write_cmd(ctx->wp, TTY_CELL, gc, &gu);
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
void
screen_write_overwrite(struct screen_write_ctx *ctx)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	const struct grid_cell	*gc;
	const struct grid_utf8	*gu;
	u_int			 xx;

	gc = grid_view_peek_cell(gd, s->cx, s->cy);
	gu = grid_view_peek_utf8(gd, s->cx, s->cy);

	if (gc->flags & GRID_FLAG_PADDING) {
		/*
		 * A padding cell, so clear any following and leading padding
		 * cells back to the character. Don't overwrite the current
		 * cell as that happens later anyway.
		 */
		xx = s->cx + 1;
		while (--xx > 0) {
			gc = grid_view_peek_cell(gd, xx, s->cy);
			if (!(gc->flags & GRID_FLAG_PADDING))
				break;
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}

		/* Overwrite the character at the start of this padding. */
		grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);

		/* Overwrite following padding cells. */
		xx = s->cx;
		while (++xx < screen_size_x(s)) {
			gc = grid_view_peek_cell(gd, xx, s->cy);
			if (!(gc->flags & GRID_FLAG_PADDING))
				break;
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}
	} else if (gc->flags & GRID_FLAG_UTF8 && gu->width > 1) {
		/*
		 * An UTF-8 wide cell; overwrite following padding cells only.
		 */
		xx = s->cx;
		while (++xx < screen_size_x(s)) {
			gc = grid_view_peek_cell(gd, xx, s->cy);
			if (!(gc->flags & GRID_FLAG_PADDING))
				break;
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}
	}
}
