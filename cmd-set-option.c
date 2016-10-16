/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

static enum cmd_retval	cmd_set_option_exec(struct cmd *, struct cmdq_item *);

static enum cmd_retval	cmd_set_option_user(struct cmd *, struct cmdq_item *,
			    const char *, const char *);

static int	cmd_set_option_unset(struct cmd *, struct cmdq_item *,
		    const struct options_table_entry *, struct options *,
		    const char *);
static int	cmd_set_option_set(struct cmd *, struct cmdq_item *,
		    const struct options_table_entry *, struct options *,
		    const char *);

static struct options_entry *cmd_set_option_string(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);
static struct options_entry *cmd_set_option_number(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);
static struct options_entry *cmd_set_option_key(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);
static struct options_entry *cmd_set_option_colour(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);
static struct options_entry *cmd_set_option_attributes(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);
static struct options_entry *cmd_set_option_flag(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);
static struct options_entry *cmd_set_option_choice(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);
static struct options_entry *cmd_set_option_style(struct cmd *,
	    struct cmdq_item *, const struct options_table_entry *,
	    struct options *, const char *);

const struct cmd_entry cmd_set_option_entry = {
	.name = "set-option",
	.alias = "set",

	.args = { "agoqst:uw", 1, 2 },
	.usage = "[-agosquw] [-t target-window] option [value]",

	.tflag = CMD_WINDOW_CANFAIL,

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

const struct cmd_entry cmd_set_window_option_entry = {
	.name = "set-window-option",
	.alias = "setw",

	.args = { "agoqt:u", 1, 2 },
	.usage = "[-agoqu] " CMD_TARGET_WINDOW_USAGE " option [value]",

	.tflag = CMD_WINDOW_CANFAIL,

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

static enum cmd_retval
cmd_set_option_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args				*args = self->args;
	struct session				*s = item->state.tflag.s;
	struct winlink				*wl = item->state.tflag.wl;
	struct window				*w;
	struct client				*c;
	const struct options_table_entry	*oe;
	struct options				*oo;
	const char				*optstr, *valstr, *target;

	/* Get the option name and value. */
	optstr = args->argv[0];
	if (*optstr == '\0') {
		cmdq_error(item, "invalid option");
		return (CMD_RETURN_ERROR);
	}
	if (args->argc < 2)
		valstr = NULL;
	else
		valstr = args->argv[1];

	/* Is this a user option? */
	if (*optstr == '@')
		return (cmd_set_option_user(self, item, optstr, valstr));

	/* Find the option entry, try each table. */
	oe = NULL;
	if (options_table_find(optstr, &oe) != 0) {
		if (!args_has(args, 'q')) {
			cmdq_error(item, "ambiguous option: %s", optstr);
			return (CMD_RETURN_ERROR);
		}
		return (CMD_RETURN_NORMAL);
	}
	if (oe == NULL) {
		if (!args_has(args, 'q')) {
			cmdq_error(item, "unknown option: %s", optstr);
			return (CMD_RETURN_ERROR);
		}
		return (CMD_RETURN_NORMAL);
	}

	/* Work out the tree from the scope of the option. */
	if (oe->scope == OPTIONS_TABLE_SERVER)
		oo = global_options;
	else if (oe->scope == OPTIONS_TABLE_WINDOW) {
		if (args_has(self->args, 'g'))
			oo = global_w_options;
		else if (wl == NULL) {
			target = args_get(args, 't');
			if (target != NULL) {
				cmdq_error(item, "no such window: %s",
				    target);
			} else
				cmdq_error(item, "no current window");
			return (CMD_RETURN_ERROR);
		} else
			oo = wl->window->options;
	} else if (oe->scope == OPTIONS_TABLE_SESSION) {
		if (args_has(self->args, 'g'))
			oo = global_s_options;
		else if (s == NULL) {
			target = args_get(args, 't');
			if (target != NULL) {
				cmdq_error(item, "no such session: %s",
				    target);
			} else
				cmdq_error(item, "no current session");
			return (CMD_RETURN_ERROR);
		} else
			oo = s->options;
	} else {
		cmdq_error(item, "unknown table");
		return (CMD_RETURN_ERROR);
	}

	/* Unset or set the option. */
	if (args_has(args, 'u')) {
		if (cmd_set_option_unset(self, item, oe, oo, valstr) != 0)
			return (CMD_RETURN_ERROR);
	} else {
		if (args_has(args, 'o') && options_find1(oo, optstr) != NULL) {
			if (!args_has(args, 'q')) {
				cmdq_error(item, "already set: %s", optstr);
				return (CMD_RETURN_ERROR);
			}
			return (CMD_RETURN_NORMAL);
		}
		if (cmd_set_option_set(self, item, oe, oo, valstr) != 0)
			return (CMD_RETURN_ERROR);
	}

	/* Start or stop timers if necessary. */
	if (strcmp(oe->name, "automatic-rename") == 0) {
		RB_FOREACH(w, windows, &windows) {
			if (options_get_number(w->options, "automatic-rename"))
				w->active->flags |= PANE_CHANGED;
		}
	}
	if (strcmp(oe->name, "key-table") == 0) {
		TAILQ_FOREACH(c, &clients, entry)
			server_client_set_key_table(c, NULL);
	}
	if (strcmp(oe->name, "status") == 0 ||
	    strcmp(oe->name, "status-interval") == 0)
		status_timer_start_all();
	if (strcmp(oe->name, "monitor-silence") == 0)
		alerts_reset_all();
	if (strcmp(oe->name, "window-style") == 0 ||
	    strcmp(oe->name, "window-active-style") == 0) {
		RB_FOREACH(w, windows, &windows)
			w->flags |= WINDOW_STYLECHANGED;
	}

	/* When the pane-border-status option has been changed, resize panes. */
	if (strcmp(oe->name, "pane-border-status") == 0) {
		RB_FOREACH(w, windows, &windows)
			layout_fix_panes(w, w->sx, w->sy);
	}

	/* Update sizes and redraw. May not need it but meh. */
	recalculate_sizes();
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL)
			server_redraw_client(c);
	}

	return (CMD_RETURN_NORMAL);
}

/* Set user option. */
static enum cmd_retval
cmd_set_option_user(struct cmd *self, struct cmdq_item *item,
    const char *optstr, const char *valstr)
{
	struct args		*args = self->args;
	struct session		*s = item->state.tflag.s;
	struct winlink		*wl = item->state.tflag.wl;
	struct options		*oo;
	struct options_entry	*o;
	const char		*target;

	if (args_has(args, 's'))
		oo = global_options;
	else if (args_has(self->args, 'w') ||
	    self->entry == &cmd_set_window_option_entry) {
		if (args_has(self->args, 'g'))
			oo = global_w_options;
		else if (wl == NULL) {
			target = args_get(args, 't');
			if (target != NULL) {
				cmdq_error(item, "no such window: %s",
				    target);
			} else
				cmdq_error(item, "no current window");
			return (CMD_RETURN_ERROR);
		} else
			oo = wl->window->options;
	} else {
		if (args_has(self->args, 'g'))
			oo = global_s_options;
		else if (s == NULL) {
			target = args_get(args, 't');
			if (target != NULL) {
				cmdq_error(item, "no such session: %s",
				    target);
			} else
				cmdq_error(item, "no current session");
			return (CMD_RETURN_ERROR);
		} else
			oo = s->options;
	}

	if (args_has(args, 'u')) {
		if (options_find1(oo, optstr) == NULL) {
			if (!args_has(args, 'q')) {
				cmdq_error(item, "unknown option: %s", optstr);
				return (CMD_RETURN_ERROR);
			}
			return (CMD_RETURN_NORMAL);
		}
		if (valstr != NULL) {
			cmdq_error(item, "value passed to unset option: %s",
			    optstr);
			return (CMD_RETURN_ERROR);
		}
		options_remove(oo, optstr);
	} else {
		o = options_find1(oo, optstr);
		if (args_has(args, 'o') && o != NULL) {
			if (!args_has(args, 'q')) {
				cmdq_error(item, "already set: %s", optstr);
				return (CMD_RETURN_ERROR);
			}
			return (CMD_RETURN_NORMAL);
		}
		if (valstr == NULL) {
			cmdq_error(item, "empty value");
			return (CMD_RETURN_ERROR);
		}
		if (o != NULL && args_has(args, 'a'))
			options_set_string(oo, optstr, "%s%s", o->str, valstr);
		else
			options_set_string(oo, optstr, "%s", valstr);
	}
	return (CMD_RETURN_NORMAL);
}

/* Unset an option. */
static int
cmd_set_option_unset(struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	struct args	*args = self->args;

	if (value != NULL) {
		cmdq_error(item, "value passed to unset option: %s", oe->name);
		return (-1);
	}

	if (args_has(args, 'g') || oo == global_options) {
		switch (oe->type) {
		case OPTIONS_TABLE_STRING:
			options_set_string(oo, oe->name, "%s", oe->default_str);
			break;
		case OPTIONS_TABLE_STYLE:
			options_set_style(oo, oe->name, oe->default_str, 0);
			break;
		default:
			options_set_number(oo, oe->name, oe->default_num);
			break;
		}
	} else
		options_remove(oo, oe->name);
	return (0);
}

/* Set an option. */
static int
cmd_set_option_set(struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	struct options_entry	*o;

	switch (oe->type) {
	case OPTIONS_TABLE_FLAG:
	case OPTIONS_TABLE_CHOICE:
		break;
	default:
		if (value == NULL) {
			cmdq_error(item, "empty value");
			return (-1);
		}
	}

	o = NULL;
	switch (oe->type) {
	case OPTIONS_TABLE_STRING:
		o = cmd_set_option_string(self, item, oe, oo, value);
		break;
	case OPTIONS_TABLE_NUMBER:
		o = cmd_set_option_number(self, item, oe, oo, value);
		break;
	case OPTIONS_TABLE_KEY:
		o = cmd_set_option_key(self, item, oe, oo, value);
		break;
	case OPTIONS_TABLE_COLOUR:
		o = cmd_set_option_colour(self, item, oe, oo, value);
		if (o != NULL)
			style_update_new(oo, o->name, oe->style);
		break;
	case OPTIONS_TABLE_ATTRIBUTES:
		o = cmd_set_option_attributes(self, item, oe, oo, value);
		if (o != NULL)
			style_update_new(oo, o->name, oe->style);
		break;
	case OPTIONS_TABLE_FLAG:
		o = cmd_set_option_flag(self, item, oe, oo, value);
		break;
	case OPTIONS_TABLE_CHOICE:
		o = cmd_set_option_choice(self, item, oe, oo, value);
		break;
	case OPTIONS_TABLE_STYLE:
		o = cmd_set_option_style(self, item, oe, oo, value);
		break;
	}
	if (o == NULL)
		return (-1);
	return (0);
}

/* Set a string option. */
static struct options_entry *
cmd_set_option_string(struct cmd *self, __unused struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
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
static struct options_entry *
cmd_set_option_number(__unused struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	long long	 ll;
	const char     	*errstr;

	ll = strtonum(value, oe->minimum, oe->maximum, &errstr);
	if (errstr != NULL) {
		cmdq_error(item, "value is %s: %s", errstr, value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, ll));
}

/* Set a key option. */
static struct options_entry *
cmd_set_option_key(__unused struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	key_code	key;

	key = key_string_lookup_string(value);
	if (key == KEYC_UNKNOWN) {
		cmdq_error(item, "bad key: %s", value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, key));
}

/* Set a colour option. */
static struct options_entry *
cmd_set_option_colour(__unused struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	int	colour;

	if ((colour = colour_fromstring(value)) == -1) {
		cmdq_error(item, "bad colour: %s", value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, colour));
}

/* Set an attributes option. */
static struct options_entry *
cmd_set_option_attributes(__unused struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	int	attr;

	if ((attr = attributes_fromstring(value)) == -1) {
		cmdq_error(item, "bad attributes: %s", value);
		return (NULL);
	}

	return (options_set_number(oo, oe->name, attr));
}

/* Set a flag option. */
static struct options_entry *
cmd_set_option_flag(__unused struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
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
			cmdq_error(item, "bad value: %s", value);
			return (NULL);
		}
	}

	return (options_set_number(oo, oe->name, flag));
}

/* Set a choice option. */
static struct options_entry *
cmd_set_option_choice(__unused struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	const char	**choicep;
	int		  n, choice = -1;

	if (value == NULL) {
		choice = options_get_number(oo, oe->name);
		if (choice < 2)
			choice = !choice;
	} else {
		n = 0;
		for (choicep = oe->choices; *choicep != NULL; choicep++) {
			n++;
			if (strncmp(*choicep, value, strlen(value)) != 0)
				continue;

			if (choice != -1) {
				cmdq_error(item, "ambiguous value: %s", value);
				return (NULL);
			}
			choice = n - 1;
		}
		if (choice == -1) {
			cmdq_error(item, "unknown value: %s", value);
			return (NULL);
		}
	}

	return (options_set_number(oo, oe->name, choice));
}

/* Set a style option. */
static struct options_entry *
cmd_set_option_style(struct cmd *self, struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	struct args		*args = self->args;
	struct options_entry	*o;
	int			 append;

	append = args_has(args, 'a');
	if ((o = options_set_style(oo, oe->name, value, append)) == NULL) {
		cmdq_error(item, "bad style: %s", value);
		return (NULL);
	}

	style_update_old(oo, oe->name, &o->style);
	return (o);
}
