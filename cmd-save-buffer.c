/* $Id$ */

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

#include "tmux.h"

/*
 * Saves a paste buffer to a file.
 */

enum cmd_retval	 cmd_save_buffer_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_save_buffer_entry = {
	"save-buffer", "saveb",
	"ab:", 1, 1,
	"[-a] " CMD_BUFFER_USAGE " path",
	0,
	NULL,
	NULL,
	cmd_save_buffer_exec
};

const struct cmd_entry cmd_show_buffer_entry = {
	"show-buffer", "showb",
	"b:", 0, 0,
	CMD_BUFFER_USAGE,
	0,
	NULL,
	NULL,
	cmd_save_buffer_exec
};

enum cmd_retval
cmd_save_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct client		*c;
	struct session          *s;
	struct paste_buffer	*pb;
	const char		*path, *newpath, *wd;
	char			*cause, *start, *end;
	size_t			 size, used;
	int			 buffer;
	mode_t			 mask;
	FILE			*f;
	char			*msg;
	size_t			 msglen;

	if (!args_has(args, 'b')) {
		if ((pb = paste_get_top(&global_buffers)) == NULL) {
			ctx->error(ctx, "no buffers");
			return (CMD_RETURN_ERROR);
		}
	} else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "buffer %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		pb = paste_get_index(&global_buffers, buffer);
		if (pb == NULL) {
			ctx->error(ctx, "no buffer %d", buffer);
			return (CMD_RETURN_ERROR);
		}
	}

	if (self->entry == &cmd_show_buffer_entry)
		path = "-";
	else
		path = args->argv[0];
	if (strcmp(path, "-") == 0) {
		c = ctx->cmdclient;
		if (c != NULL)
			goto do_stdout;
		c = ctx->curclient;
		if (c->flags & CLIENT_CONTROL)
			goto do_stdout;
		if (c != NULL)
			goto do_print;
		ctx->error(ctx, "can't write to stdout");
		return (CMD_RETURN_ERROR);
	}

	c = ctx->cmdclient;
	if (c != NULL)
		wd = c->cwd;
	else if ((s = cmd_current_session(ctx, 0)) != NULL) {
		wd = options_get_string(&s->options, "default-path");
		if (*wd == '\0')
			wd = s->cwd;
	} else
		wd = NULL;
	if (wd != NULL && *wd != '\0') {
		newpath = get_full_path(wd, path);
		if (newpath != NULL)
			path = newpath;
	}

	mask = umask(S_IRWXG | S_IRWXO);
	if (args_has(self->args, 'a'))
		f = fopen(path, "ab");
	else
		f = fopen(path, "wb");
	umask(mask);
	if (f == NULL) {
		ctx->error(ctx, "%s: %s", path, strerror(errno));
		return (CMD_RETURN_ERROR);
	}
	if (fwrite(pb->data, 1, pb->size, f) != pb->size) {
		ctx->error(ctx, "%s: fwrite error", path);
		fclose(f);
		return (CMD_RETURN_ERROR);
	}
	fclose(f);

	return (CMD_RETURN_NORMAL);

do_stdout:
	evbuffer_add(c->stdout_data, pb->data, pb->size);
	server_push_stdout(c);
	return (CMD_RETURN_NORMAL);

do_print:
	if (pb->size > (INT_MAX / 4) - 1) {
		ctx->error(ctx, "buffer too big");
		return (CMD_RETURN_ERROR);
	}
	msg = NULL;
	msglen = 0;

	used = 0;
	while (used != pb->size) {
		start = pb->data + used;
		end = memchr(start, '\n', pb->size - used);
		if (end != NULL)
			size = end - start;
		else
			size = pb->size - used;

		msglen = size * 4 + 1;
		msg = xrealloc(msg, 1, msglen);

		strvisx(msg, start, size, VIS_OCTAL|VIS_TAB);
		ctx->print(ctx, "%s", msg);

		used += size + (end != NULL);
	}

	free(msg);
	return (CMD_RETURN_NORMAL);
}
