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
	TAILQ_ENTRY(cfg_cond)	entry;
};
TAILQ_HEAD(conds, cfg_cond);

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

static void
cfg_dir_if_helper(const char *path, size_t line, struct cfg_cond *condition,
    const char *p, const char *dirname)
{
	struct format_tree	*ft;

	while (isspace((u_char)*p))
		p++;
	if (p[0] == '\0') {
		cfg_add_cause("%s:%zu: invalid %%%s", path, line, dirname);
		condition->may_meet = 0;
		condition->met = 0;
		condition->in_else = 0;
		condition->meets = 0;
		return;
	}
	condition->in_else = 0;
	condition->meets = 0;
	if (p[0] != '\0' && condition->may_meet && !condition->met) {
		ft = format_create(NULL, NULL, FORMAT_NONE, FORMAT_NOJOBS);
		p = format_expand(ft, p);
		condition->meets = (*p != '\0') &&
		    (p[0] != '0' || p[1] != '\0');
		condition->met = condition->meets;
		free((char *)p);
		format_free(ft);
	}
}

static void
cfg_dir_if(const char *path, size_t line, struct conds *conds, const char *p)
{
	struct cfg_cond	*parent = TAILQ_FIRST(conds);
	struct cfg_cond	*new_condition;

	new_condition = xmalloc(sizeof *new_condition);
	new_condition->line = line;
	new_condition->may_meet = (parent != NULL) ?
	    (parent->may_meet && parent->meets) : 1;
	new_condition->met = 0;
	new_condition->in_else = 0;
	new_condition->meets = 0;
	TAILQ_INSERT_HEAD(conds, new_condition, entry);
	cfg_dir_if_helper(path, line, new_condition, p, "if");
}

static void
cfg_dir_elif(const char *path, size_t line, struct conds *conds, const char *p)
{
	struct cfg_cond	*condition = TAILQ_FIRST(conds);

	if (condition == NULL || condition->in_else) {
		cfg_add_cause("%s:%zu: unexpected %%elif", path, line);
		return;
	}
	cfg_dir_if_helper(path, line, condition, p, "elif");
}

static void
cfg_dir_else(const char *path, size_t line, struct conds *conds, const char *p)
{
	struct cfg_cond	*condition = TAILQ_FIRST(conds);

	if (condition == NULL || condition->in_else) {
		cfg_add_cause("%s:%zu: unexpected %%else", path, line);
		return;
	}
	condition->in_else = 1;
	while (isspace((u_char)*p))
		p++;
	if (p[0] != '\0' && p[0] != '#') {
		cfg_add_cause("%s:%zu: invalid %%else", path, line);
		condition->meets = 0;
		return;
	}
	condition->meets = condition->may_meet && !condition->met;
	condition->met = 1;
}

static void
cfg_dir_endif(const char *path, size_t line, struct conds *conds, const char *p)
{
	struct cfg_cond	*condition = TAILQ_FIRST(conds);

	if (condition == NULL) {
		cfg_add_cause("%s:%zu: unexpected %%endif", path, line);
		return;
	}
	while (isspace((u_char)*p))
		p++;
	if (p[0] != '\0' && p[0] != '#') {
		cfg_add_cause("%s:%zu: invalid %%endif", path, line);
	}
	TAILQ_REMOVE(conds, condition, entry);
	free(condition);
}

static void
cfg_dir_error(const char *path, size_t line, __unused struct conds *conds,
    const char *p)
{
	struct cfg_cond	*condition = TAILQ_FIRST(conds);

	if (condition == NULL || condition->meets) {
		cfg_add_cause("%s:%zu: %%error%s", path, line, p);
	}
}

static const struct cfg_dirs {
	const char	*name;
	void		 (*func)(const char *, size_t, struct conds *,
			     const char *);
} cfg_dirs[] = {
	{ "if",		cfg_dir_if },
	{ "elif",	cfg_dir_elif },
	{ "else",	cfg_dir_else },
	{ "endif",	cfg_dir_endif },
	{ "error",	cfg_dir_error },
};

int
load_cfg(const char *path, struct client *c, struct cmdq_item *item, int quiet)
{
	FILE			*f;
	const char		 delim[3] = { '\\', '\\', '\0' };
	u_int			 found = 0;
	size_t			 line = 0, i, n;
	char			*buf, *cause1, *p, *q;
	struct cmd_list		*cmdlist;
	struct cmdq_item	*new_item;
	struct cfg_cond		*condition, *tcondition;
	struct conds		 conds;
	const struct cfg_dirs	*dir;
	int			 valid;

	TAILQ_INIT(&conds);

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

		if (p[0] == '%') {
			p++;
			valid = 0;
			for (i = 0; i < nitems(cfg_dirs); i++) {
				dir = &cfg_dirs[i];
				n = strlen(dir->name);
				if (strncmp(p, dir->name, n) == 0 &&
				    (isspace((u_char)p[n]) || p[n] == '\0')) {
					valid = 1;
					(*dir->func)(path, line, &conds, p + n);
					break;
				}
			}
			if (!valid)
				cfg_add_cause("%s:%zu: unknown directive: %%%s",
				    path, line, p);
			continue;
		}
		condition = TAILQ_FIRST(&conds);
		if (condition != NULL && !condition->meets) {
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

	TAILQ_FOREACH_SAFE(condition, &conds, entry, tcondition) {
		cfg_add_cause("%s:%zu: unterminated %%if", path,
		    condition->line);
		free(condition);
		TAILQ_REMOVE(&conds, condition, entry);
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
