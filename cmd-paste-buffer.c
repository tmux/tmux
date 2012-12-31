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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Paste paste buffer if present.
 */

enum cmd_retval	 cmd_paste_buffer_exec(struct cmd *, struct cmd_ctx *);

void	cmd_paste_buffer_filter(struct window_pane *,
	    const char *, size_t, const char *, int);

const struct cmd_entry cmd_paste_buffer_entry = {
	"paste-buffer", "pasteb",
	"db:prs:t:", 0, 0,
	"[-dpr] [-s separator] [-b buffer-index] " CMD_TARGET_PANE_USAGE,
	0,
	NULL,
	NULL,
	cmd_paste_buffer_exec
};

enum cmd_retval
cmd_paste_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	struct session		*s;
	struct paste_buffer	*pb;
	const char		*sepstr;
	char			*cause;
	int			 buffer;
	int			 pflag;

	if (cmd_find_pane(ctx, args_get(args, 't'), &s, &wp) == NULL)
		return (CMD_RETURN_ERROR);

	if (!args_has(args, 'b'))
		buffer = -1;
	else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "buffer %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	if (buffer == -1)
		pb = paste_get_top(&global_buffers);
	else {
		pb = paste_get_index(&global_buffers, buffer);
		if (pb == NULL) {
			ctx->error(ctx, "no buffer %d", buffer);
			return (CMD_RETURN_ERROR);
		}
	}

	if (pb != NULL) {
		sepstr = args_get(args, 's');
		if (sepstr == NULL) {
			if (args_has(args, 'r'))
				sepstr = "\n";
			else
				sepstr = "\r";
		}
		pflag = (wp->screen->mode & MODE_BRACKETPASTE);
		paste_send_pane(pb, wp, sepstr, args_has(args, 'p') && pflag);
	}

	/* Delete the buffer if -d. */
	if (args_has(args, 'd')) {
		if (buffer == -1)
			paste_free_top(&global_buffers);
		else
			paste_free_index(&global_buffers, buffer);
	}

	return (CMD_RETURN_NORMAL);
}
