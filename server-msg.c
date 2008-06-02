/* $Id: server-msg.c,v 1.44 2008-06-02 18:08:17 nicm Exp $ */

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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

int	server_msg_fn_command(struct hdr *, struct client *);
int	server_msg_fn_identify(struct hdr *, struct client *);
int	server_msg_fn_resize(struct hdr *, struct client *);
int	server_msg_fn_exiting(struct hdr *, struct client *);

void printflike2 server_msg_fn_command_error(
    	    struct cmd_ctx *, const char *, ...);
void printflike2 server_msg_fn_command_print(
    	    struct cmd_ctx *, const char *, ...);

struct server_msg {
	enum hdrtype	type;

	int	        (*fn)(struct hdr *, struct client *);
};
const struct server_msg server_msg_table[] = {
	{ MSG_IDENTIFY, server_msg_fn_identify },
	{ MSG_COMMAND, server_msg_fn_command },
	{ MSG_RESIZE, server_msg_fn_resize },
	{ MSG_EXITING, server_msg_fn_exiting }
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

void printflike2
server_msg_fn_command_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_ERROR, msg, strlen(msg));
	xfree(msg);
}

void printflike2
server_msg_fn_command_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_PRINT, msg, strlen(msg));
	xfree(msg);
}

int
server_msg_fn_command(struct hdr *hdr, struct client *c)
{
	struct msg_command_data	data;
	struct cmd_ctx	 	ctx;
	struct cmd	       *cmd;

	if (hdr->size < sizeof data)
		fatalx("bad MSG_COMMAND size");
	buffer_read(c->in, &data, sizeof data);

	cmd = cmd_recv(c->in);
	log_debug("got command %s from client %d", cmd->entry->name, c->fd);

	ctx.error = server_msg_fn_command_error;
	ctx.print = server_msg_fn_command_print;

	ctx.curclient = NULL;
	ctx.cursession = NULL;
	ctx.msgdata = &data;

	ctx.cmdclient = c;
	ctx.flags = 0;

	/* XXX */
	if (data.pid != -1 && (cmd->entry->flags & CMD_CANTNEST)) {
		server_msg_fn_command_error(&ctx, "sessions "
		    "should be nested with care. unset $TMUX to force");
		cmd_free(cmd);
		return (0);
	}

	cmd_exec(cmd, &ctx);
	cmd_free(cmd);
	return (0);
}

int
server_msg_fn_identify(struct hdr *hdr, struct client *c)
{
	struct msg_identify_data	data;
        char			       *term;

	if (hdr->size < sizeof data)
		fatalx("bad MSG_IDENTIFY size");
	buffer_read(c->in, &data, sizeof data);
	term = cmd_recv_string(c->in);

	log_debug("identify msg from client: %u,%u", data.sx, data.sy);

	c->sx = data.sx;
	c->sy = data.sy;

	data.tty[(sizeof data.tty) - 1] = '\0';
	tty_init(&c->tty, data.tty, xstrdup(term));
	xfree(term);

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

	/* Always redraw this client. */
	server_redraw_client(c);

	return (0);
}

int
server_msg_fn_exiting(struct hdr *hdr, struct client *c)
{
	if (hdr->size != 0)
		fatalx("bad MSG_EXITING size");

	log_debug("exiting msg from client");

	c->session = NULL;
	
	tty_close(&c->tty);

	server_write_client(c, MSG_EXITED, NULL, 0);

	return (0);
}
