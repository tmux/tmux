/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicm@users.sourceforge.net>
 * Copyright (c) 2012 George Nachman <tmux@georgester.com>
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

#include <event.h>
#include <string.h>

#include "tmux.h"

void printflike2 control_msg_error(struct cmd_ctx *, const char *, ...);
void printflike2 control_msg_print(struct cmd_ctx *, const char *, ...);
void printflike2 control_msg_info(struct cmd_ctx *, const char *, ...);
void printflike2 control_write(struct client *, const char *, ...);

/* Command error callback. */
void printflike2
control_msg_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct client	*c = ctx->curclient;
	va_list		 ap;

	va_start(ap, fmt);
	evbuffer_add_vprintf(c->stdout_data, fmt, ap);
	va_end(ap);

	evbuffer_add(c->stdout_data, "\n", 1);
	server_push_stdout(c);
}

/* Command print callback. */
void printflike2
control_msg_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct client	*c = ctx->curclient;
	va_list		 ap;

	va_start(ap, fmt);
	evbuffer_add_vprintf(c->stdout_data, fmt, ap);
	va_end(ap);

	evbuffer_add(c->stdout_data, "\n", 1);
	server_push_stdout(c);
}

/* Command info callback. */
void printflike2
control_msg_info(unused struct cmd_ctx *ctx, unused const char *fmt, ...)
{
}

/* Write a line. */
void printflike2
control_write(struct client *c, const char *fmt, ...)
{
	va_list		 ap;

	va_start(ap, fmt);
	evbuffer_add_vprintf(c->stdout_data, fmt, ap);
	va_end(ap);

	evbuffer_add(c->stdout_data, "\n", 1);
	server_push_stdout(c);
}

/* Control input callback. Read lines and fire commands. */
void
control_callback(struct client *c, int closed, unused void *data)
{
	char		*line, *cause;
	struct cmd_ctx	 ctx;
	struct cmd_list	*cmdlist;

	if (closed)
		c->flags |= CLIENT_EXIT;

	for (;;) {
		line = evbuffer_readln(c->stdin_data, NULL, EVBUFFER_EOL_LF);
		if (line == NULL)
			break;
		if (*line == '\0') { /* empty line exit */
			c->flags |= CLIENT_EXIT;
			break;
		}

		ctx.msgdata = NULL;
		ctx.cmdclient = NULL;
		ctx.curclient = c;

		ctx.error = control_msg_error;
		ctx.print = control_msg_print;
		ctx.info = control_msg_info;

		if (cmd_string_parse(line, &cmdlist, &cause) != 0) {
			control_write(c, "%%error in line \"%s\": %s", line,
			    cause);
			xfree(cause);
		} else {
			cmd_list_exec(cmdlist, &ctx);
			cmd_list_free(cmdlist);
		}

		xfree(line);
	}
}
