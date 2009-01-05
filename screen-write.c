/* $Id: screen-write.c,v 1.18 2009-01-05 11:04:06 nicm Exp $ */

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

/* Initialise writing with a window. */
void
screen_write_start_window(struct screen_write_ctx *ctx, struct window *w)
{
	struct screen	*t = w->screen;

	screen_write_start(ctx, t, tty_write_window, w);
}

/* Initialise writing with a client. */
void
screen_write_start_client(struct screen_write_ctx *ctx, struct client *c)
{
	struct screen	*t = c->session->curw->window->screen;

	screen_write_start(ctx, t, tty_write_client, c);
}

/* Initialise writing with a session. */
void
screen_write_start_session(struct screen_write_ctx *ctx, struct session *s)
{
	struct screen	*t = s->curw->window->screen;

	screen_write_start(ctx, t, tty_write_session, s);
}

/* Initialise writing. */
void
screen_write_start(struct screen_write_ctx *ctx,
    struct screen *s, void (*write)(void *, int, ...), void *data)
{
	ctx->write = write;
	ctx->data = data;

	ctx->s = s;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMODE, 0);
}

/* Finish writing. */
void
screen_write_stop(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	if (ctx->write != NULL && s->mode & MODE_CURSOR)
		ctx->write(ctx->data, TTY_CURSORMODE, 1);
}

/* Write character. */
void
screen_write_putc(
    struct screen_write_ctx *ctx, struct grid_cell *gc, u_char ch)
{
	gc->data = ch;
	screen_write_cell(ctx, gc);
}

/* Write string. */
void printflike3
screen_write_puts(
    struct screen_write_ctx *ctx, struct grid_cell *gc, const char *fmt, ...)
{
	va_list	ap;
	char   *msg, *ptr;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	for (ptr = msg; *ptr != '\0'; ptr++)
		screen_write_putc(ctx, gc, (u_char) *ptr);

	xfree(msg);
}

/* Copy from another screen. */
void
screen_write_copy(struct screen_write_ctx *ctx,
    struct screen *src, u_int px, u_int py, u_int nx, u_int ny)
{
	struct screen		*s = ctx->s;
	struct grid_data	*gd = src->grid;
	const struct grid_cell	*gc;
	u_int		 	 xx, yy, cx, cy;

	cx = s->cx;
	cy = s->cy;
	for (yy = py; yy < py + ny; yy++) {
		for (xx = px; xx < px + nx; xx++) {
			if (xx >= gd->sx || yy >= gd->hsize + gd->sy)
				gc = &grid_default_cell;
			else
				gc = grid_peek_cell(gd, xx, yy);
			screen_write_cell(ctx, gc);
		}
		cy++;
		screen_write_cursormove(ctx, cx, cy);
	}
}

/* Cursor up by ny. */
void
screen_write_cursorup(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny > s->cy)
		ny = s->cy;
	if (ny == 0)
		ny = 1;

	s->cy -= ny;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORUP, ny);
}

/* Cursor down by ny. */
void
screen_write_cursordown(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny > screen_size_y(s) - 1 - s->cy)
		ny = screen_size_y(s) - 1 - s->cy;
	if (ny == 0)
		ny = 1;

	s->cy += ny;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORDOWN, ny);
}

/* Cursor right by nx.  */
void
screen_write_cursorright(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx > screen_size_x(s) - 1 - s->cx)
		nx = screen_size_x(s) - 1 - s->cx;
	if (nx == 0)
		nx = 1;

	s->cx += nx;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORRIGHT, nx);
}

/* Cursor left by nx. */
void
screen_write_cursorleft(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx > s->cx)
		nx = s->cx;
	if (nx == 0)
		nx = 1;

	s->cx -= nx;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORLEFT, nx);
}

/* Insert nx characters. */
void
screen_write_insertcharacter(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx > screen_size_x(s) - 1 - s->cx)
		nx = screen_size_x(s) - 1 - s->cx;
	if (nx == 0)
		nx = 1;

	grid_view_insert_cells(s->grid, s->cx, s->cy, nx);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_INSERTCHARACTER, nx);
}

/* Delete nx characters. */
void
screen_write_deletecharacter(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;

	if (nx > screen_size_x(s) - 1 - s->cx)
		nx = screen_size_x(s) - 1 - s->cx;
	if (nx == 0)
		nx = 1;

	grid_view_delete_cells(s->grid, s->cx, s->cy, nx);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_DELETECHARACTER, nx);
}

/* Insert ny lines. */
void
screen_write_insertline(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny > screen_size_y(s) - 1 - s->cy)
		ny = screen_size_y(s) - 1 - s->cy;
	if (ny == 0)
		ny = 1;

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_insert_lines(s->grid, s->cy, ny);
	else {
		grid_view_insert_lines_region(
		    s->grid, s->rupper, s->rlower, s->cy, ny);
	}

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_INSERTLINE, ny);
}

/* Delete ny lines. */
void
screen_write_deleteline(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;

	if (ny > screen_size_y(s) - 1 - s->cy)
		ny = screen_size_y(s) - 1 - s->cy;
	if (ny == 0)
		ny = 1;

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_delete_lines(s->grid, s->cy, ny);
	else {
		grid_view_delete_lines_region(
		    s->grid, s->rupper, s->rlower, s->cy, ny);
	}

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_DELETELINE, ny);
}

/* Clear line at cursor. */
void
screen_write_clearline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	grid_view_clear(s->grid, 0, s->cy, screen_size_x(s), 1);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARLINE);
}

/* Clear to end of line from cursor. */
void
screen_write_clearendofline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	grid_view_clear(s->grid, s->cx, s->cy, screen_size_x(s) - s->cx, 1);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARENDOFLINE);
}

/* Clear to start of line from cursor. */
void
screen_write_clearstartofline(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARSTARTOFLINE);
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

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMOVE, px, py);
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

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMODE, state);
}

/* Reverse index (up with scroll).  */
void
screen_write_reverseindex(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	if (s->cy == s->rupper)
		grid_view_scroll_region_down(s->grid, s->rupper, s->rlower);
	else if (s->cy > 0)
		s->cy--;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_REVERSEINDEX);
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
	s->cy = rupper;

	s->rupper = rupper;
	s->rlower = rlower;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_SCROLLREGION, rupper, rlower);
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

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_INSERTMODE, state);
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

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_MOUSEMODE, state);
}

/* Line feed (down with scroll). */
void
screen_write_linefeed(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	if (s->cy == s->rlower)
		grid_view_scroll_region_up(s->grid, s->rupper, s->rlower);
	else if (s->cy < screen_size_x(s) - 1)
		s->cy++;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_LINEFEED);
}

/* Carriage return (cursor to start of line). */
void
screen_write_carriagereturn(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	s->cx = 0;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CARRIAGERETURN);
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

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_KCURSORMODE);
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

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_KKEYPADMODE);
}

/* Clear to end of screen from cursor. */
void
screen_write_clearendofscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx, sy;

	sx = screen_size_x(s);
	sy = screen_size_y(s);

	grid_view_clear(s->grid, s->cx, s->cy, sx - s->cx, 1);
	grid_view_clear(s->grid, 0, s->cy + 1, sx, sy - (s->cy + 1));

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARENDOFSCREEN);
}

/* Clear to start of screen. */
void
screen_write_clearstartofscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx;

	sx = screen_size_x(s);

	if (s->cy > 0)
		grid_view_clear(s->grid, 0, 0, sx, s->cy - 1);
	grid_view_clear(s->grid, 0, s->cy, s->cx, 1);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARSTARTOFSCREEN);
}

/* Clear entire screen. */
void
screen_write_clearscreen(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	grid_view_clear(s->grid, 0, 0, screen_size_x(s), screen_size_y(s));

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARSCREEN);
}

/* Write cell data. */
void
screen_write_cell(struct screen_write_ctx *ctx, const struct grid_cell *gc)
{
	struct screen		*s = ctx->s;
	struct grid_data	*gd = s->grid;
	u_int		 	 width, xx;
	const struct grid_cell 	*hc;
	struct grid_cell 	*ic, tc;

	width = utf8_width(gc->data);

	/* If the character is wider than the screen, don't print it. */
	if (width > screen_size_x(s)) {
		memcpy(&tc, gc, sizeof tc);
		tc.data = '_';

		width = 1;
		gc = &tc;
	}

	/* Check this will fit on the current line; scroll if not. */
	if (s->cx >= screen_size_x(s) + 1 - width) {
		screen_write_carriagereturn(ctx);
		screen_write_linefeed(ctx);
	}

	/* Sanity checks. */
	if (s->cx > screen_size_x(s) - 1 || s->cy > screen_size_y(s) - 1)
		return;

	/*
	 * UTF-8 wide characters are a bit of an annoyance. They take up more
	 * than one cell on the screen, so following cells must not be drawn by
	 * marking them as padding.
	 *
	 * So far, so good. The problem is, when overwriting a padding cell, or
	 * a UTF-8 character, it is necessary to also overwrite any other cells
	 * which covered by the same character.
	 */
	hc = grid_view_peek_cell(gd, s->cx, s->cy);
	if (hc->flags & GRID_FLAG_PADDING) {
		/*
		 * A padding cell, so clear any following and leading padding
		 * cells back to the character. Don't overwrite the current
		 * cell as that happens later anyway.
		 */
		xx = s->cx;
		while (xx-- > 0) {
			hc = grid_view_peek_cell(gd, xx, s->cy);
			if (!(hc->flags & GRID_FLAG_PADDING))
				break;
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}

		/* Overwrite the character at the start of this padding. */
		grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);

		/* Overwrite following padding cells. */
		xx = s->cx;
		while (++xx < screen_size_x(s)) {
			hc = grid_view_peek_cell(gd, xx, s->cy);
			if (!(hc->flags & GRID_FLAG_PADDING))
				break;
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}
	} else if (utf8_width(hc->data) > 1) {
		/*
		 * An UTF-8 wide cell; overwrite following padding cells only.
		 */
		xx = s->cx;
		while (++xx < screen_size_x(s)) {
			hc = grid_view_peek_cell(gd, xx, s->cy);
			if (!(hc->flags & GRID_FLAG_PADDING))
				break;
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}
	}

	/*
	 * If the new character is UTF-8 wide, fill in padding cells. Have
	 * already ensured there is enough room.
	 */
	for (xx = s->cx + 1; xx < s->cx + width; xx++) {
		ic = grid_view_get_cell(gd, xx, s->cy);
		ic->flags |= GRID_FLAG_PADDING;
	}

	/* Write the actual cell. */
	grid_view_set_cell(gd, s->cx, s->cy, gc);
	s->cx += width;

	if (ctx->write != NULL) {
		if (screen_check_selection(ctx->s, s->cx, s->cy)) {
			memcpy(&tc, &ctx->s->sel.cell, sizeof tc);
			tc.data = gc->data;
			ctx->write(ctx->data, TTY_CELL, &tc);
		} else
			ctx->write(ctx->data, TTY_CELL, gc);
	}
}
