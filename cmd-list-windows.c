/* $OpenBSD: cmd-list-windows.c,v 1.50 2026/02/27 08:25:12 nicm Exp $ */

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
	int			 flag, human, all;
	struct sort_criteria	 sort_crit;
	struct cmd_output_table	*table = NULL;
	const char		*headers_all[] = {
		"SESSION", "INDEX", "NAME", "PANES", "SIZE", "STATE", "ID"
	};
	const char		*headers_one[] = {
		"INDEX", "NAME", "PANES", "SIZE", "STATE", "ID"
	};
	const char		*formats_all[] = {
		"#{session_name}", "#{window_index}", "#{window_name}",
		"#{window_panes}", "#{window_width}x#{window_height}",
		"#{?window_active,active,#{?window_bell_flag,bell,"
		"#{?window_activity_flag,activity,"
		"#{?window_silence_flag,silence,-}}}}", "#{window_id}"
	};
	const char		*formats_one[] = {
		"#{window_index}", "#{window_name}", "#{window_panes}",
		"#{window_width}x#{window_height}",
		"#{?window_active,active,#{?window_bell_flag,bell,"
		"#{?window_activity_flag,activity,"
		"#{?window_silence_flag,silence,-}}}}", "#{window_id}"
	};
	enum cmd_output_style	 styles_all[] = {
		CMD_OUTPUT_IDENTIFIER, CMD_OUTPUT_IDENTIFIER,
		CMD_OUTPUT_DEFAULT, CMD_OUTPUT_DEFAULT, CMD_OUTPUT_DIM,
		CMD_OUTPUT_DEFAULT, CMD_OUTPUT_DIM
	};
	enum cmd_output_style	 styles_one[] = {
		CMD_OUTPUT_IDENTIFIER, CMD_OUTPUT_DEFAULT, CMD_OUTPUT_DEFAULT,
		CMD_OUTPUT_DIM, CMD_OUTPUT_DEFAULT, CMD_OUTPUT_DIM
	};

	template = args_get(args, 'F');
	human = (template == NULL && cmd_output_is_human(item));
	filter = args_get(args, 'f');

	sort_crit.order = sort_order_from_string(args_get(args, 'O'));
	if (sort_crit.order == SORT_END && args_has(args, 'O')) {
		cmdq_error(item, "invalid sort order");
		return (CMD_RETURN_ERROR);
	}
	sort_crit.reversed = args_has(args, 'r');

	all = args_has(args, 'a');
	if (all) {
		/* Windows from multiple sessions need a session column. */
		l = sort_get_winlinks(&n, &sort_crit);
		if (template == NULL)
			template = LIST_WINDOWS_WITH_SESSION_TEMPLATE;
		if (human)
			table = cmd_output_table_create(item, "Windows", 7,
			    headers_all);
	} else {
		l = sort_get_winlinks_session(target->s, &n, &sort_crit);
		if (template == NULL)
			template = LIST_WINDOWS_TEMPLATE;
		if (human)
			table = cmd_output_table_create(item, "Windows", 6,
			    headers_one);
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
			if (human) {
				if (all) {
					styles_all[5] = (wl == s->curw) ?
					    CMD_OUTPUT_SUCCESS :
					    ((wl->flags & (WINLINK_BELL|
					    WINLINK_ACTIVITY|WINLINK_SILENCE)) ?
					    CMD_OUTPUT_WARNING : CMD_OUTPUT_DIM);
					cmd_output_table_add_formats(table, ft,
					    formats_all, styles_all);
				} else {
					styles_one[4] = (wl == s->curw) ?
					    CMD_OUTPUT_SUCCESS :
					    ((wl->flags & (WINLINK_BELL|
					    WINLINK_ACTIVITY|WINLINK_SILENCE)) ?
					    CMD_OUTPUT_WARNING : CMD_OUTPUT_DIM);
					cmd_output_table_add_formats(table, ft,
					    formats_one, styles_one);
				}
			} else {
				line = format_expand(ft, template);
				cmdq_print(item, "%s", line);
				free(line);
			}
		}

		format_free(ft);
	}
	if (human) {
		cmd_output_table_print(table);
		cmd_output_table_free(table);
	}

	return (CMD_RETURN_NORMAL);
}
