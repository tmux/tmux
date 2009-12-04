/* $Id: cmd-set-window-option.c,v 1.43 2009-12-04 22:11:23 tcunha Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * Set a window option. This is just an alias for set-option -w.
 */

int	cmd_set_window_option_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_set_window_option_entry = {
	"set-window-option", "setw",
	"[-agu] " CMD_TARGET_WINDOW_USAGE " option [value]",
	CMD_ARG12, "agu",
	NULL,
	cmd_target_parse,
	cmd_set_window_option_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_set_window_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;

	cmd_set_flag(&data->chflags, 'w');
	return (cmd_set_option_entry.exec(self, ctx));
}
