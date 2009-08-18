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
 * Enter scroll mode.
 */

void	cmd_scroll_mode_init(struct cmd *, int);
int	cmd_scroll_mode_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_scroll_mode_entry = {
	"scroll-mode", NULL,
	"[-u] " CMD_TARGET_PANE_USAGE,
	0, CMD_CHFLAG('u'),
	cmd_scroll_mode_init,
	cmd_target_parse,
	cmd_scroll_mode_exec,
	cmd_target_free,
	cmd_target_print
};

void
cmd_scroll_mode_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	switch (key) {
	case KEYC_PPAGE:
		data->chflags |= CMD_CHFLAG('u');
		break;
	}
}

int
cmd_scroll_mode_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct window_pane	*wp;

	if (cmd_find_pane(ctx, data->target, NULL, &wp) == NULL)
		return (-1);

	window_pane_set_mode(wp, &window_scroll_mode);
	if (wp->mode == &window_scroll_mode && data->chflags & CMD_CHFLAG('u'))
		window_scroll_pageup(wp);

	return (0);
}
