/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Switch window to selected layout.
 */

static enum cmd_retval	cmd_select_layout_exec(struct cmd *,
			    struct cmdq_item *);
static enum cmd_retval	cmd_select_layout_exec_multiple(struct cmdq_item *,
			    const char *);

struct cmd_select_layout_record {
	struct window			*w;
	char				*layout;
	struct layout_prepared		*prepared;
};

const struct cmd_entry cmd_select_layout_entry = {
	.name = "select-layout",
	.alias = "selectl",

	.args = { "Enopt:", 0, 1, NULL },
	.usage = "[-Enop] " CMD_TARGET_PANE_USAGE " [layout-name]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_select_layout_exec
};

const struct cmd_entry cmd_next_layout_entry = {
	.name = "next-layout",
	.alias = "nextl",

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_select_layout_exec
};

const struct cmd_entry cmd_previous_layout_entry = {
	.name = "previous-layout",
	.alias = "prevl",

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_select_layout_exec
};

static void
cmd_select_layout_free_records(struct cmd_select_layout_record *records,
    u_int nrecords)
{
	u_int	i;

	for (i = 0; i < nrecords; i++) {
		free(records[i].layout);
		layout_free_prepared(records[i].prepared);
	}
	free(records);
}

/* Apply a list of @window-id:layout records. */
static enum cmd_retval
cmd_select_layout_exec_multiple(struct cmdq_item *item, const char *input)
{
	struct cmd_select_layout_record	*records = NULL;
	struct window			*w;
	const char			*ptr = input, *idstart, *idend;
	const char			*layoutstart, *layoutend;
	char				*id, *cause, *oldlayout;
	u_int				 i, nrecords = 0;

	for (;;) {
		while (*ptr != '\0' && isspace((u_char)*ptr))
			ptr++;
		if (*ptr == '\0')
			break;
		if (*ptr != '@')
			goto invalid;

		idstart = ptr++;
		if (!isdigit((u_char)*ptr))
			goto invalid;
		while (isdigit((u_char)*ptr))
			ptr++;
		idend = ptr;
		while (*ptr != '\0' && isspace((u_char)*ptr))
			ptr++;
		if (*ptr++ != ':')
			goto invalid;

		id = xstrndup(idstart, idend - idstart);
		w = window_find_by_id_str(id);
		if (w == NULL) {
			cmdq_error(item, "unknown window: %s", id);
			free(id);
			goto fail;
		}
		free(id);
		for (i = 0; i < nrecords; i++) {
			if (records[i].w == w) {
				cmdq_error(item, "duplicate window: @%u", w->id);
				goto fail;
			}
		}

		layoutstart = ptr;
		while (*ptr != '\0' && *ptr != '@')
			ptr++;
		layoutend = ptr;
		while (layoutend != layoutstart &&
		    isspace((u_char)layoutend[-1]))
			layoutend--;
		if (layoutend == layoutstart)
			goto invalid;

		records = xreallocarray(records, nrecords + 1,
		    sizeof *records);
		records[nrecords].w = w;
		records[nrecords].layout = xstrndup(layoutstart,
		    layoutend - layoutstart);
		records[nrecords].prepared = NULL;
		nrecords++;
	}
	if (nrecords == 0)
		goto invalid;

	/* Validate every record before changing any window. */
	for (i = 0; i < nrecords; i++) {
		records[i].prepared = layout_prepare(records[i].w,
		    records[i].layout, &cause);
		if (records[i].prepared == NULL) {
			cmdq_error(item, "@%u: %s", records[i].w->id, cause);
			free(cause);
			goto fail;
		}
	}

	for (i = 0; i < nrecords; i++) {
		w = records[i].w;
		server_unzoom_window(w);
		oldlayout = w->old_layout;
		w->old_layout = layout_dump(w, w->layout_root);
		layout_apply_prepared(w, records[i].prepared);
		records[i].prepared = NULL;
		free(oldlayout);
	}
	recalculate_sizes();
	for (i = 0; i < nrecords; i++) {
		server_redraw_window(records[i].w);
		notify_window("window-layout-changed", records[i].w);
	}

	cmd_select_layout_free_records(records, nrecords);
	return (CMD_RETURN_NORMAL);

invalid:
	cmdq_error(item, "invalid multiple-window layout");
fail:
	cmd_select_layout_free_records(records, nrecords);
	return (CMD_RETURN_ERROR);
}

static enum cmd_retval
cmd_select_layout_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct window_pane	*wp = target->wp;
	struct layout_prepared	*prepared = NULL;
	const char		*layoutname;
	char			*oldlayout, *cause;
	int			 next, previous, layout;
	const char		*ptr;

	if (cmd_get_entry(self) == &cmd_select_layout_entry &&
	    args_count(args) != 0 && !args_has(args, 'E') &&
	    !args_has(args, 'n') && !args_has(args, 'o') &&
	    !args_has(args, 'p')) {
		ptr = args_string(args, 0);
		while (*ptr != '\0' && isspace((u_char)*ptr))
			ptr++;
		if (*ptr == '@')
			return (cmd_select_layout_exec_multiple(item, ptr));
		if (layout_set_lookup(ptr) == -1) {
			prepared = layout_prepare(w, ptr, &cause);
			if (prepared == NULL) {
				cmdq_error(item, "%s: %s", cause, ptr);
				free(cause);
				return (CMD_RETURN_ERROR);
			}
		}
	}

	server_unzoom_window(w);

	next = (cmd_get_entry(self) == &cmd_next_layout_entry);
	if (args_has(args, 'n'))
		next = 1;
	previous = (cmd_get_entry(self) == &cmd_previous_layout_entry);
	if (args_has(args, 'p'))
		previous = 1;

	oldlayout = w->old_layout;
	w->old_layout = layout_dump(w, w->layout_root);

	if (next || previous) {
		if (next)
			layout_set_next(w);
		else
			layout_set_previous(w);
		goto changed;
	}

	if (args_has(args, 'E')) {
		layout_spread_out(wp);
		goto changed;
	}

	if (args_count(args) != 0)
		layoutname = args_string(args, 0);
	else if (args_has(args, 'o'))
		layoutname = oldlayout;
	else
		layoutname = NULL;

	if (!args_has(args, 'o')) {
		if (layoutname == NULL)
			layout = w->lastlayout;
		else
			layout = layout_set_lookup(layoutname);
		if (layout != -1) {
			layout_set_select(w, layout);
			goto changed;
		}
	}

	if (layoutname != NULL) {
		if (prepared == NULL) {
			prepared = layout_prepare(w, layoutname, &cause);
			if (prepared == NULL) {
				cmdq_error(item, "%s: %s", cause, layoutname);
				free(cause);
				goto error;
			}
		}
		layout_apply_prepared(w, prepared);
		prepared = NULL;
		goto changed;
	}

	free(oldlayout);
	return (CMD_RETURN_NORMAL);

changed:
	free(oldlayout);
	recalculate_sizes();
	server_redraw_window(w);
	notify_window("window-layout-changed", w);
	return (CMD_RETURN_NORMAL);

error:
	layout_free_prepared(prepared);
	free(w->old_layout);
	w->old_layout = oldlayout;
	return (CMD_RETURN_ERROR);
}
