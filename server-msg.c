/* $Id: server-msg.c,v 1.86 2009-09-23 14:44:02 tcunha Exp $ */

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
#include <sys/ioctl.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

void	server_msg_command(struct client *, struct msg_command_data *);
void	server_msg_identify(struct client *, struct msg_identify_data *, int);

void printflike2 server_msg_command_error(struct cmd_ctx *, const char *, ...);
void printflike2 server_msg_command_print(struct cmd_ctx *, const char *, ...);
void printflike2 server_msg_command_info(struct cmd_ctx *, const char *, ...);

int
server_msg_dispatch(struct client *c)
{
	struct imsg		 imsg;
	struct msg_command_data	 commanddata;
	struct msg_identify_data identifydata;
	struct msg_unlock_data	 unlockdata;
	struct msg_environ_data	 environdata;
	ssize_t			 n, datalen;

        if ((n = imsg_read(&c->ibuf)) == -1 || n == 0)
                return (-1);

	for (;;) {
		if ((n = imsg_get(&c->ibuf, &imsg)) == -1)
			return (-1);
		if (n == 0)
			return (0);
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		if (imsg.hdr.peerid != PROTOCOL_VERSION) {
			server_write_client(c, MSG_VERSION, NULL, 0);
			c->flags |= CLIENT_BAD;
			imsg_free(&imsg);
			continue;
		}

		log_debug("got %d from client %d", imsg.hdr.type, c->ibuf.fd);
		switch (imsg.hdr.type) {
		case MSG_COMMAND:
			if (datalen != sizeof commanddata)
				fatalx("bad MSG_COMMAND size");
			memcpy(&commanddata, imsg.data, sizeof commanddata);

			server_msg_command(c, &commanddata);
			break;
		case MSG_IDENTIFY:
			if (datalen != sizeof identifydata)
				fatalx("bad MSG_IDENTIFY size");
			if (imsg.fd == -1)
				fatalx("MSG_IDENTIFY missing fd");
			memcpy(&identifydata, imsg.data, sizeof identifydata);

			server_msg_identify(c, &identifydata, imsg.fd);
			break;
		case MSG_RESIZE:
			if (datalen != 0)
				fatalx("bad MSG_RESIZE size");

			tty_resize(&c->tty);
			recalculate_sizes();
			server_redraw_client(c);
			break;
		case MSG_EXITING:
			if (datalen != 0)
				fatalx("bad MSG_EXITING size");

			c->session = NULL;
			tty_close(&c->tty);
			server_write_client(c, MSG_EXITED, NULL, 0);
			break;
		case MSG_UNLOCK:
			if (datalen != sizeof unlockdata)
				fatalx("bad MSG_UNLOCK size");
			memcpy(&unlockdata, imsg.data, sizeof unlockdata);

			unlockdata.pass[(sizeof unlockdata.pass) - 1] = '\0';
			switch (server_unlock(unlockdata.pass)) {
			case -1:
				server_write_error(c, "bad password");
				break;
			case -2:
				server_write_error(c,
				    "too many bad passwords, sleeping");
				break;
			}
			memset(&unlockdata, 0, sizeof unlockdata);
			server_write_client(c, MSG_EXIT, NULL, 0);
			break;
		case MSG_WAKEUP:
			if (datalen != 0)
				fatalx("bad MSG_WAKEUP size");

			c->flags &= ~CLIENT_SUSPENDED;
			tty_start_tty(&c->tty);
			server_redraw_client(c);
			break;
		case MSG_ENVIRON:
			if (datalen != sizeof environdata)
				fatalx("bad MSG_ENVIRON size");
			memcpy(&environdata, imsg.data, sizeof environdata);

			environdata.var[(sizeof environdata.var) - 1] = '\0';
			if (strchr(environdata.var, '=') != NULL)
				environ_put(&c->environ, environdata.var);
			break;
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
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
server_msg_identify(struct client *c, struct msg_identify_data *data, int fd)
{
	c->cwd = NULL;
	data->cwd[(sizeof data->cwd) - 1] = '\0';
	if (*data->cwd != '\0')
		c->cwd = xstrdup(data->cwd);

	data->term[(sizeof data->term) - 1] = '\0';
	tty_init(&c->tty, fd, data->term);
	if (data->flags & IDENTIFY_UTF8)
		c->tty.flags |= TTY_UTF8;
	if (data->flags & IDENTIFY_256COLOURS)
		c->tty.term_flags |= TERM_256COLOURS;
	else if (data->flags & IDENTIFY_88COLOURS)
		c->tty.term_flags |= TERM_88COLOURS;
	if (data->flags & IDENTIFY_HASDEFAULTS)
		c->tty.term_flags |= TERM_HASDEFAULTS;

	tty_resize(&c->tty);

	c->flags |= CLIENT_TERMINAL;
}
