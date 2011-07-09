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

int	cmd_paste_buffer_exec(struct cmd *, struct cmd_ctx *);

void	cmd_paste_buffer_filter(
	    struct window_pane *, const char *, size_t, const char *);

const struct cmd_entry cmd_paste_buffer_entry = {
	"paste-buffer", "pasteb",
	"db:rs:t:", 0, 0,
	"[-dr] [-s separator] [-b buffer-index] [-t target-pane]",
	0,
	NULL,
	NULL,
	cmd_paste_buffer_exec
};

int
cmd_paste_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	struct session		*s;
	struct paste_buffer	*pb;
	const char		*sepstr;
	char			*cause;
	int			 buffer;

	if (cmd_find_pane(ctx, args_get(args, 't'), &s, &wp) == NULL)
		return (-1);

	if (!args_has(args, 'b'))
		buffer = -1;
	else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "buffer %s", cause);
			xfree(cause);
			return (-1);
		}
	}

	if (buffer == -1)
		pb = paste_get_top(&global_buffers);
	else {
		pb = paste_get_index(&global_buffers, buffer);
		if (pb == NULL) {
			ctx->error(ctx, "no buffer %d", buffer);
			return (-1);
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
		cmd_paste_buffer_filter(wp, pb->data, pb->size, sepstr);
	}

	/* Delete the buffer if -d. */
	if (args_has(args, 'd')) {
		if (buffer == -1)
			paste_free_top(&global_buffers);
		else
			paste_free_index(&global_buffers, buffer);
	}

	return (0);
}

/* Add bytes to a buffer and filter '\n' according to separator. */
void
cmd_paste_buffer_filter(
    struct window_pane *wp, const char *data, size_t size, const char *sep)
{
	const char	*end = data + size;
	const char	*lf;
	size_t		 seplen;

	seplen = strlen(sep);
	while ((lf = memchr(data, '\n', end - data)) != NULL) {
		if (lf != data)
			bufferevent_write(wp->event, data, lf - data);
		bufferevent_write(wp->event, sep, seplen);
		data = lf + 1;
	}

	if (end != data)
		bufferevent_write(wp->event, data, end - data);
}
