/* $OpenBSD: window-choose.c,v 1.2 2009/06/24 23:00:31 nicm Exp $ */

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

struct screen *window_choose_init(struct window_pane *);
void	window_choose_free(struct window_pane *);
void	window_choose_resize(struct window_pane *, u_int, u_int);
void	window_choose_key(struct window_pane *, struct client *, int);
void	window_choose_mouse(
    	    struct window_pane *, struct client *, u_char, u_char, u_char);

void	window_choose_redraw_screen(struct window_pane *);
void	window_choose_write_line(
    	    struct window_pane *, struct screen_write_ctx *, u_int);

void	window_choose_scroll_up(struct window_pane *);
void	window_choose_scroll_down(struct window_pane *);

const struct window_mode window_choose_mode = {
	window_choose_init,
	window_choose_free,
	window_choose_resize,
	window_choose_key,
	window_choose_mouse,
	NULL,
};

struct window_choose_mode_item {
	char		       *name;
	int			idx;
};

struct window_choose_mode_data {
	struct screen	        screen;

	struct mode_key_data	mdata;

	ARRAY_DECL(, struct window_choose_mode_item) list;
	u_int			top;
	u_int			selected;

	void 			(*callback)(void *, int);
	void		       *data;
};

void
window_choose_vadd(struct window_pane *wp, int idx, const char *fmt, va_list ap)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;

	ARRAY_EXPAND(&data->list, 1);
	item = &ARRAY_LAST(&data->list);
	xvasprintf(&item->name, fmt, ap);
	item->idx = idx;
}

void printflike3
window_choose_add(struct window_pane *wp, int idx, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	window_choose_vadd(wp, idx, fmt, ap);
	va_end(ap);
}

void
window_choose_ready(struct window_pane *wp,
    u_int cur, void (*callback)(void *, int), void *cdata)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	data->selected = cur;
	if (data->selected > screen_size_y(s) - 1)
		data->top = ARRAY_LENGTH(&data->list) - screen_size_y(s);

	data->callback = callback;
	data->data = cdata;

	window_choose_redraw_screen(wp);
}

struct screen *
window_choose_init(struct window_pane *wp)
{
	struct window_choose_mode_data	*data;
	struct screen			*s;

	wp->modedata = data = xmalloc(sizeof *data);
	data->callback = NULL;
	ARRAY_INIT(&data->list);
	data->top = 0;

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	s->mode &= ~MODE_CURSOR;
	s->mode |= MODE_MOUSE;

	mode_key_init(&data->mdata,
	    options_get_number(&wp->window->options, "mode-keys"),
	    MODEKEY_CHOOSEMODE);

	return (s);
}

void
window_choose_free(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	u_int				 i;

 	mode_key_free(&data->mdata);

	for (i = 0; i < ARRAY_LENGTH(&data->list); i++)
		xfree(ARRAY_ITEM(&data->list, i).name);
	ARRAY_FREE(&data->list);

	screen_free(&data->screen);
	xfree(data);
}

void
window_choose_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	data->top = 0;
	if (data->selected > sy - 1)
		data->top = data->selected - (sy - 1);

	screen_resize(s, sx, sy);
	window_choose_redraw_screen(wp);
}

void
window_choose_key(struct window_pane *wp, unused struct client *c, int key)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	struct window_choose_mode_item	*item;
	u_int				 items;

	items = ARRAY_LENGTH(&data->list);

	switch (mode_key_lookup(&data->mdata, key)) {
	case MODEKEYCMD_QUIT:
		data->callback(data->data, -1);
		window_pane_reset_mode(wp);
		break;
	case MODEKEYCMD_CHOOSE:
		item = &ARRAY_ITEM(&data->list, data->selected);
		data->callback(data->data, item->idx);
		window_pane_reset_mode(wp);
		break;
	case MODEKEYCMD_UP:
		if (items == 0)
			break;
		if (data->selected == 0) {
			data->selected = items - 1;
			if (data->selected > screen_size_y(s) - 1)
				data->top = items - screen_size_y(s);
			window_choose_redraw_screen(wp);
			break;
		}
		data->selected--;
		if (data->selected < data->top)
			window_choose_scroll_up(wp);
		else {
			screen_write_start(&ctx, wp, NULL);
			window_choose_write_line(
			    wp, &ctx, data->selected - data->top);
			window_choose_write_line(
			    wp, &ctx, data->selected + 1 - data->top);
			screen_write_stop(&ctx);
		}
		break;
	case MODEKEYCMD_DOWN:
		if (items == 0)
			break;
		if (data->selected == items - 1) {
			data->selected = 0;
			data->top = 0;
			window_choose_redraw_screen(wp);
			break;
		}
		data->selected++;
		if (data->selected >= data->top + screen_size_y(&data->screen))
			window_choose_scroll_down(wp);
		else {
			screen_write_start(&ctx, wp, NULL);
			window_choose_write_line(
			    wp, &ctx, data->selected - data->top);
			window_choose_write_line(
			    wp, &ctx, data->selected - 1 - data->top);
			screen_write_stop(&ctx);
		}
		break;
	case MODEKEYCMD_PREVIOUSPAGE:
		if (data->selected < screen_size_y(s)) {
			data->selected = 0;
			data->top = 0;
		} else {
			data->selected -= screen_size_y(s);
			if (data->top < screen_size_y(s))
				data->top = 0;
			else
				data->top -= screen_size_y(s);
		}
 		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCMD_NEXTPAGE:
		data->selected += screen_size_y(s);
		if (data->selected > items - 1)
			data->selected = items - 1;
		data->top += screen_size_y(s);
		if (screen_size_y(s) < items) {
			if (data->top + screen_size_y(s) > items)
				data->top = items - screen_size_y(s);
		} else
			data->top = 0;
		if (data->selected < data->top)
			data->top = data->selected;
		window_choose_redraw_screen(wp);
		break;
	default:
		break;
	}
}

void
window_choose_mouse(struct window_pane *wp,
    unused struct client *c, u_char b, u_char x, u_char y)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct window_choose_mode_item	*item;
	u_int				 idx;

	if ((b & 3) == 3)
		return;
	if (x >= screen_size_x(s))
		return;
	if (y >= screen_size_y(s))
		return;

	idx = data->top + y;
	if (idx >= ARRAY_LENGTH(&data->list))
		return;
	data->selected = idx;

	item = &ARRAY_ITEM(&data->list, data->selected);
	data->callback(data->data, item->idx);
	window_pane_reset_mode(wp);
}

void
window_choose_write_line(
    struct window_pane *wp, struct screen_write_ctx *ctx, u_int py)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	struct screen			*s = &data->screen;
	struct grid_cell		 gc;
 	int				 utf8flag;

	if (data->callback == NULL)
		fatalx("called before callback assigned");

	utf8flag = options_get_number(&wp->window->options, "utf8");
	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (data->selected == data->top + py) {
		gc.fg = options_get_number(&wp->window->options, "mode-bg");
		gc.bg = options_get_number(&wp->window->options, "mode-fg");
		gc.attr |= options_get_number(&wp->window->options, "mode-attr");
	}

	screen_write_cursormove(ctx, 0, py);
	if (data->top + py  < ARRAY_LENGTH(&data->list)) {
		item = &ARRAY_ITEM(&data->list, data->top + py);
		screen_write_nputs(
		    ctx, screen_size_x(s) - 1, &gc, utf8flag, "%s", item->name);
	}
	while (s->cx < screen_size_x(s))
		screen_write_putc(ctx, &gc, ' ');
}

void
window_choose_redraw_screen(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start(&ctx, wp, NULL);
	for (i = 0; i < screen_size_y(s); i++)
		window_choose_write_line(wp, &ctx, i);
	screen_write_stop(&ctx);
}

void
window_choose_scroll_up(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen_write_ctx		 ctx;

	if (data->top == 0)
		return;
	data->top--;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_insertline(&ctx, 1);
	window_choose_write_line(wp, &ctx, 0);
	if (screen_size_y(&data->screen) > 1)
		window_choose_write_line(wp, &ctx, 1);
	screen_write_stop(&ctx);
}

void
window_choose_scroll_down(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->top >= ARRAY_LENGTH(&data->list))
		return;
	data->top++;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_deleteline(&ctx, 1);
	window_choose_write_line(wp, &ctx, screen_size_y(s) - 1);
	if (screen_size_y(&data->screen) > 1)
		window_choose_write_line(wp, &ctx, screen_size_y(s) - 2);
	screen_write_stop(&ctx);
}
