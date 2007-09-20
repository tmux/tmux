/* $Id: command.c,v 1.6 2007-09-20 18:51:34 nicm Exp $ */

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

int	cmd_prefix = META;

int	cmd_fn_select(struct buffer *, int);
int	cmd_fn_detach(struct buffer *, int);
int	cmd_fn_msg(struct buffer *, int);

struct cmd {
	int	key;
	int	(*fn)(struct buffer *, int);
	int	arg;
};

struct cmd cmd_table[] = {
	{ '0', cmd_fn_select, 0 },
	{ '1', cmd_fn_select, 1 },
	{ '2', cmd_fn_select, 2 },
	{ '3', cmd_fn_select, 3 },
	{ '4', cmd_fn_select, 4 },
	{ '5', cmd_fn_select, 5 },
	{ '6', cmd_fn_select, 6 },
	{ '7', cmd_fn_select, 7 },
	{ '8', cmd_fn_select, 8 },
	{ '9', cmd_fn_select, 9 },
	{ 'C', cmd_fn_msg, MSG_CREATE },
	{ 'c', cmd_fn_msg, MSG_CREATE },
	{ 'D', cmd_fn_detach, 0 },
	{ 'd', cmd_fn_detach, 0 },
	{ 'N', cmd_fn_msg, MSG_NEXT },
	{ 'n', cmd_fn_msg, MSG_NEXT },
	{ 'P', cmd_fn_msg, MSG_PREVIOUS },
	{ 'p', cmd_fn_msg, MSG_PREVIOUS },
	{ 'R', cmd_fn_msg, MSG_REFRESH },
	{ 'r', cmd_fn_msg, MSG_REFRESH },
	{ 'T', cmd_fn_msg, MSG_RENAME },
	{ 't', cmd_fn_msg, MSG_RENAME },
	{ 'L', cmd_fn_msg, MSG_LAST },
	{ 'l', cmd_fn_msg, MSG_LAST }
};

/* Dispatch to a command. */
int
cmd_execute(int key, struct buffer *srv_out)
{
	struct cmd	*cmd;
	u_int		 i;

	for (i = 0; i < (sizeof cmd_table / sizeof cmd_table[0]); i++) {
		cmd = cmd_table + i;
		if (cmd->key == key)
			return (cmd->fn(srv_out, cmd->arg));
	}
	return (0);
}

/* Handle generic command. */
int
cmd_fn_msg(struct buffer *srv_out, int type)
{
 	struct hdr	hdr;

	hdr.type = type;
	hdr.size = 0;
	buffer_write(srv_out, &hdr, sizeof hdr);

 	return (0);
}

/* Handle select command. */
int
cmd_fn_select(struct buffer *srv_out, int arg)
{
 	struct hdr		hdr;
	struct select_data	data;

	hdr.type = MSG_SELECT;
	hdr.size = sizeof data;
	buffer_write(srv_out, &hdr, sizeof hdr);
	data.idx = arg;
	buffer_write(srv_out, &data, sizeof data);

	return (0);
}

/* Handle detach command. */
int
cmd_fn_detach(unused struct buffer *srv_out, unused int arg)
{
	return (-1);
}
