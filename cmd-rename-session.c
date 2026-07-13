/* $OpenBSD: cmd-rename-session.c,v 1.39 2026/07/10 13:38:45 nicm Exp $ */

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
 * Change session name.
 */

static enum cmd_retval	cmd_rename_session_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_rename_session_entry = {
	.name = "rename-session",
	.alias = "rename",

	.args = { "t:", 1, 1, NULL },
	.usage = CMD_TARGET_SESSION_USAGE " new-name",

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_rename_session_exec
};

static enum cmd_retval
cmd_rename_session_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s;
	struct event_payload	*ep;
	struct cmd_find_state	 fs;
	char			*newname, *tmp;

	tmp = format_single_from_target(item, args_string(args, 0));
	if (!check_name(tmp)) {
		cmdq_error(item, "invalid session name: %s", tmp);
		free(tmp);
		return (CMD_RETURN_ERROR);
	}
	newname = clean_name(tmp, 0);
	free(tmp);
	if (strcmp(newname, s->name) == 0) {
		free(newname);
		return (CMD_RETURN_NORMAL);
	}
	if (session_find(newname) != NULL) {
		cmdq_error(item, "duplicate session: %s", newname);
		free(newname);
		return (CMD_RETURN_ERROR);
	}

	ep = event_payload_create();
	cmd_find_from_session(&fs, s, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_session(ep, "session", s);
	event_payload_set_string(ep, "old_name", "%s", s->name);
	event_payload_set_string(ep, "new_name", "%s", newname);

	RB_REMOVE(sessions, &sessions, s);
	free(s->name);
	s->name = newname;
	RB_INSERT(sessions, &sessions, s);

	server_status_session(s);
	events_fire("session-renamed", ep);

	return (CMD_RETURN_NORMAL);
}
