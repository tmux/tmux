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

int	cmd_load_buffer_exec(struct cmd *, struct cmd_ctx *);
void	cmd_load_buffer_callback(struct client *, void *);

const struct cmd_entry cmd_load_buffer_entry = {
	"load-buffer", "loadb",
	"b:", 1, 1,
	CMD_BUFFER_USAGE " path",
	0,
	NULL,
	NULL,
	cmd_load_buffer_exec
};

int
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
	int		 ch, buffer;
	int		*buffer_ptr;

	if (!args_has(args, 'b'))
		buffer = -1;
	else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "buffer %s", cause);
			xfree(cause);
			return (-1);
		}
	}

	path = args->argv[0];
	if (strcmp(path, "-") == 0) {
		if (c == NULL) {
			ctx->error(ctx, "%s: can't read from stdin", path);
			return (-1);
		}
		if (c->flags & CLIENT_TERMINAL) {
			ctx->error(ctx, "%s: stdin is a tty", path);
			return (-1);
		}
		if (c->stdin_fd == -1) {
			ctx->error(ctx, "%s: can't read from stdin", path);
			return (-1);
		}

		buffer_ptr = xmalloc(sizeof *buffer_ptr);
		*buffer_ptr = buffer;

		c->stdin_data = buffer_ptr;
		c->stdin_callback = cmd_load_buffer_callback;

		c->references++;
		bufferevent_enable(c->stdin_event, EV_READ);
		return (1);
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
		return (-1);
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
		return (0);
	}
	if (paste_replace(&global_buffers, buffer, pdata, psize) != 0) {
		ctx->error(ctx, "no buffer %d", buffer);
		xfree(pdata);
		return (-1);
	}

	return (0);

error:
	if (pdata != NULL)
		xfree(pdata);
	if (f != NULL)
		fclose(f);
	return (-1);
}

void
cmd_load_buffer_callback(struct client *c, void *data)
{
	int	*buffer = data;
	char	*pdata;
	size_t	 psize;
	u_int	 limit;

	/*
	 * Event callback has already checked client is not dead and reduced
	 * its reference count. But tell it to exit.
	 */
	c->flags |= CLIENT_EXIT;

	psize = EVBUFFER_LENGTH(c->stdin_event->input);
	if (psize == 0 || (pdata = malloc(psize + 1)) == NULL) {
		xfree(data);
		return;
	}
	bufferevent_read(c->stdin_event, pdata, psize);
	pdata[psize] = '\0';

	limit = options_get_number(&global_options, "buffer-limit");
	if (*buffer == -1)
		paste_add(&global_buffers, pdata, psize, limit);
	else if (paste_replace(&global_buffers, *buffer, pdata, psize) != 0) {
		/* No context so can't use server_client_msg_error. */
		evbuffer_add_printf(
		    c->stderr_event->output, "no buffer %d\n", *buffer);
		bufferevent_enable(c->stderr_event, EV_WRITE);
	}

	xfree(data);
}
