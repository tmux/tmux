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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Saves a paste buffer to a file.
 */

static enum cmd_retval	cmd_save_buffer_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_save_buffer_entry = {
	.name = "save-buffer",
	.alias = "saveb",

	.args = { "ab:", 1, 1 },
	.usage = "[-a] " CMD_BUFFER_USAGE " path",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_save_buffer_exec
};

const struct cmd_entry cmd_show_buffer_entry = {
	.name = "show-buffer",
	.alias = "showb",

	.args = { "b:", 0, 0 },
	.usage = CMD_BUFFER_USAGE,

	.flags = CMD_AFTERHOOK,
	.exec = cmd_save_buffer_exec
};

static enum cmd_retval
cmd_save_buffer_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct client		*c = item->client;
	struct paste_buffer	*pb;
	const char		*path, *bufname, *bufdata, *start, *end;
	const char		*flags;
	char			*msg, *file;
	size_t			 size, used, msglen, bufsize;
	FILE			*f;

	if (!args_has(args, 'b')) {
		if ((pb = paste_get_top(NULL)) == NULL) {
			cmdq_error(item, "no buffers");
			return (CMD_RETURN_ERROR);
		}
	} else {
		bufname = args_get(args, 'b');
		pb = paste_get_name(bufname);
		if (pb == NULL) {
			cmdq_error(item, "no buffer %s", bufname);
			return (CMD_RETURN_ERROR);
		}
	}
	bufdata = paste_buffer_data(pb, &bufsize);

	if (self->entry == &cmd_show_buffer_entry)
		path = "-";
	else
		path = args->argv[0];
	if (strcmp(path, "-") == 0) {
		if (c == NULL) {
			cmdq_error(item, "can't write to stdout");
			return (CMD_RETURN_ERROR);
		}
		if (c->session == NULL || (c->flags & CLIENT_CONTROL))
			goto do_stdout;
		goto do_print;
	}

	flags = "wb";
	if (args_has(self->args, 'a'))
		flags = "ab";

	file = server_client_get_path(c, path);
	f = fopen(file, flags);
	if (f == NULL) {
		cmdq_error(item, "%s: %s", file, strerror(errno));
		free(file);
		return (CMD_RETURN_ERROR);
	}

	if (fwrite(bufdata, 1, bufsize, f) != bufsize) {
		cmdq_error(item, "%s: write error", file);
		fclose(f);
		free(file);
		return (CMD_RETURN_ERROR);
	}

	fclose(f);
	free(file);

	return (CMD_RETURN_NORMAL);

do_stdout:
	evbuffer_add(c->stdout_data, bufdata, bufsize);
	server_client_push_stdout(c);
	return (CMD_RETURN_NORMAL);

do_print:
	if (bufsize > (INT_MAX / 4) - 1) {
		cmdq_error(item, "buffer too big");
		return (CMD_RETURN_ERROR);
	}
	msg = NULL;

	used = 0;
	while (used != bufsize) {
		start = bufdata + used;
		end = memchr(start, '\n', bufsize - used);
		if (end != NULL)
			size = end - start;
		else
			size = bufsize - used;

		msglen = size * 4 + 1;
		msg = xrealloc(msg, msglen);

		strvisx(msg, start, size, VIS_OCTAL|VIS_TAB);
		cmdq_print(item, "%s", msg);

		used += size + (end != NULL);
	}

	free(msg);
	return (CMD_RETURN_NORMAL);
}
