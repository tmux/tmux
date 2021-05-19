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
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

struct window_copy_mode_data;

static const char *window_copy_key_table(struct window_mode_entry *);
static void	window_copy_command(struct window_mode_entry *, struct client *,
		    struct session *, struct winlink *, struct args *,
		    struct mouse_event *);
static struct screen *window_copy_init(struct window_mode_entry *,
		    struct cmd_find_state *, struct args *);
static struct screen *window_copy_view_init(struct window_mode_entry *,
		    struct cmd_find_state *, struct args *);
static void	window_copy_free(struct window_mode_entry *);
static void	window_copy_resize(struct window_mode_entry *, u_int, u_int);
static void	window_copy_formats(struct window_mode_entry *,
		    struct format_tree *);
static void	window_copy_pageup1(struct window_mode_entry *, int);
static int	window_copy_pagedown(struct window_mode_entry *, int, int);
static void	window_copy_next_paragraph(struct window_mode_entry *);
static void	window_copy_previous_paragraph(struct window_mode_entry *);
static void	window_copy_redraw_selection(struct window_mode_entry *, u_int);
static void	window_copy_redraw_lines(struct window_mode_entry *, u_int,
		    u_int);
static void	window_copy_redraw_screen(struct window_mode_entry *);
static void	window_copy_write_line(struct window_mode_entry *,
		    struct screen_write_ctx *, u_int);
static void	window_copy_write_lines(struct window_mode_entry *,
		    struct screen_write_ctx *, u_int, u_int);
static char    *window_copy_match_at_cursor(struct window_copy_mode_data *);
static void	window_copy_scroll_to(struct window_mode_entry *, u_int, u_int,
		    int);
static int	window_copy_search_compare(struct grid *, u_int, u_int,
		    struct grid *, u_int, int);
static int	window_copy_search_lr(struct grid *, struct grid *, u_int *,
		    u_int, u_int, u_int, int);
static int	window_copy_search_rl(struct grid *, struct grid *, u_int *,
		    u_int, u_int, u_int, int);
static int	window_copy_last_regex(struct grid *, u_int, u_int, u_int,
		    u_int, u_int *, u_int *, const char *, const regex_t *,
		    int);
static int	window_copy_search_mark_at(struct window_copy_mode_data *,
		    u_int, u_int, u_int *);
static char    *window_copy_stringify(struct grid *, u_int, u_int, u_int,
		    char *, u_int *);
static void	window_copy_cstrtocellpos(struct grid *, u_int, u_int *,
		    u_int *, const char *);
static int	window_copy_search_marks(struct window_mode_entry *,
		    struct screen *, int, int);
static void	window_copy_clear_marks(struct window_mode_entry *);
static int	window_copy_is_lowercase(const char *);
static void	window_copy_search_back_overlap(struct grid *, regex_t *,
		    u_int *, u_int *, u_int *, u_int);
static int	window_copy_search_jump(struct window_mode_entry *,
		    struct grid *, struct grid *, u_int, u_int, u_int, int, int,
		    int, int);
static int	window_copy_search(struct window_mode_entry *, int, int);
static int	window_copy_search_up(struct window_mode_entry *, int);
static int	window_copy_search_down(struct window_mode_entry *, int);
static void	window_copy_goto_line(struct window_mode_entry *, const char *);
static void	window_copy_update_cursor(struct window_mode_entry *, u_int,
		    u_int);
static void	window_copy_start_selection(struct window_mode_entry *);
static int	window_copy_adjust_selection(struct window_mode_entry *,
		    u_int *, u_int *);
static int	window_copy_set_selection(struct window_mode_entry *, int, int);
static int	window_copy_update_selection(struct window_mode_entry *, int,
		    int);
static void	window_copy_synchronize_cursor(struct window_mode_entry *, int);
static void    *window_copy_get_selection(struct window_mode_entry *, size_t *);
static void	window_copy_copy_buffer(struct window_mode_entry *,
		    const char *, void *, size_t);
static void	window_copy_pipe(struct window_mode_entry *,
		    struct session *, const char *);
static void	window_copy_copy_pipe(struct window_mode_entry *,
		    struct session *, const char *, const char *);
static void	window_copy_copy_selection(struct window_mode_entry *,
		    const char *);
static void	window_copy_append_selection(struct window_mode_entry *);
static void	window_copy_clear_selection(struct window_mode_entry *);
static void	window_copy_copy_line(struct window_mode_entry *, char **,
		    size_t *, u_int, u_int, u_int);
static int	window_copy_in_set(struct window_mode_entry *, u_int, u_int,
		    const char *);
static u_int	window_copy_find_length(struct window_mode_entry *, u_int);
static void	window_copy_cursor_start_of_line(struct window_mode_entry *);
static void	window_copy_cursor_back_to_indentation(
		    struct window_mode_entry *);
static void	window_copy_cursor_end_of_line(struct window_mode_entry *);
static void	window_copy_other_end(struct window_mode_entry *);
static void	window_copy_cursor_left(struct window_mode_entry *);
static void	window_copy_cursor_right(struct window_mode_entry *, int);
static void	window_copy_cursor_up(struct window_mode_entry *, int);
static void	window_copy_cursor_down(struct window_mode_entry *, int);
static void	window_copy_cursor_jump(struct window_mode_entry *);
static void	window_copy_cursor_jump_back(struct window_mode_entry *);
static void	window_copy_cursor_jump_to(struct window_mode_entry *);
static void	window_copy_cursor_jump_to_back(struct window_mode_entry *);
static void	window_copy_cursor_next_word(struct window_mode_entry *,
		    const char *);
static void	window_copy_cursor_next_word_end_pos(struct window_mode_entry *,
		    const char *, u_int *, u_int *);
static void	window_copy_cursor_next_word_end(struct window_mode_entry *,
		    const char *, int);
static void	window_copy_cursor_previous_word_pos(struct window_mode_entry *,
		    const char *, int, u_int *, u_int *);
static void	window_copy_cursor_previous_word(struct window_mode_entry *,
		    const char *, int);
static void	window_copy_scroll_up(struct window_mode_entry *, u_int);
static void	window_copy_scroll_down(struct window_mode_entry *, u_int);
static void	window_copy_rectangle_set(struct window_mode_entry *, int);
static void	window_copy_move_mouse(struct mouse_event *);
static void	window_copy_drag_update(struct client *, struct mouse_event *);
static void	window_copy_drag_release(struct client *, struct mouse_event *);
static void	window_copy_jump_to_mark(struct window_mode_entry *);
static void	window_copy_acquire_cursor_up(struct window_mode_entry *,
		    u_int, u_int, u_int, u_int, u_int);
static void	window_copy_acquire_cursor_down(struct window_mode_entry *,
		    u_int, u_int, u_int, u_int, u_int, u_int, int);

const struct window_mode window_copy_mode = {
	.name = "copy-mode",

	.init = window_copy_init,
	.free = window_copy_free,
	.resize = window_copy_resize,
	.key_table = window_copy_key_table,
	.command = window_copy_command,
	.formats = window_copy_formats,
};

const struct window_mode window_view_mode = {
	.name = "view-mode",

	.init = window_copy_view_init,
	.free = window_copy_free,
	.resize = window_copy_resize,
	.key_table = window_copy_key_table,
	.command = window_copy_command,
	.formats = window_copy_formats,
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

enum window_copy_cmd_action {
	WINDOW_COPY_CMD_NOTHING,
	WINDOW_COPY_CMD_REDRAW,
	WINDOW_COPY_CMD_CANCEL,
};

enum window_copy_cmd_clear {
	WINDOW_COPY_CMD_CLEAR_ALWAYS,
	WINDOW_COPY_CMD_CLEAR_NEVER,
	WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
};

struct window_copy_cmd_state {
	struct window_mode_entry	*wme;
	struct args			*args;
	struct mouse_event		*m;

	struct client			*c;
	struct session			*s;
	struct winlink			*wl;
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

	int		 viewmode;	/* view mode entered */

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
	int		 hide_position;	/* hide position marker */

	enum {
		SEL_CHAR,		/* select one char at a time */
		SEL_WORD,		/* select one word at a time */
		SEL_LINE,		/* select one line at a time */
	} selflag;

	const char	*ws;		/* word separators */

	u_int		 dx;		/* drag start position */
	u_int		 dy;

	u_int		 selrx;		/* selection reset positions */
	u_int		 selry;
	u_int		 endselrx;
	u_int		 endselry;

	u_int		 cx;
	u_int		 cy;

	u_int		 lastcx; 	/* position in last line w/ content */
	u_int		 lastsx;	/* size of last line w/ content */

	u_int		 mx;		/* mark position */
	u_int		 my;
	int		 showmark;

	int		 searchtype;
	int		 searchdirection;
	int		 searchregex;
	char		*searchstr;
	u_char		*searchmark;
	int		 searchcount;
	int		 searchmore;
	int		 searchall;
	int		 searchx;
	int		 searchy;
	int		 searcho;
	u_char		 searchgen;

	int		 timeout;	/* search has timed out */
#define WINDOW_COPY_SEARCH_TIMEOUT 10000
#define WINDOW_COPY_SEARCH_ALL_TIMEOUT 200

	int			 jumptype;
	struct utf8_data	*jumpchar;

	struct event	 dragtimer;
#define WINDOW_COPY_DRAG_REPEAT_TIME 50000
};

static void
window_copy_scroll_timer(__unused int fd, __unused short events, void *arg)
{
	struct window_mode_entry	*wme = arg;
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct timeval			 tv = {
		.tv_usec = WINDOW_COPY_DRAG_REPEAT_TIME
	};

	evtimer_del(&data->dragtimer);

	if (TAILQ_FIRST(&wp->modes) != wme)
		return;

	if (data->cy == 0) {
		evtimer_add(&data->dragtimer, &tv);
		window_copy_cursor_up(wme, 1);
	} else if (data->cy == screen_size_y(&data->screen) - 1) {
		evtimer_add(&data->dragtimer, &tv);
		window_copy_cursor_down(wme, 1);
	}
}

static struct screen *
window_copy_clone_screen(struct screen *src, struct screen *hint, u_int *cx,
    u_int *cy, int trim)
{
	struct screen		*dst;
	const struct grid_line	*gl;
	u_int			 sy, wx, wy;
	int			 reflow;

	dst = xcalloc(1, sizeof *dst);

	sy = screen_hsize(src) + screen_size_y(src);
	if (trim) {
		while (sy > screen_hsize(src)) {
			gl = grid_peek_line(src->grid, sy - 1);
			if (gl->cellused != 0)
				break;
			sy--;
		}
	}
	log_debug("%s: target screen is %ux%u, source %ux%u", __func__,
	    screen_size_x(src), sy, screen_size_x(hint),
	    screen_hsize(src) + screen_size_y(src));
	screen_init(dst, screen_size_x(src), sy, screen_hlimit(src));

	/*
	 * Ensure history is on for the backing grid so lines are not deleted
	 * during resizing.
	 */
	dst->grid->flags |= GRID_HISTORY;
	grid_duplicate_lines(dst->grid, 0, src->grid, 0, sy);

	dst->grid->sy = sy - screen_hsize(src);
	dst->grid->hsize = screen_hsize(src);
	dst->grid->hscrolled = src->grid->hscrolled;
	if (src->cy > dst->grid->sy - 1) {
		dst->cx = 0;
		dst->cy = dst->grid->sy - 1;
	} else {
		dst->cx = src->cx;
		dst->cy = src->cy;
	}

	if (cx != NULL && cy != NULL) {
		*cx = dst->cx;
		*cy = screen_hsize(dst) + dst->cy;
		reflow = (screen_size_x(hint) != screen_size_x(dst));
	}
	else
		reflow = 0;
	if (reflow)
		grid_wrap_position(dst->grid, *cx, *cy, &wx, &wy);
	screen_resize_cursor(dst, screen_size_x(hint), screen_size_y(hint), 1,
	    0, 0);
	if (reflow)
		grid_unwrap_position(dst->grid, cx, cy, wx, wy);

	return (dst);
}

static struct window_copy_mode_data *
window_copy_common_init(struct window_mode_entry *wme)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data;
	struct screen			*base = &wp->base;

	wme->data = data = xcalloc(1, sizeof *data);

	data->cursordrag = CURSORDRAG_NONE;
	data->lineflag = LINE_SEL_NONE;
	data->selflag = SEL_CHAR;

	if (wp->searchstr != NULL) {
		data->searchtype = WINDOW_COPY_SEARCHUP;
		data->searchregex = wp->searchregex;
		data->searchstr = xstrdup(wp->searchstr);
	} else {
		data->searchtype = WINDOW_COPY_OFF;
		data->searchregex = 0;
		data->searchstr = NULL;
	}
	data->searchx = data->searchy = data->searcho = -1;
	data->searchall = 1;

	data->jumptype = WINDOW_COPY_OFF;
	data->jumpchar = NULL;

	screen_init(&data->screen, screen_size_x(base), screen_size_y(base), 0);
	data->modekeys = options_get_number(wp->window->options, "mode-keys");

	evtimer_set(&data->dragtimer, window_copy_scroll_timer, wme);

	return (data);
}

static struct screen *
window_copy_init(struct window_mode_entry *wme,
    __unused struct cmd_find_state *fs, struct args *args)
{
	struct window_pane		*wp = wme->swp;
	struct window_copy_mode_data	*data;
	struct screen			*base = &wp->base;
	struct screen_write_ctx		 ctx;
	u_int				 i, cx, cy;

	data = window_copy_common_init(wme);
	data->backing = window_copy_clone_screen(base, &data->screen, &cx, &cy,
	    wme->swp != wme->wp);

	data->cx = cx;
	if (cy < screen_hsize(data->backing)) {
		data->cy = 0;
		data->oy = screen_hsize(data->backing) - cy;
	} else {
		data->cy = cy - screen_hsize(data->backing);
		data->oy = 0;
	}

	data->scroll_exit = args_has(args, 'e');
	data->hide_position = args_has(args, 'H');

	data->screen.cx = data->cx;
	data->screen.cy = data->cy;
	data->mx = data->cx;
	data->my = screen_hsize(data->backing) + data->cy - data->oy;
	data->showmark = 0;

	screen_write_start(&ctx, &data->screen);
	for (i = 0; i < screen_size_y(&data->screen); i++)
		window_copy_write_line(wme, &ctx, i);
	screen_write_cursormove(&ctx, data->cx, data->cy, 0);
	screen_write_stop(&ctx);

	return (&data->screen);
}

static struct screen *
window_copy_view_init(struct window_mode_entry *wme,
    __unused struct cmd_find_state *fs, __unused struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data;
	struct screen			*base = &wp->base;
	struct screen			*s;

	data = window_copy_common_init(wme);
	data->viewmode = 1;

	data->backing = s = xmalloc(sizeof *data->backing);
	screen_init(s, screen_size_x(base), screen_size_y(base), UINT_MAX);
	data->mx = data->cx;
	data->my = screen_hsize(data->backing) + data->cy - data->oy;
	data->showmark = 0;

	return (&data->screen);
}

static void
window_copy_free(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;

	evtimer_del(&data->dragtimer);

	free(data->searchmark);
	free(data->searchstr);
	free(data->jumpchar);

	screen_free(data->backing);
	free(data->backing);

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
	struct window_mode_entry	*wme = TAILQ_FIRST(&wp->modes);
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*backing = data->backing;
	struct screen_write_ctx	 	 back_ctx, ctx;
	struct grid_cell		 gc;
	u_int				 old_hsize, old_cy;

	memcpy(&gc, &grid_default_cell, sizeof gc);

	old_hsize = screen_hsize(data->backing);
	screen_write_start(&back_ctx, backing);
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

	screen_write_start_pane(&ctx, wp, &data->screen);

	/*
	 * If the history has changed, draw the top line.
	 * (If there's any history at all, it has changed.)
	 */
	if (screen_hsize(data->backing))
		window_copy_redraw_lines(wme, 0, 1);

	/* Write the new lines. */
	window_copy_redraw_lines(wme, old_cy, backing->cy - old_cy + 1);

	screen_write_stop(&ctx);
}

void
window_copy_pageup(struct window_pane *wp, int half_page)
{
	window_copy_pageup1(TAILQ_FIRST(&wp->modes), half_page);
}

static void
window_copy_pageup1(struct window_mode_entry *wme, int half_page)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	u_int				 n, ox, oy, px, py;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wme, oy);

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

	if (data->oy + n > screen_hsize(data->backing)) {
		data->oy = screen_hsize(data->backing);
		if (data->cy < n)
			data->cy = 0;
		else
			data->cy -= n;
	} else
		data->oy += n;

	if (data->screen.sel == NULL || !data->rectflag) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wme, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
			window_copy_cursor_end_of_line(wme);
	}

	if (data->searchmark != NULL && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 1);
	window_copy_update_selection(wme, 1, 0);
	window_copy_redraw_screen(wme);
}

static int
window_copy_pagedown(struct window_mode_entry *wme, int half_page,
    int scroll_exit)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	u_int				 n, ox, oy, px, py;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wme, oy);

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

	if (data->oy < n) {
		data->oy = 0;
		if (data->cy + (n - data->oy) >= screen_size_y(data->backing))
			data->cy = screen_size_y(data->backing) - 1;
		else
			data->cy += n - data->oy;
	} else
		data->oy -= n;

	if (data->screen.sel == NULL || !data->rectflag) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wme, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
			window_copy_cursor_end_of_line(wme);
	}

	if (scroll_exit && data->oy == 0)
		return (1);
	if (data->searchmark != NULL && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 1);
	window_copy_update_selection(wme, 1, 0);
	window_copy_redraw_screen(wme);
	return (0);
}

static void
window_copy_previous_paragraph(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	u_int				 oy;

	oy = screen_hsize(data->backing) + data->cy - data->oy;

	while (oy > 0 && window_copy_find_length(wme, oy) == 0)
		oy--;

	while (oy > 0 && window_copy_find_length(wme, oy) > 0)
		oy--;

	window_copy_scroll_to(wme, 0, oy, 0);
}

static void
window_copy_next_paragraph(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	u_int				 maxy, ox, oy;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	maxy = screen_hsize(data->backing) + screen_size_y(s) - 1;

	while (oy < maxy && window_copy_find_length(wme, oy) == 0)
		oy++;

	while (oy < maxy && window_copy_find_length(wme, oy) > 0)
		oy++;

	ox = window_copy_find_length(wme, oy);
	window_copy_scroll_to(wme, ox, oy, 0);
}

char *
window_copy_get_word(struct window_pane *wp, u_int x, u_int y)
{
	struct window_mode_entry	*wme = TAILQ_FIRST(&wp->modes);
	struct window_copy_mode_data	*data = wme->data;
	struct grid			*gd = data->screen.grid;

	return (format_grid_word(gd, x, gd->hsize + y));
}

char *
window_copy_get_line(struct window_pane *wp, u_int y)
{
	struct window_mode_entry	*wme = TAILQ_FIRST(&wp->modes);
	struct window_copy_mode_data	*data = wme->data;
	struct grid			*gd = data->screen.grid;

	return (format_grid_line(gd, gd->hsize + y));
}

static void *
window_copy_cursor_word_cb(struct format_tree *ft)
{
	struct window_pane		*wp = format_get_pane(ft);
	struct window_mode_entry	*wme = TAILQ_FIRST(&wp->modes);
	struct window_copy_mode_data	*data = wme->data;

	return (window_copy_get_word(wp, data->cx, data->cy));
}

static void *
window_copy_cursor_line_cb(struct format_tree *ft)
{
	struct window_pane		*wp = format_get_pane(ft);
	struct window_mode_entry	*wme = TAILQ_FIRST(&wp->modes);
	struct window_copy_mode_data	*data = wme->data;

	return (window_copy_get_line(wp, data->cy));
}

static void *
window_copy_search_match_cb(struct format_tree *ft)
{
	struct window_pane		*wp = format_get_pane(ft);
	struct window_mode_entry	*wme = TAILQ_FIRST(&wp->modes);
	struct window_copy_mode_data	*data = wme->data;

	return (window_copy_match_at_cursor(data));
}

static void
window_copy_formats(struct window_mode_entry *wme, struct format_tree *ft)
{
	struct window_copy_mode_data	*data = wme->data;

	format_add(ft, "scroll_position", "%d", data->oy);
	format_add(ft, "rectangle_toggle", "%d", data->rectflag);

	format_add(ft, "copy_cursor_x", "%d", data->cx);
	format_add(ft, "copy_cursor_y", "%d", data->cy);

	format_add(ft, "selection_present", "%d", data->screen.sel != NULL);
	if (data->screen.sel != NULL) {
		format_add(ft, "selection_start_x", "%d", data->selx);
		format_add(ft, "selection_start_y", "%d", data->sely);
		format_add(ft, "selection_end_x", "%d", data->endselx);
		format_add(ft, "selection_end_y", "%d", data->endsely);
		format_add(ft, "selection_active", "%d",
		    data->cursordrag != CURSORDRAG_NONE);
	} else
		format_add(ft, "selection_active", "%d", 0);

	format_add(ft, "search_present", "%d", data->searchmark != NULL);
	format_add_cb(ft, "search_match", window_copy_search_match_cb);

	format_add_cb(ft, "copy_cursor_word", window_copy_cursor_word_cb);
	format_add_cb(ft, "copy_cursor_line", window_copy_cursor_line_cb);
}

static void
window_copy_size_changed(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	int				 search = (data->searchmark != NULL);

	window_copy_clear_selection(wme);
	window_copy_clear_marks(wme);

	screen_write_start(&ctx, s);
	window_copy_write_lines(wme, &ctx, 0, screen_size_y(s));
	screen_write_stop(&ctx);

	if (search && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 0);
	data->searchx = data->cx;
	data->searchy = data->cy;
	data->searcho = data->oy;
}

static void
window_copy_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	struct grid			*gd = data->backing->grid;
	u_int				 cx, cy, wx, wy;
	int				 reflow;

	screen_resize(s, sx, sy, 0);
	cx = data->cx;
	cy = gd->hsize + data->cy - data->oy;
	reflow = (gd->sx != sx);
	if (reflow)
		grid_wrap_position(gd, cx, cy, &wx, &wy);
	screen_resize_cursor(data->backing, sx, sy, 1, 0, 0);
	if (reflow)
		grid_unwrap_position(gd, &cx, &cy, wx, wy);

	data->cx = cx;
	if (cy < gd->hsize) {
		data->cy = 0;
		data->oy = gd->hsize - cy;
	} else {
		data->cy = cy - gd->hsize;
		data->oy = 0;
	}

	window_copy_size_changed(wme);
	window_copy_redraw_screen(wme);
}

static const char *
window_copy_key_table(struct window_mode_entry *wme)
{
	struct window_pane	*wp = wme->wp;

	if (options_get_number(wp->window->options, "mode-keys") == MODEKEY_VI)
		return ("copy-mode-vi");
	return ("copy-mode");
}

static int
window_copy_expand_search_string(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	const char			*argument;
	char				*expanded;

	if (cs->args->argc == 2) {
		argument = cs->args->argv[1];
		if (*argument != '\0') {
			if (args_has(cs->args, 'F')) {
				expanded = format_single(NULL, argument, NULL,
				    NULL, NULL, wme->wp);
				if (*expanded == '\0') {
					free(expanded);
					return (0);
				}
				free(data->searchstr);
				data->searchstr = expanded;
			} else {
				free(data->searchstr);
				data->searchstr = xstrdup(argument);
			}
		}
	}
	return (1);
}

static enum window_copy_cmd_action
window_copy_cmd_append_selection(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct session			*s = cs->s;

	if (s != NULL)
		window_copy_append_selection(wme);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_append_selection_and_cancel(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct session			*s = cs->s;

	if (s != NULL)
		window_copy_append_selection(wme);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_CANCEL);
}

static enum window_copy_cmd_action
window_copy_cmd_back_to_indentation(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cursor_back_to_indentation(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_begin_selection(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct client			*c = cs->c;
	struct mouse_event		*m = cs->m;
	struct window_copy_mode_data	*data = wme->data;

	if (m != NULL) {
		window_copy_start_drag(c, m);
		return (WINDOW_COPY_CMD_NOTHING);
	}

	data->lineflag = LINE_SEL_NONE;
	data->selflag = SEL_CHAR;
	window_copy_start_selection(wme);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_stop_selection(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;

	data->cursordrag = CURSORDRAG_NONE;
	data->lineflag = LINE_SEL_NONE;
	data->selflag = SEL_CHAR;
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_bottom_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;

	data->cx = 0;
	data->cy = screen_size_y(&data->screen) - 1;

	window_copy_update_selection(wme, 1, 0);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_cancel(__unused struct window_copy_cmd_state *cs)
{
	return (WINDOW_COPY_CMD_CANCEL);
}

static enum window_copy_cmd_action
window_copy_cmd_clear_selection(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_end_of_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct client			*c = cs->c;
	struct session			*s = cs->s;
	struct winlink			*wl = cs->wl;
	struct window_pane		*wp = wme->wp;
	u_int				 np = wme->prefix;
	char				*prefix = NULL;

	if (cs->args->argc == 2)
		prefix = format_single(NULL, cs->args->argv[1], c, s, wl, wp);

	window_copy_start_selection(wme);
	for (; np > 1; np--)
		window_copy_cursor_down(wme, 0);
	window_copy_cursor_end_of_line(wme);

	if (s != NULL) {
		window_copy_copy_selection(wme, prefix);

		free(prefix);
		return (WINDOW_COPY_CMD_CANCEL);
	}

	free(prefix);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct client			*c = cs->c;
	struct session			*s = cs->s;
	struct winlink			*wl = cs->wl;
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;
	char				*prefix = NULL;

	if (cs->args->argc == 2)
		prefix = format_single(NULL, cs->args->argv[1], c, s, wl, wp);

	data->selflag = SEL_CHAR;
	window_copy_cursor_start_of_line(wme);
	window_copy_start_selection(wme);
	for (; np > 1; np--)
		window_copy_cursor_down(wme, 0);
	window_copy_cursor_end_of_line(wme);

	if (s != NULL) {
		window_copy_copy_selection(wme, prefix);

		free(prefix);
		return (WINDOW_COPY_CMD_CANCEL);
	}

	free(prefix);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_selection_no_clear(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct client			*c = cs->c;
	struct session			*s = cs->s;
	struct winlink			*wl = cs->wl;
	struct window_pane		*wp = wme->wp;
	char				*prefix = NULL;

	if (cs->args->argc == 2)
		prefix = format_single(NULL, cs->args->argv[1], c, s, wl, wp);

	if (s != NULL)
		window_copy_copy_selection(wme, prefix);

	free(prefix);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_selection(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cmd_copy_selection_no_clear(cs);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_selection_and_cancel(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cmd_copy_selection_no_clear(cs);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_CANCEL);
}

static enum window_copy_cmd_action
window_copy_cmd_cursor_down(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_down(wme, 0);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_cursor_down_and_cancel(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix, cy;

	cy = data->cy;
	for (; np != 0; np--)
		window_copy_cursor_down(wme, 0);
	if (cy == data->cy && data->oy == 0)
		return (WINDOW_COPY_CMD_CANCEL);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_cursor_left(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_left(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_cursor_right(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	for (; np != 0; np--) {
		window_copy_cursor_right(wme, data->screen.sel != NULL &&
		    data->rectflag);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_cursor_up(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_up(wme, 0);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_end_of_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cursor_end_of_line(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_halfpage_down(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	for (; np != 0; np--) {
		if (window_copy_pagedown(wme, 1, data->scroll_exit))
			return (WINDOW_COPY_CMD_CANCEL);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_halfpage_down_and_cancel(struct window_copy_cmd_state *cs)
{

	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--) {
		if (window_copy_pagedown(wme, 1, 1))
			return (WINDOW_COPY_CMD_CANCEL);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_halfpage_up(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_pageup1(wme, 1);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_history_bottom(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = data->backing;
	u_int				 oy;

	oy = screen_hsize(s) + data->cy - data->oy;
	if (data->lineflag == LINE_SEL_RIGHT_LEFT && oy == data->endsely)
		window_copy_other_end(wme);

	data->cy = screen_size_y(&data->screen) - 1;
	data->cx = window_copy_find_length(wme, screen_hsize(s) + data->cy);
	data->oy = 0;

	if (data->searchmark != NULL && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 1);
	window_copy_update_selection(wme, 1, 0);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_history_top(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 oy;

	oy = screen_hsize(data->backing) + data->cy - data->oy;
	if (data->lineflag == LINE_SEL_LEFT_RIGHT && oy == data->sely)
		window_copy_other_end(wme);

	data->cy = 0;
	data->cx = 0;
	data->oy = screen_hsize(data->backing);

	if (data->searchmark != NULL && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 1);
	window_copy_update_selection(wme, 1, 0);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_jump_again(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	switch (data->jumptype) {
	case WINDOW_COPY_JUMPFORWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump(wme);
		break;
	case WINDOW_COPY_JUMPBACKWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump_back(wme);
		break;
	case WINDOW_COPY_JUMPTOFORWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump_to(wme);
		break;
	case WINDOW_COPY_JUMPTOBACKWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump_to_back(wme);
		break;
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_jump_reverse(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	switch (data->jumptype) {
	case WINDOW_COPY_JUMPFORWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump_back(wme);
		break;
	case WINDOW_COPY_JUMPBACKWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump(wme);
		break;
	case WINDOW_COPY_JUMPTOFORWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump_to_back(wme);
		break;
	case WINDOW_COPY_JUMPTOBACKWARD:
		for (; np != 0; np--)
			window_copy_cursor_jump_to(wme);
		break;
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_middle_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;

	data->cx = 0;
	data->cy = (screen_size_y(&data->screen) - 1) / 2;

	window_copy_update_selection(wme, 1, 0);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_previous_matching_bracket(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = data->backing;
	char				 open[] = "{[(", close[] = "}])";
	char				 tried, found, start, *cp;
	u_int				 px, py, xx, n;
	struct grid_cell		 gc;
	int				 failed;

	for (; np != 0; np--) {
		/* Get cursor position and line length. */
		px = data->cx;
		py = screen_hsize(s) + data->cy - data->oy;
		xx = window_copy_find_length(wme, py);
		if (xx == 0)
			break;

		/*
		 * Get the current character. If not on a bracket, try the
		 * previous. If still not, then behave like previous-word.
		 */
		tried = 0;
	retry:
		grid_get_cell(s->grid, px, py, &gc);
		if (gc.data.size != 1 || (gc.flags & GRID_FLAG_PADDING))
			cp = NULL;
		else {
			found = *gc.data.data;
			cp = strchr(close, found);
		}
		if (cp == NULL) {
			if (data->modekeys == MODEKEY_EMACS) {
				if (!tried && px > 0) {
					px--;
					tried = 1;
					goto retry;
				}
				window_copy_cursor_previous_word(wme, "}]) ", 1);
			}
			continue;
		}
		start = open[cp - close];

		/* Walk backward until the matching bracket is reached. */
		n = 1;
		failed = 0;
		do {
			if (px == 0) {
				if (py == 0) {
					failed = 1;
					break;
				}
				do {
					py--;
					xx = window_copy_find_length(wme, py);
				} while (xx == 0 && py > 0);
				if (xx == 0 && py == 0) {
					failed = 1;
					break;
				}
				px = xx - 1;
			} else
				px--;

			grid_get_cell(s->grid, px, py, &gc);
			if (gc.data.size == 1 &&
			    (~gc.flags & GRID_FLAG_PADDING)) {
				if (*gc.data.data == found)
					n++;
				else if (*gc.data.data == start)
					n--;
			}
		} while (n != 0);

		/* Move the cursor to the found location if any. */
		if (!failed)
			window_copy_scroll_to(wme, px, py, 0);
	}

	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_next_matching_bracket(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = data->backing;
	char				 open[] = "{[(", close[] = "}])";
	char				 tried, found, end, *cp;
	u_int				 px, py, xx, yy, sx, sy, n;
	struct grid_cell		 gc;
	int				 failed;
	struct grid_line		*gl;

	for (; np != 0; np--) {
		/* Get cursor position and line length. */
		px = data->cx;
		py = screen_hsize(s) + data->cy - data->oy;
		xx = window_copy_find_length(wme, py);
		yy = screen_hsize(s) + screen_size_y(s) - 1;
		if (xx == 0)
			break;

		/*
		 * Get the current character. If not on a bracket, try the
		 * next. If still not, then behave like next-word.
		 */
		tried = 0;
	retry:
		grid_get_cell(s->grid, px, py, &gc);
		if (gc.data.size != 1 || (gc.flags & GRID_FLAG_PADDING))
			cp = NULL;
		else {
			found = *gc.data.data;

			/*
			 * In vi mode, attempt to move to previous bracket if a
			 * closing bracket is found first. If this fails,
			 * return to the original cursor position.
			 */
			cp = strchr(close, found);
			if (cp != NULL && data->modekeys == MODEKEY_VI) {
				sx = data->cx;
				sy = screen_hsize(s) + data->cy - data->oy;

				window_copy_scroll_to(wme, px, py, 0);
				window_copy_cmd_previous_matching_bracket(cs);

				px = data->cx;
				py = screen_hsize(s) + data->cy - data->oy;
				grid_get_cell(s->grid, px, py, &gc);
				if (gc.data.size == 1 &&
				    (~gc.flags & GRID_FLAG_PADDING) &&
				    strchr(close, *gc.data.data) != NULL)
					window_copy_scroll_to(wme, sx, sy, 0);
				break;
			}

			cp = strchr(open, found);
		}
		if (cp == NULL) {
			if (data->modekeys == MODEKEY_EMACS) {
				if (!tried && px <= xx) {
					px++;
					tried = 1;
					goto retry;
				}
				window_copy_cursor_next_word_end(wme, "{[( ",
				    0);
				continue;
			}
			/* For vi, continue searching for bracket until EOL. */
			if (px > xx) {
				if (py == yy)
					continue;
				gl = grid_get_line(s->grid, py);
				if (~gl->flags & GRID_LINE_WRAPPED)
					continue;
				if (gl->cellsize > s->grid->sx)
					continue;
				px = 0;
				py++;
				xx = window_copy_find_length(wme, py);
			} else
				px++;
			goto retry;
		}
		end = close[cp - open];

		/* Walk forward until the matching bracket is reached. */
		n = 1;
		failed = 0;
		do {
			if (px > xx) {
				if (py == yy) {
					failed = 1;
					break;
				}
				px = 0;
				py++;
				xx = window_copy_find_length(wme, py);
			} else
				px++;

			grid_get_cell(s->grid, px, py, &gc);
			if (gc.data.size == 1 &&
			    (~gc.flags & GRID_FLAG_PADDING)) {
				if (*gc.data.data == found)
					n++;
				else if (*gc.data.data == end)
					n--;
			}
		} while (n != 0);

		/* Move the cursor to the found location if any. */
		if (!failed)
			window_copy_scroll_to(wme, px, py, 0);
	}

	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_next_paragraph(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_next_paragraph(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_next_space(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_next_word(wme, " ");
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_next_space_end(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_next_word_end(wme, " ", 0);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_next_word(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct session			*s = cs->s;
	u_int				 np = wme->prefix;
	const char			*ws;

	ws = options_get_string(s->options, "word-separators");
	for (; np != 0; np--)
		window_copy_cursor_next_word(wme, ws);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_next_word_end(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct session			*s = cs->s;
	u_int				 np = wme->prefix;
	const char			*ws;

	ws = options_get_string(s->options, "word-separators");
	for (; np != 0; np--)
		window_copy_cursor_next_word_end(wme, ws, 0);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_other_end(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;
	struct window_copy_mode_data	*data = wme->data;

	data->selflag = SEL_CHAR;
	if ((np % 2) != 0)
		window_copy_other_end(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_page_down(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	for (; np != 0; np--) {
		if (window_copy_pagedown(wme, 0, data->scroll_exit))
			return (WINDOW_COPY_CMD_CANCEL);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_page_down_and_cancel(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--) {
		if (window_copy_pagedown(wme, 0, 1))
			return (WINDOW_COPY_CMD_CANCEL);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_page_up(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_pageup1(wme, 0);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_previous_paragraph(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_previous_paragraph(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_previous_space(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_previous_word(wme, " ", 1);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_previous_word(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct session			*s = cs->s;
	u_int				 np = wme->prefix;
	const char			*ws;

	ws = options_get_string(s->options, "word-separators");
	for (; np != 0; np--)
		window_copy_cursor_previous_word(wme, ws, 1);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_rectangle_on(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;

	data->lineflag = LINE_SEL_NONE;
	window_copy_rectangle_set(wme, 1);

	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_rectangle_off(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;

	data->lineflag = LINE_SEL_NONE;
	window_copy_rectangle_set(wme, 0);

	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_rectangle_toggle(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;

	data->lineflag = LINE_SEL_NONE;
	window_copy_rectangle_set(wme, !data->rectflag);

	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_scroll_down(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_down(wme, 1);
	if (data->scroll_exit && data->oy == 0)
		return (WINDOW_COPY_CMD_CANCEL);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_scroll_down_and_cancel(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_down(wme, 1);
	if (data->oy == 0)
		return (WINDOW_COPY_CMD_CANCEL);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_scroll_up(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	u_int				 np = wme->prefix;

	for (; np != 0; np--)
		window_copy_cursor_up(wme, 1);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_search_again(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	if (data->searchtype == WINDOW_COPY_SEARCHUP) {
		for (; np != 0; np--)
			window_copy_search_up(wme, data->searchregex);
	} else if (data->searchtype == WINDOW_COPY_SEARCHDOWN) {
		for (; np != 0; np--)
			window_copy_search_down(wme, data->searchregex);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_search_reverse(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	if (data->searchtype == WINDOW_COPY_SEARCHUP) {
		for (; np != 0; np--)
			window_copy_search_down(wme, data->searchregex);
	} else if (data->searchtype == WINDOW_COPY_SEARCHDOWN) {
		for (; np != 0; np--)
			window_copy_search_up(wme, data->searchregex);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_select_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	data->lineflag = LINE_SEL_LEFT_RIGHT;
	data->rectflag = 0;
	data->selflag = SEL_LINE;
	data->dx = data->cx;
	data->dy = screen_hsize(data->backing) + data->cy - data->oy;

	window_copy_cursor_start_of_line(wme);
	data->selrx = data->cx;
	data->selry = screen_hsize(data->backing) + data->cy - data->oy;
	data->endselry = data->selry;
	window_copy_start_selection(wme);
	window_copy_cursor_end_of_line(wme);
	data->endselry = screen_hsize(data->backing) + data->cy - data->oy;
	data->endselrx = window_copy_find_length(wme, data->endselry);
	for (; np > 1; np--) {
		window_copy_cursor_down(wme, 0);
		window_copy_cursor_end_of_line(wme);
	}

	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_select_word(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct session			*s = cs->s;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 px, py, nextx, nexty;

	data->lineflag = LINE_SEL_LEFT_RIGHT;
	data->rectflag = 0;
	data->selflag = SEL_WORD;
	data->dx = data->cx;
	data->dy = screen_hsize(data->backing) + data->cy - data->oy;

	data->ws = options_get_string(s->options, "word-separators");
	window_copy_cursor_previous_word(wme, data->ws, 0);
	px = data->cx;
	py = screen_hsize(data->backing) + data->cy - data->oy;
	data->selrx = px;
	data->selry = py;
	window_copy_start_selection(wme);

	/* Handle single character words. */
	nextx = px + 1;
	nexty = py;
	if (grid_get_line(data->backing->grid, nexty)->flags &
	    GRID_LINE_WRAPPED && nextx > screen_size_x(data->backing) - 1) {
		nextx = 0;
		nexty++;
	}
	if (px >= window_copy_find_length(wme, py) ||
	    !window_copy_in_set(wme, nextx, nexty, data->ws))
		window_copy_cursor_next_word_end(wme, data->ws, 1);
	else {
		window_copy_update_cursor(wme, px, data->cy);
		if (window_copy_update_selection(wme, 1, 1))
			window_copy_redraw_lines(wme, data->cy, 1);
	}
	data->endselrx = data->cx;
	data->endselry = screen_hsize(data->backing) + data->cy - data->oy;
	if (data->dy > data->endselry) {
		data->dy = data->endselry;
		data->dx = data->endselrx;
	} else if (data->dx > data->endselrx)
		data->dx = data->endselrx;

	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_set_mark(struct window_copy_cmd_state *cs)
{
	struct window_copy_mode_data	*data = cs->wme->data;

	data->mx = data->cx;
	data->my = screen_hsize(data->backing) + data->cy - data->oy;
	data->showmark = 1;
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_start_of_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cursor_start_of_line(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_top_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;

	data->cx = 0;
	data->cy = 0;

	window_copy_update_selection(wme, 1, 0);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_pipe_no_clear(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct client			*c = cs->c;
	struct session			*s = cs->s;
	struct winlink			*wl = cs->wl;
	struct window_pane		*wp = wme->wp;
	char				*command = NULL;
	char				*prefix = NULL;

	if (cs->args->argc == 3)
		prefix = format_single(NULL, cs->args->argv[2], c, s, wl, wp);

	if (s != NULL && cs->args->argc > 1 && *cs->args->argv[1] != '\0')
		command = format_single(NULL, cs->args->argv[1], c, s, wl, wp);
	window_copy_copy_pipe(wme, s, prefix, command);
	free(command);

	free(prefix);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_pipe(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cmd_copy_pipe_no_clear(cs);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_copy_pipe_and_cancel(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cmd_copy_pipe_no_clear(cs);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_CANCEL);
}

static enum window_copy_cmd_action
window_copy_cmd_pipe_no_clear(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct client			*c = cs->c;
	struct session			*s = cs->s;
	struct winlink			*wl = cs->wl;
	struct window_pane		*wp = wme->wp;
	char				*command = NULL;

	if (s != NULL && cs->args->argc > 1 && *cs->args->argv[1] != '\0')
		command = format_single(NULL, cs->args->argv[1], c, s, wl, wp);
	window_copy_pipe(wme, s, command);
	free(command);

	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_pipe(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cmd_pipe_no_clear(cs);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_REDRAW);
}

static enum window_copy_cmd_action
window_copy_cmd_pipe_and_cancel(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_cmd_pipe_no_clear(cs);
	window_copy_clear_selection(wme);
	return (WINDOW_COPY_CMD_CANCEL);
}

static enum window_copy_cmd_action
window_copy_cmd_goto_line(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	const char			*argument = cs->args->argv[1];

	if (*argument != '\0')
		window_copy_goto_line(wme, argument);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_jump_backward(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;
	const char			*argument = cs->args->argv[1];

	if (*argument != '\0') {
		data->jumptype = WINDOW_COPY_JUMPBACKWARD;
		free(data->jumpchar);
		data->jumpchar = utf8_fromcstr(argument);
		for (; np != 0; np--)
			window_copy_cursor_jump_back(wme);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_jump_forward(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;
	const char			*argument = cs->args->argv[1];

	if (*argument != '\0') {
		data->jumptype = WINDOW_COPY_JUMPFORWARD;
		free(data->jumpchar);
		data->jumpchar = utf8_fromcstr(argument);
		for (; np != 0; np--)
			window_copy_cursor_jump(wme);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_jump_to_backward(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;
	const char			*argument = cs->args->argv[1];

	if (*argument != '\0') {
		data->jumptype = WINDOW_COPY_JUMPTOBACKWARD;
		free(data->jumpchar);
		data->jumpchar = utf8_fromcstr(argument);
		for (; np != 0; np--)
			window_copy_cursor_jump_to_back(wme);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_jump_to_forward(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;
	const char			*argument = cs->args->argv[1];

	if (*argument != '\0') {
		data->jumptype = WINDOW_COPY_JUMPTOFORWARD;
		free(data->jumpchar);
		data->jumpchar = utf8_fromcstr(argument);
		for (; np != 0; np--)
			window_copy_cursor_jump_to(wme);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_jump_to_mark(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;

	window_copy_jump_to_mark(wme);
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_search_backward(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	if (!window_copy_expand_search_string(cs))
		return (WINDOW_COPY_CMD_NOTHING);

	if (data->searchstr != NULL) {
		data->searchtype = WINDOW_COPY_SEARCHUP;
		data->searchregex = 1;
		data->timeout = 0;
		for (; np != 0; np--)
			window_copy_search_up(wme, 1);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_search_backward_text(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	if (!window_copy_expand_search_string(cs))
		return (WINDOW_COPY_CMD_NOTHING);

	if (data->searchstr != NULL) {
		data->searchtype = WINDOW_COPY_SEARCHUP;
		data->searchregex = 0;
		data->timeout = 0;
		for (; np != 0; np--)
			window_copy_search_up(wme, 0);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_search_forward(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	if (!window_copy_expand_search_string(cs))
		return (WINDOW_COPY_CMD_NOTHING);

	if (data->searchstr != NULL) {
		data->searchtype = WINDOW_COPY_SEARCHDOWN;
		data->searchregex = 1;
		data->timeout = 0;
		for (; np != 0; np--)
			window_copy_search_down(wme, 1);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_search_forward_text(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 np = wme->prefix;

	if (!window_copy_expand_search_string(cs))
		return (WINDOW_COPY_CMD_NOTHING);

	if (data->searchstr != NULL) {
		data->searchtype = WINDOW_COPY_SEARCHDOWN;
		data->searchregex = 0;
		data->timeout = 0;
		for (; np != 0; np--)
			window_copy_search_down(wme, 0);
	}
	return (WINDOW_COPY_CMD_NOTHING);
}

static enum window_copy_cmd_action
window_copy_cmd_search_backward_incremental(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	const char			*argument = cs->args->argv[1];
	const char			*ss = data->searchstr;
	char				 prefix;
	enum window_copy_cmd_action	 action = WINDOW_COPY_CMD_NOTHING;

	data->timeout = 0;

	log_debug ("%s: %s", __func__, argument);

	prefix = *argument++;
	if (data->searchx == -1 || data->searchy == -1) {
		data->searchx = data->cx;
		data->searchy = data->cy;
		data->searcho = data->oy;
	} else if (ss != NULL && strcmp(argument, ss) != 0) {
		data->cx = data->searchx;
		data->cy = data->searchy;
		data->oy = data->searcho;
		action = WINDOW_COPY_CMD_REDRAW;
	}
	if (*argument == '\0') {
		window_copy_clear_marks(wme);
		return (WINDOW_COPY_CMD_REDRAW);
	}
	switch (prefix) {
	case '=':
	case '-':
		data->searchtype = WINDOW_COPY_SEARCHUP;
		data->searchregex = 0;
		free(data->searchstr);
		data->searchstr = xstrdup(argument);
		if (!window_copy_search_up(wme, 0)) {
			window_copy_clear_marks(wme);
			return (WINDOW_COPY_CMD_REDRAW);
		}
		break;
	case '+':
		data->searchtype = WINDOW_COPY_SEARCHDOWN;
		data->searchregex = 0;
		free(data->searchstr);
		data->searchstr = xstrdup(argument);
		if (!window_copy_search_down(wme, 0)) {
			window_copy_clear_marks(wme);
			return (WINDOW_COPY_CMD_REDRAW);
		}
		break;
	}
	return (action);
}

static enum window_copy_cmd_action
window_copy_cmd_search_forward_incremental(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_copy_mode_data	*data = wme->data;
	const char			*argument = cs->args->argv[1];
	const char			*ss = data->searchstr;
	char				 prefix;
	enum window_copy_cmd_action	 action = WINDOW_COPY_CMD_NOTHING;

	data->timeout = 0;

	log_debug ("%s: %s", __func__, argument);

	prefix = *argument++;
	if (data->searchx == -1 || data->searchy == -1) {
		data->searchx = data->cx;
		data->searchy = data->cy;
		data->searcho = data->oy;
	} else if (ss != NULL && strcmp(argument, ss) != 0) {
		data->cx = data->searchx;
		data->cy = data->searchy;
		data->oy = data->searcho;
		action = WINDOW_COPY_CMD_REDRAW;
	}
	if (*argument == '\0') {
		window_copy_clear_marks(wme);
		return (WINDOW_COPY_CMD_REDRAW);
	}
	switch (prefix) {
	case '=':
	case '+':
		data->searchtype = WINDOW_COPY_SEARCHDOWN;
		data->searchregex = 0;
		free(data->searchstr);
		data->searchstr = xstrdup(argument);
		if (!window_copy_search_down(wme, 0)) {
			window_copy_clear_marks(wme);
			return (WINDOW_COPY_CMD_REDRAW);
		}
		break;
	case '-':
		data->searchtype = WINDOW_COPY_SEARCHUP;
		data->searchregex = 0;
		free(data->searchstr);
		data->searchstr = xstrdup(argument);
		if (!window_copy_search_up(wme, 0)) {
			window_copy_clear_marks(wme);
			return (WINDOW_COPY_CMD_REDRAW);
		}
	}
	return (action);
}

static enum window_copy_cmd_action
window_copy_cmd_refresh_from_pane(struct window_copy_cmd_state *cs)
{
	struct window_mode_entry	*wme = cs->wme;
	struct window_pane		*wp = wme->swp;
	struct window_copy_mode_data	*data = wme->data;

	if (data->viewmode)
		return (WINDOW_COPY_CMD_NOTHING);

	screen_free(data->backing);
	free(data->backing);
	data->backing = window_copy_clone_screen(&wp->base, &data->screen, NULL,
	    NULL, wme->swp != wme->wp);

	window_copy_size_changed(wme);
	return (WINDOW_COPY_CMD_REDRAW);
}

static const struct {
	const char			 *command;
	int				  minargs;
	int				  maxargs;
	enum window_copy_cmd_clear	  clear;
	enum window_copy_cmd_action	(*f)(struct window_copy_cmd_state *);
} window_copy_cmd_table[] = {
	{ "append-selection", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_append_selection },
	{ "append-selection-and-cancel", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_append_selection_and_cancel },
	{ "back-to-indentation", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_back_to_indentation },
	{ "begin-selection", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_begin_selection },
	{ "bottom-line", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_bottom_line },
	{ "cancel", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_cancel },
	{ "clear-selection", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_clear_selection },
	{ "copy-end-of-line", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_copy_end_of_line },
	{ "copy-line", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_copy_line },
	{ "copy-pipe-no-clear", 0, 2, WINDOW_COPY_CMD_CLEAR_NEVER,
	  window_copy_cmd_copy_pipe_no_clear },
	{ "copy-pipe", 0, 2, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_copy_pipe },
	{ "copy-pipe-and-cancel", 0, 2, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_copy_pipe_and_cancel },
	{ "copy-selection-no-clear", 0, 1, WINDOW_COPY_CMD_CLEAR_NEVER,
	  window_copy_cmd_copy_selection_no_clear },
	{ "copy-selection", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_copy_selection },
	{ "copy-selection-and-cancel", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_copy_selection_and_cancel },
	{ "cursor-down", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_cursor_down },
	{ "cursor-down-and-cancel", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_cursor_down_and_cancel },
	{ "cursor-left", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_cursor_left },
	{ "cursor-right", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_cursor_right },
	{ "cursor-up", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_cursor_up },
	{ "end-of-line", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_end_of_line },
	{ "goto-line", 1, 1, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_goto_line },
	{ "halfpage-down", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_halfpage_down },
	{ "halfpage-down-and-cancel", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_halfpage_down_and_cancel },
	{ "halfpage-up", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_halfpage_up },
	{ "history-bottom", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_history_bottom },
	{ "history-top", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_history_top },
	{ "jump-again", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_jump_again },
	{ "jump-backward", 1, 1, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_jump_backward },
	{ "jump-forward", 1, 1, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_jump_forward },
	{ "jump-reverse", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_jump_reverse },
	{ "jump-to-backward", 1, 1, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_jump_to_backward },
	{ "jump-to-forward", 1, 1, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_jump_to_forward },
	{ "jump-to-mark", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_jump_to_mark },
	{ "middle-line", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_middle_line },
	{ "next-matching-bracket", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_next_matching_bracket },
	{ "next-paragraph", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_next_paragraph },
	{ "next-space", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_next_space },
	{ "next-space-end", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_next_space_end },
	{ "next-word", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_next_word },
	{ "next-word-end", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_next_word_end },
	{ "other-end", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_other_end },
	{ "page-down", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_page_down },
	{ "page-down-and-cancel", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_page_down_and_cancel },
	{ "page-up", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_page_up },
	{ "pipe-no-clear", 0, 1, WINDOW_COPY_CMD_CLEAR_NEVER,
	  window_copy_cmd_pipe_no_clear },
	{ "pipe", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_pipe },
	{ "pipe-and-cancel", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_pipe_and_cancel },
	{ "previous-matching-bracket", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_previous_matching_bracket },
	{ "previous-paragraph", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_previous_paragraph },
	{ "previous-space", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_previous_space },
	{ "previous-word", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_previous_word },
	{ "rectangle-on", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_rectangle_on },
	{ "rectangle-off", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_rectangle_off },
	{ "rectangle-toggle", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_rectangle_toggle },
	{ "refresh-from-pane", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_refresh_from_pane },
	{ "scroll-down", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_scroll_down },
	{ "scroll-down-and-cancel", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_scroll_down_and_cancel },
	{ "scroll-up", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_scroll_up },
	{ "search-again", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_again },
	{ "search-backward", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_backward },
	{ "search-backward-text", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_backward_text },
	{ "search-backward-incremental", 1, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_backward_incremental },
	{ "search-forward", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_forward },
	{ "search-forward-text", 0, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_forward_text },
	{ "search-forward-incremental", 1, 1, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_forward_incremental },
	{ "search-reverse", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_search_reverse },
	{ "select-line", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_select_line },
	{ "select-word", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_select_word },
	{ "set-mark", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_set_mark },
	{ "start-of-line", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_start_of_line },
	{ "stop-selection", 0, 0, WINDOW_COPY_CMD_CLEAR_ALWAYS,
	  window_copy_cmd_stop_selection },
	{ "top-line", 0, 0, WINDOW_COPY_CMD_CLEAR_EMACS_ONLY,
	  window_copy_cmd_top_line },
};

static void
window_copy_command(struct window_mode_entry *wme, struct client *c,
    struct session *s, struct winlink *wl, struct args *args,
    struct mouse_event *m)
{
	struct window_copy_mode_data	*data = wme->data;
	struct window_copy_cmd_state	 cs;
	enum window_copy_cmd_action	 action;
	enum window_copy_cmd_clear	 clear = WINDOW_COPY_CMD_CLEAR_NEVER;
	const char			*command;
	u_int				 i;
	int				 keys;

	if (args->argc == 0)
		return;
	command = args->argv[0];

	if (m != NULL && m->valid && !MOUSE_WHEEL(m->b))
		window_copy_move_mouse(m);

	cs.wme = wme;
	cs.args = args;
	cs.m = m;

	cs.c = c;
	cs.s = s;
	cs.wl = wl;

	action = WINDOW_COPY_CMD_NOTHING;
	for (i = 0; i < nitems(window_copy_cmd_table); i++) {
		if (strcmp(window_copy_cmd_table[i].command, command) == 0) {
			if (args->argc - 1 < window_copy_cmd_table[i].minargs ||
			    args->argc - 1 > window_copy_cmd_table[i].maxargs)
				break;
			clear = window_copy_cmd_table[i].clear;
			action = window_copy_cmd_table[i].f (&cs);
			break;
		}
	}

	if (strncmp(command, "search-", 7) != 0 && data->searchmark != NULL) {
		keys = options_get_number(wme->wp->window->options, "mode-keys");
		if (clear == WINDOW_COPY_CMD_CLEAR_EMACS_ONLY &&
		    keys == MODEKEY_VI)
			clear = WINDOW_COPY_CMD_CLEAR_NEVER;
		if (clear != WINDOW_COPY_CMD_CLEAR_NEVER) {
			window_copy_clear_marks(wme);
			data->searchx = data->searchy = -1;
		}
		if (action == WINDOW_COPY_CMD_NOTHING)
			action = WINDOW_COPY_CMD_REDRAW;
	}
	wme->prefix = 1;

	if (action == WINDOW_COPY_CMD_CANCEL)
		window_pane_reset_mode(wme->wp);
	else if (action == WINDOW_COPY_CMD_REDRAW)
		window_copy_redraw_screen(wme);
}

static void
window_copy_scroll_to(struct window_mode_entry *wme, u_int px, u_int py,
    int no_redraw)
{
	struct window_copy_mode_data	*data = wme->data;
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

	if (!no_redraw && data->searchmark != NULL && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 1);
	window_copy_update_selection(wme, 1, 0);
	if (!no_redraw)
		window_copy_redraw_screen(wme);
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
window_copy_search_lr(struct grid *gd, struct grid *sgd, u_int *ppx, u_int py,
    u_int first, u_int last, int cis)
{
	u_int			 ax, bx, px, pywrap, endline;
	int			 matched;
	struct grid_line	*gl;

	endline = gd->hsize + gd->sy - 1;
	for (ax = first; ax < last; ax++) {
		for (bx = 0; bx < sgd->sx; bx++) {
			px = ax + bx;
			pywrap = py;
			/* Wrap line. */
			while (px >= gd->sx && pywrap < endline) {
				gl = grid_get_line(gd, pywrap);
				if (~gl->flags & GRID_LINE_WRAPPED)
					break;
				px -= gd->sx;
				pywrap++;
			}
			/* We have run off the end of the grid. */
			if (px >= gd->sx)
				break;
			matched = window_copy_search_compare(gd, px, pywrap,
			    sgd, bx, cis);
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
	u_int			 ax, bx, px, pywrap, endline;
	int			 matched;
	struct grid_line	*gl;

	endline = gd->hsize + gd->sy - 1;
	for (ax = last; ax > first; ax--) {
		for (bx = 0; bx < sgd->sx; bx++) {
			px = ax - 1 + bx;
			pywrap = py;
			/* Wrap line. */
			while (px >= gd->sx && pywrap < endline) {
				gl = grid_get_line(gd, pywrap);
				if (~gl->flags & GRID_LINE_WRAPPED)
					break;
				px -= gd->sx;
				pywrap++;
			}
			/* We have run off the end of the grid. */
			if (px >= gd->sx)
				break;
			matched = window_copy_search_compare(gd, px, pywrap,
			    sgd, bx, cis);
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

static int
window_copy_search_lr_regex(struct grid *gd, u_int *ppx, u_int *psx, u_int py,
    u_int first, u_int last, regex_t *reg)
{
	int			eflags = 0;
	u_int			endline, foundx, foundy, len, pywrap, size = 1;
	char		       *buf;
	regmatch_t		regmatch;
	struct grid_line       *gl;

	/*
	 * This can happen during search if the last match was the last
	 * character on a line.
	 */
	if (first >= last)
		return (0);

	/* Set flags for regex search. */
	if (first != 0)
		eflags |= REG_NOTBOL;

	/* Need to look at the entire string. */
	buf = xmalloc(size);
	buf[0] = '\0';
	buf = window_copy_stringify(gd, py, first, gd->sx, buf, &size);
	len = gd->sx - first;
	endline = gd->hsize + gd->sy - 1;
	pywrap = py;
	while (buf != NULL && pywrap <= endline) {
		gl = grid_get_line(gd, pywrap);
		if (~gl->flags & GRID_LINE_WRAPPED)
			break;
		pywrap++;
		buf = window_copy_stringify(gd, pywrap, 0, gd->sx, buf, &size);
		len += gd->sx;
	}

	if (regexec(reg, buf, 1, &regmatch, eflags) == 0 &&
	    regmatch.rm_so != regmatch.rm_eo) {
		foundx = first;
		foundy = py;
		window_copy_cstrtocellpos(gd, len, &foundx, &foundy,
		    buf + regmatch.rm_so);
		if (foundy == py && foundx < last) {
			*ppx = foundx;
			len -= foundx - first;
			window_copy_cstrtocellpos(gd, len, &foundx, &foundy,
			    buf + regmatch.rm_eo);
			*psx = foundx;
			while (foundy > py) {
				*psx += gd->sx;
				foundy--;
			}
			*psx -= *ppx;
			free(buf);
			return (1);
		}
	}

	free(buf);
	*ppx = 0;
	*psx = 0;
	return (0);
}

static int
window_copy_search_rl_regex(struct grid *gd, u_int *ppx, u_int *psx, u_int py,
    u_int first, u_int last, regex_t *reg)
{
	int			eflags = 0;
	u_int			endline, len, pywrap, size = 1;
	char		       *buf;
	struct grid_line       *gl;

	/* Set flags for regex search. */
	if (first != 0)
		eflags |= REG_NOTBOL;

	/* Need to look at the entire string. */
	buf = xmalloc(size);
	buf[0] = '\0';
	buf = window_copy_stringify(gd, py, first, gd->sx, buf, &size);
	len = gd->sx - first;
	endline = gd->hsize + gd->sy - 1;
	pywrap = py;
	while (buf != NULL && (pywrap <= endline)) {
		gl = grid_get_line(gd, pywrap);
		if (~gl->flags & GRID_LINE_WRAPPED)
			break;
		pywrap++;
		buf = window_copy_stringify(gd, pywrap, 0, gd->sx, buf, &size);
		len += gd->sx;
	}

	if (window_copy_last_regex(gd, py, first, last, len, ppx, psx, buf,
	    reg, eflags))
	{
		free(buf);
		return (1);
	}

	free(buf);
	*ppx = 0;
	*psx = 0;
	return (0);
}

static const char *
window_copy_cellstring(const struct grid_line *gl, u_int px, size_t *size,
    int *allocated)
{
	static struct utf8_data	 ud;
	struct grid_cell_entry	*gce;
	char			*copy;

	if (px >= gl->cellsize) {
		*size = 1;
		*allocated = 0;
		return (" ");
	}

	gce = &gl->celldata[px];
	if (gce->flags & GRID_FLAG_PADDING) {
		*size = 0;
		*allocated = 0;
		return (NULL);
	}
	if (~gce->flags & GRID_FLAG_EXTENDED) {
		*size = 1;
		*allocated = 0;
		return (&gce->data.data);
	}

	utf8_to_data(gl->extddata[gce->offset].data, &ud);
	*size = ud.size;
	*allocated = 1;

	copy = xmalloc(ud.size);
	memcpy(copy, ud.data, ud.size);
	return (copy);
}

/* Find last match in given range. */
static int
window_copy_last_regex(struct grid *gd, u_int py, u_int first, u_int last,
    u_int len, u_int *ppx, u_int *psx, const char *buf, const regex_t *preg,
    int eflags)
{
	u_int		foundx, foundy, oldx, px = 0, savepx, savesx = 0;
	regmatch_t	regmatch;

	foundx = first;
	foundy = py;
	oldx = first;
	while (regexec(preg, buf + px, 1, &regmatch, eflags) == 0) {
		if (regmatch.rm_so == regmatch.rm_eo)
			break;
		window_copy_cstrtocellpos(gd, len, &foundx, &foundy,
		    buf + px + regmatch.rm_so);
		if (foundy > py || foundx >= last)
			break;
		len -= foundx - oldx;
		savepx = foundx;
		window_copy_cstrtocellpos(gd, len, &foundx, &foundy,
		    buf + px + regmatch.rm_eo);
		if (foundy > py || foundx >= last) {
			*ppx = savepx;
			*psx = foundx;
			while (foundy > py) {
				*psx += gd->sx;
				foundy--;
			}
			*psx -= *ppx;
			return (1);
		} else {
			savesx = foundx - savepx;
			len -= savesx;
			oldx = foundx;
		}
		px += regmatch.rm_eo;
	}

	if (savesx > 0) {
		*ppx = savepx;
		*psx = savesx;
		return (1);
	} else {
		*ppx = 0;
		*psx = 0;
		return (0);
	}
}

/* Stringify line and append to input buffer. Caller frees. */
static char *
window_copy_stringify(struct grid *gd, u_int py, u_int first, u_int last,
    char *buf, u_int *size)
{
	u_int			 ax, bx, newsize = *size;
	const struct grid_line	*gl;
	const char		*d;
	size_t			 bufsize = 1024, dlen;
	int			 allocated;

	while (bufsize < newsize)
		bufsize *= 2;
	buf = xrealloc(buf, bufsize);

	gl = grid_peek_line(gd, py);
	bx = *size - 1;
	for (ax = first; ax < last; ax++) {
		d = window_copy_cellstring(gl, ax, &dlen, &allocated);
		newsize += dlen;
		while (bufsize < newsize) {
			bufsize *= 2;
			buf = xrealloc(buf, bufsize);
		}
		if (dlen == 1)
			buf[bx++] = *d;
		else {
			memcpy(buf + bx, d, dlen);
			bx += dlen;
		}
		if (allocated)
			free((void *)d);
	}
	buf[newsize - 1] = '\0';

	*size = newsize;
	return (buf);
}

/* Map start of C string containing UTF-8 data to grid cell position. */
static void
window_copy_cstrtocellpos(struct grid *gd, u_int ncells, u_int *ppx, u_int *ppy,
    const char *str)
{
	u_int			 cell, ccell, px, pywrap, pos, len;
	int			 match;
	const struct grid_line	*gl;
	const char		*d;
	size_t			 dlen;
	struct {
		const char	*d;
		size_t		 dlen;
		int		 allocated;
	} *cells;

	/* Populate the array of cell data. */
	cells = xreallocarray(NULL, ncells, sizeof cells[0]);
	cell = 0;
	px = *ppx;
	pywrap = *ppy;
	gl = grid_peek_line(gd, pywrap);
	while (cell < ncells) {
		cells[cell].d = window_copy_cellstring(gl, px,
		    &cells[cell].dlen, &cells[cell].allocated);
		cell++;
		px++;
		if (px == gd->sx) {
			px = 0;
			pywrap++;
			gl = grid_peek_line(gd, pywrap);
		}
	}

	/* Locate starting cell. */
	cell = 0;
	len = strlen(str);
	while (cell < ncells) {
		ccell = cell;
		pos = 0;
		match = 1;
		while (ccell < ncells) {
			if (str[pos] == '\0') {
				match = 0;
				break;
			}
			d = cells[ccell].d;
			dlen = cells[ccell].dlen;
			if (dlen == 1) {
				if (str[pos] != *d) {
					match = 0;
					break;
				}
				pos++;
			} else {
				if (dlen > len - pos)
					dlen = len - pos;
				if (memcmp(str + pos, d, dlen) != 0) {
					match = 0;
					break;
				}
				pos += dlen;
			}
			ccell++;
		}
		if (match)
			break;
		cell++;
	}

	/* If not found this will be one past the end. */
	px = *ppx + cell;
	pywrap = *ppy;
	while (px >= gd->sx) {
		px -= gd->sx;
		pywrap++;
	}

	*ppx = px;
	*ppy = pywrap;

	/* Free cell data. */
	for (cell = 0; cell < ncells; cell++) {
		if (cells[cell].allocated)
			free((void *)cells[cell].d);
	}
	free(cells);
}

static void
window_copy_move_left(struct screen *s, u_int *fx, u_int *fy, int wrapflag)
{
	if (*fx == 0) {	/* left */
		if (*fy == 0) { /* top */
			if (wrapflag) {
				*fx = screen_size_x(s) - 1;
				*fy = screen_hsize(s) + screen_size_y(s) - 1;
			}
			return;
		}
		*fx = screen_size_x(s) - 1;
		*fy = *fy - 1;
	} else
		*fx = *fx - 1;
}

static void
window_copy_move_right(struct screen *s, u_int *fx, u_int *fy, int wrapflag)
{
	if (*fx == screen_size_x(s) - 1) { /* right */
		if (*fy == screen_hsize(s) + screen_size_y(s) - 1) { /* bottom */
			if (wrapflag) {
				*fx = 0;
				*fy = 0;
			}
			return;
		}
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
 * Handle backward wrapped regex searches with overlapping matches. In this case
 * find the longest overlapping match from previous wrapped lines.
 */
static void
window_copy_search_back_overlap(struct grid *gd, regex_t *preg, u_int *ppx,
    u_int *psx, u_int *ppy, u_int endline)
{
	u_int	endx, endy, oldendx, oldendy, px, py, sx;
	int	found = 1;

	oldendx = *ppx + *psx;
	oldendy = *ppy - 1;
	while (oldendx > gd->sx - 1) {
		oldendx -= gd->sx;
		oldendy++;
	}
	endx = oldendx;
	endy = oldendy;
	px = *ppx;
	py = *ppy;
	while (found && px == 0 && py - 1 > endline &&
	       grid_get_line(gd, py - 2)->flags & GRID_LINE_WRAPPED &&
	       endx == oldendx && endy == oldendy) {
		py--;
		found = window_copy_search_rl_regex(gd, &px, &sx, py - 1, 0,
		    gd->sx, preg);
		if (found) {
			endx = px + sx;
			endy = py - 1;
			while (endx > gd->sx - 1) {
				endx -= gd->sx;
				endy++;
			}
			if (endx == oldendx && endy == oldendy) {
				*ppx = px;
				*ppy = py;
			}
		}
	}
}

/*
 * Search for text stored in sgd starting from position fx,fy up to endline. If
 * found, jump to it. If cis then ignore case. The direction is 0 for searching
 * up, down otherwise. If wrap then go to begin/end of grid and try again if
 * not found.
 */
static int
window_copy_search_jump(struct window_mode_entry *wme, struct grid *gd,
    struct grid *sgd, u_int fx, u_int fy, u_int endline, int cis, int wrap,
    int direction, int regex)
{
	u_int	 i, px, sx, ssize = 1;
	int	 found = 0, cflags = REG_EXTENDED;
	char	*sbuf;
	regex_t	 reg;

	if (regex) {
		sbuf = xmalloc(ssize);
		sbuf[0] = '\0';
		sbuf = window_copy_stringify(sgd, 0, 0, sgd->sx, sbuf, &ssize);
		if (cis)
			cflags |= REG_ICASE;
		if (regcomp(&reg, sbuf, cflags) != 0) {
			free(sbuf);
			return (0);
		}
		free(sbuf);
	}

	if (direction) {
		for (i = fy; i <= endline; i++) {
			if (regex) {
				found = window_copy_search_lr_regex(gd,
				    &px, &sx, i, fx, gd->sx, &reg);
			} else {
				found = window_copy_search_lr(gd, sgd,
				    &px, i, fx, gd->sx, cis);
			}
			if (found)
				break;
			fx = 0;
		}
	} else {
		for (i = fy + 1; endline < i; i--) {
			if (regex) {
				found = window_copy_search_rl_regex(gd,
				    &px, &sx, i - 1, 0, fx + 1, &reg);
				if (found) {
					window_copy_search_back_overlap(gd,
					    &reg, &px, &sx, &i, endline);
				}
			} else {
				found = window_copy_search_rl(gd, sgd,
				    &px, i - 1, 0, fx + 1, cis);
			}
			if (found) {
				i--;
				break;
			}
			fx = gd->sx - 1;
		}
	}
	if (regex)
		regfree(&reg);

	if (found) {
		window_copy_scroll_to(wme, px, i, 1);
		return (1);
	}
	if (wrap) {
		return (window_copy_search_jump(wme, gd, sgd,
		    direction ? 0 : gd->sx - 1,
		    direction ? 0 : gd->hsize + gd->sy - 1, fy, cis, 0,
		    direction, regex));
	}
	return (0);
}

static void
window_copy_move_after_search_mark(struct window_copy_mode_data *data,
    u_int *fx, u_int *fy, int wrapflag)
{
	struct screen  *s = data->backing;
	u_int		at, start;

	if (window_copy_search_mark_at(data, *fx, *fy, &start) == 0 &&
	    data->searchmark[start] != 0) {
		while (window_copy_search_mark_at(data, *fx, *fy, &at) == 0) {
			if (data->searchmark[at] != data->searchmark[start])
				break;
			/* Stop if not wrapping and at the end of the grid. */
			if (!wrapflag &&
			    *fx == screen_size_x(s) - 1 &&
			    *fy == screen_hsize(s) + screen_size_y(s) - 1)
				break;

			window_copy_move_right(s, fx, fy, wrapflag);
		}
	}
}

/*
 * Search in for text searchstr. If direction is 0 then search up, otherwise
 * down.
 */
static int
window_copy_search(struct window_mode_entry *wme, int direction, int regex)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = data->backing, ss;
	struct screen_write_ctx		 ctx;
	struct grid			*gd = s->grid;
	const char			*str = data->searchstr;
	u_int				 at, endline, fx, fy, start;
	int				 cis, found, keys, visible_only;
	int				 wrapflag;

	if (regex && str[strcspn(str, "^$*+()?[].\\")] == '\0')
		regex = 0;

	data->searchdirection = direction;

	if (data->timeout)
		return (0);

	if (data->searchall || wp->searchstr == NULL ||
	    wp->searchregex != regex) {
		visible_only = 0;
		data->searchall = 0;
	} else
		visible_only = (strcmp(wp->searchstr, str) == 0);
	free(wp->searchstr);
	wp->searchstr = xstrdup(str);
	wp->searchregex = regex;

	fx = data->cx;
	fy = screen_hsize(data->backing) - data->oy + data->cy;

	screen_init(&ss, screen_write_strlen("%s", str), 1, 0);
	screen_write_start(&ctx, &ss);
	screen_write_nputs(&ctx, -1, &grid_default_cell, "%s", str);
	screen_write_stop(&ctx);

	wrapflag = options_get_number(wp->window->options, "wrap-search");
	cis = window_copy_is_lowercase(str);

	keys = options_get_number(wp->window->options, "mode-keys");

	if (direction) {
		/*
		 * Behave according to mode-keys. If it is emacs, search forward
		 * leaves the cursor after the match. If it is vi, the cursor
		 * remains at the beginning of the match, regardless of
		 * direction, which means that we need to start the next search
		 * after the term the cursor is currently on when searching
		 * forward.
		 */
		if (keys == MODEKEY_VI) {
			if (data->searchmark != NULL)
				window_copy_move_after_search_mark(data, &fx,
				    &fy, wrapflag);
			else {
				/*
				 * When there are no search marks, start the
				 * search after the current cursor position.
				 */
				window_copy_move_right(s, &fx, &fy, wrapflag);
			}
		}
		endline = gd->hsize + gd->sy - 1;
	}
	else {
		window_copy_move_left(s, &fx, &fy, wrapflag);
		endline = 0;
	}

	found = window_copy_search_jump(wme, gd, ss.grid, fx, fy, endline, cis,
	    wrapflag, direction, regex);
	if (found) {
		window_copy_search_marks(wme, &ss, regex, visible_only);
		fx = data->cx;
		fy = screen_hsize(data->backing) - data->oy + data->cy;

		/*
		 * When searching forward, if the cursor is not at the beginning
		 * of the mark, search again.
		 */
		if (direction &&
		    window_copy_search_mark_at(data, fx, fy, &at) == 0 &&
		    at > 0 &&
		    data->searchmark[at] == data->searchmark[at - 1]) {
			window_copy_move_after_search_mark(data, &fx, &fy,
			    wrapflag);
			window_copy_search_jump(wme, gd, ss.grid, fx,
			    fy, endline, cis, wrapflag, direction,
			    regex);
			fx = data->cx;
			fy = screen_hsize(data->backing) - data->oy + data->cy;
		}

		if (direction) {
			/*
			 * When in Emacs mode, position the cursor just after
			 * the mark.
			 */
			if (keys == MODEKEY_EMACS) {
				window_copy_move_after_search_mark(data, &fx,
				    &fy, wrapflag);
				data->cx = fx;
				data->cy = fy - screen_hsize(data->backing) +
				    data-> oy;
			}
		}
		else {
			/*
			 * When searching backward, position the cursor at the
			 * beginning of the mark.
			 */
			if (window_copy_search_mark_at(data, fx, fy,
			        &start) == 0) {
				while (window_copy_search_mark_at(data, fx, fy,
				           &at) == 0 &&
				       data->searchmark[at] ==
				           data->searchmark[start]) {
					data->cx = fx;
					data->cy = fy -
					    screen_hsize(data->backing) +
					    data-> oy;
					if (at == 0)
						break;

					window_copy_move_left(s, &fx, &fy, 0);
				}
			}
		}
	}
	window_copy_redraw_screen(wme);

	screen_free(&ss);
	return (found);
}

static void
window_copy_visible_lines(struct window_copy_mode_data *data, u_int *start,
    u_int *end)
{
	struct grid		*gd = data->backing->grid;
	const struct grid_line	*gl;

	for (*start = gd->hsize - data->oy; *start > 0; (*start)--) {
		gl = grid_peek_line(gd, (*start) - 1);
		if (~gl->flags & GRID_LINE_WRAPPED)
			break;
	}
	*end = gd->hsize - data->oy + gd->sy;
}

static int
window_copy_search_mark_at(struct window_copy_mode_data *data, u_int px,
    u_int py, u_int *at)
{
	struct screen	*s = data->backing;
	struct grid	*gd = s->grid;

	if (py < gd->hsize - data->oy)
		return (-1);
	if (py > gd->hsize - data->oy + gd->sy - 1)
		return (-1);
	*at = ((py - (gd->hsize - data->oy)) * gd->sx) + px;
	return (0);
}

static int
window_copy_search_marks(struct window_mode_entry *wme, struct screen *ssp,
    int regex, int visible_only)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = data->backing, ss;
	struct screen_write_ctx		 ctx;
	struct grid			*gd = s->grid;
	int				 found, cis, stopped = 0;
	int				 cflags = REG_EXTENDED;
	u_int				 px, py, i, b, nfound = 0, width;
	u_int				 ssize = 1, start, end;
	char				*sbuf;
	regex_t				 reg;
	uint64_t			 stop = 0, tstart, t;

	if (ssp == NULL) {
		width = screen_write_strlen("%s", data->searchstr);
		screen_init(&ss, width, 1, 0);
		screen_write_start(&ctx, &ss);
		screen_write_nputs(&ctx, -1, &grid_default_cell, "%s",
		    data->searchstr);
		screen_write_stop(&ctx);
		ssp = &ss;
	} else
		width = screen_size_x(ssp);

	cis = window_copy_is_lowercase(data->searchstr);

	if (regex) {
		sbuf = xmalloc(ssize);
		sbuf[0] = '\0';
		sbuf = window_copy_stringify(ssp->grid, 0, 0, ssp->grid->sx,
		    sbuf, &ssize);
		if (cis)
			cflags |= REG_ICASE;
		if (regcomp(&reg, sbuf, cflags) != 0) {
			free(sbuf);
			return (0);
		}
		free(sbuf);
	}
	tstart = get_timer();

	if (visible_only)
		window_copy_visible_lines(data, &start, &end);
	else {
		start = 0;
		end = gd->hsize + gd->sy;
		stop = get_timer() + WINDOW_COPY_SEARCH_ALL_TIMEOUT;
	}

again:
	free(data->searchmark);
	data->searchmark = xcalloc(gd->sx, gd->sy);
	data->searchgen = 1;

	for (py = start; py < end; py++) {
		px = 0;
		for (;;) {
			if (regex) {
				found = window_copy_search_lr_regex(gd,
				    &px, &width, py, px, gd->sx, &reg);
				if (!found)
					break;
			} else {
				found = window_copy_search_lr(gd, ssp->grid,
				    &px, py, px, gd->sx, cis);
				if (!found)
					break;
			}
			nfound++;

			if (window_copy_search_mark_at(data, px, py, &b) == 0) {
				if (b + width > gd->sx * gd->sy)
					width = (gd->sx * gd->sy) - b;
				for (i = b; i < b + width; i++) {
					if (data->searchmark[i] != 0)
						continue;
					data->searchmark[i] = data->searchgen;
				}
				if (data->searchgen == UCHAR_MAX)
					data->searchgen = 1;
				else
					data->searchgen++;
			}
			px += width;
		}

		t = get_timer();
		if (t - tstart > WINDOW_COPY_SEARCH_TIMEOUT) {
			data->timeout = 1;
			break;
		}
		if (stop != 0 && t > stop) {
			stopped = 1;
			break;
		}
	}
	if (data->timeout) {
		window_copy_clear_marks(wme);
		goto out;
	}

	if (stopped && stop != 0) {
		/* Try again but just the visible context. */
		window_copy_visible_lines(data, &start, &end);
		stop = 0;
		goto again;
	}

	if (!visible_only) {
		if (stopped) {
			if (nfound > 1000)
				data->searchcount = 1000;
			else if (nfound > 100)
				data->searchcount = 100;
			else if (nfound > 10)
				data->searchcount = 10;
			else
				data->searchcount = -1;
			data->searchmore = 1;
		} else {
			data->searchcount = nfound;
			data->searchmore = 0;
		}
	}

out:
	if (ssp == &ss)
		screen_free(&ss);
	if (regex)
		regfree(&reg);
	return (1);
}

static void
window_copy_clear_marks(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;

	free(data->searchmark);
	data->searchmark = NULL;
}

static int
window_copy_search_up(struct window_mode_entry *wme, int regex)
{
	return (window_copy_search(wme, 0, regex));
}

static int
window_copy_search_down(struct window_mode_entry *wme, int regex)
{
	return (window_copy_search(wme, 1, regex));
}

static void
window_copy_goto_line(struct window_mode_entry *wme, const char *linestr)
{
	struct window_copy_mode_data	*data = wme->data;
	const char			*errstr;
	int				 lineno;

	lineno = strtonum(linestr, -1, INT_MAX, &errstr);
	if (errstr != NULL)
		return;
	if (lineno < 0 || (u_int)lineno > screen_hsize(data->backing))
		lineno = screen_hsize(data->backing);

	data->oy = lineno;
	window_copy_update_selection(wme, 1, 0);
	window_copy_redraw_screen(wme);
}

static void
window_copy_match_start_end(struct window_copy_mode_data *data, u_int at,
    u_int *start, u_int *end)
{
	struct grid	*gd = data->backing->grid;
	u_int		 last = (gd->sy * gd->sx) - 1;
	u_char		 mark = data->searchmark[at];

	*start = *end = at;
	while (*start != 0 && data->searchmark[*start] == mark)
		(*start)--;
	if (data->searchmark[*start] != mark)
		(*start)++;
	while (*end != last && data->searchmark[*end] == mark)
		(*end)++;
	if (data->searchmark[*end] != mark)
		(*end)--;
}

static char *
window_copy_match_at_cursor(struct window_copy_mode_data *data)
{
	struct grid	*gd = data->backing->grid;
	struct grid_cell gc;
	u_int		 at, start, end, cy, px, py;
	u_int		 sx = screen_size_x(data->backing);
	char		*buf = NULL;
	size_t		 len = 0;

	if (data->searchmark == NULL)
		return (NULL);

	cy = screen_hsize(data->backing) - data->oy + data->cy;
	if (window_copy_search_mark_at(data, data->cx, cy, &at) != 0)
		return (NULL);
	if (data->searchmark[at] == 0) {
		/* Allow one position after the match. */
		if (at == 0 || data->searchmark[--at] == 0)
			return (NULL);
	}
	window_copy_match_start_end(data, at, &start, &end);

	/*
	 * Cells will not be set in the marked array unless they are valid text
	 * and wrapping will be taken care of, so we can just copy.
 	 */
	for (at = start; at <= end; at++) {
		py = at / sx;
		px = at - (py * sx);

		grid_get_cell(gd, px, gd->hsize + py - data->oy, &gc);
		buf = xrealloc(buf, len + gc.data.size + 1);
		memcpy(buf + len, gc.data.data, gc.data.size);
		len += gc.data.size;
	}
	if (len != 0)
		buf[len] = '\0';
	return (buf);
}

static void
window_copy_update_style(struct window_mode_entry *wme, u_int fx, u_int fy,
    struct grid_cell *gc, const struct grid_cell *mgc,
    const struct grid_cell *cgc, const struct grid_cell *mkgc)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	u_int				 mark, start, end, cy, cursor, current;
	int				 inv = 0, found = 0;
	int				 keys;

	if (data->showmark && fy == data->my) {
		gc->attr = mkgc->attr;
		if (fx == data->mx)
			inv = 1;
		if (inv) {
			gc->fg = mkgc->bg;
			gc->bg = mkgc->fg;
		}
		else {
			gc->fg = mkgc->fg;
			gc->bg = mkgc->bg;
		}
	}

	if (data->searchmark == NULL)
		return;

	if (window_copy_search_mark_at(data, fx, fy, &current) != 0)
		return;
	mark = data->searchmark[current];
	if (mark == 0)
		return;

	cy = screen_hsize(data->backing) - data->oy + data->cy;
	if (window_copy_search_mark_at(data, data->cx, cy, &cursor) == 0) {
		keys = options_get_number(wp->window->options, "mode-keys");
		if (cursor != 0 &&
		    keys == MODEKEY_EMACS &&
		    data->searchdirection) {
			if (data->searchmark[cursor - 1] == mark) {
				cursor--;
				found = 1;
			}
		} else if (data->searchmark[cursor] == mark)
			found = 1;
		if (found) {
			window_copy_match_start_end(data, cursor, &start, &end);
			if (current >= start && current <= end) {
				gc->attr = cgc->attr;
				if (inv) {
					gc->fg = cgc->bg;
					gc->bg = cgc->fg;
				}
				else {
					gc->fg = cgc->fg;
					gc->bg = cgc->bg;
				}
				return;
			}
		}
	}

	gc->attr = mgc->attr;
	if (inv) {
		gc->fg = mgc->bg;
		gc->bg = mgc->fg;
	}
	else {
		gc->fg = mgc->fg;
		gc->bg = mgc->bg;
	}
}

static void
window_copy_write_one(struct window_mode_entry *wme,
    struct screen_write_ctx *ctx, u_int py, u_int fy, u_int nx,
    const struct grid_cell *mgc, const struct grid_cell *cgc,
    const struct grid_cell *mkgc)
{
	struct window_copy_mode_data	*data = wme->data;
	struct grid			*gd = data->backing->grid;
	struct grid_cell		 gc;
	u_int		 		 fx;

	screen_write_cursormove(ctx, 0, py, 0);
	for (fx = 0; fx < nx; fx++) {
		grid_get_cell(gd, fx, fy, &gc);
		if (fx + gc.data.width <= nx) {
			window_copy_update_style(wme, fx, fy, &gc, mgc, cgc,
			    mkgc);
			screen_write_cell(ctx, &gc);
		}
	}
}

static void
window_copy_write_line(struct window_mode_entry *wme,
    struct screen_write_ctx *ctx, u_int py)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	struct options			*oo = wp->window->options;
	struct grid_cell		 gc, mgc, cgc, mkgc;
	char				 hdr[512];
	size_t				 size = 0;
	u_int				 hsize = screen_hsize(data->backing);

	style_apply(&gc, oo, "mode-style", NULL);
	gc.flags |= GRID_FLAG_NOPALETTE;
	style_apply(&mgc, oo, "copy-mode-match-style", NULL);
	mgc.flags |= GRID_FLAG_NOPALETTE;
	style_apply(&cgc, oo, "copy-mode-current-match-style", NULL);
	cgc.flags |= GRID_FLAG_NOPALETTE;
	style_apply(&mkgc, oo, "copy-mode-mark-style", NULL);
	mkgc.flags |= GRID_FLAG_NOPALETTE;

	if (py == 0 && s->rupper < s->rlower && !data->hide_position) {
		if (data->searchmark == NULL) {
			if (data->timeout) {
				size = xsnprintf(hdr, sizeof hdr,
				    "(timed out) [%u/%u]", data->oy, hsize);
			} else {
				size = xsnprintf(hdr, sizeof hdr,
				    "[%u/%u]", data->oy, hsize);
			}
		} else {
			if (data->searchcount == -1) {
				size = xsnprintf(hdr, sizeof hdr,
				    "[%u/%u]", data->oy, hsize);
			} else {
				size = xsnprintf(hdr, sizeof hdr,
				    "(%d%s results) [%u/%u]", data->searchcount,
				    data->searchmore ? "+" : "", data->oy,
				    hsize);
			}
		}
		if (size > screen_size_x(s))
			size = screen_size_x(s);
		screen_write_cursormove(ctx, screen_size_x(s) - size, 0, 0);
		screen_write_puts(ctx, &gc, "%s", hdr);
	} else
		size = 0;

	if (size < screen_size_x(s)) {
		window_copy_write_one(wme, ctx, py, hsize - data->oy + py,
		    screen_size_x(s) - size, &mgc, &cgc, &mkgc);
	}

	if (py == data->cy && data->cx == screen_size_x(s)) {
		screen_write_cursormove(ctx, screen_size_x(s) - 1, py, 0);
		screen_write_putc(ctx, &grid_default_cell, '$');
	}
}

static void
window_copy_write_lines(struct window_mode_entry *wme,
    struct screen_write_ctx *ctx, u_int py, u_int ny)
{
	u_int	yy;

	for (yy = py; yy < py + ny; yy++)
		window_copy_write_line(wme, ctx, py);
}

static void
window_copy_redraw_selection(struct window_mode_entry *wme, u_int old_y)
{
	struct window_copy_mode_data	*data = wme->data;
	struct grid			*gd = data->backing->grid;
	u_int				 new_y, start, end;

	new_y = data->cy;
	if (old_y <= new_y) {
		start = old_y;
		end = new_y;
	} else {
		start = new_y;
		end = old_y;
	}

	/*
	 * In word selection mode the first word on the line below the cursor
	 * might be selected, so add this line to the redraw area.
	 */
	if (data->selflag == SEL_WORD) {
		/* Last grid line in data coordinates. */
		if (end < gd->sy + data->oy - 1)
			end++;
	}
	window_copy_redraw_lines(wme, start, end - start + 1);
}

static void
window_copy_redraw_lines(struct window_mode_entry *wme, u_int py, u_int ny)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start_pane(&ctx, wp, NULL);
	for (i = py; i < py + ny; i++)
		window_copy_write_line(wme, &ctx, i);
	screen_write_cursormove(&ctx, data->cx, data->cy, 0);
	screen_write_stop(&ctx);
}

static void
window_copy_redraw_screen(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;

	window_copy_redraw_lines(wme, 0, screen_size_y(&data->screen));
}

static void
window_copy_synchronize_cursor_end(struct window_mode_entry *wme, int begin,
    int no_reset)
{
	struct window_copy_mode_data	*data = wme->data;
	u_int				 xx, yy;

	xx = data->cx;
	yy = screen_hsize(data->backing) + data->cy - data->oy;
	switch (data->selflag) {
	case SEL_WORD:
		if (no_reset)
			break;
		begin = 0;
		if (data->dy > yy || (data->dy == yy && data->dx > xx)) {
			/* Right to left selection. */
			window_copy_cursor_previous_word_pos(wme, data->ws, 0,
			    &xx, &yy);
			begin = 1;

			/* Reset the end. */
			data->endselx = data->endselrx;
			data->endsely = data->endselry;
		} else {
			/* Left to right selection. */
			if (xx >= window_copy_find_length(wme, yy) ||
			    !window_copy_in_set(wme, xx + 1, yy, data->ws))
				window_copy_cursor_next_word_end_pos(wme,
				    data->ws, &xx, &yy);

			/* Reset the start. */
			data->selx = data->selrx;
			data->sely = data->selry;
		}
		break;
	case SEL_LINE:
		if (no_reset)
			break;
		begin = 0;
		if (data->dy > yy) {
			/* Right to left selection. */
			xx = 0;
			begin = 1;

			/* Reset the end. */
			data->endselx = data->endselrx;
			data->endsely = data->endselry;
		} else {
			/* Left to right selection. */
			if (yy < data->endselry)
				yy = data->endselry;
			xx = window_copy_find_length(wme, yy);

			/* Reset the start. */
			data->selx = data->selrx;
			data->sely = data->selry;
		}
		break;
	case SEL_CHAR:
		break;
	}
	if (begin) {
		data->selx = xx;
		data->sely = yy;
	} else {
		data->endselx = xx;
		data->endsely = yy;
	}
}

static void
window_copy_synchronize_cursor(struct window_mode_entry *wme, int no_reset)
{
	struct window_copy_mode_data	*data = wme->data;

	switch (data->cursordrag) {
	case CURSORDRAG_ENDSEL:
		window_copy_synchronize_cursor_end(wme, 0, no_reset);
		break;
	case CURSORDRAG_SEL:
		window_copy_synchronize_cursor_end(wme, 1, no_reset);
		break;
	case CURSORDRAG_NONE:
		break;
	}
}

static void
window_copy_update_cursor(struct window_mode_entry *wme, u_int cx, u_int cy)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int				 old_cx, old_cy;

	old_cx = data->cx; old_cy = data->cy;
	data->cx = cx; data->cy = cy;
	if (old_cx == screen_size_x(s))
		window_copy_redraw_lines(wme, old_cy, 1);
	if (data->cx == screen_size_x(s))
		window_copy_redraw_lines(wme, data->cy, 1);
	else {
		screen_write_start_pane(&ctx, wp, NULL);
		screen_write_cursormove(&ctx, data->cx, data->cy, 0);
		screen_write_stop(&ctx);
	}
}

static void
window_copy_start_selection(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;

	data->selx = data->cx;
	data->sely = screen_hsize(data->backing) + data->cy - data->oy;

	data->endselx = data->selx;
	data->endsely = data->sely;

	data->cursordrag = CURSORDRAG_ENDSEL;

	window_copy_set_selection(wme, 1, 0);
}

static int
window_copy_adjust_selection(struct window_mode_entry *wme, u_int *selx,
    u_int *sely)
{
	struct window_copy_mode_data	*data = wme->data;
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
window_copy_update_selection(struct window_mode_entry *wme, int may_redraw,
    int no_reset)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;

	if (s->sel == NULL && data->lineflag == LINE_SEL_NONE)
		return (0);
	return (window_copy_set_selection(wme, may_redraw, no_reset));
}

static int
window_copy_set_selection(struct window_mode_entry *wme, int may_redraw,
    int no_reset)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	struct options			*oo = wp->window->options;
	struct grid_cell		 gc;
	u_int				 sx, sy, cy, endsx, endsy;
	int				 startrelpos, endrelpos;

	window_copy_synchronize_cursor(wme, no_reset);

	/* Adjust the selection. */
	sx = data->selx;
	sy = data->sely;
	startrelpos = window_copy_adjust_selection(wme, &sx, &sy);

	/* Adjust the end of selection. */
	endsx = data->endselx;
	endsy = data->endsely;
	endrelpos = window_copy_adjust_selection(wme, &endsx, &endsy);

	/* Selection is outside of the current screen */
	if (startrelpos == endrelpos &&
	    startrelpos != WINDOW_COPY_REL_POS_ON_SCREEN) {
		screen_hide_selection(s);
		return (0);
	}

	/* Set colours and selection. */
	style_apply(&gc, oo, "mode-style", NULL);
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
				window_copy_redraw_lines(wme, sy, cy - sy + 1);
			else
				window_copy_redraw_lines(wme, cy, sy - cy + 1);
		} else {
			if (endsy < cy) {
				window_copy_redraw_lines(wme, endsy,
				    cy - endsy + 1);
			} else {
				window_copy_redraw_lines(wme, cy,
				    endsy - cy + 1);
			}
		}
	}

	return (1);
}

static void *
window_copy_get_selection(struct window_mode_entry *wme, size_t *len)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	char				*buf;
	size_t				 off;
	u_int				 i, xx, yy, sx, sy, ex, ey, ey_last;
	u_int				 firstsx, lastex, restex, restsx, selx;
	int				 keys;

	if (data->screen.sel == NULL && data->lineflag == LINE_SEL_NONE) {
		buf = window_copy_match_at_cursor(data);
		if (buf != NULL)
			*len = strlen(buf);
		else
			*len = 0;
		return (buf);
	}

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
	ey_last = window_copy_find_length(wme, ey);
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
		window_copy_copy_line(wme, &buf, &off, i,
		    (i == sy ? firstsx : restsx),
		    (i == ey ? lastex : restex));
	}

	/* Don't bother if no data. */
	if (off == 0) {
		free(buf);
		*len = 0;
		return (NULL);
	}
	 /* Remove final \n (unless at end in vi mode). */
	if (keys == MODEKEY_EMACS || lastex <= ey_last) {
		if (~grid_get_line(data->backing->grid, ey)->flags &
		    GRID_LINE_WRAPPED || lastex != ey_last)
		off -= 1;
	}
	*len = off;
	return (buf);
}

static void
window_copy_copy_buffer(struct window_mode_entry *wme, const char *prefix,
    void *buf, size_t len)
{
	struct window_pane	*wp = wme->wp;
	struct screen_write_ctx	 ctx;

	if (options_get_number(global_options, "set-clipboard") != 0) {
		screen_write_start_pane(&ctx, wp, NULL);
		screen_write_setselection(&ctx, buf, len);
		screen_write_stop(&ctx);
		notify_pane("pane-set-clipboard", wp);
	}

	paste_add(prefix, buf, len);
}

static void *
window_copy_pipe_run(struct window_mode_entry *wme, struct session *s,
    const char *cmd, size_t *len)
{
	void		*buf;
	struct job	*job;

	buf = window_copy_get_selection(wme, len);
	if (cmd == NULL || *cmd == '\0')
		cmd = options_get_string(global_options, "copy-command");
	if (cmd != NULL && *cmd != '\0') {
		job = job_run(cmd, 0, NULL, s, NULL, NULL, NULL, NULL, NULL,
		    JOB_NOWAIT, -1, -1);
		bufferevent_write(job_get_event(job), buf, *len);
	}
	return (buf);
}

static void
window_copy_pipe(struct window_mode_entry *wme, struct session *s,
    const char *cmd)
{
	size_t	len;

	window_copy_pipe_run(wme, s, cmd, &len);
}

static void
window_copy_copy_pipe(struct window_mode_entry *wme, struct session *s,
    const char *prefix, const char *cmd)
{
	void	*buf;
	size_t	 len;

	buf = window_copy_pipe_run(wme, s, cmd, &len);
	if (buf != NULL)
		window_copy_copy_buffer(wme, prefix, buf, len);
}

static void
window_copy_copy_selection(struct window_mode_entry *wme, const char *prefix)
{
	char	*buf;
	size_t	 len;

	buf = window_copy_get_selection(wme, &len);
	if (buf != NULL)
		window_copy_copy_buffer(wme, prefix, buf, len);
}

static void
window_copy_append_selection(struct window_mode_entry *wme)
{
	struct window_pane		*wp = wme->wp;
	char				*buf;
	struct paste_buffer		*pb;
	const char			*bufdata, *bufname = NULL;
	size_t				 len, bufsize;
	struct screen_write_ctx		 ctx;

	buf = window_copy_get_selection(wme, &len);
	if (buf == NULL)
		return;

	if (options_get_number(global_options, "set-clipboard") != 0) {
		screen_write_start_pane(&ctx, wp, NULL);
		screen_write_setselection(&ctx, buf, len);
		screen_write_stop(&ctx);
		notify_pane("pane-set-clipboard", wp);
	}

	pb = paste_get_top(&bufname);
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
window_copy_copy_line(struct window_mode_entry *wme, char **buf, size_t *off,
    u_int sy, u_int sx, u_int ex)
{
	struct window_copy_mode_data	*data = wme->data;
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
		xx = window_copy_find_length(wme, sy);
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
window_copy_clear_selection(struct window_mode_entry *wme)
{
	struct window_copy_mode_data   *data = wme->data;
	u_int				px, py;

	screen_clear_selection(&data->screen);

	data->cursordrag = CURSORDRAG_NONE;
	data->lineflag = LINE_SEL_NONE;
	data->selflag = SEL_CHAR;

	py = screen_hsize(data->backing) + data->cy - data->oy;
	px = window_copy_find_length(wme, py);
	if (data->cx > px)
		window_copy_update_cursor(wme, px, data->cy);
}

static int
window_copy_in_set(struct window_mode_entry *wme, u_int px, u_int py,
    const char *set)
{
	struct window_copy_mode_data	*data = wme->data;
	struct grid_cell		 gc;

	grid_get_cell(data->backing->grid, px, py, &gc);
	if (gc.flags & GRID_FLAG_PADDING)
		return (0);
	return (utf8_cstrhas(set, &gc.data));
}

static u_int
window_copy_find_length(struct window_mode_entry *wme, u_int py)
{
	struct window_copy_mode_data	*data = wme->data;

	return (grid_line_length(data->backing->grid, py));
}

static void
window_copy_cursor_start_of_line(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_start_of_line(&gr, 1);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_up(wme, hsize, data->oy, oldy, px, py);
}

static void
window_copy_cursor_back_to_indentation(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_back_to_indentation(&gr);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_up(wme, hsize, data->oy, oldy, px, py);
}

static void
window_copy_cursor_end_of_line(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py =  hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	if (data->screen.sel != NULL && data->rectflag)
		grid_reader_cursor_end_of_line(&gr, 1, 1);
	else
		grid_reader_cursor_end_of_line(&gr, 1, 0);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_down(wme, hsize, screen_size_y(back_s),
	    data->oy, oldy, px, py, 0);
}

static void
window_copy_other_end(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
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

	window_copy_update_selection(wme, 1, 1);
	window_copy_redraw_screen(wme);
}

static void
window_copy_cursor_left(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_left(&gr, 1);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_up(wme, hsize, data->oy, oldy, px, py);
}

static void
window_copy_cursor_right(struct window_mode_entry *wme, int all)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_right(&gr, 1, all);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_down(wme, hsize, screen_size_y(back_s),
	    data->oy, oldy, px, py, 0);
}

static void
window_copy_cursor_up(struct window_mode_entry *wme, int scroll_only)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	u_int				 ox, oy, px, py;
	int				 norectsel;

	norectsel = data->screen.sel == NULL || !data->rectflag;
	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wme, oy);
	if (norectsel && data->cx != ox) {
		data->lastcx = data->cx;
		data->lastsx = ox;
	}

	if (data->lineflag == LINE_SEL_LEFT_RIGHT && oy == data->sely)
		window_copy_other_end(wme);

	if (scroll_only || data->cy == 0) {
		if (norectsel)
			data->cx = data->lastcx;
		window_copy_scroll_down(wme, 1);
		if (scroll_only) {
			if (data->cy == screen_size_y(s) - 1)
				window_copy_redraw_lines(wme, data->cy, 1);
			else
				window_copy_redraw_lines(wme, data->cy, 2);
		}
	} else {
		if (norectsel) {
			window_copy_update_cursor(wme, data->lastcx,
			    data->cy - 1);
		} else
			window_copy_update_cursor(wme, data->cx, data->cy - 1);
		if (window_copy_update_selection(wme, 1, 0)) {
			if (data->cy == screen_size_y(s) - 1)
				window_copy_redraw_lines(wme, data->cy, 1);
			else
				window_copy_redraw_lines(wme, data->cy, 2);
		}
	}

	if (norectsel) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wme, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
		{
			window_copy_update_cursor(wme, px, data->cy);
			if (window_copy_update_selection(wme, 1, 0))
				window_copy_redraw_lines(wme, data->cy, 1);
		}
	}

	if (data->lineflag == LINE_SEL_LEFT_RIGHT)
	{
		py = screen_hsize(data->backing) + data->cy - data->oy;
		if (data->rectflag)
			px = screen_size_x(data->backing);
		else
			px = window_copy_find_length(wme, py);
		window_copy_update_cursor(wme, px, data->cy);
		if (window_copy_update_selection(wme, 1, 0))
			window_copy_redraw_lines(wme, data->cy, 1);
	}
	else if (data->lineflag == LINE_SEL_RIGHT_LEFT)
	{
		window_copy_update_cursor(wme, 0, data->cy);
		if (window_copy_update_selection(wme, 1, 0))
			window_copy_redraw_lines(wme, data->cy, 1);
	}
}

static void
window_copy_cursor_down(struct window_mode_entry *wme, int scroll_only)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	u_int				 ox, oy, px, py;
	int				 norectsel;

	norectsel = data->screen.sel == NULL || !data->rectflag;
	oy = screen_hsize(data->backing) + data->cy - data->oy;
	ox = window_copy_find_length(wme, oy);
	if (norectsel && data->cx != ox) {
		data->lastcx = data->cx;
		data->lastsx = ox;
	}

	if (data->lineflag == LINE_SEL_RIGHT_LEFT && oy == data->endsely)
		window_copy_other_end(wme);

	if (scroll_only || data->cy == screen_size_y(s) - 1) {
		if (norectsel)
			data->cx = data->lastcx;
		window_copy_scroll_up(wme, 1);
		if (scroll_only && data->cy > 0)
			window_copy_redraw_lines(wme, data->cy - 1, 2);
	} else {
		if (norectsel) {
			window_copy_update_cursor(wme, data->lastcx,
			    data->cy + 1);
		} else
			window_copy_update_cursor(wme, data->cx, data->cy + 1);
		if (window_copy_update_selection(wme, 1, 0))
			window_copy_redraw_lines(wme, data->cy - 1, 2);
	}

	if (norectsel) {
		py = screen_hsize(data->backing) + data->cy - data->oy;
		px = window_copy_find_length(wme, py);
		if ((data->cx >= data->lastsx && data->cx != px) ||
		    data->cx > px)
		{
			window_copy_update_cursor(wme, px, data->cy);
			if (window_copy_update_selection(wme, 1, 0))
				window_copy_redraw_lines(wme, data->cy, 1);
		}
	}

	if (data->lineflag == LINE_SEL_LEFT_RIGHT)
	{
		py = screen_hsize(data->backing) + data->cy - data->oy;
		if (data->rectflag)
			px = screen_size_x(data->backing);
		else
			px = window_copy_find_length(wme, py);
		window_copy_update_cursor(wme, px, data->cy);
		if (window_copy_update_selection(wme, 1, 0))
			window_copy_redraw_lines(wme, data->cy, 1);
	}
	else if (data->lineflag == LINE_SEL_RIGHT_LEFT)
	{
		window_copy_update_cursor(wme, 0, data->cy);
		if (window_copy_update_selection(wme, 1, 0))
			window_copy_redraw_lines(wme, data->cy, 1);
	}
}

static void
window_copy_cursor_jump(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx + 1;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	if (grid_reader_cursor_jump(&gr, data->jumpchar)) {
		grid_reader_get_cursor(&gr, &px, &py);
		window_copy_acquire_cursor_down(wme, hsize,
		    screen_size_y(back_s), data->oy, oldy, px, py, 0);
	}
}

static void
window_copy_cursor_jump_back(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_left(&gr, 0);
	if (grid_reader_cursor_jump_back(&gr, data->jumpchar)) {
		grid_reader_get_cursor(&gr, &px, &py);
		window_copy_acquire_cursor_up(wme, hsize, data->oy, oldy, px,
		    py);
	}
}

static void
window_copy_cursor_jump_to(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx + 2;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	if (grid_reader_cursor_jump(&gr, data->jumpchar)) {
		grid_reader_cursor_left(&gr, 1);
		grid_reader_get_cursor(&gr, &px, &py);
		window_copy_acquire_cursor_down(wme, hsize,
		    screen_size_y(back_s), data->oy, oldy, px, py, 0);
	}
}

static void
window_copy_cursor_jump_to_back(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_left(&gr, 0);
	grid_reader_cursor_left(&gr, 0);
	if (grid_reader_cursor_jump_back(&gr, data->jumpchar)) {
		grid_reader_cursor_right(&gr, 1, 0);
		grid_reader_get_cursor(&gr, &px, &py);
		window_copy_acquire_cursor_up(wme, hsize, data->oy, oldy, px,
		    py);
	}
}

static void
window_copy_cursor_next_word(struct window_mode_entry *wme,
    const char *separators)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py =  hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_next_word(&gr, separators);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_down(wme, hsize, screen_size_y(back_s),
	    data->oy, oldy, px, py, 0);
}

/* Compute the next place where a word ends. */
static void
window_copy_cursor_next_word_end_pos(struct window_mode_entry *wme,
    const char *separators, u_int *ppx, u_int *ppy)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct options			*oo = wp->window->options;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, hsize;
	int				 keys;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py =  hsize + data->cy - data->oy;

	grid_reader_start(&gr, back_s->grid, px, py);
	keys = options_get_number(oo, "mode-keys");
	if (keys == MODEKEY_VI && !grid_reader_in_set(&gr, separators))
		grid_reader_cursor_right(&gr, 0, 0);
	grid_reader_cursor_next_word_end(&gr, separators);
	if (keys == MODEKEY_VI)
		grid_reader_cursor_left(&gr, 1);
	grid_reader_get_cursor(&gr, &px, &py);
	*ppx = px;
	*ppy = py;
}

/* Move to the next place where a word ends. */
static void
window_copy_cursor_next_word_end(struct window_mode_entry *wme,
    const char *separators, int no_reset)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct options			*oo = wp->window->options;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;
	int				 keys;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py =  hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	keys = options_get_number(oo, "mode-keys");
	if (keys == MODEKEY_VI && !grid_reader_in_set(&gr, separators))
		grid_reader_cursor_right(&gr, 0, 0);
	grid_reader_cursor_next_word_end(&gr, separators);
	if (keys == MODEKEY_VI)
		grid_reader_cursor_left(&gr, 1);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_down(wme, hsize, screen_size_y(back_s),
	    data->oy, oldy, px, py, no_reset);
}

/* Compute the previous place where a word begins. */
static void
window_copy_cursor_previous_word_pos(struct window_mode_entry *wme,
    const char *separators, int already, u_int *ppx, u_int *ppy)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_previous_word(&gr, separators, already);
	grid_reader_get_cursor(&gr, &px, &py);
	*ppx = px;
	*ppy = py;
}

/* Move to the previous place where a word begins. */
static void
window_copy_cursor_previous_word(struct window_mode_entry *wme,
    const char *separators, int already)
{
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*back_s = data->backing;
	struct grid_reader		 gr;
	u_int				 px, py, oldy, hsize;

	px = data->cx;
	hsize = screen_hsize(back_s);
	py = hsize + data->cy - data->oy;
	oldy = data->cy;

	grid_reader_start(&gr, back_s->grid, px, py);
	grid_reader_cursor_previous_word(&gr, separators, already);
	grid_reader_get_cursor(&gr, &px, &py);
	window_copy_acquire_cursor_up(wme, hsize, data->oy, oldy, px, py);
}

static void
window_copy_scroll_up(struct window_mode_entry *wme, u_int ny)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->oy < ny)
		ny = data->oy;
	if (ny == 0)
		return;
	data->oy -= ny;

	if (data->searchmark != NULL && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 1);
	window_copy_update_selection(wme, 0, 0);

	screen_write_start_pane(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0, 0);
	screen_write_deleteline(&ctx, ny, 8);
	window_copy_write_lines(wme, &ctx, screen_size_y(s) - ny, ny);
	window_copy_write_line(wme, &ctx, 0);
	if (screen_size_y(s) > 1)
		window_copy_write_line(wme, &ctx, 1);
	if (screen_size_y(s) > 3)
		window_copy_write_line(wme, &ctx, screen_size_y(s) - 2);
	if (s->sel != NULL && screen_size_y(s) > ny)
		window_copy_write_line(wme, &ctx, screen_size_y(s) - ny - 1);
	screen_write_cursormove(&ctx, data->cx, data->cy, 0);
	screen_write_stop(&ctx);
}

static void
window_copy_scroll_down(struct window_mode_entry *wme, u_int ny)
{
	struct window_pane		*wp = wme->wp;
	struct window_copy_mode_data	*data = wme->data;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (ny > screen_hsize(data->backing))
		return;

	if (data->oy > screen_hsize(data->backing) - ny)
		ny = screen_hsize(data->backing) - data->oy;
	if (ny == 0)
		return;
	data->oy += ny;

	if (data->searchmark != NULL && !data->timeout)
		window_copy_search_marks(wme, NULL, data->searchregex, 1);
	window_copy_update_selection(wme, 0, 0);

	screen_write_start_pane(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0, 0);
	screen_write_insertline(&ctx, ny, 8);
	window_copy_write_lines(wme, &ctx, 0, ny);
	if (s->sel != NULL && screen_size_y(s) > ny)
		window_copy_write_line(wme, &ctx, ny);
	else if (ny == 1) /* nuke position */
		window_copy_write_line(wme, &ctx, 1);
	screen_write_cursormove(&ctx, data->cx, data->cy, 0);
	screen_write_stop(&ctx);
}

static void
window_copy_rectangle_set(struct window_mode_entry *wme, int rectflag)
{
	struct window_copy_mode_data	*data = wme->data;
	u_int				 px, py;

	data->rectflag = rectflag;

	py = screen_hsize(data->backing) + data->cy - data->oy;
	px = window_copy_find_length(wme, py);
	if (data->cx > px)
		window_copy_update_cursor(wme, px, data->cy);

	window_copy_update_selection(wme, 1, 0);
	window_copy_redraw_screen(wme);
}

static void
window_copy_move_mouse(struct mouse_event *m)
{
	struct window_pane		*wp;
	struct window_mode_entry	*wme;
	u_int				 x, y;

	wp = cmd_mouse_pane(m, NULL, NULL);
	if (wp == NULL)
		return;
	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL)
		return;
	if (wme->mode != &window_copy_mode && wme->mode != &window_view_mode)
		return;

	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return;

	window_copy_update_cursor(wme, x, y);
}

void
window_copy_start_drag(struct client *c, struct mouse_event *m)
{
	struct window_pane		*wp;
	struct window_mode_entry	*wme;
	struct window_copy_mode_data	*data;
	u_int				 x, y, yg;

	if (c == NULL)
		return;

	wp = cmd_mouse_pane(m, NULL, NULL);
	if (wp == NULL)
		return;
	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL)
		return;
	if (wme->mode != &window_copy_mode && wme->mode != &window_view_mode)
		return;

	if (cmd_mouse_at(wp, m, &x, &y, 1) != 0)
		return;

	c->tty.mouse_drag_update = window_copy_drag_update;
	c->tty.mouse_drag_release = window_copy_drag_release;

	data = wme->data;
	yg = screen_hsize(data->backing) + y - data->oy;
	if (x < data->selrx || x > data->endselrx || yg != data->selry)
		data->selflag = SEL_CHAR;
	switch (data->selflag) {
	case SEL_WORD:
		if (data->ws != NULL) {
			window_copy_update_cursor(wme, x, y);
			window_copy_cursor_previous_word_pos(wme, data->ws, 0,
			    &x, &y);
			y -= screen_hsize(data->backing) - data->oy;
		}
		window_copy_update_cursor(wme, x, y);
		break;
	case SEL_LINE:
		window_copy_update_cursor(wme, 0, y);
		break;
	case SEL_CHAR:
		window_copy_update_cursor(wme, x, y);
		window_copy_start_selection(wme);
		break;
	}

	window_copy_redraw_screen(wme);
	window_copy_drag_update(c, m);
}

static void
window_copy_drag_update(struct client *c, struct mouse_event *m)
{
	struct window_pane		*wp;
	struct window_mode_entry	*wme;
	struct window_copy_mode_data	*data;
	u_int				 x, y, old_cx, old_cy;
	struct timeval			 tv = {
		.tv_usec = WINDOW_COPY_DRAG_REPEAT_TIME
	};

	if (c == NULL)
		return;

	wp = cmd_mouse_pane(m, NULL, NULL);
	if (wp == NULL)
		return;
	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL)
		return;
	if (wme->mode != &window_copy_mode && wme->mode != &window_view_mode)
		return;

	data = wme->data;
	evtimer_del(&data->dragtimer);

	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return;
	old_cx = data->cx;
	old_cy = data->cy;

	window_copy_update_cursor(wme, x, y);
	if (window_copy_update_selection(wme, 1, 0))
		window_copy_redraw_selection(wme, old_cy);
	if (old_cy != data->cy || old_cx == data->cx) {
		if (y == 0) {
			evtimer_add(&data->dragtimer, &tv);
			window_copy_cursor_up(wme, 1);
		} else if (y == screen_size_y(&data->screen) - 1) {
			evtimer_add(&data->dragtimer, &tv);
			window_copy_cursor_down(wme, 1);
		}
	}
}

static void
window_copy_drag_release(struct client *c, struct mouse_event *m)
{
	struct window_pane		*wp;
	struct window_mode_entry	*wme;
	struct window_copy_mode_data	*data;

	if (c == NULL)
		return;

	wp = cmd_mouse_pane(m, NULL, NULL);
	if (wp == NULL)
		return;
	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL)
		return;
	if (wme->mode != &window_copy_mode && wme->mode != &window_view_mode)
		return;

	data = wme->data;
	evtimer_del(&data->dragtimer);
}

static void
window_copy_jump_to_mark(struct window_mode_entry *wme)
{
	struct window_copy_mode_data	*data = wme->data;
	u_int				 tmx, tmy;

	tmx = data->cx;
	tmy = screen_hsize(data->backing) + data->cy - data->oy;
	data->cx = data->mx;
	if (data->my < screen_hsize(data->backing)) {
		data->cy = 0;
		data->oy = screen_hsize(data->backing) - data->my;
	} else {
		data->cy = data->my - screen_hsize(data->backing);
		data->oy = 0;
	}
	data->mx = tmx;
	data->my = tmy;
	data->showmark = 1;
	window_copy_update_selection(wme, 0, 0);
	window_copy_redraw_screen(wme);
}

/* Scroll up if the cursor went off the visible screen. */
static void
window_copy_acquire_cursor_up(struct window_mode_entry *wme, u_int hsize,
    u_int oy, u_int oldy, u_int px, u_int py)
{
	u_int	cy, yy, ny, nd;

	yy = hsize - oy;
	if (py < yy) {
		ny = yy - py;
		cy = 0;
		nd = 1;
	} else {
		ny = 0;
		cy = py - yy;
		nd = oldy - cy + 1;
	}
	while (ny > 0) {
		window_copy_cursor_up(wme, 1);
		ny--;
	}
	window_copy_update_cursor(wme, px, cy);
	if (window_copy_update_selection(wme, 1, 0))
		window_copy_redraw_lines(wme, cy, nd);
}

/* Scroll down if the cursor went off the visible screen. */
static void
window_copy_acquire_cursor_down(struct window_mode_entry *wme, u_int hsize,
    u_int sy, u_int oy, u_int oldy, u_int px, u_int py, int no_reset)
{
	u_int	cy, yy, ny, nd;

	cy = py - hsize + oy;
	yy = sy - 1;
	if (cy > yy) {
		ny = cy - yy;
		oldy = yy;
		nd = 1;
	} else {
		ny = 0;
		nd = cy - oldy + 1;
	}
	while (ny > 0) {
	  window_copy_cursor_down(wme, 1);
	  ny--;
	}
	if (cy > yy)
		window_copy_update_cursor(wme, px, yy);
	else
		window_copy_update_cursor(wme, px, cy);
	if (window_copy_update_selection(wme, 1, no_reset))
		window_copy_redraw_lines(wme, oldy, nd);
}
