/* $Id: screen-write.c,v 1.12 2008-09-08 22:03:54 nicm Exp $ */

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

#define screen_write_limit(s, v, lower, upper) do {			\
	if (v < lower) {						\
		v = lower;						\
		SCREEN_DEBUG3(s, v, lower, upper);			\
	}								\
	if (v > upper) {						\
		v = upper;						\
		SCREEN_DEBUG3(s, v, lower, upper);			\
	}								\
} while (0)

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
		ctx->write(ctx->data, TTY_CURSOROFF);
}

/* Finalise writing. */
void
screen_write_stop(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	if (ctx->write != NULL && s->mode & MODE_CURSOR)
		ctx->write(ctx->data, TTY_CURSORON);
}

/* Set screen title. */
void
screen_write_set_title(struct screen_write_ctx *ctx, char *title)
{
	struct screen	*s = ctx->s;

	xfree(s->title);
	s->title = title;
}

/* Put a character. */
void
screen_write_put_character(struct screen_write_ctx *ctx, u_char ch)
{
	struct screen	*s = ctx->s;

	if (s->cx == screen_size_x(s)) {
		s->cx = 0;
		if (ctx->write != NULL)
			ctx->write(ctx->data, TTY_CHARACTER, '\r');
		screen_write_cursor_down_scroll(ctx);
	} else if (!screen_in_x(s, s->cx) || !screen_in_y(s, s->cy)) {
		SCREEN_DEBUG(s);
		return;
	}

	screen_display_set_cell(s, s->cx, s->cy, ch, s->attr, s->fg, s->bg);
	s->cx++;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CHARACTER, ch);
}

/* Put a string right-justified. */
size_t printflike2
screen_write_put_string_rjust(
    struct screen_write_ctx *ctx, const char *fmt, ...)
{
	struct screen	*s = ctx->s;
	va_list		 ap;
	size_t		 size;
	char   		*msg, *ptr;

	va_start(ap, fmt);
	size = vasprintf(&msg, fmt, ap);
	va_end(ap);

	ptr = msg;
	if (size > screen_size_x(s)) {
		ptr += size - screen_size_x(s);
		size = screen_size_x(s);
	}
	screen_write_move_cursor(ctx, screen_size_x(s) - size, s->cy);
	for (; *ptr != '\0'; ptr++) {
		if (s->cx == screen_size_x(s))
			break;
		screen_write_put_character(ctx, *ptr);
	}

	xfree(msg);

	return (size);
}

/* Put a string, truncating at end of line. */
void printflike2
screen_write_put_string(struct screen_write_ctx *ctx, const char *fmt, ...)
{
	struct screen	*s = ctx->s;
	va_list		 ap;
	char		*msg, *ptr;

	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);

	for (ptr = msg; *ptr != '\0'; ptr++) {
		if (s->cx == screen_size_x(s))
			break;
		screen_write_put_character(ctx, *ptr);
	}

	xfree(msg);
}

/* Set screen attributes. */
void
screen_write_set_attributes(
    struct screen_write_ctx *ctx, u_short attr, u_char fg, u_char bg)
{
	struct screen	*s = ctx->s;

	if (s->attr != attr || s->fg != fg || s->bg != bg) {
		s->attr = attr;
		s->fg = fg;
		s->bg = bg;

		if (ctx->write != NULL)
			ctx->write(ctx->data, TTY_ATTRIBUTES, attr, fg, bg);
	}
}

/* Set scroll region.  */
void
screen_write_set_region(struct screen_write_ctx *ctx, u_int upper, u_int lower)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, upper, 0, screen_last_y(s));
	screen_write_limit(s, lower, 0, screen_last_y(s));
	if (upper > lower) {
		SCREEN_DEBUG2(s,  upper, lower);
		return;
	}

	/* Cursor moves to top-left. */
	s->cx = 0;
	s->cy = upper;

	s->rupper = upper;
	s->rlower = lower;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_SCROLLREGION, s->rupper, s->rlower);
}

/* Move cursor up and scroll if necessary. */
void
screen_write_cursor_up_scroll(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	if (s->cy == s->rupper)
		screen_display_scroll_region_down(s);
	else if (s->cy > 0)
		s->cy--;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_REVERSEINDEX);
}

/* Move cursor down and scroll if necessary  */
void
screen_write_cursor_down_scroll(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	if (s->cy == s->rlower)
		screen_display_scroll_region_up(s);
	else if (s->cy < screen_last_y(s))
		s->cy++;

	if (ctx->write != NULL)	/* XXX FORWARDINDEX */
		ctx->write(ctx->data, TTY_CHARACTER, '\n');
}

/* Move cursor up. */
void
screen_write_cursor_up(struct screen_write_ctx *ctx, u_int n)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_above_y(s, s->cy) - 1);

	s->cy -= n;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
}

/* Move cursor down. */
void
screen_write_cursor_down(struct screen_write_ctx *ctx, u_int n)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_below_y(s, s->cy) - 1);

	s->cy += n;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
}

/* Move cursor left.  */
void
screen_write_cursor_left(struct screen_write_ctx *ctx, u_int n)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_left_x(s, s->cx) - 1);

	s->cx -= n;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
}

/* Move cursor right.  */
void
screen_write_cursor_right(struct screen_write_ctx *ctx, u_int n)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_right_x(s, s->cx) - 1);

	s->cx += n;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
}

/* Delete lines. */
void
screen_write_delete_lines(struct screen_write_ctx *ctx, u_int n)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_below_y(s, s->cy));

	if (s->cy < s->rupper || s->cy > s->rlower)
		screen_display_delete_lines(s, s->cy, n);
	else
		screen_display_delete_lines_region(s, s->cy, n);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_DELETELINE, n);
}

/* Delete characters. */
void
screen_write_delete_characters(struct screen_write_ctx *ctx, u_int n)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_right_x(s, s->cx));

	screen_display_delete_characters(s, s->cx, s->cy, n);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_DELETECHARACTER, n);
}

/* Insert lines. */
void
screen_write_insert_lines(struct screen_write_ctx *ctx, u_int n)
{
	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_below_y(s, s->cy));

	if (s->cy < s->rupper || s->cy > s->rlower)
		screen_display_insert_lines(s, s->cy, n);
	else
		screen_display_insert_lines_region(s, s->cy, n);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_INSERTLINE, n);
}

/* Insert characters. */
void
screen_write_insert_characters(struct screen_write_ctx *ctx, u_int n)
{
 	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 1, screen_right_x(s, s->cx));

	screen_display_insert_characters(s, s->cx, s->cy, n);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_INSERTCHARACTER, n);
}

/* Move the cursor. */
void
screen_write_move_cursor(struct screen_write_ctx *ctx, u_int n, u_int m)
{
 	struct screen	*s = ctx->s;

	screen_write_limit(s, n, 0, screen_last_x(s));
	screen_write_limit(s, m, 0, screen_last_y(s));

	s->cx = n;
	s->cy = m;

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
}

/* Full to end of screen. */
void
screen_write_fill_end_of_screen(struct screen_write_ctx *ctx)
{
 	struct screen	*s = ctx->s;
	u_int		 i;

	screen_display_fill_area(s, s->cx, s->cy,
	    screen_right_x(s, s->cx), 1, ' ', s->attr, s->fg, s->bg);
	screen_display_fill_area(s, 0, s->cy + 1, screen_size_x(s),
	    screen_below_y(s, s->cy + 1), ' ', s->attr, s->fg, s->bg);

	if (ctx->write != NULL) {
		ctx->write(ctx->data, TTY_CLEARENDOFLINE);
		for (i = s->cy + 1; i < screen_size_y(s); i++) {
			ctx->write(ctx->data, TTY_CURSORMOVE, i, 0);
			ctx->write(ctx->data, TTY_CLEARENDOFLINE);
		}
		ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
	}
}

/* Fill entire screen. */
void
screen_write_fill_screen(struct screen_write_ctx *ctx)
{
 	struct screen	*s = ctx->s;
	u_int		 i;

	screen_display_fill_area(s, 0, 0,
	    screen_size_x(s), screen_size_y(s), ' ', s->attr, s->fg, s->bg);

	if (ctx->write != NULL) {
		for (i = 0; i < screen_size_y(s); i++) {
			ctx->write(ctx->data, TTY_CURSORMOVE, i, 0);
			ctx->write(ctx->data, TTY_CLEARENDOFLINE);
		}
		ctx->write(ctx->data, TTY_CURSORMOVE, s->cy, s->cx);
	}
}

/* Fill to end of line.  */
void
screen_write_fill_end_of_line(struct screen_write_ctx *ctx)
{
 	struct screen	*s = ctx->s;

	screen_display_fill_area(s, s->cx, s->cy,
	    screen_right_x(s, s->cx), 1, ' ', s->attr, s->fg, s->bg);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARENDOFLINE);
}

/* Fill to start of line.  */
void
screen_write_fill_start_of_line(struct screen_write_ctx *ctx)
{
 	struct screen	*s = ctx->s;

	screen_display_fill_area(s, 0, s->cy,
	    screen_left_x(s, s->cx), 1, ' ', s->attr, s->fg, s->bg);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARSTARTOFLINE);
}

/* Fill entire line. */
void
screen_write_fill_line(struct screen_write_ctx *ctx)
{
 	struct screen	*s = ctx->s;

	screen_display_fill_area(
	    s, 0, s->cy, screen_size_x(s), s->cy, ' ', s->attr, s->fg, s->bg);

	if (ctx->write != NULL)
		ctx->write(ctx->data, TTY_CLEARLINE);
}

/* Set a screen mode. */
void
screen_write_set_mode(struct screen_write_ctx *ctx, int mode)
{
 	struct screen	*s = ctx->s;

	s->mode |= mode;

	if (ctx->write == NULL)
		return;

	if (mode & MODE_INSERT)
		ctx->write(ctx->data, TTY_INSERTON);
	if (mode & MODE_MOUSE)
		ctx->write(ctx->data, TTY_MOUSEON);
}

/* Clear a screen mode. */
void
screen_write_clear_mode(struct screen_write_ctx *ctx, int mode)
{
 	struct screen	*s = ctx->s;

	s->mode &= ~mode;

	if (ctx->write == NULL)
		return;

	if (mode & MODE_INSERT)
		ctx->write(ctx->data, TTY_INSERTOFF);
	if (mode & MODE_MOUSE)
		ctx->write(ctx->data, TTY_MOUSEOFF);
}

/* Copy cells from another screen. */
void
screen_write_copy_area(struct screen_write_ctx *ctx,
    struct screen *src, u_int nx, u_int ny, u_int ox, u_int oy)
{
 	struct screen		       *s = ctx->s;
	struct screen_redraw_ctx	rctx;
	int				saved_mode;

	screen_write_limit(s, nx, 1, screen_right_x(s, s->cx));
	screen_write_limit(s, ny, 1, screen_below_y(s, s->cy));

	screen_display_copy_area(ctx->s, src, s->cx, s->cy, nx, ny, ox, oy);

	if (ctx->write != NULL) {
		/* Save mode XXX hack */
		saved_mode = ctx->s->mode;
		ctx->s->mode &= ~MODE_CURSOR;

		screen_redraw_start(&rctx, ctx->s, ctx->write, ctx->data);
		screen_redraw_area(&rctx, s->cx, s->cy, nx, ny);
		screen_redraw_stop(&rctx);

		ctx->s->mode = saved_mode;
	}
}
