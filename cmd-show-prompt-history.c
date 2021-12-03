/* $OpenBSD$ */

/*
 * Copyright (c) 2021 Anindya Mukherjee <anindya49@hotmail.com>
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

#include "tmux.h"

#include <stdlib.h>

/*
 * Show or clear prompt history.
 */

static enum cmd_retval	cmd_show_prompt_history_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_show_prompt_history_entry = {
	.name = "show-prompt-history",
	.alias = "showphist",

	.args = { "T:", 0, 0, NULL },
	.usage = "[-T type]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_prompt_history_exec
};

const struct cmd_entry cmd_clear_prompt_history_entry = {
	.name = "clear-prompt-history",
	.alias = "clearphist",

	.args = { "T:", 0, 0, NULL },
	.usage = "[-T type]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_prompt_history_exec
};

static enum cmd_retval
cmd_show_prompt_history_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	const char		*typestr = args_get(args, 'T');
	enum prompt_type	 type;
	u_int			 tidx, hidx;

	if (cmd_get_entry(self) == &cmd_clear_prompt_history_entry) {
		if (typestr == NULL) {
			for (tidx = 0; tidx < PROMPT_NTYPES; tidx++) {
				free(status_prompt_hlist[tidx]);
				status_prompt_hlist[tidx] = NULL;
				status_prompt_hsize[tidx] = 0;
			}
		} else {
			type = status_prompt_type(typestr);
			if (type == PROMPT_TYPE_INVALID) {
				cmdq_error(item, "invalid type: %s", typestr);
				return (CMD_RETURN_ERROR);
			}
			free(status_prompt_hlist[type]);
			status_prompt_hlist[type] = NULL;
			status_prompt_hsize[type] = 0;
		}

		return (CMD_RETURN_NORMAL);
	}

	if (typestr == NULL) {
		for (tidx = 0; tidx < PROMPT_NTYPES; tidx++) {
			cmdq_print(item, "History for %s:\n",
			    status_prompt_type_string(tidx));
			for (hidx = 0; hidx < status_prompt_hsize[tidx];
			    hidx++) {
				cmdq_print(item, "%d: %s", hidx + 1,
				    status_prompt_hlist[tidx][hidx]);
			}
			cmdq_print(item, "%s", "");
		}
	} else {
		type = status_prompt_type(typestr);
		if (type == PROMPT_TYPE_INVALID) {
			cmdq_error(item, "invalid type: %s", typestr);
			return (CMD_RETURN_ERROR);
		}
		cmdq_print(item, "History for %s:\n",
		    status_prompt_type_string(type));
		for (hidx = 0; hidx < status_prompt_hsize[type]; hidx++) {
			cmdq_print(item, "%d: %s", hidx + 1,
			    status_prompt_hlist[type][hidx]);
		}
		cmdq_print(item, "%s", "");
	}

	return (CMD_RETURN_NORMAL);
}
