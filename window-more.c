/* $Id: window-more.c,v 1.3 2007-11-22 09:11:20 nicm Exp $ */

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
void	window_more_draw(struct window *, struct buffer *, u_int, u_int);
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

	w->screen.mode |= (MODE_BACKGROUND|MODE_NOCURSOR);
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
	size_t				 n;


	len = xsnprintf(
	    buf, sizeof buf, "[%u/%u]", data->top, ARRAY_LENGTH(&data->list));
	if (len <= screen_size_x(ctx->s))
		ptr = buf;
	else {
		ptr = buf + len - screen_size_x(ctx->s);
		len -= len - screen_size_x(ctx->s);
	}

	screen_draw_move(ctx, 0, 0);

	if (data->top < ARRAY_LENGTH(&data->list)) {
		line = ARRAY_ITEM(&data->list, data->top);
		n = strlen(line);
		if (n > screen_size_x(ctx->s) - len)
			n = screen_size_x(ctx->s) - len;
		buffer_write(ctx->b, line, n);
	} else
		n = 0;
	for (; n < screen_size_x(ctx->s) - len; n++)
		input_store8(ctx->b, SCREEN_DEFDATA);

	screen_draw_move(ctx, screen_size_x(ctx->s) - len, 0);
	screen_draw_set_attributes(ctx, 0, status_colour);
	buffer_write(ctx->b, buf, len);
}

void
window_more_draw_line(struct window *w, struct screen_draw_ctx *ctx, u_int py)
{
	struct window_more_mode_data	*data = w->modedata;
	char				*line;
	size_t				 n;
	u_int				 p;

	screen_draw_move(ctx, 0, py);
	screen_draw_set_attributes(ctx, SCREEN_DEFATTR, SCREEN_DEFCOLR);
	
	p = data->top + py;
	if (p >= ARRAY_LENGTH(&data->list)) {
		input_store_zero(ctx->b, CODE_CLEARLINE);
		return;
	}

	line = ARRAY_ITEM(&data->list, p);
	n = strlen(line);
	if (n > screen_size_x(ctx->s))
		n = screen_size_x(ctx->s);
	buffer_write(ctx->b, line, n);
	for (; n < screen_size_x(ctx->s); n++)
		input_store8(ctx->b, SCREEN_DEFDATA);
}

void
window_more_draw(struct window *w, struct buffer *b, u_int py, u_int ny)
{
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	u_int				 i;

	screen_draw_start(&ctx, s, b, 0, 0);

	for (i = py; i < py + ny; i++) {
		if (i == 0)
			continue;
		window_more_draw_line(w, &ctx, i);
	}
	if (py == 0)
		window_more_draw_position(w, &ctx);

	screen_draw_stop(&ctx);
	input_store_zero(b, CODE_CURSOROFF);
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

		w->screen.mode &= ~(MODE_BACKGROUND|MODE_NOCURSOR);

		recalculate_sizes();
		server_redraw_window_all(w);
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
		server_redraw_window_all(w);
}

void
window_more_up_1(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i;
	struct hdr			 hdr;
	size_t				 size;

	if (data->top == 0)
		return;
	data->top--;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;

		buffer_ensure(c->out, sizeof hdr);
		buffer_add(c->out, sizeof hdr);
		size = BUFFER_USED(c->out);

		screen_draw_start(&ctx, s, c->out, 0, 0);
		screen_draw_move(&ctx, 0, 0);
		input_store_one(c->out, CODE_INSERTLINE, 1);
		window_more_draw_position(w, &ctx);
		window_more_draw_line(w, &ctx, 1);
		screen_draw_stop(&ctx);

		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_more_down_1(struct window *w)
{
	struct window_more_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i;
	struct hdr			 hdr;
	size_t				 size;

	if (data->top >= ARRAY_LENGTH(&data->list))
		return;
	data->top++;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;

		buffer_ensure(c->out, sizeof hdr);
		buffer_add(c->out, sizeof hdr);
		size = BUFFER_USED(c->out);
		
		screen_draw_start(&ctx, s, c->out, 0, 0);
		screen_draw_move(&ctx, 0, 0);
		input_store_one(c->out, CODE_DELETELINE, 1);
		window_more_draw_line(w, &ctx, screen_last_y(s));
		window_more_draw_position(w, &ctx);
		screen_draw_stop(&ctx);
		
		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}
