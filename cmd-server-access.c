/* $OpenBSD$ */

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

	.args = { "adlrw", 0, 1, NULL },
	.usage = "[-adlrw] " CMD_TARGET_PANE_USAGE " [user]",

	.flags = CMD_CLIENT_CANFAIL,
	.exec = cmd_server_access_exec
};

static enum cmd_retval
cmd_server_access_deny(struct cmdq_item *item, struct passwd *pw)
{
	struct client		*loop;
	struct server_acl_user	*user;
	uid_t			 uid;

	if ((user = server_acl_user_find(pw->pw_uid)) == NULL) {
		cmdq_error(item, "user %s not found", pw->pw_name);
		return (CMD_RETURN_ERROR);
	}
	TAILQ_FOREACH(loop, &clients, entry) {
		uid = proc_get_peer_uid(loop->peer);
		if (uid == server_acl_get_uid(user)) {
			loop->exit_message = xstrdup("access not allowed");
			loop->flags |= CLIENT_EXIT;
		}
	}
	server_acl_user_deny(pw->pw_uid);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_server_access_exec(struct cmd *self, struct cmdq_item *item)
{

	struct args	*args = cmd_get_args(self);
	struct client	*c = cmdq_get_target_client(item);
	char		*name;
	struct passwd	*pw = NULL;

	if (args_has(args, 'l')) {
		server_acl_display(item);
		return (CMD_RETURN_NORMAL);
	}
	if (args_count(args) == 0) {
		cmdq_error(item, "missing user argument");
		return (CMD_RETURN_ERROR);
	}

	name = format_single(item, args_string(args, 0), c, NULL, NULL, NULL);
	if (*name != '\0')
		pw = getpwnam(name);
	if (pw == NULL) {
		cmdq_error(item, "unknown user: %s", name);
		return (CMD_RETURN_ERROR);
	}
	free(name);

	if (pw->pw_uid == 0 || pw->pw_uid == getuid()) {
		cmdq_error(item, "%s owns the server, can't change access",
		    pw->pw_name);
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
		return (cmd_server_access_deny(item, pw));
	if (args_has(args, 'a')) {
		if (server_acl_user_find(pw->pw_uid) != NULL) {
			cmdq_error(item, "user %s is already added",
			    pw->pw_name);
			return (CMD_RETURN_ERROR);
		}
		server_acl_user_allow(pw->pw_uid);
		/* Do not return - allow -r or -w with -a. */
	} else if (args_has(args, 'r') || args_has(args, 'w')) {
		/* -r or -w implies -a if user does not exist. */
		if (server_acl_user_find(pw->pw_uid) == NULL)
			server_acl_user_allow(pw->pw_uid);
	}

	if (args_has(args, 'w')) {
		if (server_acl_user_find(pw->pw_uid) == NULL) {
			cmdq_error(item, "user %s not found", pw->pw_name);
			return (CMD_RETURN_ERROR);
		}
		server_acl_user_allow_write(pw->pw_uid);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'r')) {
		if (server_acl_user_find(pw->pw_uid) == NULL) {
			cmdq_error(item, "user %s not found", pw->pw_name);
			return (CMD_RETURN_ERROR);
		}
		server_acl_user_deny_write(pw->pw_uid);
		return (CMD_RETURN_NORMAL);
	}

	return (CMD_RETURN_NORMAL);
}
