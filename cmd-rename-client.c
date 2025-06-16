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
 * Change client name.
 */

static enum cmd_retval	cmd_rename_client_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_rename_client_entry = {
	.name = "rename-client",
	.alias = "renamec",

	.args = { "c:", 1, 1, NULL },
	.usage = CMD_TARGET_CLIENT_USAGE " new-name",

	.flags = CMD_CLIENT_CFLAG,
	.exec = cmd_rename_client_exec
};

static enum cmd_retval
cmd_rename_client_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = cmd_get_args(self);
	struct client	*tc = cmdq_get_target_client(item), *c;
	char		*newname, *tmp;

	tmp = format_single_from_target(item, args_string(args, 0));
	newname = session_check_name(tmp);
	if (newname == NULL) {
		cmdq_error(item, "invalid client name: %s", tmp);
		free(tmp);
		return (CMD_RETURN_ERROR);
	}
	free(tmp);
	if (strcmp(newname, tc->name) == 0) {
		free(newname);
		return (CMD_RETURN_NORMAL);
	}
	TAILQ_FOREACH(c, &clients, entry) {
		if (strcmp(c->name, newname) == 0) {
			cmdq_error(item, "duplicate client name: %s", newname);
			free(newname);
			return (CMD_RETURN_ERROR);
		}
	}

	TAILQ_REMOVE(&clients, tc, entry);
	free(tc->name);
	tc->name = newname;
	TAILQ_INSERT_TAIL(&clients, tc, entry);

	notify_client("client-renamed", c);

	return (CMD_RETURN_NORMAL);
}
