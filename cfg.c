/* $OpenBSD$ */

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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "tmux.h"

char		 *cfg_file;
struct cmd_q	 *cfg_cmd_q;
int		  cfg_finished;
int		  cfg_references;
char		**cfg_causes;
u_int		  cfg_ncauses;
struct client	 *cfg_client;

void	cfg_default_done(struct cmd_q *);

void
set_cfg_file(const char *path)
{
	free(cfg_file);
	cfg_file = xstrdup(path);
}

void
start_cfg(void)
{
	char		*cause = NULL;
	const char	*home;

	cfg_cmd_q = cmdq_new(NULL);
	cfg_cmd_q->emptyfn = cfg_default_done;

	cfg_finished = 0;
	cfg_references = 1;

	cfg_client = TAILQ_FIRST(&clients);
	if (cfg_client != NULL)
		cfg_client->references++;

	if (access(TMUX_CONF, R_OK) == 0) {
		if (load_cfg(TMUX_CONF, cfg_cmd_q, &cause) == -1)
			cfg_add_cause("%s: %s", TMUX_CONF, cause);
	} else if (errno != ENOENT)
		cfg_add_cause("%s: %s", TMUX_CONF, strerror(errno));

	if (cfg_file == NULL && (home = find_home()) != NULL) {
		xasprintf(&cfg_file, "%s/.tmux.conf", home);
		if (access(cfg_file, R_OK) != 0 && errno == ENOENT) {
			free(cfg_file);
			cfg_file = NULL;
		}
	}
	if (cfg_file != NULL && load_cfg(cfg_file, cfg_cmd_q, &cause) == -1)
		cfg_add_cause("%s: %s", cfg_file, cause);
	free(cause);

	cmdq_continue(cfg_cmd_q);
}

int
load_cfg(const char *path, struct cmd_q *cmdq, char **cause)
{
	FILE		*f;
	char		 delim[3] = { '\\', '\\', '\0' };
	u_int		 found;
	size_t		 line = 0;
	char		*buf, *cause1, *p;
	struct cmd_list	*cmdlist;

	log_debug("loading %s", path);
	if ((f = fopen(path, "rb")) == NULL) {
		xasprintf(cause, "%s: %s", path, strerror(errno));
		return (-1);
	}

	found = 0;
	while ((buf = fparseln(f, NULL, &line, delim, 0))) {
		log_debug("%s: %s", path, buf);

		/* Skip empty lines. */
		p = buf;
		while (isspace((u_char) *p))
			p++;
		if (*p == '\0') {
			free(buf);
			continue;
		}

		/* Parse and run the command. */
		if (cmd_string_parse(p, &cmdlist, path, line, &cause1) != 0) {
			free(buf);
			if (cause1 == NULL)
				continue;
			cfg_add_cause("%s:%zu: %s", path, line, cause1);
			free(cause1);
			continue;
		}
		free(buf);

		if (cmdlist == NULL)
			continue;
		cmdq_append(cmdq, cmdlist, NULL);
		cmd_list_free(cmdlist);
		found++;
	}
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
		server_client_unref(cfg_client);
		cfg_client = NULL;
	}
}

void
cfg_add_cause(const char *fmt, ...)
{
	va_list	 ap;
	char	*msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	cfg_ncauses++;
	cfg_causes = xreallocarray(cfg_causes, cfg_ncauses, sizeof *cfg_causes);
	cfg_causes[cfg_ncauses - 1] = msg;
}

void
cfg_print_causes(struct cmd_q *cmdq)
{
	u_int	 i;

	for (i = 0; i < cfg_ncauses; i++) {
		cmdq_print(cmdq, "%s", cfg_causes[i]);
		free(cfg_causes[i]);
	}

	free(cfg_causes);
	cfg_causes = NULL;
	cfg_ncauses = 0;
}

void
cfg_show_causes(struct session *s)
{
	struct window_pane	*wp;
	u_int			 i;

	if (s == NULL || cfg_ncauses == 0)
		return;
	wp = s->curw->window->active;

	window_pane_set_mode(wp, &window_copy_mode);
	window_copy_init_for_output(wp);
	for (i = 0; i < cfg_ncauses; i++) {
		window_copy_add(wp, "%s", cfg_causes[i]);
		free(cfg_causes[i]);
	}

	free(cfg_causes);
	cfg_causes = NULL;
	cfg_ncauses = 0;
}
