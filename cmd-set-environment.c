/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * Set an environment variable.
 */

enum cmd_retval	 cmd_set_environment_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_set_environment_entry = {
	"set-environment", "setenv",
	"grt:u", 1, 2,
	"[-gru] " CMD_TARGET_SESSION_USAGE " name [value]",
	0,
	cmd_set_environment_exec
};

enum cmd_retval
cmd_set_environment_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct session	*s;
	struct environ	*env;
	const char	*name, *value;

	name = args->argv[0];
	if (*name == '\0') {
		cmdq_error(cmdq, "empty variable name");
		return (CMD_RETURN_ERROR);
	}
	if (strchr(name, '=') != NULL) {
		cmdq_error(cmdq, "variable name contains =");
		return (CMD_RETURN_ERROR);
	}

	if (args->argc < 2)
		value = NULL;
	else
		value = args->argv[1];

	if (args_has(self->args, 'g'))
		env = &global_environ;
	else {
		if ((s = cmd_find_session(cmdq, args_get(args, 't'), 0)) == NULL)
			return (CMD_RETURN_ERROR);
		env = &s->environ;
	}

	if (args_has(self->args, 'u')) {
		if (value != NULL) {
			cmdq_error(cmdq, "can't specify a value with -u");
			return (CMD_RETURN_ERROR);
		}
		environ_unset(env, name);
	} else if (args_has(self->args, 'r')) {
		if (value != NULL) {
			cmdq_error(cmdq, "can't specify a value with -r");
			return (CMD_RETURN_ERROR);
		}
		environ_set(env, name, NULL);
	} else {
		if (value == NULL) {
			cmdq_error(cmdq, "no value specified");
			return (CMD_RETURN_ERROR);
		}
		environ_set(env, name, value);
	}

	return (CMD_RETURN_NORMAL);
}
