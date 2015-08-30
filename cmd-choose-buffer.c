/* $OpenBSD$ */

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

#define CHOOSE_BUFFER_TEMPLATE						\
	"#{buffer_name}: #{buffer_size} bytes: #{buffer_sample}"

enum cmd_retval	 cmd_choose_buffer_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_choose_buffer_entry = {
	"choose-buffer", NULL,
	"F:t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [-F format] [template]",
	0,
	cmd_choose_buffer_exec
};

enum cmd_retval
cmd_choose_buffer_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	struct client			*c;
	struct window_choose_data	*cdata;
	struct winlink			*wl;
	struct paste_buffer		*pb;
	char				*action, *action_data;
	const char			*template;
	u_int				 idx;
	int				 utf8flag;

	if ((c = cmd_find_client(cmdq, NULL, 1)) == NULL) {
		cmdq_error(cmdq, "no client available");
		return (CMD_RETURN_ERROR);
	}

	if ((template = args_get(args, 'F')) == NULL)
		template = CHOOSE_BUFFER_TEMPLATE;

	if ((wl = cmd_find_window(cmdq, args_get(args, 't'), NULL)) == NULL)
		return (CMD_RETURN_ERROR);
	utf8flag = options_get_number(&wl->window->options, "utf8");

	if (paste_get_top(NULL) == NULL)
		return (CMD_RETURN_NORMAL);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (CMD_RETURN_NORMAL);

	if (args->argc != 0)
		action = xstrdup(args->argv[0]);
	else
		action = xstrdup("paste-buffer -b '%%'");

	idx = 0;
	pb = NULL;
	while ((pb = paste_walk(pb)) != NULL) {
		cdata = window_choose_data_create(TREE_OTHER, c, c->session);
		cdata->idx = idx;

		cdata->ft_template = xstrdup(template);
		format_defaults_paste_buffer(cdata->ft, pb, utf8flag);

		xasprintf(&action_data, "%s", paste_buffer_name(pb));
		cdata->command = cmd_template_replace(action, action_data, 1);
		free(action_data);

		window_choose_add(wl->window->active, cdata);
		idx++;
	}
	free(action);

	window_choose_ready(wl->window->active, 0, NULL);

	return (CMD_RETURN_NORMAL);
}
