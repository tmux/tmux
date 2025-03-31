/* $OpenBSD$ */

/*
 * Copyright (c) 2017 Nicholas Marriott <nicholas.marriott@gmail.com>
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

static struct screen	*window_tree_init(struct window_mode_entry *,
			     struct cmd_find_state *, struct args *);
static void		 window_tree_free(struct window_mode_entry *);
static void		 window_tree_resize(struct window_mode_entry *, u_int,
			     u_int);
static void		 window_tree_update(struct window_mode_entry *);
static void		 window_tree_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);

#define WINDOW_TREE_DEFAULT_COMMAND "switch-client -Zt '%%'"

#define WINDOW_TREE_DEFAULT_FORMAT \
	"#{?pane_format," \
		"#{?pane_marked,#[reverse],}" \
		"#{pane_current_command}#{?pane_active,*,}#{?pane_marked,M,}" \
		"#{?#{&&:#{pane_title},#{!=:#{pane_title},#{host_short}}},: \"#{pane_title}\",}" \
	"," \
		"#{?window_format," \
			"#{?window_marked_flag,#[reverse],}" \
			"#{window_name}#{window_flags}" \
			"#{?#{&&:#{==:#{window_panes},1},#{&&:#{pane_title},#{!=:#{pane_title},#{host_short}}}},: \"#{pane_title}\",}" \
		"," \
			"#{session_windows} windows" \
			"#{?session_grouped, " \
				"(group #{session_group}: " \
				"#{session_group_list})," \
			"}" \
			"#{?session_attached, (attached),}" \
		"}" \
	"}"

#define WINDOW_TREE_DEFAULT_KEY_FORMAT \
	"#{?#{e|<:#{line},10}," \
		"#{line}" \
	"," \
		"#{?#{e|<:#{line},36},"	\
	        	"M-#{a:#{e|+:97,#{e|-:#{line},10}}}" \
		"," \
	        	"" \
		"}" \
	"}"

static const struct menu_item window_tree_menu_items[] = {
	{ "Select", '\r', NULL },
	{ "Expand", KEYC_RIGHT, NULL },
	{ "Mark", 'm', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Tag", 't', NULL },
	{ "Tag All", '\024', NULL },
	{ "Tag None", 'T', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Kill", 'x', NULL },
	{ "Kill Tagged", 'X', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Cancel", 'q', NULL },

	{ NULL, KEYC_NONE, NULL }
};

const struct window_mode window_tree_mode = {
	.name = "tree-mode",
	.default_format = WINDOW_TREE_DEFAULT_FORMAT,

	.init = window_tree_init,
	.free = window_tree_free,
	.resize = window_tree_resize,
	.update = window_tree_update,
	.key = window_tree_key,
};

enum window_tree_sort_type {
	WINDOW_TREE_BY_INDEX,
	WINDOW_TREE_BY_NAME,
	WINDOW_TREE_BY_TIME,
};
static const char *window_tree_sort_list[] = {
	"index",
	"name",
	"time"
};
static struct mode_tree_sort_criteria *window_tree_sort;

enum window_tree_type {
	WINDOW_TREE_NONE,
	WINDOW_TREE_SESSION,
	WINDOW_TREE_WINDOW,
	WINDOW_TREE_PANE,
};

struct window_tree_itemdata {
	enum window_tree_type	type;
	int			session;
	int			winlink;
	int			pane;
};

struct window_tree_modedata {
	struct window_pane		 *wp;
	int				  dead;
	int				  references;

	struct mode_tree_data		 *data;
	char				 *format;
	char				 *key_format;
	char				 *command;
	int				  squash_groups;
	int				  prompt_flags;

	struct window_tree_itemdata	**item_list;
	u_int				  item_size;

	const char			 *entered;

	struct cmd_find_state		  fs;
	enum window_tree_type		  type;

	int				  offset;

	int				  left;
	int				  right;
	u_int				  start;
	u_int				  end;
	u_int				  each;
};

static void
window_tree_pull_item(struct window_tree_itemdata *item, struct session **sp,
    struct winlink **wlp, struct window_pane **wp)
{
	*wp = NULL;
	*wlp = NULL;
	*sp = session_find_by_id(item->session);
	if (*sp == NULL)
		return;
	if (item->type == WINDOW_TREE_SESSION) {
		*wlp = (*sp)->curw;
		*wp = (*wlp)->window->active;
		return;
	}

	*wlp = winlink_find_by_index(&(*sp)->windows, item->winlink);
	if (*wlp == NULL) {
		*sp = NULL;
		return;
	}
	if (item->type == WINDOW_TREE_WINDOW) {
		*wp = (*wlp)->window->active;
		return;
	}

	*wp = window_pane_find_by_id(item->pane);
	if (!window_has_pane((*wlp)->window, *wp))
		*wp = NULL;
	if (*wp == NULL) {
		*sp = NULL;
		*wlp = NULL;
		return;
	}
}

static struct window_tree_itemdata *
window_tree_add_item(struct window_tree_modedata *data)
{
	struct window_tree_itemdata	*item;

	data->item_list = xreallocarray(data->item_list, data->item_size + 1,
	    sizeof *data->item_list);
	item = data->item_list[data->item_size++] = xcalloc(1, sizeof *item);
	return (item);
}

static void
window_tree_free_item(struct window_tree_itemdata *item)
{
	free(item);
}

static int
window_tree_cmp_session(const void *a0, const void *b0)
{
	const struct session *const	*a = a0;
	const struct session *const	*b = b0;
	const struct session		*sa = *a;
	const struct session		*sb = *b;
	int				 result = 0;

	switch (window_tree_sort->field) {
	case WINDOW_TREE_BY_INDEX:
		result = sa->id - sb->id;
		break;
	case WINDOW_TREE_BY_TIME:
		if (timercmp(&sa->activity_time, &sb->activity_time, >)) {
			result = -1;
			break;
		}
		if (timercmp(&sa->activity_time, &sb->activity_time, <)) {
			result = 1;
			break;
		}
		/* FALLTHROUGH */
	case WINDOW_TREE_BY_NAME:
		result = strcmp(sa->name, sb->name);
		break;
	}

	if (window_tree_sort->reversed)
		result = -result;
	return (result);
}

static int
window_tree_cmp_window(const void *a0, const void *b0)
{
	const struct winlink *const	*a = a0;
	const struct winlink *const	*b = b0;
	const struct winlink		*wla = *a;
	const struct winlink		*wlb = *b;
	struct window			*wa = wla->window;
	struct window			*wb = wlb->window;
	int				 result = 0;

	switch (window_tree_sort->field) {
	case WINDOW_TREE_BY_INDEX:
		result = wla->idx - wlb->idx;
		break;
	case WINDOW_TREE_BY_TIME:
		if (timercmp(&wa->activity_time, &wb->activity_time, >)) {
			result = -1;
			break;
		}
		if (timercmp(&wa->activity_time, &wb->activity_time, <)) {
			result = 1;
			break;
		}
		/* FALLTHROUGH */
	case WINDOW_TREE_BY_NAME:
		result = strcmp(wa->name, wb->name);
		break;
	}

	if (window_tree_sort->reversed)
		result = -result;
	return (result);
}

static int
window_tree_cmp_pane(const void *a0, const void *b0)
{
	struct window_pane	**a = (struct window_pane **)a0;
	struct window_pane	**b = (struct window_pane **)b0;
	int			  result;
	u_int			  ai, bi;

	if (window_tree_sort->field == WINDOW_TREE_BY_TIME)
		result = (*a)->active_point - (*b)->active_point;
	else {
		/*
		 * Panes don't have names, so use number order for any other
		 * sort field.
		 */
		window_pane_index(*a, &ai);
		window_pane_index(*b, &bi);
		result = ai - bi;
	}
	if (window_tree_sort->reversed)
		result = -result;
	return (result);
}

static void
window_tree_build_pane(struct session *s, struct winlink *wl,
    struct window_pane *wp, void *modedata, struct mode_tree_item *parent)
{
	struct window_tree_modedata	*data = modedata;
	struct window_tree_itemdata	*item;
	struct mode_tree_item		*mti;
	char				*name, *text;
	u_int				 idx;
	struct format_tree		*ft;

	window_pane_index(wp, &idx);

	item = window_tree_add_item(data);
	item->type = WINDOW_TREE_PANE;
	item->session = s->id;
	item->winlink = wl->idx;
	item->pane = wp->id;

	ft = format_create(NULL, NULL, FORMAT_PANE|wp->id, 0);
	format_defaults(ft, NULL, s, wl, wp);
	text = format_expand(ft, data->format);
	xasprintf(&name, "%u", idx);
	format_free(ft);

	mti = mode_tree_add(data->data, parent, item, (uint64_t)wp, name, text,
	    -1);
	free(text);
	free(name);
	mode_tree_align(mti, 1);
}

static int
window_tree_filter_pane(struct session *s, struct winlink *wl,
    struct window_pane *wp, const char *filter)
{
	char	*cp;
	int	 result;

	if (filter == NULL)
		return (1);

	cp = format_single(NULL, filter, NULL, s, wl, wp);
	result = format_true(cp);
	free(cp);

	return (result);
}

static int
window_tree_build_window(struct session *s, struct winlink *wl,
    void *modedata, struct mode_tree_sort_criteria *sort_crit,
    struct mode_tree_item *parent, const char *filter)
{
	struct window_tree_modedata	*data = modedata;
	struct window_tree_itemdata	*item;
	struct mode_tree_item		*mti;
	char				*name, *text;
	struct window_pane		*wp, **l;
	u_int				 n, i;
	int				 expanded;
	struct format_tree		*ft;

	item = window_tree_add_item(data);
	item->type = WINDOW_TREE_WINDOW;
	item->session = s->id;
	item->winlink = wl->idx;
	item->pane = -1;

	ft = format_create(NULL, NULL, FORMAT_PANE|wl->window->active->id, 0);
	format_defaults(ft, NULL, s, wl, NULL);
	text = format_expand(ft, data->format);
	xasprintf(&name, "%u", wl->idx);
	format_free(ft);

	if (data->type == WINDOW_TREE_SESSION ||
	    data->type == WINDOW_TREE_WINDOW)
		expanded = 0;
	else
		expanded = 1;
	mti = mode_tree_add(data->data, parent, item, (uint64_t)wl, name, text,
	    expanded);
	free(text);
	free(name);
	mode_tree_align(mti, 1);

	if ((wp = TAILQ_FIRST(&wl->window->panes)) == NULL)
		goto empty;
	if (TAILQ_NEXT(wp, entry) == NULL) {
		if (!window_tree_filter_pane(s, wl, wp, filter))
			goto empty;
	}

	l = NULL;
	n = 0;

	TAILQ_FOREACH(wp, &wl->window->panes, entry) {
		if (!window_tree_filter_pane(s, wl, wp, filter))
			continue;
		l = xreallocarray(l, n + 1, sizeof *l);
		l[n++] = wp;
	}
	if (n == 0)
		goto empty;

	window_tree_sort = sort_crit;
	qsort(l, n, sizeof *l, window_tree_cmp_pane);

	for (i = 0; i < n; i++)
		window_tree_build_pane(s, wl, l[i], modedata, mti);
	free(l);
	return (1);

empty:
	window_tree_free_item(item);
	data->item_size--;
	mode_tree_remove(data->data, mti);
	return (0);
}

static void
window_tree_build_session(struct session *s, void *modedata,
    struct mode_tree_sort_criteria *sort_crit, const char *filter)
{
	struct window_tree_modedata	*data = modedata;
	struct window_tree_itemdata	*item;
	struct mode_tree_item		*mti;
	char				*text;
	struct winlink			*wl = s->curw, **l;
	u_int				 n, i, empty;
	int				 expanded;
	struct format_tree		*ft;

	item = window_tree_add_item(data);
	item->type = WINDOW_TREE_SESSION;
	item->session = s->id;
	item->winlink = -1;
	item->pane = -1;

	ft = format_create(NULL, NULL, FORMAT_PANE|wl->window->active->id, 0);
	format_defaults(ft, NULL, s, NULL, NULL);
	text = format_expand(ft, data->format);
	format_free(ft);

	if (data->type == WINDOW_TREE_SESSION)
		expanded = 0;
	else
		expanded = 1;
	mti = mode_tree_add(data->data, NULL, item, (uint64_t)s, s->name, text,
	    expanded);
	free(text);

	l = NULL;
	n = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		l = xreallocarray(l, n + 1, sizeof *l);
		l[n++] = wl;
	}
	window_tree_sort = sort_crit;
	qsort(l, n, sizeof *l, window_tree_cmp_window);

	empty = 0;
	for (i = 0; i < n; i++) {
		if (!window_tree_build_window(s, l[i], modedata, sort_crit, mti,
		    filter))
			empty++;
	}
	if (empty == n) {
		window_tree_free_item(item);
		data->item_size--;
		mode_tree_remove(data->data, mti);
	}
	free(l);
}

static void
window_tree_build(void *modedata, struct mode_tree_sort_criteria *sort_crit,
    uint64_t *tag, const char *filter)
{
	struct window_tree_modedata	*data = modedata;
	struct session			*s, **l;
	struct session_group		*sg, *current;
	u_int				 n, i;

	current = session_group_contains(data->fs.s);

	for (i = 0; i < data->item_size; i++)
		window_tree_free_item(data->item_list[i]);
	free(data->item_list);
	data->item_list = NULL;
	data->item_size = 0;

	l = NULL;
	n = 0;
	RB_FOREACH(s, sessions, &sessions) {
		if (data->squash_groups &&
		    (sg = session_group_contains(s)) != NULL) {
			if ((sg == current && s != data->fs.s) ||
			    (sg != current && s != TAILQ_FIRST(&sg->sessions)))
				continue;
		}
		l = xreallocarray(l, n + 1, sizeof *l);
		l[n++] = s;
	}
	window_tree_sort = sort_crit;
	qsort(l, n, sizeof *l, window_tree_cmp_session);

	for (i = 0; i < n; i++)
		window_tree_build_session(l[i], modedata, sort_crit, filter);
	free(l);

	switch (data->type) {
	case WINDOW_TREE_NONE:
		break;
	case WINDOW_TREE_SESSION:
		*tag = (uint64_t)data->fs.s;
		break;
	case WINDOW_TREE_WINDOW:
		*tag = (uint64_t)data->fs.wl;
		break;
	case WINDOW_TREE_PANE:
		if (window_count_panes(data->fs.wl->window) == 1)
			*tag = (uint64_t)data->fs.wl;
		else
			*tag = (uint64_t)data->fs.wp;
		break;
	}
}

static void
window_tree_draw_label(struct screen_write_ctx *ctx, u_int px, u_int py,
    u_int sx, u_int sy, const struct grid_cell *gc, const char *label)
{
	size_t	 len;
	u_int	 ox, oy;

	len = strlen(label);
	if (sx == 0 || sy == 1 || len > sx)
		return;
	ox = (sx - len + 1) / 2;
	oy = (sy + 1) / 2;

	if (ox > 1 && ox + len < sx - 1 && sy >= 3) {
		screen_write_cursormove(ctx, px + ox - 1, py + oy - 1, 0);
		screen_write_box(ctx, len + 2, 3, BOX_LINES_DEFAULT, NULL,
		    NULL);
	}
	screen_write_cursormove(ctx, px + ox, py + oy, 0);
	screen_write_puts(ctx, gc, "%s", label);
}

static void
window_tree_draw_session(struct window_tree_modedata *data, struct session *s,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct options		*oo = s->options;
	struct winlink		*wl;
	struct window		*w;
	u_int			 cx = ctx->s->cx, cy = ctx->s->cy;
	u_int			 loop, total, visible, each, width, offset;
	u_int			 current, start, end, remaining, i;
	struct grid_cell	 gc;
	int			 colour, active_colour, left, right;
	char			*label;

	total = winlink_count(&s->windows);

	memcpy(&gc, &grid_default_cell, sizeof gc);
	colour = options_get_number(oo, "display-panes-colour");
	active_colour = options_get_number(oo, "display-panes-active-colour");

	if (sx / total < 24) {
		visible = sx / 24;
		if (visible == 0)
			visible = 1;
	} else
		visible = total;

	current = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		if (wl == s->curw)
			break;
		current++;
	}

	if (current < visible) {
		start = 0;
		end = visible;
	} else if (current >= total - visible) {
		start = total - visible;
		end = total;
	} else {
		start = current - (visible / 2);
		end = start + visible;
	}

	if (data->offset < -(int)start)
		data->offset = -(int)start;
	if (data->offset > (int)(total - end))
		data->offset = (int)(total - end);
	start += data->offset;
	end += data->offset;

	left = (start != 0);
	right = (end != total);
	if (((left && right) && sx <= 6) || ((left || right) && sx <= 3))
		left = right = 0;
	if (left && right) {
		each = (sx - 6) / visible;
		remaining = (sx - 6) - (visible * each);
	} else if (left || right) {
		each = (sx - 3) / visible;
		remaining = (sx - 3) - (visible * each);
	} else {
		each = sx / visible;
		remaining = sx - (visible * each);
	}
	if (each == 0)
		return;

	if (left) {
		data->left = cx + 2;
		screen_write_cursormove(ctx, cx + 2, cy, 0);
		screen_write_vline(ctx, sy, 0, 0);
		screen_write_cursormove(ctx, cx, cy + sy / 2, 0);
		screen_write_puts(ctx, &grid_default_cell, "<");
	} else
		data->left = -1;
	if (right) {
		data->right = cx + sx - 3;
		screen_write_cursormove(ctx, cx + sx - 3, cy, 0);
		screen_write_vline(ctx, sy, 0, 0);
		screen_write_cursormove(ctx, cx + sx - 1, cy + sy / 2, 0);
		screen_write_puts(ctx, &grid_default_cell, ">");
	} else
		data->right = -1;

	data->start = start;
	data->end = end;
	data->each = each;

	i = loop = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		if (loop == end)
			break;
		if (loop < start) {
			loop++;
			continue;
		}
		w = wl->window;

		if (wl == s->curw)
			gc.fg = active_colour;
		else
			gc.fg = colour;

		if (left)
			offset = 3 + (i * each);
		else
			offset = (i * each);
		if (loop == end - 1)
			width = each + remaining;
		else
			width = each - 1;

		screen_write_cursormove(ctx, cx + offset, cy, 0);
		screen_write_preview(ctx, &w->active->base, width, sy);

		xasprintf(&label, " %u:%s ", wl->idx, w->name);
		if (strlen(label) > width)
			xasprintf(&label, " %u ", wl->idx);
		window_tree_draw_label(ctx, cx + offset, cy, width, sy, &gc,
		    label);
		free(label);

		if (loop != end - 1) {
			screen_write_cursormove(ctx, cx + offset + width, cy, 0);
			screen_write_vline(ctx, sy, 0, 0);
		}
		loop++;

		i++;
	}
}

static void
window_tree_draw_window(struct window_tree_modedata *data, struct session *s,
    struct window *w, struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct options		*oo = s->options;
	struct window_pane	*wp;
	u_int			 cx = ctx->s->cx, cy = ctx->s->cy;
	u_int			 loop, total, visible, each, width, offset;
	u_int			 current, start, end, remaining, i, pane_idx;
	struct grid_cell	 gc;
	int			 colour, active_colour, left, right;
	char			*label;

	total = window_count_panes(w);

	memcpy(&gc, &grid_default_cell, sizeof gc);
	colour = options_get_number(oo, "display-panes-colour");
	active_colour = options_get_number(oo, "display-panes-active-colour");

	if (sx / total < 24) {
		visible = sx / 24;
		if (visible == 0)
			visible = 1;
	} else
		visible = total;

	current = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp == w->active)
			break;
		current++;
	}

	if (current < visible) {
		start = 0;
		end = visible;
	} else if (current >= total - visible) {
		start = total - visible;
		end = total;
	} else {
		start = current - (visible / 2);
		end = start + visible;
	}

	if (data->offset < -(int)start)
		data->offset = -(int)start;
	if (data->offset > (int)(total - end))
		data->offset = (int)(total - end);
	start += data->offset;
	end += data->offset;

	left = (start != 0);
	right = (end != total);
	if (((left && right) && sx <= 6) || ((left || right) && sx <= 3))
		left = right = 0;
	if (left && right) {
		each = (sx - 6) / visible;
		remaining = (sx - 6) - (visible * each);
	} else if (left || right) {
		each = (sx - 3) / visible;
		remaining = (sx - 3) - (visible * each);
	} else {
		each = sx / visible;
		remaining = sx - (visible * each);
	}
	if (each == 0)
		return;

	if (left) {
		data->left = cx + 2;
		screen_write_cursormove(ctx, cx + 2, cy, 0);
		screen_write_vline(ctx, sy, 0, 0);
		screen_write_cursormove(ctx, cx, cy + sy / 2, 0);
		screen_write_puts(ctx, &grid_default_cell, "<");
	} else
		data->left = -1;
	if (right) {
		data->right = cx + sx - 3;
		screen_write_cursormove(ctx, cx + sx - 3, cy, 0);
		screen_write_vline(ctx, sy, 0, 0);
		screen_write_cursormove(ctx, cx + sx - 1, cy + sy / 2, 0);
		screen_write_puts(ctx, &grid_default_cell, ">");
	} else
		data->right = -1;

	data->start = start;
	data->end = end;
	data->each = each;

	i = loop = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (loop == end)
			break;
		if (loop < start) {
			loop++;
			continue;
		}

		if (wp == w->active)
			gc.fg = active_colour;
		else
			gc.fg = colour;

		if (left)
			offset = 3 + (i * each);
		else
			offset = (i * each);
		if (loop == end - 1)
			width = each + remaining;
		else
			width = each - 1;

		screen_write_cursormove(ctx, cx + offset, cy, 0);
		screen_write_preview(ctx, &wp->base, width, sy);

		if (window_pane_index(wp, &pane_idx) != 0)
			pane_idx = loop;
		xasprintf(&label, " %u ", pane_idx);
		window_tree_draw_label(ctx, cx + offset, cy, each, sy, &gc,
		    label);
		free(label);

		if (loop != end - 1) {
			screen_write_cursormove(ctx, cx + offset + width, cy, 0);
			screen_write_vline(ctx, sy, 0, 0);
		}
		loop++;

		i++;
	}
}

static void
window_tree_draw(void *modedata, void *itemdata, struct screen_write_ctx *ctx,
    u_int sx, u_int sy)
{
	struct window_tree_itemdata	*item = itemdata;
	struct session			*sp;
	struct winlink			*wlp;
	struct window_pane		*wp;

	window_tree_pull_item(item, &sp, &wlp, &wp);
	if (wp == NULL)
		return;

	switch (item->type) {
	case WINDOW_TREE_NONE:
		break;
	case WINDOW_TREE_SESSION:
		window_tree_draw_session(modedata, sp, ctx, sx, sy);
		break;
	case WINDOW_TREE_WINDOW:
		window_tree_draw_window(modedata, sp, wlp->window, ctx, sx, sy);
		break;
	case WINDOW_TREE_PANE:
		screen_write_preview(ctx, &wp->base, sx, sy);
		break;
	}
}

static int
window_tree_search(__unused void *modedata, void *itemdata, const char *ss)
{
	struct window_tree_itemdata	*item = itemdata;
	struct session			*s;
	struct winlink			*wl;
	struct window_pane		*wp;
	char				*cmd;
	int				 retval;

	window_tree_pull_item(item, &s, &wl, &wp);

	switch (item->type) {
	case WINDOW_TREE_NONE:
		return (0);
	case WINDOW_TREE_SESSION:
		if (s == NULL)
			return (0);
		return (strstr(s->name, ss) != NULL);
	case WINDOW_TREE_WINDOW:
		if (s == NULL || wl == NULL)
			return (0);
		return (strstr(wl->window->name, ss) != NULL);
	case WINDOW_TREE_PANE:
		if (s == NULL || wl == NULL || wp == NULL)
			break;
		cmd = osdep_get_name(wp->fd, wp->tty);
		if (cmd == NULL || *cmd == '\0')
			return (0);
		retval = (strstr(cmd, ss) != NULL);
		free(cmd);
		return (retval);
	}
	return (0);
}

static void
window_tree_menu(void *modedata, struct client *c, key_code key)
{
	struct window_tree_modedata	*data = modedata;
	struct window_pane		*wp = data->wp;
	struct window_mode_entry	*wme;

	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->data != modedata)
		return;
	window_tree_key(wme, c, NULL, NULL, key, NULL);
}

static key_code
window_tree_get_key(void *modedata, void *itemdata, u_int line)
{
	struct window_tree_modedata	*data = modedata;
	struct window_tree_itemdata	*item = itemdata;
	struct format_tree		*ft;
	struct session			*s;
	struct winlink			*wl;
	struct window_pane		*wp;
	char				*expanded;
	key_code			 key;

	ft = format_create(NULL, NULL, FORMAT_NONE, 0);
	window_tree_pull_item(item, &s, &wl, &wp);
	if (item->type == WINDOW_TREE_SESSION)
		format_defaults(ft, NULL, s, NULL, NULL);
	else if (item->type == WINDOW_TREE_WINDOW)
		format_defaults(ft, NULL, s, wl, NULL);
	else
		format_defaults(ft, NULL, s, wl, wp);
	format_add(ft, "line", "%u", line);

	expanded = format_expand(ft, data->key_format);
	key = key_string_lookup_string(expanded);
	free(expanded);
	format_free(ft);
	return (key);
}

static int
window_tree_swap(void *cur_itemdata, void *other_itemdata)
{
	struct window_tree_itemdata	*cur = cur_itemdata;
	struct window_tree_itemdata	*other = other_itemdata;
	struct session			*cur_session, *other_session;
	struct winlink			*cur_winlink, *other_winlink;
	struct window			*cur_window, *other_window;
	struct window_pane		*cur_pane, *other_pane;

	if (cur->type != other->type)
		return (0);
	if (cur->type != WINDOW_TREE_WINDOW)
		return (0);

	window_tree_pull_item(cur, &cur_session, &cur_winlink, &cur_pane);
	window_tree_pull_item(other, &other_session, &other_winlink,
	    &other_pane);

	if (cur_session != other_session)
		return (0);

	if (window_tree_sort->field != WINDOW_TREE_BY_INDEX &&
	    window_tree_cmp_window(&cur_winlink, &other_winlink) != 0) {
		/*
		 * Swapping indexes would not swap positions in the tree, so
		 * prevent swapping to avoid confusing the user.
		 */
		return (0);
	}

	other_window = other_winlink->window;
	TAILQ_REMOVE(&other_window->winlinks, other_winlink, wentry);
	cur_window = cur_winlink->window;
	TAILQ_REMOVE(&cur_window->winlinks, cur_winlink, wentry);

	other_winlink->window = cur_window;
	TAILQ_INSERT_TAIL(&cur_window->winlinks, other_winlink, wentry);
	cur_winlink->window = other_window;
	TAILQ_INSERT_TAIL(&other_window->winlinks, cur_winlink, wentry);

	if (cur_session->curw == cur_winlink)
		session_set_current(cur_session, other_winlink);
	else if (cur_session->curw == other_winlink)
		session_set_current(cur_session, cur_winlink);
	session_group_synchronize_from(cur_session);
	server_redraw_session_group(cur_session);
	recalculate_sizes();

	return (1);
}

static struct screen *
window_tree_init(struct window_mode_entry *wme, struct cmd_find_state *fs,
    struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_tree_modedata	*data;
	struct screen			*s;

	wme->data = data = xcalloc(1, sizeof *data);
	data->wp = wp;
	data->references = 1;

	if (args_has(args, 's'))
		data->type = WINDOW_TREE_SESSION;
	else if (args_has(args, 'w'))
		data->type = WINDOW_TREE_WINDOW;
	else
		data->type = WINDOW_TREE_PANE;
	memcpy(&data->fs, fs, sizeof data->fs);

	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_TREE_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));
	if (args == NULL || !args_has(args, 'K'))
		data->key_format = xstrdup(WINDOW_TREE_DEFAULT_KEY_FORMAT);
	else
		data->key_format = xstrdup(args_get(args, 'K'));
	if (args == NULL || args_count(args) == 0)
		data->command = xstrdup(WINDOW_TREE_DEFAULT_COMMAND);
	else
		data->command = xstrdup(args_string(args, 0));
	data->squash_groups = !args_has(args, 'G');
	if (args_has(args, 'y'))
		data->prompt_flags = PROMPT_ACCEPT;

	data->data = mode_tree_start(wp, args, window_tree_build,
	    window_tree_draw, window_tree_search, window_tree_menu, NULL,
	    window_tree_get_key, window_tree_swap, data, window_tree_menu_items,
	    window_tree_sort_list, nitems(window_tree_sort_list), &s);
	mode_tree_zoom(data->data, args);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);

	data->type = WINDOW_TREE_NONE;

	return (s);
}

static void
window_tree_destroy(struct window_tree_modedata *data)
{
	u_int	i;

	if (--data->references != 0)
		return;

	for (i = 0; i < data->item_size; i++)
		window_tree_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->format);
	free(data->key_format);
	free(data->command);

	free(data);
}

static void
window_tree_free(struct window_mode_entry *wme)
{
	struct window_tree_modedata *data = wme->data;

	if (data == NULL)
		return;

	data->dead = 1;
	mode_tree_free(data->data);
	window_tree_destroy(data);
}

static void
window_tree_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_tree_modedata	*data = wme->data;

	mode_tree_resize(data->data, sx, sy);
}

static void
window_tree_update(struct window_mode_entry *wme)
{
	struct window_tree_modedata	*data = wme->data;

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;
}

static char *
window_tree_get_target(struct window_tree_itemdata *item,
    struct cmd_find_state *fs)
{
	struct session		*s;
	struct winlink		*wl;
	struct window_pane	*wp;
	char			*target;

	window_tree_pull_item(item, &s, &wl, &wp);

	target = NULL;
	switch (item->type) {
	case WINDOW_TREE_NONE:
		break;
	case WINDOW_TREE_SESSION:
		if (s == NULL)
			break;
		xasprintf(&target, "=%s:", s->name);
		break;
	case WINDOW_TREE_WINDOW:
		if (s == NULL || wl == NULL)
			break;
		xasprintf(&target, "=%s:%u.", s->name, wl->idx);
		break;
	case WINDOW_TREE_PANE:
		if (s == NULL || wl == NULL || wp == NULL)
			break;
		xasprintf(&target, "=%s:%u.%%%u", s->name, wl->idx, wp->id);
		break;
	}
	if (target == NULL)
		cmd_find_clear_state(fs, 0);
	else
		cmd_find_from_winlink_pane(fs, wl, wp, 0);
	return (target);
}

static void
window_tree_command_each(void *modedata, void *itemdata, struct client *c,
    __unused key_code key)
{
	struct window_tree_modedata	*data = modedata;
	struct window_tree_itemdata	*item = itemdata;
	char				*name;
	struct cmd_find_state		 fs;

	name = window_tree_get_target(item, &fs);
	if (name != NULL)
		mode_tree_run_command(c, &fs, data->entered, name);
	free(name);
}

static enum cmd_retval
window_tree_command_done(__unused struct cmdq_item *item, void *modedata)
{
	struct window_tree_modedata	*data = modedata;

	if (!data->dead) {
		mode_tree_build(data->data);
		mode_tree_draw(data->data);
		data->wp->flags |= PANE_REDRAW;
	}
	window_tree_destroy(data);
	return (CMD_RETURN_NORMAL);
}

static int
window_tree_command_callback(struct client *c, void *modedata, const char *s,
    __unused int done)
{
	struct window_tree_modedata	*data = modedata;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);

	data->entered = s;
	mode_tree_each_tagged(data->data, window_tree_command_each, c,
	    KEYC_NONE, 1);
	data->entered = NULL;

	data->references++;
	cmdq_append(c, cmdq_get_callback(window_tree_command_done, data));

	return (0);
}

static void
window_tree_command_free(void *modedata)
{
	struct window_tree_modedata	*data = modedata;

	window_tree_destroy(data);
}

static void
window_tree_kill_each(__unused void *modedata, void *itemdata,
    __unused struct client *c, __unused key_code key)
{
	struct window_tree_itemdata	*item = itemdata;
	struct session			*s;
	struct winlink			*wl;
	struct window_pane		*wp;

	window_tree_pull_item(item, &s, &wl, &wp);

	switch (item->type) {
	case WINDOW_TREE_NONE:
		break;
	case WINDOW_TREE_SESSION:
		if (s != NULL) {
			server_destroy_session(s);
			session_destroy(s, 1, __func__);
		}
		break;
	case WINDOW_TREE_WINDOW:
		if (wl != NULL)
			server_kill_window(wl->window, 0);
		break;
	case WINDOW_TREE_PANE:
		if (wp != NULL)
			server_kill_pane(wp);
		break;
	}
}

static int
window_tree_kill_current_callback(struct client *c, void *modedata,
    const char *s, __unused int done)
{
	struct window_tree_modedata	*data = modedata;
	struct mode_tree_data		*mtd = data->data;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (0);

	window_tree_kill_each(data, mode_tree_get_current(mtd), c, KEYC_NONE);
	server_renumber_all();

	data->references++;
	cmdq_append(c, cmdq_get_callback(window_tree_command_done, data));

	return (0);
}

static int
window_tree_kill_tagged_callback(struct client *c, void *modedata,
    const char *s, __unused int done)
{
	struct window_tree_modedata	*data = modedata;
	struct mode_tree_data		*mtd = data->data;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (0);

	mode_tree_each_tagged(mtd, window_tree_kill_each, c, KEYC_NONE, 1);
	server_renumber_all();

	data->references++;
	cmdq_append(c, cmdq_get_callback(window_tree_command_done, data));

	return (0);
}

static key_code
window_tree_mouse(struct window_tree_modedata *data, key_code key, u_int x,
    struct window_tree_itemdata *item)
{
	struct session		*s;
	struct winlink		*wl;
	struct window_pane	*wp;
	u_int			 loop;

	if (key != KEYC_MOUSEDOWN1_PANE)
		return (KEYC_NONE);

	if (data->left != -1 && x <= (u_int)data->left)
		return ('<');
	if (data->right != -1 && x >= (u_int)data->right)
		return ('>');

	if (data->left != -1)
		x -= data->left;
	else if (x != 0)
		x--;
	if (x == 0 || data->end == 0)
		x = 0;
	else {
		x = x / data->each;
		if (data->start + x >= data->end)
			x = data->end - 1;
	}

	window_tree_pull_item(item, &s, &wl, &wp);
	if (item->type == WINDOW_TREE_SESSION) {
		if (s == NULL)
			return (KEYC_NONE);
		mode_tree_expand_current(data->data);
		loop = 0;
		RB_FOREACH(wl, winlinks, &s->windows) {
			if (loop == data->start + x)
				break;
			loop++;
		}
		if (wl != NULL)
			mode_tree_set_current(data->data, (uint64_t)wl);
		return ('\r');
	}
	if (item->type == WINDOW_TREE_WINDOW) {
		if (wl == NULL)
			return (KEYC_NONE);
		mode_tree_expand_current(data->data);
		loop = 0;
		TAILQ_FOREACH(wp, &wl->window->panes, entry) {
			if (loop == data->start + x)
				break;
			loop++;
		}
		if (wp != NULL)
			mode_tree_set_current(data->data, (uint64_t)wp);
		return ('\r');
	}
	return (KEYC_NONE);
}

static void
window_tree_key(struct window_mode_entry *wme, struct client *c,
    __unused struct session *s, __unused struct winlink *wl, key_code key,
    struct mouse_event *m)
{
	struct window_pane		*wp = wme->wp;
	struct window_tree_modedata	*data = wme->data;
	struct window_tree_itemdata	*item, *new_item;
	char				*name, *prompt = NULL;
	struct cmd_find_state		 fs, *fsp = &data->fs;
	int				 finished;
	u_int				 tagged, x, y, idx;
	struct session			*ns;
	struct winlink			*nwl;
	struct window_pane		*nwp;

	item = mode_tree_get_current(data->data);
	finished = mode_tree_key(data->data, c, &key, m, &x, &y);

again:
	if (item != (new_item = mode_tree_get_current(data->data))) {
		item = new_item;
		data->offset = 0;
	}
	if (KEYC_IS_MOUSE(key) && m != NULL) {
		key = window_tree_mouse(data, key, x, item);
		goto again;
	}

	switch (key) {
	case '<':
		data->offset--;
		break;
	case '>':
		data->offset++;
		break;
	case 'H':
		mode_tree_expand(data->data, (uint64_t)fsp->s);
		mode_tree_expand(data->data, (uint64_t)fsp->wl);
		if (!mode_tree_set_current(data->data, (uint64_t)wme->wp))
			mode_tree_set_current(data->data, (uint64_t)fsp->wl);
		break;
	case 'm':
		window_tree_pull_item(item, &ns, &nwl, &nwp);
		server_set_marked(ns, nwl, nwp);
		mode_tree_build(data->data);
		break;
	case 'M':
		server_clear_marked();
		mode_tree_build(data->data);
		break;
	case 'x':
		window_tree_pull_item(item, &ns, &nwl, &nwp);
		switch (item->type) {
		case WINDOW_TREE_NONE:
			break;
		case WINDOW_TREE_SESSION:
			if (ns == NULL)
				break;
			xasprintf(&prompt, "Kill session %s? ", ns->name);
			break;
		case WINDOW_TREE_WINDOW:
			if (nwl == NULL)
				break;
			xasprintf(&prompt, "Kill window %u? ", nwl->idx);
			break;
		case WINDOW_TREE_PANE:
			if (nwp == NULL || window_pane_index(nwp, &idx) != 0)
				break;
			xasprintf(&prompt, "Kill pane %u? ", idx);
			break;
		}
		if (prompt == NULL)
			break;
		data->references++;
		status_prompt_set(c, NULL, prompt, "",
		    window_tree_kill_current_callback, window_tree_command_free,
		    data, PROMPT_SINGLE|PROMPT_NOFORMAT|data->prompt_flags,
		    PROMPT_TYPE_COMMAND);
		free(prompt);
		break;
	case 'X':
		tagged = mode_tree_count_tagged(data->data);
		if (tagged == 0)
			break;
		xasprintf(&prompt, "Kill %u tagged? ", tagged);
		data->references++;
		status_prompt_set(c, NULL, prompt, "",
		    window_tree_kill_tagged_callback, window_tree_command_free,
		    data, PROMPT_SINGLE|PROMPT_NOFORMAT|data->prompt_flags,
		    PROMPT_TYPE_COMMAND);
		free(prompt);
		break;
	case ':':
		tagged = mode_tree_count_tagged(data->data);
		if (tagged != 0)
			xasprintf(&prompt, "(%u tagged) ", tagged);
		else
			xasprintf(&prompt, "(current) ");
		data->references++;
		status_prompt_set(c, NULL, prompt, "",
		    window_tree_command_callback, window_tree_command_free,
		    data, PROMPT_NOFORMAT, PROMPT_TYPE_COMMAND);
		free(prompt);
		break;
	case '\r':
		name = window_tree_get_target(item, &fs);
		if (name != NULL)
			mode_tree_run_command(c, NULL, data->command, name);
		finished = 1;
		free(name);
		break;
	}
	if (finished)
		window_pane_reset_mode(wp);
	else {
		mode_tree_draw(data->data);
		wp->flags |= PANE_REDRAW;
	}
}
