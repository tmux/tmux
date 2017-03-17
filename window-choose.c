/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

static struct screen *window_choose_init(struct window_pane *);
static void	window_choose_free(struct window_pane *);
static void	window_choose_resize(struct window_pane *, u_int, u_int);
static void	window_choose_key(struct window_pane *, struct client *,
		    struct session *, key_code, struct mouse_event *);

static void	window_choose_default_callback(struct window_choose_data *);
static struct window_choose_mode_item *window_choose_get_item(
		    struct window_pane *, key_code, struct mouse_event *);

static void	window_choose_fire_callback(struct window_pane *,
		    struct window_choose_data *);
static void	window_choose_redraw_screen(struct window_pane *);
static void	window_choose_write_line(struct window_pane *,
		    struct screen_write_ctx *, u_int);

static void	window_choose_scroll_up(struct window_pane *);
static void	window_choose_scroll_down(struct window_pane *);

static void	window_choose_collapse(struct window_pane *, struct session *,
		    u_int);
static void	window_choose_expand(struct window_pane *, struct session *,
		    u_int);
static void	window_choose_collapse_all(struct window_pane *);

static void	window_choose_data_free(struct window_choose_data *);

enum window_choose_input_type {
	WINDOW_CHOOSE_NORMAL = -1,
	WINDOW_CHOOSE_GOTO_ITEM,
};

const struct window_mode window_choose_mode = {
	.init = window_choose_init,
	.free = window_choose_free,
	.resize = window_choose_resize,
	.key = window_choose_key,
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

	struct window_choose_mode_item *list;
	u_int			list_size;
	struct window_choose_mode_item *old_list;
	u_int			old_list_size;

	int			width;
	u_int			top;
	u_int			selected;
	enum window_choose_input_type input_type;
	const char		*input_prompt;
	char			*input_str;

	void 			(*callbackfn)(struct window_choose_data *);
};

static const char window_choose_keys_emacs[] = "0123456789"
	                                       "abcdefghijklmnoprstuvwxyz"
	                                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char window_choose_keys_vi[] = "0123456789"
	                                    "abcdefimnoprstuvwxyz"
	                                    "ABCDEFIJKMNOPQRSTUVWXYZ";

static void	window_choose_free1(struct window_choose_mode_data *);
static int	window_choose_key_index(struct window_pane *, u_int);
static int	window_choose_index_key(struct window_pane *, key_code);
static void	window_choose_prompt_input(enum window_choose_input_type,
		    const char *, struct window_pane *, key_code);
static void	window_choose_reset_top(struct window_pane *, u_int);

void
window_choose_add(struct window_pane *wp, struct window_choose_data *wcd)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	char				 tmp[11];

	data->list = xreallocarray(data->list, data->list_size + 1,
	    sizeof *data->list);
	item = &data->list[data->list_size++];

	item->name = format_expand(wcd->ft, wcd->ft_template);
	item->wcd = wcd;
	item->pos = data->list_size - 1;
	item->state = 0;

	data->width = xsnprintf(tmp, sizeof tmp, "%d", item->pos);
}

void
window_choose_set_current(struct window_pane *wp, u_int cur)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	data->selected = cur;
	window_choose_reset_top(wp, screen_size_y(s));
}

static void
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
	u_int				 size;

	data->callbackfn = callbackfn;
	if (data->callbackfn == NULL)
		data->callbackfn = window_choose_default_callback;

	size = data->old_list_size;
	data->old_list_size += data->list_size;
	data->old_list = xreallocarray(data->old_list, data->old_list_size,
	    sizeof *data->old_list);
	memcpy(data->old_list + size, data->list, data->list_size *
	    sizeof *data->list);

	window_choose_set_current(wp, cur);
	window_choose_collapse_all(wp);
}

static struct screen *
window_choose_init(struct window_pane *wp)
{
	struct window_choose_mode_data	*data;
	struct screen			*s;

	wp->modedata = data = xcalloc(1, sizeof *data);

	data->callbackfn = NULL;
	data->input_type = WINDOW_CHOOSE_NORMAL;
	data->input_str = xstrdup("");
	data->input_prompt = NULL;

	data->list = NULL;
	data->list_size = 0;

	data->old_list = NULL;
	data->old_list_size = 0;

	data->top = 0;

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	s->mode &= ~MODE_CURSOR;

	return (s);
}

struct window_choose_data *
window_choose_data_create(int type, struct client *c, struct session *s)
{
	struct window_choose_data	*wcd;

	wcd = xmalloc(sizeof *wcd);
	wcd->type = type;

	wcd->ft = format_create(NULL, FORMAT_NONE, 0);
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

static void
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
	struct cmd_list		*cmdlist;
	char			*cause;
	struct cmdq_item	*item;

	/*
	 * The command template will have already been replaced. But if it's
	 * NULL, bail here.
	 */
	if (cdata->command == NULL)
		return;

	cmdlist = cmd_string_parse(cdata->command, NULL, 0, &cause);
	if (cmdlist == NULL) {
		if (cause != NULL) {
			*cause = toupper((u_char) *cause);
			status_message_set(cdata->start_client, "%s", cause);
			free(cause);
		}
		return;
	}

	item = cmdq_get_command(cmdlist, NULL, NULL, 0);
	cmdq_append(cdata->start_client, item);
	cmd_list_free(cmdlist);
}

static void
window_choose_default_callback(struct window_choose_data *wcd)
{
	if (wcd == NULL)
		return;
	if (wcd->start_client->flags & CLIENT_DEAD)
		return;

	window_choose_data_run(wcd);
}

static void
window_choose_free(struct window_pane *wp)
{
	if (wp->modedata != NULL)
		window_choose_free1(wp->modedata);
}

static void
window_choose_free1(struct window_choose_mode_data *data)
{
	struct window_choose_mode_item	*item;
	u_int				 i;

	if (data == NULL)
		return;

	for (i = 0; i < data->old_list_size; i++) {
		item = &data->old_list[i];
		window_choose_data_free(item->wcd);
		free(item->name);
	}
	free(data->list);
	free(data->old_list);

	free(data->input_str);

	screen_free(&data->screen);
	free(data);
}

static void
window_choose_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	window_choose_reset_top(wp, sy);
	screen_resize(s, sx, sy, 0);
	window_choose_redraw_screen(wp);
}

static void
window_choose_fire_callback(struct window_pane *wp,
    struct window_choose_data *wcd)
{
	struct window_choose_mode_data	*data = wp->modedata;

	wp->modedata = NULL;
	window_pane_reset_mode(wp);

	data->callbackfn(wcd);

	window_choose_free1(data);
}

static void
window_choose_prompt_input(enum window_choose_input_type input_type,
    const char *prompt, struct window_pane *wp, key_code key)
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

static void
window_choose_collapse(struct window_pane *wp, struct session *s, u_int pos)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item, *chosen, *copy = NULL;
	struct window_choose_data	*wcd;
	u_int				 i, copy_size = 0;

	chosen = &data->list[pos];
	chosen->state &= ~TREE_EXPANDED;

	/*
	 * Trying to mangle the &data->list in-place has lots of problems, so
	 * assign the actual result we want to render and copy the new one over
	 * the top of it.
	 */
	for (i = 0; i < data->list_size; i++) {
		item = &data->list[i];
		wcd = item->wcd;

		if (s == wcd->tree_session) {
			/* We only show the session when collapsed. */
			if (wcd->type & TREE_SESSION) {
				item->state &= ~TREE_EXPANDED;

				copy = xreallocarray(copy, copy_size + 1,
				    sizeof *copy);
				memcpy(&copy[copy_size], item, sizeof *copy);
				copy_size++;

				/*
				 * Update the selection to this session item so
				 * we don't end up highlighting a non-existent
				 * item.
				 */
				data->selected = i;
			}
		} else {
			copy = xreallocarray(copy, copy_size + 1, sizeof *copy);
			memcpy(&copy[copy_size], item, sizeof *copy);
			copy_size++;
		}
	}

	if (copy_size != 0) {
		free(data->list);
		data->list = copy;
		data->list_size = copy_size;
	}
}

static void
window_choose_collapse_all(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	struct screen			*scr = &data->screen;
	struct session			*s, *chosen;
	u_int				 i;

	chosen = data->list[data->selected].wcd->start_session;

	RB_FOREACH(s, sessions, &sessions)
		window_choose_collapse(wp, s, data->selected);

	/* Reset the selection back to the starting session. */
	for (i = 0; i < data->list_size; i++) {
		item = &data->list[i];

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
		for (i = 0; i < data->list_size; i++) {
			item = &data->list[i];

			if (s != item->wcd->tree_session)
				continue;

			if (item->wcd->type & TREE_SESSION)
				window_choose_expand(wp, s, i);
		}
	}

	window_choose_reset_top(wp, screen_size_y(scr));
}

static void
window_choose_expand(struct window_pane *wp, struct session *s, u_int pos)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item, *chosen;
	struct window_choose_data	*wcd;
	u_int				 i, items;

	chosen = &data->list[pos];
	items = data->old_list_size - 1;

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
		item = &data->old_list[i];
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
				if (pos < i) {
					data->list = xreallocarray(data->list,
					    data->list_size + 1,
					    sizeof *data->list);
					memmove(&data->list[pos + 2],
					    &data->list[pos + 1],
					    (data->list_size - (pos + 1)) *
					    sizeof *data->list);
					memcpy(&data->list[pos + 1],
					    &data->old_list[i],
					    sizeof *data->list);
					data->list_size++;
				} else {
					/* Ran out of room, add to the end. */
					data->list = xreallocarray(data->list,
					    data->list_size + 1,
					    sizeof *data->list);
					memcpy(&data->list[data->list_size],
					    &data->old_list[i],
					    sizeof *data->list);
					data->list_size++;
				}
			}
		}
	}
}

static struct window_choose_mode_item *
window_choose_get_item(struct window_pane *wp, key_code key,
    struct mouse_event *m)
{
	struct window_choose_mode_data	*data = wp->modedata;
	u_int				 x, y, idx;

	if (!KEYC_IS_MOUSE(key))
		return (&data->list[data->selected]);

	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return (NULL);

	idx = data->top + y;
	if (idx >= data->list_size)
		return (NULL);
	return (&data->list[idx]);
}

static key_code
window_choose_translate_key(key_code key)
{
	switch (key) {
	case '0'|KEYC_ESCAPE:
	case '1'|KEYC_ESCAPE:
	case '2'|KEYC_ESCAPE:
	case '3'|KEYC_ESCAPE:
	case '4'|KEYC_ESCAPE:
	case '5'|KEYC_ESCAPE:
	case '6'|KEYC_ESCAPE:
	case '7'|KEYC_ESCAPE:
	case '8'|KEYC_ESCAPE:
	case '9'|KEYC_ESCAPE:
	case '\003': /* C-c */
	case 'q':
	case '\n':
	case '\r':
	case KEYC_BSPACE:
	case ' ':
	case KEYC_LEFT|KEYC_CTRL:
	case KEYC_RIGHT|KEYC_CTRL:
	case KEYC_MOUSEDOWN1_PANE:
	case KEYC_MOUSEDOWN3_PANE:
	case KEYC_WHEELUP_PANE:
	case KEYC_WHEELDOWN_PANE:
		return (key);
	case '\031': /* C-y */
	case KEYC_UP|KEYC_CTRL:
		return (KEYC_UP|KEYC_CTRL);
	case '\002': /* C-b */
	case KEYC_PPAGE:
		return (KEYC_PPAGE);
	case '\005': /* C-e */
	case KEYC_DOWN|KEYC_CTRL:
		return (KEYC_DOWN|KEYC_CTRL);
	case '\006': /* C-f */
	case KEYC_NPAGE:
		return (KEYC_NPAGE);
	case 'h':
	case KEYC_LEFT:
		return (KEYC_LEFT);
	case 'j':
	case KEYC_DOWN:
		return (KEYC_DOWN);
	case 'k':
	case KEYC_UP:
		return (KEYC_UP);
	case 'l':
	case KEYC_RIGHT:
		return (KEYC_RIGHT);
	case 'g':
	case KEYC_HOME:
		return (KEYC_HOME);
	case 'G':
	case KEYC_END:
		return (KEYC_END);
	case 'H':
		return ('R'|KEYC_ESCAPE);
	case 'L':
		return ('r'|KEYC_ESCAPE);
	}
	if ((key >= '0' && key <= '9') ||
	    (key >= 'a' && key <= 'z') ||
	    (key >= 'A' && key <= 'Z'))
		return (key);
	return (KEYC_NONE);
}

static void
window_choose_key(struct window_pane *wp, __unused struct client *c,
    __unused struct session *sp, key_code key, struct mouse_event *m)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	struct window_choose_mode_item	*item;
	size_t				 input_len;
	u_int				 items, n;
	int				 idx, keys;

	keys = options_get_number(wp->window->options, "mode-keys");
	if (keys == MODEKEY_VI) {
		key = window_choose_translate_key(key);
		if (key == KEYC_NONE)
			return;
	}
	items = data->list_size;

	if (data->input_type == WINDOW_CHOOSE_GOTO_ITEM) {
		switch (key) {
		case '\003': /* C-c */
		case '\033': /* Escape */
		case 'q':
			data->input_type = WINDOW_CHOOSE_NORMAL;
			window_choose_redraw_screen(wp);
			break;
		case '\n':
		case '\r':
			n = strtonum(data->input_str, 0, INT_MAX, NULL);
			if (n > items - 1) {
				data->input_type = WINDOW_CHOOSE_NORMAL;
				window_choose_redraw_screen(wp);
				break;
			}
			window_choose_fire_callback(wp, data->list[n].wcd);
			break;
		case KEYC_BSPACE:
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

	switch (key) {
	case '\003': /* C-c */
	case '\033': /* Escape */
	case 'q':
		window_choose_fire_callback(wp, NULL);
		break;
	case '\n':
	case '\r':
	case KEYC_MOUSEDOWN1_PANE:
		if ((item = window_choose_get_item(wp, key, m)) == NULL)
			break;
		window_choose_fire_callback(wp, item->wcd);
		break;
	case ' ':
	case KEYC_MOUSEDOWN3_PANE:
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
	case KEYC_LEFT:
		if ((item = window_choose_get_item(wp, key, m)) == NULL)
			break;
		if (item->state & TREE_EXPANDED) {
			window_choose_collapse(wp, item->wcd->tree_session,
			    data->selected);
			window_choose_redraw_screen(wp);
		}
		break;
	case KEYC_LEFT|KEYC_CTRL:
		window_choose_collapse_all(wp);
		break;
	case KEYC_RIGHT:
		if ((item = window_choose_get_item(wp, key, m)) == NULL)
			break;
		if (!(item->state & TREE_EXPANDED)) {
			window_choose_expand(wp, item->wcd->tree_session,
			    data->selected);
			window_choose_redraw_screen(wp);
		}
		break;
	case KEYC_RIGHT|KEYC_CTRL:
		window_choose_expand_all(wp);
		break;
	case '\020': /* C-p */
	case KEYC_UP:
	case KEYC_WHEELUP_PANE:
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
			window_choose_write_line(wp, &ctx,
			    data->selected - data->top);
			window_choose_write_line(wp, &ctx,
			    data->selected + 1 - data->top);
			screen_write_stop(&ctx);
		}
		break;
	case '\016': /* C-n */
	case KEYC_DOWN:
	case KEYC_WHEELDOWN_PANE:
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
			window_choose_write_line(wp, &ctx,
			    data->selected - data->top);
			window_choose_write_line(wp, &ctx,
			    data->selected - 1 - data->top);
			screen_write_stop(&ctx);
		} else
			window_choose_scroll_down(wp);
		break;
	case KEYC_UP|KEYC_CTRL:
		if (items == 0 || data->top == 0)
			break;
		if (data->selected == data->top + screen_size_y(s) - 1) {
			data->selected--;
			window_choose_scroll_up(wp);
			screen_write_start(&ctx, wp, NULL);
			window_choose_write_line(wp, &ctx,
			    screen_size_y(s) - 1);
			screen_write_stop(&ctx);
		} else
			window_choose_scroll_up(wp);
		break;
	case KEYC_DOWN|KEYC_CTRL:
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
	case KEYC_PPAGE:
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
	case KEYC_NPAGE:
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
	case KEYC_BSPACE:
		input_len = strlen(data->input_str);
		if (input_len > 0)
			data->input_str[input_len - 1] = '\0';
		window_choose_redraw_screen(wp);
		break;
	case '0'|KEYC_ESCAPE:
	case '1'|KEYC_ESCAPE:
	case '2'|KEYC_ESCAPE:
	case '3'|KEYC_ESCAPE:
	case '4'|KEYC_ESCAPE:
	case '5'|KEYC_ESCAPE:
	case '6'|KEYC_ESCAPE:
	case '7'|KEYC_ESCAPE:
	case '8'|KEYC_ESCAPE:
	case '9'|KEYC_ESCAPE:
		key &= KEYC_MASK_KEY;
		if (key < '0' || key > '9')
			break;
		window_choose_prompt_input(WINDOW_CHOOSE_GOTO_ITEM,
		    "Goto Item", wp, key);
		break;
	case KEYC_HOME:
	case '<'|KEYC_ESCAPE:
		data->selected = 0;
		data->top = 0;
		window_choose_redraw_screen(wp);
		break;
	case 'R'|KEYC_ESCAPE:
		data->selected = data->top;
		window_choose_redraw_screen(wp);
		break;
	case 'r'|KEYC_ESCAPE:
		data->selected = data->top + screen_size_y(s) - 1;
		if (data->selected > items - 1)
			data->selected = items - 1;
		window_choose_redraw_screen(wp);
		break;
	case KEYC_END:
	case '>'|KEYC_ESCAPE:
		data->selected = items - 1;
		if (screen_size_y(s) < items)
			data->top = items - screen_size_y(s);
		else
			data->top = 0;
		window_choose_redraw_screen(wp);
		break;
	default:
		idx = window_choose_index_key(wp, key);
		if (idx < 0 || (u_int) idx >= data->list_size)
			break;
		data->selected = idx;
		window_choose_fire_callback(wp, data->list[idx].wcd);
		break;
	}
}

static void
window_choose_write_line(struct window_pane *wp, struct screen_write_ctx *ctx,
    u_int py)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct window_choose_mode_item	*item;
	struct options			*oo = wp->window->options;
	struct screen			*s = &data->screen;
	struct grid_cell		 gc;
	size_t				 last, xoff = 0;
	char				 hdr[32], label[32];
	int				 key;

	if (data->callbackfn == NULL)
		fatalx("called before callback assigned");

	last = screen_size_y(s) - 1;
	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.flags |= GRID_FLAG_NOPALETTE;
	if (data->selected == data->top + py)
		style_apply(&gc, oo, "mode-style");

	screen_write_cursormove(ctx, 0, py);
	if (data->top + py  < data->list_size) {
		item = &data->list[data->top + py];
		if (item->wcd->wl != NULL &&
		    item->wcd->wl->flags & WINLINK_ALERTFLAGS)
			gc.attr |= GRID_ATTR_BRIGHT;

		key = window_choose_key_index(wp, data->top + py);
		if (key != -1)
			xsnprintf(label, sizeof label, "(%c)", key);
		else
			xsnprintf(label, sizeof label, "(%d)", item->pos);
		screen_write_nputs(ctx, screen_size_x(s) - 1, &gc,
		    "%*s %s %s", data->width + 2, label,
		    /*
		     * Add indication to tree if necessary about whether it's
		     * expanded or not.
		     */
		    (item->wcd->type & TREE_SESSION) ?
		    ((item->state & TREE_EXPANDED) ? "-" : "+") : "", item->name);
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

static int
window_choose_key_index(struct window_pane *wp, u_int idx)
{
	const char	*ptr;
	int		 keys;

	keys = options_get_number(wp->window->options, "mode-keys");
	if (keys == MODEKEY_VI)
		ptr = window_choose_keys_vi;
	else
		ptr = window_choose_keys_emacs;
	for (; *ptr != '\0'; ptr++) {
		if (idx-- == 0)
			return (*ptr);
	}
	return (-1);
}

static int
window_choose_index_key(struct window_pane *wp, key_code key)
{
	const char	*ptr;
	int		 keys;
	u_int		 idx = 0;

	keys = options_get_number(wp->window->options, "mode-keys");
	if (keys == MODEKEY_VI)
		ptr = window_choose_keys_vi;
	else
		ptr = window_choose_keys_emacs;
	for (; *ptr != '\0'; ptr++) {
		if (key == (key_code)*ptr)
			return (idx);
		idx++;
	}
	return (-1);
}

static void
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

static void
window_choose_scroll_up(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen_write_ctx		 ctx;

	if (data->top == 0)
		return;
	data->top--;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_insertline(&ctx, 1, 8);
	window_choose_write_line(wp, &ctx, 0);
	if (screen_size_y(&data->screen) > 1)
		window_choose_write_line(wp, &ctx, 1);
	screen_write_stop(&ctx);
}

static void
window_choose_scroll_down(struct window_pane *wp)
{
	struct window_choose_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->top >= data->list_size)
		return;
	data->top++;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_deleteline(&ctx, 1, 8);
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
