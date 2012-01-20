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

int	cmd_new_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_new_window_entry = {
	"new-window", "neww",
	"adkn:Pt:", 0, 1,
	"[-adk] [-n window-name] [-t target-window] [command]",
	0,
	NULL,
	NULL,
	cmd_new_window_exec
};

int
cmd_new_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;
	struct winlink	*wl;
	const char     	*cmd, *cwd;
	char		*cause;
	int		 idx, last, detached;

	if (args_has(args, 'a')) {
		wl = cmd_find_window(ctx, args_get(args, 't'), &s);
		if (wl == NULL)
			return (-1);
		idx = wl->idx + 1;

		/* Find the next free index. */
		for (last = idx; last < INT_MAX; last++) {
			if (winlink_find_by_index(&s->windows, last) == NULL)
				break;
		}
		if (last == INT_MAX) {
			ctx->error(ctx, "no free window indexes");
			return (-1);
		}

		/* Move everything from last - 1 to idx up a bit. */
		for (; last > idx; last--) {
			wl = winlink_find_by_index(&s->windows, last - 1);
			server_link_window(s, wl, s, last, 0, 0, NULL);
			server_unlink_window(s, wl);
		}
	} else {
		if ((idx = cmd_find_index(ctx, args_get(args, 't'), &s)) == -2)
			return (-1);
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
	cwd = cmd_get_default_path(ctx);

	if (idx == -1)
		idx = -1 - options_get_number(&s->options, "base-index");
	wl = session_new(s, args_get(args, 'n'), cmd, cwd, idx, &cause);
	if (wl == NULL) {
		ctx->error(ctx, "create window failed: %s", cause);
		xfree(cause);
		return (-1);
	}
	if (!detached) {
		session_select(s, wl->idx);
		server_redraw_session_group(s);
	} else
		server_status_session_group(s);

	if (args_has(args, 'P'))
		ctx->print(ctx, "%s:%u", s->name, wl->idx);
	return (0);
}
