/* $Id$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * List panes on given window.
 */

int	cmd_list_panes_exec(struct cmd *, struct cmd_ctx *);

void	cmd_list_panes_server(struct cmd *, struct cmd_ctx *);
void	cmd_list_panes_session(
	    struct cmd *, struct session *, struct cmd_ctx *, int);
void	cmd_list_panes_window(struct cmd *,
	    struct session *, struct winlink *, struct cmd_ctx *, int);

const struct cmd_entry cmd_list_panes_entry = {
	"list-panes", "lsp",
	"asF:t:", 0, 0,
	"[-as] [-F format] [-t target]",
	0,
	NULL,
	NULL,
	cmd_list_panes_exec
};

int
cmd_list_panes_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;
	struct winlink	*wl;

	if (args_has(args, 'a'))
		cmd_list_panes_server(self, ctx);
	else if (args_has(args, 's')) {
		s = cmd_find_session(ctx, args_get(args, 't'), 0);
		if (s == NULL)
			return (-1);
		cmd_list_panes_session(self, s, ctx, 1);
	} else {
		wl = cmd_find_window(ctx, args_get(args, 't'), &s);
		if (wl == NULL)
			return (-1);
		cmd_list_panes_window(self, s, wl, ctx, 0);
	}

	return (0);
}

void
cmd_list_panes_server(struct cmd *self, struct cmd_ctx *ctx)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions)
		cmd_list_panes_session(self, s, ctx, 2);
}

void
cmd_list_panes_session(
    struct cmd *self, struct session *s, struct cmd_ctx *ctx, int type)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, &s->windows)
		cmd_list_panes_window(self, s, wl, ctx, type);
}

void
cmd_list_panes_window(struct cmd *self,
    struct session *s, struct winlink *wl, struct cmd_ctx *ctx, int type)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	u_int			 n;
	struct format_tree	*ft;
	const char		*template;
	char			*line;

	template = args_get(args, 'F');
	if (template == NULL) {
		switch (type) {
		case 0:
			template = "#{line}: "
			    "[#{pane_width}x#{pane_height}] [history "
			    "#{history_size}/#{history_limit}, "
			    "#{history_bytes} bytes] #{pane_id}"
			    "#{?pane_active, (active),}#{?pane_dead, (dead),}";
			break;
		case 1:
			template = "#{window_index}.#{line}: "
			    "[#{pane_width}x#{pane_height}] [history "
			    "#{history_size}/#{history_limit}, "
			    "#{history_bytes} bytes] #{pane_id}"
			    "#{?pane_active, (active),}#{?pane_dead, (dead),}";
			break;
		case 2:
			template = "#{session_name}:#{window_index}.#{line}: "
			    "[#{pane_width}x#{pane_height}] [history "
			    "#{history_size}/#{history_limit}, "
			    "#{history_bytes} bytes] #{pane_id}"
			    "#{?pane_active, (active),}#{?pane_dead, (dead),}";
			break;
		}
	}

	n = 0;
	TAILQ_FOREACH(wp, &wl->window->panes, entry) {
		ft = format_create();
		format_add(ft, "line", "%u", n);
		format_session(ft, s);
		format_winlink(ft, s, wl);
		format_window_pane(ft, wp);

		line = format_expand(ft, template);
		ctx->print(ctx, "%s", line);
		xfree(line);

		format_free(ft);
		n++;
	}
}
