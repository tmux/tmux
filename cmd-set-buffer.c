/* $OpenBSD: cmd-set-buffer.c,v 1.37 2026/02/15 17:43:26 nicm Exp $ */

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

static enum cmd_retval	cmd_set_buffer_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_set_buffer_entry = {
	.name = "set-buffer",
	.alias = "setb",

	.args = { "ab:t:n:w", 0, 1, NULL },
	.usage = "[-aw] " CMD_BUFFER_USAGE " [-n new-buffer-name] "
	         CMD_TARGET_CLIENT_USAGE " [data]",

	.flags = CMD_AFTERHOOK|CMD_CLIENT_TFLAG|CMD_CLIENT_CANFAIL,
	.exec = cmd_set_buffer_exec
};

const struct cmd_entry cmd_delete_buffer_entry = {
	.name = "delete-buffer",
	.alias = "deleteb",

	.args = { "b:", 0, 0, NULL },
	.usage = CMD_BUFFER_USAGE,

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_buffer_exec
};

static enum cmd_retval
cmd_set_buffer_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*tc = cmdq_get_target_client(item);
	struct paste_buffer	*pb = NULL;
	char			*bufname = NULL, *bufdata = NULL, *cause = NULL;
	const char		*olddata;
	size_t			 bufsize = 0, newsize;

	if (args_get(args, 'b') != NULL) {
		bufname = xstrdup(args_get(args, 'b'));
		pb = paste_get_name(bufname);
	}

	if (cmd_get_entry(self) == &cmd_delete_buffer_entry) {
		if (pb == NULL) {
			if (bufname != NULL) {
				cmdq_error(item, "unknown buffer: %s", bufname);
				goto fail;
			}
			pb = paste_get_top(&bufname);
		}
		if (pb == NULL) {
			cmdq_error(item, "no buffer");
			goto fail;
		}
		paste_free(pb);
		free(bufname);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'n')) {
		if (pb == NULL) {
			if (bufname != NULL) {
				cmdq_error(item, "unknown buffer: %s", bufname);
				goto fail;
			}
			pb = paste_get_top(&bufname);
		}
		if (pb == NULL) {
			cmdq_error(item, "no buffer");
			goto fail;
		}
		if (paste_rename(bufname, args_get(args, 'n'), &cause) != 0) {
			cmdq_error(item, "%s", cause);
			goto fail;
		}
		return (CMD_RETURN_NORMAL);
	}

	if (args_count(args) != 1) {
		cmdq_error(item, "no data specified");
		goto fail;
	}
	if ((newsize = strlen(args_string(args, 0))) == 0)
		return (CMD_RETURN_NORMAL);

	if (args_has(args, 'a') && pb != NULL) {
		olddata = paste_buffer_data(pb, &bufsize);
		bufdata = xmalloc(bufsize);
		memcpy(bufdata, olddata, bufsize);
	}

	bufdata = xrealloc(bufdata, bufsize + newsize);
	memcpy(bufdata + bufsize, args_string(args, 0), newsize);
	bufsize += newsize;

	if (paste_set(bufdata, bufsize, bufname, &cause) != 0) {
		cmdq_error(item, "%s", cause);
		goto fail;
	}
	if (args_has(args, 'w') && tc != NULL)
 		tty_set_selection(&tc->tty, "", bufdata, bufsize);

	return (CMD_RETURN_NORMAL);

fail:
	free(bufdata);
	free(bufname);
	free(cause);
	return (CMD_RETURN_ERROR);
}
