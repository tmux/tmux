/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
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

#include <time.h>

#include "tmux.h"

/*
 * Displays a message in the status line.
 */

int	cmd_display_message_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_display_message_entry = {
	"display-message", "display",
	CMD_TARGET_CLIENT_USAGE " [message]",
	CMD_ARG01, 0,
	cmd_target_init,
	cmd_target_parse,
	cmd_display_message_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

int
cmd_display_message_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct client		*c;
	const char		*template;
	char			*msg;

	if ((c = cmd_find_client(ctx, data->target)) == NULL)
		return (-1);

	if (data->arg == NULL)
		template = "[#S] #I:#W, current pane #P - (%H:%M %d-%b-%y)";
	else
		template = data->arg;

	msg = status_replace(c->session, template, time(NULL));
	status_message_set(c, "%s", msg);
	xfree(msg);

	return (0);
}
