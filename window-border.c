/* $OpenBSD: window-border.c,v 1.1 2026/06/22 08:47:46 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include "tmux.h"

/* Get border cell. */
void
window_get_border_cell(struct window *w, struct window_pane *wp,
    enum pane_lines pane_lines, int cell_type, struct grid_cell *gc)
{
	u_int	idx;

	if (cell_type == CELL_NONE && w->fill_character != NULL) {
		utf8_copy(&gc->data, &w->fill_character[0]);
		return;
	}

	switch (pane_lines) {
	case PANE_LINES_NUMBER:
		if (cell_type == CELL_NONE) {
			gc->attr |= GRID_ATTR_CHARSET;
			utf8_set(&gc->data, CELL_BORDERS[CELL_NONE]);
			break;
		}
		gc->attr &= ~GRID_ATTR_CHARSET;
		if (wp != NULL && window_pane_index(wp, &idx) == 0)
			utf8_set(&gc->data, '0' + (idx % 10));
		else
			utf8_set(&gc->data, '*');
		break;
	case PANE_LINES_DOUBLE:
		gc->attr &= ~GRID_ATTR_CHARSET;
		utf8_copy(&gc->data, tty_acs_double_borders(cell_type));
		break;
	case PANE_LINES_HEAVY:
		gc->attr &= ~GRID_ATTR_CHARSET;
		utf8_copy(&gc->data, tty_acs_heavy_borders(cell_type));
		break;
	case PANE_LINES_SIMPLE:
		gc->attr &= ~GRID_ATTR_CHARSET;
		utf8_set(&gc->data, SIMPLE_BORDERS[cell_type]);
		break;
	case PANE_LINES_NONE:
	case PANE_LINES_SPACES:
		gc->attr &= ~GRID_ATTR_CHARSET;
		utf8_set(&gc->data, ' ');
		break;
	default:
		gc->attr |= GRID_ATTR_CHARSET;
		utf8_set(&gc->data, CELL_BORDERS[cell_type]);
		break;
	}
}

/* Get pane border cell. */
void
window_pane_get_border_cell(struct window_pane *wp, int cell_type,
    struct grid_cell *gc)
{
	enum pane_lines	pane_lines = window_pane_get_pane_lines(wp);

	window_get_border_cell(wp->window, wp, pane_lines, cell_type, gc);
}

/* Get pane border style. */
void
window_pane_get_border_style(struct window_pane *wp, struct client *c,
    struct grid_cell *gc)
{
	struct session		*s = c->session;
	struct format_tree	*ft;
	const char		*option;
	struct grid_cell	*saved;
	int			*flag;

	if (wp == server_client_get_pane(c)) {
		flag = &wp->active_border_gc_set;
		saved = &wp->active_border_gc;
		option = "pane-active-border-style";
	} else {
		flag = &wp->border_gc_set;
		saved = &wp->border_gc;
		option = "pane-border-style";
	}

	if (!*flag) {
		ft = format_create_defaults(NULL, c, s, s->curw, wp);
		style_apply(saved, wp->options, option, ft);
		format_free(ft);
		*flag = 1;
	}
	memcpy(gc, saved, sizeof *gc);
}

/* Build pane status line. */
int
window_make_pane_status(struct window_pane *wp, struct client *c, u_int width,
    struct redraw_span *span)
{
	struct grid_cell	 gc;
	const char		*fmt;
	struct format_tree	*ft;
	struct style_line_entry	*sle = &wp->border_status_line;
	struct screen_write_ctx	 ctx;
	struct screen		 old;
	char			*expanded;
	u_int			 i;
	enum pane_lines		 pane_lines;
	int			 pane_status, cell_type;

	pane_status = window_pane_get_pane_status(wp);
	if (pane_status == PANE_STATUS_OFF || width == 0)
		return (0);

	ft = format_create(c, NULL, FORMAT_PANE|wp->id, FORMAT_STATUS);
	format_defaults(ft, c, c->session, c->session->curw, wp);

	fmt = options_get_string(wp->options, "pane-border-format");
	expanded = format_expand_time(ft, fmt);

	memcpy(&old, &wp->status_screen, sizeof old);
	screen_init(&wp->status_screen, width, 1, 0);
	wp->status_screen.mode = 0;
	screen_write_start(&ctx, &wp->status_screen);

	window_pane_get_border_style(wp, c, &gc);
	pane_lines = window_pane_get_pane_lines(wp);
	for (i = 0; i < width; i++) {
		cell_type = redraw_get_status_border_cell_type(&span, i);
		window_get_border_cell(wp->window, wp, pane_lines, cell_type, &gc);
		screen_write_cell(&ctx, &gc);
	}
	gc.attr &= ~GRID_ATTR_CHARSET;

	screen_write_cursormove(&ctx, 0, 0, 0);
	style_ranges_free(&sle->ranges);
	format_draw(&ctx, &gc, width, expanded, &sle->ranges, 0);

	screen_write_stop(&ctx);
	format_free(ft);

	free(sle->expanded);
	sle->expanded = expanded;

	if (grid_compare(wp->status_screen.grid, old.grid) == 0) {
		screen_free(&old);
		return (0);
	}
	screen_free(&old);
	return (1);
}
