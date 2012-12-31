/* $Id$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * Create a new window.
 */

enum cmd_retval	cmd_new_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_new_window_entry = {
	"new-window", "neww",
	"ac:dF:kn:Pt:", 0, 1,
	"[-adkP] [-c start-directory] [-F format] [-n window-name] "
	CMD_TARGET_WINDOW_USAGE " [command]",
	0,
	NULL,
	NULL,
	cmd_new_window_exec
};

enum cmd_retval
cmd_new_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct session		*s;
	struct winlink		*wl;
	struct client		*c;
	const char		*cmd, *cwd;
	const char		*template;
	char			*cause;
	int			 idx, last, detached;
	struct format_tree	*ft;
	char			*cp;

	if (args_has(args, 'a')) {
		wl = cmd_find_window(ctx, args_get(args, 't'), &s);
		if (wl == NULL)
			return (CMD_RETURN_ERROR);
		idx = wl->idx + 1;

		/* Find the next free index. */
		for (last = idx; last < INT_MAX; last++) {
			if (winlink_find_by_index(&s->windows, last) == NULL)
				break;
		}
		if (last == INT_MAX) {
			ctx->error(ctx, "no free window indexes");
			return (CMD_RETURN_ERROR);
		}

		/* Move everything from last - 1 to idx up a bit. */
		for (; last > idx; last--) {
			wl = winlink_find_by_index(&s->windows, last - 1);
			server_link_window(s, wl, s, last, 0, 0, NULL);
			server_unlink_window(s, wl);
		}
	} else {
		if ((idx = cmd_find_index(ctx, args_get(args, 't'), &s)) == -2)
			return (CMD_RETURN_ERROR);
	}
	detached = args_has(args, 'd');

	wl = NULL;
	if (idx != -1)
		wl = winlink_find_by_index(&s->windows, idx);
	if (wl != NULL && args_has(args, 'k')) {
		/*
		 * Can't use session_detach as it will destroy session if this
		 * makes it empty.
		 */
		notify_window_unlinked(s, wl->window);
		wl->flags &= ~WINLINK_ALERTFLAGS;
		winlink_stack_remove(&s->lastw, wl);
		winlink_remove(&s->windows, wl);

		/* Force select/redraw if current. */
		if (wl == s->curw) {
			detached = 0;
			s->curw = NULL;
		}
	}

	if (args->argc == 0)
		cmd = options_get_string(&s->options, "default-command");
	else
		cmd = args->argv[0];
	cwd = cmd_get_default_path(ctx, args_get(args, 'c'));

	if (idx == -1)
		idx = -1 - options_get_number(&s->options, "base-index");
	wl = session_new(s, args_get(args, 'n'), cmd, cwd, idx, &cause);
	if (wl == NULL) {
		ctx->error(ctx, "create window failed: %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}
	if (!detached) {
		session_select(s, wl->idx);
		server_redraw_session_group(s);
	} else
		server_status_session_group(s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = NEW_WINDOW_TEMPLATE;

		ft = format_create();
		if ((c = cmd_find_client(ctx, NULL)) != NULL)
		    format_client(ft, c);
		format_session(ft, s);
		format_winlink(ft, s, wl);
		format_window_pane(ft, wl->window->active);

		cp = format_expand(ft, template);
		ctx->print(ctx, "%s", cp);
		free(cp);

		format_free(ft);
	}

	return (CMD_RETURN_NORMAL);
}
