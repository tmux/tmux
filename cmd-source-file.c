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

#include <errno.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <vis.h>

#include "tmux.h"

/*
 * Sources a configuration file.
 */

static enum cmd_retval	cmd_source_file_exec(struct cmd *, struct cmdq_item *);

static enum cmd_retval	cmd_source_file_done(struct cmdq_item *, void *);

const struct cmd_entry cmd_source_file_entry = {
	.name = "source-file",
	.alias = "source",

	.args = { "nq", 1, -1 },
	.usage = "[-nq] path ...",

	.flags = 0,
	.exec = cmd_source_file_exec
};

static enum cmd_retval
cmd_source_file_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	int			 flags = 0;
	struct client		*c = item->client;
	struct cmdq_item	*new_item, *after;
	enum cmd_retval		 retval;
	char			*pattern, *cwd;
	const char		*path, *error;
	glob_t			 g;
	int			 i;
	u_int			 j;

	if (args_has(args, 'q'))
		flags |= CMD_PARSE_QUIET;
	if (args_has(args, 'n'))
		flags |= CMD_PARSE_PARSEONLY;
	utf8_stravis(&cwd, server_client_get_cwd(c, NULL), VIS_GLOB);

	retval = CMD_RETURN_NORMAL;
	for (i = 0; i < args->argc; i++) {
		path = args->argv[i];
		if (*path == '/')
			pattern = xstrdup(path);
		else
			xasprintf(&pattern, "%s/%s", cwd, path);
		log_debug("%s: %s", __func__, pattern);

		if (glob(pattern, 0, NULL, &g) != 0) {
			error = strerror(errno);
			if (errno != ENOENT || (~flags & CMD_PARSE_QUIET)) {
				cmdq_error(item, "%s: %s", path, error);
				retval = CMD_RETURN_ERROR;
			}
			free(pattern);
			continue;
		}
		free(pattern);

		after = item;
		for (j = 0; j < g.gl_pathc; j++) {
			path = g.gl_pathv[j];
			if (load_cfg(path, c, after, flags, &new_item) < 0)
				retval = CMD_RETURN_ERROR;
			else if (new_item != NULL)
				after = new_item;
		}
		globfree(&g);
	}
	if (cfg_finished) {
		if (retval == CMD_RETURN_ERROR && c->session == NULL)
			c->retval = 1;
		new_item = cmdq_get_callback(cmd_source_file_done, NULL);
		cmdq_insert_after(item, new_item);
	}

	free(cwd);
	return (retval);
}

static enum cmd_retval
cmd_source_file_done(struct cmdq_item *item, __unused void *data)
{
	cfg_print_causes(item);
	return (CMD_RETURN_NORMAL);
}
