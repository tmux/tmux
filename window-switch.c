/* $OpenBSD: window-switch.c,v 1.1 2026/06/26 14:40:30 nicm Exp $ */

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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct screen	*window_switch_init(struct window_mode_entry *,
			     struct cmd_find_state *, struct args *);
static void		 window_switch_free(struct window_mode_entry *);
static void		 window_switch_resize(struct window_mode_entry *, u_int,
			     u_int);
static void		 window_switch_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);
static enum prompt_result window_switch_prompt_callback(void *, const char *,
			     enum prompt_key_result);

#define WINDOW_SWITCH_DEFAULT_COMMAND "switch-client -Zt '%%'"

#define WINDOW_SWITCH_DEFAULT_FORMAT		\
	"#{?window_format," \
		"#{window_name} " \
		"#[dim]#{session_name}:#{window_index}#{window_flags}#[default] " \
		"#[dim]#{pane_current_command}#[default] " \
	        "#[dim]#{?#{!=:#{pane_title},#{host_short}},#{pane_title},}#[default]" \
	"," \
		"#{session_name} " \
		"#[dim]#{session_windows} windows#[default] " \
		"#{?session_attached,attached,#[dim]detached#[default]} " \
		"#[dim]#{window_name}#[default]" \
	"}"

const struct window_mode window_switch_mode = {
	.name = "switch-mode",
	.default_format = WINDOW_SWITCH_DEFAULT_FORMAT,

	.init = window_switch_init,
	.free = window_switch_free,
	.resize = window_switch_resize,
	.key = window_switch_key,
};

enum window_switch_type {
	WINDOW_SWITCH_TYPE_SESSION,
	WINDOW_SWITCH_TYPE_WINDOW
};

struct window_switch_itemdata {
	enum window_switch_type	 type;
	int			 session;
	int			 winlink;

	uint64_t		 tag;
	char			*text;
	bitstr_t		*match;

	u_int			 score;
	u_int			 order;
};

struct window_switch_modedata {
	struct window_pane		 *wp;
	struct screen			  screen;
	int				  zoomed;

	char				 *format;
	char				 *command;

	enum window_switch_type		  type;
	char				 *filter;
	struct prompt			 *prompt;
	u_int				  prompt_cx;

	struct window_switch_itemdata	**item_list;
	u_int				  item_size;

	struct window_switch_itemdata	**matches;
	u_int				  matches_size;

	u_int				  current;
	u_int				  offset;
};

static void
window_switch_free_item(struct window_switch_itemdata *item)
{
	free(item->match);
	free(item->text);
	free(item);
}

static struct window_switch_itemdata *
window_switch_add_item(struct window_switch_modedata *data)
{
	struct window_switch_itemdata	*item;

	data->item_list = xreallocarray(data->item_list, data->item_size + 1,
	    sizeof *data->item_list);
	item = data->item_list[data->item_size++] = xcalloc(1, sizeof *item);
	return (item);
}

static void
window_switch_add_session(struct window_switch_modedata *data,
    struct session *s, u_int *order)
{
	struct window_switch_itemdata	*item;
	struct format_tree		*ft;

	ft = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_defaults(ft, NULL, s, NULL, NULL);

	item = window_switch_add_item(data);
	item->type = WINDOW_SWITCH_TYPE_SESSION;
	item->session = s->id;
	item->winlink = -1;
	item->tag = (uint64_t)s;
	item->order = (*order)++;
	item->text = format_expand(ft, data->format);

	format_free(ft);
}

static void
window_switch_add_window(struct window_switch_modedata *data,
    struct winlink *wl, u_int *order)
{
	struct window_switch_itemdata	*item;
	struct format_tree		*ft;

	ft = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_defaults(ft, NULL, wl->session, wl, NULL);

	item = window_switch_add_item(data);
	item->type = WINDOW_SWITCH_TYPE_WINDOW;
	item->session = wl->session->id;
	item->winlink = wl->idx;
	item->tag = (uint64_t)wl;
	item->order = (*order)++;
	item->text = format_expand(ft, data->format);

	format_free(ft);
}

static int
window_switch_compare(const void *a0, const void *b0)
{
	struct window_switch_itemdata	*const *a = a0;
	struct window_switch_itemdata	*const *b = b0;

	if ((*a)->score > (*b)->score)
		return (-1);
	if ((*a)->score < (*b)->score)
		return (1);
	if ((*a)->order < (*b)->order)
		return (-1);
	if ((*a)->order > (*b)->order)
		return (1);
	return (0);
}

static void
window_switch_build(struct window_switch_modedata *data)
{
	struct window_switch_itemdata	 *item, **m = NULL;
	const char			 *f = data->filter;
	u_int				  ns, nw, i, n = 0, order = 0;
	u_int				  sx = screen_size_x(&data->screen);
	struct session			**sl;
	struct winlink			**wl;
	struct sort_criteria		  sort_crit;

	sort_crit.order = SORT_NAME;
	sort_crit.reversed = 0;

	for (i = 0; i < data->item_size; i++)
		window_switch_free_item(data->item_list[i]);
	free(data->item_list);
	data->item_list = NULL;
	data->item_size = 0;

	switch (data->type) {
	case WINDOW_SWITCH_TYPE_SESSION:
		sl = sort_get_sessions(&ns, &sort_crit);
		for (i = 0; i < ns; i++)
			window_switch_add_session(data, sl[i], &order);
		break;
	case WINDOW_SWITCH_TYPE_WINDOW:
		wl = sort_get_winlinks(&nw, &sort_crit);
		for (i = 0; i < nw; i++)
			window_switch_add_window(data, wl[i], &order);
		break;
	}

	for (i = 0; i < data->item_size; i++) {
		item = data->item_list[i];
		if (*f == '\0') {
			m = xreallocarray(m, n + 1, sizeof *m);
			m[n++] = item;
			continue;
		}

		item->match = fuzzy_match(f, item->text, sx, &item->score);
		if (item->match == NULL)
			continue;
		m = xreallocarray(m, n + 1, sizeof *m);
		m[n++] = item;
	}
	qsort(m, n, sizeof *m, window_switch_compare);

	free(data->matches);
	data->matches = m;
	data->matches_size = n;
}

static u_int
window_switch_visible(struct window_switch_modedata *data)
{
	u_int	sy = screen_size_y(&data->screen);

	if (sy <= 1)
		return (0);
	return (sy - 1);
}

static void
window_switch_set_current(struct window_switch_modedata *data, u_int current)
{
	u_int	visible = window_switch_visible(data);

	if (data->matches_size == 0) {
		data->current = 0;
		data->offset = 0;
		return;
	}

	if (current > data->matches_size - 1)
		current = data->matches_size - 1;
	data->current = current;

	if (data->current < data->offset)
		data->offset = data->current;
	else if (visible != 0 && data->current >= data->offset + visible)
		data->offset = data->current - visible + 1;
}

static void
window_switch_draw_screen(struct window_mode_entry *wme)
{
	struct window_pane		*wp = wme->wp;
	struct window_switch_modedata	*data = wme->data;
	struct options			*oo = wp->options;
	struct screen_write_ctx		 ctx;
	struct screen			*s = &data->screen;
	u_int				 sx = screen_size_x(s), i, j;
	u_int				 sy = screen_size_y(s), visible, idx;
	struct window_switch_itemdata	*item;
	struct grid_cell		 mgc, sgc, gc;
	const struct grid_cell		*dgc = &grid_default_cell;
	struct prompt_draw_data		 pdd;
	screen_write_start(&ctx, s);
	screen_write_clearscreen(&ctx, 8);

	if (sy <= 1) {
		screen_write_stop(&ctx);
		return;
	}

	style_apply(&mgc, oo, "switch-mode-match-style", NULL);
	style_apply(&sgc, oo, "mode-style", NULL);

	visible = window_switch_visible(data);
	for (i = 0; i < visible; i++) {
		idx = data->offset + i;
		if (idx >= data->matches_size)
			break;
		item = data->matches[idx];

		screen_write_cursormove(&ctx, 0, i, 0);
		if (idx != data->current)
			format_draw(&ctx, dgc, sx, item->text, NULL, 0);
		else {
			screen_write_clearendofline(&ctx, sgc.bg);
			format_draw(&ctx, &sgc, sx, item->text, NULL, 0);
		}

		if (item->match == NULL)
			continue;
		for (j = 0; j < sx; j++) {
			if (!bit_test(item->match, j))
				continue;
			grid_get_cell(s->grid, j, i, &gc);
			gc.attr = mgc.attr;
			gc.fg = mgc.fg;
			gc.bg = mgc.bg;
			screen_write_cursormove(&ctx, j, i, 0);
			screen_write_cell(&ctx, &gc);
		}
	}

	if (data->prompt != NULL) {
		pdd.ctx = &ctx;
		pdd.cursor_x = &data->prompt_cx;
		pdd.area_x = 0;
		pdd.area_width = sx;
		pdd.prompt_line = sy - 1;
		s->mode |= MODE_CURSOR;
		prompt_draw(data->prompt, &pdd);
		screen_write_cursormove(&ctx, data->prompt_cx, sy - 1, 0);
	}
	screen_write_stop(&ctx);
}

static struct screen *
window_switch_init(struct window_mode_entry *wme,
    struct cmd_find_state *fs, struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_switch_modedata	*data;
	struct screen			*s;
	struct prompt_create_data	 pd;

	wme->data = data = xcalloc(1, sizeof *data);
	data->wp = wp;

	if (args_has(args, 'w'))
		data->type = WINDOW_SWITCH_TYPE_WINDOW;
	else
		data->type = WINDOW_SWITCH_TYPE_SESSION;

	data->filter = xstrdup("");
	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_SWITCH_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));
	if (args == NULL || args_count(args) == 0)
		data->command = xstrdup(WINDOW_SWITCH_DEFAULT_COMMAND);
	else
		data->command = xstrdup(args_string(args, 0));

	memset(&pd, 0, sizeof pd);
	prompt_set_options(&pd, fs->s);
	pd.fs = fs;
	pd.prompt = "(search) ";
	pd.input = "";
	pd.type = PROMPT_TYPE_SEARCH;
	pd.flags = PROMPT_INCREMENTAL|PROMPT_NOFORMAT|PROMPT_ISMODE|
	    PROMPT_EDITARROWS;
	pd.inputcb = window_switch_prompt_callback;
	pd.data = data;
	data->prompt = prompt_create(&pd);
	prompt_update(data->prompt, "(search) ", data->filter);

	if (!args_has(args, 'Z'))
		data->zoomed = -1;
	else {
		data->zoomed = (wp->window->flags & WINDOW_ZOOMED);
		if (!data->zoomed && window_zoom(wp) == 0)
			server_redraw_window(wp->window);
	}

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);

	window_switch_build(data);
	prompt_incremental_start(data->prompt);
	window_switch_draw_screen(wme);

	return (s);
}

static void
window_switch_free(struct window_mode_entry *wme)
{
	struct window_switch_modedata	*data = wme->data;
	u_int				 i;

	if (data->zoomed == 0)
		server_unzoom_window(wme->wp->window);

	for (i = 0; i < data->item_size; i++)
		window_switch_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->matches);
	free(data->filter);
	prompt_free(data->prompt);
	free(data->format);
	free(data->command);
	screen_free(&data->screen);

	free(data);
}

static void
window_switch_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_switch_modedata	*data = wme->data;
	struct screen			*s = &data->screen;

	screen_resize(s, sx, sy, 0);
	window_switch_build(data);
	window_switch_set_current(data, data->current);
	window_switch_draw_screen(wme);
}

static int
window_switch_run_command(struct window_switch_modedata *data, struct client *c)
{
	struct window_switch_itemdata	*item;
	struct cmd_find_state		 fs;
	struct session			*s;
	struct winlink			*wl;
	char				*target = NULL;
	struct cmdq_state		*state;
	char				*command, *error;
	enum cmd_parse_status		 status;

	if (data->matches_size == 0)
		return (0);
	item = data->matches[data->current];

	cmd_find_clear_state(&fs, 0);
	switch (item->type) {
	case WINDOW_SWITCH_TYPE_SESSION:
		s = session_find_by_id(item->session);
		if (s != NULL) {
			xasprintf(&target, "=%s:", s->name);
			cmd_find_from_session(&fs, s, 0);
		}
		break;
	case WINDOW_SWITCH_TYPE_WINDOW:
		s = session_find_by_id(item->session);
		if (s != NULL) {
			wl = winlink_find_by_index(&s->windows, item->winlink);
			if (s != NULL && wl != NULL) {
				xasprintf(&target, "=%s:%u.", s->name, wl->idx);
				cmd_find_from_winlink(&fs, wl, 0);
			}
		}
		break;
	}
	if (target == NULL)
		return (0);

	command = cmd_template_replace(data->command, target, 1);
	if (command != NULL && *command != '\0') {
		state = cmdq_new_state(&fs, NULL, 0);
		status = cmd_parse_and_append(command, NULL, c, state, &error);
		if (status == CMD_PARSE_ERROR) {
			if (c != NULL) {
				*error = toupper((u_char)*error);
				status_message_set(c, -1, 1, 0, 0, "%s", error);
			}
			free(error);
		}
		cmdq_free_state(state);
	}
	free(command);
	free(target);
	return (1);
}

static enum prompt_result
window_switch_prompt_callback(void *arg, const char *s,
    enum prompt_key_result key)
{
	struct window_switch_modedata	*data = arg;

	if (key != PROMPT_KEY_HANDLED)
		return (PROMPT_CONTINUE);

	if (s == NULL)
		s = "";
	else if (*s != '\0')
		s++;

	free(data->filter);
	data->filter = xstrdup(s);
	window_switch_build(data);
	data->current = 0;
	data->offset = 0;

	return (PROMPT_CONTINUE);
}

static void
window_switch_key(struct window_mode_entry *wme, struct client *c,
    __unused struct session *s, __unused struct winlink *wl, key_code key,
    struct mouse_event *m)
{
	struct window_pane		*wp = wme->wp;
	struct window_switch_modedata	*data = wme->data;
	u_int				 visible, current = data->current;
	u_int				 x, y, size = data->matches_size;
	enum prompt_key_result		 result;
	int				 redraw = 0;

	if (KEYC_IS_MOUSE(key)) {
		if (m == NULL || cmd_mouse_at(wp, m, &x, &y, 0) != 0)
			return;
		if (data->prompt != NULL && screen_size_y(&data->screen) != 0 &&
		    y == screen_size_y(&data->screen) - 1 &&
		    MOUSE_BUTTONS(m->b) == MOUSE_BUTTON_1 && !MOUSE_DRAG(m->b) &&
		    !MOUSE_RELEASE(m->b)) {
			result = prompt_mouse(data->prompt, x, 0,
			    screen_size_x(&data->screen), &redraw);
			if (redraw || result == PROMPT_KEY_HANDLED) {
				window_switch_draw_screen(wme);
				wp->flags |= PANE_REDRAW;
			}
			return;
		}
		switch (key) {
		case KEYC_WHEELUP_PANE:
			if (size != 0 && current != 0)
				window_switch_set_current(data, current - 1);
			goto moved;
		case KEYC_WHEELDOWN_PANE:
			if (size != 0 && current != size - 1)
				window_switch_set_current(data, current + 1);
			goto moved;
		case KEYC_MOUSEDOWN1_PANE:
		case KEYC_DOUBLECLICK1_PANE:
			if (y >= window_switch_visible(data) ||
			    data->offset + y >= size)
				return;
			window_switch_set_current(data, data->offset + y);
			if (key == KEYC_DOUBLECLICK1_PANE) {
				if (window_switch_run_command(data, c))
					window_pane_reset_mode(wp);
				return;
			}
			goto moved;
		}
		return;
	}

	switch (key) {
	case 'p'|KEYC_CTRL:
	case 'k'|KEYC_CTRL:
		key = KEYC_UP;
		break;
	case 'n'|KEYC_CTRL:
	case 'j'|KEYC_CTRL:
		key = KEYC_DOWN;
		break;
	}

	switch (key) {
	case '\r':
		if (window_switch_run_command(data, c))
			window_pane_reset_mode(wp);
		return;
	case '\033': /* Escape */
	case '['|KEYC_CTRL:
	case 'c'|KEYC_CTRL:
	case 'g'|KEYC_CTRL:
		window_pane_reset_mode(wp);
		return;
	}

	if (data->prompt != NULL) {
		result = prompt_key(data->prompt, key, &redraw);
		if (redraw) {
			window_switch_draw_screen(wme);
			wp->flags |= PANE_REDRAW;
		}
		if (result == PROMPT_KEY_HANDLED ||
		    result == PROMPT_KEY_NOT_HANDLED)
			return;
		current = data->current;
		size = data->matches_size;
	}

	switch (key) {
	case KEYC_UP:
		if (size == 0)
			goto moved;
		if (current == 0)
			window_switch_set_current(data, size - 1);
		else
			window_switch_set_current(data, current - 1);
		goto moved;
	case KEYC_DOWN:
		if (size == 0)
			goto moved;
		if (current == size - 1)
			window_switch_set_current(data, 0);
		else
			window_switch_set_current(data, current + 1);
		goto moved;
	case KEYC_PPAGE:
		visible = window_switch_visible(data);
		if (current >= visible)
			window_switch_set_current(data, current - visible);
		else
			window_switch_set_current(data, 0);
		goto moved;
	case KEYC_NPAGE:
		visible = window_switch_visible(data);
		window_switch_set_current(data, current + visible);
		goto moved;
	case KEYC_HOME:
		window_switch_set_current(data, 0);
		goto moved;
	case KEYC_END:
		if (size > 0)
			window_switch_set_current(data, size - 1);
		goto moved;
	}

moved:
	window_switch_draw_screen(wme);
	wp->flags |= PANE_REDRAW;
}
