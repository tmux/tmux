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

#include <unistd.h>

#include "tmux.h"

/*
 * List windows on given session.
 */

int	cmd_list_windows_exec(struct cmd *, struct cmd_ctx *);

void	cmd_list_windows_server(struct cmd_ctx *);
void	cmd_list_windows_session(struct session *, struct cmd_ctx *, int);

const struct cmd_entry cmd_list_windows_entry = {
	"list-windows", "lsw",
	"at:", 0, 0,
	"[-a] " CMD_TARGET_SESSION_USAGE,
	0,
	NULL,
	NULL,
	cmd_list_windows_exec
};

int
cmd_list_windows_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;

	if (args_has(args, 'a'))
		cmd_list_windows_server(ctx);
	else {
		s = cmd_find_session(ctx, args_get(args, 't'), 0);
		if (s == NULL)
			return (-1);
		cmd_list_windows_session(s, ctx, 0);
	}

	return (0);
}

void
cmd_list_windows_server(struct cmd_ctx *ctx)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions)
		cmd_list_windows_session(s, ctx, 1);
}

void
cmd_list_windows_session(struct session *s, struct cmd_ctx *ctx, int type)
{
	struct winlink	*wl;
	char		*layout;

	RB_FOREACH(wl, winlinks, &s->windows) {
		layout = layout_dump(wl->window);
		if (type) {
			ctx->print(ctx, "%s:%d: %s [%ux%u] [layout %s]%s",
			    s->name, wl->idx, wl->window->name, wl->window->sx,
			    wl->window->sy, layout,
			    wl == s->curw ? " (active)" : "");
		} else {
			ctx->print(ctx, "%d: %s [%ux%u] [layout %s]%s",
			    wl->idx, wl->window->name, wl->window->sx,
			    wl->window->sy, layout,
			    wl == s->curw ? " (active)" : "");
		}
		xfree(layout);
	}
}
