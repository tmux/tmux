/* $OpenBSD$ */

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

const char *cmd_set_option_print(
	    const struct set_option_entry *, struct options_entry *);
void	cmd_set_option_string(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *, int);
void	cmd_set_option_number(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);
void	cmd_set_option_keys(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);
void	cmd_set_option_colour(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);
void	cmd_set_option_attributes(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);
void	cmd_set_option_flag(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);
void	cmd_set_option_choice(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);

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

const char *set_option_mode_keys_list[] = {
	"emacs", "vi", NULL
};
const char *set_option_clock_mode_style_list[] = {
	"12", "24", NULL
};
const char *set_option_status_keys_list[] = {
	"emacs", "vi", NULL
};
const char *set_option_status_justify_list[] = {
	"left", "centre", "right", NULL
};
const char *set_option_bell_action_list[] = {
	"none", "any", "current", NULL
};

const struct set_option_entry set_option_table[] = {
	{ "escape-time", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "quiet", SET_OPTION_FLAG, 0, 0, NULL },
	{ NULL, 0, 0, 0, NULL }
};

const struct set_option_entry set_session_option_table[] = {
	{ "base-index", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "bell-action", SET_OPTION_CHOICE, 0, 0, set_option_bell_action_list },
	{ "buffer-limit", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "default-command", SET_OPTION_STRING, 0, 0, NULL },
	{ "default-path", SET_OPTION_STRING, 0, 0, NULL },
	{ "default-shell", SET_OPTION_STRING, 0, 0, NULL },
	{ "default-terminal", SET_OPTION_STRING, 0, 0, NULL },
	{ "display-panes-colour", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "display-panes-active-colour", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "display-panes-time", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "display-time", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "history-limit", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "lock-after-time", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "lock-command", SET_OPTION_STRING, 0, 0, NULL },
	{ "lock-server", SET_OPTION_FLAG, 0, 0, NULL },
	{ "message-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "message-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "message-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "message-limit", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "mouse-select-pane", SET_OPTION_FLAG, 0, 0, NULL },
	{ "pane-active-border-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "pane-active-border-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "pane-border-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "pane-border-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "prefix", SET_OPTION_KEYS, 0, 0, NULL },
	{ "repeat-time", SET_OPTION_NUMBER, 0, SHRT_MAX, NULL },
	{ "set-remain-on-exit", SET_OPTION_FLAG, 0, 0, NULL },
	{ "set-titles", SET_OPTION_FLAG, 0, 0, NULL },
	{ "set-titles-string", SET_OPTION_STRING, 0, 0, NULL },
	{ "status", SET_OPTION_FLAG, 0, 0, NULL },
	{ "status-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "status-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-interval", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "status-justify",
	  SET_OPTION_CHOICE, 0, 0, set_option_status_justify_list },
	{ "status-keys", SET_OPTION_CHOICE, 0, 0, set_option_status_keys_list },
	{ "status-left", SET_OPTION_STRING, 0, 0, NULL },
	{ "status-left-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "status-left-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-left-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-left-length", SET_OPTION_NUMBER, 0, SHRT_MAX, NULL },
	{ "status-right", SET_OPTION_STRING, 0, 0, NULL },
	{ "status-right-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "status-right-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-right-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-right-length", SET_OPTION_NUMBER, 0, SHRT_MAX, NULL },
	{ "status-utf8", SET_OPTION_FLAG, 0, 0, NULL },
	{ "terminal-overrides", SET_OPTION_STRING, 0, 0, NULL },
	{ "update-environment", SET_OPTION_STRING, 0, 0, NULL },
	{ "visual-activity", SET_OPTION_FLAG, 0, 0, NULL },
	{ "visual-bell", SET_OPTION_FLAG, 0, 0, NULL },
	{ "visual-content", SET_OPTION_FLAG, 0, 0, NULL },
	{ NULL, 0, 0, 0, NULL }
};

const struct set_option_entry set_window_option_table[] = {
	{ "aggressive-resize", SET_OPTION_FLAG, 0, 0, NULL },
	{ "alternate-screen", SET_OPTION_FLAG, 0, 0, NULL },
	{ "automatic-rename", SET_OPTION_FLAG, 0, 0, NULL },
	{ "clock-mode-colour", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "clock-mode-style",
	  SET_OPTION_CHOICE, 0, 0, set_option_clock_mode_style_list },
	{ "force-height", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "force-width", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "main-pane-height", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "main-pane-width", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "mode-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "mode-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "mode-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "mode-keys", SET_OPTION_CHOICE, 0, 0, set_option_mode_keys_list },
	{ "mode-mouse", SET_OPTION_FLAG, 0, 0, NULL },
	{ "monitor-activity", SET_OPTION_FLAG, 0, 0, NULL },
	{ "monitor-content", SET_OPTION_STRING, 0, 0, NULL },
	{ "remain-on-exit", SET_OPTION_FLAG, 0, 0, NULL },
	{ "synchronize-panes", SET_OPTION_FLAG, 0, 0, NULL },
	{ "utf8", SET_OPTION_FLAG, 0, 0, NULL },
	{ "window-status-alert-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "window-status-alert-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-alert-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "window-status-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-current-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "window-status-current-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-current-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-current-format", SET_OPTION_STRING, 0, 0, NULL },
	{ "window-status-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-format", SET_OPTION_STRING, 0, 0, NULL },
	{ "word-separators", SET_OPTION_STRING, 0, 0, NULL },
	{ "xterm-keys", SET_OPTION_FLAG, 0, 0, NULL },
	{ NULL, 0, 0, 0, NULL }
};

int
cmd_set_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	const struct set_option_entry	*table;
	struct session			*s;
	struct winlink			*wl;
	struct client			*c;
	struct options			*oo;
	const struct set_option_entry   *entry, *opt;
	struct jobs			*jobs;
	struct job			*job, *nextjob;
	u_int				 i;
	int				 try_again;

	if (cmd_check_flag(data->chflags, 's')) {
		oo = &global_options;
		table = set_option_table;
	} else if (cmd_check_flag(data->chflags, 'w')) {
		table = set_window_option_table;
		if (cmd_check_flag(data->chflags, 'g'))
			oo = &global_w_options;
		else {
			wl = cmd_find_window(ctx, data->target, NULL);
			if (wl == NULL)
				return (-1);
			oo = &wl->window->options;
		}
	} else {
		table = set_session_option_table;
		if (cmd_check_flag(data->chflags, 'g'))
			oo = &global_s_options;
		else {
			s = cmd_find_session(ctx, data->target);
			if (s == NULL)
				return (-1);
			oo = &s->options;
		}
	}

	if (*data->arg == '\0') {
		ctx->error(ctx, "invalid option");
		return (-1);
	}

	entry = NULL;
	for (opt = table; opt->name != NULL; opt++) {
		if (strncmp(opt->name, data->arg, strlen(data->arg)) != 0)
			continue;
		if (entry != NULL) {
			ctx->error(ctx, "ambiguous option: %s", data->arg);
			return (-1);
		}
		entry = opt;

		/* Bail now if an exact match. */
		if (strcmp(entry->name, data->arg) == 0)
			break;
	}
	if (entry == NULL) {
		ctx->error(ctx, "unknown option: %s", data->arg);
		return (-1);
	}

	if (cmd_check_flag(data->chflags, 'u')) {
		if (cmd_check_flag(data->chflags, 'g')) {
			ctx->error(ctx,
			    "can't unset global option: %s", entry->name);
			return (-1);
		}
		if (data->arg2 != NULL) {
			ctx->error(ctx,
			    "value passed to unset option: %s", entry->name);
			return (-1);
		}

		options_remove(oo, entry->name);
		ctx->info(ctx, "unset option: %s", entry->name);
	} else {
		switch (entry->type) {
		case SET_OPTION_STRING:
			cmd_set_option_string(ctx, oo, entry,
			    data->arg2, cmd_check_flag(data->chflags, 'a'));
			break;
		case SET_OPTION_NUMBER:
			cmd_set_option_number(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_KEYS:
			cmd_set_option_keys(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_COLOUR:
			cmd_set_option_colour(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_ATTRIBUTES:
			cmd_set_option_attributes(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_FLAG:
			cmd_set_option_flag(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_CHOICE:
			cmd_set_option_choice(ctx, oo, entry, data->arg2);
			break;
		}
	}

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
	if (strcmp(entry->name, "status-left") == 0 ||
	    strcmp(entry->name, "status-right") == 0 ||
	    strcmp(entry->name, "set-titles-string") == 0 ||
	    strcmp(entry->name, "window-status-format") == 0) {
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

const char *
cmd_set_option_print(
    const struct set_option_entry *entry, struct options_entry *o)
{
	static char	out[BUFSIZ];
	const char     *s;
	struct keylist *keylist;
	u_int		i;

	*out = '\0';
	switch (entry->type) {
		case SET_OPTION_STRING:
			xsnprintf(out, sizeof out, "\"%s\"", o->str);
			break;
		case SET_OPTION_NUMBER:
			xsnprintf(out, sizeof out, "%lld", o->num);
			break;
		case SET_OPTION_KEYS:
			keylist = o->data;
			for (i = 0; i < ARRAY_LENGTH(keylist); i++) {
				strlcat(out, key_string_lookup_key(
				    ARRAY_ITEM(keylist, i)), sizeof out);
				if (i != ARRAY_LENGTH(keylist) - 1)
					strlcat(out, ",", sizeof out);
			}
			break;
		case SET_OPTION_COLOUR:
			s = colour_tostring(o->num);
			xsnprintf(out, sizeof out, "%s", s);
			break;
		case SET_OPTION_ATTRIBUTES:
			s = attributes_tostring(o->num);
			xsnprintf(out, sizeof out, "%s", s);
			break;
		case SET_OPTION_FLAG:
			if (o->num)
				strlcpy(out, "on", sizeof out);
			else
				strlcpy(out, "off", sizeof out);
			break;
		case SET_OPTION_CHOICE:
			s = entry->choices[o->num];
			xsnprintf(out, sizeof out, "%s", s);
			break;
	}
	return (out);
}

void
cmd_set_option_string(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value, int append)
{
	struct options_entry	*o;
	char			*oldvalue, *newvalue;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if (append) {
		oldvalue = options_get_string(oo, entry->name);
		xasprintf(&newvalue, "%s%s", oldvalue, value);
	} else
		newvalue = value;

	o = options_set_string(oo, entry->name, "%s", newvalue);
	ctx->info(ctx,
	    "set option: %s -> %s", o->name, cmd_set_option_print(entry, o));

	if (newvalue != value)
		xfree(newvalue);
}

void
cmd_set_option_number(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	long long		 number;
	const char     		*errstr;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	number = strtonum(value, entry->minimum, entry->maximum, &errstr);
	if (errstr != NULL) {
		ctx->error(ctx, "value is %s: %s", errstr, value);
		return;
	}

	o = options_set_number(oo, entry->name, number);
	ctx->info(ctx,
	    "set option: %s -> %s", o->name, cmd_set_option_print(entry, o));
}

void
cmd_set_option_keys(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	struct keylist		*keylist;
	char			*copyvalue, *ptr, *str;
	int		 	 key;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	keylist = xmalloc(sizeof *keylist);
	ARRAY_INIT(keylist);

	ptr = copyvalue = xstrdup(value);
	while ((str = strsep(&ptr, ",")) != NULL) {
		if ((key = key_string_lookup_string(str)) == KEYC_NONE) {
			xfree(keylist);
			ctx->error(ctx, "unknown key: %s", str);
			xfree(copyvalue);
			return;
		}
		ARRAY_ADD(keylist, key);
	}
	xfree(copyvalue);

	o = options_set_data(oo, entry->name, keylist, xfree);
	ctx->info(ctx,
	    "set option: %s -> %s", o->name, cmd_set_option_print(entry, o));
}

void
cmd_set_option_colour(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	int			 colour;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((colour = colour_fromstring(value)) == -1) {
		ctx->error(ctx, "bad colour: %s", value);
		return;
	}

	o = options_set_number(oo, entry->name, colour);
	ctx->info(ctx,
	    "set option: %s -> %s", o->name, cmd_set_option_print(entry, o));
}

void
cmd_set_option_attributes(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	int			 attr;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((attr = attributes_fromstring(value)) == -1) {
		ctx->error(ctx, "bad attributes: %s", value);
		return;
	}

	o = options_set_number(oo, entry->name, attr);
	ctx->info(ctx,
	    "set option: %s -> %s", o->name, cmd_set_option_print(entry, o));
}

void
cmd_set_option_flag(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	int			 flag;

	if (value == NULL || *value == '\0')
		flag = !options_get_number(oo, entry->name);
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
			ctx->error(ctx, "bad value: %s", value);
			return;
		}
	}

	o = options_set_number(oo, entry->name, flag);
	ctx->info(ctx,
	    "set option: %s -> %s", o->name, cmd_set_option_print(entry, o));
}

void
cmd_set_option_choice(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	const char     	       **choicep;
	int		 	 n, choice = -1;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	n = 0;
	for (choicep = entry->choices; *choicep != NULL; choicep++) {
		n++;
		if (strncmp(*choicep, value, strlen(value)) != 0)
			continue;

		if (choice != -1) {
			ctx->error(ctx, "ambiguous option value: %s", value);
			return;
		}
		choice = n - 1;
	}
	if (choice == -1) {
		ctx->error(ctx, "unknown option value: %s", value);
		return;
	}

	o = options_set_number(oo, entry->name, choice);
	ctx->info(ctx,
	    "set option: %s -> %s", o->name, cmd_set_option_print(entry, o));
}
