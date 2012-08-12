/* $Id$ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct cmd_list *
cmd_list_parse(int argc, char **argv, char **cause)
{
	struct cmd_list	*cmdlist;
	struct cmd	*cmd;
	int		 i, lastsplit;
	size_t		 arglen, new_argc;
	char	       **copy_argv, **new_argv;

	copy_argv = cmd_copy_argv(argc, argv);

	cmdlist = xmalloc(sizeof *cmdlist);
	cmdlist->references = 1;
	TAILQ_INIT(&cmdlist->list);

	lastsplit = 0;
	for (i = 0; i < argc; i++) {
		arglen = strlen(copy_argv[i]);
		if (arglen == 0 || copy_argv[i][arglen - 1] != ';')
			continue;
		copy_argv[i][arglen - 1] = '\0';

		if (arglen > 1 && copy_argv[i][arglen - 2] == '\\') {
			copy_argv[i][arglen - 2] = ';';
			continue;
		}

		new_argc = i - lastsplit;
		new_argv = copy_argv + lastsplit;
		if (arglen != 1)
			new_argc++;

		cmd = cmd_parse(new_argc, new_argv, cause);
		if (cmd == NULL)
			goto bad;
		TAILQ_INSERT_TAIL(&cmdlist->list, cmd, qentry);

		lastsplit = i + 1;
	}

	if (lastsplit != argc) {
		cmd = cmd_parse(argc - lastsplit, copy_argv + lastsplit, cause);
		if (cmd == NULL)
			goto bad;
		TAILQ_INSERT_TAIL(&cmdlist->list, cmd, qentry);
	}

	cmd_free_argv(argc, copy_argv);
	return (cmdlist);

bad:
	cmd_list_free(cmdlist);
	cmd_free_argv(argc, copy_argv);
	return (NULL);
}

enum cmd_retval
cmd_list_exec(struct cmd_list *cmdlist, struct cmd_ctx *ctx)
{
	struct client	*c = ctx->curclient;
	struct cmd	*cmd;
	enum cmd_retval	 retval;
	int		 guards, n;

	guards = 0;
	if (c != NULL && c->session != NULL)
		guards = c->flags & CLIENT_CONTROL;

	notify_disable();

	retval = 0;
	TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
		if (guards)
			ctx->print(ctx, "%%begin");
		n = cmd_exec(cmd, ctx);
		if (guards)
			ctx->print(ctx, "%%end");

		switch (n)
		{
		case CMD_RETURN_ERROR:
			return (CMD_RETURN_ERROR);
		case CMD_RETURN_ATTACH:
			/* Client is being attached (send MSG_READY). */
			retval = CMD_RETURN_ATTACH;

			/*
			 * Mangle the context to treat any following commands
			 * as if they were called from inside.
			 */
			if (ctx->curclient == NULL) {
				ctx->curclient = ctx->cmdclient;
				ctx->cmdclient = NULL;

				ctx->error = key_bindings_error;
				ctx->print = key_bindings_print;
				ctx->info = key_bindings_info;
			}
			break;
		case CMD_RETURN_YIELD:
			if (retval == CMD_RETURN_NORMAL)
				retval = CMD_RETURN_YIELD;
			break;
		case CMD_RETURN_NORMAL:
			break;
		}
	}

	notify_enable();
	return (retval);
}

void
cmd_list_free(struct cmd_list *cmdlist)
{
	struct cmd	*cmd;

	if (--cmdlist->references != 0)
		return;

	while (!TAILQ_EMPTY(&cmdlist->list)) {
		cmd = TAILQ_FIRST(&cmdlist->list);
		TAILQ_REMOVE(&cmdlist->list, cmd, qentry);
		cmd_free(cmd);
	}
	free(cmdlist);
}

size_t
cmd_list_print(struct cmd_list *cmdlist, char *buf, size_t len)
{
	struct cmd	*cmd;
	size_t		 off;

	off = 0;
	TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
		if (off >= len)
			break;
		off += cmd_print(cmd, buf + off, len - off);
		if (off >= len)
			break;
		if (TAILQ_NEXT(cmd, qentry) != NULL)
			off += xsnprintf(buf + off, len - off, " ; ");
	}
	return (off);
}
