/* $Id$ */

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

enum cmd_retval	 cmd_capture_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_capture_pane_entry = {
	"capture-pane", "capturep",
	"b:E:S:t:", 0, 0,
	"[-b buffer-index] [-E end-line] [-S start-line] "
	CMD_TARGET_PANE_USAGE,
	0,
	NULL,
	NULL,
	cmd_capture_pane_exec
};

enum cmd_retval
cmd_capture_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	char 			*buf, *line, *cause;
	struct screen		*s;
	struct grid		*gd;
	int			 buffer, n;
	u_int			 i, limit, top, bottom, tmp;
	size_t         		 len, linelen;

	if (cmd_find_pane(ctx, args_get(args, 't'), NULL, &wp) == NULL)
		return (CMD_RETURN_ERROR);
	s = &wp->base;
	gd = s->grid;

	buf = NULL;
	len = 0;

	n = args_strtonum(args, 'S', INT_MIN, SHRT_MAX, &cause);
	if (cause != NULL) {
		top = gd->hsize;
		free(cause);
	} else if (n < 0 && (u_int) -n > gd->hsize)
		top = 0;
	else
		top = gd->hsize + n;
	if (top > gd->hsize + gd->sy - 1)
		top = gd->hsize + gd->sy - 1;

	n = args_strtonum(args, 'E', INT_MIN, SHRT_MAX, &cause);
	if (cause != NULL) {
		bottom = gd->hsize + gd->sy - 1;
		free(cause);
	} else if (n < 0 && (u_int) -n > gd->hsize)
		bottom = 0;
	else
		bottom = gd->hsize + n;
	if (bottom > gd->hsize + gd->sy - 1)
		bottom = gd->hsize + gd->sy - 1;

	if (bottom < top) {
		tmp = bottom;
		bottom = top;
		top = tmp;
	}

	for (i = top; i <= bottom; i++) {
	       line = grid_string_cells(s->grid, 0, i, screen_size_x(s));
	       linelen = strlen(line);

	       buf = xrealloc(buf, 1, len + linelen + 1);
	       memcpy(buf + len, line, linelen);
	       len += linelen;
	       buf[len++] = '\n';

	       free(line);
	}

	limit = options_get_number(&global_options, "buffer-limit");

	if (!args_has(args, 'b')) {
		paste_add(&global_buffers, buf, len, limit);
		return (CMD_RETURN_NORMAL);
	}

	buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
	if (cause != NULL) {
		ctx->error(ctx, "buffer %s", cause);
		free(buf);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	if (paste_replace(&global_buffers, buffer, buf, len) != 0) {
		ctx->error(ctx, "no buffer %d", buffer);
		free(buf);
		return (CMD_RETURN_ERROR);
	}

	return (CMD_RETURN_NORMAL);
}
