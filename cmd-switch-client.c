/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

/*
 * Switch client to a different session.
 */

static enum cmd_retval	cmd_switch_client_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_switch_client_entry = {
	.name = "switch-client",
	.alias = "switchc",

	.args = { "lc:Enpt:rT:Z", 0, 0 },
	.usage = "[-ElnprZ] [-c target-client] [-t target-session] "
		 "[-T key-table]",

	/* -t is special */

	.flags = CMD_READONLY,
	.exec = cmd_switch_client_exec
};

static enum cmd_retval
cmd_switch_client_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	 target;
	const char		*tflag = args_get(args, 't');
	enum cmd_find_type	 type;
	int			 flags;
	struct client		*c;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	const char		*tablename;
	struct key_table	*table;

	if ((c = cmd_find_client(item, args_get(args, 'c'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	if (tflag != NULL && tflag[strcspn(tflag, ":.%")] != '\0') {
		type = CMD_FIND_PANE;
		flags = 0;
	} else {
		type = CMD_FIND_SESSION;
		flags = CMD_FIND_PREFER_UNATTACHED;
	}
	if (cmd_find_target(&target, item, tflag, type, flags) != 0)
		return (CMD_RETURN_ERROR);
	s = target.s;
	wl = target.wl;
	wp = target.wp;

	if (args_has(args, 'r'))
		c->flags ^= CLIENT_READONLY;

	tablename = args_get(args, 'T');
	if (tablename != NULL) {
		table = key_bindings_get_table(tablename, 0);
		if (table == NULL) {
			cmdq_error(item, "table %s doesn't exist", tablename);
			return (CMD_RETURN_ERROR);
		}
		table->references++;
		key_bindings_unref_table(c->keytable);
		c->keytable = table;
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'n')) {
		if ((s = session_next_session(c->session)) == NULL) {
			cmdq_error(item, "can't find next session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'p')) {
		if ((s = session_previous_session(c->session)) == NULL) {
			cmdq_error(item, "can't find previous session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'l')) {
		if (c->last_session != NULL && session_alive(c->last_session))
			s = c->last_session;
		else
			s = NULL;
		if (s == NULL) {
			cmdq_error(item, "can't find last session");
			return (CMD_RETURN_ERROR);
		}
	} else {
		if (cmdq_get_client(item) == NULL)
			return (CMD_RETURN_NORMAL);
		if (wl != NULL && wp != NULL) {
			w = wl->window;
			if (window_push_zoom(w, args_has(args, 'Z')))
				server_redraw_window(w);
			window_redraw_active_switch(w, wp);
			window_set_active_pane(w, wp, 1);
			if (window_pop_zoom(w))
				server_redraw_window(w);
		}
		if (wl != NULL) {
			session_set_current(s, wl);
			cmd_find_from_session(current, s, 0);
		}
	}

	if (!args_has(args, 'E'))
		environ_update(s->options, c->environ, s->environ);

	if (c->session != NULL && c->session != s)
		c->last_session = c->session;
	c->session = s;
	if (~cmdq_get_flags(item) & CMDQ_STATE_REPEAT)
		server_client_set_key_table(c, NULL);
	tty_update_client_offset(c);
	status_timer_start(c);
	notify_client("client-session-changed", c);
	session_update_activity(s, NULL);
	gettimeofday(&s->last_attached_time, NULL);

	server_check_unattached();
	server_redraw_client(c);
	s->curw->flags &= ~WINLINK_ALERTFLAGS;
	s->curw->window->latest = c;
	recalculate_sizes();
	alerts_check_session(s);

	return (CMD_RETURN_NORMAL);
}
