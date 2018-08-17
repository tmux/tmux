/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static const char *window_copy_key_table(struct window_pane *);
static void	window_copy_command(struct window_pane *, struct client *,
		    struct session *, struct args *, struct mouse_event *);
static struct screen *window_copy_init(struct window_pane *,
		    struct cmd_find_state *, struct args *);
static void	window_copy_free(struct window_pane *);
static int	window_copy_pagedown(struct window_pane *, int, int);
static void	window_copy_next_paragraph(struct window_pane *);
static void	window_copy_previous_paragraph(struct window_pane *);
static void	window_copy_resize(struct window_pane *, u_int, u_int);

static void	window_copy_redraw_selection(struct window_pane *, u_int);
static void	window_copy_redraw_lines(struct window_pane *, u_int, u_int);
static void	window_copy_redraw_screen(struct window_pane *);
static void	window_copy_write_line(struct window_pane *,
		    struct screen_write_ctx *, u_int);
static void	window_copy_write_lines(struct window_pane *,
		    struct screen_write_ctx *, u_int, u_int);

static void	window_copy_scroll_to(struct window_pane *, u_int, u_int);
static int	window_copy_search_compare(struct grid *, u_int, u_int,
		    struct grid *, u_int, int);
static int	window_copy_search_lr(struct grid *, struct grid *, u_int *,
		    u_int, u_int, u_int, int);
static int	window_copy_search_rl(struct grid *, struct grid *, u_int *,
		    u_int, u_int, u_int, int);
static int	window_copy_search_marks(struct window_pane *, struct screen *);
static void	window_copy_clear_marks(struct window_pane *);
static void	window_copy_move_left(struct screen *, u_int *, u_int *);
static void	window_copy_move_right(struct screen *, u_int *, u_int *);
static int	window_copy_is_lowercase(const char *);
static int	window_copy_search_jump(struct window_pane *, struct grid *,
		    struct grid *, u_int, u_int, u_int, int, int, int);
static int	window_copy_search(struct window_pane *, int);
static int	window_copy_search_up(struct window_pane *);
static int	window_copy_search_down(struct window_pane *);
static void	window_copy_goto_line(struct window_pane *, const char *);
static void	window_copy_update_cursor(struct window_pane *, u_int, u_int);
static void	window_copy_start_selection(struct window_pane *);
static int	window_copy_adjust_selection(struct window_pane *, u_int *,
		    u_int *);
static int	window_copy_set_selection(struct window_pane *, int);
static int	window_copy_update_selection(struct window_pane *, int);
static void	window_copy_synchronize_cursor(struct window_pane *);
static void    *window_copy_get_selection(struct window_pane *, size_t *);
static void	window_copy_copy_buffer(struct window_pane *, const char *,
		    void *, size_t);
static void	window_copy_copy_pipe(struct window_pane *, struct session *,
		    const char *, const char *);
static void	window_copy_copy_selection(struct window_pane *, const char *);
static void	window_copy_append_selection(struct window_pane *,
		    const char *);
static void	window_copy_clear_selection(struct window_pane *);
static void	window_copy_copy_line(struct window_pane *, char **, size_t *,
		    u_int, u_int, u_int);
static int	window_copy_in_set(struct window_pane *, u_int, u_int,
		    const char *);
static u_int	window_copy_find_length(struct window_pane *, u_int);
static void	window_copy_cursor_start_of_line(struct window_pane *);
static void	window_copy_cursor_back_to_indentation(struct window_pane *);
static void	window_copy_cursor_end_of_line(struct window_pane *);
static void	window_copy_other_end(struct window_pane *);
static void	window_copy_cursor_left(struct window_pane *);
static void	window_copy_cursor_right(struct window_pane *);
static void	window_copy_cursor_up(struct window_pane *, int);
static void	window_copy_cursor_down(struct window_pane *, int);
static void	window_copy_cursor_jump(struct window_pane *);
static void	window_copy_cursor_jump_back(struct window_pane *);
static void	window_copy_cursor_jump_to(struct window_pane *);
static void	window_copy_cursor_jump_to_back(struct window_pane *);
static void	window_copy_cursor_next_word(struct window_pane *,
		    const char *);
static void	window_copy_cursor_next_word_end(struct window_pane *,
		    const char *);
static void	window_copy_cursor_previous_word(struct window_pane *,
		    const char *);
static void	window_copy_scroll_up(struct window_pane *, u_int);
static void	window_copy_scroll_down(struct window_pane *, u_int);
static void	window_copy_rectangle_toggle(struct window_pane *);
static void	window_copy_move_mouse(struct mouse_event *);
static void	window_copy_drag_update(struct client *, struct mouse_event *);

const struct window_mode window_copy_mode = {
	.name = "copy-mode",

	.init = window_copy_init,
	.free = window_copy_free,
	.resize = window_copy_resize,
	.key_table = window_copy_key_table,
	.command = window_copy_command,
};

enum {
	WINDOW_COPY_OFF,
	WINDOW_COPY_SEARCHUP,
	WINDOW_COPY_SEARCHDOWN,
	WINDOW_COPY_JUMPFORWARD,
	WINDOW_COPY_JUMPBACKWARD,
	WINDOW_COPY_JUMPTOFORWARD,
	WINDOW_COPY_JUMPTOBACKWARD,
};

enum {
	WINDOW_COPY_REL_POS_ABOVE,
	WINDOW_COPY_REL_POS_ON_SCREEN,
	WINDOW_COPY_REL_POS_BELOW,
};

/*
 * Copy mode's visible screen (the "screen" field) is filled from one of two
 * sources: the original contents of the pane (used when we actually enter via
 * the "copy-mode" command, to copy the contents of the current pane), or else
 * a series of lines containing the output from an output-writing tmux command
 * (such as any of the "show-*" or "list-*" commands).
 *
 * In either case, the full content of the copy-mode grid is pointed at by the
 * "backing" field, and is copied into "screen" as needed (that is, when
 * scrolling occurs). When copy-mode is backed by a pane, backing points
 * directly at that pane's screen structure (&wp->base); when backed by a list
 * of output-lines from a command, it points at a newly-allocated screen
 * structure (which is deallocated when the mode ends).
 */
struct window_copy_mode_data {
	struct screen	 screen;

	struct screen	*backing;
	int		 backing_written; /* backing display started */

	u_int		 oy;		/* number of lines scrolled up */

	u_int		 selx;		/* beginning of selection */
	u_int		 sely;

	u_int		 endselx;	/* end of selection */
	u_int		 endsely;

	enum {
		CURSORDRAG_NONE,	/* selection is independent of cursor */
		CURSORDRAG_ENDSEL,	/* end is synchronized with cursor */
		CURSORDRAG_SEL,		/* start is synchronized with cursor */
	} cursordrag;

	int		 modekeys;
	enum {
		LINE_SEL_NONE,
		LINE_SEL_LEFT_RIGHT,
		LINE_SEL_RIGHT_LEFT,
	} lineflag;			/* line selection mode */
	int		 rectflag;	/* in rectangle copy mode? */
	int		 scroll_exit;	/* exit on scroll to end? */

	u_int		 cx;
	u_int		 cy;

	u_int		 lastcx; 	/* position in last line w/ content */
	u_int		 lastsx;	/* size of last line w/ content */

	int		 searchtype;
	char		*searchstr;
	bitstr_t        *searchmark;
	u_int		 searchcount;
	int		 searchthis;
	int		 searchx;
	int		 searchy;
	int		 searcho;

	int		 jumptype;
	char		 jumpchar;
};

static struct screen *
window_copy_init(struct window_pane *wp, __unused struct cmd_find_state *fs,
    __unused struct args *args)
{
	struct window_copy_mode_data	*data;
	struct screen			*s;

	wp->modedata = data = xcalloc(1, sizeof *data);

	data->cursordrag = CURSORDRAG_NONE;
	data->lineflag = LINE_SEL_NONE;

	if (wp->searchstr != NULL) {
		data->searchtype = WINDOW_COPY_SEARCHUP;
		data->searchstr = xstrdup(wp->searchstr);
	} else {
		data->searchtype = WINDOW_COPY_OFF;
		data->searchstr = NULL;
	}
	data->searchmark = NULL;
	data->searchx = data->searchy = data->searcho = -1;

	if (wp->fd != -1)
		bufferevent_disable(wp->event, EV_READ|EV_WRITE);

	data->jumptype = WINDOW_COPY_OFF;
	data->jumpchar = '\0';

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	data->modekeys = options_get_number(wp->window->options, "mode-keys");

	data->backing = NULL;

	return (s);
}

void
window_copy_init_from_pane(struct window_pane *wp, int scroll_exit)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	if (wp->mode != &window_copy_mode)
		fatalx("not in copy mode");

	data->backing = &wp->base;
	data->cx = data->backing->cx;
	data->cy = data->backing->cy;
	data->scroll_exit = scroll_exit;

	s->cx = data->cx;
	s->cy = data->cy;

	screen_write_start(&ctx, NULL, s);
	for (i = 0; i < screen_size_y(s); i++)
		window_copy_write_line(wp, &ctx, i);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_init_for_output(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	data->backing = xmalloc(sizeof *data->backing);
	screen_init(data->backing, screen_size_x(&wp->base),
	    screen_size_y(&wp->base), UINT_MAX);
}

static void
window_copy_free(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	if (wp->fd != -1)
		bufferevent_enable(wp->event, EV_READ|EV_WRITE);

	free(data->searchmark);
	free(data->searchstr);

	if (data->backing != &wp->base) {
		screen_free(data->backing);
		free(data->backing);
	}
	screen_free(&data->screen);

	free(data);
}

void
window_copy_add(struct window_pane *wp, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	window_copy_vadd(wp, fmt, ap);
	va_end(ap);
}

void
window_copy_vadd(struct window_pane *wp, const char *fmt, va_list ap)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*backing = data->backing;
	struct screen_write_ctx	 	 back_ctx, ctx;
	struct grid_cell		 gc;
	u_int				 old_hsize, old_cy;

	if (backing == &wp->base)
		return;

	memcpy(&gc, &grid_default_cell, sizeof gc);

	old_hsize = screen_hsize(data->backing);
	screen_write_start(&back_ctx, NULL, backing);
	if (data->backing_written) {
		/*
		 * On the second or later line, do a CRLF before writing
		 * (so it's on a new line).
		 */
		screen_write_carriagereturn(&back_ctx);
		screen_write_linefeed(&back_ctx, 0, 8);
	} else
		data->backing_written = 1;
	old_cy = backing->cy;
	screen_write_vnputs(&back_ctx, 0, &gc, fmt, ap);
	screen_write_stop(&back_ctx);

	data->oy += screen_hsize(data->backing) - old_hsize;

	screen_write_start(&ctx, wp, &data->screen);

	/*
	 * If the history has changed, draw the top line.
	 * (If there's any history at all, it has changed.)
	 */
	if (screen_hsize(data->backing))
		window_copy_redraw_lines(wp, 0, 1);

	/* Write the new lines. */
	window_copy_redraw_lines(wp, old_cy, backing->cy - old_cy + 1);

	screen_write_stop(&ctx);
}

void
window_copy_pageup(struct window_pane *wp, int half_page)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 n, ox, oy, px, py;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wp, oy);

	if (data->lineflag == LINE_SEL_LEFT_RIGHT && oy == data->sely)
		window_copy_other_end(wp);

	if (data->cx != ox) {
		data->lastcx = data->cx;
		data->lastsx = ox;
	}
	data->cx = data->lastcx;

	n = 1;
	if (screen_size_y(s) > 2) {
		if (half_page)
			n = screen_size_y(s) / 2;
		else
			n = screen_size_y(s) - 2;
	}

	if (data->oy + n > screen_hsize(data->backing))
		data->oy = screen_hsize(data->backing);
	else
		data->oy += n;

	if (data->screen.sel == NULL || !data->rectflag) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wp, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
			window_copy_cursor_end_of_line(wp);
	}

	window_copy_update_selection(wp, 1);
	window_copy_redraw_screen(wp);
}

static int
window_copy_pagedown(struct window_pane *wp, int half_page, int scroll_exit)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 n, ox, oy, px, py;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wp, oy);

	if (data->lineflag == LINE_SEL_RIGHT_LEFT && oy == data->sely)
		window_copy_other_end(wp);

	if (data->cx != ox) {
		data->lastcx = data->cx;
		data->lastsx = ox;
	}
	data->cx = data->lastcx;

	n = 1;
	if (screen_size_y(s) > 2) {
		if (half_page)
			n = screen_size_y(s) / 2;
		else
			n = screen_size_y(s) - 2;
	}

	if (data->oy < n)
		data->oy = 0;
	else
		data->oy -= n;

	if (data->screen.sel == NULL || !data->rectflag) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wp, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
			window_copy_cursor_end_of_line(wp);
	}

	if (scroll_exit && data->oy == 0)
		return (1);
	window_copy_update_selection(wp, 1);
	window_copy_redraw_screen(wp);
	return (0);
}

static void
window_copy_previous_paragraph(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 oy;

	oy = screen_hsize(data->backing) + data->cy - data->oy;

	while (oy > 0 && window_copy_find_length(wp, oy) == 0)
		oy--;

	while (oy > 0 && window_copy_find_length(wp, oy) > 0)
		oy--;

	window_copy_scroll_to(wp, 0, oy);
}

static void
window_copy_next_paragraph(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 maxy, ox, oy;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	maxy = screen_hsize(data->backing) + screen_size_y(s) - 1;

	while (oy < maxy && window_copy_find_length(wp, oy) == 0)
		oy++;

	while (oy < maxy && window_copy_find_length(wp, oy) > 0)
		oy++;

	ox = window_copy_find_length(wp, oy);
	window_copy_scroll_to(wp, ox, oy);
}

static void
window_copy_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;

	screen_resize(s, sx, sy, 1);
	if (data->backing != &wp->base)
		screen_resize(data->backing, sx, sy, 1);

	if (data->cy > sy - 1)
		data->cy = sy - 1;
	if (data->cx > sx)
		data->cx = sx;
	if (data->oy > screen_hsize(data->backing))
		data->oy = screen_hsize(data->backing);

	window_copy_clear_selection(wp);

	screen_write_start(&ctx, NULL, s);
	window_copy_write_lines(wp, &ctx, 0, screen_size_y(s) - 1);
	screen_write_stop(&ctx);

	if (data->searchmark != NULL)
		window_copy_search_marks(wp, NULL);
	data->searchx = data->cx;
	data->searchy = data->cy;
	data->searcho = data->oy;

	window_copy_redraw_screen(wp);
}

static const char *
window_copy_key_table(struct window_pane *wp)
{
	if (options_get_number(wp->window->options, "mode-keys") == MODEKEY_VI)
		return ("copy-mode-vi");
	return ("copy-mode");
}

static void
window_copy_command(struct window_pane *wp, struct client *c, struct session *s,
    struct args *args, struct mouse_event *m)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*sn = &data->screen;
	const char			*command, *argument, *ws;
	u_int				 np = wp->modeprefix;
	int				 cancel = 0, redraw = 0, scroll_exit;
	char				 prefix;

	if (args->argc == 0)
		return;
	command = args->argv[0];

	if (m != NULL && m->valid)
		window_copy_move_mouse(m);

	if (args->argc == 1) {
		if (strcmp(command, "append-selection") == 0) {
			if (s != NULL)
				window_copy_append_selection(wp, NULL);
			window_copy_clear_selection(wp);
			redraw = 1;
		}
		if (strcmp(command, "append-selection-and-cancel") == 0) {
			if (s != NULL)
				window_copy_append_selection(wp, NULL);
			window_copy_clear_selection(wp);
			redraw = 1;
			cancel = 1;
		}
		if (strcmp(command, "back-to-indentation") == 0)
			window_copy_cursor_back_to_indentation(wp);
		if (strcmp(command, "begin-selection") == 0) {
			if (m != NULL)
				window_copy_start_drag(c, m);
			else {
				data->lineflag = LINE_SEL_NONE;
				window_copy_start_selection(wp);
				redraw = 1;
			}
		}
		if (strcmp(command, "stop-selection") == 0)
			data->cursordrag = CURSORDRAG_NONE;
		if (strcmp(command, "bottom-line") == 0) {
			data->cx = 0;
			data->cy = screen_size_y(sn) - 1;
			window_copy_update_selection(wp, 1);
			redraw = 1;
		}
		if (strcmp(command, "cancel") == 0)
			cancel = 1;
		if (strcmp(command, "clear-selection") == 0) {
			window_copy_clear_selection(wp);
			redraw = 1;
		}
		if (strcmp(command, "copy-end-of-line") == 0) {
			window_copy_start_selection(wp);
			for (; np > 1; np--)
				window_copy_cursor_down(wp, 0);
			window_copy_cursor_end_of_line(wp);
			redraw = 1;

			if (s != NULL) {
				window_copy_copy_selection(wp, NULL);
				cancel = 1;
			}
		}
		if (strcmp(command, "copy-line") == 0) {
			window_copy_cursor_start_of_line(wp);
			window_copy_start_selection(wp);
			for (; np > 1; np--)
				window_copy_cursor_down(wp, 0);
			window_copy_cursor_end_of_line(wp);
			redraw = 1;

			if (s != NULL) {
				window_copy_copy_selection(wp, NULL);
				cancel = 1;
			}
		}
		if (strcmp(command, "copy-selection") == 0) {
			if (s != NULL)
				window_copy_copy_selection(wp, NULL);
			window_copy_clear_selection(wp);
			redraw = 1;
		}
		if (strcmp(command, "copy-selection-and-cancel") == 0) {
			if (s != NULL)
				window_copy_copy_selection(wp, NULL);
			window_copy_clear_selection(wp);
			redraw = 1;
			cancel = 1;
		}
		if (strcmp(command, "cursor-down") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_down(wp, 0);
		}
		if (strcmp(command, "cursor-left") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_left(wp);
		}
		if (strcmp(command, "cursor-right") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_right(wp);
		}
		if (strcmp(command, "cursor-up") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_up(wp, 0);
		}
		if (strcmp(command, "end-of-line") == 0)
			window_copy_cursor_end_of_line(wp);
		if (strcmp(command, "halfpage-down") == 0 ||
		    strcmp(command, "halfpage-down-and-cancel") == 0) {
			if (strcmp(command, "halfpage-down-and-cancel") == 0)
				scroll_exit = 1;
			else
				scroll_exit = data->scroll_exit;
			for (; np != 0; np--) {
				if (window_copy_pagedown(wp, 1, scroll_exit)) {
					cancel = 1;
					break;
				}
			}
		}
		if (strcmp(command, "halfpage-up") == 0) {
			for (; np != 0; np--)
				window_copy_pageup(wp, 1);
		}
		if (strcmp(command, "history-bottom") == 0) {
			data->cx = 0;
			data->cy = screen_size_y(sn) - 1;
			data->oy = 0;
			window_copy_update_selection(wp, 1);
			redraw = 1;
		}
		if (strcmp(command, "history-top") == 0) {
			data->cx = 0;
			data->cy = 0;
			data->oy = screen_hsize(data->backing);
			window_copy_update_selection(wp, 1);
			redraw = 1;
		}
		if (strcmp(command, "jump-again") == 0) {
			switch (data->jumptype) {
			case WINDOW_COPY_JUMPFORWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump(wp);
				break;
			case WINDOW_COPY_JUMPBACKWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump_back(wp);
				break;
			case WINDOW_COPY_JUMPTOFORWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump_to(wp);
				break;
			case WINDOW_COPY_JUMPTOBACKWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump_to_back(wp);
				break;
			}
		}
		if (strcmp(command, "jump-reverse") == 0) {
			switch (data->jumptype) {
			case WINDOW_COPY_JUMPFORWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump_back(wp);
				break;
			case WINDOW_COPY_JUMPBACKWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump(wp);
				break;
			case WINDOW_COPY_JUMPTOFORWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump_to_back(wp);
				break;
			case WINDOW_COPY_JUMPTOBACKWARD:
				for (; np != 0; np--)
					window_copy_cursor_jump_to(wp);
				break;
			}
		}
		if (strcmp(command, "middle-line") == 0) {
			data->cx = 0;
			data->cy = (screen_size_y(sn) - 1) / 2;
			window_copy_update_selection(wp, 1);
			redraw = 1;
		}
		if (strcmp(command, "next-paragraph") == 0) {
			for (; np != 0; np--)
				window_copy_next_paragraph(wp);
		}
		if (strcmp(command, "next-space") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_next_word(wp, " ");
		}
		if (strcmp(command, "next-space-end") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_next_word_end(wp, " ");
		}
		if (strcmp(command, "next-word") == 0) {
			ws = options_get_string(s->options, "word-separators");
			for (; np != 0; np--)
				window_copy_cursor_next_word(wp, ws);
		}
		if (strcmp(command, "next-word-end") == 0) {
			ws = options_get_string(s->options, "word-separators");
			for (; np != 0; np--)
				window_copy_cursor_next_word_end(wp, ws);
		}
		if (strcmp(command, "other-end") == 0) {
			if ((np % 2) != 0)
				window_copy_other_end(wp);
		}
		if (strcmp(command, "page-down") == 0 ||
		    strcmp(command, "page-down-and-cancel") == 0) {
			if (strcmp(command, "page-down-and-cancel") == 0)
				scroll_exit = 1;
			else
				scroll_exit = data->scroll_exit;
			for (; np != 0; np--) {
				if (window_copy_pagedown(wp, 0, scroll_exit)) {
					cancel = 1;
					break;
				}
			}
		}
		if (strcmp(command, "page-up") == 0) {
			for (; np != 0; np--)
				window_copy_pageup(wp, 0);
		}
		if (strcmp(command, "previous-paragraph") == 0) {
			for (; np != 0; np--)
				window_copy_previous_paragraph(wp);
		}
		if (strcmp(command, "previous-space") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_previous_word(wp, " ");
		}
		if (strcmp(command, "previous-word") == 0) {
			ws = options_get_string(s->options, "word-separators");
			for (; np != 0; np--)
				window_copy_cursor_previous_word(wp, ws);
		}
		if (strcmp(command, "rectangle-toggle") == 0) {
			data->lineflag = LINE_SEL_NONE;
			window_copy_rectangle_toggle(wp);
		}
		if (strcmp(command, "scroll-down") == 0 ||
		    strcmp(command, "scroll-down-and-cancel") == 0) {
			if (strcmp(command, "scroll-down-and-cancel") == 0)
				scroll_exit = 1;
			else
				scroll_exit = data->scroll_exit;
			for (; np != 0; np--)
				window_copy_cursor_down(wp, 1);
			if (scroll_exit && data->oy == 0)
				cancel = 1;
		}
		if (strcmp(command, "scroll-up") == 0) {
			for (; np != 0; np--)
				window_copy_cursor_up(wp, 1);
		}
		if (strcmp(command, "search-again") == 0) {
			if (data->searchtype == WINDOW_COPY_SEARCHUP) {
				for (; np != 0; np--)
					window_copy_search_up(wp);
			} else if (data->searchtype == WINDOW_COPY_SEARCHDOWN) {
				for (; np != 0; np--)
					window_copy_search_down(wp);
			}
		}
		if (strcmp(command, "search-reverse") == 0) {
			if (data->searchtype == WINDOW_COPY_SEARCHUP) {
				for (; np != 0; np--)
					window_copy_search_down(wp);
			} else if (data->searchtype == WINDOW_COPY_SEARCHDOWN) {
				for (; np != 0; np--)
					window_copy_search_up(wp);
			}
		}
		if (strcmp(command, "select-line") == 0) {
			data->lineflag = LINE_SEL_LEFT_RIGHT;
			data->rectflag = 0;
			window_copy_cursor_start_of_line(wp);
			window_copy_start_selection(wp);
			for (; np > 1; np--)
				window_copy_cursor_down(wp, 0);
			window_copy_cursor_end_of_line(wp);
			redraw = 1;
		}
		if (strcmp(command, "select-word") == 0) {
			data->lineflag = LINE_SEL_LEFT_RIGHT;
			data->rectflag = 0;
			ws = options_get_string(s->options, "word-separators");
			window_copy_cursor_previous_word(wp, ws);
			window_copy_start_selection(wp);
			window_copy_cursor_next_word_end(wp, ws);
			redraw = 1;
		}
		if (strcmp(command, "start-of-line") == 0)
			window_copy_cursor_start_of_line(wp);
		if (strcmp(command, "top-line") == 0) {
			data->cx = 0;
			data->cy = 0;
			window_copy_update_selection(wp, 1);
			redraw = 1;
		}
	} else if (args->argc == 2 && *args->argv[1] != '\0') {
		argument = args->argv[1];
		if (strcmp(command, "copy-pipe") == 0) {
			if (s != NULL)
				window_copy_copy_pipe(wp, s, NULL, argument);
		}
		if (strcmp(command, "copy-pipe-and-cancel") == 0) {
			if (s != NULL) {
				window_copy_copy_pipe(wp, s, NULL, argument);
				cancel = 1;
			}
		}
		if (strcmp(command, "goto-line") == 0)
			window_copy_goto_line(wp, argument);
		if (strcmp(command, "jump-backward") == 0) {
			data->jumptype = WINDOW_COPY_JUMPBACKWARD;
			data->jumpchar = *argument;
			for (; np != 0; np--)
				window_copy_cursor_jump_back(wp);
		}
		if (strcmp(command, "jump-forward") == 0) {
			data->jumptype = WINDOW_COPY_JUMPFORWARD;
			data->jumpchar = *argument;
			for (; np != 0; np--)
				window_copy_cursor_jump(wp);
		}
		if (strcmp(command, "jump-to-backward") == 0) {
			data->jumptype = WINDOW_COPY_JUMPTOBACKWARD;
			data->jumpchar = *argument;
			for (; np != 0; np--)
				window_copy_cursor_jump_to_back(wp);
		}
		if (strcmp(command, "jump-to-forward") == 0) {
			data->jumptype = WINDOW_COPY_JUMPTOFORWARD;
			data->jumpchar = *argument;
			for (; np != 0; np--)
				window_copy_cursor_jump_to(wp);
		}
		if (strcmp(command, "search-backward") == 0) {
			data->searchtype = WINDOW_COPY_SEARCHUP;
			free(data->searchstr);
			data->searchstr = xstrdup(argument);
			for (; np != 0; np--)
				window_copy_search_up(wp);
		}
		if (strcmp(command, "search-forward") == 0) {
			data->searchtype = WINDOW_COPY_SEARCHDOWN;
			free(data->searchstr);
			data->searchstr = xstrdup(argument);
			for (; np != 0; np--)
				window_copy_search_down(wp);
		}
		if (strcmp(command, "search-backward-incremental") == 0) {
			prefix = *argument++;
			if (data->searchx == -1 || data->searchy == -1) {
				data->searchx = data->cx;
				data->searchy = data->cy;
				data->searcho = data->oy;
			} else if (data->searchstr != NULL &&
			    strcmp(argument, data->searchstr) != 0) {
				data->cx = data->searchx;
				data->cy = data->searchy;
				data->oy = data->searcho;
				redraw = 1;
			}
			if (*argument == '\0') {
				window_copy_clear_marks(wp);
				redraw = 1;
			} else if (prefix == '=' || prefix == '-') {
				data->searchtype = WINDOW_COPY_SEARCHUP;
				free(data->searchstr);
				data->searchstr = xstrdup(argument);
				if (!window_copy_search_up(wp)) {
					window_copy_clear_marks(wp);
					redraw = 1;
				}
			} else if (prefix == '+') {
				data->searchtype = WINDOW_COPY_SEARCHDOWN;
				free(data->searchstr);
				data->searchstr = xstrdup(argument);
				if (!window_copy_search_down(wp)) {
					window_copy_clear_marks(wp);
					redraw = 1;
				}
			}
		}
		if (strcmp(command, "search-forward-incremental") == 0) {
			prefix = *argument++;
			if (data->searchx == -1 || data->searchy == -1) {
				data->searchx = data->cx;
				data->searchy = data->cy;
				data->searcho = data->oy;
			} else if (data->searchstr != NULL &&
			    strcmp(argument, data->searchstr) != 0) {
				data->cx = data->searchx;
				data->cy = data->searchy;
				data->oy = data->searcho;
				redraw = 1;
			}
			if (*argument == '\0') {
				window_copy_clear_marks(wp);
				redraw = 1;
			} else if (prefix == '=' || prefix == '+') {
				data->searchtype = WINDOW_COPY_SEARCHDOWN;
				free(data->searchstr);
				data->searchstr = xstrdup(argument);
				if (!window_copy_search_down(wp)) {
					window_copy_clear_marks(wp);
					redraw = 1;
				}
			} else if (prefix == '-') {
				data->searchtype = WINDOW_COPY_SEARCHUP;
				free(data->searchstr);
				data->searchstr = xstrdup(argument);
				if (!window_copy_search_up(wp)) {
					window_copy_clear_marks(wp);
					redraw = 1;
				}
			}
		}
	}

	if (strncmp(command, "search-", 7) != 0 && data->searchmark != NULL) {
		window_copy_clear_marks(wp);
		redraw = 1;
		data->searchx = data->searchy = -1;
	}

	if (cancel)
		window_pane_reset_mode(wp);
	else if (redraw)
		window_copy_redraw_screen(wp);
	wp->modeprefix = 1;
}

static void
window_copy_scroll_to(struct window_pane *wp, u_int px, u_int py)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct grid			*gd = data->backing->grid;
	u_int				 offset, gap;

	data->cx = px;

	if (py >= gd->hsize - data->oy && py < gd->hsize - data->oy + gd->sy)
		data->cy = py - (gd->hsize - data->oy);
	else {
		gap = gd->sy / 4;
		if (py < gd->sy) {
			offset = 0;
			data->cy = py;
		} else if (py > gd->hsize + gd->sy - gap) {
			offset = gd->hsize;
			data->cy = py - gd->hsize;
		} else {
			offset = py + gap - gd->sy;
			data->cy = py - offset;
		}
		data->oy = gd->hsize - offset;
	}

	window_copy_update_selection(wp, 1);
	window_copy_redraw_screen(wp);
}

static int
window_copy_search_compare(struct grid *gd, u_int px, u_int py,
    struct grid *sgd, u_int spx, int cis)
{
	struct grid_cell	 gc, sgc;
	const struct utf8_data	*ud, *sud;

	grid_get_cell(gd, px, py, &gc);
	ud = &gc.data;
	grid_get_cell(sgd, spx, 0, &sgc);
	sud = &sgc.data;

	if (ud->size != sud->size || ud->width != sud->width)
		return (0);

	if (cis && ud->size == 1)
		return (tolower(ud->data[0]) == sud->data[0]);

	return (memcmp(ud->data, sud->data, ud->size) == 0);
}

static int
window_copy_search_lr(struct grid *gd,
    struct grid *sgd, u_int *ppx, u_int py, u_int first, u_int last, int cis)
{
	u_int	ax, bx, px;
	int	matched;

	for (ax = first; ax < last; ax++) {
		if (ax + sgd->sx > gd->sx)
			break;
		for (bx = 0; bx < sgd->sx; bx++) {
			px = ax + bx;
			matched = window_copy_search_compare(gd, px, py, sgd,
			    bx, cis);
			if (!matched)
				break;
		}
		if (bx == sgd->sx) {
			*ppx = ax;
			return (1);
		}
	}
	return (0);
}

static int
window_copy_search_rl(struct grid *gd,
    struct grid *sgd, u_int *ppx, u_int py, u_int first, u_int last, int cis)
{
	u_int	ax, bx, px;
	int	matched;

	for (ax = last + 1; ax > first; ax--) {
		if (gd->sx - (ax - 1) < sgd->sx)
			continue;
		for (bx = 0; bx < sgd->sx; bx++) {
			px = ax - 1 + bx;
			matched = window_copy_search_compare(gd, px, py, sgd,
			    bx, cis);
			if (!matched)
				break;
		}
		if (bx == sgd->sx) {
			*ppx = ax - 1;
			return (1);
		}
	}
	return (0);
}

static void
window_copy_move_left(struct screen *s, u_int *fx, u_int *fy)
{
	if (*fx == 0) {	/* left */
		if (*fy == 0) /* top */
			return;
		*fx = screen_size_x(s) - 1;
		*fy = *fy - 1;
	} else
		*fx = *fx - 1;
}

static void
window_copy_move_right(struct screen *s, u_int *fx, u_int *fy)
{
	if (*fx == screen_size_x(s) - 1) { /* right */
		if (*fy == screen_hsize(s) + screen_size_y(s)) /* bottom */
			return;
		*fx = 0;
		*fy = *fy + 1;
	} else
		*fx = *fx + 1;
}

static int
window_copy_is_lowercase(const char *ptr)
{
	while (*ptr != '\0') {
		if (*ptr != tolower((u_char)*ptr))
			return (0);
		++ptr;
	}
	return (1);
}

/*
 * Search for text stored in sgd starting from position fx,fy up to endline. If
 * found, jump to it. If cis then ignore case. The direction is 0 for searching
 * up, down otherwise. If wrap then go to begin/end of grid and try again if
 * not found.
 */
static int
window_copy_search_jump(struct window_pane *wp, struct grid *gd,
    struct grid *sgd, u_int fx, u_int fy, u_int endline, int cis, int wrap,
    int direction)
{
	u_int	i, px;
	int	found;

	found = 0;
	if (direction) {
		for (i = fy; i <= endline; i++) {
			found = window_copy_search_lr(gd, sgd, &px, i, fx,
			    gd->sx, cis);
			if (found)
				break;
			fx = 0;
		}
	} else {
		for (i = fy + 1; endline < i; i--) {
			found = window_copy_search_rl(gd, sgd, &px, i - 1, 0,
			    fx, cis);
			if (found) {
				i--;
				break;
			}
			fx = gd->sx;
		}
	}

	if (found) {
		window_copy_scroll_to(wp, px, i);
		return (1);
	}
	if (wrap) {
		return (window_copy_search_jump(wp, gd, sgd,
		    direction ? 0 : gd->sx - 1,
		    direction ? 0 : gd->hsize + gd->sy - 1, fy, cis, 0,
		    direction));
	}
	return (0);
}

/*
 * Search in for text searchstr. If direction is 0 then search up, otherwise
 * down.
 */
static int
window_copy_search(struct window_pane *wp, int direction)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = data->backing, ss;
	struct screen_write_ctx		 ctx;
	struct grid			*gd = s->grid;
	u_int				 fx, fy, endline;
	int				 wrapflag, cis, found;

	free(wp->searchstr);
	wp->searchstr = xstrdup(data->searchstr);

	fx = data->cx;
	fy = screen_hsize(data->backing) - data->oy + data->cy;

	screen_init(&ss, screen_write_strlen("%s", data->searchstr), 1, 0);
	screen_write_start(&ctx, NULL, &ss);
	screen_write_nputs(&ctx, -1, &grid_default_cell, "%s", data->searchstr);
	screen_write_stop(&ctx);

	if (direction)
		window_copy_move_right(s, &fx, &fy);
	else
		window_copy_move_left(s, &fx, &fy);
	window_copy_clear_selection(wp);

	wrapflag = options_get_number(wp->window->options, "wrap-search");
	cis = window_copy_is_lowercase(data->searchstr);

	if (direction)
		endline = gd->hsize + gd->sy - 1;
	else
		endline = 0;
	found = window_copy_search_jump(wp, gd, ss.grid, fx, fy, endline, cis,
	    wrapflag, direction);

	if (window_copy_search_marks(wp, &ss))
		window_copy_redraw_screen(wp);

	screen_free(&ss);
	return (found);
}

static int
window_copy_search_marks(struct window_pane *wp, struct screen *ssp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = data->backing, ss;
	struct screen_write_ctx		 ctx;
	struct grid			*gd = s->grid;
	int				 found, cis, which = -1;
	u_int				 px, py, b, nfound = 0, width;

	if (ssp == NULL) {
		width = screen_write_strlen("%s", data->searchstr);
		screen_init(&ss, width, 1, 0);
		screen_write_start(&ctx, NULL, &ss);
		screen_write_nputs(&ctx, -1, &grid_default_cell, "%s",
		    data->searchstr);
		screen_write_stop(&ctx);
		ssp = &ss;
	} else
		width = screen_size_x(ssp);

	cis = window_copy_is_lowercase(data->searchstr);

	free(data->searchmark);
	data->searchmark = bit_alloc((gd->hsize + gd->sy) * gd->sx);

	for (py = 0; py < gd->hsize + gd->sy; py++) {
		px = 0;
		for (;;) {
			found = window_copy_search_lr(gd, ssp->grid, &px, py,
			    px, gd->sx, cis);
			if (!found)
				break;

			nfound++;
			if (px == data->cx && py == gd->hsize + data->cy - data->oy)
				which = nfound;

			b = (py * gd->sx) + px;
			bit_nset(data->searchmark, b, b + width - 1);

			px++;
		}
	}

	if (which != -1)
		data->searchthis = 1 + nfound - which;
	else
		data->searchthis = -1;
	data->searchcount = nfound;

	if (ssp == &ss)
		screen_free(&ss);
	return (nfound);
}

static void
window_copy_clear_marks(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	free(data->searchmark);
	data->searchmark = NULL;
}

static int
window_copy_search_up(struct window_pane *wp)
{
	return (window_copy_search(wp, 0));
}

static int
window_copy_search_down(struct window_pane *wp)
{
	return (window_copy_search(wp, 1));
}

static void
window_copy_goto_line(struct window_pane *wp, const char *linestr)
{
	struct window_copy_mode_data	*data = wp->modedata;
	const char			*errstr;
	u_int				 lineno;

	lineno = strtonum(linestr, 0, screen_hsize(data->backing), &errstr);
	if (errstr != NULL)
		return;

	data->oy = lineno;
	window_copy_update_selection(wp, 1);
	window_copy_redraw_screen(wp);
}

static void
window_copy_write_line(struct window_pane *wp, struct screen_write_ctx *ctx,
    u_int py)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct options			*oo = wp->window->options;
	struct grid_cell		 gc;
	char				 hdr[512];
	size_t				 size = 0;

	style_apply(&gc, oo, "mode-style");
	gc.flags |= GRID_FLAG_NOPALETTE;

	if (py == 0) {
		if (data->searchmark == NULL) {
			size = xsnprintf(hdr, sizeof hdr,
			    "[%u/%u]", data->oy, screen_hsize(data->backing));
		} else {
			if (data->searchthis == -1) {
				size = xsnprintf(hdr, sizeof hdr,
				    "(%u results) [%d/%u]", data->searchcount,
				    data->oy, screen_hsize(data->backing));
			} else {
				size = xsnprintf(hdr, sizeof hdr,
				    "(%u/%u results) [%d/%u]", data->searchthis,
				    data->searchcount, data->oy,
				    screen_hsize(data->backing));
			}
		}
		if (size > screen_size_x(s))
			size = screen_size_x(s);
		screen_write_cursormove(ctx, screen_size_x(s) - size, 0);
		screen_write_puts(ctx, &gc, "%s", hdr);
	} else
		size = 0;

	if (size < screen_size_x(s)) {
		screen_write_cursormove(ctx, 0, py);
		screen_write_copy(ctx, data->backing, 0,
		    (screen_hsize(data->backing) - data->oy) + py,
		    screen_size_x(s) - size, 1, data->searchmark, &gc);
	}

	if (py == data->cy && data->cx == screen_size_x(s)) {
		memcpy(&gc, &grid_default_cell, sizeof gc);
		screen_write_cursormove(ctx, screen_size_x(s) - 1, py);
		screen_write_putc(ctx, &gc, '$');
	}
}

static void
window_copy_write_lines(struct window_pane *wp, struct screen_write_ctx *ctx,
    u_int py, u_int ny)
{
	u_int	yy;

	for (yy = py; yy < py + ny; yy++)
		window_copy_write_line(wp, ctx, py);
}

static void
window_copy_redraw_selection(struct window_pane *wp, u_int old_y)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 new_y, start, end;

	new_y = data->cy;
	if (old_y <= new_y) {
		start = old_y;
		end = new_y;
	} else {
		start = new_y;
		end = old_y;
	}
	window_copy_redraw_lines(wp, start, end - start + 1);
}

static void
window_copy_redraw_lines(struct window_pane *wp, u_int py, u_int ny)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start(&ctx, wp, NULL);
	for (i = py; i < py + ny; i++)
		window_copy_write_line(wp, &ctx, i);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

static void
window_copy_redraw_screen(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	window_copy_redraw_lines(wp, 0, screen_size_y(&data->screen));
}

static void
window_copy_synchronize_cursor(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 xx, yy;

	xx = data->cx;
	yy = screen_hsize(data->backing) + data->cy - data->oy;

	switch (data->cursordrag) {
	case CURSORDRAG_ENDSEL:
		data->endselx = xx;
		data->endsely = yy;
		break;
	case CURSORDRAG_SEL:
		data->selx = xx;
		data->sely = yy;
		break;
	case CURSORDRAG_NONE:
		break;
	}
}

static void
window_copy_update_cursor(struct window_pane *wp, u_int cx, u_int cy)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int				 old_cx, old_cy;

	old_cx = data->cx; old_cy = data->cy;
	data->cx = cx; data->cy = cy;
	if (old_cx == screen_size_x(s))
		window_copy_redraw_lines(wp, old_cy, 1);
	if (data->cx == screen_size_x(s))
		window_copy_redraw_lines(wp, data->cy, 1);
	else {
		screen_write_start(&ctx, wp, NULL);
		screen_write_cursormove(&ctx, data->cx, data->cy);
		screen_write_stop(&ctx);
	}
}

static void
window_copy_start_selection(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	data->selx = data->cx;
	data->sely = screen_hsize(data->backing) + data->cy - data->oy;

	data->endselx = data->selx;
	data->endsely = data->sely;

	data->cursordrag = CURSORDRAG_ENDSEL;

	window_copy_set_selection(wp, 1);
}

static int
window_copy_adjust_selection(struct window_pane *wp, u_int *selx, u_int *sely)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int 				 sx, sy, ty;
	int				 relpos;

	sx = *selx;
	sy = *sely;

	ty = screen_hsize(data->backing) - data->oy;
	if (sy < ty) {
		relpos = WINDOW_COPY_REL_POS_ABOVE;
		if (!data->rectflag)
			sx = 0;
		sy = 0;
	} else if (sy > ty + screen_size_y(s) - 1) {
		relpos = WINDOW_COPY_REL_POS_BELOW;
		if (!data->rectflag)
			sx = screen_size_x(s) - 1;
		sy = screen_size_y(s) - 1;
	} else {
		relpos = WINDOW_COPY_REL_POS_ON_SCREEN;
		sy -= ty;
	}

	*selx = sx;
	*sely = sy;
	return (relpos);
}

static int
window_copy_update_selection(struct window_pane *wp, int may_redraw)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	if (s->sel == NULL && data->lineflag == LINE_SEL_NONE)
		return (0);
	return (window_copy_set_selection(wp, may_redraw));
}

static int
window_copy_set_selection(struct window_pane *wp, int may_redraw)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct options			*oo = wp->window->options;
	struct grid_cell		 gc;
	u_int				 sx, sy, cy, endsx, endsy;
	int				 startrelpos, endrelpos;

	window_copy_synchronize_cursor(wp);

	/* Adjust the selection. */
	sx = data->selx;
	sy = data->sely;
	startrelpos = window_copy_adjust_selection(wp, &sx, &sy);

	/* Adjust the end of selection. */
	endsx = data->endselx;
	endsy = data->endsely;
	endrelpos = window_copy_adjust_selection(wp, &endsx, &endsy);

	/* Selection is outside of the current screen */
	if (startrelpos == endrelpos &&
	    startrelpos != WINDOW_COPY_REL_POS_ON_SCREEN) {
		screen_hide_selection(s);
		return (0);
	}

	/* Set colours and selection. */
	style_apply(&gc, oo, "mode-style");
	gc.flags |= GRID_FLAG_NOPALETTE;
	screen_set_selection(s, sx, sy, endsx, endsy, data->rectflag,
	    data->modekeys, &gc);

	if (data->rectflag && may_redraw) {
		/*
		 * Can't rely on the caller to redraw the right lines for
		 * rectangle selection - find the highest line and the number
		 * of lines, and redraw just past that in both directions
		 */
		cy = data->cy;
		if (data->cursordrag == CURSORDRAG_ENDSEL) {
			if (sy < cy)
				window_copy_redraw_lines(wp, sy, cy - sy + 1);
			else
				window_copy_redraw_lines(wp, cy, sy - cy + 1);
		} else {
			if (endsy < cy)
				window_copy_redraw_lines(wp, endsy, cy - endsy + 1);
			else
				window_copy_redraw_lines(wp, cy, endsy - cy + 1);
		}
	}

	return (1);
}

static void *
window_copy_get_selection(struct window_pane *wp, size_t *len)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	char				*buf;
	size_t				 off;
	u_int				 i, xx, yy, sx, sy, ex, ey, ey_last;
	u_int				 firstsx, lastex, restex, restsx, selx;
	int				 keys;

	if (data->screen.sel == NULL && data->lineflag == LINE_SEL_NONE)
		return (NULL);

	buf = xmalloc(1);
	off = 0;

	*buf = '\0';

	/*
	 * The selection extends from selx,sely to (adjusted) cx,cy on
	 * the base screen.
	 */

	/* Find start and end. */
	xx = data->endselx;
	yy = data->endsely;
	if (yy < data->sely || (yy == data->sely && xx < data->selx)) {
		sx = xx; sy = yy;
		ex = data->selx; ey = data->sely;
	} else {
		sx = data->selx; sy = data->sely;
		ex = xx; ey = yy;
	}

	/* Trim ex to end of line. */
	ey_last = window_copy_find_length(wp, ey);
	if (ex > ey_last)
		ex = ey_last;

	/*
	 * Deal with rectangle-copy if necessary; four situations: start of
	 * first line (firstsx), end of last line (lastex), start (restsx) and
	 * end (restex) of all other lines.
	 */
	xx = screen_size_x(s);

	/*
	 * Behave according to mode-keys. If it is emacs, copy like emacs,
	 * keeping the top-left-most character, and dropping the
	 * bottom-right-most, regardless of copy direction. If it is vi, also
	 * keep bottom-right-most character.
	 */
	keys = options_get_number(wp->window->options, "mode-keys");
	if (data->rectflag) {
		/*
		 * Need to ignore the column with the cursor in it, which for
		 * rectangular copy means knowing which side the cursor is on.
		 */
		if (data->cursordrag == CURSORDRAG_ENDSEL)
			selx = data->selx;
		else
			selx = data->endselx;
		if (selx < data->cx) {
			/* Selection start is on the left. */
			if (keys == MODEKEY_EMACS) {
				lastex = data->cx;
				restex = data->cx;
			}
			else {
				lastex = data->cx + 1;
				restex = data->cx + 1;
			}
			firstsx = selx;
			restsx = selx;
		} else {
			/* Cursor is on the left. */
			lastex = selx + 1;
			restex = selx + 1;
			firstsx = data->cx;
			restsx = data->cx;
		}
	} else {
		if (keys == MODEKEY_EMACS)
			lastex = ex;
		else
			lastex = ex + 1;
		restex = xx;
		firstsx = sx;
		restsx = 0;
	}

	/* Copy the lines. */
	for (i = sy; i <= ey; i++) {
		window_copy_copy_line(wp, &buf, &off, i,
		    (i == sy ? firstsx : restsx),
		    (i == ey ? lastex : restex));
	}

	/* Don't bother if no data. */
	if (off == 0) {
		free(buf);
		return (NULL);
	}
	if (keys == MODEKEY_EMACS || lastex <= ey_last)
		off -= 1; /* remove final \n (unless at end in vi mode) */
	*len = off;
	return (buf);
}

static void
window_copy_copy_buffer(struct window_pane *wp, const char *bufname, void *buf,
    size_t len)
{
	struct screen_write_ctx	ctx;

	if (options_get_number(global_options, "set-clipboard") != 0) {
		screen_write_start(&ctx, wp, NULL);
		screen_write_setselection(&ctx, buf, len);
		screen_write_stop(&ctx);
		notify_pane("pane-set-clipboard", wp);
	}

	if (paste_set(buf, len, bufname, NULL) != 0)
		free(buf);
}

static void
window_copy_copy_pipe(struct window_pane *wp, struct session *s,
    const char *bufname, const char *arg)
{
	void		*buf;
	size_t		 len;
	struct job	*job;
	char		*expanded;

	buf = window_copy_get_selection(wp, &len);
	if (buf == NULL)
		return;
	expanded = format_single(NULL, arg, NULL, s, NULL, wp);

	job = job_run(expanded, s, NULL, NULL, NULL, NULL, NULL, JOB_NOWAIT);
	bufferevent_write(job->event, buf, len);

	free(expanded);
	window_copy_copy_buffer(wp, bufname, buf, len);
}

static void
window_copy_copy_selection(struct window_pane *wp, const char *bufname)
{
	void	*buf;
	size_t	 len;

	buf = window_copy_get_selection(wp, &len);
	if (buf == NULL)
		return;

	window_copy_copy_buffer(wp, bufname, buf, len);
}

static void
window_copy_append_selection(struct window_pane *wp, const char *bufname)
{
	char				*buf;
	struct paste_buffer		*pb;
	const char			*bufdata;
	size_t				 len, bufsize;
	struct screen_write_ctx		 ctx;

	buf = window_copy_get_selection(wp, &len);
	if (buf == NULL)
		return;

	if (options_get_number(global_options, "set-clipboard") != 0) {
		screen_write_start(&ctx, wp, NULL);
		screen_write_setselection(&ctx, buf, len);
		screen_write_stop(&ctx);
		notify_pane("pane-set-clipboard", wp);
	}

	if (bufname == NULL || *bufname == '\0')
		pb = paste_get_top(&bufname);
	else
		pb = paste_get_name(bufname);
	if (pb != NULL) {
		bufdata = paste_buffer_data(pb, &bufsize);
		buf = xrealloc(buf, len + bufsize);
		memmove(buf + bufsize, buf, len);
		memcpy(buf, bufdata, bufsize);
		len += bufsize;
	}
	if (paste_set(buf, len, bufname, NULL) != 0)
		free(buf);
}

static void
window_copy_copy_line(struct window_pane *wp, char **buf, size_t *off, u_int sy,
    u_int sx, u_int ex)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct grid			*gd = data->backing->grid;
	struct grid_cell		 gc;
	struct grid_line		*gl;
	struct utf8_data		 ud;
	u_int				 i, xx, wrapped = 0;
	const char			*s;

	if (sx > ex)
		return;

	/*
	 * Work out if the line was wrapped at the screen edge and all of it is
	 * on screen.
	 */
	gl = grid_get_line(gd, sy);
	if (gl->flags & GRID_LINE_WRAPPED && gl->cellsize <= gd->sx)
		wrapped = 1;

	/* If the line was wrapped, don't strip spaces (use the full length). */
	if (wrapped)
		xx = gl->cellsize;
	else
		xx = window_copy_find_length(wp, sy);
	if (ex > xx)
		ex = xx;
	if (sx > xx)
		sx = xx;

	if (sx < ex) {
		for (i = sx; i < ex; i++) {
			grid_get_cell(gd, i, sy, &gc);
			if (gc.flags & GRID_FLAG_PADDING)
				continue;
			utf8_copy(&ud, &gc.data);
			if (ud.size == 1 && (gc.attr & GRID_ATTR_CHARSET)) {
				s = tty_acs_get(NULL, ud.data[0]);
				if (s != NULL && strlen(s) <= sizeof ud.data) {
					ud.size = strlen(s);
					memcpy(ud.data, s, ud.size);
				}
			}

			*buf = xrealloc(*buf, (*off) + ud.size);
			memcpy(*buf + *off, ud.data, ud.size);
			*off += ud.size;
		}
	}

	/* Only add a newline if the line wasn't wrapped. */
	if (!wrapped || ex != xx) {
		*buf = xrealloc(*buf, (*off) + 1);
		(*buf)[(*off)++] = '\n';
	}
}

static void
window_copy_clear_selection(struct window_pane *wp)
{
	struct window_copy_mode_data   *data = wp->modedata;
	u_int				px, py;

	screen_clear_selection(&data->screen);

	data->cursordrag = CURSORDRAG_NONE;

	py = screen_hsize(data->backing) + data->cy - data->oy;
	px = window_copy_find_length(wp, py);
	if (data->cx > px)
		window_copy_update_cursor(wp, px, data->cy);
}

static int
window_copy_in_set(struct window_pane *wp, u_int px, u_int py, const char *set)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct grid_cell		 gc;
	const struct utf8_data		*ud;

	grid_get_cell(data->backing->grid, px, py, &gc);

	ud = &gc.data;
	if (ud->size != 1 || (gc.flags & GRID_FLAG_PADDING))
		return (0);
	if (*ud->data == 0x00 || *ud->data == 0x7f)
		return (0);
	return (strchr(set, *ud->data) != NULL);
}

static u_int
window_copy_find_length(struct window_pane *wp, u_int py)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = data->backing;
	struct grid_cell		 gc;
	u_int				 px;

	/*
	 * If the pane has been resized, its grid can contain old overlong
	 * lines. grid_peek_cell does not allow accessing cells beyond the
	 * width of the grid, and screen_write_copy treats them as spaces, so
	 * ignore them here too.
	 */
	px = grid_get_line(s->grid, py)->cellsize;
	if (px > screen_size_x(s))
		px = screen_size_x(s);
	while (px > 0) {
		grid_get_cell(s->grid, px - 1, py, &gc);
		if (gc.data.size != 1 || *gc.data.data != ' ')
			break;
		px--;
	}
	return (px);
}

static void
window_copy_cursor_start_of_line(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*back_s = data->backing;
	struct grid			*gd = back_s->grid;
	u_int				 py;

	if (data->cx == 0 && data->lineflag == LINE_SEL_NONE) {
		py = screen_hsize(back_s) + data->cy - data->oy;
		while (py > 0 &&
		    grid_get_line(gd, py - 1)->flags & GRID_LINE_WRAPPED) {
			window_copy_cursor_up(wp, 0);
			py = screen_hsize(back_s) + data->cy - data->oy;
		}
	}
	window_copy_update_cursor(wp, 0, data->cy);
	if (window_copy_update_selection(wp, 1))
		window_copy_redraw_lines(wp, data->cy, 1);
}

static void
window_copy_cursor_back_to_indentation(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 px, py, xx;
	struct grid_cell		 gc;

	px = 0;
	py = screen_hsize(data->backing) + data->cy - data->oy;
	xx = window_copy_find_length(wp, py);

	while (px < xx) {
		grid_get_cell(data->backing->grid, px, py, &gc);
		if (gc.data.size != 1 || *gc.data.data != ' ')
			break;
		px++;
	}

	window_copy_update_cursor(wp, px, data->cy);
	if (window_copy_update_selection(wp, 1))
		window_copy_redraw_lines(wp, data->cy, 1);
}

static void
window_copy_cursor_end_of_line(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*back_s = data->backing;
	struct grid			*gd = back_s->grid;
	struct grid_line		*gl;
	u_int				 px, py;

	py = screen_hsize(back_s) + data->cy - data->oy;
	px = window_copy_find_length(wp, py);

	if (data->cx == px && data->lineflag == LINE_SEL_NONE) {
		if (data->screen.sel != NULL && data->rectflag)
			px = screen_size_x(back_s);
		gl = grid_get_line(gd, py);
		if (gl->flags & GRID_LINE_WRAPPED) {
			while (py < gd->sy + gd->hsize) {
				gl = grid_get_line(gd, py);
				if (~gl->flags & GRID_LINE_WRAPPED)
					break;
				window_copy_cursor_down(wp, 0);
				py = screen_hsize(back_s) + data->cy - data->oy;
			}
			px = window_copy_find_length(wp, py);
		}
	}
	window_copy_update_cursor(wp, px, data->cy);

	if (window_copy_update_selection(wp, 1))
		window_copy_redraw_lines(wp, data->cy, 1);
}

static void
window_copy_other_end(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 selx, sely, cy, yy, hsize;

	if (s->sel == NULL && data->lineflag == LINE_SEL_NONE)
		return;

	if (data->lineflag == LINE_SEL_LEFT_RIGHT)
		data->lineflag = LINE_SEL_RIGHT_LEFT;
	else if (data->lineflag == LINE_SEL_RIGHT_LEFT)
		data->lineflag = LINE_SEL_LEFT_RIGHT;

	switch (data->cursordrag) {
		case CURSORDRAG_NONE:
		case CURSORDRAG_SEL:
			data->cursordrag = CURSORDRAG_ENDSEL;
			break;
		case CURSORDRAG_ENDSEL:
			data->cursordrag = CURSORDRAG_SEL;
			break;
	}

	selx = data->endselx;
	sely = data->endsely;
	if (data->cursordrag == CURSORDRAG_SEL) {
		selx = data->selx;
		sely = data->sely;
	}

	cy = data->cy;
	yy = screen_hsize(data->backing) + data->cy - data->oy;

	data->cx = selx;

	hsize = screen_hsize(data->backing);
	if (sely < hsize - data->oy) { /* above */
		data->oy = hsize - sely;
		data->cy = 0;
	} else if (sely > hsize - data->oy + screen_size_y(s)) { /* below */
		data->oy = hsize - sely + screen_size_y(s) - 1;
		data->cy = screen_size_y(s) - 1;
	} else
		data->cy = cy + sely - yy;

	window_copy_update_selection(wp, 1);
	window_copy_redraw_screen(wp);
}

static void
window_copy_cursor_left(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 py, cx;
	struct grid_cell		 gc;

	py = screen_hsize(data->backing) + data->cy - data->oy;
	cx = data->cx;
	while (cx > 0) {
		grid_get_cell(data->backing->grid, cx, py, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
		cx--;
	}
	if (cx == 0 && py > 0) {
		window_copy_cursor_up(wp, 0);
		window_copy_cursor_end_of_line(wp);
	} else if (cx > 0) {
		window_copy_update_cursor(wp, cx - 1, data->cy);
		if (window_copy_update_selection(wp, 1))
			window_copy_redraw_lines(wp, data->cy, 1);
	}
}

static void
window_copy_cursor_right(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 px, py, yy, cx, cy;
	struct grid_cell		 gc;

	py = screen_hsize(data->backing) + data->cy - data->oy;
	yy = screen_hsize(data->backing) + screen_size_y(data->backing) - 1;
	if (data->screen.sel != NULL && data->rectflag)
		px = screen_size_x(&data->screen);
	else
		px = window_copy_find_length(wp, py);

	if (data->cx >= px && py < yy) {
		window_copy_cursor_start_of_line(wp);
		window_copy_cursor_down(wp, 0);
	} else if (data->cx < px) {
		cx = data->cx + 1;
		cy = screen_hsize(data->backing) + data->cy - data->oy;
		while (cx < px) {
			grid_get_cell(data->backing->grid, cx, cy, &gc);
			if (~gc.flags & GRID_FLAG_PADDING)
				break;
			cx++;
		}
		window_copy_update_cursor(wp, cx, data->cy);
		if (window_copy_update_selection(wp, 1))
			window_copy_redraw_lines(wp, data->cy, 1);
	}
}

static void
window_copy_cursor_up(struct window_pane *wp, int scroll_only)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 ox, oy, px, py;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wp, oy);
	if (data->cx != ox) {
		data->lastcx = data->cx;
		data->lastsx = ox;
	}

	if (data->lineflag == LINE_SEL_LEFT_RIGHT && oy == data->sely)
		window_copy_other_end(wp);

	data->cx = data->lastcx;
	if (scroll_only || data->cy == 0) {
		window_copy_scroll_down(wp, 1);
		if (scroll_only) {
			if (data->cy == screen_size_y(s) - 1)
				window_copy_redraw_lines(wp, data->cy, 1);
			else
				window_copy_redraw_lines(wp, data->cy, 2);
		}
	} else {
		window_copy_update_cursor(wp, data->cx, data->cy - 1);
		if (window_copy_update_selection(wp, 1)) {
			if (data->cy == screen_size_y(s) - 1)
				window_copy_redraw_lines(wp, data->cy, 1);
			else
				window_copy_redraw_lines(wp, data->cy, 2);
		}
	}

	if (data->screen.sel != NULL || !data->rectflag) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wp, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
			window_copy_cursor_end_of_line(wp);
	}

	if (data->lineflag == LINE_SEL_LEFT_RIGHT)
		window_copy_cursor_end_of_line(wp);
	else if (data->lineflag == LINE_SEL_RIGHT_LEFT)
		window_copy_cursor_start_of_line(wp);
}

static void
window_copy_cursor_down(struct window_pane *wp, int scroll_only)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 ox, oy, px, py;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wp, oy);
	if (data->cx != ox) {
		data->lastcx = data->cx;
		data->lastsx = ox;
	}

	if (data->lineflag == LINE_SEL_RIGHT_LEFT && oy == data->endsely)
		window_copy_other_end(wp);

	data->cx = data->lastcx;
	if (scroll_only || data->cy == screen_size_y(s) - 1) {
		window_copy_scroll_up(wp, 1);
		if (scroll_only && data->cy > 0)
			window_copy_redraw_lines(wp, data->cy - 1, 2);
	} else {
		window_copy_update_cursor(wp, data->cx, data->cy + 1);
		if (window_copy_update_selection(wp, 1))
			window_copy_redraw_lines(wp, data->cy - 1, 2);
	}

	if (data->screen.sel == NULL || !data->rectflag) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wp, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
			window_copy_cursor_end_of_line(wp);
	}

	if (data->lineflag == LINE_SEL_LEFT_RIGHT)
		window_copy_cursor_end_of_line(wp);
	else if (data->lineflag == LINE_SEL_RIGHT_LEFT)
		window_copy_cursor_start_of_line(wp);
}

static void
window_copy_cursor_jump(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*back_s = data->backing;
	struct grid_cell		 gc;
	u_int				 px, py, xx;

	px = data->cx + 1;
	py = screen_hsize(back_s) + data->cy - data->oy;
	xx = window_copy_find_length(wp, py);

	while (px < xx) {
		grid_get_cell(back_s->grid, px, py, &gc);
		if (!(gc.flags & GRID_FLAG_PADDING) &&
		    gc.data.size == 1 && *gc.data.data == data->jumpchar) {
			window_copy_update_cursor(wp, px, data->cy);
			if (window_copy_update_selection(wp, 1))
				window_copy_redraw_lines(wp, data->cy, 1);
			return;
		}
		px++;
	}
}

static void
window_copy_cursor_jump_back(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*back_s = data->backing;
	struct grid_cell		 gc;
	u_int				 px, py;

	px = data->cx;
	py = screen_hsize(back_s) + data->cy - data->oy;

	if (px > 0)
		px--;

	for (;;) {
		grid_get_cell(back_s->grid, px, py, &gc);
		if (!(gc.flags & GRID_FLAG_PADDING) &&
		    gc.data.size == 1 && *gc.data.data == data->jumpchar) {
			window_copy_update_cursor(wp, px, data->cy);
			if (window_copy_update_selection(wp, 1))
				window_copy_redraw_lines(wp, data->cy, 1);
			return;
		}
		if (px == 0)
			break;
		px--;
	}
}

static void
window_copy_cursor_jump_to(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*back_s = data->backing;
	struct grid_cell		 gc;
	u_int				 px, py, xx;

	px = data->cx + 2;
	py = screen_hsize(back_s) + data->cy - data->oy;
	xx = window_copy_find_length(wp, py);

	while (px < xx) {
		grid_get_cell(back_s->grid, px, py, &gc);
		if (!(gc.flags & GRID_FLAG_PADDING) &&
		    gc.data.size == 1 && *gc.data.data == data->jumpchar) {
			window_copy_update_cursor(wp, px - 1, data->cy);
			if (window_copy_update_selection(wp, 1))
				window_copy_redraw_lines(wp, data->cy, 1);
			return;
		}
		px++;
	}
}

static void
window_copy_cursor_jump_to_back(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*back_s = data->backing;
	struct grid_cell		 gc;
	u_int				 px, py;

	px = data->cx;
	py = screen_hsize(back_s) + data->cy - data->oy;

	if (px > 0)
		px--;

	if (px > 0)
		px--;

	for (;;) {
		grid_get_cell(back_s->grid, px, py, &gc);
		if (!(gc.flags & GRID_FLAG_PADDING) &&
		    gc.data.size == 1 && *gc.data.data == data->jumpchar) {
			window_copy_update_cursor(wp, px + 1, data->cy);
			if (window_copy_update_selection(wp, 1))
				window_copy_redraw_lines(wp, data->cy, 1);
			return;
		}
		if (px == 0)
			break;
		px--;
	}
}

static void
window_copy_cursor_next_word(struct window_pane *wp, const char *separators)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*back_s = data->backing;
	u_int				 px, py, xx, yy;
	int				 expected = 0;

	px = data->cx;
	py = screen_hsize(back_s) + data->cy - data->oy;
	xx = window_copy_find_length(wp, py);
	yy = screen_hsize(back_s) + screen_size_y(back_s) - 1;

	/*
	 * First skip past any nonword characters and then any word characters.
	 *
	 * expected is initially set to 0 for the former and then 1 for the
	 * latter.
	 */
	do {
		while (px > xx ||
		    window_copy_in_set(wp, px, py, separators) == expected) {
			/* Move down if we're past the end of the line. */
			if (px > xx) {
				if (py == yy)
					return;
				window_copy_cursor_down(wp, 0);
				px = 0;

				py = screen_hsize(back_s) + data->cy - data->oy;
				xx = window_copy_find_length(wp, py);
			} else
				px++;
		}
		expected = !expected;
	} while (expected == 1);

	window_copy_update_cursor(wp, px, data->cy);
	if (window_copy_update_selection(wp, 1))
		window_copy_redraw_lines(wp, data->cy, 1);
}

static void
window_copy_cursor_next_word_end(struct window_pane *wp,
    const char *separators)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct options			*oo = wp->window->options;
	struct screen			*back_s = data->backing;
	u_int				 px, py, xx, yy;
	int				 keys, expected = 1;

	px = data->cx;
	py = screen_hsize(back_s) + data->cy - data->oy;
	xx = window_copy_find_length(wp, py);
	yy = screen_hsize(back_s) + screen_size_y(back_s) - 1;

	keys = options_get_number(oo, "mode-keys");
	if (keys == MODEKEY_VI && !window_copy_in_set(wp, px, py, separators))
		px++;

	/*
	 * First skip past any word characters, then any nonword characters.
	 *
	 * expected is initially set to 1 for the former and then 0 for the
	 * latter.
	 */
	do {
		while (px > xx ||
		    window_copy_in_set(wp, px, py, separators) == expected) {
			/* Move down if we're past the end of the line. */
			if (px > xx) {
				if (py == yy)
					return;
				window_copy_cursor_down(wp, 0);
				px = 0;

				py = screen_hsize(back_s) + data->cy - data->oy;
				xx = window_copy_find_length(wp, py);
			} else
				px++;
		}
		expected = !expected;
	} while (expected == 0);

	if (keys == MODEKEY_VI && px != 0)
		px--;

	window_copy_update_cursor(wp, px, data->cy);
	if (window_copy_update_selection(wp, 1))
		window_copy_redraw_lines(wp, data->cy, 1);
}

/* Move to the previous place where a word begins. */
static void
window_copy_cursor_previous_word(struct window_pane *wp,
    const char *separators)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 px, py;

	px = data->cx;
	py = screen_hsize(data->backing) + data->cy - data->oy;

	/* Move back to the previous word character. */
	for (;;) {
		if (px > 0) {
			px--;
			if (!window_copy_in_set(wp, px, py, separators))
				break;
		} else {
			if (data->cy == 0 &&
			    (screen_hsize(data->backing) == 0 ||
			    data->oy >= screen_hsize(data->backing) - 1))
				goto out;
			window_copy_cursor_up(wp, 0);

			py = screen_hsize(data->backing) + data->cy - data->oy;
			px = window_copy_find_length(wp, py);
		}
	}

	/* Move back to the beginning of this word. */
	while (px > 0 && !window_copy_in_set(wp, px - 1, py, separators))
		px--;

out:
	window_copy_update_cursor(wp, px, data->cy);
	if (window_copy_update_selection(wp, 1))
		window_copy_redraw_lines(wp, data->cy, 1);
}

static void
window_copy_scroll_up(struct window_pane *wp, u_int ny)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->oy < ny)
		ny = data->oy;
	if (ny == 0)
		return;
	data->oy -= ny;

	window_copy_update_selection(wp, 0);

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_deleteline(&ctx, ny, 8);
	window_copy_write_lines(wp, &ctx, screen_size_y(s) - ny, ny);
	window_copy_write_line(wp, &ctx, 0);
	if (screen_size_y(s) > 1)
		window_copy_write_line(wp, &ctx, 1);
	if (screen_size_y(s) > 3)
		window_copy_write_line(wp, &ctx, screen_size_y(s) - 2);
	if (s->sel != NULL && screen_size_y(s) > ny)
		window_copy_write_line(wp, &ctx, screen_size_y(s) - ny - 1);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

static void
window_copy_scroll_down(struct window_pane *wp, u_int ny)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (ny > screen_hsize(data->backing))
		return;

	if (data->oy > screen_hsize(data->backing) - ny)
		ny = screen_hsize(data->backing) - data->oy;
	if (ny == 0)
		return;
	data->oy += ny;

	window_copy_update_selection(wp, 0);

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_insertline(&ctx, ny, 8);
	window_copy_write_lines(wp, &ctx, 0, ny);
	if (s->sel != NULL && screen_size_y(s) > ny)
		window_copy_write_line(wp, &ctx, ny);
	else if (ny == 1) /* nuke position */
		window_copy_write_line(wp, &ctx, 1);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_add_formats(struct window_pane *wp, struct format_tree *ft)
{
	struct window_copy_mode_data	*data = wp->modedata;

	if (wp->mode != &window_copy_mode)
		return;

	format_add(ft, "selection_present", "%d", data->screen.sel != NULL);
	format_add(ft, "scroll_position", "%d", data->oy);
	format_add(ft, "rectangle_toggle", "%d", data->rectflag);
}

static void
window_copy_rectangle_toggle(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 px, py;

	data->rectflag = !data->rectflag;

	py = screen_hsize(data->backing) + data->cy - data->oy;
	px = window_copy_find_length(wp, py);
	if (data->cx > px)
		window_copy_update_cursor(wp, px, data->cy);

	window_copy_update_selection(wp, 1);
	window_copy_redraw_screen(wp);
}

static void
window_copy_move_mouse(struct mouse_event *m)
{
	struct window_pane	*wp;
	u_int			 x, y;

	wp = cmd_mouse_pane(m, NULL, NULL);
	if (wp == NULL || wp->mode != &window_copy_mode)
		return;

	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return;

	window_copy_update_cursor(wp, x, y);
}

void
window_copy_start_drag(struct client *c, struct mouse_event *m)
{
	struct window_pane	*wp;
	u_int			 x, y;

	if (c == NULL)
		return;

	wp = cmd_mouse_pane(m, NULL, NULL);
	if (wp == NULL || wp->mode != &window_copy_mode)
		return;

	if (cmd_mouse_at(wp, m, &x, &y, 1) != 0)
		return;

	c->tty.mouse_drag_update = window_copy_drag_update;
	c->tty.mouse_drag_release = NULL; /* will fire MouseDragEnd key */

	window_copy_update_cursor(wp, x, y);
	window_copy_start_selection(wp);
	window_copy_redraw_screen(wp);
}

static void
window_copy_drag_update(__unused struct client *c, struct mouse_event *m)
{
	struct window_pane		*wp;
	struct window_copy_mode_data	*data;
	u_int				 x, y, old_cy;

	wp = cmd_mouse_pane(m, NULL, NULL);
	if (wp == NULL || wp->mode != &window_copy_mode)
		return;
	data = wp->modedata;

	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return;
	old_cy = data->cy;

	window_copy_update_cursor(wp, x, y);
	if (window_copy_update_selection(wp, 1))
		window_copy_redraw_selection(wp, old_cy);
}
