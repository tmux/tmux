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

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Sources a configuration file.
 */

#define CMD_SOURCE_FILE_DEPTH_LIMIT 50
static u_int cmd_source_file_depth;

static enum cmd_retval	cmd_source_file_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_source_file_entry = {
	.name = "source-file",
	.alias = "source",

	.args = { "t:Fnqv", 1, -1, NULL },
	.usage = "[-Fnqv] " CMD_TARGET_PANE_USAGE " path ...",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

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
	struct client	*c = cmdq_get_client(item);

	if (c == NULL) {
		cmd_source_file_depth--;
		log_debug("%s: depth now %u", __func__, cmd_source_file_depth);
	} else {
		c->source_file_depth--;
		log_debug("%s: depth now %u", __func__, c->source_file_depth);
	}

	cfg_print_causes(item);
	return (CMD_RETURN_NORMAL);
}

static void
cmd_source_file_complete(struct client *c, struct cmd_source_file_data *cdata)
{
	struct cmdq_item	*new_item;
	u_int			 i;

	if (cfg_finished) {
		if (cdata->retval == CMD_RETURN_ERROR &&
		    c != NULL &&
		    c->session == NULL)
			c->retval = 1;
		new_item = cmdq_get_callback(cmd_source_file_complete_cb, NULL);
		cmdq_insert_after(cdata->after, new_item);
	}

	for (i = 0; i < cdata->nfiles; i++)
		free(cdata->files[i]);
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
	struct cmd_find_state		*target = cmdq_get_target(item);

	if (!closed)
		return;

	if (error != 0)
		cmdq_error(item, "%s: %s", strerror(error), path);
	else if (bsize != 0) {
		if (load_cfg_from_buffer(bdata, bsize, path, c, cdata->after,
		    target, cdata->flags, &new_item) < 0)
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
	char	resolved[PATH_MAX];

	if (realpath(path, resolved) == NULL) {
		log_debug("%s: realpath(\"%s\") failed: %s", __func__,
			path, strerror(errno));
	} else
		path = resolved;

	log_debug("%s: %s", __func__, path);

	cdata->files = xreallocarray(cdata->files, cdata->nfiles + 1,
	    sizeof *cdata->files);
	cdata->files[cdata->nfiles++] = xstrdup(path);
}

static char *
cmd_source_file_quote_for_glob(const char *path)
{
	char		*quoted = xmalloc(2 * strlen(path) + 1), *q = quoted;
	const char	*p = path;

	while (*p != '\0') {
		if ((u_char)*p < 128 && !isalnum((u_char)*p) && *p != '/')
			*q++ = '\\';
		*q++ = *p++;
	}
	*q = '\0';
	return (quoted);
}

static enum cmd_retval
cmd_source_file_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_source_file_data	*cdata;
	struct client			*c = cmdq_get_client(item);
	enum cmd_retval			 retval = CMD_RETURN_NORMAL;
	char				*pattern, *cwd, *expanded = NULL;
	const char			*path, *error;
	glob_t				 g;
	int				 result;
	u_int				 i, j;

	if (c == NULL) {
		if (cmd_source_file_depth >= CMD_SOURCE_FILE_DEPTH_LIMIT) {
			cmdq_error(item, "too many nested files");
			return (CMD_RETURN_ERROR);
		}
		cmd_source_file_depth++;
		log_debug("%s: depth now %u", __func__, cmd_source_file_depth);
	} else {
		if (c->source_file_depth >= CMD_SOURCE_FILE_DEPTH_LIMIT) {
			cmdq_error(item, "too many nested files");
			return (CMD_RETURN_ERROR);
		}
		c->source_file_depth++;
		log_debug("%s: depth now %u", __func__, c->source_file_depth);
	}

	cdata = xcalloc(1, sizeof *cdata);
	cdata->item = item;

	if (args_has(args, 'q'))
		cdata->flags |= CMD_PARSE_QUIET;
	if (args_has(args, 'n'))
		cdata->flags |= CMD_PARSE_PARSEONLY;
	if (args_has(args, 'v') && (c == NULL || ~c->flags & CLIENT_CONTROL))
		cdata->flags |= CMD_PARSE_VERBOSE;

	cwd = cmd_source_file_quote_for_glob(server_client_get_cwd(c, NULL));

	for (i = 0; i < args_count(args); i++) {
		path = args_string(args, i);
		if (args_has(args, 'F')) {
			free(expanded);
			expanded = format_single_from_target(item, path);
			path = expanded;
		}
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
				cmdq_error(item, "%s: %s", error, path);
				retval = CMD_RETURN_ERROR;
			}
			globfree(&g);
			free(pattern);
			continue;
		}
		free(pattern);

		for (j = 0; j < g.gl_pathc; j++)
			cmd_source_file_add(cdata, g.gl_pathv[j]);
		globfree(&g);
	}
	free(expanded);

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
