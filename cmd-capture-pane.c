/* $Id: cmd-capture-pane.c,v 1.6 2011-01-07 14:45:33 tcunha Exp $ */

/*
 * Copyright (c) 2009 Jonathan Alvarado <radobobo@users.sourceforge.net>
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
 * Write the entire contents of a pane to a buffer.
 */

int	cmd_capture_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_capture_pane_entry = {
	"capture-pane", "capturep",
	"b:t:", 0, 0,
	"[-b buffer-index] [-t target-pane]",
	0,
	NULL,
	NULL,
	cmd_capture_pane_exec
};

int
cmd_capture_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	char 			*buf, *line, *cause;
	struct screen		*s;
	int			 buffer;
	u_int			 i, limit;
	size_t         		 len, linelen;

	if (cmd_find_pane(ctx, args_get(args, 't'), NULL, &wp) == NULL)
		return (-1);
	s = &wp->base;

	buf = NULL;
	len = 0;

	for (i = 0; i < screen_size_y(s); i++) {
	       line = grid_view_string_cells(s->grid, 0, i, screen_size_x(s));
	       linelen = strlen(line);

	       buf = xrealloc(buf, 1, len + linelen + 1);
	       memcpy(buf + len, line, linelen);
	       len += linelen;
	       buf[len++] = '\n';

	       xfree(line);
	}

	limit = options_get_number(&global_options, "buffer-limit");

	if (!args_has(args, 'b')) {
		paste_add(&global_buffers, buf, len, limit);
		return (0);
	}

	buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
	if (cause != NULL) {
		ctx->error(ctx, "buffer %s", cause);
		xfree(cause);
		return (-1);
	}

	if (paste_replace(&global_buffers, buffer, buf, len) != 0) {
		ctx->error(ctx, "no buffer %d", buffer);
		xfree(buf);
		return (-1);
	}

	return (0);
}
