/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Display panes on a client.
 */

static enum cmd_retval	 cmd_display_panes_exec(struct cmd *, struct cmd_q *);

static void		 cmd_display_panes_callback(struct client *,
			     struct window_pane *);

const struct cmd_entry cmd_display_panes_entry = {
	.name = "display-panes",
	.alias = "displayp",

	.args = { "t:", 0, 1 },
	.usage = CMD_TARGET_CLIENT_USAGE,

	.tflag = CMD_CLIENT,

	.flags = 0,
	.exec = cmd_display_panes_exec
};

static enum cmd_retval
cmd_display_panes_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct client	*c = cmdq->state.c;

	if (c->identify_callback != NULL)
		return (CMD_RETURN_NORMAL);

	c->identify_callback = cmd_display_panes_callback;
	if (args->argc != 0)
		c->identify_callback_data = xstrdup(args->argv[0]);
	else
		c->identify_callback_data = xstrdup("select-pane -t '%%'");

	server_set_identify(c);

	return (CMD_RETURN_NORMAL);
}

static void
cmd_display_panes_callback(struct client *c, struct window_pane *wp)
{
	struct cmd_list	*cmdlist;
	char		*template, *cmd, *expanded, *cause;

	template = c->identify_callback_data;
	if (wp != NULL) {
		xasprintf(&expanded, "%%%u", wp->id);
		cmd = cmd_template_replace(template, expanded, 1);

		if (cmd_string_parse(cmd, &cmdlist, NULL, 0, &cause) != 0) {
			if (cause != NULL) {
				*cause = toupper((u_char) *cause);
				status_message_set(c, "%s", cause);
				free(cause);
			}
		} else {
			cmdq_run(c->cmdq, cmdlist, NULL);
			cmd_list_free(cmdlist);
		}

		free(cmd);
		free(expanded);
	}

	free(c->identify_callback_data);
	c->identify_callback_data = NULL;
	c->identify_callback = NULL;
}
