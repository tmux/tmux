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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Loads a paste buffer from a file.
 */

enum cmd_retval	 cmd_load_buffer_exec(struct cmd *, struct cmd_ctx *);
void		 cmd_load_buffer_callback(struct client *, int, void *);

const struct cmd_entry cmd_load_buffer_entry = {
	"load-buffer", "loadb",
	"b:", 1, 1,
	CMD_BUFFER_USAGE " path",
	0,
	NULL,
	NULL,
	cmd_load_buffer_exec
};

enum cmd_retval
cmd_load_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct client	*c = ctx->cmdclient;
	struct session  *s;
	FILE		*f;
	const char	*path, *newpath, *wd;
	char		*pdata, *new_pdata, *cause;
	size_t		 psize;
	u_int		 limit;
	int		 ch, error, buffer, *buffer_ptr;

	if (!args_has(args, 'b'))
		buffer = -1;
	else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "buffer %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	path = args->argv[0];
	if (strcmp(path, "-") == 0) {
		buffer_ptr = xmalloc(sizeof *buffer_ptr);
		*buffer_ptr = buffer;

		error = server_set_stdin_callback (c, cmd_load_buffer_callback,
		    buffer_ptr, &cause);
		if (error != 0) {
			ctx->error(ctx, "%s: %s", path, cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		return (CMD_RETURN_YIELD);
	}

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
	if ((f = fopen(path, "rb")) == NULL) {
		ctx->error(ctx, "%s: %s", path, strerror(errno));
		return (CMD_RETURN_ERROR);
	}

	pdata = NULL;
	psize = 0;
	while ((ch = getc(f)) != EOF) {
		/* Do not let the server die due to memory exhaustion. */
		if ((new_pdata = realloc(pdata, psize + 2)) == NULL) {
			ctx->error(ctx, "realloc error: %s", strerror(errno));
			goto error;
		}
		pdata = new_pdata;
		pdata[psize++] = ch;
	}
	if (ferror(f)) {
		ctx->error(ctx, "%s: read error", path);
		goto error;
	}
	if (pdata != NULL)
		pdata[psize] = '\0';

	fclose(f);

	limit = options_get_number(&global_options, "buffer-limit");
	if (buffer == -1) {
		paste_add(&global_buffers, pdata, psize, limit);
		return (CMD_RETURN_NORMAL);
	}
	if (paste_replace(&global_buffers, buffer, pdata, psize) != 0) {
		ctx->error(ctx, "no buffer %d", buffer);
		free(pdata);
		return (CMD_RETURN_ERROR);
	}

	return (CMD_RETURN_NORMAL);

error:
	free(pdata);
	if (f != NULL)
		fclose(f);
	return (CMD_RETURN_ERROR);
}

void
cmd_load_buffer_callback(struct client *c, int closed, void *data)
{
	int	*buffer = data;
	char	*pdata;
	size_t	 psize;
	u_int	 limit;

	if (!closed)
		return;
	c->stdin_callback = NULL;

	c->references--;
	c->flags |= CLIENT_EXIT;

	psize = EVBUFFER_LENGTH(c->stdin_data);
	if (psize == 0 || (pdata = malloc(psize + 1)) == NULL) {
		free(data);
		return;
	}
	memcpy(pdata, EVBUFFER_DATA(c->stdin_data), psize);
	pdata[psize] = '\0';
	evbuffer_drain(c->stdin_data, psize);

	limit = options_get_number(&global_options, "buffer-limit");
	if (*buffer == -1)
		paste_add(&global_buffers, pdata, psize, limit);
	else if (paste_replace(&global_buffers, *buffer, pdata, psize) != 0) {
		/* No context so can't use server_client_msg_error. */
		evbuffer_add_printf(c->stderr_data, "no buffer %d\n", *buffer);
		server_push_stderr(c);
	}

	free(data);
}
