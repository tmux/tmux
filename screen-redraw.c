/* $Id: screen-redraw.c,v 1.13 2008-09-26 06:45:26 nicm Exp $ */

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

/* Initialise redrawing with a window. */
void
screen_redraw_start_window(struct screen_redraw_ctx *ctx, struct window *w)
{
	struct screen	*t = w->screen;

	screen_redraw_start(ctx, t, tty_write_window, w);
}

/* Initialise redrawing with a client. */
void
screen_redraw_start_client(struct screen_redraw_ctx *ctx, struct client *c)
{
	struct screen	*t = c->session->curw->window->screen;

	screen_redraw_start(ctx, t, tty_write_client, c);
}

/* Initialise redrawing with a session. */
void
screen_redraw_start_session(struct screen_redraw_ctx *ctx, struct session *s)
{
	struct screen	*t = s->curw->window->screen;

	screen_redraw_start(ctx, t, tty_write_session, s);
}

/* Initialise for redrawing. */
void
screen_redraw_start(struct screen_redraw_ctx *ctx,
    struct screen *s, void (*write)(void *, int, ...), void *data)
{
	ctx->write = write;
	ctx->data = data;

	ctx->s = s;

	/*
	 * Save screen cursor position. Emulation of some TTY_* commands
	 * requires this to be correct in the screen, so rather than having
	 * a local copy and just manipulating it, save the screen's values,
	 * modify them during redraw, and restore them when finished. XXX.
	 */
	ctx->saved_cx = s->cx;
	ctx->saved_cy = s->cy;

	ctx->write(ctx->data, TTY_SCROLLREGION, 0, screen_size_y(s) - 1);
	ctx->write(ctx->data, TTY_CURSORMOVE, s->cx, s->cy);
	ctx->write(ctx->data, TTY_CURSORMODE, 0);
	ctx->write(ctx->data, TTY_MOUSEMODE, 0);
}

/* Finish redrawing. */
void
screen_redraw_stop(struct screen_redraw_ctx *ctx)
{
	struct screen	*s = ctx->s;

	s->cx = ctx->saved_cx;
	s->cy = ctx->saved_cy;

	ctx->write(ctx->data, TTY_SCROLLREGION, s->rupper, s->rlower);
	ctx->write(ctx->data, TTY_CURSORMOVE, s->cx, s->cy);
	if (s->mode & MODE_CURSOR)
		ctx->write(ctx->data, TTY_CURSORMODE, 1);
	if (s->mode & MODE_MOUSE)
		ctx->write(ctx->data, TTY_MOUSEMODE, 1);
}

/* Write character. */
void
screen_redraw_putc(
    struct screen_redraw_ctx *ctx, struct grid_cell *gc, u_char ch)
{
	gc->data = ch;
	ctx->write(ctx->data, TTY_CELL, gc);
	ctx->s->cx++;
}

/* Write string. */
void printflike3
screen_redraw_puts(
    struct screen_redraw_ctx *ctx, struct grid_cell *gc, const char *fmt, ...)
{
	va_list	ap;
	char   *msg, *ptr;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	for (ptr = msg; *ptr != '\0'; ptr++)
		screen_redraw_putc(ctx, gc, (u_char) *ptr);

	xfree(msg);
}

/* Redraw single cell. */
void
screen_redraw_cell(struct screen_redraw_ctx *ctx, u_int px, u_int py)
{
	const struct grid_cell	*gc;
	struct grid_cell	 hc;

	if (px != ctx->s->cx || py != ctx->s->cy) {
		ctx->s->cx = px;
		ctx->s->cy = py;
		ctx->write(ctx->data, TTY_CURSORMOVE, ctx->s->cx, ctx->s->cy);
	}

	gc = grid_view_peek_cell(ctx->s->grid, px, py);
        if (screen_check_selection(ctx->s, px, py)) {
		memcpy(&hc, gc, sizeof hc);
		hc.attr |= GRID_ATTR_REVERSE;
		ctx->write(ctx->data, TTY_CELL, &hc);
	} else
		ctx->write(ctx->data, TTY_CELL, gc);
	ctx->s->cx++;
}

/* Draw set of lines. */
void
screen_redraw_lines(struct screen_redraw_ctx *ctx, u_int py, u_int ny)
{
	u_int	i, j;

	for (j = py; j < py + ny; j++) {
		for (i = 0; i < screen_size_x(ctx->s); i++)
			screen_redraw_cell(ctx, i, j);
	}
}

/* Draw set of columns. */
void
screen_redraw_columns(struct screen_redraw_ctx *ctx, u_int px, u_int nx)
{
	u_int	i, j;

	for (j = 0; j < screen_size_y(ctx->s); j++) {
		for (i = px; i < px + nx; i++)
			screen_redraw_cell(ctx, i, j);
	}
}
