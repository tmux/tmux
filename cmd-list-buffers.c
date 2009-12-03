/* $OpenBSD$ */

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
#include <vis.h>

#include "tmux.h"

/*
 * List paste buffers.
 */

int	cmd_list_buffers_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_list_buffers_entry = {
	"list-buffers", "lsb",
	CMD_TARGET_SESSION_USAGE,
	0, "",
	cmd_target_init,
	cmd_target_parse,
	cmd_list_buffers_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_list_buffers_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;
	struct paste_buffer	*pb;
	u_int			 idx;
	char			 tmp[51 * 4 + 1];
	size_t			 size, len;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

	idx = 0;
	while ((pb = paste_walk_stack(&s->buffers, &idx)) != NULL) {
		size = pb->size;

		/* Translate the first 50 characters. */
		len = size;
		if (len > 50)
			len = 50;
		strvisx(tmp, pb->data, len, VIS_OCTAL|VIS_TAB|VIS_NL);

		/*
		 * If the first 50 characters were encoded as a longer string,
		 * or there is definitely more data, add "...".
		 */
		if (size > 50 || strlen(tmp) > 50) {
			tmp[50 - 3] = '\0';
			strlcat(tmp, "...", sizeof tmp);
		}

		ctx->print(ctx, "%u: %zu bytes: \"%s\"", idx - 1, size, tmp);
	}

	return (0);
}
