/* $Id$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include "tmux.h"

/*
 * Attach existing session to the current terminal.
 */

enum cmd_retval	cmd_attach_session_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_attach_session_entry = {
	"attach-session", "attach",
	"drt:", 0, 0,
	"[-dr] " CMD_TARGET_SESSION_USAGE,
	CMD_CANTNEST|CMD_STARTSERVER|CMD_SENDENVIRON,
	NULL,
	NULL,
	cmd_attach_session_exec
};

enum cmd_retval
cmd_attach_session(struct cmd_q *cmdq, const char* tflag, int dflag, int rflag)
{
	struct session	*s;
	struct client	*c;
	const char	*update;
	char		*cause;
	u_int		 i;

	if (RB_EMPTY(&sessions)) {
		cmdq_error(cmdq, "no sessions");
		return (CMD_RETURN_ERROR);
	}

	if ((s = cmd_find_session(cmdq, tflag, 1)) == NULL)
		return (CMD_RETURN_ERROR);

	if (cmdq->client == NULL)
		return (CMD_RETURN_NORMAL);

	if (cmdq->client->session != NULL) {
		if (dflag) {
			/*
			 * Can't use server_write_session in case attaching to
			 * the same session as currently attached to.
			 */
			for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
				c = ARRAY_ITEM(&clients, i);
				if (c == NULL || c->session != s)
					continue;
				if (c == cmdq->client)
					continue;
				server_write_client(c, MSG_DETACH, NULL, 0);
			}
		}

		cmdq->client->session = s;
		notify_attached_session_changed(cmdq->client);
		session_update_activity(s);
		server_redraw_client(cmdq->client);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;
	} else {
		if (server_client_open(cmdq->client, s, &cause) != 0) {
			cmdq_error(cmdq, "open terminal failed: %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		if (rflag)
			cmdq->client->flags |= CLIENT_READONLY;

		if (dflag)
			server_write_session(s, MSG_DETACH, NULL, 0);

		update = options_get_string(&s->options, "update-environment");
		environ_update(update, &cmdq->client->environ, &s->environ);

		cmdq->client->session = s;
		notify_attached_session_changed(cmdq->client);
		session_update_activity(s);
		server_redraw_client(cmdq->client);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;

		server_write_ready(cmdq->client);
		cmdq->client_exit = 0;
	}
	recalculate_sizes();
	server_update_socket();

	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_attach_session_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;

	return (cmd_attach_session(cmdq, args_get(args, 't'),
	    args_has(args, 'd'), args_has(args, 'r')));
}
