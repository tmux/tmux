/* $Id: client-cmd.c,v 1.4 2007-09-27 09:15:58 nicm Exp $ */

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

#include "tmux.h"

int	client_cmd_prefix = META;

int	client_cmd_fn_select(int, struct client_ctx *, char **);
int	client_cmd_fn_detach(int, struct client_ctx *, char **);
int	client_cmd_fn_msg(int, struct client_ctx *, char **);

struct cmd {
	int	key;
	int	(*fn)(int, struct client_ctx *, char **);
	int	arg;
};

struct cmd client_cmd_table[] = {
	{ '0', client_cmd_fn_select, 0 },
	{ '1', client_cmd_fn_select, 1 },
	{ '2', client_cmd_fn_select, 2 },
	{ '3', client_cmd_fn_select, 3 },
	{ '4', client_cmd_fn_select, 4 },
	{ '5', client_cmd_fn_select, 5 },
	{ '6', client_cmd_fn_select, 6 },
	{ '7', client_cmd_fn_select, 7 },
	{ '8', client_cmd_fn_select, 8 },
	{ '9', client_cmd_fn_select, 9 },
	{ 'C', client_cmd_fn_msg, MSG_CREATE },
	{ 'c', client_cmd_fn_msg, MSG_CREATE },
	{ 'D', client_cmd_fn_detach, 0 },
	{ 'd', client_cmd_fn_detach, 0 },
	{ 'N', client_cmd_fn_msg, MSG_NEXT },
	{ 'n', client_cmd_fn_msg, MSG_NEXT },
	{ 'P', client_cmd_fn_msg, MSG_PREVIOUS },
	{ 'p', client_cmd_fn_msg, MSG_PREVIOUS },
	{ 'R', client_cmd_fn_msg, MSG_REFRESH },
	{ 'r', client_cmd_fn_msg, MSG_REFRESH },
	{ 'T', client_cmd_fn_msg, MSG_RENAME },
	{ 't', client_cmd_fn_msg, MSG_RENAME },
	{ 'L', client_cmd_fn_msg, MSG_LAST },
	{ 'l', client_cmd_fn_msg, MSG_LAST },
	{ 'W', client_cmd_fn_msg, MSG_WINDOWLIST },
	{ 'w', client_cmd_fn_msg, MSG_WINDOWLIST },
};
#define NCLIENTCMD (sizeof client_cmd_table / sizeof client_cmd_table[0])

int
client_cmd_dispatch(int key, struct client_ctx *cctx, char **error)
{
	struct cmd	*cmd;
	u_int		 i;

	for (i = 0; i < NCLIENTCMD; i++) {
		cmd = client_cmd_table + i;
		if (cmd->key == key)
			return (cmd->fn(cmd->arg, cctx, error));
	}
	return (0);
}

/* Handle generic command. */
int
client_cmd_fn_msg(int arg, struct client_ctx *cctx, unused char **error)
{
	client_write_server(cctx, arg, NULL, 0);

 	return (0);
}

/* Handle select command. */
int
client_cmd_fn_select(int arg, struct client_ctx *cctx, unused char **error)
{
	struct select_data	data;

	data.idx = arg;
	client_write_server(cctx, MSG_SELECT, &data, sizeof data);

	return (0);
}

/* Handle detach command. */
int
client_cmd_fn_detach(
    unused int arg, unused struct client_ctx *cctx, unused char **error)
{
	return (-1);
}
