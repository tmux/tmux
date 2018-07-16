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

	struct client		*client;
	struct session		*session;
	struct window		*window;
	int			 pane;

	struct cmd_find_state	 fs;
};

static void
notify_hook1(struct cmdq_item *item, struct notify_entry *ne)
{
	struct cmd_find_state	 fs;
	struct hook		*hook;
	struct cmdq_item	*new_item;
	struct session		*s = ne->session;
	struct window		*w = ne->window;

	cmd_find_clear_state(&fs, 0);
	if (cmd_find_empty_state(&ne->fs) || !cmd_find_valid_state(&ne->fs))
		cmd_find_from_nothing(&fs, 0);
	else
		cmd_find_copy_state(&fs, &ne->fs);

	hook = hooks_find(hooks_get(fs.s), ne->name);
	if (hook == NULL)
		return;
	log_debug("notify hook %s", ne->name);

	new_item = cmdq_get_command(hook->cmdlist, &fs, NULL, CMDQ_NOHOOKS);
	cmdq_format(new_item, "hook", "%s", ne->name);

	if (s != NULL) {
		cmdq_format(new_item, "hook_session", "$%u", s->id);
		cmdq_format(new_item, "hook_session_name", "%s", s->name);
	}
	if (w != NULL) {
		cmdq_format(new_item, "hook_window", "@%u", w->id);
		cmdq_format(new_item, "hook_window_name", "%s", w->name);
	}
	if (ne->pane != -1)
		cmdq_format(new_item, "hook_pane", "%%%d", ne->pane);

	cmdq_insert_after(item, new_item);
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
	if (strcmp(ne->name, "session-renamed") == 0)
		control_notify_session_renamed(ne->session);
	if (strcmp(ne->name, "session-created") == 0)
		control_notify_session_created(ne->session);
	if (strcmp(ne->name, "session-closed") == 0)
		control_notify_session_closed(ne->session);
	if (strcmp(ne->name, "session-window-changed") == 0)
		control_notify_session_window_changed(ne->session);

	notify_hook1(item, ne);

	if (ne->client != NULL)
		server_client_unref(ne->client);
	if (ne->session != NULL)
		session_remove_ref(ne->session, __func__);
	if (ne->window != NULL)
		window_remove_ref(ne->window, __func__);

	if (ne->fs.s != NULL)
		session_remove_ref(ne->fs.s, __func__);

	free((void *)ne->name);
	free(ne);

	return (CMD_RETURN_NORMAL);
}

static void
notify_add(const char *name, struct cmd_find_state *fs, struct client *c,
    struct session *s, struct window *w, struct window_pane *wp)
{
	struct notify_entry	*ne;
	struct cmdq_item	*new_item;

	ne = xcalloc(1, sizeof *ne);
	ne->name = xstrdup(name);

	ne->client = c;
	ne->session = s;
	ne->window = w;

	if (wp != NULL)
		ne->pane = wp->id;
	else
		ne->pane = -1;

	if (c != NULL)
		c->references++;
	if (s != NULL)
		session_add_ref(s, __func__);
	if (w != NULL)
		window_add_ref(w, __func__);

	cmd_find_copy_state(&ne->fs, fs);
	if (ne->fs.s != NULL) /* cmd_find_valid_state needs session */
		session_add_ref(ne->fs.s, __func__);

	new_item = cmdq_get_callback(notify_callback, ne);
	cmdq_append(NULL, new_item);
}

void
notify_hook(struct cmdq_item *item, const char *name)
{
	struct notify_entry	ne;

	memset(&ne, 0, sizeof ne);

	ne.name = name;
	cmd_find_copy_state(&ne.fs, &item->target);

	ne.client = item->client;
	ne.session = item->target.s;
	ne.window = item->target.w;
	ne.pane = item->target.wp->id;

	notify_hook1(item, &ne);
}

void
notify_input(struct window_pane *wp, struct evbuffer *input)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->flags & CLIENT_CONTROL)
			control_notify_input(c, wp, input);
	}
}

void
notify_client(const char *name, struct client *c)
{
	struct cmd_find_state	fs;

	cmd_find_from_client(&fs, c, 0);
	notify_add(name, &fs, c, NULL, NULL, NULL);
}

void
notify_session(const char *name, struct session *s)
{
	struct cmd_find_state	fs;

	if (session_alive(s))
		cmd_find_from_session(&fs, s, 0);
	else
		cmd_find_from_nothing(&fs, 0);
	notify_add(name, &fs, NULL, s, NULL, NULL);
}

void
notify_winlink(const char *name, struct winlink *wl)
{
	struct cmd_find_state	fs;

	cmd_find_from_winlink(&fs, wl, 0);
	notify_add(name, &fs, NULL, wl->session, wl->window, NULL);
}

void
notify_session_window(const char *name, struct session *s, struct window *w)
{
	struct cmd_find_state	fs;

	cmd_find_from_session_window(&fs, s, w, 0);
	notify_add(name, &fs, NULL, s, w, NULL);
}

void
notify_window(const char *name, struct window *w)
{
	struct cmd_find_state	fs;

	cmd_find_from_window(&fs, w, 0);
	notify_add(name, &fs, NULL, NULL, w, NULL);
}

void
notify_pane(const char *name, struct window_pane *wp)
{
	struct cmd_find_state	fs;

	cmd_find_from_pane(&fs, wp, 0);
	notify_add(name, &fs, NULL, NULL, NULL, wp);
}
