/* $Id: cmd-server-info.c,v 1.2 2009-01-10 01:41:02 nicm Exp $ */

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

#include <stdlib.h>
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
	struct client			*cmdclient;
	struct tty_code			*code;
	struct tty_term_code_entry	*ent;
	u_int		 		 i;
	char				 s[BUFSIZ];

	ctx->print(ctx, "tmux " BUILD 
	    ", pid %ld, started %s", (long) getpid(), ctime(&start_time));
	ctx->print(ctx, "socket path %s, debug level %d%s",
	    socket_path, debug_level, be_quiet ? ", quiet" : "");
	if (cfg_file != NULL)
		ctx->print(ctx, "configuration file %s", cfg_file);
	else
		ctx->print(ctx, "configuration file not specified");
	ctx->print(ctx, "%u clients, %u sessions", 
	    ARRAY_LENGTH(&clients), ARRAY_LENGTH(&sessions));
	ctx->print(ctx, "");

	cmdclient = ctx->cmdclient;
	ctx->cmdclient = NULL;

	ctx->print(ctx, "Clients:");
	cmd_list_clients_entry.exec(self, ctx);
	ctx->print(ctx, "");

 	ctx->print(ctx, "Sessions:");
	cmd_list_sessions_entry.exec(self, ctx);
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
				ctx->print(ctx, "  %2d,%s: [missing]",
				    ent->code, ent->name);
				break;
			case TTYCODE_STRING:
				strnvis(
				    s, code->value.string, sizeof s, VIS_OCTAL);
				s[(sizeof s) - 1] = '\0';

				ctx->print(ctx, "  %2d,%s: (string) %s",
				    ent->code, ent->name, s);
				break;
			case TTYCODE_NUMBER:
				ctx->print(ctx, "  %2d,%s: (number) %d",
				    ent->code, ent->name, code->value.number);
				break;
			case TTYCODE_FLAG:
				ctx->print(ctx, "  %2d,%s: (flag) %s",
				    ent->code, ent->name, 
				    code->value.flag ? "true" : "false");
				break;
			}
		}
	}
	ctx->print(ctx, "");	
	
	if (cmdclient != NULL)
		server_write_client(cmdclient, MSG_EXIT, NULL, 0);
}
