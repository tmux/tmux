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

#include <stdlib.h>
#include <string.h>
#include <vis.h>

#include "tmux.h"

/*
 * Paste paste buffer if present.
 */

enum cmd_retval	 cmd_paste_buffer_exec(struct cmd *, struct cmd_q *);

void	cmd_paste_buffer_filter(struct window_pane *,
	    const char *, size_t, const char *, int);

const struct cmd_entry cmd_paste_buffer_entry = {
	"paste-buffer", "pasteb",
	"db:prs:t:", 0, 0,
	"[-dpr] [-s separator] " CMD_BUFFER_USAGE " " CMD_TARGET_PANE_USAGE,
	0,
	cmd_paste_buffer_exec
};

enum cmd_retval
cmd_paste_buffer_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	struct session		*s;
	struct paste_buffer	*pb;
	const char		*sepstr, *bufname;

	if (cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp) == NULL)
		return (CMD_RETURN_ERROR);

	bufname = NULL;
	if (args_has(args, 'b'))
		bufname = args_get(args, 'b');

	if (bufname == NULL)
		pb = paste_get_top(NULL);
	else {
		pb = paste_get_name(bufname);
		if (pb == NULL) {
			cmdq_error(cmdq, "no buffer %s", bufname);
			return (CMD_RETURN_ERROR);
		}
	}

	if (pb != NULL) {
		sepstr = args_get(args, 's');
		if (sepstr == NULL) {
			if (args_has(args, 'r'))
				sepstr = "\n";
			else
				sepstr = "\r";
		}
		paste_send_pane(pb, wp, sepstr, args_has(args, 'p'));
	}

	/* Delete the buffer if -d. */
	if (args_has(args, 'd')) {
		if (bufname == NULL)
			paste_free_top();
		else
			paste_free_name(bufname);
	}

	return (CMD_RETURN_NORMAL);
}
