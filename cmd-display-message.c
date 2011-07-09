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
	"c:pt:", 0, 1,
	"[-p] [-c target-client] [-t target-pane] [message]",
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

	if ((c = cmd_find_client(ctx, args_get(args, 'c'))) == NULL)
		return (-1);

	if (args_has(args, 't') != 0) {
		wl = cmd_find_pane(ctx, args_get(args, 't'), &s, &wp);
		if (wl == NULL)
			return (-1);
	} else {
		s = NULL;
		wl = NULL;
		wp = NULL;
	}

	if (args->argc == 0)
		template = "[#S] #I:#W, current pane #P - (%H:%M %d-%b-%y)";
	else
		template = args->argv[0];

	msg = status_replace(c, s, wl, wp, template, time(NULL), 0);
	if (args_has(self->args, 'p'))
		ctx->print(ctx, "%s", msg);
	else
		status_message_set(c, "%s", msg);
	xfree(msg);

	return (0);
}
