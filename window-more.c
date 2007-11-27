/* $Id: window-more.c,v 1.5 2007-11-27 19:23:34 nicm Exp $ */

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

void	window_more_init(struct window *);
void	window_more_resize(struct window *, u_int, u_int);
void	window_more_draw(
    	    struct window *, struct screen_draw_ctx *, u_int, u_int);
void	window_more_key(struct window *, int);

void	window_more_draw_position(struct window *, struct screen_draw_ctx *);
void	window_more_draw_line(struct window *, struct screen_draw_ctx *, u_int);

void	window_more_up_1(struct window *);
void	window_more_down_1(struct window *);

const struct window_mode window_more_mode = {
	window_more_init,
	window_more_resize,
	window_more_draw,
	window_more_key
};

struct window_more_mode_data {
	ARRAY_DECL(, char *)	list;
	u_int			top;
};

void
window_more_vadd(struct window *w, const char *fmt, va_list ap)
{
	struct window_more_mode_data	*data = w->modedata;
	char   				*s;

	xvasprintf(&s, fmt, ap);
	ARRAY_ADD(&data->list, s);
}

void
window_more_add(struct window *w, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	window_more_vadd(w, fmt, ap);
	va_end(ap);
}

void
window_more_init(struct window *w)
{
	struct window_more_mode_data	*data;

	w->modedata = data = xmalloc(sizeof *data);
	ARRAY_INIT(&data->list);
	data->top = 0;

	w->screen.mode |= MODE_BACKGROUND;
	w->screen.mode &= ~MODE_BGCURSOR;
}

void
window_more_resize(unused struct window *w, unused u_int sx, unused u_int sy)
{
}

void
window_more_draw_position(struct window *w, struct screen_draw_ctx *ctx)
{
	struct window_more_mode_data	*data = w->modedata;
	char				*ptr, buf[32];
	size_t	 			 len;
	char				*line;

	len = xsnprintf(
	    buf, sizeof buf, "[%u/%u]", data->top, ARRAY_LENGTH(&data->list));
	if (len <= screen_size_x(ctx->s))
		ptr = buf;
	else {
		ptr = buf + len - screen_size_x(ctx->s);
		len -= len - screen_size_x(ctx->s);
	}

	screen_draw_move_cursor(ctx, 0, 0);

	if (data->top < ARRAY_LENGTH(&data->list)) {
		line = xstrdup(ARRAY_ITEM(&data->list, data->top));
		if (strlen(line) > screen_size_x(ctx->s) - len)
			line[screen_size_x(ctx->s) - len] = '\0';
		screen_draw_write_string(ctx, "%s", line);
		xfree(line);
	}
	screen_draw_clear_line_to(ctx, screen_size_x(ctx->s) - len - 1);

	screen_draw_move_cursor(ctx, screen_size_x(ctx->s) - len, 0);
	screen_draw_set_attributes(ctx, 0, status_colour);
	screen_draw_write_string(ctx, "%s", ptr);
}

void
window_more_draw_line(struct window *w, struct screen_draw_ctx *ctx, u_int py)
{
	struct window_more_mode_data	*data = w->modedata;
	u_int				 p;

	screen_draw_move_cursor(ctx, 0, py);
	screen_draw_set_attributes(ctx, SCREEN_DEFATTR, SCREEN_DEFCOLR);
	
	p = data->top + py;
	if (p < ARRAY_LENGTH(&data->list))
		screen_draw_write_string(ctx, "%s", ARRAY_ITEM(&data->list, p));

	screen_draw_clear_line_to(ctx, screen_last_x(ctx->s));
}

void
window_more_draw(
    struct window *w, struct screen_draw_ctx *ctx, u_int py, u_int ny)
{
	u_int	i;

	for (i = py; i < py + ny; i++) {
		if (i == 0)
			window_more_draw_position(w, ctx);
		else
			window_more_draw_line(w, ctx, i);
	}
}

void
window_more_key(struct window *w, int key)
{
	struct window_more_mode_data	*data = w->modedata;
	u_int				 top, sy, i;
	
	sy = screen_size_y(&w->screen);

	top = data->top;

	switch (key) {
	case 'Q':
	case 'q':
		for (i = 0; i < ARRAY_LENGTH(&data->list); i++)
			xfree(ARRAY_ITEM(&data->list, i));
		ARRAY_FREE(&data->list);

		w->mode = NULL;
		xfree(w->modedata);

		w->screen.mode &= ~MODE_BACKGROUND;

		recalculate_sizes();
		server_redraw_window(w);
		return;
	case 'k':
	case 'K':
	case KEYC_UP:
		window_more_up_1(w);
		return;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		window_more_down_1(w);
		return;
	case '\025':	/* C-u */
	case KEYC_PPAGE:
		if (data->top < sy)
			data->top = 0;
		else
			data->top -= sy;
		break;
	case '\006':	/* C-f */
	case KEYC_NPAGE:
		if (data->top + sy > ARRAY_LENGTH(&data->list))
			data->top = ARRAY_LENGTH(&data->list);
		else
			data->top += sy;
		break;
	}
	if (top != data->top)
		server_redraw_window(w);
}

void
window_more_up_1(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen_draw_ctx		 ctx;

	if (data->top == 0)
		return;
	data->top--;

	screen_draw_start_window(&ctx, w, 0, 0);
	screen_draw_move_cursor(&ctx, 0, 0);
	screen_draw_insert_lines(&ctx, 1);
	window_more_draw_position(w, &ctx);
	window_more_draw_line(w, &ctx, 1);
	screen_draw_stop(&ctx);
}

void
window_more_down_1(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;

	if (data->top >= ARRAY_LENGTH(&data->list))
		return;
	data->top++;

	screen_draw_start_window(&ctx, w, 0, 0);
	screen_draw_move_cursor(&ctx, 0, 0);
	screen_draw_delete_lines(&ctx, 1);
	window_more_draw_line(w, &ctx, screen_last_y(s));
	window_more_draw_position(w, &ctx);
	screen_draw_stop(&ctx);
}
