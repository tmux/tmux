/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Alexandre Fiori <fiorix@gmail.com>
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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Saves a window as JSON to a file.
 */

static enum cmd_retval	cmd_save_state_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_save_state_entry = {
	.name = "save-state",
	.alias = NULL,

	.args = { "t:w", 1, 1, NULL },
	.usage = "-w " CMD_TARGET_WINDOW_USAGE " path",

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_save_state_exec
};

static void
cmd_save_state_done(__unused struct client *c, const char *path, int error,
    int closed, __unused struct evbuffer *buffer, void *data)
{
	struct cmdq_item	*item = data;

	if (!closed)
		return;

	if (error != 0)
		cmdq_error(item, "%s: %s", strerror(error), path);
	cmdq_continue(item);
}

static enum cmd_retval
cmd_save_state_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct winlink		*wl = cmdq_get_target(item)->wl;
	struct evbuffer		*evb;
	char			*path, *cause;

	if (!args_has(args, 'w')) {
		cmdq_error(item, "-w must be given");
		return (CMD_RETURN_ERROR);
	}

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");
	if (state_save_window(wl->window, evb, &cause) != 0) {
		cmdq_error(item, "%s", cause);
		free(cause);
		evbuffer_free(evb);
		return (CMD_RETURN_ERROR);
	}

	path = format_single_from_target(item, args_string(args, 0));
	file_write(cmdq_get_client(item), path, O_TRUNC, EVBUFFER_DATA(evb),
	    EVBUFFER_LENGTH(evb), cmd_save_state_done, item);
	free(path);
	evbuffer_free(evb);

	return (CMD_RETURN_WAIT);
}
