/* $Id: window-scroll.c,v 1.8 2007-11-21 19:44:05 nicm Exp $ */

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

void	window_scroll_init(struct window *);
void	window_scroll_resize(struct window *, u_int, u_int);
void	window_scroll_draw(struct window *, struct buffer *, u_int, u_int);
void	window_scroll_key(struct window *, int);

void	window_scroll_draw_position(struct window *, struct screen_draw_ctx *);

void	window_scroll_up_1(struct window *);
void	window_scroll_down_1(struct window *);
void	window_scroll_left_1(struct window *);
void	window_scroll_right_1(struct window *);

const struct window_mode window_scroll_mode = {
	window_scroll_init,
	window_scroll_resize,
	window_scroll_draw,
	window_scroll_key
};

struct window_scroll_mode_data {
	u_int	ox;
	u_int	oy;
	u_int	size;
};

void
window_scroll_init(struct window *w)
{
	struct window_scroll_mode_data	*data;

	w->modedata = data = xmalloc(sizeof *data);
	data->ox = data->oy = 0;
	data->size = w->screen.hsize;
}

void
window_scroll_resize(unused struct window *w, unused u_int sx, unused u_int sy)
{
}

void
window_scroll_draw_position(struct window *w, struct screen_draw_ctx *ctx)
{
	struct window_scroll_mode_data	*data = w->modedata;
	char				*ptr, buf[32];
	size_t	 			 len;

	len = xsnprintf(
	    buf, sizeof buf, "[%u,%u/%u]", data->ox, data->oy, ctx->s->hsize);
	if (len <= screen_size_x(ctx->s))
		ptr = buf;
	else {
		ptr = buf + len - screen_size_x(ctx->s);
		len -= len - screen_size_x(ctx->s);
	}
	
	screen_draw_cells(ctx, 0, 0, screen_size_x(ctx->s) - len);
	
	screen_draw_move(ctx, screen_size_x(ctx->s) - len, 0);
	screen_draw_set_attributes(ctx, 0, status_colour);
	buffer_write(ctx->b, ptr, len);
}

void
window_scroll_draw(struct window *w, struct buffer *b, u_int py, u_int ny)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;

	if (s->hsize != data->size) {
		data->ox += s->hsize - data->size;
		data->size = s->hsize;
	}

	screen_draw_start(&ctx, s, b, data->ox, data->oy);
	if (py != 0)
		screen_draw_lines(&ctx, py, ny);
	else if (ny > 1)
		screen_draw_lines(&ctx, py + 1, ny - 1);

	if (py == 0)
		window_scroll_draw_position(w, &ctx);

	screen_draw_stop(&ctx);
	input_store_zero(b, CODE_CURSOROFF);
}

void
window_scroll_key(struct window *w, int key)
{
	struct window_scroll_mode_data	*data = w->modedata;
	u_int				 ox, oy, sx, sy;
	
	sx = screen_size_x(&w->screen);
	sy = screen_size_y(&w->screen);

	ox = data->ox;
	oy = data->oy;

	switch (key) {
	case 'Q':
	case 'q':
		w->mode = NULL;
		xfree(w->modedata);

		recalculate_sizes();
		server_redraw_window_all(w);
		return;
	case 'h':
	case KEYC_LEFT:
		window_scroll_left_1(w);
		return;
	case 'l':
	case KEYC_RIGHT:
		window_scroll_right_1(w);
		return;
	case 'k':
	case 'K':
	case KEYC_UP:
		window_scroll_up_1(w);
		return;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		window_scroll_down_1(w);
		return;
	case '\025':	/* C-u */
	case KEYC_PPAGE:
		if (data->oy + sy > data->size)
			data->oy = data->size;
		else
			data->oy += sy;
		break;
	case '\006':	/* C-f */
	case KEYC_NPAGE:
		if (data->oy < sy)
			data->oy = 0;
		else
			data->oy -= sy;
		break;
	}
	if (ox != data->ox || oy != data->oy)
		server_redraw_window_all(w);
}

void
window_scroll_up_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i;
	struct hdr			 hdr;
	size_t				 size;

	if (data->oy >= data->size)
		return;
	data->oy++;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;

		buffer_ensure(c->out, sizeof hdr);
		buffer_add(c->out, sizeof hdr);
		size = BUFFER_USED(c->out);

		screen_draw_start(&ctx, s, c->out, data->ox, data->oy);
		screen_draw_move(&ctx, 0, 0);
		input_store_one(c->out, CODE_INSERTLINE, 1);
		window_scroll_draw_position(w, &ctx);
		screen_draw_line(&ctx, 1);	/* nuke old position display */
		screen_draw_stop(&ctx);
		input_store_zero(c->out, CODE_CURSOROFF);

		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_scroll_down_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i;
	struct hdr			 hdr;
	size_t				 size;

	if (data->oy == 0)
		return;
	data->oy--;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;

		buffer_ensure(c->out, sizeof hdr);
		buffer_add(c->out, sizeof hdr);
		size = BUFFER_USED(c->out);
		
		screen_draw_start(&ctx, s, c->out, data->ox, data->oy);
		screen_draw_move(&ctx, 0, 0);
		input_store_one(c->out, CODE_DELETELINE, 1);
		screen_draw_line(&ctx, screen_last_y(s));
		window_scroll_draw_position(w, &ctx);
		screen_draw_stop(&ctx);
		input_store_zero(c->out, CODE_CURSOROFF);
		
		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_scroll_right_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i, j;
	struct hdr			 hdr;
	size_t				 size;

	if (data->ox >= SHRT_MAX)
		return;
	data->ox++;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;

		buffer_ensure(c->out, sizeof hdr);
		buffer_add(c->out, sizeof hdr);
		size = BUFFER_USED(c->out);
		
		screen_draw_start(&ctx, s, c->out, data->ox, data->oy);
		for (j = 1; j < screen_size_y(s); j++) {
			screen_draw_move(&ctx, 0, j);
			input_store_one(c->out, CODE_DELETECHARACTER, 1);
		}
		screen_draw_column(&ctx, screen_last_x(s));
		window_scroll_draw_position(w, &ctx);
		screen_draw_stop(&ctx);
		input_store_zero(c->out, CODE_CURSOROFF);
		
		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_scroll_left_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i, j;
	struct hdr			 hdr;
	size_t				 size;

	if (data->ox == 0)
		return;
	data->ox--;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;

		buffer_ensure(c->out, sizeof hdr);
		buffer_add(c->out, sizeof hdr);
		size = BUFFER_USED(c->out);
		
		screen_draw_start(&ctx, s, c->out, data->ox, data->oy);
		for (j = 1; j < screen_size_y(s); j++) {
			screen_draw_move(&ctx, 0, j);
			input_store_one(c->out, CODE_INSERTCHARACTER, 1);
		}
		screen_draw_column(&ctx, 0);
		window_scroll_draw_position(w, &ctx);
		screen_draw_stop(&ctx);
		input_store_zero(c->out, CODE_CURSOROFF);
		
		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}
