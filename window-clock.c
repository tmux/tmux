/* $Id: window-clock.c,v 1.1 2009-01-10 19:35:40 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

struct screen *window_clock_init(struct window *);
void	window_clock_free(struct window *);
void	window_clock_resize(struct window *, u_int, u_int);
void	window_clock_key(struct window *, struct client *, int);
void	window_clock_timer(struct window *);

void	window_clock_draw_screen(struct window *);

const struct window_mode window_clock_mode = {
	window_clock_init,
	window_clock_free,
	window_clock_resize,
	window_clock_key,
	window_clock_timer
};

struct window_clock_mode_data {
	struct screen	        screen;
};

struct screen *
window_clock_init(struct window *w)
{
	struct window_clock_mode_data	*data;
	struct screen			*s;

	w->modedata = data = xmalloc(sizeof *data);

	s = &data->screen;
	screen_init(s, screen_size_x(&w->base), screen_size_y(&w->base), 0);
	s->mode &= ~MODE_CURSOR;

	window_clock_draw_screen(w);

	return (s);
}

void
window_clock_free(struct window *w)
{
	struct window_clock_mode_data	*data = w->modedata;

	screen_free(&data->screen);
	xfree(data);
}

void
window_clock_resize(struct window *w, u_int sx, u_int sy)
{
	struct window_clock_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

 	screen_resize(s, sx, sy);
	window_clock_draw_screen(w);
}

void
window_clock_key(struct window *w, unused struct client *c, unused int key)
{
	window_reset_mode(w);
}

void
window_clock_timer(struct window *w)
{
	window_clock_draw_screen(w);
	server_redraw_window(w);
}

void
window_clock_draw_screen(struct window *w)
{
	struct window_clock_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	struct grid_cell		 gc;
	char				 tim[64], *ptr;
	time_t				 t;
	u_int				 colour, i, j, x, y, idx;
	char				 table[14][5][5] = {
 		{ { 1,1,1,1,1 }, /* 0 */
		  { 1,0,0,0,1 },
		  { 1,0,0,0,1 },
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 } },
 		{ { 0,0,0,0,1 }, /* 1 */
		  { 0,0,0,0,1 },
		  { 0,0,0,0,1 },
		  { 0,0,0,0,1 },
		  { 0,0,0,0,1 } },
 		{ { 1,1,1,1,1 }, /* 2 */
		  { 0,0,0,0,1 },
		  { 1,1,1,1,1 },
		  { 1,0,0,0,0 },
		  { 1,1,1,1,1 } },
 		{ { 1,1,1,1,1 }, /* 3 */
		  { 0,0,0,0,1 },
		  { 1,1,1,1,1 },
		  { 0,0,0,0,1 },
		  { 1,1,1,1,1 } },
 		{ { 1,0,0,0,1 }, /* 4 */
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 },
		  { 0,0,0,0,1 },
		  { 0,0,0,0,1 } },
 		{ { 1,1,1,1,1 }, /* 5 */
		  { 1,0,0,0,0 },
		  { 1,1,1,1,1 },
		  { 0,0,0,0,1 },
		  { 1,1,1,1,1 } },
 		{ { 1,1,1,1,1 }, /* 6 */
		  { 1,0,0,0,0 },
		  { 1,1,1,1,1 },
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 } },
 		{ { 1,1,1,1,1 }, /* 7 */
		  { 0,0,0,0,1 },
		  { 0,0,0,0,1 },
		  { 0,0,0,0,1 },
		  { 0,0,0,0,1 } },
 		{ { 1,1,1,1,1 }, /* 8 */
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 },
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 } },
 		{ { 1,1,1,1,1 }, /* 9 */
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 },
		  { 0,0,0,0,1 },
		  { 1,1,1,1,1 } },
 		{ { 0,0,0,0,0 }, /* : */
		  { 0,0,1,0,0 },
		  { 0,0,0,0,0 },
		  { 0,0,1,0,0 },
		  { 0,0,0,0,0 } },
 		{ { 1,1,1,1,1 }, /* A */
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 },
		  { 1,0,0,0,1 },
		  { 1,0,0,0,1 } },
 		{ { 1,1,1,1,1 }, /* P */
		  { 1,0,0,0,1 },
		  { 1,1,1,1,1 },
		  { 1,0,0,0,0 },
		  { 1,0,0,0,0 } },
 		{ { 1,0,0,0,1 }, /* M */
		  { 1,1,0,1,1 },
		  { 1,0,1,0,1 },
		  { 1,0,0,0,1 },
		  { 1,0,0,0,1 } },
	};

	colour = options_get_number(&w->options, "clock-mode-colour");

	t = time(NULL);
	if (options_get_number(&w->options, "clock-mode-style") == 0)
		strftime(tim, sizeof tim, "%l:%M %p", localtime(&t));
	else
		strftime(tim, sizeof tim, "%H:%M", localtime(&t));

	screen_write_start(&ctx, s, NULL, NULL);
	screen_write_clearscreen(&ctx);
	memcpy(&gc, &grid_default_cell, sizeof gc);

	if (screen_size_x(s) < 6 * strlen(tim) || screen_size_y(s) < 6) {
		if (screen_size_x(s) >= strlen(tim) && screen_size_y(s) != 0) {
			x = (screen_size_x(s) / 2) - (strlen(tim) / 2);
			y = screen_size_y(s) / 2;
			screen_write_cursormove(&ctx, x, y);

			gc.fg = colour;
			screen_write_puts(&ctx, &gc, "%s", tim);
		}
		screen_write_stop(&ctx);
		return;
	}		

	x = (screen_size_x(s) / 2) - 3 * strlen(tim);
	y = (screen_size_y(s) / 2) - 3;

	for (ptr = tim; *ptr != '\0'; ptr++) {
		if (*ptr >= '0' && *ptr <= '9')
			idx = *ptr - '0';
 		else if (*ptr == ':')
			idx = 10;
 		else if (*ptr == 'A')
			idx = 11;
 		else if (*ptr == 'P')
			idx = 12;
 		else if (*ptr == 'M')
			idx = 13;
		else {
			x += 6;
			continue;
		}

		for (j = 0; j < 5; j++) {
			screen_write_cursormove(&ctx, x, y + j);
			for (i = 0; i < 5; i++) {
				if (table[idx][j][i])
					gc.bg = colour;
				else
					gc.bg = 0;
				screen_write_putc(&ctx, &gc, ' ');
			}
		}
		x += 6;
	}

	screen_write_stop(&ctx);
}
