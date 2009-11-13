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

void	cmd_previous_window_init(struct cmd *, int);
int	cmd_previous_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_previous_window_entry = {
	"previous-window", "prev",
	"[-a] " CMD_TARGET_SESSION_USAGE,
	0, "a",
	cmd_previous_window_init,
	cmd_target_parse,
	cmd_previous_window_exec,
	cmd_target_free,
	cmd_target_print
};

void
cmd_previous_window_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	if (key == ('p' | KEYC_ESCAPE))
		cmd_set_flag(&data->chflags, 'a');
}

int
cmd_previous_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;
	int			 activity;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

	activity = 0;
	if (cmd_check_flag(data->chflags, 'a'))
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
