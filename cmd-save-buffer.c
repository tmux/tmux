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
#include <vis.h>

#include "tmux.h"

/*
 * Saves a paste buffer to a file.
 */

enum cmd_retval	 cmd_save_buffer_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_save_buffer_entry = {
	"save-buffer", "saveb",
	"ab:", 1, 1,
	"[-a] " CMD_BUFFER_USAGE " path",
	0,
	NULL,
	cmd_save_buffer_exec
};

const struct cmd_entry cmd_show_buffer_entry = {
	"show-buffer", "showb",
	"b:", 0, 0,
	CMD_BUFFER_USAGE,
	0,
	NULL,
	cmd_save_buffer_exec
};

enum cmd_retval
cmd_save_buffer_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c = cmdq->client;
	struct session          *s;
	struct paste_buffer	*pb;
	const char		*path;
	char			*cause, *start, *end, *msg;
	size_t			 size, used, msglen;
	int			 cwd, fd, buffer;
	FILE			*f;

	if (!args_has(args, 'b')) {
		if ((pb = paste_get_top(&global_buffers)) == NULL) {
			cmdq_error(cmdq, "no buffers");
			return (CMD_RETURN_ERROR);
		}
	} else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(cmdq, "buffer %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		pb = paste_get_index(&global_buffers, buffer);
		if (pb == NULL) {
			cmdq_error(cmdq, "no buffer %d", buffer);
			return (CMD_RETURN_ERROR);
		}
	}

	if (self->entry == &cmd_show_buffer_entry)
		path = "-";
	else
		path = args->argv[0];
	if (strcmp(path, "-") == 0) {
		if (c == NULL) {
			cmdq_error(cmdq, "can't write to stdout");
			return (CMD_RETURN_ERROR);
		}
		if (c->session == NULL || (c->flags & CLIENT_CONTROL))
			goto do_stdout;
		goto do_print;
	}

	if (c != NULL && c->session == NULL)
		cwd = c->cwd;
	else if ((s = cmd_current_session(cmdq, 0)) != NULL)
		cwd = s->cwd;
	else
		cwd = AT_FDCWD;

	f = NULL;
	if (args_has(self->args, 'a')) {
		fd = openat(cwd, path, O_CREAT|O_RDWR|O_APPEND, 0600);
		if (fd != -1)
			f = fdopen(fd, "ab");
	} else {
		fd = openat(cwd, path, O_CREAT|O_RDWR, 0600);
		if (fd != -1)
			f = fdopen(fd, "wb");
	}
	if (f == NULL) {
		if (fd != -1)
			close(fd);
		cmdq_error(cmdq, "%s: %s", path, strerror(errno));
		return (CMD_RETURN_ERROR);
	}
	if (fwrite(pb->data, 1, pb->size, f) != pb->size) {
		cmdq_error(cmdq, "%s: fwrite error", path);
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
		cmdq_error(cmdq, "buffer too big");
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
		cmdq_print(cmdq, "%s", msg);

		used += size + (end != NULL);
	}

	free(msg);
	return (CMD_RETURN_NORMAL);
}
