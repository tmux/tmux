/* $Id$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
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

#include <time.h>

#include "tmux.h"

/*
 * Displays a message in the status line.
 */

int	cmd_display_message_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_display_message_entry = {
	"display-message", "display",
	"c:pt:F:", 0, 1,
	"[-p] [-c target-client] [-t target-pane] [-F format] [message]",
	0,
	NULL,
	NULL,
	cmd_display_message_exec
};

int
cmd_display_message_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct client		*c;
	struct session		*s;
	struct winlink		*wl;
	struct window_pane	*wp;
	const char		*template;
	char			*msg;
	struct format_tree	*ft;
	char			 out[BUFSIZ];
	time_t			 t;
	size_t			 len;

	if ((c = cmd_find_client(ctx, args_get(args, 'c'))) == NULL)
		return (-1);

	if (args_has(args, 't')) {
		wl = cmd_find_pane(ctx, args_get(args, 't'), &s, &wp);
		if (wl == NULL)
			return (-1);
	} else {
		wl = cmd_find_pane(ctx, NULL, &s, &wp);
		if (wl == NULL)
			return (-1);
	}

	if (args_has(args, 'F') && args->argc != 0) {
		ctx->error(ctx, "only one of -F or argument must be given");
		return (-1);
	}

	template = args_get(args, 'F');
	if (args->argc != 0)
		template = args->argv[0];
	if (template == NULL)
		template = "[#S] #I:#W, current pane #P - (%H:%M %d-%b-%y)";

	ft = format_create();
	format_client(ft, c);
	format_session(ft, s);
	format_winlink(ft, s, wl);
	format_window_pane(ft, wp);

	t = time(NULL);
	len = strftime(out, sizeof out, template, localtime(&t));
	out[len] = '\0';

	msg = format_expand(ft, out);
	if (args_has(self->args, 'p'))
		ctx->print(ctx, "%s", msg);
	else
		status_message_set(c, "%s", msg);

	xfree(msg);
	format_free(ft);
	return (0);
}
