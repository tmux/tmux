/* $OpenBSD: cmd-copy-mode.c,v 1.2 2009/06/29 07:11:20 nicm Exp $ */

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
 * Enter copy mode.
 */

int	cmd_copy_mode_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_copy_mode_entry = {
	"copy-mode", NULL,
	CMD_TARGET_WINDOW_USAGE,
	CMD_UFLAG,
	cmd_target_init,
	cmd_target_parse,
	cmd_copy_mode_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	NULL
};

int
cmd_copy_mode_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	struct window_pane	*wp;

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);
	wp = wl->window->active;

	window_pane_set_mode(wp, &window_copy_mode);
	if (wp->mode == &window_copy_mode && data->flags & CMD_UFLAG)
		window_copy_pageup(wp);

	return (0);
}
