/* $OpenBSD$ */

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
grid_reader_cursor_right(struct grid_reader *gr, int wrap, int all)
{
	u_int			px;
	struct grid_cell	gc;

	if (all)
		px = gr->gd->sx;
	else
		px = grid_reader_line_length(gr);

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
	struct grid_cell	gc;

	grid_get_cell(gr->gd, gr->cx, gr->cy, &gc);
	if (gc.flags & GRID_FLAG_PADDING)
		return (0);
	return (utf8_cstrhas(set, &gc.data));
}

/* Move cursor to the start of the next word. */
void
grid_reader_cursor_next_word(struct grid_reader *gr, const char *separators)
{
	u_int	xx, yy;

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
	    grid_reader_in_set(gr, WHITESPACE))
		gr->cx++;
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
			if (!(gc.flags & GRID_FLAG_PADDING) &&
			    gc.data.size == jc->size &&
			    memcmp(gc.data.data, jc->data, gc.data.size) == 0) {
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
			if (!(gc.flags & GRID_FLAG_PADDING) &&
			    gc.data.size == jc->size &&
			    memcmp(gc.data.data, jc->data, gc.data.size) == 0) {
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
			if (gc.data.size != 1 || *gc.data.data != ' ') {
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
