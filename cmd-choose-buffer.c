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
#include <stdlib.h>

#include "tmux.h"

/*
 * Enter choice mode to choose a buffer.
 */

enum cmd_retval	 cmd_choose_buffer_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_buffer_callback(struct window_choose_data *);
void	cmd_choose_buffer_free(struct window_choose_data *);

const struct cmd_entry cmd_choose_buffer_entry = {
	"choose-buffer", NULL,
	"F:t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [-F format] [template]",
	0,
	NULL,
	NULL,
	cmd_choose_buffer_exec
};

enum cmd_retval
cmd_choose_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct window_choose_data	*cdata;
	struct winlink			*wl;
	struct paste_buffer		*pb;
	char				*action, *action_data;
	const char			*template;
	u_int				 idx;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (CMD_RETURN_ERROR);
	}

	if ((template = args_get(args, 'F')) == NULL)
		template = CHOOSE_BUFFER_TEMPLATE;

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (CMD_RETURN_ERROR);

	if (paste_get_top(&global_buffers) == NULL)
		return (CMD_RETURN_NORMAL);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (CMD_RETURN_NORMAL);

	if (args->argc != 0)
		action = xstrdup(args->argv[0]);
	else
		action = xstrdup("paste-buffer -b '%%'");

	idx = 0;
	while ((pb = paste_walk_stack(&global_buffers, &idx)) != NULL) {
		cdata = window_choose_data_create(ctx);
		cdata->idx = idx - 1;
		cdata->client->references++;

		cdata->ft_template = xstrdup(template);
		format_add(cdata->ft, "line", "%u", idx - 1);
		format_paste_buffer(cdata->ft, pb);

		xasprintf(&action_data, "%u", idx - 1);
		cdata->command = cmd_template_replace(action, action_data, 1);
		free(action_data);

		window_choose_add(wl->window->active, cdata);
	}
	free(action);

	window_choose_ready(wl->window->active,
	    0, cmd_choose_buffer_callback, cmd_choose_buffer_free);

	return (CMD_RETURN_NORMAL);
}

void
cmd_choose_buffer_callback(struct window_choose_data *cdata)
{
	if (cdata == NULL)
		return;
	if (cdata->client->flags & CLIENT_DEAD)
		return;

	window_choose_ctx(cdata);
}

void
cmd_choose_buffer_free(struct window_choose_data *data)
{
	struct window_choose_data	*cdata = data;

	if (cdata == NULL)
		return;

	cdata->client->references--;

	free(cdata->command);
	free(cdata->ft_template);
	free(cdata);
}
