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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Creates a window from JSON written by save-state.
 */

static enum cmd_retval	cmd_load_state_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_load_state_entry = {
	.name = "load-state",
	.alias = NULL,

	.args = { "dt:w", 1, 1, NULL },
	.usage = "-w [-d] " CMD_TARGET_WINDOW_USAGE " path",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_WINDOW_INDEX },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_load_state_exec
};

struct cmd_load_state_data {
	struct cmdq_item	*item;
	struct session		*s;
	struct client		*tc;
	int			 idx;
	int			 detached;
};

static void
cmd_load_state_done(__unused struct client *c, const char *path, int error,
    int closed, struct evbuffer *buffer, void *data)
{
	struct cmd_load_state_data	*cdata = data;
	struct cmdq_item		*item = cdata->item;
	struct session			*s = cdata->s;
	struct client			*tc = cdata->tc;
	struct winlink			*wl;
	struct cmd_find_state		*current = cmdq_get_current(item);
	const char			*bdata = EVBUFFER_DATA(buffer);
	size_t				 bsize = EVBUFFER_LENGTH(buffer);
	char				*copy, *cause;

	if (!closed)
		return;

	if (tc != NULL && (tc->flags & CLIENT_DEAD)) {
		server_client_unref(tc);
		tc = NULL;
	}

	if (error != 0)
		cmdq_error(item, "%s: %s", strerror(error), path);
	else if (!session_alive(s))
		cmdq_error(item, "session no longer exists");
	else if (memchr(bdata, '\0', bsize) != NULL)
		cmdq_error(item, "%s: file contains a NUL byte", path);
	else {
		copy = xmemdup(bdata, bsize);
		wl = state_load_window(copy, s, cdata->idx, tc, cdata->detached,
		    &cause);
		free(copy);
		if (wl == NULL) {
			cmdq_error(item, "%s: %s", path, cause);
			free(cause);
		} else if (!cdata->detached || wl == s->curw) {
			cmd_find_from_winlink(current, wl, 0);
			server_redraw_session_group(s);
		} else
			server_status_session_group(s);
	}

	if (tc != NULL)
		server_client_unref(tc);
	session_remove_ref(s, __func__);
	cmdq_continue(item);
	free(cdata);
}

static enum cmd_retval
cmd_load_state_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct client			*tc = cmdq_get_target_client(item);
	struct cmd_load_state_data	*cdata;
	char				*path;

	if (!args_has(args, 'w')) {
		cmdq_error(item, "-w must be given");
		return (CMD_RETURN_ERROR);
	}

	cdata = xcalloc(1, sizeof *cdata);
	cdata->item = item;
	cdata->s = target->s;
	session_add_ref(cdata->s, __func__);
	if (tc != NULL) {
		cdata->tc = tc;
		cdata->tc->references++;
	}
	cdata->idx = target->idx;
	cdata->detached = args_has(args, 'd');

	path = format_single_from_target(item, args_string(args, 0));
	file_read(cmdq_get_client(item), path, cmd_load_state_done, cdata);
	free(path);

	return (CMD_RETURN_WAIT);
}
