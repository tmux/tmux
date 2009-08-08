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

void	server_msg_command(struct client *, struct msg_command_data *);
void	server_msg_identify(struct client *, struct msg_identify_data *);
void	server_msg_resize(struct client *, struct msg_resize_data *);

void printflike2 server_msg_command_error(struct cmd_ctx *, const char *, ...);
void printflike2 server_msg_command_print(struct cmd_ctx *, const char *, ...);
void printflike2 server_msg_command_info(struct cmd_ctx *, const char *, ...);

int
server_msg_dispatch(struct client *c)
{
	struct hdr		 hdr;
	struct msg_command_data	 commanddata;
	struct msg_identify_data identifydata;
	struct msg_resize_data	 resizedata;
	struct msg_unlock_data	 unlockdata;
	struct msg_environ_data	 environdata;

	for (;;) {
		if (BUFFER_USED(c->in) < sizeof hdr)
			return (0);
		memcpy(&hdr, BUFFER_OUT(c->in), sizeof hdr);
		if (BUFFER_USED(c->in) < (sizeof hdr) + hdr.size)
			return (0);
		buffer_remove(c->in, sizeof hdr);

		switch (hdr.type) {
		case MSG_COMMAND:
			if (hdr.size != sizeof commanddata)
				fatalx("bad MSG_COMMAND size");
			buffer_read(c->in, &commanddata, sizeof commanddata);

			server_msg_command(c, &commanddata);
			break;
		case MSG_IDENTIFY:
			if (hdr.size != sizeof identifydata)
				fatalx("bad MSG_IDENTIFY size");
			buffer_read(c->in, &identifydata, sizeof identifydata);

			server_msg_identify(c, &identifydata);
			break;
		case MSG_RESIZE:
			if (hdr.size != sizeof resizedata)
				fatalx("bad MSG_RESIZE size");
			buffer_read(c->in, &resizedata, sizeof resizedata);

			server_msg_resize(c, &resizedata);
			break;
		case MSG_EXITING:
			if (hdr.size != 0)
				fatalx("bad MSG_EXITING size");

			c->session = NULL;
			tty_close(&c->tty, c->flags & CLIENT_SUSPENDED);
			server_write_client(c, MSG_EXITED, NULL, 0);
			break;
		case MSG_UNLOCK:
			if (hdr.size != sizeof unlockdata)
				fatalx("bad MSG_UNLOCK size");
			buffer_read(c->in, &unlockdata, sizeof unlockdata);

			unlockdata.pass[(sizeof unlockdata.pass) - 1] = '\0';
			if (server_unlock(unlockdata.pass) != 0)
				server_write_error(c, "bad password");
			memset(&unlockdata, 0, sizeof unlockdata);
			server_write_client(c, MSG_EXIT, NULL, 0);
			break;
		case MSG_WAKEUP:
			if (hdr.size != 0)
				fatalx("bad MSG_WAKEUP size");

			c->flags &= ~CLIENT_SUSPENDED;
			tty_start_tty(&c->tty);
			server_redraw_client(c);
			break;
		case MSG_ENVIRON:
			if (hdr.size != sizeof environdata)
				fatalx("bad MSG_ENVIRON size");
			buffer_read(c->in, &environdata, sizeof environdata);

			environdata.var[(sizeof environdata.var) - 1] = '\0';
			if (strchr(environdata.var, '=') != NULL)
				environ_put(&c->environ, environdata.var);
			break;
		default:
			fatalx("unexpected message");
		}
	}
}

void printflike2
server_msg_command_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_ERROR, &data, sizeof data);
}

void printflike2
server_msg_command_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_PRINT, &data, sizeof data);
}

void printflike2
server_msg_command_info(struct cmd_ctx *ctx, const char *fmt, ...)
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
server_msg_command(struct client *c, struct msg_command_data *data)
{
	struct cmd_ctx	 ctx;
	struct cmd_list	*cmdlist = NULL;
	struct cmd	*cmd;
	int		 argc;
	char	       **argv, *cause;

	server_activity = time(NULL);

	ctx.error = server_msg_command_error;
	ctx.print = server_msg_command_print;
	ctx.info = server_msg_command_info;

	ctx.msgdata = data;
	ctx.curclient = NULL;
	ctx.cursession = NULL;

	ctx.cmdclient = c;

	argc = data->argc;
	data->argv[(sizeof data->argv) - 1] = '\0';
	if (cmd_unpack_argv(data->argv, sizeof data->argv, argc, &argv) != 0) {
		server_msg_command_error(&ctx, "command too long");
		goto error;
	}

	if (argc == 0) {
		argc = 1;
		argv = xcalloc(1, sizeof *argv);
		*argv = xstrdup("new-session");
	}

	if ((cmdlist = cmd_list_parse(argc, argv, &cause)) == NULL) {
		server_msg_command_error(&ctx, "%s", cause);
		cmd_free_argv(argc, argv);
		goto error;
	}
	cmd_free_argv(argc, argv);

	if (data->pid != -1) {
		TAILQ_FOREACH(cmd, cmdlist, qentry) {
			if (cmd->entry->flags & CMD_CANTNEST) {
				server_msg_command_error(&ctx,
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
server_msg_identify(struct client *c, struct msg_identify_data *data)
{
	if (data->version != PROTOCOL_VERSION) {
		server_write_error(c, "protocol version mismatch");
		return;
	}

	c->tty.sx = data->sx;
	c->tty.sy = data->sy;

	c->cwd = NULL;
	data->cwd[(sizeof data->cwd) - 1] = '\0';
	if (*data->cwd != '\0')
		c->cwd = xstrdup(data->cwd);

	data->tty[(sizeof data->tty) - 1] = '\0';
	data->term[(sizeof data->term) - 1] = '\0';
	tty_init(&c->tty, data->tty, data->term);
	if (data->flags & IDENTIFY_UTF8)
		c->tty.flags |= TTY_UTF8;
	if (data->flags & IDENTIFY_256COLOURS)
		c->tty.term_flags |= TERM_256COLOURS;
	else if (data->flags & IDENTIFY_88COLOURS)
		c->tty.term_flags |= TERM_88COLOURS;
	if (data->flags & IDENTIFY_HASDEFAULTS)
		c->tty.term_flags |= TERM_HASDEFAULTS;

	c->flags |= CLIENT_TERMINAL;
}

void
server_msg_resize(struct client *c, struct msg_resize_data *data)
{
	c->tty.sx = data->sx;
	if (c->tty.sx == 0)
		c->tty.sx = 80;
	c->tty.sy = data->sy;
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
