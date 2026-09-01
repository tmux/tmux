/*
 * Copyright (c) 2026 asRizvi888 <asrizvi2357@gmail.com>
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

/*
 * Recreate the session/window/pane layout previously written by 'dump'.
 * This just replays the recorded new-session/new-window/split-window/
 * select-layout commands - it starts a fresh shell in each pane at the
 * recorded working directory, it does not try to relaunch whatever was
 * actually running there before.
 */

static enum cmd_retval	cmd_restore_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_restore_entry = {
	.name = "restore",
	.alias = NULL,

	.args = { "f:", 0, 0, NULL },
	.usage = "[-f path]",

	.flags = CMD_STARTSERVER,
	.exec = cmd_restore_exec
};

static enum cmd_retval
cmd_restore_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct client		*c = cmdq_get_client(item);
	const char		*path;
	char			*freeme = NULL;

	if ((path = args_get(args, 'f')) == NULL) {
		if ((path = freeme = dump_get_path()) == NULL) {
			cmdq_error(item, "could not determine dump file "
			    "location");
			return (CMD_RETURN_ERROR);
		}
	}

	if (access(path, R_OK) != 0) {
		cmdq_error(item, "no dump file found: %s", path);
		free(freeme);
		return (CMD_RETURN_ERROR);
	}

	if (load_cfg(path, c, item, current, 0, NULL) != 0)
		cfg_print_causes(item);

	free(freeme);
	return (CMD_RETURN_NORMAL);
}
