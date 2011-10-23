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

#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * List all clients.
 */

int	cmd_list_clients_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_list_clients_entry = {
	"list-clients", "lsc",
	"F:t:", 0, 0,
	"[-F format] " CMD_TARGET_SESSION_USAGE,
	CMD_READONLY,
	NULL,
	NULL,
	cmd_list_clients_exec
};

/* ARGSUSED */
int
cmd_list_clients_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args 		*args = self->args;
	struct client		*c;
	struct session		*s;
	struct format_tree	*ft;
	const char		*template;
	u_int			 i;
	char			*line;

	if (args_has(args, 't')) {
		s = cmd_find_session(ctx, args_get(args, 't'), 0);
		if (s == NULL)
			return (-1);
	} else
		s = NULL;

	template = args_get(args, 'F');
	if (template == NULL) {
		template = "#{client_tty}: #{session_name} "
		    "[#{client_width}x#{client_height} #{client_termname}]"
		    "#{?client_utf8, (utf8),}"
		    "#{?client_readonly, (ro),}";
	}

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		if (s != NULL && s != c->session)
			continue;

		ft = format_create();
		format_add(ft, "line", "%u", i);
		format_session(ft, c->session);
		format_client(ft, c);

		line = format_expand(ft, template);
		ctx->print(ctx, "%s", line);
		xfree(line);

		format_free(ft);
	}

	return (0);
}
