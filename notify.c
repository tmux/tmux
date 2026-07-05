/* $OpenBSD$ */

/*
 * Copyright (c) 2012 George Nachman <tmux@georgester.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct notify_entry {
	const char		*name;
	struct cmd_find_state	 fs;
	struct format_tree	*formats;
	struct options		*oo;

	struct client		*client;
	struct session		*session;
	struct window		*window;
	int			 pane;
	const char		*pbname;
	int			 expand;
};

struct notify_monitor {
	struct options		*oo;

	struct monitor_set	*set;
	struct cmd_find_state	 fs;

	enum monitor_type	 type;
	int			 id;
	char			*format;
};

static struct cmdq_item *
notify_insert_one_hook(struct cmdq_item *item, struct notify_entry *ne,
    struct cmd_list *cmdlist, struct cmdq_state *state)
{
	struct cmdq_item	*new_item;
	char			*s;

	if (cmdlist == NULL)
		return (item);
	if (log_get_level() != 0) {
		s = cmd_list_print(cmdlist, 0);
		log_debug("%s: hook %s is: %s", __func__, ne->name, s);
		free(s);
	}
	new_item = cmdq_get_command(cmdlist, state);
	if (item != NULL)
		return (cmdq_insert_after(item, new_item));
	return (cmdq_append(NULL, new_item));
}

static struct cmd_parse_result *
notify_parse_hook(struct notify_entry *ne, struct cmd_find_state *fs,
    const char *value)
{
	struct cmd_parse_result	*pr;
	struct format_tree	*ft;
	char			*expanded;

	if (!ne->expand)
		return (cmd_parse_from_string(value, NULL));

	ft = format_create_defaults(NULL, ne->client, fs->s, fs->wl, fs->wp);
	if (ne->formats != NULL)
		format_merge(ft, ne->formats);
	expanded = format_expand(ft, value);
	format_free(ft);

	pr = cmd_parse_from_string(expanded, NULL);
	free(expanded);
	return (pr);
}

static void
notify_insert_hook(struct cmdq_item *item, struct notify_entry *ne)
{
	struct cmd_find_state		 fs;
	struct options			*oo;
	struct cmdq_state		*state;
	struct options_entry		*o;
	struct options_array_item	*a;
	struct cmd_list			*cmdlist;
	const char			*value;
	struct cmd_parse_result		*pr;

	log_debug("%s: inserting hook %s", __func__, ne->name);

	cmd_find_clear_state(&fs, 0);
	if (cmd_find_empty_state(&ne->fs) || !cmd_find_valid_state(&ne->fs))
		cmd_find_from_nothing(&fs, 0);
	else
		cmd_find_copy_state(&fs, &ne->fs);

	if (ne->oo != NULL) {
		oo = ne->oo;
		o = options_get_only(oo, ne->name);
	} else {
		if (fs.s == NULL)
			oo = global_s_options;
		else
			oo = fs.s->options;
		o = options_get(oo, ne->name);
		if (o == NULL && fs.wp != NULL) {
			oo = fs.wp->options;
			o = options_get(oo, ne->name);
		}
		if (o == NULL && fs.wl != NULL) {
			oo = fs.wl->window->options;
			o = options_get(oo, ne->name);
		}
	}
	if (o == NULL) {
		log_debug("%s: hook %s not found", __func__, ne->name);
		return;
	}

	state = cmdq_new_state(&fs, NULL, CMDQ_STATE_NOHOOKS);
	cmdq_add_formats(state, ne->formats);

	if (*ne->name == '@') {
		value = options_get_string(oo, ne->name);
		pr = notify_parse_hook(ne, &fs, value);
		switch (pr->status) {
		case CMD_PARSE_ERROR:
			log_debug("%s: can't parse hook %s: %s", __func__,
			    ne->name, pr->error);
			free(pr->error);
			break;
		case CMD_PARSE_SUCCESS:
			notify_insert_one_hook(item, ne, pr->cmdlist, state);
			break;
		}
	} else {
		a = options_array_first(o);
		while (a != NULL) {
			if (ne->expand) {
				value = options_array_item_value(a)->string;
				pr = notify_parse_hook(ne, &fs, value);
				switch (pr->status) {
				case CMD_PARSE_ERROR:
					if (pr->error != NULL)
						cmdq_error(item, "%s", pr->error);
					break;
				case CMD_PARSE_SUCCESS:
					item = notify_insert_one_hook(item, ne,
					    pr->cmdlist, state);
					break;
				}
			} else {
				cmdlist = options_array_item_value(a)->cmdlist;
				item = notify_insert_one_hook(item, ne, cmdlist,
				    state);
			}
			a = options_array_next(a);
		}
	}

	cmdq_free_state(state);
}

static enum cmd_retval
notify_callback(struct cmdq_item *item, void *data)
{
	struct notify_entry	*ne = data;

	log_debug("%s: %s", __func__, ne->name);

	if (strcmp(ne->name, "pane-mode-changed") == 0)
		control_notify_pane_mode_changed(ne->pane);
	if (strcmp(ne->name, "window-layout-changed") == 0)
		control_notify_window_layout_changed(ne->window);
	if (strcmp(ne->name, "window-pane-changed") == 0)
		control_notify_window_pane_changed(ne->window);
	if (strcmp(ne->name, "window-unlinked") == 0)
		control_notify_window_unlinked(ne->session, ne->window);
	if (strcmp(ne->name, "window-linked") == 0)
		control_notify_window_linked(ne->session, ne->window);
	if (strcmp(ne->name, "window-renamed") == 0)
		control_notify_window_renamed(ne->window);
	if (strcmp(ne->name, "client-session-changed") == 0)
		control_notify_client_session_changed(ne->client);
	if (strcmp(ne->name, "client-detached") == 0)
		control_notify_client_detached(ne->client);
	if (strcmp(ne->name, "session-renamed") == 0)
		control_notify_session_renamed(ne->session);
	if (strcmp(ne->name, "session-created") == 0)
		control_notify_session_created(ne->session);
	if (strcmp(ne->name, "session-closed") == 0)
		control_notify_session_closed(ne->session);
	if (strcmp(ne->name, "session-window-changed") == 0)
		control_notify_session_window_changed(ne->session);
	if (strcmp(ne->name, "paste-buffer-changed") == 0)
		control_notify_paste_buffer_changed(ne->pbname);
	if (strcmp(ne->name, "paste-buffer-deleted") == 0)
		control_notify_paste_buffer_deleted(ne->pbname);

	notify_insert_hook(item, ne);

	if (ne->client != NULL)
		server_client_unref(ne->client);
	if (ne->session != NULL)
		session_remove_ref(ne->session, __func__);
	if (ne->window != NULL)
		window_remove_ref(ne->window, __func__);

	if (ne->fs.s != NULL)
		session_remove_ref(ne->fs.s, __func__);

	format_free(ne->formats);
	free((void *)ne->name);
	free((void *)ne->pbname);
	free(ne);

	return (CMD_RETURN_NORMAL);
}

void
notify_monitor_free(void *data)
{
	struct notify_monitor	*nhm = data;

	monitor_destroy(nhm->set);
	free(nhm->format);
	free(nhm);
}

void
notify_monitor_remove(struct options *oo, const char *name)
{
	struct options_entry	*o;
	struct notify_monitor	*nhm;

	o = options_get_only(oo, name);
	if (o == NULL)
		return;

	nhm = options_get_monitor_data(o);
	if (nhm != NULL) {
		options_set_monitor_data(o, NULL);
		notify_monitor_free(nhm);
	}
}

static void
notify_monitor_cb(struct monitor_change *change, void *data)
{
	struct notify_monitor	*nhm = data;
	struct notify_entry		 ne;
	struct cmdq_item		*item;
	struct window			*w;

	item = cmdq_running(NULL);
	if (item != NULL && (cmdq_get_flags(item) & CMDQ_STATE_NOHOOKS))
		return;

	memset(&ne, 0, sizeof ne);
	ne.name = change->name;
	ne.oo = nhm->oo;
	ne.client = change->c;
	ne.expand = 1;
	if (change->wp != NULL && change->wl != NULL)
		cmd_find_from_winlink_pane(&ne.fs, change->wl, change->wp, 0);
	else if (change->wl != NULL)
		cmd_find_from_winlink(&ne.fs, change->wl, 0);
	else if (change->s != NULL)
		cmd_find_from_session(&ne.fs, change->s, 0);
	else
		cmd_find_copy_state(&ne.fs, &nhm->fs);
	ne.formats = format_create(change->c, item, FORMAT_NONE, FORMAT_NOJOBS);
	format_add(ne.formats, "hook", "%s", change->name);
	format_add(ne.formats, "hook_value", "%s", change->value);
	format_add(ne.formats, "hook_last", "%s",
	    change->last == NULL ? "" : change->last);
	if (change->s != NULL) {
		format_add(ne.formats, "hook_session", "$%u", change->s->id);
		format_add(ne.formats, "hook_session_name", "%s", change->s->name);
	}
	if (change->wl != NULL) {
		w = change->wl->window;
		format_add(ne.formats, "hook_window", "@%u", w->id);
		format_add(ne.formats, "hook_window_name", "%s", w->name);
		format_add(ne.formats, "hook_window_index", "%d", change->wl->idx);
	}
	if (change->wp != NULL) {
		format_add(ne.formats, "hook_pane", "%%%u", change->wp->id);
	}

	notify_insert_hook(item, &ne);
	format_free(ne.formats);
}

void
notify_monitor_add(__unused struct cmdq_item *item, struct options *oo,
    const char *name, enum monitor_type type, int id, const char *format,
    struct cmd_find_state *fs, struct session *s)
{
	struct options_entry	*o;
	struct notify_monitor	*nhm;

	notify_monitor_remove(oo, name);
	o = options_get_only(oo, name);
	if (o == NULL)
		o = options_set_string(oo, name, 0, "%s", "");

	nhm = xcalloc(1, sizeof *nhm);
	nhm->oo = oo;
	cmd_find_copy_state(&nhm->fs, fs);
	nhm->type = type;
	nhm->id = id;
	nhm->format = xstrdup(format);
	nhm->set = monitor_create_session(s, notify_monitor_cb, nhm);
	options_set_monitor_data(o, nhm);
	monitor_add(nhm->set, name, type, id, format, 0);
}

/* Convert a hook monitor to its value. */
char *
notify_monitor_to_string(struct options_entry *o)
{
	struct notify_monitor	*nhm = options_get_monitor_data(o);
	const char		*name = options_name(o);
	char			*s;

	if (nhm == NULL)
		return (NULL);

	switch (nhm->type) {
	case MONITOR_SESSION:
		xasprintf(&s, "%s::%s", name, nhm->format);
		break;
	case MONITOR_PANE:
		xasprintf(&s, "%s:%%%d:%s", name, nhm->id, nhm->format);
		break;
	case MONITOR_ALL_PANES:
		xasprintf(&s, "%s:%%*:%s", name, nhm->format);
		break;
	case MONITOR_WINDOW:
		xasprintf(&s, "%s:@%d:%s", name, nhm->id, nhm->format);
		break;
	case MONITOR_ALL_WINDOWS:
		xasprintf(&s, "%s:@*:%s", name, nhm->format);
		break;
	}
	return (s);
}

static void
notify_add(const char *name, struct cmd_find_state *fs, struct client *c,
    struct session *s, struct window *w, struct window_pane *wp,
    const char *pbname)
{
	struct notify_entry	*ne;
	struct cmdq_item	*item;

	item = cmdq_running(NULL);
	if (item != NULL && (cmdq_get_flags(item) & CMDQ_STATE_NOHOOKS))
		return;

	ne = xcalloc(1, sizeof *ne);
	ne->name = xstrdup(name);

	ne->client = c;
	ne->session = s;
	ne->window = w;
	ne->pane = (wp != NULL ? (int)wp->id : -1);
	ne->pbname = (pbname != NULL ? xstrdup(pbname) : NULL);

	ne->formats = format_create(NULL, NULL, 0, FORMAT_NOJOBS);
	format_add(ne->formats, "hook", "%s", name);
	if (c != NULL)
		format_add(ne->formats, "hook_client", "%s", c->name);
	if (s != NULL) {
		format_add(ne->formats, "hook_session", "$%u", s->id);
		format_add(ne->formats, "hook_session_name", "%s", s->name);
	}
	if (w != NULL) {
		format_add(ne->formats, "hook_window", "@%u", w->id);
		format_add(ne->formats, "hook_window_name", "%s", w->name);
	}
	if (wp != NULL) {
		format_add(ne->formats, "hook_pane", "%%%d", wp->id);
		format_add(ne->formats, "hook_window", "@%u", wp->window->id);
		format_add(ne->formats, "hook_window_name", "%s",
		    wp->window->name);
	}
	format_log_debug(ne->formats, __func__);

	if (c != NULL)
		c->references++;
	if (s != NULL)
		session_add_ref(s, __func__);
	if (w != NULL)
		window_add_ref(w, __func__);

	cmd_find_copy_state(&ne->fs, fs);
	if (ne->fs.s != NULL) /* cmd_find_valid_state needs session */
		session_add_ref(ne->fs.s, __func__);

	cmdq_append(NULL, cmdq_get_callback(notify_callback, ne));
}

void
notify_hook(struct cmdq_item *item, const char *name)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct notify_entry	 ne;

	memset(&ne, 0, sizeof ne);

	ne.name = name;
	cmd_find_copy_state(&ne.fs, target);

	ne.client = cmdq_get_client(item);
	ne.session = target->s;
	ne.window = target->w;
	ne.pane = (target->wp != NULL ? (int)target->wp->id : -1);

	ne.formats = format_create(NULL, NULL, 0, FORMAT_NOJOBS);
	format_add(ne.formats, "hook", "%s", name);
	format_log_debug(ne.formats, __func__);

	notify_insert_hook(item, &ne);
	format_free(ne.formats);
}

void
notify_client(const char *name, struct client *c)
{
	struct cmd_find_state	fs;

	cmd_find_from_client(&fs, c, 0);
	notify_add(name, &fs, c, NULL, NULL, NULL, NULL);
}

void
notify_session(const char *name, struct session *s)
{
	struct cmd_find_state	fs;

	if (session_alive(s))
		cmd_find_from_session(&fs, s, 0);
	else
		cmd_find_from_nothing(&fs, 0);
	notify_add(name, &fs, NULL, s, NULL, NULL, NULL);
}

void
notify_winlink(const char *name, struct winlink *wl)
{
	struct cmd_find_state	fs;

	cmd_find_from_winlink(&fs, wl, 0);
	notify_add(name, &fs, NULL, wl->session, wl->window, NULL, NULL);
}

void
notify_session_window(const char *name, struct session *s, struct window *w)
{
	struct cmd_find_state	fs;

	cmd_find_from_session_window(&fs, s, w, 0);
	notify_add(name, &fs, NULL, s, w, NULL, NULL);
}

void
notify_window(const char *name, struct window *w)
{
	struct cmd_find_state	fs;

	cmd_find_from_window(&fs, w, 0);
	notify_add(name, &fs, NULL, NULL, w, NULL, NULL);
}

void
notify_pane(const char *name, struct window_pane *wp)
{
	struct cmd_find_state	fs;

	cmd_find_from_pane(&fs, wp, 0);
	notify_add(name, &fs, NULL, NULL, NULL, wp, NULL);
}

void
notify_paste_buffer(const char *pbname, int deleted)
{
  	struct cmd_find_state	fs;

	cmd_find_clear_state(&fs, 0);
	if (deleted) {
		notify_add("paste-buffer-deleted", &fs, NULL, NULL, NULL, NULL,
		    pbname);
	} else {
		notify_add("paste-buffer-changed", &fs, NULL, NULL, NULL, NULL,
		    pbname);
	}
}
