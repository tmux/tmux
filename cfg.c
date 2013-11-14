/* $Id$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct cmd_q		*cfg_cmd_q;
int			 cfg_finished;
int			 cfg_references;
struct causelist	 cfg_causes;
struct client		*cfg_client;

int
load_cfg(const char *path, struct cmd_q *cmdq, char **cause)
{
	FILE		*f;
	u_int		 n, found;
	char		*buf, *copy, *line, *cause1, *msg;
	size_t		 len, oldlen;
	struct cmd_list	*cmdlist;

	log_debug("loading %s", path);
	if ((f = fopen(path, "rb")) == NULL) {
		xasprintf(cause, "%s: %s", path, strerror(errno));
		return (-1);
	}

	n = found = 0;
	line = NULL;
	while ((buf = fgetln(f, &len))) {
		/* Trim \n. */
		if (buf[len - 1] == '\n')
			len--;
		log_debug("%s: %.*s", path, (int)len, buf);

		/* Current line is the continuation of the previous one. */
		if (line != NULL) {
			oldlen = strlen(line);
			line = xrealloc(line, 1, oldlen + len + 1);
		} else {
			oldlen = 0;
			line = xmalloc(len + 1);
		}

		/* Append current line to the previous. */
		memcpy(line + oldlen, buf, len);
		line[oldlen + len] = '\0';
		n++;

		/* Continuation: get next line? */
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\\') {
			line[len - 1] = '\0';

			/* Ignore escaped backslash at EOL. */
			if (len > 1 && line[len - 2] != '\\')
				continue;
		}
		copy = line;
		line = NULL;

		/* Skip empty lines. */
		buf = copy;
		while (isspace((u_char)*buf))
			buf++;
		if (*buf == '\0') {
			free(copy);
			continue;
		}

		/* Parse and run the command. */
		if (cmd_string_parse(buf, &cmdlist, path, n, &cause1) != 0) {
			free(copy);
			if (cause1 == NULL)
				continue;
			xasprintf(&msg, "%s:%u: %s", path, n, cause1);
			ARRAY_ADD(&cfg_causes, msg);
			free(cause1);
			continue;
		}
		free(copy);

		if (cmdlist == NULL)
			continue;
		cmdq_append(cmdq, cmdlist);
		cmd_list_free(cmdlist);
		found++;
	}
	if (line != NULL)
		free(line);
	fclose(f);

	return (found);
}

void
cfg_default_done(unused struct cmd_q *cmdq)
{
	if (--cfg_references != 0)
		return;
	cfg_finished = 1;

	if (!RB_EMPTY(&sessions))
		cfg_show_causes(RB_MIN(sessions, &sessions));

	cmdq_free(cfg_cmd_q);
	cfg_cmd_q = NULL;

	if (cfg_client != NULL) {
		/*
		 * The client command queue starts with client_exit set to 1 so
		 * only continue if not empty (that is, we have been delayed
		 * during configuration parsing for long enough that the
		 * MSG_COMMAND has arrived), else the client will exit before
		 * the MSG_COMMAND which might tell it not to.
		 */
		if (!TAILQ_EMPTY(&cfg_client->cmdq->queue))
			cmdq_continue(cfg_client->cmdq);
		cfg_client->references--;
		cfg_client = NULL;
	}
}

void
cfg_show_causes(struct session *s)
{
	struct window_pane	*wp;
	char			*cause;
	u_int			 i;

	if (s == NULL || ARRAY_EMPTY(&cfg_causes))
		return;
	wp = s->curw->window->active;

	window_pane_set_mode(wp, &window_copy_mode);
	window_copy_init_for_output(wp);
	for (i = 0; i < ARRAY_LENGTH(&cfg_causes); i++) {
		cause = ARRAY_ITEM(&cfg_causes, i);
		window_copy_add(wp, "%s", cause);
		free(cause);
	}
	ARRAY_FREE(&cfg_causes);
}
