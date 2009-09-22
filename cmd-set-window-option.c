/* $Id: cmd-set-window-option.c,v 1.39 2009-09-22 14:22:20 tcunha Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * Set a window option.
 */

int	cmd_set_window_option_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_set_window_option_entry = {
	"set-window-option", "setw",
	"[-agu] " CMD_TARGET_WINDOW_USAGE " option [value]",
	CMD_ARG12, CMD_CHFLAG('a')|CMD_CHFLAG('g')|CMD_CHFLAG('u'),
	NULL,
	cmd_target_parse,
	cmd_set_window_option_exec,
	cmd_target_free,
	cmd_target_print
};

const char *set_option_mode_keys_list[] = {
	"emacs", "vi", NULL
};
const char *set_option_clock_mode_style_list[] = {
	"12", "24", NULL
};
const struct set_option_entry set_window_option_table[] = {
	{ "aggressive-resize", SET_OPTION_FLAG, 0, 0, NULL },
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
	{ "utf8", SET_OPTION_FLAG, 0, 0, NULL },
	{ "window-status-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "window-status-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-current-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "window-status-current-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-current-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "window-status-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "xterm-keys", SET_OPTION_FLAG, 0, 0, NULL },
	{ NULL, 0, 0, 0, NULL }
};

int
cmd_set_window_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct winlink			*wl;
	struct client			*c;
	struct options			*oo;
	const struct set_option_entry   *entry, *opt;
	u_int				 i;

	if (data->chflags & CMD_CHFLAG('g'))
		oo = &global_w_options;
	else {
		if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
			return (-1);
		oo = &wl->window->options;
	}

	if (*data->arg == '\0') {
		ctx->error(ctx, "invalid option");
		return (-1);
	}

	entry = NULL;
	for (opt = set_window_option_table; opt->name != NULL; opt++) {
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

	if (data->chflags & CMD_CHFLAG('u')) {
		if (data->chflags & CMD_CHFLAG('g')) {
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
			set_option_string(ctx, oo, entry,
			    data->arg2, data->chflags & CMD_CHFLAG('a'));
			break;
		case SET_OPTION_NUMBER:
			set_option_number(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_KEYS:
			set_option_keys(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_COLOUR:
			set_option_colour(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_ATTRIBUTES:
			set_option_attributes(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_FLAG:
			set_option_flag(ctx, oo, entry, data->arg2);
			break;
		case SET_OPTION_CHOICE:
			set_option_choice(ctx, oo, entry, data->arg2);
			break;
		}
	}

	recalculate_sizes();
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL)
			server_redraw_client(c);
	}

	return (0);
}
