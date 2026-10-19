/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Alexandre Fiori <fiorix@gmail.com>
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Convert windows and panes to JSON and create them again from it.
 *
 * The file is an object with a version and one window. Keys the loader does
 * not know are ignored and an empty string is written by leaving its key out,
 * so a missing string key means an empty string.
 */

#define STATE_VERSION 1

/*
 * Names of screen modes. MODE_SYNC is left out because it is ended by a timer
 * that a new pane does not have.
 */
static const struct {
	const char	*name;
	int		 mode;
} state_modes[] = {
	{ "cursor", MODE_CURSOR },
	{ "insert", MODE_INSERT },
	{ "keypad-cursor", MODE_KCURSOR },
	{ "keypad", MODE_KKEYPAD },
	{ "wrap", MODE_WRAP },
	{ "mouse-standard", MODE_MOUSE_STANDARD },
	{ "mouse-button", MODE_MOUSE_BUTTON },
	{ "cursor-blinking", MODE_CURSOR_BLINKING },
	{ "mouse-utf8", MODE_MOUSE_UTF8 },
	{ "mouse-sgr", MODE_MOUSE_SGR },
	{ "bracket-paste", MODE_BRACKETPASTE },
	{ "focus", MODE_FOCUSON },
	{ "mouse-all", MODE_MOUSE_ALL },
	{ "origin", MODE_ORIGIN },
	{ "crlf", MODE_CRLF },
	{ "extended-keys", MODE_KEYS_EXTENDED },
	{ "cursor-very-visible", MODE_CURSOR_VERY_VISIBLE },
	{ "cursor-blinking-set", MODE_CURSOR_BLINKING_SET },
	{ "extended-keys-2", MODE_KEYS_EXTENDED_2 },
	{ "theme-updates", MODE_THEME_UPDATES }
};

/* Names of cursor styles, in the order of enum screen_cursor_style. */
static const char *state_cursor_styles[] = {
	"default", "block", "underline", "bar"
};

/* Number of a hyperlink in a state file and the number it has now. */
struct state_link {
	u_int	id;
	u_int	inner;
};

/* Screen read from a state file. */
struct state_screen {
	struct grid			*grid;
	u_int				 cx;
	u_int				 cy;

	int				 flags;
#define STATE_STYLE 0x1
#define STATE_COLOUR 0x2
#define STATE_MODES 0x4
#define STATE_SAVED 0x8
	u_int				 cstyle;
	int				 ccolour;
	int				 mode;
	u_int				 rupper;
	u_int				 rlower;
	bitstr_t			*tabs;
	char				**titles;
	u_int				 ntitles;
	char				*path;
	struct input_saved_cursor	 saved;

	struct hyperlinks		*links;
	struct state_link		*link_map;
	u_int				 nlinks;

	struct grid			*saved_grid;
	u_int				 saved_cx;
	u_int				 saved_cy;
	struct grid_cell		 saved_cell;
};

/* Pane read from a state file. */
struct state_pane {
	char			 *title;
	char			 *cwd;
	int			  argc;
	char			**argv;
	int			  zoomed;
	int			  float_over_zoom;
	struct options		 *options;
	struct colour_palette	 *palette;
	struct state_screen	  screen;
};

/* Window read from a state file. */
struct state_window {
	char			*name;
	char			*layout;
	u_int			 sx;
	u_int			 sy;
	int			 zoomed;
	struct options		*options;
	u_int			 npanes;
	struct state_pane	*panes;
};

/* Start a key in an object, with a comma unless it is the first. */
static void
state_add_key(struct evbuffer *evb, int *comma, const char *key)
{
	if (*comma)
		evbuffer_add(evb, ",", 1);
	evbuffer_add_printf(evb, "\"%s\":", key);
	*comma = 1;
}

/* Add a boolean key, or nothing if it is false. */
static void
state_add_boolean(struct evbuffer *evb, int *comma, const char *key, int value)
{
	if (!value)
		return;
	state_add_key(evb, comma, key);
	evbuffer_add(evb, "true", 4);
}

/* Add a string key, or nothing if the string is empty. */
static void
state_add_string(struct evbuffer *evb, int *comma, const char *key,
    const char *value)
{
	if (value == NULL || *value == '\0')
		return;
	state_add_key(evb, comma, key);
	json_write_string(evb, value);
}

/*
 * Add a string tmux keeps encoded with vis(3), such as a name, title, path or
 * hyperlink, decoded so it is written as it was given.
 */
static void
state_add_name(struct evbuffer *evb, int *comma, const char *key,
    const char *value)
{
	char	*decoded;
	size_t	 size = strlen(value) + 1;

	decoded = xmalloc(size);
	if (strunvis(decoded, value) == -1)
		strlcpy(decoded, value, size);
	state_add_string(evb, comma, key, decoded);
	free(decoded);
}

/* Check if two cells have the same style. */
static int
state_same_style(const struct grid_cell *gc1, const struct grid_cell *gc2)
{
	return (gc1->attr == gc2->attr &&
	    gc1->fg == gc2->fg &&
	    gc1->bg == gc2->bg &&
	    gc1->us == gc2->us &&
	    gc1->link == gc2->link);
}

/* Add a number key. */
static void
state_add_number(struct evbuffer *evb, int *comma, const char *key, u_int n)
{
	state_add_key(evb, comma, key);
	evbuffer_add_printf(evb, "%u", n);
}

/* Add a colour key, or nothing if it is the default. */
static void
state_add_colour(struct evbuffer *evb, int *comma, const char *key, int c)
{
	if (c != 8)
		state_add_string(evb, comma, key, colour_tostring(c));
}

/* Add the style keys of a cell, leaving out those that are the default. */
static void
state_add_style(struct evbuffer *evb, int *comma, const struct grid_cell *gc)
{
	if (gc->attr != 0) {
		state_add_string(evb, comma, "a",
		    attributes_tostring(gc->attr));
	}
	state_add_colour(evb, comma, "f", gc->fg);
	state_add_colour(evb, comma, "b", gc->bg);
	state_add_colour(evb, comma, "u", gc->us);
}

/*
 * Add a run of cells with the same style, or nothing if it is empty. A run that
 * does not start where the width of the cell before puts it has its column in
 * "x". A tab covers a number of columns that cannot be worked out from its
 * text, so it has its own run with the columns in "s".
 */
static void
state_add_run(struct evbuffer *evb, struct evbuffer *text,
    const struct grid_cell *gc, u_int x, u_int span, int *comma)
{
	int	rcomma = 0;

	if (EVBUFFER_LENGTH(text) == 0)
		return;
	evbuffer_add(text, "", 1);

	if (*comma)
		evbuffer_add(evb, ",", 1);
	*comma = 1;
	evbuffer_add(evb, "{", 1);
	state_add_string(evb, &rcomma, "t", EVBUFFER_DATA(text));
	if (x != UINT_MAX)
		state_add_number(evb, &rcomma, "x", x);
	if (span != 0)
		state_add_number(evb, &rcomma, "s", span);
	state_add_style(evb, &rcomma, gc);
	if (gc->link != 0)
		state_add_number(evb, &rcomma, "l", gc->link);
	evbuffer_add(evb, "}", 1);

	evbuffer_drain(text, EVBUFFER_LENGTH(text));
}

/* Check if a cell would be added to the cell before it when read back. */
static int
state_would_combine(const struct grid_cell *last, const struct grid_cell *gc)
{
	struct utf8_data	ud;
	enum utf8_state		more;
	u_int			i;
	int			force_wide;

	if (gc->data.size < 2 || (gc->flags & GRID_FLAG_TAB))
		return (0);
	if ((more = utf8_open(&ud, gc->data.data[0])) != UTF8_MORE)
		return (0);
	for (i = 1; i < gc->data.size && more == UTF8_MORE; i++)
		more = utf8_append(&ud, gc->data.data[i]);
	if (more != UTF8_DONE)
		return (0);
	return (utf8_combine(&last->data, &ud, &force_wide) == 1);
}

/*
 * Add the OSC 133 marks of a line, with a key for each mark holding the column
 * where it was made.
 */
static void
state_add_marks(struct evbuffer *evb, int *comma, struct grid_line *gl)
{
	struct osc133_data	*od = &gl->osc133_data;
	int			 mcomma = 0;

	if ((gl->flags & GRID_LINE_OSC133_FLAGS) == 0)
		return;
	state_add_key(evb, comma, "m");
	evbuffer_add(evb, "{", 1);
	if (gl->flags & GRID_LINE_START_PROMPT)
		state_add_number(evb, &mcomma, "p", od->prompt_col);
	if (gl->flags & GRID_LINE_SECOND_PROMPT)
		state_add_number(evb, &mcomma, "q", od->prompt_col);
	if (gl->flags & GRID_LINE_START_COMMAND)
		state_add_number(evb, &mcomma, "c", od->cmd_col);
	if (gl->flags & GRID_LINE_START_OUTPUT)
		state_add_number(evb, &mcomma, "s", od->out_start_col);
	if (gl->flags & GRID_LINE_END_OUTPUT) {
		state_add_number(evb, &mcomma, "e", od->out_end_col);
		state_add_number(evb, &mcomma, "x", od->exit_status);
	}
	evbuffer_add(evb, "}", 1);
}

/*
 * Add a grid line. Cells are written up to the last one used. Padding is left
 * out because the width of the cell before implies it; any that does not
 * follow a wide cell shows as a gap before the "x" of the next run.
 */
static void
state_add_line(struct evbuffer *evb, struct evbuffer *text, struct grid *gd,
    u_int py)
{
	struct grid_line	*gl = grid_get_line(gd, py);
	struct grid_cell	 gc, style, last;
	u_int			 px, next = 0, x = UINT_MAX;
	int			 comma = 0, rcomma = 0;

	evbuffer_add(evb, "{", 1);
	state_add_boolean(evb, &comma, "w", gl->flags & GRID_LINE_WRAPPED);
	state_add_marks(evb, &comma, gl);
	if (gl->cellused != 0) {
		state_add_key(evb, &comma, "c");
		evbuffer_add(evb, "[", 1);
		memcpy(&style, &grid_default_cell, sizeof style);
		memcpy(&last, &grid_default_cell, sizeof last);
		for (px = 0; px < gl->cellused; px++) {
			grid_get_cell(gd, px, py, &gc);
			if (gc.flags & GRID_FLAG_PADDING)
				continue;

			/*
			 * Start a new run if the style changes, if the cell is
			 * not where the cell before puts it, or if reading it
			 * back would add it to the cell before.
			 */
			if (!state_same_style(&gc, &style) ||
			    px != next ||
			    (gc.flags & GRID_FLAG_TAB) ||
			    state_would_combine(&last, &gc)) {
				state_add_run(evb, text, &style, x, 0, &rcomma);
				memcpy(&style, &gc, sizeof style);
				x = (px != next) ? px : UINT_MAX;
			}
			if (gc.flags & GRID_FLAG_TAB) {
				evbuffer_add(text, "\t", 1);
				state_add_run(evb, text, &gc, x, gc.data.width,
				    &rcomma);
				x = UINT_MAX;
			} else
				evbuffer_add(text, gc.data.data, gc.data.size);
			memcpy(&last, &gc, sizeof last);
			next = px + gc.data.width;
		}
		state_add_run(evb, text, &style, x, 0, &rcomma);
		evbuffer_add(evb, "]", 1);
	}
	evbuffer_add(evb, "}", 1);
}

/* Add some lines of a grid as an array, or nothing if there are none. */
static void
state_add_lines(struct evbuffer *evb, struct evbuffer *text, int *comma,
    const char *key, struct grid *gd, u_int py, u_int ny)
{
	u_int	yy;

	if (ny == 0)
		return;
	state_add_key(evb, comma, key);
	evbuffer_add(evb, "[", 1);
	for (yy = py; yy < py + ny; yy++) {
		if (yy != py)
			evbuffer_add(evb, ",", 1);
		state_add_line(evb, text, gd, yy);
	}
	evbuffer_add(evb, "]", 1);
}

/* Add a cursor position. */
static void
state_add_cursor(struct evbuffer *evb, int *comma, const char *key, u_int cx,
    u_int cy)
{
	state_add_key(evb, comma, key);
	evbuffer_add_printf(evb, "{\"x\":%u,\"y\":%u}", cx, cy);
}

/* Add the modes as a list of names. */
static void
state_add_modes(struct evbuffer *evb, int *comma, int mode)
{
	u_int	i;
	int	mcomma = 0;

	state_add_key(evb, comma, "modes");
	evbuffer_add(evb, "[", 1);
	for (i = 0; i < nitems(state_modes); i++) {
		if (~mode & state_modes[i].mode)
			continue;
		if (mcomma)
			evbuffer_add(evb, ",", 1);
		mcomma = 1;
		evbuffer_add_printf(evb, "{\"name\":\"%s\"}",
		    state_modes[i].name);
	}
	evbuffer_add(evb, "]", 1);
}

/* Add the columns with a tab stop, or nothing if they are the default. */
static void
state_add_tabs(struct evbuffer *evb, int *comma, struct screen *s)
{
	u_int	x;
	int	tcomma = 0;

	/* The default is a tab stop every eight columns. */
	for (x = 0; x < screen_size_x(s); x++) {
		if ((bit_test(s->tabs, x) != 0) != (x != 0 && x % 8 == 0))
			break;
	}
	if (x == screen_size_x(s))
		return;

	state_add_key(evb, comma, "tabs");
	evbuffer_add(evb, "[", 1);
	for (x = 0; x < screen_size_x(s); x++) {
		if (!bit_test(s->tabs, x))
			continue;
		if (tcomma)
			evbuffer_add(evb, ",", 1);
		tcomma = 1;
		evbuffer_add_printf(evb, "{\"x\":%u}", x);
	}
	evbuffer_add(evb, "]", 1);
}

/* Add the title stack, the most recently pushed first. */
static void
state_add_titles(struct evbuffer *evb, int *comma, struct screen *s)
{
	const char	*title;
	u_int		 i;
	int		 tcomma;

	if (s->ntitles == 0)
		return;
	state_add_key(evb, comma, "titles");
	evbuffer_add(evb, "[", 1);
	for (i = 0; (title = screen_get_title(s, i)) != NULL; i++) {
		if (i != 0)
			evbuffer_add(evb, ",", 1);
		evbuffer_add(evb, "{", 1);
		tcomma = 0;
		state_add_name(evb, &tcomma, "title", title);
		evbuffer_add(evb, "}", 1);
	}
	evbuffer_add(evb, "]", 1);
}

/*
 * Add the cursor, cell and character sets saved by DECSC, or nothing if they
 * are as a new pane has them.
 */
static void
state_add_saved(struct evbuffer *evb, int *comma, struct input_ctx *ictx)
{
	struct input_saved_cursor	isc;
	int				scomma = 1;

	input_get_saved_cursor(ictx, &isc);
	if (isc.cx == 0 &&
	    isc.cy == 0 &&
	    (~isc.mode & MODE_ORIGIN) &&
	    !isc.set &&
	    !isc.g0set &&
	    !isc.g1set &&
	    grid_cells_equal(&isc.cell, &grid_default_cell))
		return;

	state_add_key(evb, comma, "saved");
	evbuffer_add_printf(evb, "{\"x\":%u,\"y\":%u", isc.cx, isc.cy);
	state_add_boolean(evb, &scomma, "origin", isc.mode & MODE_ORIGIN);
	state_add_boolean(evb, &scomma, "g1", isc.set);
	state_add_boolean(evb, &scomma, "g0-acs", isc.g0set);
	state_add_boolean(evb, &scomma, "g1-acs", isc.g1set);
	state_add_style(evb, &scomma, &isc.cell);
	evbuffer_add(evb, "}", 1);
}

/* Compare hyperlink numbers for qsort. */
static int
state_link_cmp(const void *a, const void *b)
{
	u_int	la = *(const u_int *)a, lb = *(const u_int *)b;

	if (la < lb)
		return (-1);
	return (la > lb);
}

/* Add the hyperlinks used in a grid to a list. */
static void
state_find_links(struct grid *gd, u_int **links, u_int *nlinks)
{
	struct grid_line	*gl;
	struct grid_cell	 gc;
	u_int			 px, py;

	for (py = 0; py < gd->hsize + gd->sy; py++) {
		gl = grid_get_line(gd, py);
		if (~gl->flags & GRID_LINE_HYPERLINK)
			continue;
		for (px = 0; px < gl->cellused; px++) {
			grid_get_cell(gd, px, py, &gc);
			if (gc.link == 0)
				continue;
			if (*nlinks != 0 && (*links)[*nlinks - 1] == gc.link)
				continue;
			*links = xreallocarray(*links, *nlinks + 1,
			    sizeof **links);
			(*links)[(*nlinks)++] = gc.link;
		}
	}
}

/*
 * Add the hyperlinks used by a screen, which runs refer to by number. A link
 * that has been dropped because there were too many is left out.
 */
static void
state_add_links(struct evbuffer *evb, int *comma, struct screen *s)
{
	u_int		*links = NULL, nlinks = 0, i;
	const char	*uri, *name;
	int		 lcomma = 0, ncomma;

	state_find_links(s->grid, &links, &nlinks);
	if (s->saved_grid != NULL)
		state_find_links(s->saved_grid, &links, &nlinks);
	if (nlinks == 0)
		return;
	qsort(links, nlinks, sizeof *links, state_link_cmp);

	state_add_key(evb, comma, "links");
	evbuffer_add(evb, "[", 1);
	for (i = 0; i < nlinks; i++) {
		if (i != 0 && links[i] == links[i - 1])
			continue;
		if (!hyperlinks_get(s->hyperlinks, links[i], &uri, &name,
		    NULL))
			continue;
		if (lcomma)
			evbuffer_add(evb, ",", 1);
		lcomma = 1;
		ncomma = 0;
		evbuffer_add(evb, "{", 1);
		state_add_number(evb, &ncomma, "id", links[i]);
		state_add_name(evb, &ncomma, "uri", uri);
		state_add_name(evb, &ncomma, "name", name);
		evbuffer_add(evb, "}", 1);
	}
	evbuffer_add(evb, "]", 1);
	free(links);
}

/*
 * Add a pane's screen. While the alternate screen is on, the history stays with
 * the visible lines and the normal screen's lines are kept aside, as
 * screen_alternate_on leaves them; "alternate" holds those. The cursor and cell
 * to go back to when the alternate screen ends are kept even after it has
 * ended.
 */
static void
state_add_screen(struct evbuffer *evb, struct window_pane *wp)
{
	struct screen	*s = &wp->base;
	struct grid	*gd = s->grid, *sgd = s->saved_grid;
	struct evbuffer	*text;
	int		 comma = 1, acomma = 1;

	text = evbuffer_new();
	if (text == NULL)
		fatalx("out of memory");

	evbuffer_add_printf(evb, "{\"sx\":%u,\"sy\":%u", gd->sx, gd->sy);
	state_add_cursor(evb, &comma, "cursor", s->cx, s->cy);
	if (s->cstyle != SCREEN_CURSOR_DEFAULT) {
		state_add_string(evb, &comma, "cursor-style",
		    state_cursor_styles[s->cstyle]);
	}
	if (s->ccolour != -1) {
		state_add_string(evb, &comma, "cursor-colour",
		    colour_tostring(s->ccolour));
	}
	state_add_modes(evb, &comma, s->mode);
	if (s->rupper != 0 || s->rlower != gd->sy - 1) {
		state_add_key(evb, &comma, "region");
		evbuffer_add_printf(evb, "{\"upper\":%u,\"lower\":%u}",
		    s->rupper, s->rlower);
	}
	state_add_tabs(evb, &comma, s);
	state_add_titles(evb, &comma, s);
	if (s->path != NULL)
		state_add_name(evb, &comma, "path", s->path);
	if (wp->ictx != NULL)
		state_add_saved(evb, &comma, wp->ictx);
	state_add_lines(evb, text, &comma, "history", gd, 0, gd->hsize);
	state_add_lines(evb, text, &comma, "lines", gd, gd->hsize, gd->sy);
	if (sgd != NULL) {
		state_add_key(evb, &comma, "alternate");
		evbuffer_add_printf(evb, "{\"sx\":%u,\"sy\":%u", sgd->sx,
		    sgd->sy);
		state_add_lines(evb, text, &acomma, "lines", sgd, 0, sgd->sy);
		evbuffer_add(evb, "}", 1);
	}
	if (s->saved_cx != UINT_MAX && s->saved_cy != UINT_MAX) {
		state_add_key(evb, &comma, "alternate-cursor");
		evbuffer_add_printf(evb, "{\"x\":%u,\"y\":%u", s->saved_cx,
		    s->saved_cy);
		acomma = 1;
		state_add_style(evb, &acomma, &s->saved_cell);
		evbuffer_add(evb, "}", 1);
	}
	state_add_links(evb, &comma, s);
	evbuffer_add(evb, "}", 1);

	evbuffer_free(text);
}

/* Add the colours set in a pane's palette, or nothing if there are none. */
static void
state_add_palette(struct evbuffer *evb, int *comma, struct colour_palette *p)
{
	u_int	i;
	int	pcomma = 0, ccomma = 0;

	if (p->fg == 8 && p->bg == 8 && p->palette == NULL)
		return;
	state_add_key(evb, comma, "palette");
	evbuffer_add(evb, "{", 1);
	state_add_colour(evb, &pcomma, "fg", p->fg);
	state_add_colour(evb, &pcomma, "bg", p->bg);
	if (p->palette != NULL) {
		state_add_key(evb, &pcomma, "colours");
		evbuffer_add(evb, "[", 1);
		for (i = 0; i < 256; i++) {
			if (p->palette[i] == -1)
				continue;
			if (ccomma)
				evbuffer_add(evb, ",", 1);
			ccomma = 1;
			evbuffer_add_printf(evb,
			    "{\"index\":%u,\"colour\":\"%s\"}", i,
			    colour_tostring(p->palette[i]));
		}
		evbuffer_add(evb, "]", 1);
	}
	evbuffer_add(evb, "}", 1);
}

/* Add one option or array item. */
static void
state_add_option(struct evbuffer *evb, int *comma, const char *name,
    const char *key, const char *value)
{
	int	ocomma = 0;

	if (*comma)
		evbuffer_add(evb, ",", 1);
	*comma = 1;
	evbuffer_add(evb, "{", 1);
	state_add_string(evb, &ocomma, "name", name);
	state_add_string(evb, &ocomma, "index", key);
	state_add_string(evb, &ocomma, "value", value);
	evbuffer_add(evb, "}", 1);
}

/*
 * Add the options set on an object itself rather than inherited, one entry for
 * each array item and one with no index for an array with no items.
 */
static void
state_add_options(struct evbuffer *evb, int *comma, struct options *oo)
{
	struct options_entry		*o;
	struct options_array_item	*a;
	const char			*name, *key;
	char				*value;
	int				 ocomma = 0;

	state_add_key(evb, comma, "options");
	evbuffer_add(evb, "[", 1);
	for (o = options_first(oo); o != NULL; o = options_next(o)) {
		name = options_name(o);
		if (!options_is_array(o)) {
			value = options_to_string(o, NULL, 0);
			state_add_option(evb, &ocomma, name, NULL, value);
			free(value);
			continue;
		}
		if ((a = options_array_first(o)) == NULL)
			state_add_option(evb, &ocomma, name, NULL, NULL);
		for (; a != NULL; a = options_array_next(a)) {
			key = options_array_item_key(a);
			value = options_to_string(o, key, 0);
			state_add_option(evb, &ocomma, name, key, value);
			free(value);
		}
	}
	evbuffer_add(evb, "]", 1);
}

/* Add a pane. */
static void
state_add_pane(struct evbuffer *evb, struct window_pane *wp)
{
	const char	*cwd;
	int		 comma = 0, i;

	evbuffer_add(evb, "{", 1);
	state_add_name(evb, &comma, "title", wp->base.title);
	if ((cwd = osdep_get_cwd(wp->fd)) == NULL)
		cwd = wp->cwd;
	state_add_string(evb, &comma, "cwd", cwd);
	state_add_boolean(evb, &comma, "zoomed", wp->flags & PANE_ZOOMED);
	state_add_boolean(evb, &comma, "float-over-zoom",
	    wp->flags & PANE_FLOATOVERZOOM);
	if (wp->argc != 0) {
		state_add_key(evb, &comma, "command");
		evbuffer_add(evb, "[", 1);
		for (i = 0; i < wp->argc; i++) {
			if (i != 0)
				evbuffer_add(evb, ",", 1);
			evbuffer_add(evb, "{", 1);
			if (*wp->argv[i] != '\0') {
				evbuffer_add(evb, "\"arg\":", 6);
				json_write_string(evb, wp->argv[i]);
			}
			evbuffer_add(evb, "}", 1);
		}
		evbuffer_add(evb, "]", 1);
	}
	state_add_options(evb, &comma, wp->options);
	state_add_palette(evb, &comma, &wp->palette);
	state_add_key(evb, &comma, "screen");
	state_add_screen(evb, wp);
	evbuffer_add(evb, "}", 1);
}

/* Add a window. */
static int
state_add_window(struct evbuffer *evb, struct window *w, char **cause)
{
	struct window_pane	*wp;
	struct layout_cell	*lcroot;
	char			*layout;
	int			 comma = 0;

	/* A zoomed window keeps its full layout aside. */
	if (w->saved_layout_root != NULL)
		lcroot = w->saved_layout_root;
	else
		lcroot = w->layout_root;
	if ((layout = layout_dump(w, lcroot, 0)) == NULL) {
		xasprintf(cause, "can't save layout of window @%u", w->id);
		return (-1);
	}

	evbuffer_add(evb, "{", 1);
	state_add_name(evb, &comma, "name", w->name);
	state_add_key(evb, &comma, "layout");
	evbuffer_add(evb, layout, strlen(layout));
	free(layout);
	state_add_boolean(evb, &comma, "zoomed", w->flags & WINDOW_ZOOMED);
	state_add_options(evb, &comma, w->options);
	state_add_key(evb, &comma, "panes");
	evbuffer_add(evb, "[", 1);
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp != TAILQ_FIRST(&w->panes))
			evbuffer_add(evb, ",", 1);
		state_add_pane(evb, wp);
	}
	evbuffer_add(evb, "]}", 2);
	return (0);
}

/* Save a window as JSON. */
int
state_save_window(struct window *w, struct evbuffer *evb, char **cause)
{
	evbuffer_add_printf(evb, "{\"version\":%d,\"window\":", STATE_VERSION);
	if (state_add_window(evb, w, cause) != 0)
		return (-1);
	evbuffer_add(evb, "}\n", 2);
	return (0);
}

/* Get a string key, which is empty if missing. */
static int
state_get_string(struct json_node *jn, const char *key, char **out,
    char **cause)
{
	const char	*s;

	if (json_find(jn, key) == NULL) {
		*out = xstrdup("");
		return (0);
	}
	if (json_find_string(jn, key, &s, cause) != 0)
		return (-1);
	if ((*out = json_decode_string(s)) == NULL) {
		xasprintf(cause, "key \"%s\" has an invalid string", key);
		return (-1);
	}
	return (0);
}

/* Get an optional boolean key, which is false if missing. */
static int
state_get_boolean(struct json_node *jn, const char *key, int *out,
    char **cause)
{
	*out = 0;
	if (json_find(jn, key) == NULL)
		return (0);
	return (json_find_boolean(jn, key, out, cause));
}

/* Get a number key that must be within a range. */
static int
state_get_number(struct json_node *jn, const char *key, int64_t min,
    int64_t max, u_int *out, char **cause)
{
	int64_t	n;

	if (json_find_number(jn, key, &n, cause) != 0)
		return (-1);
	if (n < min || n > max) {
		xasprintf(cause, "key \"%s\" is out of range", key);
		return (-1);
	}
	*out = n;
	return (0);
}

/* Get a cursor position with x up to maxx and y up to maxy. */
static int
state_get_cursor(struct json_node *jn, const char *key, u_int maxx,
    u_int maxy, u_int *cx, u_int *cy, char **cause)
{
	struct json_node	*cursor;

	if (json_find_object(jn, key, &cursor, cause) != 0)
		return (-1);
	if (state_get_number(cursor, "x", 0, maxx, cx, cause) != 0)
		return (-1);
	return (state_get_number(cursor, "y", 0, maxy, cy, cause));
}

/* Get a colour key, which is the default if missing. */
static int
state_get_colour(struct json_node *jn, const char *key, int *out,
    char **cause)
{
	char	*s;

	if (state_get_string(jn, key, &s, cause) != 0)
		return (-1);
	if (*s == '\0')
		*out = 8;
	else if ((*out = colour_fromstring(s)) == -1) {
		xasprintf(cause, "invalid colour \"%s\"", s);
		free(s);
		return (-1);
	}
	free(s);
	return (0);
}

/* Get the style of a run. */
static int
state_get_style(struct json_node *jn, struct grid_cell *gc, char **cause)
{
	char	*s;
	int	 attr = 0;

	memcpy(gc, &grid_default_cell, sizeof *gc);

	if (state_get_string(jn, "a", &s, cause) != 0)
		return (-1);
	if (*s != '\0' && (attr = attributes_fromstring(s)) == -1) {
		xasprintf(cause, "invalid attributes \"%s\"", s);
		free(s);
		return (-1);
	}
	free(s);
	gc->attr = attr;

	if (state_get_colour(jn, "f", &gc->fg, cause) != 0)
		return (-1);
	if (state_get_colour(jn, "b", &gc->bg, cause) != 0)
		return (-1);
	return (state_get_colour(jn, "u", &gc->us, cause));
}

/* Put a cell into a grid if it fits, with padding for a wide cell. */
static void
state_set_cell(struct grid *gd, u_int px, u_int py, const struct grid_cell *gc)
{
	u_int	xx;

	if (px + gc->data.width > gd->sx)
		return;
	grid_set_cell(gd, px, py, gc);
	for (xx = px + 1; xx < px + gc->data.width; xx++)
		grid_set_padding(gd, xx, py, gc->bg);
}

/* Compare hyperlinks read from a file by their number in the file. */
static int
state_link_map_cmp(const void *a, const void *b)
{
	const struct state_link	*la = a, *lb = b;

	if (la->id < lb->id)
		return (-1);
	return (la->id > lb->id);
}

/*
 * Read a run of cells into a grid line. Widths come from the current tables,
 * so they may differ from those of the saved line; cells are moved along to
 * fit, and any that end up past the edge are left out.
 */
static int
state_read_run(struct json_node *jn, struct state_screen *ss, struct grid *gd,
    u_int py, u_int *px, char **cause)
{
	struct grid_cell	 gc, cell;
	struct utf8_data	*ud;
	struct state_link	 find, *sl;
	char			*text = NULL;
	u_int			 x, span, i;
	int			 have = 0, force_wide;

	if (json_get_object(jn, &jn) != 0) {
		*cause = xstrdup("run is not an object");
		return (-1);
	}
	if (state_get_style(jn, &gc, cause) != 0)
		return (-1);
	if (json_find(jn, "l") != NULL) {
		if (state_get_number(jn, "l", 1, UINT_MAX, &find.id,
		    cause) != 0)
			return (-1);
		sl = bsearch(&find, ss->link_map, ss->nlinks, sizeof *sl,
		    state_link_map_cmp);
		if (sl != NULL)
			gc.link = sl->inner;
	}
	if (state_get_string(jn, "t", &text, cause) != 0)
		return (-1);
	if (*text == '\0') {
		*cause = xstrdup("run has no text");
		goto fail;
	}

	/*
	 * Only padding is left out of a line, so a gap before a run that says
	 * where it starts was padding.
	 */
	if (json_find(jn, "x") != NULL) {
		if (state_get_number(jn, "x", 0, gd->sx - 1, &x, cause) != 0)
			goto fail;
		for (; *px < x; (*px)++)
			grid_set_padding(gd, *px, py, 8);
		*px = x;
	}

	if (json_find(jn, "s") != NULL) {
		if (state_get_number(jn, "s", 1, sizeof gc.data.data, &span,
		    cause) != 0)
			goto fail;
		if (strcmp(text, "\t") != 0) {
			*cause = xstrdup("run with \"s\" is not a tab");
			goto fail;
		}
		grid_set_tab(&gc, span);
		state_set_cell(gd, *px, py, &gc);
		*px += span;
		free(text);
		return (0);
	}

	if (!utf8_isvalid(text)) {
		*cause = xstrdup("run has invalid text");
		goto fail;
	}
	ud = utf8_fromcstr(text);
	for (i = 0; ud[i].size != 0; i++) {
		if (have &&
		    ud[i].size > 1 &&
		    cell.data.size + ud[i].size <= sizeof cell.data.data &&
		    utf8_combine(&cell.data, &ud[i], &force_wide) == 1) {
			memcpy(cell.data.data + cell.data.size, ud[i].data,
			    ud[i].size);
			cell.data.size += ud[i].size;
			cell.data.have = cell.data.size;
			if (cell.data.width == 1 && force_wide)
				cell.data.width = 2;
			continue;
		}
		if (have) {
			state_set_cell(gd, *px, py, &cell);
			*px += cell.data.width;
		}
		memcpy(&cell, &gc, sizeof cell);
		utf8_copy(&cell.data, &ud[i]);
		if (cell.data.width == 0)
			cell.data.width = 1;
		have = 1;
	}
	free(ud);
	if (have) {
		state_set_cell(gd, *px, py, &cell);
		*px += cell.data.width;
	}
	free(text);
	return (0);

fail:
	free(text);
	return (-1);
}

/* Get an optional OSC 133 mark of a line. */
static int
state_get_mark(struct json_node *jn, const char *key, int flag, u_short *col,
    int *flags, char **cause)
{
	u_int	n;

	if (json_find(jn, key) == NULL)
		return (0);
	if (state_get_number(jn, key, 0, USHRT_MAX, &n, cause) != 0)
		return (-1);
	*col = n;
	*flags |= flag;
	return (0);
}

/* Read the OSC 133 marks of a line. */
static int
state_read_marks(struct json_node *jn, struct grid_line *gl, char **cause)
{
	struct osc133_data	 od;
	int			 flags = 0;
	u_int			 status = 0;

	if (json_find_object(jn, "m", &jn, cause) != 0)
		return (-1);
	memset(&od, 0, sizeof od);
	if (state_get_mark(jn, "p", GRID_LINE_START_PROMPT, &od.prompt_col,
	    &flags, cause) != 0 ||
	    state_get_mark(jn, "q", GRID_LINE_SECOND_PROMPT, &od.prompt_col,
	    &flags, cause) != 0 ||
	    state_get_mark(jn, "c", GRID_LINE_START_COMMAND, &od.cmd_col,
	    &flags, cause) != 0 ||
	    state_get_mark(jn, "s", GRID_LINE_START_OUTPUT, &od.out_start_col,
	    &flags, cause) != 0 ||
	    state_get_mark(jn, "e", GRID_LINE_END_OUTPUT, &od.out_end_col,
	    &flags, cause) != 0)
		return (-1);
	if (json_find(jn, "x") != NULL &&
	    state_get_number(jn, "x", 0, UCHAR_MAX, &status, cause) != 0)
		return (-1);
	od.exit_status = status;

	memcpy(&gl->osc133_data, &od, sizeof gl->osc133_data);
	gl->flags |= flags;
	return (0);
}

/* Read a grid line. */
static int
state_read_line(struct json_node *jn, struct state_screen *ss,
    struct grid *gd, u_int py, char **cause)
{
	struct json_node	*array, *member;
	int			 wrapped;
	u_int			 px = 0;

	if (json_get_object(jn, &jn) != 0) {
		*cause = xstrdup("line is not an object");
		return (-1);
	}
	if (state_get_boolean(jn, "w", &wrapped, cause) != 0)
		return (-1);
	if (json_find(jn, "c") != NULL) {
		if (json_find_array(jn, "c", &array, cause) != 0)
			return (-1);
		member = json_array_first(array);
		while (member != NULL) {
			if (state_read_run(member, ss, gd, py, &px,
			    cause) != 0)
				return (-1);
			member = json_array_next(member);
		}
	}
	if (wrapped)
		grid_get_line(gd, py)->flags |= GRID_LINE_WRAPPED;
	if (json_find(jn, "m") != NULL &&
	    state_read_marks(jn, grid_get_line(gd, py), cause) != 0)
		return (-1);
	return (0);
}

/* Count the members of an optional array key. */
static int
state_count_array(struct json_node *jn, const char *key, u_int *n,
    char **cause)
{
	struct json_node	*array, *member;

	*n = 0;
	if (json_find(jn, key) == NULL)
		return (0);
	if (json_find_array(jn, key, &array, cause) != 0)
		return (-1);
	member = json_array_first(array);
	while (member != NULL) {
		(*n)++;
		member = json_array_next(member);
	}
	return (0);
}

/* Read the lines in an optional array key into a grid from line py. */
static int
state_read_lines(struct json_node *jn, const char *key,
    struct state_screen *ss, struct grid *gd, u_int py, u_int skip,
    char **cause)
{
	struct json_node	*array, *member;
	u_int			 n = 0;

	if (json_find(jn, key) == NULL)
		return (0);
	json_find_array(jn, key, &array, NULL);
	member = json_array_first(array);
	while (member != NULL) {
		if (n >= skip &&
		    state_read_line(member, ss, gd, py + n - skip, cause) != 0)
			return (-1);
		n++;
		member = json_array_next(member);
	}
	return (0);
}

/*
 * Read the size and lines of a screen into a new grid. Only the newest of the
 * history lines are kept if there are more than hlimit.
 */
static struct grid *
state_read_grid(struct json_node *jn, struct state_screen *ss, u_int hlimit,
    char **cause)
{
	struct grid	*gd;
	u_int		 sx, sy, nhistory, nlines, hsize;

	if (state_get_number(jn, "sx", 1, PANE_MAXIMUM, &sx, cause) != 0)
		return (NULL);
	if (state_get_number(jn, "sy", 1, PANE_MAXIMUM, &sy, cause) != 0)
		return (NULL);
	if (state_count_array(jn, "history", &nhistory, cause) != 0)
		return (NULL);
	if (state_count_array(jn, "lines", &nlines, cause) != 0)
		return (NULL);
	if (nlines > sy) {
		xasprintf(cause, "screen has %u lines but height %u", nlines,
		    sy);
		return (NULL);
	}
	if (nhistory > hlimit)
		hsize = hlimit;
	else
		hsize = nhistory;

	gd = grid_create(sx, sy, hlimit);
	if (hsize != 0) {
		grid_adjust_lines(gd, hsize + sy);
		memset(gd->linedata, 0, (hsize + sy) * sizeof *gd->linedata);
		gd->hsize = gd->hscrolled = hsize;
	}
	if (state_read_lines(jn, "history", ss, gd, 0, nhistory - hsize,
	    cause) != 0 ||
	    state_read_lines(jn, "lines", ss, gd, hsize, 0, cause) != 0) {
		grid_destroy(gd);
		return (NULL);
	}
	return (gd);
}

/* Read the cursor style and colour. */
static int
state_read_cursor_style(struct json_node *jn, struct state_screen *ss,
    char **cause)
{
	char	*name;
	u_int	 i;

	if (json_find(jn, "cursor-style") != NULL) {
		if (state_get_string(jn, "cursor-style", &name, cause) != 0)
			return (-1);
		for (i = 0; i < nitems(state_cursor_styles); i++) {
			if (strcmp(name, state_cursor_styles[i]) == 0)
				break;
		}
		if (i == nitems(state_cursor_styles)) {
			xasprintf(cause, "unknown cursor style \"%s\"", name);
			free(name);
			return (-1);
		}
		free(name);
		ss->cstyle = i;
		ss->flags |= STATE_STYLE;
	}
	if (json_find(jn, "cursor-colour") != NULL) {
		if (state_get_colour(jn, "cursor-colour", &ss->ccolour,
		    cause) != 0)
			return (-1);
		ss->flags |= STATE_COLOUR;
	}
	return (0);
}

/* Read the modes. */
static int
state_read_modes(struct json_node *jn, struct state_screen *ss, char **cause)
{
	struct json_node	*array, *member;
	char			*name;
	u_int			 i;

	if (json_find(jn, "modes") == NULL)
		return (0);
	if (json_find_array(jn, "modes", &array, cause) != 0)
		return (-1);
	member = json_array_first(array);
	while (member != NULL) {
		if (state_get_string(member, "name", &name, cause) != 0)
			return (-1);
		for (i = 0; i < nitems(state_modes); i++) {
			if (strcmp(name, state_modes[i].name) == 0)
				break;
		}
		if (i == nitems(state_modes)) {
			xasprintf(cause, "unknown mode \"%s\"", name);
			free(name);
			return (-1);
		}
		free(name);
		ss->mode |= state_modes[i].mode;
		member = json_array_next(member);
	}
	ss->flags |= STATE_MODES;
	return (0);
}

/* Read the scroll region, which is the whole screen if missing. */
static int
state_read_region(struct json_node *jn, struct state_screen *ss, char **cause)
{
	struct json_node	*region;
	u_int			 sy = ss->grid->sy;

	ss->rupper = 0;
	ss->rlower = sy - 1;
	if (json_find(jn, "region") == NULL)
		return (0);
	if (json_find_object(jn, "region", &region, cause) != 0)
		return (-1);
	if (state_get_number(region, "upper", 0, sy - 1, &ss->rupper,
	    cause) != 0 ||
	    state_get_number(region, "lower", 0, sy - 1, &ss->rlower,
	    cause) != 0)
		return (-1);
	if (ss->rupper >= ss->rlower) {
		*cause = xstrdup("invalid scroll region");
		return (-1);
	}
	return (0);
}

/* Read the tab stops. */
static int
state_read_tabs(struct json_node *jn, struct state_screen *ss, char **cause)
{
	struct json_node	*array, *member;
	u_int			 x, sx = ss->grid->sx;

	if (json_find(jn, "tabs") == NULL)
		return (0);
	if (json_find_array(jn, "tabs", &array, cause) != 0)
		return (-1);
	ss->tabs = bit_alloc(sx);
	member = json_array_first(array);
	while (member != NULL) {
		if (state_get_number(member, "x", 0, sx - 1, &x, cause) != 0)
			return (-1);
		bit_set(ss->tabs, x);
		member = json_array_next(member);
	}
	return (0);
}

/* Read the title stack. */
static int
state_read_titles(struct json_node *jn, struct state_screen *ss, char **cause)
{
	struct json_node	*array, *member;
	char			*title;

	if (json_find(jn, "titles") == NULL)
		return (0);
	if (json_find_array(jn, "titles", &array, cause) != 0)
		return (-1);
	member = json_array_first(array);
	while (member != NULL) {
		if (state_get_string(member, "title", &title, cause) != 0)
			return (-1);
		ss->titles = xreallocarray(ss->titles, ss->ntitles + 1,
		    sizeof *ss->titles);
		ss->titles[ss->ntitles++] = title;
		if (!check_name(title)) {
			*cause = xstrdup("invalid title in stack");
			return (-1);
		}
		member = json_array_next(member);
	}
	return (0);
}

/*
 * Read the cursor, cell and character sets saved by DECSC. A resize does not
 * change them and DECRC moves the cursor inside the screen, so the position
 * may be outside it.
 */
static int
state_read_saved(struct json_node *jn, struct state_screen *ss, char **cause)
{
	struct input_saved_cursor	*isc = &ss->saved;
	int				 origin;

	if (json_find(jn, "saved") == NULL)
		return (0);
	if (json_find_object(jn, "saved", &jn, cause) != 0)
		return (-1);
	if (state_get_number(jn, "x", 0, PANE_MAXIMUM, &isc->cx,
	    cause) != 0 ||
	    state_get_number(jn, "y", 0, PANE_MAXIMUM, &isc->cy,
	    cause) != 0)
		return (-1);
	if (state_get_boolean(jn, "origin", &origin, cause) != 0 ||
	    state_get_boolean(jn, "g1", &isc->set, cause) != 0 ||
	    state_get_boolean(jn, "g0-acs", &isc->g0set, cause) != 0 ||
	    state_get_boolean(jn, "g1-acs", &isc->g1set, cause) != 0)
		return (-1);
	isc->mode = origin ? MODE_ORIGIN : 0;
	if (state_get_style(jn, &isc->cell, cause) != 0)
		return (-1);
	ss->flags |= STATE_SAVED;
	return (0);
}

/*
 * Read the hyperlinks into a set of their own, with a map from the numbers
 * runs use in the file to the numbers they have now.
 */
static int
state_read_links(struct json_node *jn, struct state_screen *ss, char **cause)
{
	struct json_node	*array, *member;
	char			*uri, *name;
	u_int			 id;

	if (json_find(jn, "links") == NULL)
		return (0);
	if (json_find_array(jn, "links", &array, cause) != 0)
		return (-1);
	ss->links = hyperlinks_init();
	member = json_array_first(array);
	while (member != NULL) {
		if (state_get_number(member, "id", 1, UINT_MAX, &id,
		    cause) != 0)
			return (-1);
		if (state_get_string(member, "uri", &uri, cause) != 0)
			return (-1);
		if (state_get_string(member, "name", &name, cause) != 0) {
			free(uri);
			return (-1);
		}
		ss->link_map = xreallocarray(ss->link_map, ss->nlinks + 1,
		    sizeof *ss->link_map);
		ss->link_map[ss->nlinks].id = id;
		ss->link_map[ss->nlinks].inner = hyperlinks_put(ss->links,
		    uri, name);
		ss->nlinks++;
		free(uri);
		free(name);
		member = json_array_next(member);
	}
	qsort(ss->link_map, ss->nlinks, sizeof *ss->link_map,
	    state_link_map_cmp);
	return (0);
}

/* Read a screen. */
static int
state_read_screen(struct json_node *jn, struct state_screen *ss, u_int hlimit,
    char **cause)
{
	struct json_node	*alternate;

	if (json_get_object(jn, &jn) != 0) {
		*cause = xstrdup("screen is not an object");
		return (-1);
	}
	if (state_read_links(jn, ss, cause) != 0)
		return (-1);
	if ((ss->grid = state_read_grid(jn, ss, hlimit, cause)) == NULL)
		return (-1);
	if (state_read_cursor_style(jn, ss, cause) != 0 ||
	    state_read_modes(jn, ss, cause) != 0 ||
	    state_read_region(jn, ss, cause) != 0 ||
	    state_read_tabs(jn, ss, cause) != 0 ||
	    state_read_titles(jn, ss, cause) != 0 ||
	    state_read_saved(jn, ss, cause) != 0)
		return (-1);
	if (json_find(jn, "path") != NULL) {
		if (state_get_string(jn, "path", &ss->path, cause) != 0)
			return (-1);
		if (!check_name(ss->path)) {
			*cause = xstrdup("invalid path");
			return (-1);
		}
	}

	/*
	 * The cursor can be past the last column, either just past it after
	 * the last column is written or further if the pane was made narrower
	 * in the alternate screen, which is not reflowed.
	 */
	if (json_find(jn, "cursor") != NULL &&
	    state_get_cursor(jn, "cursor", PANE_MAXIMUM, ss->grid->sy - 1,
	    &ss->cx, &ss->cy, cause) != 0)
		return (-1);

	/*
	 * Once the alternate screen has ended a resize does not change the
	 * cursor kept for it, which is moved inside the screen when it is
	 * used.
	 */
	ss->saved_cx = ss->saved_cy = UINT_MAX;
	memcpy(&ss->saved_cell, &grid_default_cell, sizeof ss->saved_cell);
	if (json_find(jn, "alternate-cursor") != NULL) {
		if (state_get_cursor(jn, "alternate-cursor", PANE_MAXIMUM,
		    PANE_MAXIMUM, &ss->saved_cx, &ss->saved_cy, cause) != 0)
			return (-1);
		if (state_get_style(json_find(jn, "alternate-cursor"),
		    &ss->saved_cell, cause) != 0)
			return (-1);
	}

	if (json_find(jn, "alternate") == NULL)
		return (0);
	if (json_find_object(jn, "alternate", &alternate, cause) != 0)
		return (-1);
	if ((ss->saved_grid = state_read_grid(alternate, ss, 0, cause)) == NULL)
		return (-1);
	return (0);
}

/* Read a pane's palette. */
static int
state_read_palette(struct json_node *jn, struct colour_palette **out,
    char **cause)
{
	struct json_node	*array, *member;
	struct colour_palette	*p;
	u_int			 idx;
	int			 c;

	if (json_find(jn, "palette") == NULL)
		return (0);
	if (json_find_object(jn, "palette", &jn, cause) != 0)
		return (-1);
	p = *out = xcalloc(1, sizeof *p);
	colour_palette_init(p);
	if (state_get_colour(jn, "fg", &p->fg, cause) != 0 ||
	    state_get_colour(jn, "bg", &p->bg, cause) != 0)
		return (-1);

	if (json_find(jn, "colours") == NULL)
		return (0);
	if (json_find_array(jn, "colours", &array, cause) != 0)
		return (-1);
	member = json_array_first(array);
	while (member != NULL) {
		if (state_get_number(member, "index", 0, 255, &idx,
		    cause) != 0)
			return (-1);
		if (state_get_colour(member, "colour", &c, cause) != 0)
			return (-1);
		colour_palette_set(p, idx, c);
		member = json_array_next(member);
	}
	return (0);
}

/*
 * Give a pane the screen that was read, as it would be if the pane had been
 * resized from the saved size to its own.
 */
static void
state_apply_screen(struct window_pane *wp, struct state_screen *ss)
{
	struct screen	*s = &wp->base;
	u_int		 sx = screen_size_x(s), sy = screen_size_y(s), i;

	if (ss->grid == NULL)
		return;

	grid_destroy(s->grid);
	s->grid = ss->grid;
	ss->grid = NULL;
	s->cx = ss->cx;
	s->cy = ss->cy;
	s->rupper = ss->rupper;
	s->rlower = ss->rlower;
	if (ss->tabs != NULL) {
		free(s->tabs);
		s->tabs = ss->tabs;
		ss->tabs = NULL;
	}
	if (ss->links != NULL) {
		hyperlinks_free(s->hyperlinks);
		s->hyperlinks = ss->links;
		ss->links = NULL;
	}

	s->saved_cx = ss->saved_cx;
	s->saved_cy = ss->saved_cy;
	memcpy(&s->saved_cell, &ss->saved_cell, sizeof s->saved_cell);
	if (ss->saved_grid != NULL) {
		s->saved_grid = ss->saved_grid;
		ss->saved_grid = NULL;
		s->saved_flags = s->grid->flags;
		s->grid->flags &= ~GRID_HISTORY;
	}

	/*
	 * Only the alternate screen leaves the cursor further out. Resizing
	 * resets the scroll region and tab stops if the size changes.
	 */
	if (s->saved_grid == NULL && s->cx > screen_size_x(s))
		s->cx = screen_size_x(s);
	screen_resize(s, sx, sy, s->saved_grid == NULL);

	if (ss->flags & STATE_STYLE)
		s->cstyle = ss->cstyle;
	if (ss->flags & STATE_COLOUR)
		s->ccolour = ss->ccolour;
	if (ss->flags & STATE_MODES)
		s->mode = ss->mode;
	if ((ss->flags & STATE_SAVED) && wp->ictx != NULL)
		input_set_saved_cursor(wp->ictx, &ss->saved);

	/* Push the oldest title first so the newest ends up on top. */
	for (i = ss->ntitles; i > 0; i--) {
		screen_set_title(s, ss->titles[i - 1], 0);
		screen_push_title(s);
	}
	if (ss->path != NULL)
		screen_set_path(s, ss->path, 0);

	wp->flags |= PANE_REDRAW;
}

/* Give a pane the palette that was read. */
static void
state_apply_palette(struct window_pane *wp, struct colour_palette *p)
{
	u_int	i;

	if (p == NULL)
		return;
	wp->palette.fg = p->fg;
	wp->palette.bg = p->bg;
	for (i = 0; p->palette != NULL && i < 256; i++) {
		if (p->palette[i] != -1)
			colour_palette_set(&wp->palette, i, p->palette[i]);
	}
	wp->flags |= (PANE_REDRAW|PANE_STYLECHANGED);
}

/* Read one option or array item into a tree. */
static int
state_read_option(struct json_node *jn, struct options *oo, int scope,
    char **cause)
{
	const struct options_table_entry	*oe;
	struct options_entry			*o;
	char					*name = NULL, *key = NULL;
	char					*value = NULL;
	char					*error = NULL;
	int					 has_key, retval = -1;

	if (json_get_object(jn, &jn) != 0) {
		*cause = xstrdup("option is not an object");
		return (-1);
	}
	if (state_get_string(jn, "name", &name, cause) != 0 ||
	    state_get_string(jn, "index", &key, cause) != 0 ||
	    state_get_string(jn, "value", &value, cause) != 0)
		goto out;
	has_key = (json_find(jn, "index") != NULL);

	if (*name == '@') {
		if (has_key) {
			xasprintf(cause, "not an array: %s", name);
			goto out;
		}
		options_set_string(oo, name, 0, "%s", value);
		retval = 0;
		goto out;
	}

	oe = options_search(name);
	if (oe == NULL || (~oe->scope & scope)) {
		xasprintf(cause, "invalid option: %s", name);
		goto out;
	}
	if (~oe->flags & OPTIONS_TABLE_IS_ARRAY) {
		if (has_key) {
			xasprintf(cause, "not an array: %s", name);
			goto out;
		}
		if (options_from_string(oo, oe, name, value, 0, &error) != 0)
			goto fail;
		retval = 0;
		goto out;
	}
	if ((o = options_get_only(oo, name)) == NULL)
		o = options_empty(oo, oe);
	if (has_key && options_array_set(o, key, value, 0, &error) != 0)
		goto fail;
	retval = 0;
	goto out;

fail:
	xasprintf(cause, "%s: %s", name, error);
	free(error);
out:
	free(name);
	free(key);
	free(value);
	return (retval);
}

/*
 * Read the options set on an object into a tree of their own, which checks
 * them without changing anything. The tree is NULL if there are none.
 */
static int
state_read_options(struct json_node *jn, int scope, struct options **out,
    char **cause)
{
	struct json_node	*array, *member;

	if (json_find(jn, "options") == NULL)
		return (0);
	if (json_find_array(jn, "options", &array, cause) != 0)
		return (-1);
	*out = options_create(global_w_options);
	member = json_array_first(array);
	while (member != NULL) {
		if (state_read_option(member, *out, scope, cause) != 0)
			return (-1);
		member = json_array_next(member);
	}
	return (0);
}

/*
 * Replace the options set on an object with those that were read, and apply
 * them as set-option does.
 */
static void
state_apply_options(struct options *oo, struct options *from)
{
	const struct options_table_entry	*oe;
	struct options_entry			*o, *next, *to;
	struct options_array_item		*a;
	const char				*key;
	char					*name, *value, *error;

	if (from == NULL)
		return;

	o = options_first(oo);
	while (o != NULL) {
		next = options_next(o);
		name = xstrdup(options_name(o));
		options_remove_or_default(o, NULL, NULL);
		options_push_changes(name);
		free(name);
		o = next;
	}

	for (o = options_first(from); o != NULL; o = options_next(o)) {
		oe = options_table_entry(o);
		if (oe == NULL) {
			value = options_to_string(o, NULL, 0);
			options_set_string(oo, options_name(o), 0, "%s", value);
			free(value);
		} else if (!options_is_array(o)) {
			value = options_to_string(o, NULL, 0);
			error = NULL;
			if (options_from_string(oo, oe, options_name(o), value,
			    0, &error) != 0)
				free(error);
			free(value);
		} else {
			to = options_empty(oo, oe);
			a = options_array_first(o);
			while (a != NULL) {
				key = options_array_item_key(a);
				value = options_to_string(o, key, 0);
				error = NULL;
				if (options_array_set(to, key, value, 0,
				    &error) != 0)
					free(error);
				free(value);
				a = options_array_next(a);
			}
		}
		options_push_changes(options_name(o));
	}
}

/* Free a screen read from a state file. */
static void
state_free_screen(struct state_screen *ss)
{
	u_int	i;

	if (ss->grid != NULL)
		grid_destroy(ss->grid);
	if (ss->saved_grid != NULL)
		grid_destroy(ss->saved_grid);
	free(ss->tabs);
	for (i = 0; i < ss->ntitles; i++)
		free(ss->titles[i]);
	free(ss->titles);
	free(ss->path);
	if (ss->links != NULL)
		hyperlinks_free(ss->links);
	free(ss->link_map);
}

/* Free a window read from a state file. */
static void
state_free_window(struct state_window *sw)
{
	struct state_pane	*sp;
	u_int			 i;

	for (i = 0; i < sw->npanes; i++) {
		sp = &sw->panes[i];
		free(sp->title);
		free(sp->cwd);
		cmd_free_argv(sp->argc, sp->argv);
		if (sp->options != NULL)
			options_free(sp->options);
		if (sp->palette != NULL) {
			colour_palette_free(sp->palette);
			free(sp->palette);
		}
		state_free_screen(&sp->screen);
	}
	free(sw->panes);
	free(sw->name);
	free(sw->layout);
	if (sw->options != NULL)
		options_free(sw->options);
}

/* Read a pane. */
static int
state_read_pane(struct json_node *jn, struct state_pane *sp, u_int hlimit,
    char **cause)
{
	struct json_node	*array, *member;
	char			*arg;

	if (json_get_object(jn, &jn) != 0) {
		*cause = xstrdup("pane is not an object");
		return (-1);
	}
	if (state_get_string(jn, "title", &sp->title, cause) != 0)
		return (-1);
	if (!check_name(sp->title)) {
		*cause = xstrdup("invalid pane title");
		return (-1);
	}
	if (state_get_string(jn, "cwd", &sp->cwd, cause) != 0)
		return (-1);
	if (state_get_boolean(jn, "zoomed", &sp->zoomed, cause) != 0)
		return (-1);
	if (state_get_boolean(jn, "float-over-zoom", &sp->float_over_zoom,
	    cause) != 0)
		return (-1);

	if (json_find(jn, "command") != NULL) {
		if (json_find_array(jn, "command", &array, cause) != 0)
			return (-1);
		member = json_array_first(array);
		while (member != NULL) {
			if (state_get_string(member, "arg", &arg, cause) != 0)
				return (-1);
			sp->argv = xreallocarray(sp->argv, sp->argc + 1,
			    sizeof *sp->argv);
			sp->argv[sp->argc++] = arg;
			member = json_array_next(member);
		}
	}

	if (state_read_options(jn, OPTIONS_TABLE_PANE, &sp->options,
	    cause) != 0)
		return (-1);
	if (state_read_palette(jn, &sp->palette, cause) != 0)
		return (-1);
	if (json_find(jn, "screen") != NULL &&
	    state_read_screen(json_find(jn, "screen"), &sp->screen, hlimit,
	    cause) != 0)
		return (-1);
	return (0);
}

/* Read a window. Nothing is created, so this can fail without side effects. */
static int
state_read_window(struct json_node *jn, struct state_window *sw, u_int hlimit,
    char **cause)
{
	struct json_node	*layout, *root, *array, *member;
	int64_t			 sx, sy;

	if (state_get_string(jn, "name", &sw->name, cause) != 0)
		return (-1);
	if (!check_name(sw->name)) {
		*cause = xstrdup("invalid window name");
		return (-1);
	}
	if (state_get_boolean(jn, "zoomed", &sw->zoomed, cause) != 0)
		return (-1);
	if (state_read_options(jn, OPTIONS_TABLE_WINDOW, &sw->options,
	    cause) != 0)
		return (-1);

	if (json_find_object(jn, "layout", &layout, cause) != 0)
		return (-1);
	sw->layout = json_to_string(layout);

	if (json_find_array(jn, "panes", &array, cause) != 0)
		return (-1);
	member = json_array_first(array);
	while (member != NULL) {
		sw->panes = xreallocarray(sw->panes, sw->npanes + 1,
		    sizeof *sw->panes);
		memset(&sw->panes[sw->npanes], 0, sizeof *sw->panes);
		sw->npanes++;
		if (state_read_pane(member, &sw->panes[sw->npanes - 1], hlimit,
		    cause) != 0)
			return (-1);
		member = json_array_next(member);
	}
	if (sw->npanes == 0) {
		*cause = xstrdup("window has no panes");
		return (-1);
	}

	/* The checked layout has a root cell with a size. */
	if (layout_check_string(sw->layout, sw->npanes, cause) != 0)
		return (-1);
	json_find_object(layout, "L", &root, NULL);
	json_find_number(root, "w", &sx, NULL);
	json_find_number(root, "h", &sy, NULL);
	sw->sx = sx;
	sw->sy = sy;
	return (0);
}

/*
 * Create a window from what was read. The panes are created empty, so nothing
 * runs in them, but keep the command and working directory for respawn-pane.
 */
static struct winlink *
state_build_window(struct state_window *sw, struct session *s, int idx,
    struct client *tc, int detached, char **cause)
{
	struct spawn_context	 sc;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp, *zoomed;
	struct state_pane	*sp;
	struct layout_geometry	 lg;
	char			*name, *error;
	u_int			 i;
	int			 size;

	/*
	 * The item is left out of the spawn context so the working directory
	 * is used as it is rather than expanded as a format. The window is not
	 * made current until it is complete.
	 */
	memset(&sc, 0, sizeof sc);
	sc.s = s;
	sc.tc = tc;
	sc.name = name = clean_name(sw->name, 0);
	sc.idx = idx;
	sc.cwd = sw->panes[0].cwd;
	sc.argc = sw->panes[0].argc;
	sc.argv = sw->panes[0].argv;
	sc.flags = SPAWN_EMPTY|SPAWN_DETACHED;
	wl = spawn_window(&sc, &error);
	free(name);
	if (wl == NULL) {
		xasprintf(cause, "create window failed: %s", error);
		free(error);
		return (NULL);
	}
	w = wl->window;

	/*
	 * Some options change the size of panes, so set them before the
	 * layout is applied.
	 */
	state_apply_options(w->options, sw->options);

	/*
	 * Give the other panes floating cells, which need no space. The
	 * layout replaces them.
	 */
	lg.sx = w->sx;
	lg.sy = w->sy;
	lg.xoff = 0;
	lg.yoff = 0;
	for (i = 1; i < sw->npanes; i++) {
		sp = &sw->panes[i];

		memset(&sc, 0, sizeof sc);
		sc.s = s;
		sc.wl = wl;
		sc.tc = tc;
		sc.wp0 = TAILQ_LAST(&w->panes, window_panes);
		sc.lc = layout_floating_pane(w, sc.wp0, &lg);
		sc.idx = -1;
		sc.cwd = sp->cwd;
		sc.argc = sp->argc;
		sc.argv = sp->argv;
		sc.flags = SPAWN_EMPTY|SPAWN_DETACHED|SPAWN_NONOTIFY;
		if (spawn_pane(&sc, cause) == NULL)
			goto fail;
	}
	i = 0;
	TAILQ_FOREACH(wp, &w->panes, entry)
		state_apply_options(wp->options, sw->panes[i++].options);

	/*
	 * A window with a manual size keeps it, so give it the size of the
	 * layout before applying it.
	 */
	size = options_get_number(w->options, "window-size");
	if (size == WINDOW_SIZE_MANUAL) {
		w->manual_sx = sw->sx;
		w->manual_sy = sw->sy;
	}
	if (layout_parse(w, sw->layout, cause) != 0)
		goto fail;

	/*
	 * Mark the floating panes that stay above a zoomed pane before
	 * zooming, so window_zoom leaves one of them active if it was.
	 */
	zoomed = NULL;
	i = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		sp = &sw->panes[i++];
		if (sp->float_over_zoom)
			wp->flags |= PANE_FLOATOVERZOOM;
		if (sp->zoomed)
			zoomed = wp;
	}
	if (sw->zoomed)
		window_zoom(zoomed != NULL ? zoomed : w->active);

	i = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		sp = &sw->panes[i++];
		state_apply_screen(wp, &sp->screen);
		state_apply_palette(wp, sp->palette);
		screen_set_title(&wp->base, sp->title, 0);
	}

	if (!detached)
		session_select(s, wl->idx);
	events_fire_window("window-layout-changed", w);
	return (wl);

fail:
	session_detach(s, wl);
	return (NULL);
}

/* Create a window from JSON written by state_save_window. */
struct winlink *
state_load_window(const char *data, struct session *s, int idx,
    struct client *tc, int detached, char **cause)
{
	struct json_node	*root, *jn;
	struct state_window	 sw;
	struct winlink		*wl = NULL;
	int64_t			 version;
	u_int			 hlimit;
	char			*error = NULL;

	memset(&sw, 0, sizeof sw);
	if ((root = json_parse(data, &error)) == NULL) {
		xasprintf(cause, "invalid JSON: %s", error);
		free(error);
		return (NULL);
	}
	if (json_find_number(root, "version", &version, cause) != 0)
		goto out;
	if (version != STATE_VERSION) {
		xasprintf(cause, "unsupported version %lld",
		    (long long)version);
		goto out;
	}
	if (json_find_object(root, "window", &jn, cause) != 0)
		goto out;
	hlimit = options_get_number(s->options, "history-limit");
	if (state_read_window(jn, &sw, hlimit, cause) != 0)
		goto out;
	wl = state_build_window(&sw, s, idx, tc, detached, cause);

out:
	state_free_window(&sw);
	json_destroy_node(root);
	return (wl);
}
