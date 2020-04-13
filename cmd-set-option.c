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

#include <fnmatch.h>
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

	.args = { "aFgopqst:uw", 1, 2 },
	.usage = "[-aFgopqsuw] " CMD_TARGET_PANE_USAGE " option [value]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

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

const struct cmd_entry cmd_set_hook_entry = {
	.name = "set-hook",
	.alias = NULL,

	.args = { "agpRt:uw", 1, 2 },
	.usage = "[-agpRuw] " CMD_TARGET_PANE_USAGE " hook [command]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

static enum cmd_retval
cmd_set_option_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	int				 append = args_has(args, 'a');
	struct cmd_find_state		*fs = &item->target;
	struct client			*c, *loop;
	struct session			*s = fs->s;
	struct winlink			*wl = fs->wl;
	struct window			*w;
	struct window_pane		*wp;
	struct options			*oo;
	struct options_entry		*parent, *o;
	char				*name, *argument, *value = NULL, *cause;
	int				 window, idx, already, error, ambiguous;
	int				 scope;
	struct style			*sy;

	window = (cmd_get_entry(self) == &cmd_set_window_option_entry);

	/* Expand argument. */
	c = cmd_find_client(item, NULL, 1);
	argument = format_single(item, args->argv[0], c, s, wl, NULL);

	/* If set-hook -R, fire the hook straight away. */
	if (cmd_get_entry(self) == &cmd_set_hook_entry && args_has(args, 'R')) {
		notify_hook(item, argument);
		free(argument);
		return (CMD_RETURN_NORMAL);
	}

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

	/* Get the scope and table for the option .*/
	scope = options_scope_from_name(args, window, name, fs, &oo, &cause);
	if (scope == OPTIONS_TABLE_NONE) {
		if (args_has(args, 'q'))
			goto out;
		cmdq_error(item, "%s", cause);
		free(cause);
		goto fail;
	}
	o = options_get_only(oo, name);
	parent = options_get(oo, name);

	/* Check that array options and indexes match up. */
	if (idx != -1 && (*name == '@' || !options_isarray(parent))) {
		cmdq_error(item, "not an array: %s", argument);
		goto fail;
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
			if (*name == '@')
				options_remove(o);
			else if (oo == global_options ||
			    oo == global_s_options ||
			    oo == global_w_options)
				options_default(oo, options_table_entry(o));
			else
				options_remove(o);
		} else if (options_array_set(o, idx, NULL, 0, &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			goto fail;
		}
	} else if (*name == '@') {
		if (value == NULL) {
			cmdq_error(item, "empty value");
			goto fail;
		}
		options_set_string(oo, name, append, "%s", value);
	} else if (idx == -1 && !options_isarray(parent)) {
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
			if (options_array_assign(o, value, &cause) != 0) {
				cmdq_error(item, "%s", cause);
				free(cause);
				goto fail;
			}
		} else if (options_array_set(o, idx, value, append,
		    &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
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
	if (strcmp(name, "status-fg") == 0 || strcmp(name, "status-bg") == 0) {
		sy = options_get_style(oo, "status-style");
		sy->gc.fg = options_get_number(oo, "status-fg");
		sy->gc.bg = options_get_number(oo, "status-bg");
	}
	if (strcmp(name, "status-style") == 0) {
		sy = options_get_style(oo, "status-style");
		options_set_number(oo, "status-fg", sy->gc.fg);
		options_set_number(oo, "status-bg", sy->gc.bg);
	}
	if (strcmp(name, "status") == 0 ||
	    strcmp(name, "status-interval") == 0)
		status_timer_start_all();
	if (strcmp(name, "monitor-silence") == 0)
		alerts_reset_all();
	if (strcmp(name, "window-style") == 0 ||
	    strcmp(name, "window-active-style") == 0) {
		RB_FOREACH(wp, window_pane_tree, &all_window_panes)
			wp->flags |= PANE_STYLECHANGED;
	}
	if (strcmp(name, "pane-border-status") == 0) {
		RB_FOREACH(w, windows, &windows)
			layout_fix_panes(w);
	}
	RB_FOREACH(s, sessions, &sessions)
		status_update_cache(s);

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
	struct args				*args = cmd_get_args(self);
	int					 append = args_has(args, 'a');
	struct options_entry			*o;
	long long				 number;
	const char				*errstr, *new;
	char					*old;
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
		old = xstrdup(options_get_string(oo, oe->name));
		options_set_string(oo, oe->name, append, "%s", value);
		new = options_get_string(oo, oe->name);
		if (strcmp(oe->name, "default-shell") == 0 &&
		    !checkshell(new)) {
			options_set_string(oo, oe->name, 0, "%s", old);
			free(old);
			cmdq_error(item, "not a suitable shell: %s", value);
			return (-1);
		}
		if (oe->pattern != NULL && fnmatch(oe->pattern, new, 0) != 0) {
			options_set_string(oo, oe->name, 0, "%s", old);
			free(old);
			cmdq_error(item, "value is invalid: %s", value);
			return (-1);
		}
		free(old);
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
		options_set_number(oo, oe->name, number);
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
		return (0);
	case OPTIONS_TABLE_COMMAND:
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
