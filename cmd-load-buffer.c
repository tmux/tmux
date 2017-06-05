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

static void		cmd_load_buffer_callback(struct client *, int, void *);

const struct cmd_entry cmd_load_buffer_entry = {
	.name = "load-buffer",
	.alias = "loadb",

	.args = { "b:", 1, 1 },
	.usage = CMD_BUFFER_USAGE " path",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_load_buffer_exec
};

struct cmd_load_buffer_data {
	struct cmdq_item	*item;
	char			*bufname;
};

static enum cmd_retval
cmd_load_buffer_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	struct cmd_load_buffer_data	*cdata;
	struct client			*c = item->client;
	FILE				*f;
	const char			*path, *bufname;
	char				*pdata, *new_pdata, *cause, *file;
	size_t				 psize;
	int				 ch, error;

	bufname = NULL;
	if (args_has(args, 'b'))
		bufname = args_get(args, 'b');

	path = args->argv[0];
	if (strcmp(path, "-") == 0) {
		cdata = xcalloc(1, sizeof *cdata);
		cdata->item = item;

		if (bufname != NULL)
			cdata->bufname = xstrdup(bufname);

		error = server_set_stdin_callback(c, cmd_load_buffer_callback,
		    cdata, &cause);
		if (error != 0) {
			cmdq_error(item, "%s: %s", path, cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		return (CMD_RETURN_WAIT);
	}

	file = server_client_get_path(c, path);
	f = fopen(file, "rb");
	if (f == NULL) {
		cmdq_error(item, "%s: %s", file, strerror(errno));
		free(file);
		return (CMD_RETURN_ERROR);
	}

	pdata = NULL;
	psize = 0;
	while ((ch = getc(f)) != EOF) {
		/* Do not let the server die due to memory exhaustion. */
		if ((new_pdata = realloc(pdata, psize + 2)) == NULL) {
			cmdq_error(item, "realloc error: %s", strerror(errno));
			goto error;
		}
		pdata = new_pdata;
		pdata[psize++] = ch;
	}
	if (ferror(f)) {
		cmdq_error(item, "%s: read error", file);
		goto error;
	}
	if (pdata != NULL)
		pdata[psize] = '\0';

	fclose(f);
	free(file);

	if (paste_set(pdata, psize, bufname, &cause) != 0) {
		cmdq_error(item, "%s", cause);
		free(pdata);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	return (CMD_RETURN_NORMAL);

error:
	free(pdata);
	if (f != NULL)
		fclose(f);
	free(file);
	return (CMD_RETURN_ERROR);
}

static void
cmd_load_buffer_callback(struct client *c, int closed, void *data)
{
	struct cmd_load_buffer_data	*cdata = data;
	char				*pdata, *cause, *saved;
	size_t				 psize;

	if (!closed)
		return;
	c->stdin_callback = NULL;

	server_client_unref(c);
	if (c->flags & CLIENT_DEAD)
		goto out;

	psize = EVBUFFER_LENGTH(c->stdin_data);
	if (psize == 0 || (pdata = malloc(psize + 1)) == NULL)
		goto out;

	memcpy(pdata, EVBUFFER_DATA(c->stdin_data), psize);
	pdata[psize] = '\0';
	evbuffer_drain(c->stdin_data, psize);

	if (paste_set(pdata, psize, cdata->bufname, &cause) != 0) {
		/* No context so can't use server_client_msg_error. */
		if (~c->flags & CLIENT_UTF8) {
			saved = cause;
			cause = utf8_sanitize(saved);
			free(saved);
		}
		evbuffer_add_printf(c->stderr_data, "%s", cause);
		server_client_push_stderr(c);
		free(pdata);
		free(cause);
	}
out:
	cdata->item->flags &= ~CMDQ_WAITING;

	free(cdata->bufname);
	free(cdata);
}
