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
	va_list	ap;

	va_start(ap, fmt);
	file_vprint(c, fmt, ap);
	file_print(c, "\n");
	va_end(ap);
}

/* Control error callback. */
static enum cmd_retval
control_error(struct cmdq_item *item, void *data)
{
	struct client	*c = item->client;
	char		*error = data;

	cmdq_guard(item, "begin", 1);
	control_write(c, "parse error: %s", error);
	cmdq_guard(item, "error", 1);

	free(error);
	return (CMD_RETURN_NORMAL);
}

/* Control input callback. Read lines and fire commands. */
static void
control_callback(__unused struct client *c, __unused const char *path,
    int error, int closed, struct evbuffer *buffer, __unused void *data)
{
	char			*line;
	struct cmdq_item	*item;
	struct cmd_parse_result	*pr;

	if (closed || error != 0)
		c->flags |= CLIENT_EXIT;

	for (;;) {
		line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_LF);
		if (line == NULL)
			break;
		log_debug("%s: %s", __func__, line);
		if (*line == '\0') { /* empty line exit */
			free(line);
			c->flags |= CLIENT_EXIT;
			break;
		}

		pr = cmd_parse_from_string(line, NULL);
		switch (pr->status) {
		case CMD_PARSE_EMPTY:
			break;
		case CMD_PARSE_ERROR:
			item = cmdq_get_callback(control_error, pr->error);
			cmdq_append(c, item);
			break;
		case CMD_PARSE_SUCCESS:
			item = cmdq_get_command(pr->cmdlist, NULL, NULL, 0);
			item->shared->flags |= CMDQ_SHARED_CONTROL;
			cmdq_append(c, item);
			cmd_list_free(pr->cmdlist);
			break;
		}

		free(line);
	}
}

void
control_start(struct client *c)
{
	file_read(c, "-", control_callback, c);

	if (c->flags & CLIENT_CONTROLCONTROL)
		file_print(c, "\033P1000p");
}
