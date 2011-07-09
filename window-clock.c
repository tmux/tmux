/* $Id$ */

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
#include <time.h>

#include "tmux.h"

struct screen *window_clock_init(struct window_pane *);
void	window_clock_free(struct window_pane *);
void	window_clock_resize(struct window_pane *, u_int, u_int);
void	window_clock_key(struct window_pane *, struct session *, int);
void	window_clock_timer(struct window_pane *);

void	window_clock_draw_screen(struct window_pane *);

const struct window_mode window_clock_mode = {
	window_clock_init,
	window_clock_free,
	window_clock_resize,
	window_clock_key,
	NULL,
	window_clock_timer,
};

struct window_clock_mode_data {
	struct screen	        screen;
	time_t			tim;
};

struct screen *
window_clock_init(struct window_pane *wp)
{
	struct window_clock_mode_data	*data;
	struct screen			*s;

	wp->modedata = data = xmalloc(sizeof *data);
	data->tim = time(NULL);

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	s->mode &= ~MODE_CURSOR;

	window_clock_draw_screen(wp);

	return (s);
}

void
window_clock_free(struct window_pane *wp)
{
	struct window_clock_mode_data	*data = wp->modedata;

	screen_free(&data->screen);
	xfree(data);
}

void
window_clock_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_clock_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	screen_resize(s, sx, sy);
	window_clock_draw_screen(wp);
}

/* ARGSUSED */
void
window_clock_key(
    struct window_pane *wp, unused struct session *sess, unused int key)
{
	window_pane_reset_mode(wp);
}

void
window_clock_timer(struct window_pane *wp)
{
	struct window_clock_mode_data	*data = wp->modedata;
	struct tm			 now, then;
	time_t				 t;

	t = time(NULL);
	gmtime_r(&t, &now);
	gmtime_r(&data->tim, &then);
	if (now.tm_min == then.tm_min)
		return;
	data->tim = t;

	window_clock_draw_screen(wp);
	server_redraw_window(wp->window);
}

void
window_clock_draw_screen(struct window_pane *wp)
{
	struct window_clock_mode_data	*data = wp->modedata;
	struct screen_write_ctx	 	 ctx;
	int				 colour, style;

	colour = options_get_number(&wp->window->options, "clock-mode-colour");
	style = options_get_number(&wp->window->options, "clock-mode-style");

	screen_write_start(&ctx, NULL, &data->screen);
	clock_draw(&ctx, colour, style);
	screen_write_stop(&ctx);
}
