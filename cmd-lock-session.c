/* $OpenBSD$ */

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

#include "tmux.h"

/*
 * Lock all clients attached to a session.
 */

int	cmd_lock_session_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_lock_session_entry = {
	"lock-session", "locks",
	CMD_TARGET_SESSION_USAGE,
	0, 0,
	cmd_target_init,
	cmd_target_parse,
	cmd_lock_session_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_lock_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

	server_lock_session(s);
	recalculate_sizes();

	return (0);
}
