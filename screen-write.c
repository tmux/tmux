/* $OpenBSD$ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

void	screen_write_initctx(struct screen_write_ctx *, struct tty_ctx *, int);
void	screen_write_overwrite(struct screen_write_ctx *, u_int);
int	screen_write_combine(struct screen_write_ctx *,
	    const struct utf8_data *);

/* Initialise writing with a window. */
void
screen_write_start(struct screen_write_ctx *ctx, struct window_pane *wp,
    struct screen *s)
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

/* Reset screen state. */
void
screen_write_reset(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	screen_reset_tabs(s);
	screen_write_scrollregion(ctx, 0, screen_size_y(s) - 1);

	s->mode &= ~(MODE_INSERT|MODE_KCURSOR|MODE_KKEYPAD|MODE_FOCUSON);
	s->mode &= ~(ALL_MOUSE_MODES|MODE_MOUSE_UTF8|MODE_MOUSE_SGR);

	screen_write_clearscreen(ctx);
	screen_write_cursormove(ctx, 0, 0);
}

/* Write character. */
void
screen_write_putc(struct screen_write_ctx *ctx, struct grid_cell *gc,
    u_char ch)
{
	grid_cell_one(gc, ch);
	screen_write_cell(ctx, gc);
}

/* Calculate string length, with embedded formatting. */
size_t
screen_write_cstrlen(int utf8flag, const char *fmt, ...)
{
	va_list	ap;
	char   *msg, *msg2, *ptr, *ptr2;
	size_t	size;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);
	msg2 = xmalloc(strlen(msg) + 1);

	ptr = msg;
	ptr2 = msg2;
	while (*ptr != '\0') {
		if (ptr[0] == '#' && ptr[1] == '[') {
			while (*ptr != ']' && *ptr != '\0')
				ptr++;
			if (*ptr == ']')
				ptr++;
			continue;
		}
		*ptr2++ = *ptr++;
	}
	*ptr2 = '\0';

	size = screen_write_strlen(utf8flag, "%s", msg2);

	free(msg);
	free(msg2);

	return (size);
}

/* Calculate string length. */
size_t
screen_write_strlen(int utf8flag, const char *fmt, ...)
{
	va_list			ap;
	char   	       	       *msg;
	struct utf8_data	utf8data;
	u_char 	      	       *ptr;
	size_t			left, size = 0;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (utf8flag && *ptr > 0x7f && utf8_open(&utf8data, *ptr)) {
			ptr++;

			left = strlen(ptr);
			if (left < utf8data.size - 1)
				break;
			while (utf8_append(&utf8data, *ptr))
				ptr++;
			ptr++;

			size += utf8data.width;
		} else {
			size++;
			ptr++;
		}
	}

	free(msg);
	return (size);
}

/* Write simple string (no UTF-8 or maximum length). */
void
screen_write_puts(struct screen_write_ctx *ctx, struct grid_cell *gc,
    const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	screen_write_vnputs(ctx, -1, gc, 0, fmt, ap);
	va_end(ap);
}

/* Write string with length limit (-1 for unlimited). */
void
screen_write_nputs(struct screen_write_ctx *ctx, ssize_t maxlen,
    struct grid_cell *gc, int utf8flag, const char *fmt, ...)
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
	char   		       *msg;
	struct utf8_data	utf8data;
	u_char 		       *ptr;
	size_t		 	left, size = 0;

	xvasprintf(&msg, fmt, ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (utf8flag && *ptr > 0x7f && utf8_open(&utf8data, *ptr)) {
			ptr++;

			left = strlen(ptr);
			if (left < utf8data.size - 1)
				break;
			while (utf8_append(&utf8data, *ptr))
				ptr++;
			ptr++;

			if (maxlen > 0 &&
			    size + utf8data.width > (size_t) maxlen) {
				while (size < (size_t) maxlen) {
					screen_write_putc(ctx, gc, ' ');
					size++;
				}
				break;
			}
			size += utf8data.width;

			grid_cell_set(gc, &utf8data);
			screen_write_cell(ctx, gc);
		} else {
			if (maxlen > 0 && size + 1 > (size_t) maxlen)
				break;

			if (*ptr == '\001')
				gc->attr ^= GRID_ATTR_CHARSET;
			else {
				size++;
				screen_write_putc(ctx, gc, *ptr);
			}
			ptr++;
		}
	}

	free(msg);
}

/* Write string, similar to nputs, but with embedded formatting (#[]). */
void
screen_write_cnputs(struct screen_write_ctx *ctx,
    ssize_t maxlen, struct grid_cell *gc, int utf8flag, const char *fmt, ...)
{
	struct grid_cell	 lgc;
	struct utf8_data	 utf8data;
	va_list			 ap;
	char			*msg;
	u_char 			*ptr, *last;
	size_t			 left, size = 0;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	memcpy(&lgc, gc, sizeof lgc);

	ptr = msg;
	while (*ptr != '\0') {
		if (ptr[0] == '#' && ptr[1] == '[') {
			ptr += 2;
			last = ptr + strcspn(ptr, "]");
			if (*last == '\0') {
				/* No ]. Not much point in doing anything. */
				break;
			}
			*last = '\0';

			style_parse(gc, &lgc, ptr);
			ptr = last + 1;
			continue;
		}

		if (utf8flag && *ptr > 0x7f && utf8_open(&utf8data, *ptr)) {
			ptr++;

			left = strlen(ptr);
			if (left < utf8data.size - 1)
				break;
			while (utf8_append(&utf8data, *ptr))
				ptr++;
			ptr++;

			if (maxlen > 0 &&
			    size + utf8data.width > (size_t) maxlen) {
				while (size < (size_t) maxlen) {
					screen_write_putc(ctx, gc, ' ');
					size++;
				}
				break;
			}
			size += utf8data.width;

			grid_cell_set(&lgc, &utf8data);
			screen_write_cell(ctx, &lgc);
		} else {
			if (maxlen > 0 && size + 1 > (size_t) maxlen)
				break;

			size++;
			screen_write_putc(ctx, &lgc, *ptr);
			ptr++;
		}
	}

	free(msg);
}

/* Copy from another screen. */
void
screen_write_copy(struct screen_write_ctx *ctx,
    struct screen *src, u_int px, u_int py, u_int nx, u_int ny)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = src->grid;
	struct grid_line	*gl;
	const struct grid_cell	*gc;
	struct utf8_data	 ud;
	u_int		 	 xx, yy, cx, cy, ax, bx;

	cx = s->cx;
	cy = s->cy;
	for (yy = py; yy < py + ny; yy++) {
		gl = &gd->linedata[yy];
		if (yy < gd->hsize + gd->sy) {
			/*
			 * Find start and end position and copy between
			 * them. Limit to the real end of the line then use a
			 * clear EOL only if copying to the end, otherwise
			 * could overwrite whatever is there already.
			 */
			if (px > gl->cellsize)
				ax = gl->cellsize;
			else
				ax = px;
			if (px + nx == gd->sx && px + nx > gl->cellsize)
				bx = gl->cellsize;
			else
				bx = px + nx;

			for (xx = ax; xx < bx; xx++) {
				if (xx >= gl->cellsize)
					gc = &grid_default_cell;
				else
					gc = &gl->celldata[xx];
				grid_cell_get(gc, &ud);
				screen_write_cell(ctx, gc);
			}
			if (px + nx == gd->sx && px + nx > gl->cellsize)
				screen_write_clearendofline(ctx);
		} else
			screen_write_clearline(ctx);
		cy++;
		screen_write_cursormove(ctx, cx, cy);
	}
}

/* Set up context for TTY command. */
void
screen_write_initctx(
    struct screen_write_ctx *ctx, struct tty_ctx *ttyctx, int save_last)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	const struct grid_cell	*gc;
	u_int			 xx;

	ttyctx->wp = ctx->wp;

	ttyctx->ocx = s->cx;
	ttyctx->ocy = s->cy;

	ttyctx->orlower = s->rlower;
	ttyctx->orupper = s->rupper;

	if (!save_last)
		return;

	/* Save the last cell on the screen. */
	gc = &grid_default_cell;
	for (xx = 1; xx <= screen_size_x(s); xx++) {
		gc = grid_view_peek_cell(gd, screen_size_x(s) - xx, s->cy);
		if (!(gc->flags & GRID_FLAG_PADDING))
			break;
	}
	ttyctx->last_width = xx;
	memcpy(&ttyctx->last_cell, gc, sizeof ttyctx->last_cell);
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

	if (ny == 0)
		ny = 1;

	if (s->cy < s->rupper) {
		/* Above region. */
		if (ny > s->cy)
			ny = s->cy;
	} else {
		/* Below region. */
		if (ny > s->cy - s->rupper)
			ny = s->cy - s->rupper;
	}
	if (s->cx == screen_size_x(s))
	    s->cx--;
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

	if (s->cy > s->rlower) {
		/* Below region. */
		if (ny > screen_size_y(s) - 1 - s->cy)
			ny = screen_size_y(s) - 1 - s->cy;
	} else {
		/* Above region. */
		if (ny > s->rlower - s->cy)
			ny = s->rlower - s->cy;
	}
	if (s->cx == screen_size_x(s))
	    s->cx--;
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

/* Backspace; cursor left unless at start of wrapped line when can move up. */
void
screen_write_backspace(struct screen_write_ctx *ctx)
{
	struct screen		*s = ctx->s;
	struct grid_line	*gl;

	if (s->cx == 0) {
		if (s->cy == 0)
			return;
		gl = &s->grid->linedata[s->grid->hsize + s->cy - 1];
		if (gl->flags & GRID_LINE_WRAPPED) {
			s->cy--;
			s->cx = screen_size_x(s) - 1;
		}
	} else
		s->cx--;
}

/* VT100 alignment test. */
void
screen_write_alignmenttest(struct screen_write_ctx *ctx)
{
	struct screen		*s = ctx->s;
	struct tty_ctx	 	 ttyctx;
	struct grid_cell       	 gc;
	u_int			 xx, yy;

	screen_write_initctx(ctx, &ttyctx, 0);

	memcpy(&gc, &grid_default_cell, sizeof gc);
	grid_cell_one(&gc, 'E');

	for (yy = 0; yy < screen_size_y(s); yy++) {
		for (xx = 0; xx < screen_size_x(s); xx++)
			grid_view_set_cell(s->grid, xx, yy, &gc);
	}

	s->cx = 0;
	s->cy = 0;

	s->rupper = 0;

	s->rlower = screen_size_y(s) - 1;

	tty_write(tty_cmd_alignmenttest, &ttyctx);
}

/* Insert nx characters. */
void
screen_write_insertcharacter(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);

	if (s->cx <= screen_size_x(s) - 1)
		grid_view_insert_cells(s->grid, s->cx, s->cy, nx);

	ttyctx.num = nx;
	tty_write(tty_cmd_insertcharacter, &ttyctx);
}

/* Delete nx characters. */
void
screen_write_deletecharacter(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);

	if (s->cx <= screen_size_x(s) - 1)
		grid_view_delete_cells(s->grid, s->cx, s->cy, nx);

	ttyctx.num = nx;
	tty_write(tty_cmd_deletecharacter, &ttyctx);
}

/* Clear nx characters. */
void
screen_write_clearcharacter(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);

	if (s->cx <= screen_size_x(s) - 1)
		grid_view_clear(s->grid, s->cx, s->cy, nx, 1);

	ttyctx.num = nx;
	tty_write(tty_cmd_clearcharacter, &ttyctx);
}

/* Insert ny lines. */
void
screen_write_insertline(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (ny == 0)
		ny = 1;

	if (s->cy < s->rupper || s->cy > s->rlower) {
		if (ny > screen_size_y(s) - s->cy)
			ny = screen_size_y(s) - s->cy;
		if (ny == 0)
			return;

		screen_write_initctx(ctx, &ttyctx, 0);

		grid_view_insert_lines(s->grid, s->cy, ny);

		ttyctx.num = ny;
		tty_write(tty_cmd_insertline, &ttyctx);
		return;
	}

	if (ny > s->rlower + 1 - s->cy)
		ny = s->rlower + 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_insert_lines(s->grid, s->cy, ny);
	else
		grid_view_insert_lines_region(s->grid, s->rlower, s->cy, ny);

	ttyctx.num = ny;
	tty_write(tty_cmd_insertline, &ttyctx);
}

/* Delete ny lines. */
void
screen_write_deleteline(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (ny == 0)
		ny = 1;

	if (s->cy < s->rupper || s->cy > s->rlower) {
		if (ny > screen_size_y(s) - s->cy)
			ny = screen_size_y(s) - s->cy;
		if (ny == 0)
			return;

		screen_write_initctx(ctx, &ttyctx, 0);

		grid_view_delete_lines(s->grid, s->cy, ny);

		ttyctx.num = ny;
		tty_write(tty_cmd_deleteline, &ttyctx);
		return;
	}

	if (ny > s->rlower + 1 - s->cy)
		ny = s->rlower + 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 0);

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_delete_lines(s->grid, s->cy, ny);
	else
		grid_view_delete_lines_region(s->grid, s->rlower, s->cy, ny);

	ttyctx.num = ny;
	tty_write(tty_cmd_deleteline, &ttyctx);
}

/* Clear line at cursor. */
void
screen_write_clearline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);

	grid_view_clear(s->grid, 0, s->cy, screen_size_x(s), 1);

	tty_write(tty_cmd_clearline, &ttyctx);
}

/* Clear to end of line from cursor. */
void
screen_write_clearendofline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx;

	screen_write_initctx(ctx, &ttyctx, 0);

	sx = screen_size_x(s);

	if (s->cx <= sx - 1)
		grid_view_clear(s->grid, s->cx, s->cy, sx - s->cx, 1);

	tty_write(tty_cmd_clearendofline, &ttyctx);
}

/* Clear to start of line from cursor. */
void
screen_write_clearstartofline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx;

	screen_write_initctx(ctx, &ttyctx, 0);

	sx = screen_size_x(s);

	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1);

	tty_write(tty_cmd_clearstartofline, &ttyctx);
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

/* Reverse index (up with scroll).  */
void
screen_write_reverseindex(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);

	if (s->cy == s->rupper)
		grid_view_scroll_region_down(s->grid, s->rupper, s->rlower);
	else if (s->cy > 0)
		s->cy--;

	tty_write(tty_cmd_reverseindex, &ttyctx);
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
	if (rupper >= rlower)	/* cannot be one line */
		return;

	/* Cursor moves to top-left. */
	s->cx = 0;
	s->cy = 0;

	s->rupper = rupper;
	s->rlower = rlower;
}

/* Line feed. */
void
screen_write_linefeed(struct screen_write_ctx *ctx, int wrapped)
{
	struct screen		*s = ctx->s;
	struct grid_line	*gl;
	struct tty_ctx	 	 ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);

	gl = &s->grid->linedata[s->grid->hsize + s->cy];
	if (wrapped)
		gl->flags |= GRID_LINE_WRAPPED;
	else
		gl->flags &= ~GRID_LINE_WRAPPED;

	if (s->cy == s->rlower)
		grid_view_scroll_region_up(s->grid, s->rupper, s->rlower);
	else if (s->cy < screen_size_y(s) - 1)
		s->cy++;

	ttyctx.num = wrapped;
	tty_write(tty_cmd_linefeed, &ttyctx);
}

/* Carriage return (cursor to start of line). */
void
screen_write_carriagereturn(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	s->cx = 0;
}

/* Clear to end of screen from cursor. */
void
screen_write_clearendofscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx, sy;

	screen_write_initctx(ctx, &ttyctx, 0);

	sx = screen_size_x(s);
	sy = screen_size_y(s);

	/* Scroll into history if it is enabled and clearing entire screen. */
	if (s->cy == 0 && s->grid->flags & GRID_HISTORY)
		grid_view_clear_history(s->grid);
	else {
		if (s->cx <= sx - 1)
			grid_view_clear(s->grid, s->cx, s->cy, sx - s->cx, 1);
		grid_view_clear(s->grid, 0, s->cy + 1, sx, sy - (s->cy + 1));
	}

	tty_write(tty_cmd_clearendofscreen, &ttyctx);
}

/* Clear to start of screen. */
void
screen_write_clearstartofscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx;

	screen_write_initctx(ctx, &ttyctx, 0);

	sx = screen_size_x(s);

	if (s->cy > 0)
		grid_view_clear(s->grid, 0, 0, sx, s->cy);
	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1);

	tty_write(tty_cmd_clearstartofscreen, &ttyctx);
}

/* Clear entire screen. */
void
screen_write_clearscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);

	/* Scroll into history if it is enabled. */
	if (s->grid->flags & GRID_HISTORY)
		grid_view_clear_history(s->grid);
	else {
		grid_view_clear(
		    s->grid, 0, 0, screen_size_x(s), screen_size_y(s));
	}

	tty_write(tty_cmd_clearscreen, &ttyctx);
}

/* Clear entire history. */
void
screen_write_clearhistory(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;

	grid_move_lines(gd, 0, gd->hsize, gd->sy);
	gd->hsize = 0;
}

/* Write cell data. */
void
screen_write_cell(struct screen_write_ctx *ctx, const struct grid_cell *gc)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct tty_ctx		 ttyctx;
	u_int		 	 width, xx, last;
	struct grid_cell 	 tmp_gc, *tmp_gcp;
	struct utf8_data	 ud;
	int			 insert;

	/* Ignore padding. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;
	width = grid_cell_width(gc);

	/*
	 * If this is a wide character and there is no room on the screen, for
	 * the entire character, don't print it.
	 */
	if (!(s->mode & MODE_WRAP)
	    && (width > 1 && (width > screen_size_x(s) ||
		(s->cx != screen_size_x(s)
		 && s->cx > screen_size_x(s) - width))))
		return;

	/*
	 * If the width is zero, combine onto the previous character, if
	 * there is space.
	 */
	if (width == 0) {
		grid_cell_get(gc, &ud);
		if (screen_write_combine(ctx, &ud) == 0) {
			screen_write_initctx(ctx, &ttyctx, 0);
			tty_write(tty_cmd_utf8character, &ttyctx);
		}
		return;
	}

	/* Initialise the redraw context, saving the last cell. */
	screen_write_initctx(ctx, &ttyctx, 1);

	/* If in insert mode, make space for the cells. */
	if ((s->mode & MODE_INSERT) && s->cx <= screen_size_x(s) - width) {
		xx = screen_size_x(s) - s->cx - width;
		grid_move_cells(s->grid, s->cx + width, s->cx, s->cy, xx);
		insert = 1;
	} else
		insert = 0;

	/* Check this will fit on the current line and wrap if not. */
	if ((s->mode & MODE_WRAP) && s->cx > screen_size_x(s) - width) {
		screen_write_linefeed(ctx, 1);
		s->cx = 0;	/* carriage return */
	}

	/* Sanity check cursor position. */
	if (s->cx > screen_size_x(s) - width || s->cy > screen_size_y(s) - 1)
		return;

	/* Handle overwriting of UTF-8 characters. */
	screen_write_overwrite(ctx, width);

	/*
	 * If the new character is UTF-8 wide, fill in padding cells. Have
	 * already ensured there is enough room.
	 */
	for (xx = s->cx + 1; xx < s->cx + width; xx++) {
		tmp_gcp = grid_view_get_cell(gd, xx, s->cy);
		if (tmp_gcp != NULL)
			tmp_gcp->flags |= GRID_FLAG_PADDING;
	}

	/* Set the cell. */
	grid_view_set_cell(gd, s->cx, s->cy, gc);

	/*
	 * Move the cursor. If not wrapping, stick at the last character and
	 * replace it.
	 */
	last = !(s->mode & MODE_WRAP);
	if (s->cx <= screen_size_x(s) - last - width)
		s->cx += width;
	else
		s->cx = screen_size_x(s) - last;

	/* Draw to the screen if necessary. */
	if (insert) {
		ttyctx.num = width;
		tty_write(tty_cmd_insertcharacter, &ttyctx);
	}
	if (screen_check_selection(s, s->cx - width, s->cy)) {
		memcpy(&tmp_gc, &s->sel.cell, sizeof tmp_gc);
		grid_cell_get(gc, &ud);
		grid_cell_set(&tmp_gc, &ud);
		tmp_gc.attr = tmp_gc.attr & ~GRID_ATTR_CHARSET;
		tmp_gc.attr |= gc->attr & GRID_ATTR_CHARSET;
		tmp_gc.flags = gc->flags & ~(GRID_FLAG_FG256|GRID_FLAG_BG256);
		tmp_gc.flags |= s->sel.cell.flags &
		    (GRID_FLAG_FG256|GRID_FLAG_BG256);
		ttyctx.cell = &tmp_gc;
		tty_write(tty_cmd_cell, &ttyctx);
	} else {
		ttyctx.cell = gc;
		tty_write(tty_cmd_cell, &ttyctx);
	}
}

/* Combine a UTF-8 zero-width character onto the previous. */
int
screen_write_combine(struct screen_write_ctx *ctx, const struct utf8_data *ud)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_cell	*gc;
	struct utf8_data	 ud1;

	/* Can't combine if at 0. */
	if (s->cx == 0)
		return (-1);

	/* Empty data is out. */
	if (ud->size == 0)
		fatalx("UTF-8 data empty");

	/* Retrieve the previous cell. */
	gc = grid_view_get_cell(gd, s->cx - 1, s->cy);
	grid_cell_get(gc, &ud1);

	/* Check there is enough space. */
	if (ud1.size + ud->size > sizeof ud1.data)
		return (-1);

	/* Append the data and set the cell. */
	memcpy(ud1.data + ud1.size, ud->data, ud->size);
	ud1.size += ud->size;
	grid_cell_set(gc, &ud1);

	return (0);
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
screen_write_overwrite(struct screen_write_ctx *ctx, u_int width)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	const struct grid_cell	*gc;
	u_int			 xx;

	gc = grid_view_peek_cell(gd, s->cx, s->cy);
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
	}

	/*
	 * Overwrite any padding cells that belong to a UTF-8 character
	 * we'll be overwriting with the current character.
	 */
	xx = s->cx + width - 1;
	while (++xx < screen_size_x(s)) {
		gc = grid_view_peek_cell(gd, xx, s->cy);
		if (!(gc->flags & GRID_FLAG_PADDING))
			break;
		grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
	}
}

void
screen_write_setselection(struct screen_write_ctx *ctx, u_char *str, u_int len)
{
	struct tty_ctx	ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.ptr = str;
	ttyctx.num = len;

	tty_write(tty_cmd_setselection, &ttyctx);
}

void
screen_write_rawstring(struct screen_write_ctx *ctx, u_char *str, u_int len)
{
	struct tty_ctx		 ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.ptr = str;
	ttyctx.num = len;

	tty_write(tty_cmd_rawstring, &ttyctx);
}
