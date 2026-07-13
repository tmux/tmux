/* $OpenBSD: cmd-server-access.c,v 1.6 2026/06/09 12:58:40 nicm Exp $ */

/*
 * Copyright (c) 2021 Dallas Lyons <dallasdlyons@gmail.com>
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

#include <sys/stat.h>
#include <sys/types.h>

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Controls access to session.
 */

static enum cmd_retval cmd_server_access_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_server_access_entry = {
	.name = "server-access",
	.alias = NULL,

	.args = { "adglrw", 0, 1, NULL },
	.usage = "[-adglrw] " CMD_TARGET_PANE_USAGE " [user|group]",

	.flags = CMD_CLIENT_CANFAIL,
	.exec = cmd_server_access_exec
};

static enum cmd_retval
cmd_server_access_deny(struct cmdq_item *item, id_t id, int flags,
    const char *type, const char *name)
{
	if (!server_acl_find(id, flags)) {
		cmdq_error(item, "%s %s not found", type, name);
		return (CMD_RETURN_ERROR);
	}
	server_acl_deny(id, flags);
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_server_access_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = cmd_get_args(self);
	struct client	*c = cmdq_get_target_client(item);
	char		*arg;
	const char	*name = NULL, *type;
	struct passwd	*pw;
	struct group	*gr;
	id_t		 id;
	int		 flags = 0;

	if (args_has(args, 'l')) {
		server_acl_display(item);
		return (CMD_RETURN_NORMAL);
	}
	if (args_count(args) == 0) {
		cmdq_error(item, "missing user or group argument");
		return (CMD_RETURN_ERROR);
	}

	arg = format_single(item, args_string(args, 0), c, NULL, NULL, NULL);
	if (args_has(args, 'g')) {
		type = "group";
		if ((gr = getgrnam(arg)) != NULL) {
			id = gr->gr_gid;
			name = gr->gr_name;
			flags |= SERVER_ACL_IS_GROUP;
		}
	} else {
		type = "user";
		if ((pw = getpwnam(arg)) != NULL) {
			id = pw->pw_uid;
			name = pw->pw_name;
		}
	}
	if (name == NULL) {
		cmdq_error(item, "unknown %s: %s", type, arg);
		free(arg);
		return (CMD_RETURN_ERROR);
	}
	free(arg);

	if ((~flags & SERVER_ACL_IS_GROUP) && (id == 0 || id == getuid())) {
		cmdq_error(item, "%s owns the server, can't change access",
		    name);
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'a') && args_has(args, 'd')) {
		cmdq_error(item, "-a and -d cannot be used together");
		return (CMD_RETURN_ERROR);
	}
	if (args_has(args, 'w') && args_has(args, 'r')) {
		cmdq_error(item, "-r and -w cannot be used together");
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'd'))
		return (cmd_server_access_deny(item, id, flags, type, name));
	if (args_has(args, 'a')) {
		if (server_acl_find(id, flags)) {
			cmdq_error(item, "%s %s is already added", type, name);
			return (CMD_RETURN_ERROR);
		}
		server_acl_allow(id, flags);
		/* Do not return - allow -r or -w with -a. */
	} else if (args_has(args, 'r') || args_has(args, 'w')) {
		/* -r or -w implies -a if the entry does not exist. */
		if (!server_acl_find(id, flags))
			server_acl_allow(id, flags);
	}

	if (args_has(args, 'w')) {
		if (!server_acl_find(id, flags)) {
			cmdq_error(item, "%s %s not found", type, name);
			return (CMD_RETURN_ERROR);
		}
		server_acl_allow_write(id, flags);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'r')) {
		if (!server_acl_find(id, flags)) {
			cmdq_error(item, "%s %s not found", type, name);
			return (CMD_RETURN_ERROR);
		}
		server_acl_deny_write(id, flags);
		return (CMD_RETURN_NORMAL);
	}

	return (CMD_RETURN_NORMAL);
}
