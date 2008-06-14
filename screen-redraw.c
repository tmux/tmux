/* $Id: screen-redraw.c,v 1.5 2008-06-14 12:05:06 nicm Exp $ */

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

void	screen_redraw_get_cell(struct screen_redraw_ctx *,
    	    u_int, u_int, u_char *, u_char *, u_char *);

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
	 * modify them during redraw, and restore them when finished.
	 */
	ctx->saved_cx = s->cx;
	ctx->saved_cy = s->cy;

	ctx->write(ctx->data, TTY_ATTRIBUTES, s->attr, s->colr);
	ctx->write(ctx->data, TTY_SCROLLREGION, 0, screen_last_y(s));
	ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
	ctx->write(ctx->data, TTY_CURSOROFF);
	ctx->write(ctx->data, TTY_MOUSEOFF);
}

/* Finish redrawing. */
void
screen_redraw_stop(struct screen_redraw_ctx *ctx)
{
	struct screen	*s = ctx->s;

	s->cx = ctx->saved_cx;
	s->cy = ctx->saved_cy;

	ctx->write(ctx->data, TTY_ATTRIBUTES, s->attr, s->colr);
	ctx->write(ctx->data, TTY_SCROLLREGION, s->rupper, s->rlower);
	ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
	if (s->mode & MODE_CURSOR)
		ctx->write(ctx->data, TTY_CURSORON);
	if (s->mode & MODE_MOUSE)
		ctx->write(ctx->data, TTY_MOUSEON);
}

/* Get cell data. */
void
screen_redraw_get_cell(struct screen_redraw_ctx *ctx,
    u_int px, u_int py, u_char *data, u_char *attr, u_char *colr)
{
	struct screen	*s = ctx->s;

	screen_get_cell(s, screen_x(s, px), screen_y(s, py), data, attr, colr);
}

/* Move cursor. */
void
screen_redraw_move_cursor(struct screen_redraw_ctx *ctx, u_int px, u_int py)
{
	if (px != ctx->s->cx || py != ctx->s->cy) {
		ctx->s->cx = px;
		ctx->s->cy = py;
		ctx->write(ctx->data, TTY_CURSORMOVE, ctx->s->cy, ctx->s->cx);
	}
}

/* Set attributes. */
void
screen_redraw_set_attributes(
    struct screen_redraw_ctx *ctx, u_int attr, u_int colr)
{
	ctx->write(ctx->data, TTY_ATTRIBUTES, attr, colr);
}

/* Write string. */
void printflike2
screen_redraw_write_string(struct screen_redraw_ctx *ctx, const char *fmt, ...)
{
	struct screen	*s = ctx->s;
	va_list		 ap;
	char   		*msg, *ptr;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	for (ptr = msg; *ptr != '\0'; ptr++) {
		if (ctx->s->cx > screen_last_x(s))
			break;
		if (*ptr < 0x20)
			continue;
		ctx->write(ctx->data, TTY_CHARACTER, *ptr);
		ctx->s->cx++;
	}

	xfree(msg);
}

/* Redraw single cell. */
void
screen_redraw_cell(struct screen_redraw_ctx *ctx, u_int px, u_int py)
{
	u_char	 data, attr, colr;

	screen_redraw_move_cursor(ctx, px, py);
	screen_redraw_get_cell(ctx, px, py, &data, &attr, &colr);

	ctx->write(ctx->data, TTY_ATTRIBUTES, attr, colr);
	ctx->write(ctx->data, TTY_CHARACTER, data);

	ctx->s->cx++;
}

/* Redraw area of cells. */
void
screen_redraw_area(
    struct screen_redraw_ctx *ctx, u_int px, u_int py, u_int nx, u_int ny)
{
	u_int	i, j;

	for (i = py; i < py + ny; i++) {
		for (j = px; j < px + nx; j++)
			screen_redraw_cell(ctx, j, i);
	}
}

/* Draw set of lines. */
void
screen_redraw_lines(struct screen_redraw_ctx *ctx, u_int py, u_int ny)
{
	u_int	i, cx, sx;

	sx = screen_size_x(ctx->s);
	for (i = py; i < py + ny; i++) {
		cx = ctx->s->grid_size[screen_y(ctx->s, i)];
		if (ctx->s->sel.flag || sx < 5 || cx >= sx - 5) {
			screen_redraw_area(ctx, 0, i, screen_size_x(ctx->s), 1);
			continue;
		}
		screen_redraw_area(ctx, 0, i, cx, 1);
		screen_redraw_move_cursor(ctx, cx, i);
		screen_redraw_set_attributes(
		    ctx, SCREEN_DEFATTR, SCREEN_DEFCOLR);
		ctx->write(ctx->data, TTY_CLEARENDOFLINE);
	}
}
