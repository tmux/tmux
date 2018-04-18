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

/* Condition for %if, %elif, %else and %endif. */
struct cfg_cond {
	size_t			line;		/* line number of %if */
	int			met;		/* condition was met */
	int			skip;		/* skip later %elif/%else */
	int			saw_else;	/* saw a %else */

	TAILQ_ENTRY(cfg_cond)	entry;
};
TAILQ_HEAD(cfg_conds, cfg_cond);

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

static int
cfg_check_condition(const char *path, size_t line, const char *p, int *skip)
{
	struct format_tree	*ft;
	char			*s;
	int			 result;

	while (isspace((u_char)*p))
		p++;
	if (p[0] == '\0') {
		cfg_add_cause("%s:%zu: invalid condition", path, line);
		*skip = 1;
		return (0);
	}

	ft = format_create(NULL, NULL, FORMAT_NONE, FORMAT_NOJOBS);
	s = format_expand(ft, p);
	result = format_true(s);
	free(s);
	format_free(ft);

	*skip = result;
	return (result);
}

static void
cfg_handle_if(const char *path, size_t line, struct cfg_conds *conds,
    const char *p)
{
	struct cfg_cond	*cond;
	struct cfg_cond	*parent = TAILQ_FIRST(conds);

	/*
	 * Add a new condition. If a previous condition exists and isn't
	 * currently met, this new one also can't be met.
	 */
	cond = xcalloc(1, sizeof *cond);
	cond->line = line;
	if (parent == NULL || parent->met)
		cond->met = cfg_check_condition(path, line, p, &cond->skip);
	else
		cond->skip = 1;
	cond->saw_else = 0;
	TAILQ_INSERT_HEAD(conds, cond, entry);
}

static void
cfg_handle_elif(const char *path, size_t line, struct cfg_conds *conds,
    const char *p)
{
	struct cfg_cond	*cond = TAILQ_FIRST(conds);

	/*
	 * If a previous condition exists and wasn't met, check this
	 * one instead and change the state.
	 */
	if (cond == NULL || cond->saw_else)
		cfg_add_cause("%s:%zu: unexpected %%elif", path, line);
	else if (!cond->skip)
		cond->met = cfg_check_condition(path, line, p, &cond->skip);
	else
		cond->met = 0;
}

static void
cfg_handle_else(const char *path, size_t line, struct cfg_conds *conds)
{
	struct cfg_cond	*cond = TAILQ_FIRST(conds);

	/*
	 * If a previous condition exists and wasn't met and wasn't already
	 * %else, use this one instead.
	 */
	if (cond == NULL || cond->saw_else) {
		cfg_add_cause("%s:%zu: unexpected %%else", path, line);
		return;
	}
	cond->saw_else = 1;
	cond->met = !cond->skip;
	cond->skip = 1;
}

static void
cfg_handle_endif(const char *path, size_t line, struct cfg_conds *conds)
{
	struct cfg_cond	*cond = TAILQ_FIRST(conds);

	/*
	 * Remove previous condition if one exists.
	 */
	if (cond == NULL) {
		cfg_add_cause("%s:%zu: unexpected %%endif", path, line);
		return;
	}
	TAILQ_REMOVE(conds, cond, entry);
	free(cond);
}

static void
cfg_handle_directive(const char *p, const char *path, size_t line,
    struct cfg_conds *conds)
{
	int	n = 0;

	while (p[n] != '\0' && !isspace((u_char)p[n]))
		n++;
	if (strncmp(p, "%if", n) == 0)
		cfg_handle_if(path, line, conds, p + n);
	else if (strncmp(p, "%elif", n) == 0)
		cfg_handle_elif(path, line, conds, p + n);
	else if (strcmp(p, "%else") == 0)
		cfg_handle_else(path, line, conds);
	else if (strcmp(p, "%endif") == 0)
		cfg_handle_endif(path, line, conds);
	else
		cfg_add_cause("%s:%zu: invalid directive: %s", path, line, p);
}

int
load_cfg(const char *path, struct client *c, struct cmdq_item *item, int quiet)
{
	FILE			*f;
	const char		 delim[3] = { '\\', '\\', '\0' };
	u_int			 found = 0;
	size_t			 line = 0;
	char			*buf, *cause1, *p, *q;
	struct cmd_list		*cmdlist;
	struct cmdq_item	*new_item;
	struct cfg_cond		*cond, *cond1;
	struct cfg_conds	 conds;

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

		if (*p == '%') {
			cfg_handle_directive(p, path, line, &conds);
			continue;
		}
		cond = TAILQ_FIRST(&conds);
		if (cond != NULL && !cond->met)
			continue;

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

		new_item = cmdq_get_command(cmdlist, NULL, NULL, 0);
		if (item != NULL)
			cmdq_insert_after(item, new_item);
		else
			cmdq_append(c, new_item);
		cmd_list_free(cmdlist);

		found++;
	}
	fclose(f);

	TAILQ_FOREACH_REVERSE_SAFE(cond, &conds, cfg_conds, entry, cond1) {
		cfg_add_cause("%s:%zu: unterminated %%if", path, cond->line);
		TAILQ_REMOVE(&conds, cond, entry);
		free(cond);
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
