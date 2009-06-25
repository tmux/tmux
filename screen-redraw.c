/* $OpenBSD: screen-redraw.c,v 1.2 2009/06/25 05:56:44 nicm Exp $ */

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

/* Check if cell inside a pane. */
int
screen_redraw_check_cell(struct client *c, u_int px, u_int py)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;

	if (px > w->sx || py > w->sy)
		return (0);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		/* Inside pane. */
		if (px >= wp->xoff && px < wp->xoff + wp->sx &&
		    py >= wp->yoff && py < wp->yoff + wp->sy)
			return (1);

		/* Left/right borders. */
		if (py >= wp->yoff && py < wp->yoff + wp->sy) {
			if (wp->xoff != 0 && px == wp->xoff - 1)
				return (1);
			if (px == wp->xoff + wp->sx)
				return (1);
		}

		/* Top/bottom borders. */
		if (px >= wp->xoff && px < wp->xoff + wp->sx) {
			if (wp->yoff != 0 && py == wp->yoff - 1)
				return (1);
			if (py == wp->yoff + wp->sy)
				return (1);
		}
	}

	return (0);
}

/* Redraw entire screen.. */
void
screen_redraw_screen(struct client *c)
{
	struct window		*w = c->session->curw->window;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	u_int		 	 i, j, sx, sy;
	int		 	 status, has_acs;
	u_char			 choriz, cvert, cbackg;

	/* Get status line, er, status. */
	status = options_get_number(&c->session->options, "status");

	/* Work out ACS characters. */
	if (tty_term_has(tty->term, TTYC_ACSC)) {
		has_acs = 1;
		choriz = tty_get_acs(tty, 'q');
		cvert = tty_get_acs(tty, 'x');
		cbackg = tty_get_acs(tty, '~');
	} else {
		has_acs = 0;
		choriz = '-';
		cvert = '|';
		cbackg = '.';
	}

	/* Clear the screen. */
	tty_reset(tty);
	if (has_acs)
		tty_putcode(tty, TTYC_SMACS);
	for (j = 0; j < tty->sy - status; j++) {
		for (i = 0; i < tty->sx; i++) {
			if (!screen_redraw_check_cell(c, i, j)) {
				tty_cursor(tty, i, j, 0, 0);
				tty_putc(tty, cbackg);
			}
		}
	}
	if (has_acs)
		tty_putcode(tty, TTYC_RMACS);

	/* Draw the panes. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp->flags & PANE_HIDDEN)
			continue;

		tty_reset(tty);

		sx = wp->sx;
		sy = wp->sy;

		/* Draw left and right borders. */
		if (has_acs)
			tty_putcode(tty, TTYC_SMACS);
		if (wp->xoff > 0) {
			for (i = wp->yoff; i < wp->yoff + sy; i++) {
				tty_cursor(tty, wp->xoff - 1, i, 0, 0);
				tty_putc(tty, cvert);
			}
		}
		if (wp->xoff + sx < tty->sx) {
			for (i = wp->yoff; i < wp->yoff + sy; i++) {
				tty_cursor(tty, wp->xoff + sx, i, 0, 0);
				tty_putc(&c->tty, cvert);
			}
		}

		/* Draw top and bottom borders. */
		if (wp->yoff > 0) {
			tty_cursor(tty, wp->xoff, wp->yoff - 1, 0, 0);
			for (i = 0; i < sx; i++)
				tty_putc(tty, choriz);
		}
		if (wp->yoff + sy < tty->sy - status) {
			tty_cursor(tty, wp->xoff, wp->yoff + sy, 0, 0);
			for (i = 0; i < sx; i++)
				tty_putc(tty, choriz);
		}
		if (has_acs)
			tty_putcode(tty, TTYC_RMACS);

		/* Draw the pane. */
		screen_redraw_pane(c, wp);
	}

	/* Draw the status line. */
	screen_redraw_status(c);
}

/* Draw a single pane. */
void
screen_redraw_pane(struct client *c, struct window_pane *wp)
{
	u_int	i;

	for (i = 0; i < wp->sy; i++)
		tty_draw_line(&c->tty, wp->screen, i, wp->xoff, wp->yoff);
}


/* Draw the status line. */
void
screen_redraw_status(struct client *c)
{
	tty_draw_line(&c->tty, &c->status, 0, 0, c->tty.sy - 1);
}
