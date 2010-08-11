/* $Id: cmd-copy-mode.c,v 1.28 2010-08-11 22:18:28 tcunha Exp $ */

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

void	cmd_copy_mode_init(struct cmd *, int);
int	cmd_copy_mode_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_copy_mode_entry = {
	"copy-mode", NULL,
	"[-u] " CMD_TARGET_PANE_USAGE,
	0, "u",
	cmd_copy_mode_init,
	cmd_target_parse,
	cmd_copy_mode_exec,
	cmd_target_free,
	cmd_target_print
};

void
cmd_copy_mode_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	switch (key) {
	case KEYC_PPAGE:
		cmd_set_flag(&data->chflags, 'u');
		break;
	}
}

int
cmd_copy_mode_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct window_pane	*wp;

	if (cmd_find_pane(ctx, data->target, NULL, &wp) == NULL)
		return (-1);

	if (window_pane_set_mode(wp, &window_copy_mode) != 0)
		return (0);
	window_copy_init_from_pane(wp);
	if (wp->mode == &window_copy_mode && cmd_check_flag(data->chflags, 'u'))
		window_copy_pageup(wp);

	return (0);
}
