/* $Id$ */

/*
 * Copyright (c) 2008 Tiago Cunha <me@tiagocunha.org>
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
 * Sources a configuration file.
 */

enum cmd_retval	 cmd_source_file_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_source_file_entry = {
	"source-file", "source",
	"", 1, 1,
	"path",
	0,
	NULL,
	NULL,
	cmd_source_file_exec
};

enum cmd_retval
cmd_source_file_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	int			 retval;
	u_int			 i;
	char			*cause;

	retval = load_cfg(args->argv[0], ctx, &cfg_causes);

	/*
	 * If the context for the cmdclient came from tmux's configuration
	 * file, then return the status of this command now, regardless of the
	 * error condition. Any errors from parsing a configuration file at
	 * startup will be handled for us by the server.
	 */
	if (cfg_references > 0 ||
	    (ctx->curclient == NULL && ctx->cmdclient == NULL))
		return (retval);

	/*
	 * We were called from the command-line in which case print the errors
	 * gathered here directly.
	 */
	for (i = 0; i < ARRAY_LENGTH(&cfg_causes); i++) {
		cause = ARRAY_ITEM(&cfg_causes, i);
		ctx->print(ctx, "%s", cause);
		free(cause);
	}
	ARRAY_FREE(&cfg_causes);

	return (retval);
}
