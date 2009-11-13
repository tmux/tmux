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
 * Unlink a window, unless it would be destroyed by doing so (only one link).
 */

int	cmd_unlink_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_unlink_window_entry = {
	"unlink-window", "unlinkw",
	"[-k] " CMD_TARGET_WINDOW_USAGE,
	0, "k",
	cmd_target_init,
	cmd_target_parse,
	cmd_unlink_window_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_unlink_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	struct window		*w;
	struct session		*s, *s2;
	struct session_group	*sg;
	u_int			 references;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return (-1);
	w = wl->window;

	sg = session_group_find(s);
	if (sg != NULL) {
		references = 0;
		TAILQ_FOREACH(s2, &sg->sessions, gentry)
			references++;
	} else
		references = 1;

	if (!cmd_check_flag(data->chflags, 'k') && w->references == references) {
		ctx->error(ctx, "window is only linked to one session");
		return (-1);
	}
	
	server_unlink_window(s, wl);
	recalculate_sizes();

	return (0);
}
