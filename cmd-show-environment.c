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
 * Show environment.
 */

int	cmd_show_environment_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_show_environment_entry = {
	"show-environment", "showenv",
	"gt:", 0, 1,
	"[-g] " CMD_TARGET_SESSION_USAGE " [name]",
	0,
	NULL,
	NULL,
	cmd_show_environment_exec
};

int
cmd_show_environment_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct session		*s;
	struct environ		*env;
	struct environ_entry	*envent;

	if (args_has(self->args, 'g'))
		env = &global_environ;
	else {
		if ((s = cmd_find_session(ctx, args_get(args, 't'), 0)) == NULL)
			return (-1);
		env = &s->environ;
	}

	if (args->argc != 0) {
		envent = environ_find(env, args->argv[0]);
		if (envent == NULL) {
			ctx->error(ctx, "unknown variable: %s", args->argv[0]);
			return (-1);
		}
		if (envent->value != NULL)
			ctx->print(ctx, "%s=%s", envent->name, envent->value);
		else
			ctx->print(ctx, "-%s", envent->name);
		return (0);
	}

	RB_FOREACH(envent, environ, env) {
		if (envent->value != NULL)
			ctx->print(ctx, "%s=%s", envent->name, envent->value);
		else
			ctx->print(ctx, "-%s", envent->name);
	}

	return (0);
}
