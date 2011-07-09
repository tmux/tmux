/* $Id$ */

/*
 * Copyright (c) 2008 Tiago Cunha <me@tiagocunha.org>
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
 * Sources a configuration file.
 */

int	cmd_source_file_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_source_file_entry = {
	"source-file", "source",
	"", 1, 1,
	"path",
	0,
	NULL,
	NULL,
	cmd_source_file_exec
};

int
cmd_source_file_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct causelist	 causes;
	char			*cause;
	struct window_pane	*wp;
	int			 retval;
	u_int			 i;

	ARRAY_INIT(&causes);

	retval = load_cfg(args->argv[0], ctx, &causes);
	if (ARRAY_EMPTY(&causes))
		return (retval);

	if (retval == 1 && !RB_EMPTY(&sessions) && ctx->cmdclient != NULL) {
		wp = RB_MIN(sessions, &sessions)->curw->window->active;
		window_pane_set_mode(wp, &window_copy_mode);
		window_copy_init_for_output(wp);
		for (i = 0; i < ARRAY_LENGTH(&causes); i++) {
			cause = ARRAY_ITEM(&causes, i);
			window_copy_add(wp, "%s", cause);
			xfree(cause);
		}
	} else {
		for (i = 0; i < ARRAY_LENGTH(&causes); i++) {
			cause = ARRAY_ITEM(&causes, i);
			ctx->print(ctx, "%s", cause);
			xfree(cause);
		}
	}
	ARRAY_FREE(&causes);

	return (retval);
}
