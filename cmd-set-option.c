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

static int	cmd_set_option_set(struct cmd *, struct cmdq_item *,
		    struct options *, struct options_entry *, const char *);
static int	cmd_set_option_flag(struct cmdq_item *,
		    const struct options_table_entry *, struct options *,
		    const char *);
static int	cmd_set_option_choice(struct cmdq_item *,
		    const struct options_table_entry *, struct options *,
		    const char *);

const struct cmd_entry cmd_set_option_entry = {
	.name = "set-option",
	.alias = "set",

	.args = { "aFgoqst:uw", 1, 2 },
	.usage = "[-aFgosquw] [-t target-window] option [value]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

const struct cmd_entry cmd_set_window_option_entry = {
	.name = "set-window-option",
	.alias = "setw",

	.args = { "aFgoqt:u", 1, 2 },
	.usage = "[-aFgoqu] " CMD_TARGET_WINDOW_USAGE " option [value]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

static enum cmd_retval
cmd_set_option_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	int				 append = args_has(args, 'a');
	struct cmd_find_state		*fs = &item->target;
	struct client			*c, *loop;
	struct session			*s = fs->s;
	struct winlink			*wl = fs->wl;
	struct window			*w;
	enum options_table_scope	 scope;
	struct options			*oo;
	struct options_entry		*parent, *o;
	char				*name, *argument, *value = NULL, *cause;
	const char			*target;
	int				 window, idx, already, error, ambiguous;

	/* Expand argument. */
	c = cmd_find_client(item, NULL, 1);
	argument = format_single(item, args->argv[0], c, s, wl, NULL);

	/* Parse option name and index. */
	name = options_match(argument, &idx, &ambiguous);
	if (name == NULL) {
		if (args_has(args, 'q'))
			goto out;
		if (ambiguous)
			cmdq_error(item, "ambiguous option: %s", argument);
		else
			cmdq_error(item, "invalid option: %s", argument);
		goto fail;
	}
	if (args->argc < 2)
		value = NULL;
	else if (args_has(args, 'F'))
		value = format_single(item, args->argv[1], c, s, wl, NULL);
	else
		value = xstrdup(args->argv[1]);

	/*
	 * Figure out the scope: for user options it comes from the arguments,
	 * otherwise from the option name.
	 */
	if (*name == '@') {
		window = (self->entry == &cmd_set_window_option_entry);
		scope = options_scope_from_flags(args, window, fs, &oo, &cause);
	} else {
		if (options_get_only(global_options, name) != NULL)
			scope = OPTIONS_TABLE_SERVER;
		else if (options_get_only(global_s_options, name) != NULL)
			scope = OPTIONS_TABLE_SESSION;
		else if (options_get_only(global_w_options, name) != NULL)
			scope = OPTIONS_TABLE_WINDOW;
		else {
			scope = OPTIONS_TABLE_NONE;
			xasprintf(&cause, "unknown option: %s", argument);
		}
	}
	if (scope == OPTIONS_TABLE_NONE) {
		if (args_has(args, 'q'))
			goto out;
		cmdq_error(item, "%s", cause);
		free(cause);
		goto fail;
	}

	/* Which table should this option go into? */
	if (scope == OPTIONS_TABLE_SERVER)
		oo = global_options;
	else if (scope == OPTIONS_TABLE_SESSION) {
		if (args_has(self->args, 'g'))
			oo = global_s_options;
		else if (s == NULL) {
			target = args_get(args, 't');
			if (target != NULL)
				cmdq_error(item, "no such session: %s", target);
			else
				cmdq_error(item, "no current session");
			goto fail;
		} else
			oo = s->options;
	} else if (scope == OPTIONS_TABLE_WINDOW) {
		if (args_has(self->args, 'g'))
			oo = global_w_options;
		else if (wl == NULL) {
			target = args_get(args, 't');
			if (target != NULL)
				cmdq_error(item, "no such window: %s", target);
			else
				cmdq_error(item, "no current window");
			goto fail;
		} else
			oo = wl->window->options;
	}
	o = options_get_only(oo, name);
	parent = options_get(oo, name);

	/* Check that array options and indexes match up. */
	if (idx != -1) {
		if (*name == '@' || options_array_size(parent, NULL) == -1) {
			cmdq_error(item, "not an array: %s", argument);
			goto fail;
		}
	}

	/* With -o, check this option is not already set. */
	if (!args_has(args, 'u') && args_has(args, 'o')) {
		if (idx == -1)
			already = (o != NULL);
		else {
			if (o == NULL)
				already = 0;
			else
				already = (options_array_get(o, idx) != NULL);
		}
		if (already) {
			if (args_has(args, 'q'))
				goto out;
			cmdq_error(item, "already set: %s", argument);
			goto fail;
		}
	}

	/* Change the option. */
	if (args_has(args, 'u')) {
		if (o == NULL)
			goto out;
		if (idx == -1) {
			if (oo == global_options ||
			    oo == global_s_options ||
			    oo == global_w_options)
				options_default(oo, options_table_entry(o));
			else
				options_remove(o);
		} else
			options_array_set(o, idx, NULL, 0);
	} else if (*name == '@') {
		if (value == NULL) {
			cmdq_error(item, "empty value");
			goto fail;
		}
		options_set_string(oo, name, append, "%s", value);
	} else if (idx == -1 && options_array_size(parent, NULL) == -1) {
		error = cmd_set_option_set(self, item, oo, parent, value);
		if (error != 0)
			goto fail;
	} else {
		if (value == NULL) {
			cmdq_error(item, "empty value");
			goto fail;
		}
		if (o == NULL)
			o = options_empty(oo, options_table_entry(parent));
		if (idx == -1) {
			if (!append)
				options_array_clear(o);
			options_array_assign(o, value);
		} else if (options_array_set(o, idx, value, append) != 0) {
			cmdq_error(item, "invalid index: %s", argument);
			goto fail;
		}
	}

	/* Update timers and so on for various options. */
	if (strcmp(name, "automatic-rename") == 0) {
		RB_FOREACH(w, windows, &windows) {
			if (w->active == NULL)
				continue;
			if (options_get_number(w->options, "automatic-rename"))
				w->active->flags |= PANE_CHANGED;
		}
	}
	if (strcmp(name, "key-table") == 0) {
		TAILQ_FOREACH(loop, &clients, entry)
			server_client_set_key_table(loop, NULL);
	}
	if (strcmp(name, "user-keys") == 0) {
		TAILQ_FOREACH(loop, &clients, entry) {
			if (loop->tty.flags & TTY_OPENED)
				tty_keys_build(&loop->tty);
		}
	}
	if (strcmp(name, "status") == 0 ||
	    strcmp(name, "status-interval") == 0)
		status_timer_start_all();
	if (strcmp(name, "monitor-silence") == 0)
		alerts_reset_all();
	if (strcmp(name, "window-style") == 0 ||
	    strcmp(name, "window-active-style") == 0) {
		RB_FOREACH(w, windows, &windows)
			w->flags |= WINDOW_STYLECHANGED;
	}
	if (strcmp(name, "pane-border-status") == 0) {
		RB_FOREACH(w, windows, &windows)
			layout_fix_panes(w, w->sx, w->sy);
	}
	RB_FOREACH(s, sessions, &sessions)
		status_update_saved(s);

	/*
	 * Update sizes and redraw. May not always be necessary but do it
	 * anyway.
	 */
	recalculate_sizes();
	TAILQ_FOREACH(loop, &clients, entry) {
		if (loop->session != NULL)
			server_redraw_client(loop);
	}

out:
	free(argument);
	free(value);
	free(name);
	return (CMD_RETURN_NORMAL);

fail:
	free(argument);
	free(value);
	free(name);
	return (CMD_RETURN_ERROR);
}

static int
cmd_set_option_set(struct cmd *self, struct cmdq_item *item, struct options *oo,
    struct options_entry *parent, const char *value)
{
	const struct options_table_entry	*oe;
	struct args				*args = self->args;
	int					 append = args_has(args, 'a');
	struct options_entry			*o;
	long long				 number;
	const char				*errstr;
	key_code				 key;

	oe = options_table_entry(parent);
	if (value == NULL &&
	    oe->type != OPTIONS_TABLE_FLAG &&
	    oe->type != OPTIONS_TABLE_CHOICE) {
		cmdq_error(item, "empty value");
		return (-1);
	}

	switch (oe->type) {
	case OPTIONS_TABLE_STRING:
		options_set_string(oo, oe->name, append, "%s", value);
		return (0);
	case OPTIONS_TABLE_NUMBER:
		number = strtonum(value, oe->minimum, oe->maximum, &errstr);
		if (errstr != NULL) {
			cmdq_error(item, "value is %s: %s", errstr, value);
			return (-1);
		}
		options_set_number(oo, oe->name, number);
		return (0);
	case OPTIONS_TABLE_KEY:
		key = key_string_lookup_string(value);
		if (key == KEYC_UNKNOWN) {
			cmdq_error(item, "bad key: %s", value);
			return (-1);
		}
		options_set_number(oo, oe->name, key);
		return (0);
	case OPTIONS_TABLE_COLOUR:
		if ((number = colour_fromstring(value)) == -1) {
			cmdq_error(item, "bad colour: %s", value);
			return (-1);
		}
		o = options_set_number(oo, oe->name, number);
		options_style_update_new(oo, o);
		return (0);
	case OPTIONS_TABLE_ATTRIBUTES:
		if ((number = attributes_fromstring(value)) == -1) {
			cmdq_error(item, "bad attributes: %s", value);
			return (-1);
		}
		o = options_set_number(oo, oe->name, number);
		options_style_update_new(oo, o);
		return (0);
	case OPTIONS_TABLE_FLAG:
		return (cmd_set_option_flag(item, oe, oo, value));
	case OPTIONS_TABLE_CHOICE:
		return (cmd_set_option_choice(item, oe, oo, value));
	case OPTIONS_TABLE_STYLE:
		o = options_set_style(oo, oe->name, append, value);
		if (o == NULL) {
			cmdq_error(item, "bad style: %s", value);
			return (-1);
		}
		options_style_update_old(oo, o);
		return (0);
	case OPTIONS_TABLE_ARRAY:
		break;
	}
	return (-1);
}

static int
cmd_set_option_flag(struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	int	flag;

	if (value == NULL || *value == '\0')
		flag = !options_get_number(oo, oe->name);
	else if (strcmp(value, "1") == 0 ||
	    strcasecmp(value, "on") == 0 ||
	    strcasecmp(value, "yes") == 0)
		flag = 1;
	else if (strcmp(value, "0") == 0 ||
	    strcasecmp(value, "off") == 0 ||
	    strcasecmp(value, "no") == 0)
		flag = 0;
	else {
		cmdq_error(item, "bad value: %s", value);
		return (-1);
	}
	options_set_number(oo, oe->name, flag);
	return (0);
}

static int
cmd_set_option_choice(struct cmdq_item *item,
    const struct options_table_entry *oe, struct options *oo,
    const char *value)
{
	const char	**cp;
	int		  n, choice = -1;

	if (value == NULL) {
		choice = options_get_number(oo, oe->name);
		if (choice < 2)
			choice = !choice;
	} else {
		n = 0;
		for (cp = oe->choices; *cp != NULL; cp++) {
			if (strcmp(*cp, value) == 0)
				choice = n;
			n++;
		}
		if (choice == -1) {
			cmdq_error(item, "unknown value: %s", value);
			return (-1);
		}
	}
	options_set_number(oo, oe->name, choice);
	return (0);
}
