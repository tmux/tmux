/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Tiago Cunha <me@tiagocunha.org>
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

#include "tmux.h"

/*
 * Sources a configuration file.
 */

static enum cmd_retval	cmd_source_file_exec(struct cmd *, struct cmd_q *);

static enum cmd_retval	cmd_source_file_done(struct cmd_q *, void *);

const struct cmd_entry cmd_source_file_entry = {
	.name = "source-file",
	.alias = "source",

	.args = { "q", 1, 1 },
	.usage = "[-q] path",

	.flags = 0,
	.exec = cmd_source_file_exec
};

static enum cmd_retval
cmd_source_file_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct client	*c = cmdq->client;
	int		 quiet;
	struct cmd_q	*new_cmdq;

	quiet = args_has(args, 'q');
	switch (load_cfg(args->argv[0], c, cmdq, quiet)) {
	case -1:
		if (cfg_finished)
			cfg_print_causes(cmdq);
		return (CMD_RETURN_ERROR);
	case 0:
		if (cfg_finished)
			cfg_print_causes(cmdq);
		return (CMD_RETURN_NORMAL);
	}
	if (cfg_finished) {
		new_cmdq = cmdq_get_callback(cmd_source_file_done, NULL);
		cmdq_insert_after(cmdq, new_cmdq);
	}
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_source_file_done(struct cmd_q *cmdq, __unused void *data)
{
	cfg_print_causes(cmdq);
	return (CMD_RETURN_NORMAL);
}
