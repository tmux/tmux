/* $OpenBSD$ */

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
#include <time.h>
#include <unistd.h>

#include "tmux.h"

void	server_msg_fn_command(struct hdr *, struct client *);
void	server_msg_fn_identify(struct hdr *, struct client *);
void	server_msg_fn_resize(struct hdr *, struct client *);
void	server_msg_fn_exiting(struct hdr *, struct client *);
void	server_msg_fn_unlock(struct hdr *, struct client *);
void	server_msg_fn_wakeup(struct hdr *, struct client *);

void printflike2 server_msg_fn_command_error(
    	    struct cmd_ctx *, const char *, ...);
void printflike2 server_msg_fn_command_print(
    	    struct cmd_ctx *, const char *, ...);
void printflike2 server_msg_fn_command_info(
    	    struct cmd_ctx *, const char *, ...);

struct server_msg {
	enum hdrtype	type;
	void	        (*fn)(struct hdr *, struct client *);
};
const struct server_msg server_msg_table[] = {
	{ MSG_IDENTIFY, server_msg_fn_identify },
	{ MSG_COMMAND, server_msg_fn_command },
	{ MSG_RESIZE, server_msg_fn_resize },
	{ MSG_EXITING, server_msg_fn_exiting },
	{ MSG_UNLOCK, server_msg_fn_unlock },
	{ MSG_WAKEUP, server_msg_fn_wakeup },
};

int
server_msg_dispatch(struct client *c)
{
	struct hdr		 hdr;
	const struct server_msg	*msg;
	u_int		 	 i;

	for (;;) {
		if (BUFFER_USED(c->in) < sizeof hdr)
			return (0);
		memcpy(&hdr, BUFFER_OUT(c->in), sizeof hdr);
		if (BUFFER_USED(c->in) < (sizeof hdr) + hdr.size)
			return (0);
		buffer_remove(c->in, sizeof hdr);

		for (i = 0; i < nitems(server_msg_table); i++) {
			msg = server_msg_table + i;
			if (msg->type == hdr.type) {
				msg->fn(&hdr, c);
				break;
			}
		}
		if (i == nitems(server_msg_table))
			fatalx("unexpected message");
	}
}

void printflike2
server_msg_fn_command_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_ERROR, &data, sizeof data);
}

void printflike2
server_msg_fn_command_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_PRINT, &data, sizeof data);
}

void printflike2
server_msg_fn_command_info(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	if (be_quiet)
		return;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_PRINT, &data, sizeof data);
}

void
server_msg_fn_command(struct hdr *hdr, struct client *c)
{
	struct msg_command_data	data;
	struct cmd_ctx	 	ctx;
	struct cmd_list	       *cmdlist = NULL;
	struct cmd	       *cmd;
	int			argc;
	char		      **argv, *cause;

	if (hdr->size < sizeof data)
		fatalx("bad MSG_COMMAND size");
	buffer_read(c->in, &data, sizeof data);

	server_activity = time(NULL);

	ctx.error = server_msg_fn_command_error;
	ctx.print = server_msg_fn_command_print;
	ctx.info = server_msg_fn_command_info;

	ctx.msgdata = &data;
	ctx.curclient = NULL;
	ctx.cursession = NULL;

	ctx.cmdclient = c;

	argc = data.argc;
	data.argv[(sizeof data.argv) - 1] = '\0';
	if (cmd_unpack_argv(data.argv, sizeof data.argv, argc, &argv) != 0) {
		server_msg_fn_command_error(&ctx, "command too long");
		goto error;
	}

	if (argc == 0) {
		argc = 1;
		argv = xcalloc(1, sizeof *argv);
		*argv = xstrdup("new-session");
	}

	if ((cmdlist = cmd_list_parse(argc, argv, &cause)) == NULL) {
		server_msg_fn_command_error(&ctx, "%s", cause);
		cmd_free_argv(argc, argv);
		goto error;
	}
	cmd_free_argv(argc, argv);

	if (data.pid != -1) {
		TAILQ_FOREACH(cmd, cmdlist, qentry) {
			if (cmd->entry->flags & CMD_CANTNEST) {
				server_msg_fn_command_error(&ctx,
				    "sessions should be nested with care. "
				    "unset $TMUX to force");
				goto error;
			}
		}
	}

	if (cmd_list_exec(cmdlist, &ctx) != 1)
		server_write_client(c, MSG_EXIT, NULL, 0);
	cmd_list_free(cmdlist);
	return;

error:
	if (cmdlist != NULL)
		cmd_list_free(cmdlist);
	server_write_client(c, MSG_EXIT, NULL, 0);
}

void
server_msg_fn_identify(struct hdr *hdr, struct client *c)
{
	struct msg_identify_data	data;

	if (hdr->size < sizeof data)
		fatalx("bad MSG_IDENTIFY size");
	buffer_read(c->in, &data, sizeof data);

	log_debug("identify msg from client: %u,%u (%d)",
	    data.sx, data.sy, data.version);

	if (data.version != PROTOCOL_VERSION) {
		server_write_error(c, "protocol version mismatch");
		return;
	}

	c->tty.sx = data.sx;
	c->tty.sy = data.sy;

	c->cwd = NULL;
	data.cwd[(sizeof data.cwd) - 1] = '\0';
	if (*data.cwd != '\0')
		c->cwd = xstrdup(data.cwd);

	data.tty[(sizeof data.tty) - 1] = '\0';
	data.term[(sizeof data.term) - 1] = '\0';
	tty_init(&c->tty, data.tty, data.term);
	if (data.flags & IDENTIFY_UTF8)
		c->tty.flags |= TTY_UTF8;
	if (data.flags & IDENTIFY_256COLOURS)
		c->tty.term_flags |= TERM_256COLOURS;
	else if (data.flags & IDENTIFY_88COLOURS)
		c->tty.term_flags |= TERM_88COLOURS;
	if (data.flags & IDENTIFY_HASDEFAULTS)
		c->tty.term_flags |= TERM_HASDEFAULTS;

	c->flags |= CLIENT_TERMINAL;
}

void
server_msg_fn_resize(struct hdr *hdr, struct client *c)
{
	struct msg_resize_data	data;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_RESIZE size");
	buffer_read(c->in, &data, sizeof data);

	log_debug("resize msg from client: %u,%u", data.sx, data.sy);

	c->tty.sx = data.sx;
	if (c->tty.sx == 0)
		c->tty.sx = 80;
	c->tty.sy = data.sy;
	if (c->tty.sy == 0)
		c->tty.sy = 25;

	c->tty.cx = UINT_MAX;
	c->tty.cy = UINT_MAX;
	c->tty.rupper = UINT_MAX;
	c->tty.rlower = UINT_MAX;

	recalculate_sizes();

	/* Always redraw this client. */
	server_redraw_client(c);
}

void
server_msg_fn_exiting(struct hdr *hdr, struct client *c)
{
	if (hdr->size != 0)
		fatalx("bad MSG_EXITING size");

	log_debug("exiting msg from client");

	c->session = NULL;

	tty_close(&c->tty, c->flags & CLIENT_SUSPENDED);

	server_write_client(c, MSG_EXITED, NULL, 0);
}

void
server_msg_fn_unlock(struct hdr *hdr, struct client *c)
{
	struct msg_unlock_data	data;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_UNLOCK size");
	buffer_read(c->in, &data, sizeof data);

	log_debug("unlock msg from client");

	data.pass[(sizeof data.pass) - 1] = '\0';
	if (server_unlock(data.pass) != 0)
		server_write_error(c, "bad password");
	memset(&data, 0, sizeof data);

	server_write_client(c, MSG_EXIT, NULL, 0);
}

void
server_msg_fn_wakeup(struct hdr *hdr, struct client *c)
{
	if (hdr->size != 0)
		fatalx("bad MSG_WAKEUP size");

	log_debug("wakeup msg from client");

	c->flags &= ~CLIENT_SUSPENDED;
	tty_start_tty(&c->tty);
	server_redraw_client(c);
}
