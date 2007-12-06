/* $Id: window-more.c,v 1.7 2007-12-06 10:04:43 nicm Exp $ */

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

struct screen *window_more_init(struct window *);
void	window_more_free(struct window *);
void	window_more_resize(struct window *, u_int, u_int);
void	window_more_key(struct window *, int);

void	window_more_redraw_screen(struct window *);
void	window_more_write_line(
    	    struct window *, struct screen_write_ctx *, u_int);

void	window_more_scroll_up(struct window *);
void	window_more_scroll_down(struct window *);

const struct window_mode window_more_mode = {
	window_more_init,
	window_more_free,
	window_more_resize,
	window_more_key
};

struct window_more_mode_data {
	struct screen	        screen;

	ARRAY_DECL(, char *)	list;
	u_int			top;
};

void
window_more_vadd(struct window *w, const char *fmt, va_list ap)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	char   				*msg;
	u_int				 size;

	xvasprintf(&msg, fmt, ap);
	ARRAY_ADD(&data->list, msg);

	size = ARRAY_LENGTH(&data->list);
	if (size == 0)
		return;
	size--;
	if (size >= data->top && size <= data->top + screen_last_y(s)) {
		screen_write_start_window(&ctx, w);
		window_more_write_line(w, &ctx, size - data->top);
		if (size != data->top)
			window_more_write_line(w, &ctx, 0);
		screen_write_stop(&ctx);
	}
}

void
window_more_add(struct window *w, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	window_more_vadd(w, fmt, ap);
	va_end(ap);
}

struct screen *
window_more_init(struct window *w)
{
	struct window_more_mode_data	*data;
	struct screen			*s;

	w->modedata = data = xmalloc(sizeof *data);
	ARRAY_INIT(&data->list);
	data->top = 0;

	s = &data->screen;
	screen_create(s, screen_size_x(&w->base), screen_size_y(&w->base));
	s->mode = 0;
	
	return (s);
}

void
window_more_free(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	u_int				 i;

	for (i = 0; i < ARRAY_LENGTH(&data->list); i++)
		xfree(ARRAY_ITEM(&data->list, i));
	ARRAY_FREE(&data->list);

	screen_destroy(&data->screen);
	xfree(data);
}

void
window_more_resize(struct window *w, u_int sx, u_int sy)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	screen_resize(s, sx, sy);
	window_more_redraw_screen(w);
}

void
window_more_key(struct window *w, int key)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	switch (key) {
	case 'Q':
	case 'q':
		window_reset_mode(w);
		break;
	case 'k':
	case 'K':
	case KEYC_UP:
		window_more_scroll_up(w);
		break;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		window_more_scroll_down(w);
		break;
	case '\025':	/* C-u */
	case KEYC_PPAGE:
		if (data->top < screen_size_y(s))
			data->top = 0;
		else
			data->top -= screen_size_y(s);
		window_more_redraw_screen(w);
		break;
	case '\006':	/* C-f */
	case KEYC_NPAGE:
		if (data->top + screen_size_y(s) > ARRAY_LENGTH(&data->list))
			data->top = ARRAY_LENGTH(&data->list);
		else
			data->top += screen_size_y(s);
		window_more_redraw_screen(w);
		break;
	}
}

void
window_more_write_line(struct window *w, struct screen_write_ctx *ctx, u_int py)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	char   				*msg;
	size_t	 			 size;

	if (py == 0) {
		screen_write_set_attributes(ctx, 0, status_colour);
		screen_write_move_cursor(ctx, 0, 0);
		size = screen_write_put_string_rjust(
		    ctx, "[%u/%u]", data->top, ARRAY_LENGTH(&data->list));
	} else
		size = 0;

	screen_write_set_attributes(ctx, SCREEN_DEFATTR, SCREEN_DEFCOLR);
	screen_write_move_cursor(ctx, 0, py);
	if (data->top + py  < ARRAY_LENGTH(&data->list)) {
		msg = ARRAY_ITEM(&data->list, data->top + py);
		screen_write_put_string(
		    ctx, "%.*s", (int) (screen_size_x(s) - size), msg);
	}
	while (s->cx < screen_size_x(s) - size)
		screen_write_put_character(ctx, SCREEN_DEFDATA);
}

void
window_more_redraw_screen(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start_window(&ctx, w);
	for (i = 0; i < screen_size_y(s); i++)
		window_more_write_line(w, &ctx, i);
	screen_write_stop(&ctx);
}

void
window_more_scroll_up(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen_write_ctx		 ctx;

	if (data->top == 0)
		return;
	data->top--;

	screen_write_start_window(&ctx, w);
	screen_write_move_cursor(&ctx, 0, 0);
	screen_write_insert_lines(&ctx, 1);
	window_more_write_line(w, &ctx, 0);
	window_more_write_line(w, &ctx, 1);
	screen_write_stop(&ctx);
}

void
window_more_scroll_down(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->top >= ARRAY_LENGTH(&data->list))
		return;
	data->top++;

	screen_write_start_window(&ctx, w);
	screen_write_move_cursor(&ctx, 0, 0);
	screen_write_delete_lines(&ctx, 1);
	window_more_write_line(w, &ctx, screen_last_y(s));
	window_more_write_line(w, &ctx, 0);
	screen_write_stop(&ctx);
}
