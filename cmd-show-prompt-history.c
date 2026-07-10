/* $OpenBSD: cmd-show-prompt-history.c,v 1.5 2026/06/25 11:39:11 nicm Exp $ */

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
	.usage = "[-T prompt-type]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_prompt_history_exec
};

const struct cmd_entry cmd_clear_prompt_history_entry = {
	.name = "clear-prompt-history",
	.alias = "clearphist",

	.args = { "T:", 0, 0, NULL },
	.usage = "[-T prompt-type]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_prompt_history_exec
};

static enum cmd_retval
cmd_show_prompt_history_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	const char		*typestr = args_get(args, 'T');
	enum prompt_type	 type;
	u_int			 t, h;
	const char		*v;

	if (cmd_get_entry(self) == &cmd_clear_prompt_history_entry) {
		if (typestr == NULL) {
			for (t = 0; t < PROMPT_NTYPES; t++)
				prompt_history_clear(t);
		} else {
			type = prompt_type(typestr);
			if (type == PROMPT_TYPE_INVALID) {
				cmdq_error(item, "invalid type: %s", typestr);
				return (CMD_RETURN_ERROR);
			}
			prompt_history_clear(type);
		}
		return (CMD_RETURN_NORMAL);
	}

	if (typestr == NULL) {
		for (t = 0; t < PROMPT_NTYPES; t++) {
			typestr = prompt_type_string(t);
			cmdq_print(item, "History for %s:\n", typestr);
			for (h = 0; h < prompt_history_size(t); h++) {
				v = prompt_history_get(t, h);
				cmdq_print(item, "%d: %s", h + 1, v);
			}
			cmdq_print(item, "%s", "");
		}
	} else {
		type = prompt_type(typestr);
		if (type == PROMPT_TYPE_INVALID) {
			cmdq_error(item, "invalid type: %s", typestr);
			return (CMD_RETURN_ERROR);
		}
		cmdq_print(item, "History for %s:\n", prompt_type_string(type));
		for (h = 0; h < prompt_history_size(type); h++) {
			v = prompt_history_get(type, h);
			cmdq_print(item, "%d: %s", h + 1, v);
		}
		cmdq_print(item, "%s", "");
	}

	return (CMD_RETURN_NORMAL);
}
