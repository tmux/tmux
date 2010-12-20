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
	CMD_BUFFER_SESSION_USAGE " path",
	CMD_ARG1, "",
	cmd_buffer_init,
	cmd_buffer_parse,
	cmd_load_buffer_exec,
	cmd_buffer_free,
	cmd_buffer_print
};

struct cmd_load_buffer_cdata {
	struct session	*session;
	int		 buffer;
};

int
cmd_load_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_buffer_data		*data = self->data;
	struct cmd_load_buffer_cdata	*cdata;
	struct session			*s;
	struct client			*c = ctx->cmdclient;
	FILE				*f;
	char		      		*pdata, *new_pdata;
	size_t				 psize;
	u_int				 limit;
	int				 ch;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

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

		cdata = xmalloc(sizeof *cdata);
		cdata->session = s;
		cdata->session->references++;
		cdata->buffer = data->buffer;
		c->stdin_data = cdata;
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
	f = NULL;

	limit = options_get_number(&s->options, "buffer-limit");
	if (data->buffer == -1) {
		paste_add(&s->buffers, pdata, psize, limit);
		return (0);
	}
	if (paste_replace(&s->buffers, data->buffer, pdata, psize) != 0) {
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
	struct cmd_load_buffer_cdata	*cdata = data;
	struct session			*s = cdata->session;
	char				*pdata;
	size_t				 psize;
	u_int				 limit;

	/*
	 * Event callback has already checked client is not dead and reduced
	 * its reference count. But tell it to exit.
	 */
	c->flags |= CLIENT_EXIT;

	/* Does the target session still exist? */
	if (!session_alive(s))
		goto out;

	psize = EVBUFFER_LENGTH(c->stdin_event->input);
	if (psize == 0)
		goto out;

	pdata = malloc(psize + 1);
	if (pdata == NULL)
		goto out;
	bufferevent_read(c->stdin_event, pdata, psize);
	pdata[psize] = '\0';

	limit = options_get_number(&s->options, "buffer-limit");
	if (cdata->buffer == -1) {
		paste_add(&s->buffers, pdata, psize, limit);
		goto out;
	}
	if (paste_replace(&s->buffers, cdata->buffer, pdata, psize) != 0) {
		/* No context so can't use server_client_msg_error. */
		evbuffer_add_printf(
		    c->stderr_event->output, "no buffer %d\n", cdata->buffer);
		bufferevent_enable(c->stderr_event, EV_WRITE);
		goto out;
	}

out:
	cdata->session->references--;
	xfree(cdata);
}
