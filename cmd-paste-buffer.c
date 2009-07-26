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

#include "tmux.h"

/*
 * Paste paste buffer if present.
 */

int	cmd_paste_buffer_exec(struct cmd *, struct cmd_ctx *);
void	cmd_paste_buffer_lf2cr(struct buffer *, const char *, size_t);

const struct cmd_entry cmd_paste_buffer_entry = {
	"paste-buffer", "pasteb",
	"[-dr] " CMD_BUFFER_WINDOW_USAGE,
	0, CMD_CHFLAG('d')|CMD_CHFLAG('r'),
	cmd_buffer_init,
	cmd_buffer_parse,
	cmd_paste_buffer_exec,
	cmd_buffer_free,
	cmd_buffer_print
};

int
cmd_paste_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_buffer_data	*data = self->data;
	struct winlink		*wl;
	struct window		*w;
	struct session		*s;
	struct paste_buffer	*pb;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return (-1);
	w = wl->window;

	if (data->buffer == -1)
		pb = paste_get_top(&s->buffers);
	else {
		if ((pb = paste_get_index(&s->buffers, data->buffer)) == NULL) {
			ctx->error(ctx, "no buffer %d", data->buffer);
			return (-1);
		}
	}

	if (pb != NULL && *pb->data != '\0') {
		/* -r means raw data without LF->CR conversion. */
		if (data->chflags & CMD_CHFLAG('r')) {
			buffer_write(
			    w->active->out, pb->data, strlen(pb->data));
		} else {
			cmd_paste_buffer_lf2cr(
			    w->active->out, pb->data, strlen(pb->data));
		}
	}

	/* Delete the buffer if -d. */
	if (data->chflags & CMD_CHFLAG('d')) {
		if (data->buffer == -1)
			paste_free_top(&s->buffers);
		else
			paste_free_index(&s->buffers, data->buffer);
	}

 	return (0);
}

/* Add bytes to a buffer but change every '\n' to '\r'. */
void
cmd_paste_buffer_lf2cr(struct buffer *b, const char *data, size_t size)
{
	const char	*end = data + size;
	const char	*lf;

	while ((lf = memchr(data, '\n', end - data)) != NULL) {
		if (lf != data)
			buffer_write(b, data, lf - data);
		buffer_write8(b, '\r');
		data = lf + 1;
	}

	if (end != data)
		buffer_write(b, data, end - data);
}
