/* $OpenBSD$ */

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

#include <string.h>

#include "tmux.h"

enum tty_draw_line_state {
	TTY_DRAW_LINE_FIRST,
	TTY_DRAW_LINE_FLUSH,
	TTY_DRAW_LINE_NEW1,
	TTY_DRAW_LINE_NEW2,
	TTY_DRAW_LINE_EMPTY,
	TTY_DRAW_LINE_SAME,
	TTY_DRAW_LINE_PAD,
	TTY_DRAW_LINE_DONE
};
static const char* tty_draw_line_states[] = {
	"FIRST",
	"FLUSH",
	"NEW1",
	"NEW2",
	"EMPTY",
	"SAME",
	"PAD",
	"DONE"
};

/* Clear part of the line. */
static void
tty_draw_line_clear(struct tty *tty, u_int px, u_int py, u_int nx,
    const struct grid_cell *defaults, u_int bg, int wrapped)
{
	/* Nothing to clear. */
	if (nx == 0)
		return;

	/* If genuine BCE is available, can try escape sequences. */
	if (!wrapped && nx >= 10 && !tty_fake_bce(tty, defaults, bg)) {
		/* Off the end of the line, use EL if available. */
		if (px + nx >= tty->sx && tty_term_has(tty->term, TTYC_EL)) {
			tty_cursor(tty, px, py);
			tty_putcode(tty, TTYC_EL);
			return;
		}

		/* At the start of the line. Use EL1. */
		if (px == 0 && tty_term_has(tty->term, TTYC_EL1)) {
			tty_cursor(tty, px + nx - 1, py);
			tty_putcode(tty, TTYC_EL1);
			return;
		}

		/* Section of line. Use ECH if possible. */
		if (tty_term_has(tty->term, TTYC_ECH)) {
			tty_cursor(tty, px, py);
			tty_putcode_i(tty, TTYC_ECH, nx);
			return;
		}
	}

        /* Couldn't use an escape sequence, use spaces. */
	if (px != 0 || !wrapped)
		tty_cursor(tty, px, py);
	if (nx == 1)
		tty_putc(tty, ' ');
	else if (nx == 2)
		tty_putn(tty, "  ", 2, 2);
	else
		tty_repeat_space(tty, nx);
}

/* Is this cell empty? */
static u_int
tty_draw_line_get_empty(const struct grid_cell *gc, u_int nx)
{
	u_int	empty = 0;

	if (gc->data.width != 1 && gc->data.width > nx)
		empty = nx;
	else if (gc->attr == 0 && gc->link == 0) {
		if (gc->flags & GRID_FLAG_CLEARED)
			empty = 1;
		else if (gc->flags & GRID_FLAG_TAB)
			empty = gc->data.width;
		else if (gc->data.size == 1 && *gc->data.data == ' ')
			empty = 1;
	}
	return (empty);
}

/* Draw a line from screen to tty. */
void
tty_draw_line(struct tty *tty, struct screen *s, u_int px, u_int py, u_int nx,
    u_int atx, u_int aty, const struct grid_cell *defaults,
    struct colour_palette *palette)
{
	struct grid		*gd = s->grid;
	const struct grid_cell	*gcp;
	struct grid_cell	 gc, ngc, last;
	struct grid_line	*gl;
	u_int			 i, j, last_i, cx, ex, width;
	u_int			 cellsize, bg;
	int			 flags, empty, wrapped = 0;
	char			 buf[1000];
	size_t			 len;
	enum tty_draw_line_state current_state, next_state;

	/*
	 * py is the line in the screen to draw. px is the start x and nx is
	 * the width to draw. atx,aty is the line on the terminal to draw it.
	 */
	log_debug("%s: px=%u py=%u nx=%u atx=%u aty=%u", __func__, px, py, nx,
	    atx, aty);

	/*
	 * Clamp the width to cellsize - note this is not cellused, because
	 * there may be empty background cells after it (from BCE).
	 */
	cellsize = grid_get_line(gd, gd->hsize + py)->cellsize;
	if (screen_size_x(s) > cellsize)
		ex = cellsize;
	else {
		ex = screen_size_x(s);
		if (px > ex)
			return;
		if (px + nx > ex)
			nx = ex - px;
	}
	if (ex < nx)
		ex = nx;
	log_debug("%s: drawing %u-%u,%u (end %u) at %u,%u; defaults: fg=%d, "
	    "bg=%d", __func__, px, px + nx, py, ex, atx, aty, defaults->fg,
	    defaults->bg);

	/*
	 * If there is padding at the start, we must have truncated a wide
	 * character. Clear it.
	 */
	cx = 0;
	for (i = px; i < px + nx; i++) {
		grid_view_get_cell(gd, i, py, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
		cx++;
	}
	if (cx != 0) {
		/* Find the previous cell for the background colour. */
		for (i = px + 1; i > 0; i--) {
			grid_view_get_cell(gd, i - 1, py, &gc);
			if (~gc.flags & GRID_FLAG_PADDING)
				break;
		}
		if (i == 0)
			bg = defaults->bg;
		else {
			bg = gc.bg;
			if (gc.flags & GRID_FLAG_SELECTED) {
				memcpy(&ngc, &gc, sizeof ngc);
				if (screen_select_cell(s, &ngc, &gc))
					bg = ngc.bg;
			}
		}
		tty_attributes(tty, &last, defaults, palette, s->hyperlinks);
		log_debug("%s: clearing %u padding cells", __func__, cx);
		tty_draw_line_clear(tty, atx, aty, cx, defaults, bg, 0);
		if (cx == ex)
			return;
		atx += cx;
		px += cx;
		nx -= cx;
		ex -= cx;
	}

	/* Did the previous line wrap on to this one? */
	if (py != 0 && atx == 0 && tty->cx >= tty->sx && nx == tty->sx) {
		gl = grid_get_line(gd, gd->hsize + py - 1);
		if (gl->flags & GRID_LINE_WRAPPED)
			wrapped = 1;
	}

	/* Turn off cursor while redrawing and reset region and margins. */
	flags = (tty->flags & TTY_NOCURSOR);
	tty->flags |= TTY_NOCURSOR;
	tty_update_mode(tty, tty->mode, s);
	tty_region_off(tty);
	tty_margin_off(tty);

	/* Start with the default cell as the last cell. */
	memcpy(&last, &grid_default_cell, sizeof last);
	last.bg = defaults->bg;
	tty_default_attributes(tty, defaults, palette, 8, s->hyperlinks);

	/* Loop over each character in the range. */
	last_i = i = 0;
	len = 0;
	width = 0;
	current_state = TTY_DRAW_LINE_FIRST;
	for (;;) {
		/* Work out the next state. */
		if (i == nx) {
			/*
			 * If this is the last cell, we are done. But we need to
			 * go through the loop again to flush anything in
			 * the buffer.
			 */
			empty = 0;
			next_state = TTY_DRAW_LINE_DONE;
			gcp = &grid_default_cell;
		} else {
			/* Get the current cell. */
			grid_view_get_cell(gd, px + i, py, &gc);

			/* Update for codeset if needed. */
			gcp = tty_check_codeset(tty, &gc);

			/* And for selection. */
			if (gcp->flags & GRID_FLAG_SELECTED) {
				memcpy(&ngc, gcp, sizeof ngc);
				if (screen_select_cell(s, &ngc, gcp))
					gcp = &ngc;
			}

			/* Work out the the empty width. */
			if (i >= ex)
				empty = 1;
			else if (gcp->bg != last.bg)
				empty = 0;
			else
				empty = tty_draw_line_get_empty(gcp, nx - i);

			/* Work out the next state. */
			if (empty != 0)
				next_state = TTY_DRAW_LINE_EMPTY;
			else if (current_state == TTY_DRAW_LINE_FIRST)
				next_state = TTY_DRAW_LINE_SAME;
			else if (gcp->flags & GRID_FLAG_PADDING)
				next_state = TTY_DRAW_LINE_PAD;
			else if (grid_cells_look_equal(gcp, &last)) {
				if (gcp->data.size > (sizeof buf) - len)
					next_state = TTY_DRAW_LINE_FLUSH;
				else
					next_state = TTY_DRAW_LINE_SAME;
			} else if (current_state == TTY_DRAW_LINE_NEW1)
				next_state = TTY_DRAW_LINE_NEW2;
			else
				next_state = TTY_DRAW_LINE_NEW1;
		}
		if (log_get_level() != 0) {
			log_debug("%s: cell %u empty %u, bg %u; state: "
			    "current %s, next %s", __func__, px + i, empty,
			    gcp->bg, tty_draw_line_states[current_state],
			    tty_draw_line_states[next_state]);
		}

		/* If the state has changed, flush any collected data. */
		if (next_state != current_state) {
			if (current_state == TTY_DRAW_LINE_EMPTY) {
				tty_attributes(tty, &last, defaults, palette,
				    s->hyperlinks);
				tty_draw_line_clear(tty, atx + last_i, aty,
				    i - last_i, defaults, last.bg, wrapped);
				wrapped = 0;
			} else if (next_state != TTY_DRAW_LINE_SAME &&
			    len != 0) {
				tty_attributes(tty, &last, defaults, palette,
				    s->hyperlinks);
				if (atx + i - width != 0 || !wrapped)
					tty_cursor(tty, atx + i - width, aty);
				if (~last.attr & GRID_ATTR_CHARSET)
					tty_putn(tty, buf, len, width);
				else {
					for (j = 0; j < len; j++)
						tty_putc(tty, buf[j]);
				}
				len = 0;
				width = 0;
				wrapped = 0;
			}
			last_i = i;
		}

		/* Append the cell if it is not empty and not padding. */
		if (next_state != TTY_DRAW_LINE_EMPTY &&
		    next_state != TTY_DRAW_LINE_PAD) {
			memcpy(buf + len, gcp->data.data, gcp->data.size);
			len += gcp->data.size;
			width += gcp->data.width;
		}

		/* If this is the last cell, we are done. */
		if (next_state == TTY_DRAW_LINE_DONE)
			break;

		/* Otherwise move to the next. */
		current_state = next_state;
		memcpy(&last, gcp, sizeof last);
		if (empty != 0)
			i += empty;
		else
			i += gcp->data.width;
	}

	tty->flags = (tty->flags & ~TTY_NOCURSOR)|flags;
	tty_update_mode(tty, tty->mode, s);
}

