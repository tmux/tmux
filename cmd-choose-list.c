/* $Id$ */

/*
 * Copyright (c) 2012 Thomas Adam <thomas@xteddy.org>
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

#include <string.h>

#include "tmux.h"

#define CMD_CHOOSE_LIST_DEFAULT_TEMPLATE "run-shell '%%'"

/*
 * Enter choose mode to choose a custom list.
 */

enum cmd_retval cmd_choose_list_exec(struct cmd *, struct cmd_ctx *);

void cmd_choose_list_callback(struct window_choose_data *);
void cmd_choose_list_free(struct window_choose_data *);

const struct cmd_entry cmd_choose_list_entry = {
	"choose-list", NULL,
	"l:t:", 0, 1,
	"[-l items] " CMD_TARGET_WINDOW_USAGE "[template]",
	0,
	NULL,
	NULL,
	cmd_choose_list_exec
};

enum cmd_retval
cmd_choose_list_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct winlink			*wl;
	const char			*list1;
	char				*template, *item, *copy, *list;
	u_int				 idx;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (CMD_RETURN_ERROR);
	}

	if ((list1 = args_get(args, 'l')) == NULL)
		return (CMD_RETURN_ERROR);

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (CMD_RETURN_ERROR);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (CMD_RETURN_NORMAL);

	if (args->argc != 0)
		template = xstrdup(args->argv[0]);
	else
		template = xstrdup(CMD_CHOOSE_LIST_DEFAULT_TEMPLATE);

	copy = list = xstrdup(list1);
	idx = 0;
	while ((item = strsep(&list, ",")) != NULL)
	{
		if (*item == '\0') /* no empty entries */
			continue;
		window_choose_add_item(wl->window->active, ctx, wl, item,
		    template, idx);
		idx++;
	}
	free(copy);

	if (idx == 0) {
		free(template);
		window_pane_reset_mode(wl->window->active);
		return (CMD_RETURN_ERROR);
	}

	window_choose_ready(wl->window->active, 0, cmd_choose_list_callback,
	    cmd_choose_list_free);

	free(template);

	return (CMD_RETURN_NORMAL);
}

void
cmd_choose_list_callback(struct window_choose_data *cdata)
{
	if (cdata == NULL || (cdata->client->flags & CLIENT_DEAD))
		return;

	window_choose_ctx(cdata);
}

void
cmd_choose_list_free(struct window_choose_data *cdata)
{
	cdata->session->references--;
	cdata->client->references--;

	free(cdata->ft_template);
	free(cdata->command);
	format_free(cdata->ft);
	free(cdata);

}
