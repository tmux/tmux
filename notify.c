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
#include <sys/queue.h>

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
notify_hook(struct cmdq_item *item, struct notify_entry *ne)
{
	struct cmd_find_state	 fs;
	struct hook		*hook;
	struct cmdq_item	*new_item;
	struct session		*s = ne->session;
	struct window		*w = ne->window;

	cmd_find_clear_state(&fs, NULL, 0);
	if (cmd_find_empty_state(&ne->fs) || !cmd_find_valid_state(&ne->fs))
		cmd_find_current(&fs, item, CMD_FIND_QUIET);
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

	if (strcmp(ne->name, "window-layout-changed") == 0)
		control_notify_window_layout_changed(ne->window);
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

	notify_hook(item, ne);

	if (ne->client != NULL)
		server_client_unref(ne->client);
	if (ne->session != NULL)
		session_unref(ne->session);
	if (ne->window != NULL)
		window_remove_ref(ne->window);

	if (ne->fs.s != NULL)
		session_unref(ne->fs.s);

	free((void *)ne->name);
	free(ne);

	return (CMD_RETURN_NORMAL);
}

static void
notify_add(const char *name, struct client *c, struct session *s,
    struct window *w, struct window_pane *wp)
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
		s->references++;
	if (w != NULL)
		w->references++;

	cmd_find_clear_state(&ne->fs, NULL, 0);
	if (s != NULL && w != NULL)
		cmd_find_from_session_window(&ne->fs, s, w);
	else if (w != NULL)
		cmd_find_from_window(&ne->fs, w);
	else if (s != NULL && session_alive(s))
		cmd_find_from_session(&ne->fs, s);
	else if (wp != NULL)
		cmd_find_from_pane(&ne->fs, wp);
	else
		cmd_find_current(&ne->fs, NULL, CMD_FIND_QUIET);
	if (ne->fs.s != NULL)
		ne->fs.s->references++; /* cmd_find_valid_state need session */

	new_item = cmdq_get_callback(notify_callback, ne);
	cmdq_append(NULL, new_item);
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
	notify_add(name, c, NULL, NULL, NULL);
}

void
notify_session(const char *name, struct session *s)
{
	notify_add(name, NULL, s, NULL, NULL);
}

void
notify_session_window(const char *name, struct session *s, struct window *w)
{
	notify_add(name, NULL, s, w, NULL);
}

void
notify_window(const char *name, struct window *w)
{
	notify_add(name, NULL, NULL, w, NULL);
}

void
notify_pane(const char *name, struct window_pane *wp)
{
	notify_add(name, NULL, NULL, NULL, wp);
}
