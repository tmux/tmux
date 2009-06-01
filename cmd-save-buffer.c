/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
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
#include <sys/stat.h>

#include <errno.h>
#include <string.h>

#include "tmux.h"

/*
 * Saves a session paste buffer to a file.
 */

int	cmd_save_buffer_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_save_buffer_entry = {
	"save-buffer", "saveb",
	"[-a] " CMD_BUFFER_SESSION_USAGE " path",
	CMD_AFLAG|CMD_ARG1,
	cmd_buffer_init,
	cmd_buffer_parse,
	cmd_save_buffer_exec,
	cmd_buffer_send,
	cmd_buffer_recv,
	cmd_buffer_free,
	cmd_buffer_print
};

int
cmd_save_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_buffer_data	*data = self->data;
	struct session		*s;
	struct paste_buffer	*pb;
	mode_t			mask;
	FILE			*f;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

	if (data->buffer == -1) {
		if ((pb = paste_get_top(&s->buffers)) == NULL) {
			ctx->error(ctx, "no buffers");
			return (-1);
		}
	} else {
		if ((pb = paste_get_index(&s->buffers, data->buffer)) == NULL) {
			ctx->error(ctx, "no buffer %d", data->buffer);
			return (-1);
		}
	}

	mask = umask(S_IRWXG | S_IRWXO);
	if (data->flags & CMD_AFLAG)
		f = fopen(data->arg, "ab");
	else
		f = fopen(data->arg, "wb");
	if (f == NULL) {
		ctx->error(ctx, "%s: %s", data->arg, strerror(errno));
		return (-1);
	}

	if (fwrite(pb->data, 1, strlen(pb->data), f) != strlen(pb->data)) {
	    	ctx->error(ctx, "%s: fwrite error", data->arg);
	    	fclose(f);
	    	return (-1);
	}

	fclose(f);
	umask(mask);

	return (0);
}
