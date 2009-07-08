/* $OpenBSD: cmd-set-option.c,v 1.3 2009/07/07 19:49:19 nicm Exp $ */

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

const struct cmd_entry cmd_set_option_entry = {
	"set-option", "set",
	CMD_OPTION_SESSION_USAGE,
	CMD_GFLAG|CMD_UFLAG,
	NULL,
	cmd_option_parse,
	cmd_set_option_exec,
	cmd_option_send,
	cmd_option_recv,
	cmd_option_free,
	cmd_option_print
};

const char *set_option_status_keys_list[] = {
	"emacs", "vi", NULL
};
const char *set_option_bell_action_list[] = {
	"none", "any", "current", NULL
};
const struct set_option_entry set_option_table[NSETOPTION] = {
	{ "bell-action", SET_OPTION_CHOICE, 0, 0, set_option_bell_action_list },
	{ "buffer-limit", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "default-command", SET_OPTION_STRING, 0, 0, NULL },
	{ "default-path", SET_OPTION_STRING, 0, 0, NULL },
	{ "display-time", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "history-limit", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "lock-after-time", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "message-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "message-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "message-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "prefix", SET_OPTION_KEY, 0, 0, NULL },
	{ "repeat-time", SET_OPTION_NUMBER, 0, SHRT_MAX, NULL },
	{ "set-remain-on-exit", SET_OPTION_FLAG, 0, 0, NULL },
	{ "set-titles", SET_OPTION_FLAG, 0, 0, NULL },
	{ "status", SET_OPTION_FLAG, 0, 0, NULL },
	{ "status-attr", SET_OPTION_ATTRIBUTES, 0, 0, NULL },
	{ "status-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-interval", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "status-keys", SET_OPTION_CHOICE, 0, 0, set_option_status_keys_list },
	{ "status-left", SET_OPTION_STRING, 0, 0, NULL },
	{ "status-left-length", SET_OPTION_NUMBER, 0, SHRT_MAX, NULL },
	{ "status-right", SET_OPTION_STRING, 0, 0, NULL },
	{ "status-right-length", SET_OPTION_NUMBER, 0, SHRT_MAX, NULL },
	{ "status-utf8", SET_OPTION_FLAG, 0, 0, NULL },
};

int
cmd_set_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_option_data		*data = self->data;
	struct session			*s;
	struct client			*c;
	struct options			*oo;
	const struct set_option_entry   *entry;
	u_int				 i;

	if (data->flags & CMD_GFLAG)
		oo = &global_s_options;
	else {
		if ((s = cmd_find_session(ctx, data->target)) == NULL)
			return (-1);
		oo = &s->options;
	}

	if (*data->option == '\0') {
		ctx->error(ctx, "invalid option");
		return (-1);
	}

	entry = NULL;
	for (i = 0; i < NSETOPTION; i++) {
		if (strncmp(set_option_table[i].name,
		    data->option, strlen(data->option)) != 0)
			continue;
		if (entry != NULL) {
			ctx->error(ctx, "ambiguous option: %s", data->option);
			return (-1);
		}
		entry = &set_option_table[i];

		/* Bail now if an exact match. */
		if (strcmp(entry->name, data->option) == 0)
			break;
	}
	if (entry == NULL) {
		ctx->error(ctx, "unknown option: %s", data->option);
		return (-1);
	}

	if (data->flags & CMD_UFLAG) {
		if (data->flags & CMD_GFLAG) {
			ctx->error(ctx,
			    "can't unset global option: %s", entry->name);
			return (-1);
		}
		if (data->value != NULL) {
			ctx->error(ctx,
			    "value passed to unset option: %s", entry->name);
			return (-1);
		}

		options_remove(oo, entry->name);
		ctx->info(ctx, "unset option: %s", entry->name);
	} else {
		switch (entry->type) {
		case SET_OPTION_STRING:
			set_option_string(ctx, oo, entry, data->value);
			break;
		case SET_OPTION_NUMBER:
			set_option_number(ctx, oo, entry, data->value);
			break;
		case SET_OPTION_KEY:
			set_option_key(ctx, oo, entry, data->value);
			break;
		case SET_OPTION_COLOUR:
			set_option_colour(ctx, oo, entry, data->value);
			break;
		case SET_OPTION_ATTRIBUTES:
			set_option_attributes(ctx, oo, entry, data->value);
			break;
		case SET_OPTION_FLAG:
			set_option_flag(ctx, oo, entry, data->value);
			break;
		case SET_OPTION_CHOICE:
			set_option_choice(ctx, oo, entry, data->value);
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
