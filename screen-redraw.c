/* $Id: screen-redraw.c,v 1.16 2009-01-11 23:31:46 nicm Exp $ */

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
void	screen_redraw_blanky(struct client *, u_int, u_int);
void	screen_redraw_line(struct client *, struct screen *, u_int, u_int);

/* Redraw entire screen.. */
void
screen_redraw_screen(struct client *c, struct screen *s)
{
	struct winlink	*wl = c->session->curw;
	u_int		 i, cx, cy, sy;
	int		 status;

	status = options_get_number(&c->session->options, "status");

	/* Override the normal screen if one is given. */
	if (s != NULL) {
		for (i = 0; i < screen_size_y(s); i++)
			screen_redraw_line(c, s, 0, i);
		return;
	}

	/* 
	 * A normal client screen is made up of three parts: a top window, a
	 * bottom window and a status line. The bottom window may be turned
	 * off; the status line is always drawn.
	 */

	/* Draw the top window. */
	s = wl->window->panes[0]->screen;
	sy = screen_size_y(s);
	if (screen_size_y(s) == c->sy && wl->window->panes[1] == NULL)
		sy--;
	cx = s->cx;
	cy = s->cy;
	for (i = 0; i < sy; i++)
		screen_redraw_line(c, s, 0, i);
	s->cx = cx;
	s->cy = cy;

	/* Draw the bottom window. */
	if (wl->window->panes[1] != NULL) {
		s = wl->window->panes[1]->screen;
		sy = screen_size_y(s);
		if (!status && screen_size_y(s) == c->sy - (c->sy / 2) - 1)
			sy--;
		cx = s->cx;
		cy = s->cy;
		for (i = 0; i < sy; i++)
			screen_redraw_line(c, s, wl->window->sy / 2, i);
		s->cx = cx;
		s->cy = cy;
	}

	/* Fill in empty space. */
	if (wl->window->sx < c->sx) {
		screen_redraw_blankx(
		    c, wl->window->sx, c->sx - wl->window->sx);
	}
	if (wl->window->sy < c->sy - status) {
		screen_redraw_blanky(
		    c, wl->window->sy, c->sy - wl->window->sy);
	}

	/* Draw separator line. */
	s = wl->window->panes[0]->screen;
	if (screen_size_y(s) != wl->window->sy)
		screen_redraw_blanky(c, screen_size_y(s), 1);

	/* Draw the status line. */
	screen_redraw_status(c);
}

/* Draw the status line. */
void
screen_redraw_status(struct client *c)
{
	screen_redraw_line(c, &c->status, c->sy - 1, 0);
}

/* Draw blank columns. */
void
screen_redraw_blankx(struct client *c, u_int ox, u_int nx)
{
	u_int	i, j;

	tty_putcode(&c->tty, TTYC_SGR0);
	for (j = 0; j < c->sy; j++) {
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
screen_redraw_blanky(struct client *c, u_int oy, u_int ny)
{
	u_int	i, j;

	tty_putcode(&c->tty, TTYC_SGR0);
	for (j = 0; j < ny; j++) {
		tty_putcode2(&c->tty, TTYC_CUP, oy + j, 0);
		for (i = 0; i < c->sx; i++) {
			if (j == 0)
				tty_putc(&c->tty, '-');
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
	const struct grid_cell	*gc;
	struct grid_cell	 tc;
	u_int			 i;

	for (i = 0; i < screen_size_x(s); i++) {
		s->cx = i;
		s->cy = py;

		gc = grid_view_peek_cell(s->grid, i, py);
		if (screen_check_selection(s, i, py)) {
			memcpy(&tc, &s->sel.cell, sizeof tc);
			tc.data = gc->data;
			tty_write(&c->tty, s, oy, TTY_CELL, &tc);
		} else
			tty_write(&c->tty, s, oy, TTY_CELL, gc);
	}
}
