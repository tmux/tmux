/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Thomas Adam <thomas@xteddy.org>
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
 * Set or show global or session hooks.
 */

enum cmd_retval cmd_set_hook_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_set_hook_entry = {
	.name = "set-hook",
	.alias = NULL,

	.args = { "gt:u", 1, 2 },
	.usage = "[-gu] " CMD_TARGET_SESSION_USAGE " hook-name [command]",

	.tflag = CMD_SESSION,

	.flags = 0,
	.exec = cmd_set_hook_exec
};

const struct cmd_entry cmd_show_hooks_entry = {
	.name = "show-hooks",
	.alias = NULL,

	.args = { "gt:", 0, 1 },
	.usage = "[-g] " CMD_TARGET_SESSION_USAGE,

	.tflag = CMD_SESSION,

	.flags = 0,
	.exec = cmd_set_hook_exec
};

enum cmd_retval
cmd_set_hook_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct cmd_list	*cmdlist;
	struct hooks	*hooks;
	struct hook	*hook;
	char		*cause, *tmp;
	const char	*name, *cmd;

	if (args_has(args, 'g'))
		hooks = global_hooks;
	else
		hooks = cmdq->state.tflag.s->hooks;

	if (self->entry == &cmd_show_hooks_entry) {
		hook = hooks_first(hooks);
		while (hook != NULL) {
			tmp = cmd_list_print(hook->cmdlist);
			cmdq_print(cmdq, "%s -> %s", hook->name, tmp);
			free(tmp);

			hook = hooks_next(hook);
		}
		return (CMD_RETURN_NORMAL);
	}

	name = args->argv[0];
	if (*name == '\0') {
		cmdq_error(cmdq, "invalid hook name");
		return (CMD_RETURN_ERROR);
	}
	if (args->argc < 2)
		cmd = NULL;
	else
		cmd = args->argv[1];

	if (args_has(args, 'u')) {
		if (cmd != NULL) {
			cmdq_error(cmdq, "command passed to unset hook: %s",
			    name);
			return (CMD_RETURN_ERROR);
		}
		hooks_remove(hooks, name);
		return (CMD_RETURN_NORMAL);
	}

	if (cmd == NULL) {
		cmdq_error(cmdq, "no command to set hook: %s", name);
		return (CMD_RETURN_ERROR);
	}
	if (cmd_string_parse(cmd, &cmdlist, NULL, 0, &cause) != 0) {
		if (cause != NULL) {
			cmdq_error(cmdq, "%s", cause);
			free(cause);
		}
		return (CMD_RETURN_ERROR);
	}
	hooks_add(hooks, name, cmdlist);
	cmd_list_free(cmdlist);

	return (CMD_RETURN_NORMAL);
}
