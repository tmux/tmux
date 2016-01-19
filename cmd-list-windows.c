/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <unistd.h>

#include "tmux.h"

/*
 * List windows on given session.
 */

#define LIST_WINDOWS_TEMPLATE					\
	"#{window_index}: #{window_name}#{window_flags} "	\
	"(#{window_panes} panes) "				\
	"[#{window_width}x#{window_height}] "			\
	"[layout #{window_layout}] #{window_id}"		\
	"#{?window_active, (active),}";
#define LIST_WINDOWS_WITH_SESSION_TEMPLATE			\
	"#{session_name}:"					\
	"#{window_index}: #{window_name}#{window_flags} "	\
	"(#{window_panes} panes) "				\
	"[#{window_width}x#{window_height}] "

enum cmd_retval	 cmd_list_windows_exec(struct cmd *, struct cmd_q *);

void	cmd_list_windows_server(struct cmd *, struct cmd_q *);
void	cmd_list_windows_session(struct cmd *, struct session *,
	    struct cmd_q *, int);

const struct cmd_entry cmd_list_windows_entry = {
	.name = "list-windows",
	.alias = "lsw",

	.args = { "F:at:", 0, 0 },
	.usage = "[-a] [-F format] " CMD_TARGET_SESSION_USAGE,

	.tflag = CMD_SESSION,

	.flags = 0,
	.exec = cmd_list_windows_exec
};

enum cmd_retval
cmd_list_windows_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;

	if (args_has(args, 'a'))
		cmd_list_windows_server(self, cmdq);
	else
		cmd_list_windows_session(self, cmdq->state.tflag.s, cmdq, 0);

	return (CMD_RETURN_NORMAL);
}

void
cmd_list_windows_server(struct cmd *self, struct cmd_q *cmdq)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions)
		cmd_list_windows_session(self, s, cmdq, 1);
}

void
cmd_list_windows_session(struct cmd *self, struct session *s,
    struct cmd_q *cmdq, int type)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	u_int			n;
	struct format_tree	*ft;
	const char		*template;
	char			*line;

	template = args_get(args, 'F');
	if (template == NULL) {
		switch (type) {
		case 0:
			template = LIST_WINDOWS_TEMPLATE;
			break;
		case 1:
			template = LIST_WINDOWS_WITH_SESSION_TEMPLATE;
			break;
		}
	}

	n = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		ft = format_create(cmdq, 0);
		format_add(ft, "line", "%u", n);
		format_defaults(ft, NULL, s, wl, NULL);

		line = format_expand(ft, template);
		cmdq_print(cmdq, "%s", line);
		free(line);

		format_free(ft);
		n++;
	}
}
