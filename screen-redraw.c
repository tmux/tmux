/* $Id: screen-redraw.c,v 1.30 2009-03-31 18:39:45 nicm Exp $ */

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

void	screen_redraw_blankx(struct client *, u_int, u_int);
void	screen_redraw_blanky(struct client *, u_int, u_int, char);
void	screen_redraw_line(struct client *, struct screen *, u_int, u_int);

/* Redraw entire screen.. */
void
screen_redraw_screen(struct client *c, struct screen *s)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	u_int		 	 i, cx, cy, sy;
	int		 	 status;

	/* Override the normal screen if one is given. */
	if (s != NULL) {
		for (i = 0; i < screen_size_y(s); i++)
			screen_redraw_line(c, s, 0, i);	
		screen_redraw_status(c);
		return;
	}

	status = options_get_number(&c->session->options, "status");

	/* Fill in empty space on the right. */
	if (w->sx < c->tty.sx)
		screen_redraw_blankx(c, w->sx, c->tty.sx - w->sx);

	/* Draw the panes. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		s = wp->screen;

		sy = screen_size_y(s);

		cx = s->cx;
		cy = s->cy;
		if (wp->yoff + sy <= w->sy) {
			for (i = 0; i < sy; i++) {
				if (wp->yoff + i != c->tty.sy - 1)
					screen_redraw_line(c, s, wp->yoff, i);
			}
			if (TAILQ_NEXT(wp, entry) != NULL)
				screen_redraw_blanky(c, wp->yoff + sy, 1, '-');
		}
		s->cx = cx;
		s->cy = cy;
	}

	/* Fill in empty space below. */
	if (w->sy < c->tty.sy - status)
		screen_redraw_blanky(c, w->sy, c->tty.sy - status - w->sy, '=');

	/* Draw right border line. */
	if (w->sx < c->tty.sx) {
		for (i = 0; i < c->tty.sy; i++) {
			tty_putcode2(&c->tty, TTYC_CUP, i, w->sx);
			tty_putc(&c->tty, '|');
		}
	}

	/* Draw the status line. */
	screen_redraw_status(c);
}

/* Draw the status line. */
void
screen_redraw_status(struct client *c)
{
	screen_redraw_line(c, &c->status, c->tty.sy - 1, 0);
}

/* Draw blank columns. */
void
screen_redraw_blankx(struct client *c, u_int ox, u_int nx)
{
	u_int	i, j;

	tty_putcode(&c->tty, TTYC_SGR0);
	for (j = 0; j < c->tty.sy; j++) {
		tty_putcode2(&c->tty, TTYC_CUP, j, ox);
		for (i = 0; i < nx; i++)
			tty_putc(&c->tty, ' ');
	}

	c->tty.cx = UINT_MAX;
	c->tty.cy = UINT_MAX;
	memcpy(&c->tty.cell, &grid_default_cell, sizeof c->tty.cell);
}

/* Draw blank lines. */
void
screen_redraw_blanky(struct client *c, u_int oy, u_int ny, char ch)
{
	u_int	i, j;

	tty_putcode(&c->tty, TTYC_SGR0);
	for (j = 0; j < ny; j++) {
		tty_putcode2(&c->tty, TTYC_CUP, oy + j, 0);
		for (i = 0; i < c->tty.sx; i++) {
			if (j == 0)
				tty_putc(&c->tty, ch);
			else
				tty_putc(&c->tty, ' ');
		}
	}

	c->tty.cx = UINT_MAX;
	c->tty.cy = UINT_MAX;
	memcpy(&c->tty.cell, &grid_default_cell, sizeof c->tty.cell);
}

/* Draw a line. */
void
screen_redraw_line(struct client *c, struct screen *s, u_int oy, u_int py)
{
	tty_draw_line(&c->tty, s, py, oy);
}
