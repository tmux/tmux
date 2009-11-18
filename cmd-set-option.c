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

const struct cmd_entry cmd_set_option_entry = {
	"set-option", "set",
	"[-agu] " CMD_TARGET_SESSION_USAGE " option [value]",
	CMD_ARG12, "agu",
	NULL,
	cmd_target_parse,
	cmd_set_option_exec,
	cmd_target_free,
	cmd_target_print
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
	{ "base-index", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "bell-action", SET_OPTION_CHOICE, 0, 0, set_option_bell_action_list },
	{ "buffer-limit", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "default-command", SET_OPTION_STRING, 0, 0, NULL },
	{ "default-path", SET_OPTION_STRING, 0, 0, NULL },
	{ "default-shell", SET_OPTION_STRING, 0, 0, NULL },
	{ "default-terminal", SET_OPTION_STRING, 0, 0, NULL },
	{ "display-panes-colour", SET_OPTION_COLOUR, 0, 0, NULL },
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

int
cmd_set_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct session			*s;
	struct client			*c;
	struct options			*oo;
	const struct set_option_entry   *entry, *opt;
	struct jobs			*jobs;
	struct job			*job, *nextjob;
	u_int				 i;
	int				 try_again;

	if (cmd_check_flag(data->chflags, 'g'))
		oo = &global_s_options;
	else {
		if ((s = cmd_find_session(ctx, data->target)) == NULL)
			return (-1);
		oo = &s->options;
	}

	if (*data->arg == '\0') {
		ctx->error(ctx, "invalid option");
		return (-1);
	}

	entry = NULL;
	for (opt = set_option_table; opt->name != NULL; opt++) {
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
			set_option_string(ctx, oo, entry,
			    data->arg2, cmd_check_flag(data->chflags, 'a'));
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

	/* 
	 * Special-case: kill all persistent jobs if status-left, status-right
	 * or set-titles-string have changed. Persistent jobs are only used by
	 * the status line at the moment so this works XXX.
	 */
	if (strcmp(entry->name, "status-left") == 0 ||
	    strcmp(entry->name, "status-right") == 0 ||
	    strcmp(entry->name, "set-titles-string") == 0) {
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
