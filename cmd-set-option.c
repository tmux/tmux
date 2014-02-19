/* $Id$ */

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

enum cmd_retval	cmd_set_option_exec(struct cmd *, struct cmd_q *);

enum cmd_retval	cmd_set_option_user(struct cmd *, struct cmd_q *,
	    const char *, const char *);

int	cmd_set_option_unset(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
int	cmd_set_option_set(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);

struct options_entry *cmd_set_option_string(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
struct options_entry *cmd_set_option_number(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
struct options_entry *cmd_set_option_key(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
struct options_entry *cmd_set_option_colour(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
struct options_entry *cmd_set_option_attributes(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
struct options_entry *cmd_set_option_flag(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
struct options_entry *cmd_set_option_choice(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);
struct options_entry *cmd_set_option_style(struct cmd *, struct cmd_q *,
	    const struct options_table_entry *, struct options *,
	    const char *);

const struct cmd_entry cmd_set_option_entry = {
	"set-option", "set",
	"agoqst:uw", 1, 2,
	"[-agosquw] [-t target-session|target-window] option [value]",
	0,
	NULL,
	cmd_set_option_exec
};

const struct cmd_entry cmd_set_window_option_entry = {
	"set-window-option", "setw",
	"agoqt:u", 1, 2,
	"[-agoqu] " CMD_TARGET_WINDOW_USAGE " option [value]",
	0,
	NULL,
	cmd_set_option_exec
};

enum cmd_retval
cmd_set_option_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args				*args = self->args;
	const struct options_table_entry	*table, *oe;
	struct session				*s;
	struct winlink				*wl;
	struct client				*c;
	struct options				*oo;
	struct window				*w;
	const char				*optstr, *valstr;
	u_int					 i;

	/* Get the option name and value. */
	optstr = args->argv[0];
	if (*optstr == '\0') {
		cmdq_error(cmdq, "invalid option");
		return (CMD_RETURN_ERROR);
	}
	if (args->argc < 2)
		valstr = NULL;
	else
		valstr = args->argv[1];

	/* Is this a user option? */
	if (*optstr == '@')
		return (cmd_set_option_user(self, cmdq, optstr, valstr));

	/* Find the option entry, try each table. */
	table = oe = NULL;
	if (options_table_find(optstr, &table, &oe) != 0) {
		cmdq_error(cmdq, "ambiguous option: %s", optstr);
		return (CMD_RETURN_ERROR);
	}
	if (oe == NULL) {
		cmdq_error(cmdq, "unknown option: %s", optstr);
		return (CMD_RETURN_ERROR);
	}

	/* Work out the tree from the table. */
	if (table == server_options_table)
		oo = &global_options;
	else if (table == window_options_table) {
		if (args_has(self->args, 'g'))
			oo = &global_w_options;
		else {
			wl = cmd_find_window(cmdq, args_get(args, 't'), NULL);
			if (wl == NULL) {
				cmdq_error(cmdq,
				    "couldn't set '%s'%s", optstr,
				    (!args_has(args, 't') && !args_has(args,
				    'g')) ? " need target window or -g" : "");
				return (CMD_RETURN_ERROR);
			}
			oo = &wl->window->options;
		}
	} else if (table == session_options_table) {
		if (args_has(self->args, 'g'))
			oo = &global_s_options;
		else {
			s = cmd_find_session(cmdq, args_get(args, 't'), 0);
			if (s == NULL) {
				cmdq_error(cmdq,
				    "couldn't set '%s'%s", optstr,
				    (!args_has(args, 't') && !args_has(args,
				    'g')) ? " need target session or -g" : "");
				return (CMD_RETURN_ERROR);
			}
			oo = &s->options;
		}
	} else {
		cmdq_error(cmdq, "unknown table");
		return (CMD_RETURN_ERROR);
	}

	/* Unset or set the option. */
	if (args_has(args, 'u')) {
		if (cmd_set_option_unset(self, cmdq, oe, oo, valstr) != 0)
			return (CMD_RETURN_ERROR);
	} else {
		if (args_has(args, 'o') && options_find1(oo, optstr) != NULL) {
			if (!args_has(args, 'q'))
				cmdq_print(cmdq, "already set: %s", optstr);
			return (CMD_RETURN_NORMAL);
		}
		if (cmd_set_option_set(self, cmdq, oe, oo, valstr) != 0)
			return (CMD_RETURN_ERROR);
	}

	/* Start or stop timers when automatic-rename changed. */
	if (strcmp(oe->name, "automatic-rename") == 0) {
		for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
			if ((w = ARRAY_ITEM(&windows, i)) == NULL)
				continue;
			if (options_get_number(&w->options, "automatic-rename"))
				queue_window_name(w);
			else if (event_initialized(&w->name_timer))
				evtimer_del(&w->name_timer);
		}
	}

	/* Update sizes and redraw. May not need it but meh. */
	recalculate_sizes();
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL)
			server_redraw_client(c);
	}

	return (CMD_RETURN_NORMAL);
}

/* Set user option. */
enum cmd_retval
cmd_set_option_user(struct cmd *self, struct cmd_q *cmdq, const char* optstr,
    const char *valstr)
{
	struct args	*args = self->args;
	struct session	*s;
	struct winlink	*wl;
	struct options	*oo;

	if (args_has(args, 's'))
		oo = &global_options;
	else if (args_has(self->args, 'w') ||
	    self->entry == &cmd_set_window_option_entry) {
		if (args_has(self->args, 'g'))
			oo = &global_w_options;
		else {
			wl = cmd_find_window(cmdq, args_get(args, 't'), NULL);
			if (wl == NULL)
				return (CMD_RETURN_ERROR);
			oo = &wl->window->options;
		}
	} else {
		if (args_has(self->args, 'g'))
			oo = &global_s_options;
		else {
			s = cmd_find_session(cmdq, args_get(args, 't'), 0);
			if (s == NULL)
				return (CMD_RETURN_ERROR);
			oo = &s->options;
		}
	}

	if (args_has(args, 'u')) {
		if (options_find1(oo, optstr) == NULL) {
			cmdq_error(cmdq, "unknown option: %s", optstr);
			return (CMD_RETURN_ERROR);
		}
		if (valstr != NULL) {
			cmdq_error(cmdq, "value passed to unset option: %s",
			    optstr);
			return (CMD_RETURN_ERROR);
		}
		options_remove(oo, optstr);
	} else {
		if (valstr == NULL) {
			cmdq_error(cmdq, "empty value");
			return (CMD_RETURN_ERROR);
		}
		if (args_has(args, 'o') && options_find1(oo, optstr) != NULL) {
			if (!args_has(args, 'q'))
				cmdq_print(cmdq, "already set: %s", optstr);
			return (CMD_RETURN_NORMAL);
		}
		options_set_string(oo, optstr, "%s", valstr);
		if (!args_has(args, 'q')) {
			cmdq_info(cmdq, "set option: %s -> %s", optstr,
			    valstr);
		}
	}
	return (CMD_RETURN_NORMAL);
}


/* Unset an option. */
int
cmd_set_option_unset(struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	struct args	*args = self->args;

	if (args_has(args, 'g')) {
		cmdq_error(cmdq, "can't unset global option: %s", oe->name);
		return (-1);
	}
	if (value != NULL) {
		cmdq_error(cmdq, "value passed to unset option: %s", oe->name);
		return (-1);
	}

	options_remove(oo, oe->name);
	if (!args_has(args, 'q'))
		cmdq_info(cmdq, "unset option: %s", oe->name);
	return (0);
}

/* Set an option. */
int
cmd_set_option_set(struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	struct args		*args = self->args;
	struct options_entry	*o;
	const char		*s;

	if (oe->type != OPTIONS_TABLE_FLAG && value == NULL) {
		cmdq_error(cmdq, "empty value");
		return (-1);
	}

	o = NULL;
	switch (oe->type) {
	case OPTIONS_TABLE_STRING:
		o = cmd_set_option_string(self, cmdq, oe, oo, value);
		break;
	case OPTIONS_TABLE_NUMBER:
		o = cmd_set_option_number(self, cmdq, oe, oo, value);
		break;
	case OPTIONS_TABLE_KEY:
		o = cmd_set_option_key(self, cmdq, oe, oo, value);
		break;
	case OPTIONS_TABLE_COLOUR:
		o = cmd_set_option_colour(self, cmdq, oe, oo, value);
		if (o != NULL)
			style_update_new(oo, o->name, oe->style);
		break;
	case OPTIONS_TABLE_ATTRIBUTES:
		o = cmd_set_option_attributes(self, cmdq, oe, oo, value);
		if (o != NULL)
			style_update_new(oo, o->name, oe->style);
		break;
	case OPTIONS_TABLE_FLAG:
		o = cmd_set_option_flag(self, cmdq, oe, oo, value);
		break;
	case OPTIONS_TABLE_CHOICE:
		o = cmd_set_option_choice(self, cmdq, oe, oo, value);
		break;
	case OPTIONS_TABLE_STYLE:
		o = cmd_set_option_style(self, cmdq, oe, oo, value);
		break;
	}
	if (o == NULL)
		return (-1);

	s = options_table_print_entry(oe, o, 0);
	if (!args_has(args, 'q'))
		cmdq_info(cmdq, "set option: %s -> %s", oe->name, s);
	return (0);
}

/* Set a string option. */
struct options_entry *
cmd_set_option_string(struct cmd *self, unused struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	struct args		*args = self->args;
	struct options_entry	*o;
	char			*oldval, *newval;

	if (args_has(args, 'a')) {
		oldval = options_get_string(oo, oe->name);
		xasprintf(&newval, "%s%s", oldval, value);
	} else
		newval = xstrdup(value);

	o = options_set_string(oo, oe->name, "%s", newval);

	free(newval);
	return (o);
}

/* Set a number option. */
struct options_entry *
cmd_set_option_number(unused struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	long long	 ll;
	const char     	*errstr;

	ll = strtonum(value, oe->minimum, oe->maximum, &errstr);
	if (errstr != NULL) {
		cmdq_error(cmdq, "value is %s: %s", errstr, value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, ll));
}

/* Set a key option. */
struct options_entry *
cmd_set_option_key(unused struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	int	key;

	if ((key = key_string_lookup_string(value)) == KEYC_NONE) {
		cmdq_error(cmdq, "bad key: %s", value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, key));
}

/* Set a colour option. */
struct options_entry *
cmd_set_option_colour(unused struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	int	colour;

	if ((colour = colour_fromstring(value)) == -1) {
		cmdq_error(cmdq, "bad colour: %s", value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, colour));
}

/* Set an attributes option. */
struct options_entry *
cmd_set_option_attributes(unused struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	int	attr;

	if ((attr = attributes_fromstring(value)) == -1) {
		cmdq_error(cmdq, "bad attributes: %s", value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, attr));
}

/* Set a flag option. */
struct options_entry *
cmd_set_option_flag(unused struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo, const char *value)
{
	int	flag;

	if (value == NULL || *value == '\0')
		flag = !options_get_number(oo, oe->name);
	else {
		if ((value[0] == '1' && value[1] == '\0') ||
		    strcasecmp(value, "on") == 0 ||
		    strcasecmp(value, "yes") == 0)
			flag = 1;
		else if ((value[0] == '0' && value[1] == '\0') ||
		    strcasecmp(value, "off") == 0 ||
		    strcasecmp(value, "no") == 0)
			flag = 0;
		else {
			cmdq_error(cmdq, "bad value: %s", value);
			return (NULL);
		}
	}

	return (options_set_number(oo, oe->name, flag));
}

/* Set a choice option. */
struct options_entry *
cmd_set_option_choice(unused struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	const char	**choicep;
	int		  n, choice = -1;

	n = 0;
	for (choicep = oe->choices; *choicep != NULL; choicep++) {
		n++;
		if (strncmp(*choicep, value, strlen(value)) != 0)
			continue;

		if (choice != -1) {
			cmdq_error(cmdq, "ambiguous value: %s", value);
			return (NULL);
		}
		choice = n - 1;
	}
	if (choice == -1) {
		cmdq_error(cmdq, "unknown value: %s", value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, choice));
}

/* Set a style option. */
struct options_entry *
cmd_set_option_style(struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	struct args		*args = self->args;
	struct options_entry	*o;
	int			 append;

	append = args_has(args, 'a');
	if ((o = options_set_style(oo, oe->name, value, append)) == NULL) {
		cmdq_error(cmdq, "bad style: %s", value);
		return (NULL);
	}

	style_update_old(oo, oe->name, &o->style);
	return (o);
}
