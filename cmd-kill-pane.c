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

#include <stdlib.h>

#include "tmux.h"

/*
 * Kill pane.
 */

static enum cmd_retval	cmd_kill_pane_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval	cmd_kill_pane_all(struct cmdq_item *, const char *);
static int		cmd_kill_pane_filter(struct cmdq_item *,
			    struct session *, struct winlink *,
			    struct window_pane *, const char *);

const struct cmd_entry cmd_kill_pane_entry = {
	.name = "kill-pane",
	.alias = "killp",

	.args = { "af:t:", 0, 0, NULL },
	.usage = "[-a] [-f filter] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_kill_pane_exec
};

static enum cmd_retval
cmd_kill_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window_pane	*wp = target->wp;
	const char		*filter = args_get(args, 'f');

	if (filter != NULL && !args_has(args, 'a')) {
		cmdq_error(item, "-f only valid with -a");
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'a'))
		return (cmd_kill_pane_all(item, filter));

	if (wp == NULL) {
		cmdq_error(item, "no active pane to kill");
		return (CMD_RETURN_ERROR);
	}
	server_kill_pane(wp);
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_kill_pane_all(struct cmdq_item *item, const char *filter)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s;
	struct winlink		*wl = target->wl;
	struct window_pane	*wp = target->wp;
	struct window_pane	*loopwp, *tmpwp;

	server_unzoom_window(wl->window);
	TAILQ_FOREACH_SAFE(loopwp, &wl->window->panes, entry, tmpwp) {
		if (loopwp == wp)
			continue;
		if (!cmd_kill_pane_filter(item, s, wl, loopwp, filter))
			continue;
		server_client_remove_pane(loopwp);
		layout_close_pane(loopwp);
		window_remove_pane(wl->window, loopwp);
	}
	server_redraw_window(wl->window);
	return (CMD_RETURN_NORMAL);
}

static int
cmd_kill_pane_filter(struct cmdq_item *item, struct session *s,
    struct winlink *wl, struct window_pane *wp, const char *filter)
{
	struct format_tree	*ft;
	char			*expanded;
	int			 flag;

	if (filter == NULL)
		return (1);

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, s, wl, wp);

	expanded = format_expand(ft, filter);
	flag = format_true(expanded);
	free(expanded);

	format_free(ft);
	return (flag);
}
