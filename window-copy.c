/* $Id: window-copy.c,v 1.7 2007-11-26 20:36:30 nicm Exp $ */

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

void	window_copy_init(struct window *);
void	window_copy_resize(struct window *, u_int, u_int);
void	window_copy_draw(struct window *, struct buffer *, u_int, u_int);
void	window_copy_key(struct window *, int);

void	window_copy_draw_position(struct window *, struct screen_draw_ctx *);

void	window_copy_copy_selection(struct window *);
void	window_copy_copy_line(
	     struct window *, char **, size_t *, size_t *, u_int, u_int, u_int);
u_int	window_copy_find_length(struct window *, u_int);
void	window_copy_move_cursor(struct window *);
void	window_copy_cursor_bol(struct window *);
void	window_copy_cursor_eol(struct window *);
void	window_copy_cursor_left(struct window *);
void	window_copy_cursor_right(struct window *);
void	window_copy_cursor_up(struct window *);
void	window_copy_cursor_down(struct window *);
void	window_copy_draw_lines(struct window *, u_int, u_int);
void	window_copy_scroll_left(struct window *, u_int);
void	window_copy_scroll_right(struct window *, u_int);
void	window_copy_scroll_up(struct window *, u_int);
void	window_copy_scroll_down(struct window *, u_int);

const struct window_mode window_copy_mode = {
	window_copy_init,
	window_copy_resize,
	window_copy_draw,
	window_copy_key
};

struct window_copy_mode_data {
	u_int	ox;
	u_int	oy;
	u_int	cx;
	u_int	cy;
	u_int	size;

	int	selflag;
	u_int	selx;
	u_int	sely;
};

void
window_copy_init(struct window *w)
{
	struct window_copy_mode_data	*data;

	w->modedata = data = xmalloc(sizeof *data);
	data->ox = data->oy = 0;
	data->cx = w->screen.cx;
	data->cy = w->screen.cy;
	data->size = w->screen.hsize;
	data->selflag = 0;
	
	w->screen.mode |= (MODE_BACKGROUND|MODE_BGCURSOR);
}

void
window_copy_resize(unused struct window *w, unused u_int sx, unused u_int sy)
{
}

void
window_copy_draw_position(struct window *w, struct screen_draw_ctx *ctx)
{
	struct window_copy_mode_data	*data = w->modedata;
	char				*ptr, buf[32];
	size_t	 			 len;

	len = xsnprintf(
	    buf, sizeof buf, "[%u,%u/%u]", data->ox, data->oy, data->size);
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
window_copy_draw(struct window *w, struct buffer *b, u_int py, u_int ny)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;

	if (s->hsize != data->size) {
		data->oy += s->hsize - data->size;
		data->size = s->hsize;
	}

	screen_draw_start(&ctx, s, b, data->ox, data->oy);
	screen_draw_set_selection(&ctx, data->selflag, data->selx, data->sely,
	    data->cx + data->ox, data->size + data->cy - data->oy);
	if (py != 0)
		screen_draw_lines(&ctx, py, ny);
	else if (ny > 1)
		screen_draw_lines(&ctx, py + 1, ny - 1);

	if (py == 0)
		window_copy_draw_position(w, &ctx);

	screen_draw_stop(&ctx);

	input_store_two(b, CODE_CURSORMOVE, data->cy + 1, data->cx  + 1);
}

void
window_copy_key(struct window *w, int key)
{
	struct window_copy_mode_data	*data = w->modedata;
	u_int				 oy, sy;
	
	sy = screen_size_y(&w->screen);
	oy = data->oy;

	switch (key) {
	case 'Q':
	case 'q':
		goto done;
	case 'h':
	case KEYC_LEFT:
		window_copy_cursor_left(w);
		return;
	case 'l':
	case KEYC_RIGHT:
		window_copy_cursor_right(w);
 		return;
	case 'k':
	case 'K':
	case KEYC_UP:
		window_copy_cursor_up(w);
		return;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		window_copy_cursor_down(w);
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
	case '\000':	/* C-space */
	case ' ':
		data->selflag = !data->selflag;
		data->selx = data->cx + data->ox;
		data->sely = data->size + data->cy - data->oy;
		oy = -1;	/* XXX */
		break;
	case '\033':
		data->selflag = 0;
		oy = -1;	/* XXX */
		break;		
	case '\027':	/* C-w */
	case '\r':	/* enter */
		if (data->selflag)
			window_copy_copy_selection(w);
		goto done;
	case '\001':	/* C-a */
		window_copy_cursor_bol(w);
		return;
	case '\005':	/* C-e */
		window_copy_cursor_eol(w);
		return;
	}
	if (data->oy != oy) {
		server_redraw_window_all(w);
		window_copy_move_cursor(w);
	}
	return;

done:
	w->mode = NULL;
	xfree(w->modedata);
	
	w->screen.mode &= ~MODE_BACKGROUND;
	
	recalculate_sizes();
	server_redraw_window_all(w);
}

void
window_copy_copy_selection(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	char				*buf;
	size_t	 			 len, off;
	u_int	 			 i, xx, yy, sx, sy, ex, ey;

	len = BUFSIZ;
	buf = xmalloc(len);
	off = 0;

	*buf = '\0';

	/* Find start and end. */
	xx = data->cx + data->ox;
	yy = data->size + data->cy - data->oy;
	if (xx < data->selx || (yy == data->sely && xx < data->selx)) {
		sx = xx; sy = yy;
		ex = data->selx; ey = data->sely;
	} else {
		sx = data->selx; sy = data->sely;
		ex = xx; ey = yy;
	}

	/* Trim ex to end of line. */
	xx = window_copy_find_length(w, ey);
	if (ex > xx)
		ex = xx;

	log_debug("copying from %u,%u to %u,%u", sx, sy, ex, ey);

	/* Copy the lines. */
	if (sy == ey)
		window_copy_copy_line(w, &buf, &off, &len, sy, sx, ex);
	else {
		xx = window_copy_find_length(w, sy);
		window_copy_copy_line(w, &buf, &off, &len, sy, sx, xx);
		if (ey - sy > 1) {
			for (i = sy + 1; i < ey - 1; i++) {
				xx = window_copy_find_length(w, i);
				window_copy_copy_line(
				    w, &buf, &off, &len, i, 0, xx);
			}
		}
		window_copy_copy_line(w, &buf, &off, &len, ey, 0, ex);
	}

	/* Terminate buffer, overwriting final \n. */
	if (off != 0)
		buf[off - 1] = '\0';

	if (paste_buffer != NULL)
		xfree(paste_buffer);
	paste_buffer = buf;
}

void
window_copy_copy_line(struct window *w,
    char **buf, size_t *off, size_t *len, u_int sy, u_int sx, u_int ex)
{
	struct screen	*s = &w->screen;
	u_char		 i, xx;

	if (sx > ex)
		return;

	xx = window_copy_find_length(w, sy);
	if (ex > xx)
		ex = xx;
	if (sx > xx)
		sx = xx;

	if (sx < ex) {
		for (i = sx; i < ex; i++) {
			*buf = ensure_size(*buf, len, 1, *off + 1);
			(*buf)[*off] = s->grid_data[sy][i];
			(*off)++;
		}
	}
		
	*buf = ensure_size(*buf, len, 1, *off + 1);
	(*buf)[*off] = '\n';
	(*off)++;
}

u_int
window_copy_find_length(struct window *w, u_int py)
{
	struct screen	*s = &w->screen;
	u_int		 px;

	px = s->grid_size[py];
	while (px > 0 && s->grid_data[py][px] == SCREEN_DEFDATA)
		px--;
	return (px);
}

void
window_copy_move_cursor(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct client			*c;
	u_int		 		 i;
	struct hdr			 hdr;
	size_t				 size;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;

		buffer_ensure(c->out, sizeof hdr);
		buffer_add(c->out, sizeof hdr);
		size = BUFFER_USED(c->out);

		input_store_two(
		    c->out, CODE_CURSORMOVE, data->cy + 1, data->cx + 1);

		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}
}

void
window_copy_cursor_bol(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;

	if (data->ox != 0)
		window_copy_scroll_right(w, data->ox);
	data->cx = 0;
	window_copy_move_cursor(w);
}

void
window_copy_cursor_eol(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	u_int				 xx;

	xx = window_copy_find_length(w, data->size + data->cy - data->oy);

	/* On screen. */
	if (xx > data->ox && xx < data->ox + screen_last_x(s))
		data->cx = xx - data->ox;

	/* Off right of screen. */
	if (xx > data->ox + screen_last_x(s)) {
		/* Move cursor to last and scroll screen. */
		window_copy_scroll_left(w,
		    xx - data->ox - screen_last_x(s));
		data->cx = screen_last_x(s);
	}

	/* Off left of screen. */
	if (xx <= data->ox) {
		if (xx < screen_last_x(s)) {
			/* Short enough to fit on screen. */
			window_copy_scroll_right(w, data->ox);
			data->cx = xx;
		} else {
			/* Too long to fit on screen. */
			window_copy_scroll_right(
			    w, data->ox - (xx - screen_last_x(s)));
			data->cx = screen_last_x(s);
		}
	}

	window_copy_move_cursor(w);
}

void
window_copy_cursor_left(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;

	if (data->cx == 0)
		window_copy_scroll_right(w, 1); 
	else {
		data->cx--;
		if (data->selflag)
			window_copy_draw_lines(w, data->cy, 1);
	}
	window_copy_move_cursor(w);
}

void
window_copy_cursor_right(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;

	if (data->cx == screen_last_x(s))
		window_copy_scroll_left(w, 1);
	else {
		data->cx++;
		if (data->selflag)
			window_copy_draw_lines(w, data->cy, 1);
	}
	window_copy_move_cursor(w);
}

void
window_copy_cursor_up(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;

	if (data->cy == 0)
		window_copy_scroll_down(w, 1);
	else {
		data->cy--;
		if (data->selflag)	
			window_copy_draw_lines(w, data->cy, 2);
	}
	window_copy_move_cursor(w);
}

void
window_copy_cursor_down(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;

	if (data->cy == screen_last_y(s))
		window_copy_scroll_up(w, 1);
	else {
		data->cy++;
		if (data->selflag)	
			window_copy_draw_lines(w, data->cy - 1, 2);
	}
	window_copy_move_cursor(w);
}

void
window_copy_draw_lines(struct window *w, u_int py, u_int ny)
{
	struct client		*c;
	struct buffer		*b;
	u_int		 	 i;
	struct hdr		 hdr;
	size_t			 size;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;
		b = c->out;

		buffer_ensure(b, sizeof hdr);
		buffer_add(b, sizeof hdr);
		size = BUFFER_USED(b);
		
		window_copy_draw(w, b, py, ny);

		size = BUFFER_USED(b) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(b) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_copy_scroll_left(struct window *w, u_int nx)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	struct buffer			*b;
	u_int		 		 i, j;
	struct hdr			 hdr;
	size_t				 size;

	if (data->ox > SHRT_MAX - nx)
		nx = SHRT_MAX - data->ox;
	if (nx == 0)
		return;
	data->ox += nx;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;
		b = c->out;

		buffer_ensure(b, sizeof hdr);
		buffer_add(b, sizeof hdr);
		size = BUFFER_USED(b);
		
		screen_draw_start(&ctx, s, b, data->ox, data->oy);
		screen_draw_set_selection(&ctx,
		    data->selflag, data->selx, data->sely,
		    data->cx + data->ox, data->size + data->cy - data->oy);
		for (j = 1; j < screen_size_y(s); j++) {
			screen_draw_move(&ctx, 0, j);
			input_store_one(b, CODE_DELETECHARACTER, nx);
		}
		for (j = 0; j < nx; j++)
			screen_draw_column(&ctx, screen_last_x(s) - j);
		window_copy_draw_position(w, &ctx);
		screen_draw_stop(&ctx);
		
		size = BUFFER_USED(b) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(b) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_copy_scroll_right(struct window *w, u_int nx)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	struct buffer			*b;
	u_int		 		 i, j;
	struct hdr			 hdr;
	size_t				 size;

	if (data->ox < nx)
		nx = data->ox;
	if (nx == 0)
		return;
	data->ox -= nx;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (!session_has(c->session, w))
			continue;
		b = c->out;

		buffer_ensure(b, sizeof hdr);
		buffer_add(b, sizeof hdr);
		size = BUFFER_USED(b);
		
		screen_draw_start(&ctx, s, b, data->ox, data->oy);
		screen_draw_set_selection(&ctx,
		    data->selflag, data->selx, data->sely,
		    data->cx + data->ox, data->size + data->cy - data->oy);
		for (j = 1; j < screen_size_y(s); j++) {
			screen_draw_move(&ctx, 0, j);
			input_store_one(b, CODE_INSERTCHARACTER, nx);
		}
		for (j = 0; j < nx; j++)
			screen_draw_column(&ctx, j);
		window_copy_draw_position(w, &ctx);
		screen_draw_stop(&ctx);
		
		size = BUFFER_USED(b) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(b) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_copy_scroll_up(struct window *w, u_int ny)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i;
	struct hdr			 hdr;
	size_t				 size;

	if (data->oy < ny)
		ny = data->oy;
	if (ny == 0)
		return;
	data->oy -= ny;

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
		screen_draw_set_selection(&ctx,
		    data->selflag, data->selx, data->sely,
		    data->cx + data->ox, data->size + data->cy - data->oy);
		screen_draw_move(&ctx, 0, 0);
		input_store_one(c->out, CODE_DELETELINE, ny);
		for (i = 0; i < ny; i++)
			screen_draw_line(&ctx, screen_last_y(s) - i);
		if (data->selflag)
			screen_draw_line(&ctx, screen_last_y(s) - ny);
		window_copy_draw_position(w, &ctx);
		screen_draw_stop(&ctx);
		
		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}

void
window_copy_scroll_down(struct window *w, u_int ny)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	struct screen_draw_ctx		 ctx;
	struct client			*c;
	u_int		 		 i;
	struct hdr			 hdr;
	size_t				 size;

	if (ny > data->size)
		return;

	if (data->oy > data->size - ny)
		ny = data->size - data->oy;
	if (ny == 0)
		return;
	data->oy += ny;

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
		screen_draw_set_selection(&ctx,
		    data->selflag, data->selx, data->sely,
		    data->cx + data->ox, data->size + data->cy - data->oy);
		screen_draw_move(&ctx, 0, 0);
		input_store_one(c->out, CODE_INSERTLINE, ny);
		for (i = 1; i < ny + 1; i++)
			screen_draw_line(&ctx, i);
		window_copy_draw_position(w, &ctx);
		screen_draw_stop(&ctx);

		size = BUFFER_USED(c->out) - size;
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	}	
}
