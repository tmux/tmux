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

static enum cmd_retval	cmd_display_panes_exec(struct cmd *,
			    struct cmdq_item *);

static void		cmd_display_panes_callback(struct client *,
			    struct window_pane *);

const struct cmd_entry cmd_display_panes_entry = {
	.name = "display-panes",
	.alias = "displayp",

	.args = { "d:t:", 0, 1 },
	.usage = "[-d duration] " CMD_TARGET_CLIENT_USAGE,

	.flags = CMD_AFTERHOOK,
	.exec = cmd_display_panes_exec
};

static enum cmd_retval
cmd_display_panes_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = self->args;
	struct client	*c;
	struct session	*s;
	u_int		 delay;
	char		*cause;

	if ((c = cmd_find_client(item, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	if (c->identify_callback != NULL)
		return (CMD_RETURN_NORMAL);

	c->identify_callback = cmd_display_panes_callback;
	if (args->argc != 0)
		c->identify_callback_data = xstrdup(args->argv[0]);
	else
		c->identify_callback_data = xstrdup("select-pane -t '%%'");
	s = c->session;

	if (args_has(args, 'd')) {
		delay = args_strtonum(args, 'd', 0, UINT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "delay %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	} else
		delay = options_get_number(s->options, "display-panes-time");
	server_client_set_identify(c, delay);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_display_panes_error(struct cmdq_item *item, void *data)
{
	char	*error = data;

	cmdq_error(item, "%s", error);
	free(error);

	return (CMD_RETURN_NORMAL);
}

static void
cmd_display_panes_callback(struct client *c, struct window_pane *wp)
{
	struct cmd_list		*cmdlist;
	struct cmdq_item	*new_item;
	char			*template, *cmd, *expanded, *cause;

	template = c->identify_callback_data;
	if (wp == NULL)
		goto out;
	xasprintf(&expanded, "%%%u", wp->id);
	cmd = cmd_template_replace(template, expanded, 1);

	cmdlist = cmd_string_parse(cmd, NULL, 0, &cause);
	if (cmdlist == NULL) {
		if (cause != NULL) {
			new_item = cmdq_get_callback(cmd_display_panes_error,
			    cause);
		} else
			new_item = NULL;
	} else {
		new_item = cmdq_get_command(cmdlist, NULL, NULL, 0);
		cmd_list_free(cmdlist);
	}

	if (new_item != NULL)
		cmdq_append(c, new_item);

	free(cmd);
	free(expanded);

out:
	free(c->identify_callback_data);
	c->identify_callback_data = NULL;
	c->identify_callback = NULL;
}
