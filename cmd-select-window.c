/* $Id: cmd-select-window.c,v 1.25 2011-01-07 14:45:34 tcunha Exp $ */

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
 * Select window by index.
 */

void	cmd_select_window_key_binding(struct cmd *, int);
int	cmd_select_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_select_window_entry = {
	"select-window", "selectw",
	"t:", 0, 0,
	CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_select_window_key_binding,
	NULL,
	cmd_select_window_exec
};

void
cmd_select_window_key_binding(struct cmd *self, int key)
{
	char	tmp[16];

	xsnprintf(tmp, sizeof tmp, ":%d", key - '0');

	self->args = args_create(0);
	args_set(self->args, 't', tmp);
}

int
cmd_select_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct winlink	*wl;
	struct session	*s;

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), &s)) == NULL)
		return (-1);

	if (session_select(s, wl->idx) == 0)
		server_redraw_session(s);
	recalculate_sizes();

	return (0);
}
