/* $Id: cmd-list-buffers.c,v 1.1 2008-06-20 17:31:48 nicm Exp $ */

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
 * List paste buffers.
 */

void	cmd_list_buffers_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_list_buffers_entry = {
	"list-buffers", "lsb",
	CMD_TARGET_SESSION_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_list_buffers_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_list_buffers_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;
	struct paste_buffer	*pb;
	u_int			 idx;
	char			 tmp[16], *tim;
	size_t			 in, out;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return;

	idx = 0;
	while ((pb = paste_walk_stack(&s->buffers, &idx)) != NULL) {
		in = out = 0;
		while (out < (sizeof tmp) - 1 && pb->data[in] != '\0') {
			if (pb->data[in] > 31 && pb->data[in] != 127)
				tmp[out++] = pb->data[in];
			in++;
		}
		tmp[out] = '\0';
		if (out == (sizeof tmp) - 1) {
			tmp[out - 1] = '.';
			tmp[out - 2] = '.';
		}

		tim = ctime(&pb->ts.tv_sec);
		*strchr(tim, '\n') = '\0';

		ctx->print(ctx, "%d: %zu bytes "
		    "(created %s): \"%s\"", idx, strlen(pb->data), tim, tmp);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
