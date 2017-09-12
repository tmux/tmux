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

#include "tmux.h"

struct cfg_cond {
	size_t			line;
	int			may_meet;
	int			met;
	int			in_else;
	int			meets;
	SLIST_ENTRY(cfg_cond)	entry;
};
static SLIST_HEAD(, cfg_cond) conds = SLIST_HEAD_INITIALIZER(conds);

static char		 *cfg_file;
int			  cfg_finished;
static char		**cfg_causes;
static u_int		  cfg_ncauses;
static struct cmdq_item	 *cfg_item;

static enum cmd_retval
cfg_client_done(__unused struct cmdq_item *item, __unused void *data)
{
	if (!cfg_finished)
		return (CMD_RETURN_WAIT);
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cfg_done(__unused struct cmdq_item *item, __unused void *data)
{
	if (cfg_finished)
		return (CMD_RETURN_NORMAL);
	cfg_finished = 1;

	if (!RB_EMPTY(&sessions))
		cfg_show_causes(RB_MIN(sessions, &sessions));

	if (cfg_item != NULL)
		cfg_item->flags &= ~CMDQ_WAITING;

	status_prompt_load_history();

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
	struct client	*c;

	/*
	 * Configuration files are loaded without a client, so NULL is passed
	 * into load_cfg() and commands run in the global queue with
	 * item->client NULL.
	 *
	 * However, we must block the initial client (but just the initial
	 * client) so that its command runs after the configuration is loaded.
	 * Because start_cfg() is called so early, we can be sure the client's
	 * command queue is currently empty and our callback will be at the
	 * front - we need to get in before MSG_COMMAND.
	 */
	c = TAILQ_FIRST(&clients);
	if (c != NULL) {
		cfg_item = cmdq_get_callback(cfg_client_done, NULL);
		cmdq_append(c, cfg_item);
	}

	load_cfg(TMUX_CONF, NULL, NULL, 1);

	if (cfg_file == NULL && (home = find_home()) != NULL) {
		xasprintf(&cfg_file, "%s/.tmux.conf", home);
		quiet = 1;
	}
	if (cfg_file != NULL)
		load_cfg(cfg_file, NULL, NULL, quiet);

	cmdq_append(NULL, cmdq_get_callback(cfg_done, NULL));
}

int
load_cfg(const char *path, struct client *c, struct cmdq_item *item, int quiet)
{
	FILE			*f;
	const char		 delim[3] = { '\\', '\\', '\0' };
	u_int			 found = 0;
	size_t			 line = 0;
	char			*buf, *cause1, *p, *q, *s;
	struct cmd_list		*cmdlist;
	struct cmdq_item	*new_item;
	struct cfg_cond		*condition = NULL, *tcondition;
	enum { NA, IF, ELIF }	 if_type;
	struct format_tree	*ft;

	log_debug("loading %s", path);
	if ((f = fopen(path, "rb")) == NULL) {
		if (errno == ENOENT && quiet)
			return (0);
		cfg_add_cause("%s: %s", path, strerror(errno));
		return (-1);
	}

	while ((buf = fparseln(f, NULL, &line, delim, 0)) != NULL) {
		log_debug("%s: %s", path, buf);

		p = buf;
		while (isspace((u_char)*p))
			p++;
		if (*p == '\0') {
			free(buf);
			continue;
		}
		q = p + strlen(p) - 1;
		while (q != p && isspace((u_char)*q))
			*q-- = '\0';

		if (strncmp(p, "%if", 3) == 0 && isspace(p[3]))
			if_type = IF;
		else if (strncmp(p, "%elif", 5) == 0 && isspace(p[5]))
			if_type = ELIF;
		else
			if_type = NA;

		if (if_type != NA) {
			if (if_type == IF) {
				tcondition = condition;
				condition = xmalloc(sizeof *condition);
				condition->line = line;
				if (tcondition != NULL) {
					condition->may_meet =
					    tcondition->may_meet &&
					    tcondition->meets;
				} else {
					condition->may_meet = 1;
				}
				condition->met = 0;
				SLIST_INSERT_HEAD(&conds, condition, entry);
			} else if (condition == NULL || condition->in_else) {
				cfg_add_cause("%s:%zu: unexpected %%elif", path,
				    line);
				continue;
			}
			condition->in_else = 0;
			condition->meets = 0;

			s = p + ((if_type == IF) ? 3 : 5);
			while (isspace((u_char)*s))
				s++;
			if (condition->may_meet && !condition->met) {
				ft = format_create(NULL, NULL, FORMAT_NONE,
				    FORMAT_NOJOBS);
				s = format_expand(ft, s);
				condition->meets = (*s != '\0') &&
				    (s[0] != '0' || s[1] != '\0');
				condition->met = condition->meets;
				free(s);
				format_free(ft);
			}
			continue;
		} else if (strcmp(p, "%else") == 0) {
			if (condition == NULL || condition->in_else) {
				cfg_add_cause("%s:%zu: unexpected %%else", path,
				    line);
				continue;
			}
			condition->in_else = 1;
			condition->meets = condition->may_meet &&
			    !condition->met;
			condition->met = 1;
			continue;
		} else if (strcmp(p, "%endif") == 0) {
			if (condition == NULL) {
				cfg_add_cause("%s:%zu: unexpected %%endif",
				    path, line);
				continue;
			}
			SLIST_REMOVE_HEAD(&conds, entry);
			free(condition);
			condition = SLIST_FIRST(&conds);
			continue;
		} else if (p[0] == '%' &&
			   strncmp(p, "%error", 6) != 0) {
			cfg_add_cause("%s:%zu: unknown directive: %s", path,
			    line, p);
			continue;
		}
		if (condition != NULL && !condition->meets) {
			continue;
		}

		if (strncmp(p, "%error", 6) == 0) {
			cfg_add_cause("%s:%zu: %s", path, line, p);
			continue;
		}

		cmdlist = cmd_string_parse(p, path, line, &cause1);
		if (cmdlist == NULL) {
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

	SLIST_FOREACH_SAFE(condition, &conds, entry, tcondition) {
		cfg_add_cause("%s:%zu: unterminated %%if", path,
		    condition->line);
		free(condition);
		SLIST_REMOVE_HEAD(&conds, entry);
	}

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

	window_pane_set_mode(wp, &window_copy_mode, NULL, NULL);
	window_copy_init_for_output(wp);
	for (i = 0; i < cfg_ncauses; i++) {
		window_copy_add(wp, "%s", cfg_causes[i]);
		free(cfg_causes[i]);
	}

	free(cfg_causes);
	cfg_causes = NULL;
	cfg_ncauses = 0;
}
