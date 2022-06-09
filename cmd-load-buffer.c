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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Loads a paste buffer from a file.
 */

static enum cmd_retval	cmd_load_buffer_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_load_buffer_entry = {
	.name = "load-buffer",
	.alias = "loadb",

	.args = { "b:t:w", 1, 1, NULL },
	.usage = CMD_BUFFER_USAGE " " CMD_TARGET_CLIENT_USAGE " path",

	.flags = CMD_AFTERHOOK|CMD_CLIENT_TFLAG|CMD_CLIENT_CANFAIL,
	.exec = cmd_load_buffer_exec
};

struct cmd_load_buffer_data {
	struct client		*client;
	struct cmdq_item	*item;
	char			*name;
};

static void
cmd_load_buffer_done(__unused struct client *c, const char *path, int error,
    int closed, struct evbuffer *buffer, void *data)
{
	struct cmd_load_buffer_data	*cdata = data;
	struct client			*tc = cdata->client;
	struct cmdq_item		*item = cdata->item;
	void				*bdata = EVBUFFER_DATA(buffer);
	size_t				 bsize = EVBUFFER_LENGTH(buffer);
	void				*copy;
	char				*cause;

	if (!closed)
		return;

	if (error != 0)
		cmdq_error(item, "%s: %s", path, strerror(error));
	else if (bsize != 0) {
		copy = xmalloc(bsize);
		memcpy(copy, bdata, bsize);
		if (paste_set(copy, bsize, cdata->name, &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			free(copy);
		} else if (tc != NULL &&
		    tc->session != NULL &&
		    (~tc->flags & CLIENT_DEAD))
			tty_set_selection(&tc->tty, "", copy, bsize);
		if (tc != NULL)
			server_client_unref(tc);
	}
	cmdq_continue(item);

	free(cdata->name);
	free(cdata);
}

static enum cmd_retval
cmd_load_buffer_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct client			*tc = cmdq_get_target_client(item);
	struct cmd_load_buffer_data	*cdata;
	const char			*bufname = args_get(args, 'b');
	char				*path;

	cdata = xcalloc(1, sizeof *cdata);
	cdata->item = item;
	if (bufname != NULL)
		cdata->name = xstrdup(bufname);
	if (args_has(args, 'w') && tc != NULL) {
		cdata->client = tc;
		cdata->client->references++;
	}

	path = format_single_from_target(item, args_string(args, 0));
	file_read(cmdq_get_client(item), path, cmd_load_buffer_done, cdata);
	free(path);

	return (CMD_RETURN_WAIT);
}
