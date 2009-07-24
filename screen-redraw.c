/* $OpenBSD$ */

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

int	screen_redraw_check_cell(struct client *, u_int, u_int);

#define CELL_INSIDE 0
#define CELL_LEFT 1
#define CELL_RIGHT 2
#define CELL_TOP 3
#define CELL_BOTTOM 4
#define CELL_OUTSIDE 5

/* Check if cell inside a pane. */
int
screen_redraw_check_cell(struct client *c, u_int px, u_int py)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;

	if (px > w->sx || py > w->sy)
		return (CELL_OUTSIDE);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;

		/* Inside pane. */
		if (px >= wp->xoff && px < wp->xoff + wp->sx &&
		    py >= wp->yoff && py < wp->yoff + wp->sy)
			return (CELL_INSIDE);

		/* Left/right borders. */
		if (py >= wp->yoff && py < wp->yoff + wp->sy) {
			if (wp->xoff != 0 && px == wp->xoff - 1)
				return (CELL_LEFT);
			if (px == wp->xoff + wp->sx)
				return (CELL_RIGHT);
		}

		/* Top/bottom borders. */
		if (px >= wp->xoff && px < wp->xoff + wp->sx) {
			if (wp->yoff != 0 && py == wp->yoff - 1)
				return (CELL_TOP);
			if (py == wp->yoff + wp->sy)
				return (CELL_BOTTOM);
		}
	}

	return (CELL_OUTSIDE);
}

/* Redraw entire screen. */
void
screen_redraw_screen(struct client *c, int status_only)
{
	struct window		*w = c->session->curw->window;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	u_int		 	 i, j, type;
	int		 	 status;
	const u_char		*border;

	/* Get status line, er, status. */
	if (c->message_string != NULL || c->prompt_string != NULL)
		status = 1;
	else
		status = options_get_number(&c->session->options, "status");

	/* If only drawing status and it is present, don't need the rest. */
	if (status_only && status) {
		tty_draw_line(tty, &c->status, 0, 0, tty->sy - 1);
		return;
	}

	/* Draw background and borders. */
	tty_reset(tty);
	if (tty_term_has(tty->term, TTYC_ACSC)) {
		border = " xxqq~";
		tty_putcode(tty, TTYC_SMACS);
	} else 
		border = " ||--.";
	for (j = 0; j < tty->sy - status; j++) {
		if (status_only && j != tty->sy - 1)
			continue;
		for (i = 0; i < tty->sx; i++) {
			type = screen_redraw_check_cell(c, i, j);
			if (type != CELL_INSIDE) {
				tty_cursor(tty, i, j, 0, 0);
				tty_putc(tty, border[type]);
			}
		}
	}
	tty_putcode(tty, TTYC_RMACS);

	/* Draw the panes. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		for (i = 0; i < wp->sy; i++) {
			if (status_only && wp->yoff + i != tty->sy - 1)
				continue;
			tty_draw_line(tty, wp->screen, i, wp->xoff, wp->yoff);
		}
	}

	/* Draw the status line. */
	if (status)
		tty_draw_line(tty, &c->status, 0, 0, tty->sy - 1);
}

/* Draw a single pane. */
void
screen_redraw_pane(struct client *c, struct window_pane *wp)
{
	u_int	i;

	for (i = 0; i < wp->sy; i++)
		tty_draw_line(&c->tty, wp->screen, i, wp->xoff, wp->yoff);
}
