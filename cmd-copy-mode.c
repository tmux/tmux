/* $Id$ */

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

void	cmd_copy_mode_key_binding(struct cmd *, int);
int	cmd_copy_mode_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_copy_mode_entry = {
	"copy-mode", NULL,
	"t:u", 0, 0,
	"[-u] " CMD_TARGET_PANE_USAGE,
	0,
	cmd_copy_mode_key_binding,
	NULL,
	cmd_copy_mode_exec
};

void
cmd_copy_mode_key_binding(struct cmd *self, int key)
{
	self->args = args_create(0);
	if (key == KEYC_PPAGE)
		args_set(self->args, 'u', NULL);
}

int
cmd_copy_mode_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct window_pane	*wp;

	if (cmd_find_pane(ctx, args_get(args, 't'), NULL, &wp) == NULL)
		return (-1);

	if (window_pane_set_mode(wp, &window_copy_mode) != 0)
		return (0);
	window_copy_init_from_pane(wp);
	if (wp->mode == &window_copy_mode && args_has(self->args, 'u'))
		window_copy_pageup(wp);

	return (0);
}
