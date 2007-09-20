/* $Id: command.c,v 1.5 2007-09-20 18:48:04 nicm Exp $ */

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
int	cmd_fn_create(struct buffer *, int);
int	cmd_fn_detach(struct buffer *, int);
int	cmd_fn_next(struct buffer *, int);
int	cmd_fn_previous(struct buffer *, int);
int	cmd_fn_refresh(struct buffer *, int);
int	cmd_fn_rename(struct buffer *, int);
int	cmd_fn_last(struct buffer *, int);

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
	{ 'C', cmd_fn_create, 0 },
	{ 'c', cmd_fn_create, 0 },
	{ 'D', cmd_fn_detach, 0 },
	{ 'd', cmd_fn_detach, 0 },
	{ 'N', cmd_fn_next, 0 },
	{ 'n', cmd_fn_next, 0 },
	{ 'P', cmd_fn_previous, 0 },
	{ 'p', cmd_fn_previous, 0 },
	{ 'R', cmd_fn_refresh, 0 },
	{ 'r', cmd_fn_refresh, 0 },
	{ 'T', cmd_fn_rename, 0 },
	{ 't', cmd_fn_rename, 0 },
	{ 'L', cmd_fn_last, 0 },
	{ 'l', cmd_fn_last, 0 }
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

/* Handle create command. */
int
cmd_fn_create(struct buffer *srv_out, unused int arg)
{
 	struct hdr	hdr;

	hdr.type = MSG_CREATE;
	hdr.size = 0;
	buffer_write(srv_out, &hdr, sizeof hdr);

	return (0);
}

/* Handle detach command. */
int
cmd_fn_detach(unused struct buffer *srv_out, unused int arg)
{
	return (-1);
}

/* Handle next command. */
int
cmd_fn_next(struct buffer *srv_out, unused int arg)
{
 	struct hdr	hdr;

	hdr.type = MSG_NEXT;
	hdr.size = 0;
	buffer_write(srv_out, &hdr, sizeof hdr);

 	return (0);
}

/* Handle previous command. */
int
cmd_fn_previous(struct buffer *srv_out, unused int arg)
{
 	struct hdr	hdr;

	hdr.type = MSG_PREVIOUS;
	hdr.size = 0;
	buffer_write(srv_out, &hdr, sizeof hdr);

 	return (0);
}

/* Handle refresh command. */
int
cmd_fn_refresh(struct buffer *srv_out, unused int arg)
{
 	struct hdr	hdr;

	hdr.type = MSG_REFRESH;
	hdr.size = 0;
	buffer_write(srv_out, &hdr, sizeof hdr);

 	return (0);
}

/* Handle rename command. */
int
cmd_fn_rename(struct buffer *srv_out, unused int arg)
{
 	struct hdr	hdr;

	hdr.type = MSG_RENAME;
	hdr.size = 0;
	buffer_write(srv_out, &hdr, sizeof hdr);

	return (0);
}

/* Handle last command. */
int
cmd_fn_last(struct buffer *srv_out, unused int arg)
{
 	struct hdr	hdr;

	hdr.type = MSG_LAST;
	hdr.size = 0;
	buffer_write(srv_out, &hdr, sizeof hdr);

	return (0);
}

