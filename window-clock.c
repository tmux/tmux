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
	struct screen		screen;
	time_t			tim;
	struct event		timer;
	int 			style;
};

const char window_clock_table[37][5][5] = {
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
	{ { 1,1,1,1,0 }, /* B */
	  { 1,0,0,0,1 },
	  { 1,1,1,1,0 },
	  { 1,0,0,0,1 },
	  { 1,1,1,1,0 } },
	{ { 1,1,1,1,1 }, /* C */
	  { 1,0,0,0,0 },
	  { 1,0,0,0,0 },
	  { 1,0,0,0,0 },
	  { 1,1,1,1,1 } },
	{ { 1,1,1,1,0 }, /* D */
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 1,1,1,1,0 } },
	{ { 1,1,1,1,1 }, /* E */
	  { 1,0,0,0,0 },
	  { 1,1,1,1,0 },
	  { 1,0,0,0,0 },
	  { 1,1,1,1,1 } },
	{ { 1,1,1,1,1 }, /* F */
	  { 1,0,0,0,0 },
	  { 1,1,1,1,0 },
	  { 1,0,0,0,0 },
	  { 1,0,0,0,0 } },
	{ { 1,1,1,1,1 }, /* G */
	  { 1,0,0,0,0 },
	  { 1,0,1,1,1 },
	  { 1,0,0,0,1 },
	  { 1,1,1,1,1 } },
	{ { 1,0,0,0,1 }, /* H */
	  { 1,0,0,0,1 },
	  { 1,1,1,1,1 },
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 } },
	{ { 1,1,1,1,1 }, /* I */
	  { 0,0,1,0,0 },
	  { 0,0,1,0,0 },
	  { 0,0,1,0,0 },
	  { 1,1,1,1,1 } },
	{ { 0,0,0,0,1 }, /* J */
	  { 0,0,0,0,1 },
	  { 0,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 0,1,1,1,0 } },
	{ { 1,0,0,0,1 }, /* K */
	  { 1,0,0,1,0 },
	  { 1,1,1,0,0 },
	  { 1,0,0,1,0 },
	  { 1,0,0,0,1 } },
	{ { 1,0,0,0,0 }, /* L */
	  { 1,0,0,0,0 },
	  { 1,0,0,0,0 },
	  { 1,0,0,0,0 },
	  { 1,1,1,1,1 } },
	{ { 1,0,0,0,1 }, /* M */
	  { 1,1,0,1,1 },
	  { 1,0,1,0,1 },
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 } },
	{ { 1,0,0,0,1 }, /* N */
	  { 1,1,0,0,1 },
	  { 1,0,1,0,1 },
	  { 1,0,0,1,1 },
	  { 1,0,0,0,1 } },
	{ { 1,1,1,1,1 }, /* O */
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 1,1,1,1,1 } },
	{ { 1,1,1,1,1 }, /* P */
	  { 1,0,0,0,1 },
	  { 1,1,1,1,1 },
	  { 1,0,0,0,0 },
	  { 1,0,0,0,0 } },
	{ { 1,1,1,1,1 }, /* Q */
	  { 1,0,0,0,1 },
	  { 1,0,1,0,1 },
	  { 1,0,0,1,1 },
	  { 1,1,1,1,1 } },
	{ { 1,1,1,1,1 }, /* R */
	  { 1,0,0,0,1 },
	  { 1,1,1,1,1 },
	  { 1,0,0,1,0 },
	  { 1,0,0,0,1 } },
	{ { 1,1,1,1,1 }, /* S */
	  { 1,0,0,0,0 },
	  { 1,1,1,1,1 },
	  { 0,0,0,0,1 },
	  { 1,1,1,1,1 } },
	{ { 1,1,1,1,1 }, /* T */
	  { 0,0,1,0,0 },
	  { 0,0,1,0,0 },
	  { 0,0,1,0,0 },
	  { 0,0,1,0,0 } },
	{ { 1,0,0,0,1 }, /* U */
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 1,1,1,1,1 } },
	{ { 1,0,0,0,1 }, /* V */
	  { 1,0,0,0,1 },
	  { 1,0,0,0,1 },
	  { 0,1,0,1,0 },
	  { 0,0,1,0,0 } },
	{ { 1,0,0,0,1 }, /* W */
	  { 1,0,0,0,1 },
	  { 1,0,1,0,1 },
	  { 1,1,0,1,1 },
	  { 1,0,0,0,1 } },
	{ { 1,0,0,0,1 }, /* X */
	  { 0,1,0,1,0 },
	  { 0,0,1,0,0 },
	  { 0,1,0,1,0 },
	  { 1,0,0,0,1 } },
	{ { 1,0,0,0,1 }, /* Y */
	  { 0,1,0,1,0 },
	  { 0,0,1,0,0 },
	  { 0,0,1,0,0 },
	  { 0,0,1,0,0 } },
	{ { 1,1,1,1,1 }, /* Z */
	  { 0,0,0,1,0 },
	  { 0,0,1,0,0 },
	  { 0,1,0,0,0 },
	  { 1,1,1,1,1 } },
};

static void
window_clock_start_timer(struct window_mode_entry *wme)
{
	struct window_clock_mode_data	*data = wme->data;
	struct timeval			 tv;
	struct timespec			 ts;
	long				 delay;

	clock_gettime(CLOCK_REALTIME, &ts);
	delay = 1000000 - (ts.tv_nsec / 1000);

	tv.tv_sec = delay / 1000000;
	tv.tv_usec = delay % 1000000;
	if (tv.tv_sec < 0 || (tv.tv_sec == 0 && tv.tv_usec <= 0)) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;
	}
	evtimer_add(&data->timer, &tv);
}

static void
window_clock_timer_callback(__unused int fd, __unused short events, void *arg)
{
	struct window_mode_entry	*wme = arg;
	struct window_pane		*wp = wme->wp;
	struct window_clock_mode_data	*data = wme->data;
	struct tm			 now, then;
	time_t				 t;

	evtimer_del(&data->timer);

	t = time(NULL);
	gmtime_r(&t, &now);
	gmtime_r(&data->tim, &then);

	if (now.tm_sec != then.tm_sec) {
		data->tim = t;
		window_clock_draw_screen(wme);
		wp->flags |= PANE_REDRAW;
	}

	window_clock_start_timer(wme);
}

static struct screen *
window_clock_init(struct window_mode_entry *wme,
    __unused struct cmd_find_state *fs, __unused struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_clock_mode_data	*data;
	struct screen			*s;

	wme->data = data = xmalloc(sizeof *data);
	data->tim = time(NULL);

	evtimer_set(&data->timer, window_clock_timer_callback, wme);
	window_clock_start_timer(wme);

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
	char				 tim[64], *ptr, *old_tz;
	const char			*timeformat, *timezone;
	struct tm			 tm_buf;
	u_int				 i, j, x, y, idx;

	colour = options_get_number(wp->options, "clock-mode-colour");
	style = options_get_number(wp->options, "clock-mode-style");
	timezone = options_get_string(wp->options, "clock-mode-timezone");

	screen_write_start(&ctx, s);

	old_tz = getenv("TZ");
	if (old_tz != NULL)
		old_tz = xstrdup(old_tz);
	if (timezone != NULL && *timezone != '\0')
		setenv("TZ", timezone, 1);
	else
		unsetenv("TZ");
	tzset();

	localtime_r(&data->tim, &tm_buf);
	switch (style) {
		case 0: timeformat = "%l:%M %p";        break;
		case 1: timeformat = "%l:%M:%S %p";     break;
		case 2: timeformat = "%l:%M %p %Z";     break;
		case 3: timeformat = "%l:%M:%S %p %Z";  break;
		case 4: timeformat = "%H:%M";           break;
		case 5: timeformat = "%H:%M:%S";        break;
		case 6: timeformat = "%H:%M %Z";        break;
		case 7: timeformat = "%H:%M:%S %Z";     break;
		default: timeformat = "%H:%M";
	}

	strftime(tim, sizeof tim, timeformat, &tm_buf);

	if (old_tz != NULL) {
		setenv("TZ", old_tz, 1);
		free(old_tz);
	} else
		unsetenv("TZ");
	tzset();

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
		else if (*ptr >= 'A' && *ptr <= 'Z')
			idx = 11 + (*ptr - 'A');
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
