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

#include "tmux.h"

/*
 * Sources a configuration file.
 */

static enum cmd_retval	cmd_source_file_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_source_file_entry = {
	.name = "source-file",
	.alias = "source",

	.args = { "Fnqv", 1, -1 },
	.usage = "[-Fnqv] path ...",

	.flags = 0,
	.exec = cmd_source_file_exec
};

struct cmd_source_file_data {
	struct cmdq_item	 *item;
	int			  flags;

	struct cmdq_item	 *after;
	enum cmd_retval		  retval;

	u_int			  current;
	char			**files;
	u_int			  nfiles;
};

static enum cmd_retval
cmd_source_file_complete_cb(struct cmdq_item *item, __unused void *data)
{
	cfg_print_causes(item);
	return (CMD_RETURN_NORMAL);
}

static void
cmd_source_file_complete(struct client *c, struct cmd_source_file_data *cdata)
{
	struct cmdq_item	*new_item;

	if (cfg_finished) {
		if (cdata->retval == CMD_RETURN_ERROR &&
		    c != NULL &&
		    c->session == NULL)
			c->retval = 1;
		new_item = cmdq_get_callback(cmd_source_file_complete_cb, NULL);
		cmdq_insert_after(cdata->after, new_item);
	}

	free(cdata->files);
	free(cdata);
}

static void
cmd_source_file_done(struct client *c, const char *path, int error,
    int closed, struct evbuffer *buffer, void *data)
{
	struct cmd_source_file_data	*cdata = data;
	struct cmdq_item		*item = cdata->item;
	void				*bdata = EVBUFFER_DATA(buffer);
	size_t				 bsize = EVBUFFER_LENGTH(buffer);
	u_int				 n;
	struct cmdq_item		*new_item;

	if (!closed)
		return;

	if (error != 0)
		cmdq_error(item, "%s: %s", path, strerror(error));
	else if (bsize != 0) {
		if (load_cfg_from_buffer(bdata, bsize, path, c, cdata->after,
		    cdata->flags, &new_item) < 0)
			cdata->retval = CMD_RETURN_ERROR;
		else if (new_item != NULL)
			cdata->after = new_item;
	}

	n = ++cdata->current;
	if (n < cdata->nfiles)
		file_read(c, cdata->files[n], cmd_source_file_done, cdata);
	else {
		cmd_source_file_complete(c, cdata);
		cmdq_continue(item);
	}
}

static void
cmd_source_file_add(struct cmd_source_file_data *cdata, const char *path)
{
	log_debug("%s: %s", __func__, path);
	cdata->files = xreallocarray(cdata->files, cdata->nfiles + 1,
	    sizeof *cdata->files);
	cdata->files[cdata->nfiles++] = xstrdup(path);
}

static enum cmd_retval
cmd_source_file_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_source_file_data	*cdata;
	struct client			*c = cmdq_get_client(item);
	enum cmd_retval			 retval = CMD_RETURN_NORMAL;
	char				*pattern, *cwd, *expand = NULL;
	const char			*path, *error;
	glob_t				 g;
	int				 i, result;
	u_int				 j;

	cdata = xcalloc(1, sizeof *cdata);
	cdata->item = item;

	if (args_has(args, 'q'))
		cdata->flags |= CMD_PARSE_QUIET;
	if (args_has(args, 'n'))
		cdata->flags |= CMD_PARSE_PARSEONLY;
	if (args_has(args, 'v'))
		cdata->flags |= CMD_PARSE_VERBOSE;

	utf8_stravis(&cwd, server_client_get_cwd(c, NULL), VIS_GLOB);

	for (i = 0; i < args->argc; i++) {
		if (args_has(args, 'F')) {
			free(expand);
			expand = format_single_from_target(item, args->argv[i]);
			path = expand;
		} else
			path = args->argv[i];
		if (strcmp(path, "-") == 0) {
			cmd_source_file_add(cdata, "-");
			continue;
		}

		if (*path == '/')
			pattern = xstrdup(path);
		else
			xasprintf(&pattern, "%s/%s", cwd, path);
		log_debug("%s: %s", __func__, pattern);

		if ((result = glob(pattern, 0, NULL, &g)) != 0) {
			if (result != GLOB_NOMATCH ||
			    (~cdata->flags & CMD_PARSE_QUIET)) {
				if (result == GLOB_NOMATCH)
					error = strerror(ENOENT);
				else if (result == GLOB_NOSPACE)
					error = strerror(ENOMEM);
				else
					error = strerror(EINVAL);
				cmdq_error(item, "%s: %s", path, error);
				retval = CMD_RETURN_ERROR;
			}
			free(pattern);
			continue;
		}
		free(expand);
		free(pattern);

		for (j = 0; j < g.gl_pathc; j++)
			cmd_source_file_add(cdata, g.gl_pathv[j]);
	}

	cdata->after = item;
	cdata->retval = retval;

	if (cdata->nfiles != 0) {
		file_read(c, cdata->files[0], cmd_source_file_done, cdata);
		retval = CMD_RETURN_WAIT;
	} else
		cmd_source_file_complete(c, cdata);

	free(cwd);
	return (retval);
}
