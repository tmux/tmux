/* $OpenBSD: cmd-load-buffer.c,v 1.2 2009/07/09 09:54:56 nicm Exp $ */

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Loads a session paste buffer from a file.
 */

int	cmd_load_buffer_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_load_buffer_entry = {
	"load-buffer", "loadb",
	CMD_BUFFER_SESSION_USAGE " path",
	CMD_ARG1,
	cmd_buffer_init,
	cmd_buffer_parse,
	cmd_load_buffer_exec,
	cmd_buffer_send,
	cmd_buffer_recv,
	cmd_buffer_free,
	cmd_buffer_print
};

int
cmd_load_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_buffer_data	*data = self->data;
	struct session		*s;
	struct stat		statbuf;
	FILE			*f;
	char			*buf;
	u_int			limit;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

	if (stat(data->arg, &statbuf) < 0) {
		ctx->error(ctx, "%s: %s", data->arg, strerror(errno));
		return (-1);
	}
	if (!S_ISREG(statbuf.st_mode)) {
		ctx->error(ctx, "%s: not a regular file", data->arg);
		return (-1);
	}

	if ((f = fopen(data->arg, "rb")) == NULL) {
		ctx->error(ctx, "%s: %s", data->arg, strerror(errno));
		return (-1);
	}

	/*
	 * We don't want to die due to memory exhaustion, hence xmalloc can't
	 * be used here.
	 */
	if ((buf = malloc(statbuf.st_size + 1)) == NULL) {
		ctx->error(ctx, "malloc error: %s", strerror(errno));
		fclose(f);
		return (-1);
	}

	if (fread(buf, 1, statbuf.st_size, f) != (size_t) statbuf.st_size) {
		ctx->error(ctx, "%s: fread error", data->arg);
		xfree(buf);
		fclose(f);
		return (-1);
	}

	buf[statbuf.st_size] = '\0';
	fclose(f);

	limit = options_get_number(&s->options, "buffer-limit");
	if (data->buffer == -1) {
		paste_add(&s->buffers, buf, limit);
		return (0);
	}
	if (paste_replace(&s->buffers, data->buffer, buf) != 0) {
		ctx->error(ctx, "no buffer %d", data->buffer);
		xfree(buf);
		return (-1);
	}

	return (0);
}
