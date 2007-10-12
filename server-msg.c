/* $Id: server-msg.c,v 1.26 2007-10-12 14:46:48 nicm Exp $ */

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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

int	server_msg_fn_command(struct hdr *, struct client *);
int	server_msg_fn_identify(struct hdr *, struct client *);
int	server_msg_fn_keys(struct hdr *, struct client *);
int	server_msg_fn_resize(struct hdr *, struct client *);

void	server_msg_fn_command_error(struct cmd_ctx *, const char *, ...);
void	server_msg_fn_command_print(struct cmd_ctx *, const char *, ...);

struct server_msg {
	enum hdrtype	type;
	
	int	        (*fn)(struct hdr *, struct client *);
};
const struct server_msg server_msg_table[] = {
	{ MSG_IDENTIFY, server_msg_fn_identify },
	{ MSG_COMMAND, server_msg_fn_command },
	{ MSG_RESIZE, server_msg_fn_resize },
	{ MSG_KEYS, server_msg_fn_keys },
};
#define NSERVERMSG (sizeof server_msg_table / sizeof server_msg_table[0])

int
server_msg_dispatch(struct client *c)
{
	struct hdr		 hdr;
	const struct server_msg	*msg;
	u_int		 	 i;
	int			 n;

	for (;;) {
		if (BUFFER_USED(c->in) < sizeof hdr)
			return (0);
		memcpy(&hdr, BUFFER_OUT(c->in), sizeof hdr);
		if (BUFFER_USED(c->in) < (sizeof hdr) + hdr.size)
			return (0);
		buffer_remove(c->in, sizeof hdr);
		
		for (i = 0; i < NSERVERMSG; i++) {
			msg = server_msg_table + i;
			if (msg->type == hdr.type) {
				if ((n = msg->fn(&hdr, c)) != 0)
					return (n);
				break;
			}
		}	
		if (i == NSERVERMSG)
			fatalx("unexpected message");
	}
}

void
server_msg_fn_command_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->client, MSG_ERROR, msg, strlen(msg));
	xfree(msg);
}

void
server_msg_fn_command_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->client, MSG_PRINT, msg, strlen(msg));
	xfree(msg);
}

int
server_msg_fn_command(struct hdr *hdr, struct client *c)
{
	struct msg_command_data	data;
	struct cmd_ctx	 	ctx;
	struct cmd	       *cmd;
	char	       	       *cause;

	if (hdr->size < sizeof data)
		fatalx("bad MSG_COMMAND size");
	buffer_read(c->in, &data, sizeof data);

	cmd = cmd_recv(c->in);
	log_debug("got command %s from client %d", cmd->entry->name, c->fd);

	ctx.error = server_msg_fn_command_error;
	ctx.print = server_msg_fn_command_print;

	ctx.client = c;
	ctx.flags = 0;

	if (data.sid.pid != -1 && (cmd->entry->flags & CMD_CANTNEST)) {
		server_msg_fn_command_error(&ctx, "sessions should be nested "
		    "with care. unset $TMUX and retry to force");
		return (0);
	}

	if (cmd->entry->flags & CMD_NOSESSION)
		ctx.session = NULL;
	else {
		ctx.session = server_find_sessid(&data.sid, &cause);
		if (ctx.session == NULL) {
			server_msg_fn_command_error(&ctx, "%s", cause);
			xfree(cause);
			return (0);
		}
	}		

	cmd_exec(cmd, &ctx);
	cmd_free(cmd);

	return (0);
}

int
server_msg_fn_identify(struct hdr *hdr, struct client *c)
{
	struct msg_identify_data	data;

	if (hdr->size < sizeof data)
		fatalx("bad MSG_IDENTIFY size");
	buffer_read(c->in, &data, sizeof data);

	log_debug("identify msg from client: %u,%u", data.sx, data.sy);

	c->sx = data.sx;
	c->sy = data.sy;

	c->flags |= CLIENT_TERMINAL;

	return (0);
}

int
server_msg_fn_resize(struct hdr *hdr, struct client *c)
{
	struct msg_resize_data	data;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_RESIZE size");
	buffer_read(c->in, &data, sizeof data);

	log_debug("resize msg from client: %u,%u", data.sx, data.sy);

	c->sx = data.sx;
	if (c->sx == 0)
		c->sx = 80;
	c->sy = data.sy;
	if (c->sy == 0)
		c->sy = 25;

	recalculate_sizes();

	return (0);
}

int
server_msg_fn_keys(struct hdr *hdr, struct client *c)
{
	int	key;
	size_t	size;

	if (hdr->size & 0x1)
		fatalx("bad MSG_KEYS size");

	if (c->flags & CLIENT_HOLD) {
		server_redraw_client(c);
		c->flags &= ~CLIENT_HOLD;
	}

	size = hdr->size;
	while (size != 0) {
		key = (int16_t) input_extract16(c->in);
		size -= 2;

		if (c->flags & CLIENT_PREFIX) {
			key_bindings_dispatch(key, c);
			c->flags &= ~CLIENT_PREFIX;
			continue;
		}

		if (key == prefix_key)
			c->flags |= CLIENT_PREFIX;
		else
			window_key(c->session->window, key);
	}

	return (0);
}
