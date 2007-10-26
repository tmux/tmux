/* $Id: cmd-list-sessions.c,v 1.7 2007-10-26 12:29:07 nicm Exp $ */

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

#include <getopt.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * List all sessions.
 */

void	cmd_list_sessions_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_list_sessions_entry = {
	"list-sessions", "ls", "",
	CMD_NOSESSION,
	NULL,
	cmd_list_sessions_exec,
	NULL,
	NULL,
	NULL
};

void
cmd_list_sessions_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	struct client	*c = ctx->client;
	struct session	*s = ctx->session;
	struct winlink	*wl;
	char		*tim;
	u_int		 i, n;

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;

		n = 0;
		RB_FOREACH(wl, winlinks, &s->windows)
		    	n++;
		tim = ctime(&s->tim);
		*strchr(tim, '\n') = '\0';

		ctx->print(ctx, "%s: %u windows (created %s) [%ux%u]", s->name,
		    n, tim, s->sx, s->sy);
	}

	if (!(ctx->flags & CMD_KEY))
		server_write_client(c, MSG_EXIT, NULL, 0);
}
