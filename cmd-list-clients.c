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
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * List all clients.
 */

#define LIST_CLIENTS_TEMPLATE					\
	"#{client_tty}: #{session_name} "			\
	"[#{client_width}x#{client_height} #{client_termname}]"	\
	"#{?client_utf8, (utf8),} #{?client_readonly, (ro),}"

enum cmd_retval	cmd_list_clients_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_list_clients_entry = {
	.name = "list-clients",
	.alias = "lsc",

	.args = { "F:t:", 0, 0 },
	.usage = "[-F format] " CMD_TARGET_SESSION_USAGE,

	.tflag = CMD_SESSION,

	.flags = CMD_READONLY,
	.exec = cmd_list_clients_exec
};

enum cmd_retval
cmd_list_clients_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args 		*args = self->args;
	struct client		*c;
	struct session		*s;
	struct format_tree	*ft;
	const char		*template;
	u_int			 idx;
	char			*line;

	if (args_has(args, 't'))
		s = cmdq->state.tflag.s;
	else
		s = NULL;

	if ((template = args_get(args, 'F')) == NULL)
		template = LIST_CLIENTS_TEMPLATE;

	idx = 0;
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL || (s != NULL && s != c->session))
			continue;

		ft = format_create(cmdq, 0);
		format_add(ft, "line", "%u", idx);
		format_defaults(ft, c, NULL, NULL, NULL);

		line = format_expand(ft, template);
		cmdq_print(cmdq, "%s", line);
		free(line);

		format_free(ft);

		idx++;
	}

	return (CMD_RETURN_NORMAL);
}
