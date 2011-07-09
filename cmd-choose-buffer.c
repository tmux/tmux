/* $Id$ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <ctype.h>

#include "tmux.h"

/*
 * Enter choice mode to choose a buffer.
 */

int	cmd_choose_buffer_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_buffer_callback(void *, int);
void	cmd_choose_buffer_free(void *);

const struct cmd_entry cmd_choose_buffer_entry = {
	"choose-buffer", NULL,
	"t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [template]",
	0,
	NULL,
	NULL,
	cmd_choose_buffer_exec
};

struct cmd_choose_buffer_data {
	struct client   *client;
	char            *template;
};

int
cmd_choose_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct cmd_choose_buffer_data	*cdata;
	struct winlink			*wl;
	struct paste_buffer		*pb;
	u_int				 idx;
	char				*tmp;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (-1);

	if (paste_get_top(&global_buffers) == NULL)
		return (0);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (0);

	idx = 0;
	while ((pb = paste_walk_stack(&global_buffers, &idx)) != NULL) {
		tmp = paste_print(pb, 50);
		window_choose_add(wl->window->active, idx - 1,
		    "%u: %zu bytes: \"%s\"", idx - 1, pb->size, tmp);
		xfree(tmp);
	}

	cdata = xmalloc(sizeof *cdata);
	if (args->argc != 0)
		cdata->template = xstrdup(args->argv[0]);
	else
		cdata->template = xstrdup("paste-buffer -b '%%'");
	cdata->client = ctx->curclient;
	cdata->client->references++;

	window_choose_ready(wl->window->active,
	    0, cmd_choose_buffer_callback, cmd_choose_buffer_free, cdata);

	return (0);
}

void
cmd_choose_buffer_callback(void *data, int idx)
{
	struct cmd_choose_buffer_data	*cdata = data;
	struct cmd_list			*cmdlist;
	struct cmd_ctx			 ctx;
	char				*template, *cause, tmp[16];

	if (idx == -1)
		return;
	if (cdata->client->flags & CLIENT_DEAD)
		return;

	xsnprintf(tmp, sizeof tmp, "%u", idx);
	template = cmd_template_replace(cdata->template, tmp, 1);

	if (cmd_string_parse(template, &cmdlist, &cause) != 0) {
		if (cause != NULL) {
			*cause = toupper((u_char) *cause);
			status_message_set(cdata->client, "%s", cause);
			xfree(cause);
		}
		xfree(template);
		return;
	}
	xfree(template);

	ctx.msgdata = NULL;
	ctx.curclient = cdata->client;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_list_exec(cmdlist, &ctx);
	cmd_list_free(cmdlist);
}

void
cmd_choose_buffer_free(void *data)
{
	struct cmd_choose_buffer_data	*cdata = data;

	cdata->client->references--;
	xfree(cdata->template);
	xfree(cdata);
}
