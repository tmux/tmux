/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * List windows on given session.
 */

#define LIST_WINDOWS_TEMPLATE					\
	"#{window_index}: #{window_name}#{window_raw_flags} "	\
	"(#{window_panes} panes) "				\
	"[#{window_width}x#{window_height}] "			\
	"[layout #{window_layout}] #{window_id}"		\
	"#{?window_active, (active),}";
#define LIST_WINDOWS_WITH_SESSION_TEMPLATE			\
	"#{session_name}:"					\
	"#{window_index}: #{window_name}#{window_raw_flags} "	\
	"(#{window_panes} panes) "				\
	"[#{window_width}x#{window_height}] "

static enum cmd_retval	cmd_list_windows_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_list_windows_entry = {
	.name = "list-windows",
	.alias = "lsw",

	.args = { "aF:f:O:rt:", 0, 0, NULL },
	.usage = "[-ar] [-F format] [-f filter] [-O order]"
		 CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_list_windows_exec
};

static enum cmd_retval
cmd_list_windows_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl, **l;
	struct session		*s;
	u_int			 i, n;
	struct format_tree	*ft;
	const char		*template, *filter;
	char			*line, *expanded;
	int			 flag;
	struct sort_criteria	 sort_crit;

	template = args_get(args, 'F');
	filter = args_get(args, 'f');

	sort_crit.order = sort_order_from_string(args_get(args, 'O'));
	if (sort_crit.order == SORT_END && args_has(args, 'O')) {
		cmdq_error(item, "invalid sort order");
		return (CMD_RETURN_ERROR);
	}
	sort_crit.reversed = args_has(args, 'r');

	if (args_has(args, 'a')) {
		l = sort_get_winlinks(&n, &sort_crit);
		if (template == NULL)
			template = LIST_WINDOWS_WITH_SESSION_TEMPLATE;
	} else {
		l = sort_get_winlinks_session(target->s, &n, &sort_crit);
		if (template == NULL)
			template = LIST_WINDOWS_TEMPLATE;
	}

	for (i = 0; i < n; i++) {
		wl = l[i];
		s = wl->session;
		ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
		format_add(ft, "line", "%u", n);
		format_defaults(ft, NULL, s, wl, NULL);

		if (filter != NULL) {
			expanded = format_expand(ft, filter);
			flag = format_true(expanded);
			free(expanded);
		} else
			flag = 1;
		if (flag) {
			line = format_expand(ft, template);
			cmdq_print(item, "%s", line);
			free(line);
		}

		format_free(ft);
	}

	return (CMD_RETURN_NORMAL);
}
