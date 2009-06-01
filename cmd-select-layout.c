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
 * Switch window to selected layout.
 */

void	cmd_select_layout_init(struct cmd *, int);
int	cmd_select_layout_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_select_layout_entry = {
	"select-layout", "selectl",
	CMD_TARGET_WINDOW_USAGE " layout-name",
	CMD_ARG1,
	cmd_select_layout_init,
	cmd_target_parse,
	cmd_select_layout_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_select_layout_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	switch (key) {
	case KEYC_ADDESC('0'):
		data->arg = xstrdup("manual-vertical");
		break;
	case KEYC_ADDESC('1'):
		data->arg = xstrdup("even-horizontal");
		break;
	case KEYC_ADDESC('2'):
		data->arg = xstrdup("even-vertical");
		break;
	case KEYC_ADDESC('9'):
		data->arg = xstrdup("active-only");
		break;
	}
}

int
cmd_select_layout_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	int			 layout;

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);

	if ((layout = layout_lookup(data->arg)) == -1) {
		ctx->error(ctx, "unknown or ambiguous layout: %s", data->arg);
		return (-1);
	}

	if (layout_select(wl->window, layout) == 0)
		ctx->info(ctx, "layout now: %s", layout_name(wl->window));

	return (0);
}
