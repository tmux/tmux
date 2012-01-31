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

void	cmd_list_windows_server(struct cmd *, struct cmd_ctx *);
void	cmd_list_windows_session(
	    struct cmd *, struct session *, struct cmd_ctx *, int);

const struct cmd_entry cmd_list_windows_entry = {
	"list-windows", "lsw",
	"aF:t:", 0, 0,
	"[-a] [-F format] " CMD_TARGET_SESSION_USAGE,
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
		cmd_list_windows_server(self, ctx);
	else {
		s = cmd_find_session(ctx, args_get(args, 't'), 0);
		if (s == NULL)
			return (-1);
		cmd_list_windows_session(self, s, ctx, 0);
	}

	return (0);
}

void
cmd_list_windows_server(struct cmd *self, struct cmd_ctx *ctx)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions)
		cmd_list_windows_session(self, s, ctx, 1);
}

void
cmd_list_windows_session(
    struct cmd *self, struct session *s, struct cmd_ctx *ctx, int type)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	u_int			n;
	struct format_tree	*ft;
	const char		*template;
	char			*line;

	template = args_get(args, 'F');
	if (template == NULL) {
		switch (type) {
		case 0:
			template = "#{window_index}: "
			    "#{window_name} "
			    "[#{window_width}x#{window_height}] "
			    "[layout #{window_layout}] #{window_id}"
			    "#{?window_active, (active),}";
			break;
		case 1:
			template = "#{session_name}:#{window_index}: "
			    "#{window_name} "
			    "[#{window_width}x#{window_height}] "
			    "[layout #{window_layout}] #{window_id}"
			    "#{?window_active, (active),}";
			break;
		}
	}

	n = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		ft = format_create();
		format_add(ft, "line", "%u", n);
		format_session(ft, s);
		format_winlink(ft, s, wl);

		line = format_expand(ft, template);
		ctx->print(ctx, "%s", line);
		xfree(line);

		format_free(ft);
		n++;
	}
}
