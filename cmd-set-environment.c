/* $Id$ */

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

int	cmd_set_environment_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_set_environment_entry = {
	"set-environment", "setenv",
	"grt:u", 1, 2,
	"[-gru] " CMD_TARGET_SESSION_USAGE " name [value]",
	0,
	NULL,
	NULL,
	cmd_set_environment_exec
};

int
cmd_set_environment_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;
	struct environ	*env;
	const char	*name, *value;

	name = args->argv[0];
	if (*name == '\0') {
		ctx->error(ctx, "empty variable name");
		return (-1);
	}
	if (strchr(name, '=') != NULL) {
		ctx->error(ctx, "variable name contains =");
		return (-1);
	}

	if (args->argc < 1)
		value = NULL;
	else
		value = args->argv[1];

	if (args_has(self->args, 'g'))
		env = &global_environ;
	else {
		if ((s = cmd_find_session(ctx, args_get(args, 't'), 0)) == NULL)
			return (-1);
		env = &s->environ;
	}

	if (args_has(self->args, 'u')) {
		if (value != NULL) {
			ctx->error(ctx, "can't specify a value with -u");
			return (-1);
		}
		environ_unset(env, name);
	} else if (args_has(self->args, 'r')) {
		if (value != NULL) {
			ctx->error(ctx, "can't specify a value with -r");
			return (-1);
		}
		environ_set(env, name, NULL);
	} else {
		if (value == NULL) {
			ctx->error(ctx, "no value specified");
			return (-1);
		}
		environ_set(env, name, value);
	}

	return (0);
}
