/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

char			 *cfg_file;
int			  cfg_finished;
static char		**cfg_causes;
static u_int		  cfg_ncauses;
struct client		 *cfg_client;

static enum cmd_retval
cfg_done(__unused struct cmdq_item *item, __unused void *data)
{
	if (cfg_finished)
		return (CMD_RETURN_NORMAL);
	cfg_finished = 1;

	if (!RB_EMPTY(&sessions))
		cfg_show_causes(RB_MIN(sessions, &sessions));

	if (cfg_client != NULL)
		server_client_unref(cfg_client);
	return (CMD_RETURN_NORMAL);
}

void
set_cfg_file(const char *path)
{
	free(cfg_file);
	cfg_file = xstrdup(path);
}

void
start_cfg(void)
{
	const char	*home;
	int		 quiet = 0;

	cfg_client = TAILQ_FIRST(&clients);
	if (cfg_client != NULL)
		cfg_client->references++;

	load_cfg(TMUX_CONF, cfg_client, NULL, 1);

	if (cfg_file == NULL && (home = find_home()) != NULL) {
		xasprintf(&cfg_file, "%s/.tmux.conf", home);
		quiet = 1;
	}
	if (cfg_file != NULL)
		load_cfg(cfg_file, cfg_client, NULL, quiet);

	cmdq_append(cfg_client, cmdq_get_callback(cfg_done, NULL));
}

int
load_cfg(const char *path, struct client *c, struct cmdq_item *item, int quiet)
{
	FILE			*f;
	char			 delim[3] = { '\\', '\\', '\0' };
	u_int			 found;
	size_t			 line = 0;
	char			*buf, *cause1, *p;
	struct cmd_list		*cmdlist;
	struct cmdq_item	*new_item;

	log_debug("loading %s", path);
	if ((f = fopen(path, "rb")) == NULL) {
		if (errno == ENOENT && quiet)
			return (0);
		cfg_add_cause("%s: %s", path, strerror(errno));
		return (-1);
	}

	found = 0;
	while ((buf = fparseln(f, NULL, &line, delim, 0)) != NULL) {
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
		new_item = cmdq_get_command(cmdlist, NULL, NULL, 0);
		if (item != NULL)
			cmdq_insert_after(item, new_item);
		else
			cmdq_append(c, new_item);
		cmd_list_free(cmdlist);

		found++;
	}
	fclose(f);

	return (found);
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
cfg_print_causes(struct cmdq_item *item)
{
	u_int	 i;

	for (i = 0; i < cfg_ncauses; i++) {
		cmdq_print(item, "%s", cfg_causes[i]);
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
