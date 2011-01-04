/* $OpenBSD$ */

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

#include "tmux.h"

/*
 * Move to previous window.
 */

void	cmd_previous_window_key_binding(struct cmd *, int);
int	cmd_previous_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_previous_window_entry = {
	"previous-window", "prev",
	"at:", 0, 0,
	"[-a] " CMD_TARGET_SESSION_USAGE,
	0,
	cmd_previous_window_key_binding,
	NULL,
	cmd_previous_window_exec
};

void
cmd_previous_window_key_binding(struct cmd *self, int key)
{
	self->args = args_create(0);
	if (key == ('p' | KEYC_ESCAPE))
		args_set(self->args, 'a', NULL);
}

int
cmd_previous_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;
	int		 activity;

	if ((s = cmd_find_session(ctx, args_get(args, 't'))) == NULL)
		return (-1);

	activity = 0;
	if (args_has(self->args, 'a'))
		activity = 1;

	if (session_previous(s, activity) == 0)
		server_redraw_session(s);
	else {
		ctx->error(ctx, "no previous window");
		return (-1);
	}
	recalculate_sizes();

	return (0);
}
