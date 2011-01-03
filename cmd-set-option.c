/* $Id: cmd-set-option.c,v 1.104 2011-01-03 23:52:38 tcunha Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <string.h>

#include "tmux.h"

/*
 * Set an option.
 */

int	cmd_set_option_exec(struct cmd *, struct cmd_ctx *);

int	cmd_set_option_unset(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);
int	cmd_set_option_set(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);

struct options_entry *cmd_set_option_string(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);
struct options_entry *cmd_set_option_number(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);
struct options_entry *cmd_set_option_keys(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);
struct options_entry *cmd_set_option_colour(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);
struct options_entry *cmd_set_option_attributes(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);
struct options_entry *cmd_set_option_flag(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);
struct options_entry *cmd_set_option_choice(struct cmd *, struct cmd_ctx *,
	    const struct options_table_entry *, struct options *);

const struct cmd_entry cmd_set_option_entry = {
	"set-option", "set",
	"[-agsuw] [-t target-session|target-window] option [value]",
	CMD_ARG12, "agsuw",
	NULL,
	cmd_target_parse,
	cmd_set_option_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_set_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data			*data = self->data;
	const struct options_table_entry	*table, *oe, *oe_loop;
	struct session				*s;
	struct winlink				*wl;
	struct client				*c;
	struct options				*oo;
	struct jobs				*jobs;
	struct job				*job, *nextjob;
	u_int					 i;
	int					 try_again;

	/* Work out the options tree and table to use. */
	if (cmd_check_flag(data->chflags, 's')) {
		oo = &global_options;
		table = server_options_table;
	} else if (cmd_check_flag(data->chflags, 'w')) {
		table = window_options_table;
		if (cmd_check_flag(data->chflags, 'g'))
			oo = &global_w_options;
		else {
			wl = cmd_find_window(ctx, data->target, NULL);
			if (wl == NULL)
				return (-1);
			oo = &wl->window->options;
		}
	} else {
		table = session_options_table;
		if (cmd_check_flag(data->chflags, 'g'))
			oo = &global_s_options;
		else {
			s = cmd_find_session(ctx, data->target);
			if (s == NULL)
				return (-1);
			oo = &s->options;
		}
	}

	/* Find the option table entry. */
	oe = NULL;
	for (oe_loop = table; oe_loop->name != NULL; oe_loop++) {
		if (strncmp(oe_loop->name, data->arg, strlen(data->arg)) != 0)
			continue;
		if (oe != NULL) {
			ctx->error(ctx, "ambiguous option: %s", data->arg);
			return (-1);
		}
		oe = oe_loop;

		/* Bail now if an exact match. */
		if (strcmp(oe->name, data->arg) == 0)
			break;
	}
	if (oe == NULL) {
		ctx->error(ctx, "unknown option: %s", data->arg);
		return (-1);
	}

	/* Unset or set the option. */
	if (cmd_check_flag(data->chflags, 'u')) {
		if (cmd_set_option_unset(self, ctx, oe, oo) != 0)
			return (-1);
	} else {
		if (cmd_set_option_set(self, ctx, oe, oo) != 0)
			return (-1);
	}

	/* Update sizes and redraw. May not need it but meh. */
	recalculate_sizes();
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL)
			server_redraw_client(c);
	}

	/*
	 * Special-case: kill all persistent jobs if status-left, status-right
	 * or set-titles-string have changed. Persistent jobs are only used by
	 * the status line at the moment so this works XXX.
	 */
	if (strcmp(oe->name, "status-left") == 0 ||
	    strcmp(oe->name, "status-right") == 0 ||
	    strcmp(oe->name, "status") == 0 ||
	    strcmp(oe->name, "set-titles-string") == 0 ||
	    strcmp(oe->name, "window-status-format") == 0) {
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session == NULL)
				continue;

			jobs = &c->status_jobs;
			do {
				try_again = 0;
				job = RB_ROOT(jobs);
				while (job != NULL) {
					nextjob = RB_NEXT(jobs, jobs, job);
					if (job->flags & JOB_PERSIST) {
						job_remove(jobs, job);
						try_again = 1;
						break;
					}
					job = nextjob;
				}
			} while (try_again);
			server_redraw_client(c);
		}
	}

	return (0);
}

/* Unset an option. */
int
cmd_set_option_unset(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;

	if (cmd_check_flag(data->chflags, 'g')) {
		ctx->error(ctx, "can't unset global option: %s", oe->name);
		return (-1);
	}
	if (data->arg2 != NULL) {
		ctx->error(ctx, "value passed to unset option: %s", oe->name);
		return (-1);
	}

	options_remove(oo, oe->name);
	ctx->info(ctx, "unset option: %s", oe->name);
	return (0);
}

/* Set an option. */
int
cmd_set_option_set(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	struct options_entry	*o;
	const char		*s;

	if (oe->type != OPTIONS_TABLE_FLAG && data->arg2 == NULL) {
		ctx->error(ctx, "empty data->arg2");
		return (-1);
	}

	o = NULL;
	switch (oe->type) {
	case OPTIONS_TABLE_STRING:
		o = cmd_set_option_string(self, ctx, oe, oo);
		break;
	case OPTIONS_TABLE_NUMBER:
		o = cmd_set_option_number(self, ctx, oe, oo);
		break;
	case OPTIONS_TABLE_KEYS:
		o = cmd_set_option_keys(self, ctx, oe, oo);
		break;
	case OPTIONS_TABLE_COLOUR:
		o = cmd_set_option_colour(self, ctx, oe, oo);
		break;
	case OPTIONS_TABLE_ATTRIBUTES:
		o = cmd_set_option_attributes(self, ctx, oe, oo);
		break;
	case OPTIONS_TABLE_FLAG:
		o = cmd_set_option_flag(self, ctx, oe, oo);
		break;
	case OPTIONS_TABLE_CHOICE:
		o = cmd_set_option_choice(self, ctx, oe, oo);
		break;
	}
	if (o == NULL)
		return (-1);

	s = options_table_print_entry(oe, o);
	ctx->info(ctx, "set option: %s -> %s", oe->name, s);
	return (0);
}

/* Set a string option. */
struct options_entry *
cmd_set_option_string(struct cmd *self, unused struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	struct options_entry	*o;
	char			*oldval, *newval;

	if (cmd_check_flag(data->chflags, 'a')) {
		oldval = options_get_string(oo, oe->name);
		xasprintf(&newval, "%s%s", oldval, data->arg2);
	} else
		newval = data->arg2;

	o = options_set_string(oo, oe->name, "%s", newval);

	if (newval != data->arg2)
		xfree(newval);
	return (o);
}

/* Set a number option. */
struct options_entry *
cmd_set_option_number(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	long long		 ll;
	const char     		*errstr;

	ll = strtonum(data->arg2, oe->minimum, oe->maximum, &errstr);
	if (errstr != NULL) {
		ctx->error(ctx, "value is %s: %s", errstr, data->arg2);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, ll));
}

/* Set a keys option. */
struct options_entry *
cmd_set_option_keys(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	struct keylist		*keylist;
	char			*copy, *ptr, *s;
	int		 	 key;

	keylist = xmalloc(sizeof *keylist);
	ARRAY_INIT(keylist);

	ptr = copy = xstrdup(data->arg2);
	while ((s = strsep(&ptr, ",")) != NULL) {
		if ((key = key_string_lookup_string(s)) == KEYC_NONE) {
			ctx->error(ctx, "unknown key: %s", s);
			xfree(copy);
			xfree(keylist);
			return (NULL);
		}
		ARRAY_ADD(keylist, key);
	}
	xfree(copy);

	return (options_set_data(oo, oe->name, keylist, xfree));
}

/* Set a colour option. */
struct options_entry *
cmd_set_option_colour(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	int			 colour;

	if ((colour = colour_fromstring(data->arg2)) == -1) {
		ctx->error(ctx, "bad colour: %s", data->arg2);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, colour));
}

/* Set an attributes option. */
struct options_entry *
cmd_set_option_attributes(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	int			 attr;

	if ((attr = attributes_fromstring(data->arg2)) == -1) {
		ctx->error(ctx, "bad attributes: %s", data->arg2);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, attr));
}

/* Set a flag option. */
struct options_entry *
cmd_set_option_flag(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	int			 flag;

	if (data->arg2 == NULL || *data->arg2 == '\0')
		flag = !options_get_number(oo, oe->name);
	else {
		if ((data->arg2[0] == '1' && data->arg2[1] == '\0') ||
		    strcasecmp(data->arg2, "on") == 0 ||
		    strcasecmp(data->arg2, "yes") == 0)
			flag = 1;
		else if ((data->arg2[0] == '0' && data->arg2[1] == '\0') ||
		    strcasecmp(data->arg2, "off") == 0 ||
		    strcasecmp(data->arg2, "no") == 0)
			flag = 0;
		else {
			ctx->error(ctx, "bad value: %s", data->arg2);
			return (NULL);
		}
	}

	return (options_set_number(oo, oe->name, flag));
}

/* Set a choice option. */
struct options_entry *
cmd_set_option_choice(struct cmd *self, struct cmd_ctx *ctx,
    const struct options_table_entry *oe, struct options *oo)
{
	struct cmd_target_data	*data = self->data;
	const char     	       **choicep;
	int		 	 n, choice = -1;

	n = 0;
	for (choicep = oe->choices; *choicep != NULL; choicep++) {
		n++;
		if (strncmp(*choicep, data->arg2, strlen(data->arg2)) != 0)
			continue;

		if (choice != -1) {
			ctx->error(ctx, "ambiguous value: %s", data->arg2);
			return (NULL);
		}
		choice = n - 1;
	}
	if (choice == -1) {
		ctx->error(ctx, "unknown value: %s", data->arg2);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, choice));
}
