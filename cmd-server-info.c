/* $Id: cmd-server-info.c,v 1.8 2009-01-18 18:06:37 nicm Exp $ */

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
#include <sys/utsname.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "tmux.h"

/*
 * Show various information about server.
 */

void	cmd_server_info_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_server_info_entry = {
	"server-info", "info",
	"",
	0,
	NULL,
	NULL,
	cmd_server_info_exec,
	NULL,
	NULL,
	NULL,
	NULL
};

void
cmd_server_info_exec(unused struct cmd *self, struct cmd_ctx *ctx)
{
	struct tty_term			*term;
	struct client			*c;
	struct session			*s;
	struct tty_code			*code;
	struct tty_term_code_entry	*ent;
	struct utsname			 un;
	u_int		 		 i;
	char				 out[BUFSIZ];
	char				*tim;
	time_t		 		 t;

	tim = ctime(&start_time);
	*strchr(tim, '\n') = '\0';
	ctx->print(ctx, 
	    "tmux " BUILD ", pid %ld, started %s", (long) getpid(), tim);
	ctx->print(ctx, "socket path %s, debug level %d%s",
	    socket_path, debug_level, be_quiet ? ", quiet" : "");
        if (uname(&un) == 0) {
                ctx->print(ctx, "system is %s %s %s %s",
		    un.sysname, un.release, un.version, un.machine);
	}
	if (cfg_file != NULL)
		ctx->print(ctx, "configuration file is %s", cfg_file);
	else
		ctx->print(ctx, "configuration file not specified");
	ctx->print(ctx, "%u clients, %u sessions",
	    ARRAY_LENGTH(&clients), ARRAY_LENGTH(&sessions));
	ctx->print(ctx, "");

	ctx->print(ctx, "Clients:");
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		ctx->print(ctx, "%4d: %s (%d, %d): %s [%ux%u %s] "
		    "[flags=0x%x/0x%x]", i, c->tty.path, c->fd, c->tty.fd, 
		    c->session->name, c->sx, c->sy, c->tty.termname, c->flags,
		    c->tty.flags);
	}
	ctx->print(ctx, "");

 	ctx->print(ctx, "Sessions:");
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;

		t = s->tv.tv_sec;
		tim = ctime(&t);
		*strchr(tim, '\n') = '\0';

		ctx->print(ctx, "%4d: %s: %u windows (created %s) [%ux%u] "
		    "[flags=0x%x]", i, s->name, winlink_count(&s->windows),
		    tim, s->sx, s->sy, s->flags);
	}
	ctx->print(ctx, "");

  	ctx->print(ctx, "Terminals:");
	SLIST_FOREACH(term, &tty_terms, entry) {
		ctx->print(ctx, "%s [references=%u, flags=0x%x]:",
		    term->name, term->references, term->flags);
		for (i = 0; i < NTTYCODE; i++) {
			ent = &tty_term_codes[i];
			code = &term->codes[ent->code];
			switch (code->type) {
			case TTYCODE_NONE:
				ctx->print(ctx, "%4d: %s: [missing]",
				    ent->code, ent->name);
				break;
			case TTYCODE_STRING:
				strnvis(out, code->value.string,
				    sizeof out, VIS_OCTAL|VIS_WHITE);
				out[(sizeof out) - 1] = '\0';

				ctx->print(ctx, "%4d: %s: (string) %s",
				    ent->code, ent->name, out);
				break;
			case TTYCODE_NUMBER:
				ctx->print(ctx, "%4d: %s: (number) %d",
				    ent->code, ent->name, code->value.number);
				break;
			case TTYCODE_FLAG:
				ctx->print(ctx, "%4d: %s: (flag) %s",
				    ent->code, ent->name,
				    code->value.flag ? "true" : "false");
				break;
			}
		}
	}
	ctx->print(ctx, "");

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
