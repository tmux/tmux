/* $OpenBSD: cmd-kill-window.c,v 1.30 2026/06/09 12:57:40 nicm Exp $ */

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

#include "tmux.h"

/*
 * Destroy window.
 */

static enum cmd_retval	cmd_kill_window_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval	cmd_kill_window_all(struct cmdq_item *, const char *);
static int		cmd_kill_window_filter(struct cmdq_item *,
			    struct session *, struct winlink *, const char *);

const struct cmd_entry cmd_kill_window_entry = {
	.name = "kill-window",
	.alias = "killw",

	.args = { "af:t:", 0, 0, NULL },
	.usage = "[-a] [-f filter] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_kill_window_exec
};

const struct cmd_entry cmd_unlink_window_entry = {
	.name = "unlink-window",
	.alias = "unlinkw",

	.args = { "kt:", 0, 0, NULL },
	.usage = "[-k] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_kill_window_exec
};

static enum cmd_retval
cmd_kill_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct session		*s = target->s;
	const char		*filter = args_get(args, 'f');

	if (filter != NULL && !args_has(args, 'a')) {
		cmdq_error(item, "-f only valid with -a");
		return (CMD_RETURN_ERROR);
	}

	if (cmd_get_entry(self) == &cmd_unlink_window_entry) {
		if (!args_has(args, 'k') && !session_is_linked(s, w)) {
			cmdq_error(item, "window only linked to one session");
			return (CMD_RETURN_ERROR);
		}
		server_unlink_window(s, wl);
		recalculate_sizes();
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'a'))
		return (cmd_kill_window_all(item, filter));

	server_kill_window(wl->window, 1);
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_kill_window_all(struct cmdq_item *item, const char *filter)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s;
	struct winlink		*wl = target->wl;
	struct winlink	*loop;
	u_int		 found, kill_current;

	if (RB_PREV(winlinks, &s->windows, wl) == NULL &&
	    RB_NEXT(winlinks, &s->windows, wl) == NULL)
		return (CMD_RETURN_NORMAL);

	/* Kill all windows except the current one. */
	do {
		found = 0;
		RB_FOREACH(loop, winlinks, &s->windows) {
			if (loop->window != wl->window &&
			    cmd_kill_window_filter(item, s, loop, filter)) {
				server_kill_window(loop->window, 0);
				found++;
				break;
			}
		}
	} while (found != 0);

	/*
	 * If the current window appears in the session more than once, kill it
	 * as well if it matches the filter.
	 */
	found = kill_current = 0;
	RB_FOREACH(loop, winlinks, &s->windows) {
		if (loop->window == wl->window) {
			found++;
			if (cmd_kill_window_filter(item, s, loop, filter))
				kill_current = 1;
		}
	}
	if (kill_current && found > 1)
		server_kill_window(wl->window, 0);

	server_renumber_all();
	return (CMD_RETURN_NORMAL);
}

static int
cmd_kill_window_filter(struct cmdq_item *item, struct session *s,
    struct winlink *wl, const char *filter)
{
	struct format_tree	*ft;
	char			*expanded;
	int			 flag;

	if (filter == NULL)
		return (1);

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, s, wl, NULL);

	expanded = format_expand(ft, filter);
	flag = format_true(expanded);
	free(expanded);

	format_free(ft);
	return (flag);
}
