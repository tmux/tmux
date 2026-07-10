/* $OpenBSD: window-client.c,v 1.46 2026/07/04 22:09:06 nicm Exp $ */

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
#include <sys/time.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

static struct screen	*window_client_init(struct window_mode_entry *,
			     struct cmd_find_state *, struct args *);
static void		 window_client_free(struct window_mode_entry *);
static void		 window_client_resize(struct window_mode_entry *, u_int,
			     u_int);
static void		 window_client_update(struct window_mode_entry *);
static void		 window_client_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);

#define WINDOW_CLIENT_DEFAULT_COMMAND "detach-client -t '%%'"

#define WINDOW_CLIENT_DEFAULT_FORMAT \
	"#[fg=themelightgrey]#{t/p:client_activity}: session #[default]#{session_name}"

#define WINDOW_CLIENT_DEFAULT_KEY_FORMAT \
	"#{?#{e|<:#{line},10}," \
		"#{line}" \
	",#{e|<:#{line},36},"	\
		"M-#{a:#{e|+:97,#{e|-:#{line},10}}}" \
	"}"

#define WINDOW_CLIENT_FEATURE(f) \
	"#{?#{I/f:" #f "}," \
	"#[fg=themegreen],#[fg=themelightgrey]}#{p/15:#{l:" #f "}}" \
	"#[default]"
static const char *window_client_info_lines[] = {
	"#[fg=themelightgrey]Client Name   #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{client_name} #[fg=themelightgrey]"
	"#[fg=themelightgrey](PID #{client_pid})#[default]",
	"#[fg=themelightgrey]Session       #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{session_name}",
	"#[fg=themelightgrey]Attach Time   #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{t:client_created} "
	"#[fg=themelightgrey](#{t/r:client_created})#[default]",
	"#[fg=themelightgrey]Activity Time #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{t:client_activity} "
	"#[fg=themelightgrey](#{t/r:client_activity})#[default]",
	"#[fg=themelightgrey]Terminal Type #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{?client_termtype,#{client_termtype},Unknown}",
	"#[fg=themelightgrey]TERM          #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{client_termname}",
	"#[fg=themelightgrey]Size          #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{client_width}x#{client_height} "
	"#[fg=themelightgrey](cell #{client_cell_width}x"
	"#{client_cell_height})#[default]",
	"#[fg=themelightgrey]Bytes Written #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{client_written} "
	"#[fg=themelightgrey](#{client_discarded} discarded)#[default]",

	"#[fg=themelightgrey]Features      #[#{E:tree-mode-border-style},acs]x#[default] "
	WINDOW_CLIENT_FEATURE(256) " "
	WINDOW_CLIENT_FEATURE(RGB) " "
	WINDOW_CLIENT_FEATURE(bpaste) " "
	WINDOW_CLIENT_FEATURE(ccolour),
	"              #[#{E:tree-mode-border-style},acs]x#[default] "
	WINDOW_CLIENT_FEATURE(clipboard) " "
	WINDOW_CLIENT_FEATURE(cstyle) " "
	WINDOW_CLIENT_FEATURE(extkeys) " "
	WINDOW_CLIENT_FEATURE(focus),
	"              #[#{E:tree-mode-border-style},acs]x#[default] "
	WINDOW_CLIENT_FEATURE(hyperlinks) " "
	WINDOW_CLIENT_FEATURE(ignorefkeys) " "
	WINDOW_CLIENT_FEATURE(margins) " "
	WINDOW_CLIENT_FEATURE(mouse),
	"              #[#{E:tree-mode-border-style},acs]x#[default] "
	WINDOW_CLIENT_FEATURE(osc7) " "
	WINDOW_CLIENT_FEATURE(overline) " "
	WINDOW_CLIENT_FEATURE(progressbar) " "
	WINDOW_CLIENT_FEATURE(rectfill),
	"              #[#{E:tree-mode-border-style},acs]x#[default] "
	WINDOW_CLIENT_FEATURE(sixel) " "
	WINDOW_CLIENT_FEATURE(strikethrough) " "
	WINDOW_CLIENT_FEATURE(sync) " "
	WINDOW_CLIENT_FEATURE(title),
	"              #[#{E:tree-mode-border-style},acs]x#[default] "
	WINDOW_CLIENT_FEATURE(usstyle),
	"#[#{E:tree-mode-border-style},acs]qqqqqqqqqqqqqqn#{R:q,#{window_width}}#[default]",

	"#[fg=themelightgrey]prefix        #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{prefix}",

	"#[fg=themelightgrey]mouse         #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{?mouse,#{?#{I/c:kmous},,#[fg=themered]}on,#[fg=themelightgrey]off} "
	"#{?#{I/c:kmous},,#[align=right]unavailable: [kmous] missing}",

	"#[fg=themelightgrey]set-clipboard #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{?#{!=:#{set-clipboard},off},#{?#{I/f:clipboard},,"
	"#[fg=themered]}#{set-clipboard},#[fg=themelightgrey]off} "
	"#{?#{I/f:clipboard},,#[align=right]unavailable: [Ms] missing}",

	"#[fg=themelightgrey]get-clipboard #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{?#{!=:#{get-clipboard},off},#{?#{I/f:clipboard},,"
	"#[fg=themered]}#{get-clipboard},#[fg=themelightgrey]off} "
	"#{?#{I/f:clipboard},,#[align=right]unavailable: [Ms] missing}",

	"#[fg=themelightgrey]focus-events  #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{?focus-events,#{?#{I/f:focus},,#[fg=themered]}on,#[fg=themelightgrey]off} "
	"#{?#{I/f:focus},,#[align=right]unavailable: [Enfcs] or [Dcfcs] missing}",

	"#[fg=themelightgrey]extended-keys #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{?#{!=:#{extended-keys},off},#{?#{I/f:extkeys},,"
	"#[fg=themered]}#{extended-keys},#[fg=themelightgrey]off} "
	"#{?#{I/f:extkeys},,#[align=right]unavailable: [Eneks] or [Dseks] missing}",

	"#[fg=themelightgrey]set-titles    #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{?set-titles,on,#[fg=themelightgrey]off}",

	"#[fg=themelightgrey]escape-time   #[#{E:tree-mode-border-style},acs]x#[default] "
	"#{escape-time} ms",
};


static const struct menu_item window_client_menu_items[] = {
	{ "Detach", 'd', NULL },
	{ "Detach Tagged", 'D', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Tag", 't', NULL },
	{ "Tag All", '\024', NULL },
	{ "Tag None", 'T', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Cancel", 'q', NULL },

	{ NULL, KEYC_NONE, NULL }
};

const struct window_mode window_client_mode = {
	.name = "client-mode",
	.default_format = WINDOW_CLIENT_DEFAULT_FORMAT,

	.init = window_client_init,
	.free = window_client_free,
	.resize = window_client_resize,
	.update = window_client_update,
	.key = window_client_key,
};

struct window_client_itemdata {
	struct client	*c;
	char		*ttyname;
};

struct window_client_modedata {
	struct window_pane		 *wp;

	struct mode_tree_data		 *data;
	char				 *format;
	char				 *key_format;
	char				 *command;

	int				  hide_preview_this_pane;
	int				  preview_is_info;

	struct window_client_itemdata	**item_list;
	u_int				  item_size;
};

static enum sort_order window_client_order_seq[] = {
	SORT_NAME,
	SORT_SIZE,
	SORT_CREATION,
	SORT_ACTIVITY,
	SORT_END,
};

static struct window_client_itemdata *
window_client_add_item(struct window_client_modedata *data)
{
	struct window_client_itemdata	*item;

	data->item_list = xreallocarray(data->item_list, data->item_size + 1,
	    sizeof *data->item_list);
	item = data->item_list[data->item_size++] = xcalloc(1, sizeof *item);
	return (item);
}

static void
window_client_free_item(struct window_client_itemdata *item)
{
	server_client_unref(item->c);
	free(item->ttyname);
	free(item);
}

static void
window_client_build(void *modedata, struct sort_criteria *sort_crit,
    __unused uint64_t *tag, const char *filter)
{
	struct window_client_modedata	*data = modedata;
	struct window_client_itemdata	*item;
	u_int				 i, n;
	struct client			*c, **l;
	char				*text, *cp;

	for (i = 0; i < data->item_size; i++)
		window_client_free_item(data->item_list[i]);
	free(data->item_list);
	data->item_list = NULL;
	data->item_size = 0;

	l = sort_get_clients(&n, sort_crit);
	for (i = 0; i < n; i++) {
		if (l[i]->session == NULL ||
		    (l[i]->flags & CLIENT_UNATTACHEDFLAGS))
			continue;

		item = window_client_add_item(data);
		item->c = l[i];
		item->ttyname = xstrdup(l[i]->ttyname);

		l[i]->references++;
	}

	for (i = 0; i < data->item_size; i++) {
		item = data->item_list[i];
		c = item->c;

		if (filter != NULL) {
			cp = format_single(NULL, filter, c, NULL, NULL, NULL);
			if (!format_true(cp)) {
				free(cp);
				continue;
			}
			free(cp);
		}

		text = format_single(NULL, data->format, c, NULL, NULL, NULL);
		mode_tree_add(data->data, NULL, item, (uint64_t)c, c->name,
		    text, -1);
		free(text);
	}
}

static void
window_client_draw_info(__unused void *modedata, void *itemdata,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct window_client_itemdata	*item = itemdata;
	struct client			*c = item->c;
	struct screen			*s = ctx->s;
	struct window			*w = c->session->curw->window;
	struct grid_cell		 gc;
	u_int				 cx = s->cx, cy = s->cy, i;
	struct format_tree		*ft;
	char				*expanded;

	ft = format_create_defaults(NULL, c, NULL, NULL, NULL);

	screen_write_cursormove(ctx, cx, cy, 0);
	for (i = 0; i < nitems(window_client_info_lines); i++) {
		if (i == sy)
			break;
		expanded = format_expand(ft, window_client_info_lines[i]);
		screen_write_cursormove(ctx, cx, cy + i, 0);
		format_draw(ctx, &grid_default_cell, sx, expanded, NULL, 0);
		free(expanded);
	}
	if (sx > 14 && i < sy) {
		memcpy(&gc, &grid_default_cell, sizeof gc);
		style_apply(&gc, w->options, "tree-mode-border-style", NULL);
		screen_write_cursormove(ctx, cx + 14, cy + i, 0);
		screen_write_vline(ctx, sy - i, 0, 0, &gc);
	}

	format_free(ft);
}

static void
window_client_draw(void *modedata, void *itemdata,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct window_client_modedata	*data = modedata;
	struct window_client_itemdata	*item = itemdata;
	struct client			*c = item->c;
	struct session			*session = c->session;
	struct screen			*s = ctx->s;
	struct window			*w;
	struct window_pane		*wp;
	struct grid_cell		 gc;
	u_int				 cx = s->cx, cy = s->cy, lines, at;

	if (session == NULL || (c->flags & CLIENT_UNATTACHEDFLAGS))
		return;
	if (data->preview_is_info) {
		window_client_draw_info(modedata, itemdata, ctx, sx, sy);
		return;
	}
	w = session->curw->window;
	wp = w->active;
	if (data->hide_preview_this_pane && wp == data->wp) {
		if (!TAILQ_EMPTY(&w->last_panes))
			wp = TAILQ_FIRST(&w->last_panes);
		else
			wp = NULL;
	}

	lines = status_line_size(c);
	if (lines >= sy)
		lines = 0;
	if (status_at_line(c) == 0)
		at = lines;
	else
		at = 0;

	screen_write_cursormove(ctx, cx, cy + at, 0);
	if (wp != NULL)
		screen_write_preview(ctx, &wp->base, sx, sy - 2 - lines);

	if (at != 0)
		screen_write_cursormove(ctx, cx, cy + 2, 0);
	else
		screen_write_cursormove(ctx, cx, cy + sy - 1 - lines, 0);
	memcpy(&gc, &grid_default_cell, sizeof gc);
	style_apply(&gc, w->options, "tree-mode-border-style", NULL);
	screen_write_hline(ctx, sx, 0, 0, BOX_LINES_DEFAULT, &gc);

	if (at != 0)
		screen_write_cursormove(ctx, cx, cy, 0);
	else
		screen_write_cursormove(ctx, cx, cy + sy - lines, 0);
	screen_write_fast_copy(ctx, &c->status.screen, 0, 0, sx, lines);
}

static void
window_client_menu(void *modedata, struct client *c, key_code key)
{
	struct window_client_modedata	*data = modedata;
	struct window_pane		*wp = data->wp;
	struct window_mode_entry	*wme;

	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->data != modedata)
		return;
	window_client_key(wme, c, NULL, NULL, key, NULL);
}

static key_code
window_client_get_key(void *modedata, void *itemdata, u_int line)
{
	struct window_client_modedata	*data = modedata;
	struct window_client_itemdata	*item = itemdata;
	struct format_tree		*ft;
	char				*expanded;
	key_code			 key;

	ft = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_defaults(ft, item->c, NULL, 0, NULL);
	format_add(ft, "line", "%u", line);

	expanded = format_expand(ft, data->key_format);
	key = key_string_lookup_string(expanded);
	free(expanded);
	format_free(ft);
	return (key);
}

static void
window_client_sort(struct sort_criteria *sort_crit)
{
	sort_crit->order_seq = window_client_order_seq;
	if (sort_crit->order == SORT_END)
		sort_crit->order = sort_crit->order_seq[0];
}

static const char* window_client_help_lines[] = {
	"#[fg=themelightgrey]"
	"          i #[#{E:tree-mode-border-style},acs]x#[default] Toggle info view",
	"#[fg=themelightgrey]"
	"      Enter #[#{E:tree-mode-border-style},acs]x#[default] Choose selected %1",
	"#[fg=themelightgrey]"
	"          d #[#{E:tree-mode-border-style},acs]x#[default] Detach selected %1",
	"#[fg=themelightgrey]"
	"          D #[#{E:tree-mode-border-style},acs]x#[default] Detach tagged %1s",
	"#[fg=themelightgrey]"
	"          x #[#{E:tree-mode-border-style},acs]x#[default] Detach selected %1",
	"#[fg=themelightgrey]"
	"          X #[#{E:tree-mode-border-style},acs]x#[default] Detach tagged %1s",
	"#[fg=themelightgrey]"
	"          z #[#{E:tree-mode-border-style},acs]x#[default] Suspend selected %1",
	"#[fg=themelightgrey]"
	"          Z #[#{E:tree-mode-border-style},acs]x#[default] Suspend tagged %1s",
	"#[fg=themelightgrey]"
	"          f #[#{E:tree-mode-border-style},acs]x#[default] Enter a filter",
	NULL
};

static const char**
window_client_help(u_int *width, const char **item)
{
	*width = 0;
	*item = "client";
	return (window_client_help_lines);
}

static struct screen *
window_client_init(struct window_mode_entry *wme,
    __unused struct cmd_find_state *fs, struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_client_modedata	*data;
	struct screen			*s;

	wme->data = data = xcalloc(1, sizeof *data);
	data->wp = wp;
	data->hide_preview_this_pane = args != NULL && args_has(args, 'h');
	data->preview_is_info = args != NULL && args_has(args, 'i');

	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_CLIENT_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));
	if (args == NULL || !args_has(args, 'K'))
		data->key_format = xstrdup(WINDOW_CLIENT_DEFAULT_KEY_FORMAT);
	else
		data->key_format = xstrdup(args_get(args, 'K'));
	if (args == NULL || args_count(args) == 0)
		data->command = xstrdup(WINDOW_CLIENT_DEFAULT_COMMAND);
	else
		data->command = xstrdup(args_string(args, 0));

	data->data = mode_tree_start(wp, args, window_client_build,
	    window_client_draw, NULL, window_client_menu, NULL,
	    window_client_get_key, NULL, window_client_sort,
	    window_client_help, data, window_client_menu_items, &s);
	mode_tree_zoom(data->data, args);

	if (data->preview_is_info)
		mode_tree_view_name(data->data, "info");
	else
		mode_tree_view_name(data->data, "preview");

	mode_tree_build(data->data);
	mode_tree_draw(data->data);

	return (s);
}

static void
window_client_free(struct window_mode_entry *wme)
{
	struct window_client_modedata	*data = wme->data;
	u_int				 i;

	if (data == NULL)
		return;

	mode_tree_free(data->data);

	for (i = 0; i < data->item_size; i++)
		window_client_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->format);
	free(data->key_format);
	free(data->command);

	free(data);
}

static void
window_client_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_client_modedata	*data = wme->data;

	mode_tree_resize(data->data, sx, sy);
}

static void
window_client_update(struct window_mode_entry *wme)
{
	struct window_client_modedata	*data = wme->data;

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;
}

static void
window_client_do_detach(void *modedata, void *itemdata,
    __unused struct client *c, key_code key)
{
	struct window_client_modedata	*data = modedata;
	struct window_client_itemdata	*item = itemdata;

	if (item == mode_tree_get_current(data->data))
		mode_tree_down(data->data, 0);
	if (key == 'd' || key == 'D')
		server_client_detach(item->c, MSG_DETACH);
	else if (key == 'x' || key == 'X')
		server_client_detach(item->c, MSG_DETACHKILL);
	else if (key == 'z' || key == 'Z')
		server_client_suspend(item->c);
}

static void
window_client_key(struct window_mode_entry *wme, struct client *c,
    __unused struct session *s, __unused struct winlink *wl, key_code key,
    struct mouse_event *m)
{
	struct window_pane		*wp = wme->wp;
	struct window_client_modedata	*data = wme->data;
	struct mode_tree_data		*mtd = data->data;
	struct window_client_itemdata	*item;
	int				 finished;

	finished = mode_tree_key(mtd, c, &key, m, NULL, NULL);
	switch (key) {
	case 'd':
	case 'x':
	case 'z':
		item = mode_tree_get_current(mtd);
		window_client_do_detach(data, item, c, key);
		mode_tree_build(mtd);
		break;
	case 'D':
	case 'X':
	case 'Z':
		mode_tree_each_tagged(mtd, window_client_do_detach, c, key, 0);
		mode_tree_build(mtd);
		break;
	case 'i':
		data->preview_is_info = !data->preview_is_info;
		if (data->preview_is_info)
			mode_tree_view_name(mtd, "info");
		else
			mode_tree_view_name(mtd, "preview");
		mode_tree_build(mtd);
		break;
	case '\r':
		item = mode_tree_get_current(mtd);
		mode_tree_run_command(c, NULL, data->command, item->ttyname);
		finished = 1;
		break;
	}
	if (finished || server_client_how_many() == 0)
		window_pane_reset_mode(wp);
	else {
		mode_tree_draw(mtd);
		wp->flags |= PANE_REDRAW;
	}
}
