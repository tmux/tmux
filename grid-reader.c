/* $OpenBSD: grid-reader.c,v 1.12 2026/10/08 07:50:05 nicm Exp $ */

/*
 * Copyright (c) 2020 Anindya Mukherjee <anindya49@hotmail.com>
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

#include "tmux.h"
#include <string.h>

/* Initialise virtual cursor. */
void
grid_reader_start(struct grid_reader *gr, struct grid *gd, u_int cx, u_int cy)
{
	gr->gd = gd;
	gr->cx = cx;
	gr->cy = cy;
}

/* Get cursor position from reader. */
void
grid_reader_get_cursor(struct grid_reader *gr, u_int *cx, u_int *cy)
{
	*cx = gr->cx;
	*cy = gr->cy;
}

/* Get length of line containing the cursor. */
u_int
grid_reader_line_length(struct grid_reader *gr)
{
	return (grid_line_length(gr->gd, gr->cy));
}

/* Move cursor forward one position. */
void
grid_reader_cursor_right(struct grid_reader *gr, int wrap, int all, int onemore)
{
	u_int			px;
	struct grid_cell	gc;

	if (all)
		px = gr->gd->sx;
	else if (onemore)
		px = grid_reader_line_length(gr);
	else
		px = grid_line_limit(gr->gd, gr->cy);

	if (wrap && gr->cx >= px && gr->cy < gr->gd->hsize + gr->gd->sy - 1) {
		grid_reader_cursor_start_of_line(gr, 0);
		grid_reader_cursor_down(gr);
	} else if (gr->cx < px) {
		gr->cx++;
		while (gr->cx < px) {
			grid_get_cell(gr->gd, gr->cx, gr->cy, &gc);
			if (~gc.flags & GRID_FLAG_PADDING)
				break;
			gr->cx++;
		}
	}
}

/* Move cursor back one position. */
void
grid_reader_cursor_left(struct grid_reader *gr, int wrap)
{
	struct grid_cell	gc;

	while (gr->cx > 0) {
		grid_get_cell(gr->gd, gr->cx, gr->cy, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
		gr->cx--;
	}
	if (gr->cx == 0 && gr->cy > 0 &&
	    (wrap ||
	     grid_get_line(gr->gd, gr->cy - 1)->flags & GRID_LINE_WRAPPED)) {
		grid_reader_cursor_up(gr);
		grid_reader_cursor_end_of_line(gr, 0, 0);
	} else if (gr->cx > 0)
		gr->cx--;
}

/* Move cursor down one line. */
void
grid_reader_cursor_down(struct grid_reader *gr)
{
	struct grid_cell	gc;

	if (gr->cy < gr->gd->hsize + gr->gd->sy - 1)
		gr->cy++;
	while (gr->cx > 0) {
		grid_get_cell(gr->gd, gr->cx, gr->cy, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
		gr->cx--;
	}
}

/* Move cursor up one line. */
void
grid_reader_cursor_up(struct grid_reader *gr)
{
	struct grid_cell	gc;

	if (gr->cy > 0)
		gr->cy--;
	while (gr->cx > 0) {
		grid_get_cell(gr->gd, gr->cx, gr->cy, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
		gr->cx--;
	}
}

/* Move cursor to the start of the line. */
void
grid_reader_cursor_start_of_line(struct grid_reader *gr, int wrap)
{
	if (wrap) {
		while (gr->cy > 0 &&
		    grid_get_line(gr->gd, gr->cy - 1)->flags &
		        GRID_LINE_WRAPPED)
			gr->cy--;
	}
	gr->cx = 0;
}

/* Move cursor to the end of the line. */
void
grid_reader_cursor_end_of_line(struct grid_reader *gr, int wrap, int all)
{
	u_int	yy;

	if (wrap) {
		yy = gr->gd->hsize + gr->gd->sy - 1;
		while (gr->cy < yy && grid_get_line(gr->gd, gr->cy)->flags &
		    GRID_LINE_WRAPPED)
			gr->cy++;
	}
	if (all)
		gr->cx = gr->gd->sx;
	else
		gr->cx = grid_reader_line_length(gr);
}

/* Handle line wrapping while moving the cursor. */
static int
grid_reader_handle_wrap(struct grid_reader *gr, u_int *xx, u_int *yy)
{
	/*
	 * Make sure the cursor lies within the grid reader's bounding area,
	 * wrapping to the next line as necessary. Return zero if the cursor
	 * would wrap past the bottom of the grid.
	 */
	while (gr->cx > *xx) {
		if (gr->cy == *yy)
			return (0);
		grid_reader_cursor_start_of_line(gr, 0);
		grid_reader_cursor_down(gr);

		if (grid_get_line(gr->gd, gr->cy)->flags & GRID_LINE_WRAPPED)
			*xx = gr->gd->sx - 1;
		else
			*xx = grid_reader_line_length(gr);
	}
	return (1);
}

/* Check if character under cursor is in set. */
int
grid_reader_in_set(struct grid_reader *gr, const char *set)
{
	return (grid_in_set(gr->gd, gr->cx, gr->cy, set));
}

/* Move cursor to the start of the next word. */
void
grid_reader_cursor_next_word(struct grid_reader *gr, const char *separators)
{
	u_int	xx, yy, width;

	/* Do not break up wrapped words. */
	if (grid_get_line(gr->gd, gr->cy)->flags & GRID_LINE_WRAPPED)
		xx = gr->gd->sx - 1;
	else
		xx = grid_reader_line_length(gr);
	yy = gr->gd->hsize + gr->gd->sy - 1;

	/*
	 * When navigating via spaces (for example with next-space) separators
	 * should be empty.
	 *
	 * If we started on a separator that is not whitespace, skip over
	 * subsequent separators that are not whitespace. Otherwise, if we
	 * started on a non-whitespace character, skip over subsequent
	 * characters that are neither whitespace nor separators. Then, skip
	 * over whitespace (if any) until the next non-whitespace character.
	 */
	if (!grid_reader_handle_wrap(gr, &xx, &yy))
		return;
	if (!grid_reader_in_set(gr, WHITESPACE)) {
		if (grid_reader_in_set(gr, separators)) {
			do
				gr->cx++;
			while (grid_reader_handle_wrap(gr, &xx, &yy) &&
			    grid_reader_in_set(gr, separators) &&
			    !grid_reader_in_set(gr, WHITESPACE));
		} else {
			do
				gr->cx++;
			while (grid_reader_handle_wrap(gr, &xx, &yy) &&
			    !(grid_reader_in_set(gr, separators) ||
			    grid_reader_in_set(gr, WHITESPACE)));
		}
	}
	while (grid_reader_handle_wrap(gr, &xx, &yy) &&
	    (width = grid_reader_in_set(gr, WHITESPACE)))
		gr->cx += width;
}

/* Move cursor to the end of the next word. */
void
grid_reader_cursor_next_word_end(struct grid_reader *gr, const char *separators)
{
	u_int	xx, yy;

	/* Do not break up wrapped words. */
	if (grid_get_line(gr->gd, gr->cy)->flags & GRID_LINE_WRAPPED)
		xx = gr->gd->sx - 1;
	else
		xx = grid_reader_line_length(gr);
	yy = gr->gd->hsize + gr->gd->sy - 1;

	/*
	 * When navigating via spaces (for example with next-space), separators
	 * should be empty in both modes.
	 *
	 * If we started on a whitespace, move until reaching the first
	 * non-whitespace character. If that character is a separator, treat
	 * subsequent separators as a word, and continue moving until the first
	 * non-separator. Otherwise, continue moving until the first separator
	 * or whitespace.
	 */

	while (grid_reader_handle_wrap(gr, &xx, &yy)) {
		if (grid_reader_in_set(gr, WHITESPACE))
			gr->cx++;
		else if (grid_reader_in_set(gr, separators)) {
			do
				gr->cx++;
			while (grid_reader_handle_wrap(gr, &xx, &yy) &&
			    grid_reader_in_set(gr, separators) &&
			    !grid_reader_in_set(gr, WHITESPACE));
			return;
		} else {
			do
				gr->cx++;
			while (grid_reader_handle_wrap(gr, &xx, &yy) &&
			    !(grid_reader_in_set(gr, WHITESPACE) ||
			    grid_reader_in_set(gr, separators)));
			return;
		}
	}
}

/* Move to the previous place where a word begins. */
void
grid_reader_cursor_previous_word(struct grid_reader *gr, const char *separators,
    int already, int stop_at_eol)
{
	int	oldx, oldy, at_eol, word_is_letters;

	/* Move back to the previous word character. */
	if (already || grid_reader_in_set(gr, WHITESPACE)) {
		for (;;) {
			if (gr->cx > 0) {
				gr->cx--;
				if (!grid_reader_in_set(gr, WHITESPACE)) {
					word_is_letters =
					    !grid_reader_in_set(gr, separators);
					break;
				}
			} else {
				if (gr->cy == 0)
					return;
				grid_reader_cursor_up(gr);
				grid_reader_cursor_end_of_line(gr, 0, 0);

				/* Stop if separator at EOL. */
				if (stop_at_eol && gr->cx > 0) {
					oldx = gr->cx;
					gr->cx--;
					at_eol = grid_reader_in_set(gr,
					    WHITESPACE);
					gr->cx = oldx;
					if (at_eol) {
						word_is_letters = 0;
						break;
					}
				}
			}
		}
	} else
		word_is_letters = !grid_reader_in_set(gr, separators);

	/* Move back to the beginning of this word. */
	do {
		oldx = gr->cx;
		oldy = gr->cy;
		if (gr->cx == 0) {
			if (gr->cy == 0 ||
			    (~grid_get_line(gr->gd, gr->cy - 1)->flags &
			    GRID_LINE_WRAPPED))
				break;
			grid_reader_cursor_up(gr);
			grid_reader_cursor_end_of_line(gr, 0, 1);
		}
		if (gr->cx > 0)
			gr->cx--;
	} while (!grid_reader_in_set(gr, WHITESPACE) &&
	    word_is_letters != grid_reader_in_set(gr, separators));
	gr->cx = oldx;
	gr->cy = oldy;
}

/* Compare grid cell to UTF-8 data. Return 1 if equal, 0 if not. */
static int
grid_reader_cell_equals_data(const struct grid_cell *gc,
    const struct utf8_data *ud)
{
	if (gc->flags & GRID_FLAG_PADDING)
		return (0);
	if (gc->flags & GRID_FLAG_TAB && ud->size == 1 && *ud->data == '\t')
		return (1);
	if (gc->data.size != ud->size)
		return (0);
	return (memcmp(gc->data.data, ud->data, gc->data.size) == 0);
}

/* Jump forward to character. */
int
grid_reader_cursor_jump(struct grid_reader *gr, const struct utf8_data *jc)
{
	struct grid_cell	gc;
	u_int			px, py, xx, yy;

	px = gr->cx;
	yy = gr->gd->hsize + gr->gd->sy - 1;

	for (py = gr->cy; py <= yy; py++) {
		xx = grid_line_length(gr->gd, py);
		while (px < xx) {
			grid_get_cell(gr->gd, px, py, &gc);
			if (grid_reader_cell_equals_data(&gc, jc)) {
				gr->cx = px;
				gr->cy = py;
				return (1);
			}
			px++;
		}

		if (py == yy ||
		    !(grid_get_line(gr->gd, py)->flags & GRID_LINE_WRAPPED))
			return (0);
		px = 0;
	}
	return (0);
}

/* Jump back to character. */
int
grid_reader_cursor_jump_back(struct grid_reader *gr, const struct utf8_data *jc)
{
	struct grid_cell	gc;
	u_int			px, py, xx;

	xx = gr->cx + 1;

	for (py = gr->cy + 1; py > 0; py--) {
		for (px = xx; px > 0; px--) {
			grid_get_cell(gr->gd, px - 1, py - 1, &gc);
			if (grid_reader_cell_equals_data(&gc, jc)) {
				gr->cx = px - 1;
				gr->cy = py - 1;
				return (1);
			}
		}

		if (py == 1 ||
		    !(grid_get_line(gr->gd, py - 2)->flags & GRID_LINE_WRAPPED))
			return (0);
		xx = grid_line_length(gr->gd, py - 2);
	}
	return (0);
}

/* Jump back to the first non-blank character of the line. */
void
grid_reader_cursor_back_to_indentation(struct grid_reader *gr)
{
	struct grid_cell	gc;
	u_int			px, py, xx, yy, oldx, oldy;

	yy = gr->gd->hsize + gr->gd->sy - 1;
	oldx = gr->cx;
	oldy = gr->cy;
	grid_reader_cursor_start_of_line(gr, 1);

	for (py = gr->cy; py <= yy; py++) {
		xx = grid_line_length(gr->gd, py);
		for (px = 0; px < xx; px++) {
			grid_get_cell(gr->gd, px, py, &gc);
			if ((gc.data.size != 1 || *gc.data.data != ' ') &&
			    ~gc.flags & GRID_FLAG_TAB &&
			    ~gc.flags & GRID_FLAG_PADDING) {
				gr->cx = px;
				gr->cy = py;
				return;
			}
		}
		if (~grid_get_line(gr->gd, py)->flags & GRID_LINE_WRAPPED)
			break;
	}
	gr->cx = oldx;
	gr->cy = oldy;
}

/* Get the end of the last used line. */
static void
grid_reader_output_end(struct grid *gd, u_int *x, u_int *y)
{
	u_int	last = gd->hsize + gd->sy - 1;

	while (last > 0 && grid_get_line(gd, last)->cellused == 0)
		last--;
	*x = grid_get_line(gd, last)->cellused;
	*y = last;
}

/* Find the most recent complete output at or before the cursor. */
static int
grid_reader_previous_output_range(struct grid_reader *gr, u_int *sx,
    u_int *sy, u_int *ex, u_int *ey)
{
	struct grid		*gd = gr->gd;
	struct grid_line	*gl;
	struct osc133_data	*od;
	u_int			 cursor_x = gr->cx, cursor_y = gr->cy;
	u_int			 start_x, start_y, y, total;
	int			 found = 0, have_prompt = 0;
	int			 pending = 0;
	int			 has_start, has_end, end_first;
	int			 cleared;

	total = gd->hsize + gd->sy;

	/*
	 * Scan from the top because markers arrive in their natural order, C
	 * then D. A backward scan would meet D first and need extra state to
	 * find its C. The last complete output found is the one wanted.
	 */
	for (y = 0; y < total && y <= cursor_y; y++) {
		gl = grid_get_line(gd, y);
		od = &gl->osc133_data;

		/* On the cursor's line, ignore markers after the cursor. */
		has_start = has_end = end_first = 0;
		if (gl->flags & GRID_LINE_START_OUTPUT) {
			if (y != cursor_y || od->out_start_col <= cursor_x)
				has_start = 1;
		}
		if (gl->flags & GRID_LINE_END_OUTPUT) {
			if (y != cursor_y || od->out_end_col <= cursor_x)
				has_end = 1;
		}

		/* An end before the start ends the previous output. */
		if (has_start && has_end && od->out_end_col < od->out_start_col)
			end_first = 1;

		/* A C marker starts an output that is pending until its D. */
		if (has_start && !end_first) {
			start_x = od->out_start_col;
			start_y = y;
			pending = 1;
		}

		/* The output may have cleared its C marker from the screen. */
		if (!pending && !have_prompt &&
		    (gl->flags & GRID_LINE_END_OUTPUT)) {
			cleared = 1;
			if ((gl->flags & GRID_LINE_START_PROMPT) &&
			    od->out_end_col > od->prompt_col)
				cleared = 0;
			if (cleared) {
				start_x = start_y = 0;
				pending = 1;
			}
		}

		/* A D marker completes the pending output. */
		if (pending && has_end) {
			*sx = start_x;
			*sy = start_y;
			*ex = od->out_end_col;
			*ey = y;
			found = 1;
			pending = 0;
		}
		if (has_start && end_first) {
			start_x = od->out_start_col;
			start_y = y;
			pending = 1;
		}

		/* A prompt drops an unfinished output. */
		if (gl->flags & GRID_LINE_START_PROMPT) {
			if (!has_start || od->out_start_col < od->prompt_col)
				pending = 0;
			have_prompt = 1;
		}
	}
	return (found);
}

/*
 * Find the output range for the command at the cursor, from its C marker to
 * its D marker. If the prompt has no output yet, use the previous output.
 * Returns 0 if there is no usable range.
 */
int
grid_reader_output_range(struct grid_reader *gr, u_int *sx, u_int *sy,
    u_int *ex, u_int *ey)
{
	struct grid		*gd = gr->gd;
	struct grid_line	*gl;
	struct osc133_data	*od;
	u_int			 cursor_x = gr->cx, cursor_y = gr->cy;
	u_int			 prompt_x = 0, prompt_y = UINT_MAX;
	u_int			 y, total;
	int			 found_start = 0, found_end = 0;
	int			 next_prompt, in_range, end_ok;
	int			 found;

	/* Find the last prompt at or before the cursor. */
	total = gd->hsize + gd->sy;
	for (y = 0; y < total; y++) {
		gl = grid_get_line(gd, y);
		od = &gl->osc133_data;
		if (~gl->flags & GRID_LINE_START_PROMPT)
			continue;
		if (y > cursor_y)
			break;
		if (y == cursor_y && od->prompt_col > cursor_x)
			break;
		prompt_y = y;
		prompt_x = od->prompt_col;
	}

	/* With no prompt, its A marker may have left history, so start at 0. */
	if (prompt_y == UINT_MAX)
		y = 0;
	else
		y = prompt_y;

	/* Walk down from the prompt to its C and D, up to the next prompt. */
	for (; y < total; y++) {
		gl = grid_get_line(gd, y);
		od = &gl->osc133_data;
		next_prompt = 0;
		if (y != prompt_y && (gl->flags & GRID_LINE_START_PROMPT))
			next_prompt = 1;

		/* Output before the next prompt on its line is ours. */
		if (y == prompt_y)
			in_range = (od->out_start_col >= prompt_x);
		else if (next_prompt)
			in_range = (od->out_start_col < od->prompt_col);
		else
			in_range = 1;
		if (gl->flags & GRID_LINE_START_OUTPUT && in_range) {
			*sx = od->out_start_col;
			*sy = y;
			found_start = 1;
		}

		/* Both A and C may have left history while D remains. */
		if (!found_start && prompt_y == UINT_MAX &&
		    (gl->flags & GRID_LINE_END_OUTPUT)) {
			if (!next_prompt || od->out_end_col <= od->prompt_col) {
				*sx = *sy = 0;
				found_start = 1;
			}
		}

		/* An output may end on the same line or the next prompt's. */
		if (found_start && (gl->flags & GRID_LINE_END_OUTPUT)) {
			end_ok = 1;
			if (y == prompt_y && od->out_end_col < prompt_x)
				end_ok = 0;
			if (y == *sy && od->out_end_col < *sx)
				end_ok = 0;
			if (end_ok) {
				*ex = od->out_end_col;
				*ey = y;
				found_end = 1;
				break;
			}
		}
		if (next_prompt)
			break;
	}

	/* The cursor is on a prompt with no output yet. */
	if (!found_start) {
		found = grid_reader_previous_output_range(gr, sx, sy, ex, ey);
		return (found);
	}

	/*
	 * Without a D marker the command is still running, so its output runs
	 * to the last used line. If the next prompt came first, it has no
	 * usable end.
	 */
	if (!found_end) {
		if (y != total)
			return (0);
		grid_reader_output_end(gd, ex, ey);
	}
	return (1);
}
