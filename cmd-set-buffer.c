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

#include <string.h>

#include "tmux.h"

/*
 * Add or set a paste buffer.
 */

int	cmd_set_buffer_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_set_buffer_entry = {
	"set-buffer", "setb",
	"b:", 1, 1,
	CMD_BUFFER_USAGE " data",
	0,
	NULL,
	NULL,
	cmd_set_buffer_exec
};

int
cmd_set_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	u_int		 limit;
	char		*pdata, *cause;
	size_t		 psize;
	int		 buffer;

	limit = options_get_number(&global_options, "buffer-limit");

	pdata = xstrdup(args->argv[0]);
	psize = strlen(pdata);

	if (!args_has(args, 'b')) {
		paste_add(&global_buffers, pdata, psize, limit);
		return (0);
	}

	buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
	if (cause != NULL) {
		ctx->error(ctx, "buffer %s", cause);
		xfree(cause);
		xfree(pdata);
		return (-1);
	}

	if (paste_replace(&global_buffers, buffer, pdata, psize) != 0) {
		ctx->error(ctx, "no buffer %d", buffer);
		xfree(pdata);
		return (-1);
	}

	return (0);
}
