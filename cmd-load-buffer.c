/* $Id: cmd-load-buffer.c,v 1.19 2010-12-30 22:39:49 tcunha Exp $ */

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
 * Loads a session paste buffer from a file.
 */

int	cmd_load_buffer_exec(struct cmd *, struct cmd_ctx *);
void	cmd_load_buffer_callback(struct client *, void *);

const struct cmd_entry cmd_load_buffer_entry = {
	"load-buffer", "loadb",
	CMD_BUFFER_USAGE " path",
	CMD_ARG1, "",
	cmd_buffer_init,
	cmd_buffer_parse,
	cmd_load_buffer_exec,
	cmd_buffer_free,
	cmd_buffer_print
};

int
cmd_load_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_buffer_data	*data = self->data;
	struct client		*c = ctx->cmdclient;
	FILE			*f;
	char		      	*pdata, *new_pdata;
	size_t			 psize;
	u_int			 limit;
	int			 ch;

	if (strcmp(data->arg, "-") == 0) {
		if (c == NULL) {
			ctx->error(ctx, "%s: can't read from stdin", data->arg);
			return (-1);
		}
		if (c->flags & CLIENT_TERMINAL) {
			ctx->error(ctx, "%s: stdin is a tty", data->arg);
			return (-1);
		}
		if (c->stdin_fd == -1) {
			ctx->error(ctx, "%s: can't read from stdin", data->arg);
			return (-1);
		}

		c->stdin_data = &data->buffer;
		c->stdin_callback = cmd_load_buffer_callback;

		c->references++;
		bufferevent_enable(c->stdin_event, EV_READ);
		return (1);
	}

	if ((f = fopen(data->arg, "rb")) == NULL) {
		ctx->error(ctx, "%s: %s", data->arg, strerror(errno));
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
		ctx->error(ctx, "%s: read error", data->arg);
		goto error;
	}
	if (pdata != NULL)
		pdata[psize] = '\0';

	fclose(f);

	limit = options_get_number(&global_options, "buffer-limit");
	if (data->buffer == -1) {
		paste_add(&global_buffers, pdata, psize, limit);
		return (0);
	}
	if (paste_replace(&global_buffers, data->buffer, pdata, psize) != 0) {
		ctx->error(ctx, "no buffer %d", data->buffer);
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
	char	*pdata;
	size_t	 psize;
	u_int	 limit;
	int	*buffer = data;

	/*
	 * Event callback has already checked client is not dead and reduced
	 * its reference count. But tell it to exit.
	 */
	c->flags |= CLIENT_EXIT;

	psize = EVBUFFER_LENGTH(c->stdin_event->input);
	if (psize == 0)
		return;

	pdata = malloc(psize + 1);
	if (pdata == NULL)
		return;
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
}
