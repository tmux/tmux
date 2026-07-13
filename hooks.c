/* $OpenBSD: hooks.c,v 1.13 2026/07/10 15:20:06 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
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

/* Hook monitor state owned by an option entry. */
struct hook_monitor {
	struct options		*oo;

	struct monitor_set	*set;
	struct events_sink	*sink;
	struct cmd_find_state	 fs;

	enum monitor_type	 type;
	int			 id;
	char			*format;
};

/* Hook command data built from an event payload. */
struct hooks_data {
	const char		*name;
	struct cmd_find_state	 fs;
	struct format_tree	*formats;
	struct options		*oo;
	struct client		*client;
	int			 expand;
};

/* Hook event sink registered for a notify event name. */
struct hooks_event {
	char			*name;
	struct events_sink	*sink;
	TAILQ_ENTRY(hooks_event) entry;
};
TAILQ_HEAD(hooks_events, hooks_event);
static struct hooks_events hooks_events = TAILQ_HEAD_INITIALIZER(hooks_events);

/* Insert one hook command list. */
static struct cmdq_item *
hooks_insert_one(struct cmdq_item *item, struct hooks_data *hd,
    struct cmd_list *cmdlist, struct cmdq_state *state)
{
	struct cmdq_item	*new_item;
	char			*s;

	if (cmdlist == NULL)
		return (item);
	if (log_get_level() != 0) {
		s = cmd_list_print(cmdlist, 0);
		log_debug("%s: hook %s is: %s", __func__, hd->name, s);
		free(s);
	}
	new_item = cmdq_get_command(cmdlist, state);
	if (item != NULL)
		return (cmdq_insert_after(item, new_item));
	return (cmdq_append(NULL, new_item));
}

/* Parse a hook command. */
static struct cmd_parse_result *
hooks_parse(struct hooks_data *hd, struct cmd_find_state *fs,
    const char *value)
{
	struct cmd_parse_result	*pr;
	struct format_tree	*ft;
	char			*expanded;

	if (!hd->expand)
		return (cmd_parse_from_string(value, NULL));

	ft = format_create_defaults(NULL, hd->client, fs->s, fs->wl, fs->wp);
	if (hd->formats != NULL)
		format_merge(ft, hd->formats);
	expanded = format_expand(ft, value);
	format_free(ft);

	pr = cmd_parse_from_string(expanded, NULL);
	free(expanded);
	return (pr);
}

/* Insert commands for a hook. */
static void
hooks_insert(struct cmdq_item *item, struct hooks_data *hd)
{
	struct cmd_find_state		 fs;
	struct options			*oo;
	struct cmdq_state		*state;
	struct options_entry		*o;
	struct options_array_item	*a;
	struct cmd_list			*cmdlist;
	const char			*value;
	struct cmd_parse_result		*pr;

	log_debug("%s: inserting hook %s", __func__, hd->name);

	cmd_find_clear_state(&fs, 0);
	if (cmd_find_empty_state(&hd->fs) || !cmd_find_valid_state(&hd->fs))
		cmd_find_from_nothing(&fs, 0);
	else
		cmd_find_copy_state(&fs, &hd->fs);

	if (hd->oo != NULL) {
		oo = hd->oo;
		o = options_get_only(oo, hd->name);
	} else {
		if (fs.s == NULL)
			oo = global_s_options;
		else
			oo = fs.s->options;
		o = options_get(oo, hd->name);
		if (o == NULL && fs.wp != NULL) {
			oo = fs.wp->options;
			o = options_get(oo, hd->name);
		}
		if (o == NULL && fs.wl != NULL) {
			oo = fs.wl->window->options;
			o = options_get(oo, hd->name);
		}
	}
	if (o == NULL) {
		log_debug("%s: hook %s not found", __func__, hd->name);
		return;
	}

	if (item == NULL)
		state = cmdq_new_state(&fs, NULL, CMDQ_STATE_NOHOOKS);
	else {
		state = cmdq_new_state(&fs, cmdq_get_event(item),
		    CMDQ_STATE_NOHOOKS);
	}
	cmdq_add_formats(state, hd->formats);

	if (*hd->name == '@') {
		value = options_get_string(oo, hd->name);
		pr = hooks_parse(hd, &fs, value);
		switch (pr->status) {
		case CMD_PARSE_ERROR:
			log_debug("%s: can't parse hook %s: %s", __func__,
			    hd->name, pr->error);
			free(pr->error);
			break;
		case CMD_PARSE_SUCCESS:
			hooks_insert_one(item, hd, pr->cmdlist, state);
			break;
		}
	} else {
		a = options_array_first(o);
		while (a != NULL) {
			if (hd->expand) {
				value = options_array_item_value(a)->string;
				pr = hooks_parse(hd, &fs, value);
				switch (pr->status) {
				case CMD_PARSE_ERROR:
					if (pr->error != NULL) {
						cmdq_error(item, "%s",
						    pr->error);
					}
					break;
				case CMD_PARSE_SUCCESS:
					item = hooks_insert_one(item, hd,
					    pr->cmdlist, state);
					break;
				}
			} else {
				cmdlist = options_array_item_value(a)->cmdlist;
				item = hooks_insert_one(item, hd, cmdlist,
				    state);
			}
			a = options_array_next(a);
		}
	}

	cmdq_free_state(state);
}

/* Insert commands for a hook event. */
static void
hooks_insert_event(struct cmdq_item *item, const char *name,
    struct event_payload *ep, struct options *oo, int expand)
{
	struct hooks_data	 hd;
	struct format_tree	*ft;
	struct client		*c;

	if (item != NULL && (cmdq_get_flags(item) & CMDQ_STATE_NOHOOKS))
		return;

	c = event_payload_get_client(ep, "client");
	ft = format_create(c, item, FORMAT_NONE, FORMAT_NOJOBS);
	event_payload_add_formats(ep, ft, "hook_");
	format_add(ft, "hook", "%s", name);
	format_log_debug(ft, __func__);

	memset(&hd, 0, sizeof hd);
	hd.name = name;
	cmd_find_clear_state(&hd.fs, 0);
	event_payload_get_target(ep, &hd.fs);
	hd.formats = ft;
	hd.oo = oo;
	hd.client = c;
	hd.expand = expand;

	hooks_insert(item, &hd);
	format_free(ft);
}

/* Handle an event for hooks. */
static void
hooks_event_cb(const char *name, struct event_payload *ep,
    __unused void *sink_data)
{
	struct cmdq_item	*item;

	if (event_payload_get_pointer(ep, "_hook_monitor") != NULL)
		return;

	item = event_payload_get_pointer(ep, "_cmdq_item");
	if (item != NULL) {
		hooks_insert_event(item, name, ep, NULL, 0);
		return;
	}

	item = cmdq_running(NULL);
	if (item == NULL || (~cmdq_get_flags(item) & CMDQ_STATE_NOHOOKS))
		hooks_insert_event(NULL, name, ep, NULL, 0);
}

/* Add a hook event sink. */
void
hooks_add_event(const char *name)
{
	struct hooks_event	*he;

	TAILQ_FOREACH(he, &hooks_events, entry) {
		if (strcmp(he->name, name) == 0)
			return;
	}

	he = xcalloc(1, sizeof *he);
	he->name = xstrdup(name);
	he->sink = events_add_sink(name, hooks_event_cb, NULL);
	TAILQ_INSERT_TAIL(&hooks_events, he, entry);
}

/* Return if an event name can be fired through the hooks path. */
int
hooks_valid_event_name(const char *name)
{
	const struct options_table_entry	*oe;

	if (*name == '@')
		return (1);
	oe = options_search(name);
	return (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_HOOK));
}

/* Add hook event sinks for all built-in hooks. */
void
hooks_build_events(void)
{
	const struct options_table_entry	*oe;

	for (oe = options_table; oe->name != NULL; oe++) {
		if (oe->flags & OPTIONS_TABLE_IS_HOOK)
			hooks_add_event(oe->name);
	}
}

/* Run a hook immediately. */
void
hooks_run(struct cmdq_item *item, const char *name)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct hooks_data	 hd = { 0 };

	hd.name = name;
	cmd_find_copy_state(&hd.fs, target);
	hd.client = cmdq_get_client(item);

	hd.formats = format_create(NULL, NULL, 0, FORMAT_NOJOBS);
	format_add(hd.formats, "hook", "%s", name);
	format_log_debug(hd.formats, __func__);

	hooks_insert(item, &hd);
	format_free(hd.formats);
}

/* Free a hook monitor. */
void
hooks_monitor_free(void *data)
{
	struct hook_monitor	*hm = data;

	events_remove_sink(hm->sink);
	monitor_destroy(hm->set);
	free(hm->format);
	free(hm);
}

/* Remove a hook monitor. */
void
hooks_monitor_remove(struct options *oo, const char *name)
{
	struct options_entry	*o;
	struct hook_monitor	*hm;

	o = options_get_only(oo, name);
	if (o == NULL)
		return;

	hm = options_get_monitor_data(o);
	if (hm != NULL) {
		options_set_monitor_data(o, NULL);
		hooks_monitor_free(hm);
	}
}

/* Handle a hook monitor event. */
static void
hooks_monitor_hook_cb(const char *name, struct event_payload *ep,
    void *sink_data)
{
	struct hook_monitor	*hm = sink_data;

	if (event_payload_get_pointer(ep, "_hook_monitor") == hm)
		hooks_insert_event(cmdq_running(NULL), name, ep, hm->oo, 1);
}

/* Fire a hook monitor event. */
static void
hooks_monitor_cb(struct monitor_change *change, void *data)
{
	struct hook_monitor	*hm = data;
	struct event_payload	*ep;
	struct winlink		*wl = change->wl;
	struct window_pane	*wp = change->wp;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	event_payload_set_pointer(ep, "_hook_monitor", data, NULL, NULL);

	cmd_find_clear_state(&fs, 0);
	if (wl != NULL && wp != NULL && wp->window == wl->window)
		cmd_find_from_winlink_pane(&fs, wl, wp, 0);
	else if (wl != NULL)
		cmd_find_from_winlink(&fs, wl, 0);
	else if (wp != NULL)
		cmd_find_from_pane(&fs, wp, 0);
	else if (change->s != NULL)
		cmd_find_from_session(&fs, change->s, 0);
	else
		cmd_find_copy_state(&fs, &hm->fs);
	event_payload_set_target(ep, &fs);

	if (change->value != NULL)
		event_payload_set_string(ep, "value", "%s", change->value);
	else
		event_payload_set_string(ep, "value", "%s", "");
	if (change->last != NULL)
		event_payload_set_string(ep, "last", "%s", change->last);
	else
		event_payload_set_string(ep, "last", "%s", "");

	if (change->c != NULL)
		event_payload_set_client(ep, "client", change->c);
	if (change->s != NULL)
		event_payload_set_session(ep, "session", change->s);
	if (wl != NULL) {
		if (change->s == NULL)
			event_payload_set_session(ep, "session",
			    wl->session);
		event_payload_set_window(ep, "window", wl->window);
		event_payload_set_int(ep, "window_index", wl->idx);
	}
	if (wp != NULL) {
		event_payload_set_pane(ep, "pane", wp);
		if (wl == NULL)
			event_payload_set_window(ep, "window", wp->window);
	}

	events_fire(change->name, ep);
}

/* Add a hook monitor. */
void
hooks_monitor_add(__unused struct cmdq_item *item, struct options *oo,
    const char *name, enum monitor_type type, int id, const char *format,
    int flags, struct cmd_find_state *fs, struct session *s)
{
	struct options_entry	*o;
	struct hook_monitor	*hm;

	hooks_monitor_remove(oo, name);
	o = options_get_only(oo, name);
	if (o == NULL)
		o = options_set_string(oo, name, 0, "%s", "");

	hm = xcalloc(1, sizeof *hm);
	hm->oo = oo;
	cmd_find_copy_state(&hm->fs, fs);
	hm->type = type;
	hm->id = id;
	hm->format = xstrdup(format);
	hm->set = monitor_create_session(s, hooks_monitor_cb, hm);
	hm->sink = events_add_sink(name, hooks_monitor_hook_cb, hm);
	options_set_monitor_data(o, hm);
	monitor_add(hm->set, name, type, id, format, flags);
}

/* Convert a hook monitor to its value. */
char *
hooks_monitor_to_string(struct options_entry *o)
{
	struct hook_monitor	*hm = options_get_monitor_data(o);
	const char		*name = options_name(o);
	char			*s;

	if (hm == NULL)
		return (NULL);

	switch (hm->type) {
	case MONITOR_SESSION:
		xasprintf(&s, "%s::%s", name, hm->format);
		break;
	case MONITOR_PANE:
		xasprintf(&s, "%s:%%%d:%s", name, hm->id, hm->format);
		break;
	case MONITOR_ALL_PANES:
		xasprintf(&s, "%s:%%*:%s", name, hm->format);
		break;
	case MONITOR_WINDOW:
		xasprintf(&s, "%s:@%d:%s", name, hm->id, hm->format);
		break;
	case MONITOR_ALL_WINDOWS:
		xasprintf(&s, "%s:@*:%s", name, hm->format);
		break;
	}
	return (s);
}
