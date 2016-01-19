/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/* Write a line. */
void
control_write(struct client *c, const char *fmt, ...)
{
	va_list		 ap;

	va_start(ap, fmt);
	evbuffer_add_vprintf(c->stdout_data, fmt, ap);
	va_end(ap);

	evbuffer_add(c->stdout_data, "\n", 1);
	server_client_push_stdout(c);
}

/* Write a buffer, adding a terminal newline. Empties buffer. */
void
control_write_buffer(struct client *c, struct evbuffer *buffer)
{
	evbuffer_add_buffer(c->stdout_data, buffer);
	evbuffer_add(c->stdout_data, "\n", 1);
	server_client_push_stdout(c);
}

/* Control input callback. Read lines and fire commands. */
void
control_callback(struct client *c, int closed, __unused void *data)
{
	char		*line, *cause;
	struct cmd_list	*cmdlist;
	struct cmd	*cmd;

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

		if (cmd_string_parse(line, &cmdlist, NULL, 0, &cause) != 0) {
			c->cmdq->time = time(NULL);
			c->cmdq->number++;

			cmdq_guard(c->cmdq, "begin", 1);
			control_write(c, "parse error: %s", cause);
			cmdq_guard(c->cmdq, "error", 1);

			free(cause);
		} else {
			TAILQ_FOREACH(cmd, &cmdlist->list, qentry)
				cmd->flags |= CMD_CONTROL;
			cmdq_run(c->cmdq, cmdlist, NULL);
			cmd_list_free(cmdlist);
		}

		free(line);
	}
}
