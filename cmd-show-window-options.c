/* $Id: cmd-show-window-options.c,v 1.16 2011-01-07 14:45:34 tcunha Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Show window options. This is an alias for show-options -w.
 */

int	cmd_show_window_options_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_show_window_options_entry = {
	"show-window-options", "showw",
	"gt:", 0, 0,
	"[-g] " CMD_TARGET_WINDOW_USAGE,
	0,
	NULL,
	NULL,
	cmd_show_window_options_exec
};

int
cmd_show_window_options_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;

	args_set(args, 'w', NULL);
	return (cmd_show_options_entry.exec(self, ctx));
}
