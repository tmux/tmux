/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Add, set, append to or delete a paste buffer.
 */

enum cmd_retval	 cmd_set_buffer_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_set_buffer_entry = {
	.name = "set-buffer",
	.alias = "setb",

	.args = { "ab:n:", 0, 1 },
	.usage = "[-a] " CMD_BUFFER_USAGE " [-n new-buffer-name] data",

	.flags = 0,
	.exec = cmd_set_buffer_exec
};

const struct cmd_entry cmd_delete_buffer_entry = {
	.name = "delete-buffer",
	.alias = "deleteb",

	.args = { "b:", 0, 0 },
	.usage = CMD_BUFFER_USAGE,

	.flags = 0,
	.exec = cmd_set_buffer_exec
};

enum cmd_retval
cmd_set_buffer_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct paste_buffer	*pb;
	char			*bufdata, *cause;
	const char		*bufname, *olddata;
	size_t			 bufsize, newsize;

	bufname = args_get(args, 'b');
	if (bufname == NULL)
		pb = NULL;
	else
		pb = paste_get_name(bufname);

	if (self->entry == &cmd_delete_buffer_entry) {
		if (pb == NULL)
			pb = paste_get_top(&bufname);
		if (pb == NULL) {
			cmdq_error(cmdq, "no buffer");
			return (CMD_RETURN_ERROR);
		}
		paste_free(pb);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'n')) {
		if (pb == NULL)
			pb = paste_get_top(&bufname);
		if (pb == NULL) {
			cmdq_error(cmdq, "no buffer");
			return (CMD_RETURN_ERROR);
		}
		if (paste_rename(bufname, args_get(args, 'n'), &cause) != 0) {
			cmdq_error(cmdq, "%s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		return (CMD_RETURN_NORMAL);
	}

	if (args->argc != 1) {
		cmdq_error(cmdq, "no data specified");
		return (CMD_RETURN_ERROR);
	}
	if ((newsize = strlen(args->argv[0])) == 0)
		return (CMD_RETURN_NORMAL);

	bufsize = 0;
	bufdata = NULL;

	if (args_has(args, 'a') && pb != NULL) {
		olddata = paste_buffer_data(pb, &bufsize);
		bufdata = xmalloc(bufsize);
		memcpy(bufdata, olddata, bufsize);
	}

	bufdata = xrealloc(bufdata, bufsize + newsize);
	memcpy(bufdata + bufsize, args->argv[0], newsize);
	bufsize += newsize;

	if (paste_set(bufdata, bufsize, bufname, &cause) != 0) {
		cmdq_error(cmdq, "%s", cause);
		free(bufdata);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	return (CMD_RETURN_NORMAL);
}
