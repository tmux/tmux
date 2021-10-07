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

	.args = { "lc:EFnpt:rT:Z", 0, 0, NULL },
	.usage = "[-ElnprZ] [-c target-client] [-t target-session] "
		 "[-T key-table]",

	/* -t is special */

	.flags = CMD_READONLY|CMD_CLIENT_CFLAG,
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
	struct client		*tc = cmdq_get_target_client(item);
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	const char		*tablename;
	struct key_table	*table;

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

	if (args_has(args, 'r')) {
		if (tc->flags & CLIENT_READONLY)
			tc->flags &= ~(CLIENT_READONLY|CLIENT_IGNORESIZE);
		else
			tc->flags |= (CLIENT_READONLY|CLIENT_IGNORESIZE);
	}

	tablename = args_get(args, 'T');
	if (tablename != NULL) {
		table = key_bindings_get_table(tablename, 0);
		if (table == NULL) {
			cmdq_error(item, "table %s doesn't exist", tablename);
			return (CMD_RETURN_ERROR);
		}
		table->references++;
		key_bindings_unref_table(tc->keytable);
		tc->keytable = table;
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'n')) {
		if ((s = session_next_session(tc->session)) == NULL) {
			cmdq_error(item, "can't find next session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'p')) {
		if ((s = session_previous_session(tc->session)) == NULL) {
			cmdq_error(item, "can't find previous session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'l')) {
		if (tc->last_session != NULL && session_alive(tc->last_session))
			s = tc->last_session;
		else
			s = NULL;
		if (s == NULL) {
			cmdq_error(item, "can't find last session");
			return (CMD_RETURN_ERROR);
		}
	} else {
		if (cmdq_get_client(item) == NULL)
			return (CMD_RETURN_NORMAL);
		if (wl != NULL && wp != NULL && wp != wl->window->active) {
			w = wl->window;
			if (window_push_zoom(w, 0, args_has(args, 'Z')))
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
		environ_update(s->options, tc->environ, s->environ);

	server_client_set_session(tc, s);
	if (~cmdq_get_flags(item) & CMDQ_STATE_REPEAT)
		server_client_set_key_table(tc, NULL);

	return (CMD_RETURN_NORMAL);
}
