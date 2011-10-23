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

#include "tmux.h"

/*
 * Show a paste buffer.
 */

int	cmd_show_buffer_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_show_buffer_entry = {
	"show-buffer", "showb",
	"b:", 0, 0,
	CMD_BUFFER_USAGE,
	0,
	NULL,
	NULL,
	cmd_show_buffer_exec
};

int
cmd_show_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct session		*s;
	struct paste_buffer	*pb;
	int			 buffer;
	char			*in, *buf, *ptr, *cause;
	size_t			 size, len;
	u_int			 width;

	if ((s = cmd_find_session(ctx, NULL, 0)) == NULL)
		return (-1);

	if (!args_has(args, 'b')) {
		if ((pb = paste_get_top(&global_buffers)) == NULL) {
			ctx->error(ctx, "no buffers");
			return (-1);
		}
	} else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "buffer %s", cause);
			xfree(cause);
			return (-1);
		}

		pb = paste_get_index(&global_buffers, buffer);
		if (pb == NULL) {
			ctx->error(ctx, "no buffer %d", buffer);
			return (-1);
		}
	}

	size = pb->size;
	if (size > SIZE_MAX / 4 - 1)
		size = SIZE_MAX / 4 - 1;
	in = xmalloc(size * 4 + 1);
	strvisx(in, pb->data, size, VIS_OCTAL|VIS_TAB);

	width = s->sx;
	if (ctx->cmdclient != NULL)
		width = ctx->cmdclient->tty.sx;

	buf = xmalloc(width + 1);
	len = 0;

	ptr = in;
	do {
		buf[len++] = *ptr++;

		if (len == width || buf[len - 1] == '\n') {
			if (buf[len - 1] == '\n')
				len--;
			buf[len] = '\0';

			ctx->print(ctx, "%s", buf);
			len = 0;
		}
	} while (*ptr != '\0');

	if (len != 0) {
		buf[len] = '\0';
		ctx->print(ctx, "%s", buf);
	}
	xfree(buf);

	xfree(in);

	return (0);
}
