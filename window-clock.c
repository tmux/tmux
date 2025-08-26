/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

static struct screen *window_clock_init(struct window_mode_entry *,
		    struct cmd_find_state *, struct args *);
static void	window_clock_free(struct window_mode_entry *);
static void	window_clock_resize(struct window_mode_entry *, u_int, u_int);
static void	window_clock_key(struct window_mode_entry *, struct client *,
		     struct session *, struct winlink *, key_code,
		     struct mouse_event *);

static void	window_clock_timer_callback(int, short, void *);
static void	window_clock_draw_screen(struct window_mode_entry *);

const struct window_mode window_clock_mode = {
	.name = "clock-mode",

	.init = window_clock_init,
	.free = window_clock_free,
	.resize = window_clock_resize,
	.key = window_clock_key,
};

struct window_clock_mode_data {
	struct screen	        screen;
	time_t			tim;
	struct event		timer;
};

const char window_clock_table[14][5][5] = {
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

static void
window_clock_timer_callback(__unused int fd, __unused short events, void *arg)
{
	struct window_mode_entry	*wme = arg;
	struct window_pane		*wp = wme->wp;
	struct window_clock_mode_data	*data = wme->data;
	struct tm			 now, then;
	time_t				 t;
	struct timeval			 tv = { .tv_sec = 1 };

	evtimer_del(&data->timer);
	evtimer_add(&data->timer, &tv);

	if (TAILQ_FIRST(&wp->modes) != wme)
		return;

	t = time(NULL);
	gmtime_r(&t, &now);
	gmtime_r(&data->tim, &then);
	if (now.tm_min == then.tm_min)
		return;
	data->tim = t;

	window_clock_draw_screen(wme);
	wp->flags |= PANE_REDRAW;
}

static struct screen *
window_clock_init(struct window_mode_entry *wme,
    __unused struct cmd_find_state *fs, __unused struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_clock_mode_data	*data;
	struct screen			*s;
	struct timeval			 tv = { .tv_sec = 1 };

	wme->data = data = xmalloc(sizeof *data);
	data->tim = time(NULL);

	evtimer_set(&data->timer, window_clock_timer_callback, wme);
	evtimer_add(&data->timer, &tv);

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	s->mode &= ~MODE_CURSOR;

	window_clock_draw_screen(wme);

	return (s);
}

static void
window_clock_free(struct window_mode_entry *wme)
{
	struct window_clock_mode_data	*data = wme->data;

	evtimer_del(&data->timer);
	screen_free(&data->screen);
	free(data);
}

static void
window_clock_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_clock_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;

	screen_resize(s, sx, sy, 0);
	window_clock_draw_screen(wme);
}

static void
window_clock_key(struct window_mode_entry *wme, __unused struct client *c,
    __unused struct session *s, __unused struct winlink *wl,
    __unused key_code key, __unused struct mouse_event *m)
{
	window_pane_reset_mode(wme->wp);
}

static void
window_clock_draw_screen(struct window_mode_entry *wme)
{
	struct window_pane		*wp = wme->wp;
	struct window_clock_mode_data	*data = wme->data;
	struct screen_write_ctx		 ctx;
	int				 colour, style;
	struct screen			*s = &data->screen;
	struct grid_cell		 gc;
	char				 tim[64], *ptr;
	time_t				 t;
	struct tm			*tm;
	u_int				 i, j, x, y, idx;

	colour = options_get_number(wp->window->options, "clock-mode-colour");
	style = options_get_number(wp->window->options, "clock-mode-style");

	screen_write_start(&ctx, s);

	t = time(NULL);
	tm = localtime(&t);
	if (style == 0) {
		strftime(tim, sizeof tim, "%l:%M ", localtime(&t));
		if (tm->tm_hour >= 12)
			strlcat(tim, "PM", sizeof tim);
		else
			strlcat(tim, "AM", sizeof tim);
	} else
		strftime(tim, sizeof tim, "%H:%M", tm);

	screen_write_clearscreen(&ctx, 8);

	if (screen_size_x(s) < 6 * strlen(tim) || screen_size_y(s) < 6) {
		if (screen_size_x(s) >= strlen(tim) && screen_size_y(s) != 0) {
			x = (screen_size_x(s) / 2) - (strlen(tim) / 2);
			y = screen_size_y(s) / 2;
			screen_write_cursormove(&ctx, x, y, 0);

			memcpy(&gc, &grid_default_cell, sizeof gc);
			gc.flags |= GRID_FLAG_NOPALETTE;
			gc.fg = colour;
			screen_write_puts(&ctx, &gc, "%s", tim);
		}

		screen_write_stop(&ctx);
		return;
	}

	x = (screen_size_x(s) / 2) - 3 * strlen(tim);
	y = (screen_size_y(s) / 2) - 3;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.flags |= GRID_FLAG_NOPALETTE;
	gc.bg = colour;
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
			for (i = 0; i < 5; i++) {
				screen_write_cursormove(&ctx, x + i, y + j, 0);
				if (window_clock_table[idx][j][i])
					screen_write_putc(&ctx, &gc, ' ');
			}
		}
		x += 6;
	}

	screen_write_stop(&ctx);
}
