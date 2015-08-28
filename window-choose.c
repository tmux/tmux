/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include "array.h"
#include "tmux.h"

struct screen *window_choose_init(struct window_pane *);
void	window_choose_free(struct window_pane *);
void	window_choose_resize(struct window_pane *, u_int, u_int);
void	window_choose_key(struct window_pane *, struct client *,
	    struct session *, int, struct mouse_event *);

void	window_choose_default_callback(struct window_choose_data *);
struct window_choose_mode_item *window_choose_get_item(struct window_pane *,
	    int, struct mouse_event *);

void	window_choose_fire_callback(
	    struct window_pane *, struct window_choose_data *);
void	window_choose_redraw_screen(struct window_pane *);
void	window_choose_write_line(
	    struct window_pane *, struct screen_write_ctx *, u_int);

void	window_choose_scroll_up(struct window_pane *);
void	window_choose_scroll_down(struct window_pane *);

void	window_choose_collapse(struct window_pane *, struct session *, u_int);
void	window_choose_expand(struct window_pane *, struct session *, u_int);

enum window_choose_input_type {
	WINDOW_CHOOSE_NORMAL = -1,
	WINDOW_CHOOSE_GOTO_ITEM,
};

const struct window_mode window_choose_mode = {
	window_choose_init,
	window_choose_free,
	window_choose_resize,
	window_choose_key,
};

struct window_choose_mode_item {
	struct window_choose_data	*wcd;
	char				*name;
	int				 pos;
	int				 state;
#define TREE_EXPANDED 0x1
};

struct window_choose_mode_data {
	struct screen	        screen;

	struct mode_key_data	mdata;

	ARRAY_DECL(, struct window_choose_mode_item) list;
	ARRAY_DECL(, struct window_choose_mode_item) old_list;
	int			width;
	u_int			top;
	u_int			selected;
	enum window_choose_input_type input_type;
	const char		*input_prompt;
	char			*input_str;

	void 			(*callbackfn)(struct window_choose_data *);
};

void	window_choose_free1(struct window_choose_mode_data *);
int     window_choose_key_index(struct window_choose_mode_data *, u_int);
int     window_choose_index_key(struct window_choose_mode_data *, int);
void	window_choose_prompt_input(enum window_choose_input_type,
	    const char *, struct window_pane *, int);
void	window_choose_reset_top(struct window_pane *, u_int);

void
window_choose_add(struct window_pane *wp, struct window_choose_data *wcd)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	char				 tmp[10];

	ARRAY_EXPAND(&data->list, 1);
	item = &ARRAY_LAST(&data->list);

	item->name = format_expand(wcd->ft, wcd->ft_template);
	item->wcd = wcd;
	item->pos = ARRAY_LENGTH(&data->list) - 1;
	item->state = 0;

	data->width = xsnprintf(tmp, sizeof tmp , "%d", item->pos);
}

void
window_choose_set_current(struct window_pane *wp, u_int cur)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	data->selected = cur;
	window_choose_reset_top(wp, screen_size_y(s));
}

void
window_choose_reset_top(struct window_pane *wp, u_int sy)
{
	struct window_choose_mode_data	*data = wp->modedata;

	data->top = 0;
	if (data->selected > sy - 1)
		data->top = data->selected - (sy - 1);

	window_choose_redraw_screen(wp);
}

void
window_choose_ready(struct window_pane *wp, u_int cur,
    void (*callbackfn)(struct window_choose_data *))
{
	struct window_choose_mode_data	*data = wp->modedata;

	data->callbackfn = callbackfn;
	if (data->callbackfn == NULL)
		data->callbackfn = window_choose_default_callback;

	ARRAY_CONCAT(&data->old_list, &data->list);

	window_choose_set_current(wp, cur);
	window_choose_collapse_all(wp);
}

struct screen *
window_choose_init(struct window_pane *wp)
{
	struct window_choose_mode_data	*data;
	struct screen			*s;
	int				 keys;

	wp->modedata = data = xmalloc(sizeof *data);

	data->callbackfn = NULL;
	data->input_type = WINDOW_CHOOSE_NORMAL;
	data->input_str = xstrdup("");
	data->input_prompt = NULL;

	ARRAY_INIT(&data->list);
	ARRAY_INIT(&data->old_list);
	data->top = 0;

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	s->mode &= ~MODE_CURSOR;

	keys = options_get_number(&wp->window->options, "mode-keys");
	if (keys == MODEKEY_EMACS)
		mode_key_init(&data->mdata, &mode_key_tree_emacs_choice);
	else
		mode_key_init(&data->mdata, &mode_key_tree_vi_choice);

	return (s);
}

struct window_choose_data *
window_choose_data_create(int type, struct client *c, struct session *s)
{
	struct window_choose_data	*wcd;

	wcd = xmalloc(sizeof *wcd);
	wcd->type = type;

	wcd->ft = format_create();
	wcd->ft_template = NULL;

	wcd->command = NULL;

	wcd->wl = NULL;
	wcd->pane_id = -1;
	wcd->idx = -1;

	wcd->tree_session = NULL;

	wcd->start_client = c;
	wcd->start_client->references++;
	wcd->start_session = s;
	wcd->start_session->references++;

	return (wcd);
}

void
window_choose_data_free(struct window_choose_data *wcd)
{
	server_client_unref(wcd->start_client);
	session_unref(wcd->start_session);

	if (wcd->tree_session != NULL)
		session_unref(wcd->tree_session);

	free(wcd->ft_template);
	format_free(wcd->ft);

	free(wcd->command);
	free(wcd);
}

void
window_choose_data_run(struct window_choose_data *cdata)
{
	struct cmd_list	*cmdlist;
	char		*cause;

	/*
	 * The command template will have already been replaced. But if it's
	 * NULL, bail here.
	 */
	if (cdata->command == NULL)
		return;

	if (cmd_string_parse(cdata->command, &cmdlist, NULL, 0, &cause) != 0) {
		if (cause != NULL) {
			*cause = toupper((u_char) *cause);
			status_message_set(cdata->start_client, "%s", cause);
			free(cause);
		}
		return;
	}

	cmdq_run(cdata->start_client->cmdq, cmdlist, NULL);
	cmd_list_free(cmdlist);
}

void
window_choose_default_callback(struct window_choose_data *wcd)
{
	if (wcd == NULL)
		return;
	if (wcd->start_client->flags & CLIENT_DEAD)
		return;

	window_choose_data_run(wcd);
}

void
window_choose_free(struct window_pane *wp)
{
	if (wp->modedata != NULL)
		window_choose_free1(wp->modedata);
}

void
window_choose_free1(struct window_choose_mode_data *data)
{
	struct window_choose_mode_item	*item;
	u_int				 i;

	if (data == NULL)
		return;

	for (i = 0; i < ARRAY_LENGTH(&data->old_list); i++) {
		item = &ARRAY_ITEM(&data->old_list, i);
		window_choose_data_free(item->wcd);
		free(item->name);
	}
	ARRAY_FREE(&data->list);
	ARRAY_FREE(&data->old_list);
	free(data->input_str);

	screen_free(&data->screen);
	free(data);
}

void
window_choose_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	window_choose_reset_top(wp, sy);
	screen_resize(s, sx, sy, 0);
	window_choose_redraw_screen(wp);
}

void
window_choose_fire_callback(
    struct window_pane *wp, struct window_choose_data *wcd)
{
	struct window_choose_mode_data	*data = wp->modedata;

	wp->modedata = NULL;
	window_pane_reset_mode(wp);

	data->callbackfn(wcd);

	window_choose_free1(data);
}

void
window_choose_prompt_input(enum window_choose_input_type input_type,
    const char *prompt, struct window_pane *wp, int key)
{
	struct window_choose_mode_data	*data = wp->modedata;
	size_t				 input_len;

	data->input_type = input_type;
	data->input_prompt = prompt;
	input_len = strlen(data->input_str) + 2;

	data->input_str = xrealloc(data->input_str, input_len);
	data->input_str[input_len - 2] = key;
	data->input_str[input_len - 1] = '\0';

	window_choose_redraw_screen(wp);
}

void
window_choose_collapse(struct window_pane *wp, struct session *s, u_int pos)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item, *chosen;
	struct window_choose_data	*wcd;
	u_int				 i;

	ARRAY_DECL(, struct window_choose_mode_item) list_copy;
	ARRAY_INIT(&list_copy);

	chosen = &ARRAY_ITEM(&data->list, pos);
	chosen->state &= ~TREE_EXPANDED;

	/*
	 * Trying to mangle the &data->list in-place has lots of problems, so
	 * assign the actual result we want to render and copy the new one over
	 * the top of it.
	 */
	for (i = 0; i < ARRAY_LENGTH(&data->list); i++) {
		item = &ARRAY_ITEM(&data->list, i);
		wcd = item->wcd;

		if (s == wcd->tree_session) {
			/* We only show the session when collapsed. */
			if (wcd->type & TREE_SESSION) {
				item->state &= ~TREE_EXPANDED;
				ARRAY_ADD(&list_copy, *item);

				/*
				 * Update the selection to this session item so
				 * we don't end up highlighting a non-existent
				 * item.
				 */
				data->selected = i;
			}
		} else
			ARRAY_ADD(&list_copy, ARRAY_ITEM(&data->list, i));
	}

	if (!ARRAY_EMPTY(&list_copy)) {
		ARRAY_FREE(&data->list);
		ARRAY_CONCAT(&data->list, &list_copy);
		ARRAY_FREE(&list_copy);
	}
}

void
window_choose_collapse_all(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	struct screen			*scr = &data->screen;
	struct session			*s, *chosen;
	u_int				 i;

	chosen = ARRAY_ITEM(&data->list, data->selected).wcd->start_session;

	RB_FOREACH(s, sessions, &sessions)
		window_choose_collapse(wp, s, data->selected);

	/* Reset the selection back to the starting session. */
	for (i = 0; i < ARRAY_LENGTH(&data->list); i++) {
		item = &ARRAY_ITEM(&data->list, i);

		if (chosen != item->wcd->tree_session)
			continue;

		if (item->wcd->type & TREE_SESSION)
			data->selected = i;
	}
	window_choose_reset_top(wp, screen_size_y(scr));
}

void
window_choose_expand_all(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	struct screen			*scr = &data->screen;
	struct session			*s;
	u_int				 i;

	RB_FOREACH(s, sessions, &sessions) {
		for (i = 0; i < ARRAY_LENGTH(&data->list); i++) {
			item = &ARRAY_ITEM(&data->list, i);

			if (s != item->wcd->tree_session)
				continue;

			if (item->wcd->type & TREE_SESSION)
				window_choose_expand(wp, s, i);
		}
	}

	window_choose_reset_top(wp, screen_size_y(scr));
}

void
window_choose_expand(struct window_pane *wp, struct session *s, u_int pos)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item, *chosen;
	struct window_choose_data	*wcd;
	u_int				 i, items;

	chosen = &ARRAY_ITEM(&data->list, pos);
	items = ARRAY_LENGTH(&data->old_list) - 1;

	/* It's not possible to expand anything other than sessions. */
	if (!(chosen->wcd->type & TREE_SESSION))
		return;

	/* Don't re-expand a session which is already expanded. */
	if (chosen->state & TREE_EXPANDED)
		return;

	/* Mark the session entry as expanded. */
	chosen->state |= TREE_EXPANDED;

	/*
	 * Go back through the original list of all sessions and windows, and
	 * pull out the windows where the session matches the selection chosen
	 * to expand.
	 */
	for (i = items; i > 0; i--) {
		item = &ARRAY_ITEM(&data->old_list, i);
		item->state |= TREE_EXPANDED;
		wcd = item->wcd;

		if (s == wcd->tree_session) {
			/*
			 * Since the session is already displayed, we only care
			 * to add back in window for it.
			 */
			if (wcd->type & TREE_WINDOW) {
				/*
				 * If the insertion point for adding the
				 * windows to the session falls inside the
				 * range of the list, then we insert these
				 * entries in order *AFTER* the selected
				 * session.
				 */
				if (pos < i ) {
					ARRAY_INSERT(&data->list,
					    pos + 1,
					    ARRAY_ITEM(&data->old_list,
					    i));
				} else {
					/* Ran out of room, add to the end. */
					ARRAY_ADD(&data->list,
					    ARRAY_ITEM(&data->old_list,
					    i));
				}
			}
		}
	}
}

struct window_choose_mode_item *
window_choose_get_item(struct window_pane *wp, int key, struct mouse_event *m)
{
	struct window_choose_mode_data	*data = wp->modedata;
	u_int				 x, y, idx;

	if (!KEYC_IS_MOUSE(key))
		return (&ARRAY_ITEM(&data->list, data->selected));

	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return (NULL);

	idx = data->top + y;
	if (idx >= ARRAY_LENGTH(&data->list))
		return (NULL);
	return (&ARRAY_ITEM(&data->list, idx));
}

void
window_choose_key(struct window_pane *wp, unused struct client *c,
    unused struct session *sess, int key, struct mouse_event *m)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	struct window_choose_mode_item	*item;
	size_t				 input_len;
	u_int				 items, n;
	int				 idx;

	items = ARRAY_LENGTH(&data->list);

	if (data->input_type == WINDOW_CHOOSE_GOTO_ITEM) {
		switch (mode_key_lookup(&data->mdata, key, NULL)) {
		case MODEKEYCHOICE_CANCEL:
			data->input_type = WINDOW_CHOOSE_NORMAL;
			window_choose_redraw_screen(wp);
			break;
		case MODEKEYCHOICE_CHOOSE:
			n = strtonum(data->input_str, 0, INT_MAX, NULL);
			if (n > items - 1) {
				data->input_type = WINDOW_CHOOSE_NORMAL;
				window_choose_redraw_screen(wp);
				break;
			}
			item = &ARRAY_ITEM(&data->list, n);
			window_choose_fire_callback(wp, item->wcd);
			break;
		case MODEKEYCHOICE_BACKSPACE:
			input_len = strlen(data->input_str);
			if (input_len > 0)
				data->input_str[input_len - 1] = '\0';
			window_choose_redraw_screen(wp);
			break;
		default:
			if (key < '0' || key > '9')
				break;
			window_choose_prompt_input(WINDOW_CHOOSE_GOTO_ITEM,
			    "Goto Item", wp, key);
			break;
		}
		return;
	}

	switch (mode_key_lookup(&data->mdata, key, NULL)) {
	case MODEKEYCHOICE_CANCEL:
		window_choose_fire_callback(wp, NULL);
		break;
	case MODEKEYCHOICE_CHOOSE:
		if ((item = window_choose_get_item(wp, key, m)) == NULL)
			break;
		window_choose_fire_callback(wp, item->wcd);
		break;
	case MODEKEYCHOICE_TREE_TOGGLE:
		if ((item = window_choose_get_item(wp, key, m)) == NULL)
			break;
		if (item->state & TREE_EXPANDED) {
			window_choose_collapse(wp, item->wcd->tree_session,
			    data->selected);
		} else {
			window_choose_expand(wp, item->wcd->tree_session,
			    data->selected);
		}
		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCHOICE_TREE_COLLAPSE:
		if ((item = window_choose_get_item(wp, key, m)) == NULL)
			break;
		if (item->state & TREE_EXPANDED) {
			window_choose_collapse(wp, item->wcd->tree_session,
			    data->selected);
			window_choose_redraw_screen(wp);
		}
		break;
	case MODEKEYCHOICE_TREE_COLLAPSE_ALL:
		window_choose_collapse_all(wp);
		break;
	case MODEKEYCHOICE_TREE_EXPAND:
		if ((item = window_choose_get_item(wp, key, m)) == NULL)
			break;
		if (!(item->state & TREE_EXPANDED)) {
			window_choose_expand(wp, item->wcd->tree_session,
			    data->selected);
			window_choose_redraw_screen(wp);
		}
		break;
	case MODEKEYCHOICE_TREE_EXPAND_ALL:
		window_choose_expand_all(wp);
		break;
	case MODEKEYCHOICE_UP:
		if (items == 0)
			break;
		if (data->selected == 0) {
			data->selected = items - 1;
			if (data->selected > screen_size_y(s) - 1)
				data->top = items - screen_size_y(s);
			window_choose_redraw_screen(wp);
			break;
		}
		data->selected--;
		if (data->selected < data->top)
			window_choose_scroll_up(wp);
		else {
			screen_write_start(&ctx, wp, NULL);
			window_choose_write_line(
			    wp, &ctx, data->selected - data->top);
			window_choose_write_line(
			    wp, &ctx, data->selected + 1 - data->top);
			screen_write_stop(&ctx);
		}
		break;
	case MODEKEYCHOICE_DOWN:
		if (items == 0)
			break;
		if (data->selected == items - 1) {
			data->selected = 0;
			data->top = 0;
			window_choose_redraw_screen(wp);
			break;
		}
		data->selected++;

		if (data->selected < data->top + screen_size_y(s)) {
			screen_write_start(&ctx, wp, NULL);
			window_choose_write_line(
			    wp, &ctx, data->selected - data->top);
			window_choose_write_line(
			    wp, &ctx, data->selected - 1 - data->top);
			screen_write_stop(&ctx);
		} else
			window_choose_scroll_down(wp);
		break;
	case MODEKEYCHOICE_SCROLLUP:
		if (items == 0 || data->top == 0)
			break;
		if (data->selected == data->top + screen_size_y(s) - 1) {
			data->selected--;
			window_choose_scroll_up(wp);
			screen_write_start(&ctx, wp, NULL);
			window_choose_write_line(
			    wp, &ctx, screen_size_y(s) - 1);
			screen_write_stop(&ctx);
		} else
			window_choose_scroll_up(wp);
		break;
	case MODEKEYCHOICE_SCROLLDOWN:
		if (items == 0 ||
		    data->top + screen_size_y(&data->screen) >= items)
			break;
		if (data->selected == data->top) {
			data->selected++;
			window_choose_scroll_down(wp);
			screen_write_start(&ctx, wp, NULL);
			window_choose_write_line(wp, &ctx, 0);
			screen_write_stop(&ctx);
		} else
			window_choose_scroll_down(wp);
		break;
	case MODEKEYCHOICE_PAGEUP:
		if (data->selected < screen_size_y(s)) {
			data->selected = 0;
			data->top = 0;
		} else {
			data->selected -= screen_size_y(s);
			if (data->top < screen_size_y(s))
				data->top = 0;
			else
				data->top -= screen_size_y(s);
		}
		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCHOICE_PAGEDOWN:
		data->selected += screen_size_y(s);
		if (data->selected > items - 1)
			data->selected = items - 1;
		data->top += screen_size_y(s);
		if (screen_size_y(s) < items) {
			if (data->top + screen_size_y(s) > items)
				data->top = items - screen_size_y(s);
		} else
			data->top = 0;
		if (data->selected < data->top)
			data->top = data->selected;
		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCHOICE_BACKSPACE:
		input_len = strlen(data->input_str);
		if (input_len > 0)
			data->input_str[input_len - 1] = '\0';
		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCHOICE_STARTNUMBERPREFIX:
		key &= KEYC_MASK_KEY;
		if (key < '0' || key > '9')
			break;
		window_choose_prompt_input(WINDOW_CHOOSE_GOTO_ITEM,
		    "Goto Item", wp, key);
		break;
	case MODEKEYCHOICE_STARTOFLIST:
		data->selected = 0;
		data->top = 0;
		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCHOICE_TOPLINE:
		data->selected = data->top;
		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCHOICE_BOTTOMLINE:
		data->selected = data->top + screen_size_y(s) - 1;
		if (data->selected > items - 1)
			data->selected = items - 1;
		window_choose_redraw_screen(wp);
		break;
	case MODEKEYCHOICE_ENDOFLIST:
		data->selected = items - 1;
		if (screen_size_y(s) < items)
			data->top = items - screen_size_y(s);
		else
			data->top = 0;
		window_choose_redraw_screen(wp);
		break;
	default:
		idx = window_choose_index_key(data, key);
		if (idx < 0 || (u_int) idx >= ARRAY_LENGTH(&data->list))
			break;
		data->selected = idx;

		item = &ARRAY_ITEM(&data->list, data->selected);
		window_choose_fire_callback(wp, item->wcd);
		break;
	}
}

void
window_choose_write_line(
    struct window_pane *wp, struct screen_write_ctx *ctx, u_int py)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	struct options			*oo = &wp->window->options;
	struct screen			*s = &data->screen;
	struct grid_cell		 gc;
	size_t				 last, xoff = 0;
	char				 hdr[32], label[32];
	int				 utf8flag, key;

	if (data->callbackfn == NULL)
		fatalx("called before callback assigned");

	last = screen_size_y(s) - 1;
	utf8flag = options_get_number(&wp->window->options, "utf8");
	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (data->selected == data->top + py)
		style_apply(&gc, oo, "mode-style");

	screen_write_cursormove(ctx, 0, py);
	if (data->top + py  < ARRAY_LENGTH(&data->list)) {
		item = &ARRAY_ITEM(&data->list, data->top + py);
		if (item->wcd->wl != NULL &&
		    item->wcd->wl->flags & WINLINK_ALERTFLAGS)
			gc.attr |= GRID_ATTR_BRIGHT;

		key = window_choose_key_index(data, data->top + py);
		if (key != -1)
			xsnprintf(label, sizeof label, "(%c)", key);
		else
			xsnprintf(label, sizeof label, "(%d)", item->pos);
		screen_write_nputs(ctx, screen_size_x(s) - 1, &gc, utf8flag,
		    "%*s %s %s", data->width + 2, label,
		    /*
		     * Add indication to tree if necessary about whether it's
		     * expanded or not.
		     */
		    (item->wcd->type & TREE_SESSION) ?
		    (item->state & TREE_EXPANDED ? "-" : "+") : "", item->name);
	}
	while (s->cx < screen_size_x(s) - 1)
		screen_write_putc(ctx, &gc, ' ');

	if (data->input_type != WINDOW_CHOOSE_NORMAL) {
		style_apply(&gc, oo, "mode-style");

		xoff = xsnprintf(hdr, sizeof hdr,
			"%s: %s", data->input_prompt, data->input_str);
		screen_write_cursormove(ctx, 0, last);
		screen_write_puts(ctx, &gc, "%s", hdr);
		screen_write_cursormove(ctx, xoff, py);
		memcpy(&gc, &grid_default_cell, sizeof gc);
	}

}

int
window_choose_key_index(struct window_choose_mode_data *data, u_int idx)
{
	static const char	keys[] = "0123456789"
	                                 "abcdefghijklmnopqrstuvwxyz"
	                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char	       *ptr;
	int			mkey;

	for (ptr = keys; *ptr != '\0'; ptr++) {
		mkey = mode_key_lookup(&data->mdata, *ptr, NULL);
		if (mkey != MODEKEY_NONE && mkey != MODEKEY_OTHER)
			continue;
		if (idx-- == 0)
			return (*ptr);
	}
	return (-1);
}

int
window_choose_index_key(struct window_choose_mode_data *data, int key)
{
	static const char	keys[] = "0123456789"
	                                 "abcdefghijklmnopqrstuvwxyz"
	                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char	       *ptr;
	int			mkey;
	u_int			idx = 0;

	for (ptr = keys; *ptr != '\0'; ptr++) {
		mkey = mode_key_lookup(&data->mdata, *ptr, NULL);
		if (mkey != MODEKEY_NONE && mkey != MODEKEY_OTHER)
			continue;
		if (key == *ptr)
			return (idx);
		idx++;
	}
	return (-1);
}

void
window_choose_redraw_screen(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start(&ctx, wp, NULL);
	for (i = 0; i < screen_size_y(s); i++)
		window_choose_write_line(wp, &ctx, i);
	screen_write_stop(&ctx);
}

void
window_choose_scroll_up(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen_write_ctx		 ctx;

	if (data->top == 0)
		return;
	data->top--;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_insertline(&ctx, 1);
	window_choose_write_line(wp, &ctx, 0);
	if (screen_size_y(&data->screen) > 1)
		window_choose_write_line(wp, &ctx, 1);
	screen_write_stop(&ctx);
}

void
window_choose_scroll_down(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->top >= ARRAY_LENGTH(&data->list))
		return;
	data->top++;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_deleteline(&ctx, 1);
	window_choose_write_line(wp, &ctx, screen_size_y(s) - 1);
	if (screen_size_y(&data->screen) > 1)
		window_choose_write_line(wp, &ctx, screen_size_y(s) - 2);
	screen_write_stop(&ctx);
}

struct window_choose_data *
window_choose_add_session(struct window_pane *wp, struct client *c,
    struct session *s, const char *template, const char *action, u_int idx)
{
	struct window_choose_data	*wcd;

	wcd = window_choose_data_create(TREE_SESSION, c, c->session);
	wcd->idx = s->id;

	wcd->tree_session = s;
	wcd->tree_session->references++;

	wcd->ft_template = xstrdup(template);
	format_add(wcd->ft, "line", "%u", idx);
	format_defaults(wcd->ft, NULL, s, NULL, NULL);

	wcd->command = cmd_template_replace(action, s->name, 1);

	window_choose_add(wp, wcd);

	return (wcd);
}

struct window_choose_data *
window_choose_add_window(struct window_pane *wp, struct client *c,
    struct session *s, struct winlink *wl, const char *template,
    const char *action, u_int idx)
{
	struct window_choose_data	*wcd;
	char				*expanded;

	wcd = window_choose_data_create(TREE_WINDOW, c, c->session);
	wcd->idx = wl->idx;

	wcd->wl = wl;

	wcd->tree_session = s;
	wcd->tree_session->references++;

	wcd->ft_template = xstrdup(template);
	format_add(wcd->ft, "line", "%u", idx);
	format_defaults(wcd->ft, NULL, s, wl, NULL);

	xasprintf(&expanded, "%s:%d", s->name, wl->idx);
	wcd->command = cmd_template_replace(action, expanded, 1);
	free(expanded);

	window_choose_add(wp, wcd);

	return (wcd);
}
