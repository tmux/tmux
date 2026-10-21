/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael Grant <mgrant@grant.org>
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

#include <limits.h>

#include "tmux.h"

static enum cmd_retval	cmd_resize_status_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_resize_status_entry = {
	.name = "resize-status",
	.description = "Resize a status line.",
	.alias = "resizes",

	.args = { "-t:", 0, 1, NULL },
	.usage = CMD_TARGET_SESSION_USAGE " [adjustment]",

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_resize_status_exec
};

/* Adjust the horizontal status height or begin a mouse resize. */
static enum cmd_retval
cmd_resize_status_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s;
	struct client		*c = cmdq_get_client(item);
	struct key_event	*event = cmdq_get_event(item);
	const char		*errstr;
	long long		 lines;
	int			 adjustment, limit = STATUS_LINES_LIMIT;

	if (args_count(args) == 0) {
		status_resize_start(c, &event->m);
		return (CMD_RETURN_NORMAL);
	}
	adjustment = strtonum(args_string(args, 0), INT_MIN, INT_MAX,
	    &errstr);
	if (errstr != NULL) {
		cmdq_error(item, "adjustment %s", errstr);
		return (CMD_RETURN_ERROR);
	}

	/* Leave at least one row for the attached client's window. */
	if (c != NULL && c->session == s && (c->tty.flags & TTY_STARTED)) {
		if (c->tty.sy < 2)
			return (CMD_RETURN_NORMAL);
		if (c->tty.sy <= STATUS_LINES_LIMIT)
			limit = c->tty.sy - 1;
	}
	lines = s->statuslines;
	lines += adjustment;
	if (lines < 1)
		lines = 1;
	else if (lines > limit)
		lines = limit;
	if (lines != s->statuslines) {
		options_set_number(s->options, "status", lines);
		options_push_changes("status");
	}
	return (CMD_RETURN_NORMAL);
}
