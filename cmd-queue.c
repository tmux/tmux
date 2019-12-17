/* $OpenBSD$ */

/*
 * Copyright (c) 2013 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <time.h>

#include "tmux.h"

/* Global command queue. */
static struct cmdq_list global_queue = TAILQ_HEAD_INITIALIZER(global_queue);

/* Get command queue name. */
static const char *
cmdq_name(struct client *c)
{
	static char	s[256];

	if (c == NULL)
		return ("<global>");
	if (c->name != NULL)
		xsnprintf(s, sizeof s, "<%s>", c->name);
	else
		xsnprintf(s, sizeof s, "<%p>", c);
	return (s);
}

/* Get command queue from client. */
static struct cmdq_list *
cmdq_get(struct client *c)
{
	if (c == NULL)
		return (&global_queue);
	return (&c->queue);
}

/* Append an item. */
void
cmdq_append(struct client *c, struct cmdq_item *item)
{
	struct cmdq_list	*queue = cmdq_get(c);
	struct cmdq_item	*next;

	do {
		next = item->next;
		item->next = NULL;

		if (c != NULL)
			c->references++;
		item->client = c;

		item->queue = queue;
		TAILQ_INSERT_TAIL(queue, item, entry);
		log_debug("%s %s: %s", __func__, cmdq_name(c), item->name);

		item = next;
	} while (item != NULL);
}

/* Insert an item. */
void
cmdq_insert_after(struct cmdq_item *after, struct cmdq_item *item)
{
	struct client		*c = after->client;
	struct cmdq_list	*queue = after->queue;
	struct cmdq_item	*next;

	do {
		next = item->next;
		item->next = after->next;
		after->next = item;

		if (c != NULL)
			c->references++;
		item->client = c;

		item->queue = queue;
		TAILQ_INSERT_AFTER(queue, after, item, entry);
		log_debug("%s %s: %s after %s", __func__, cmdq_name(c),
		    item->name, after->name);

		after = item;
		item = next;
	} while (item != NULL);
}

/* Insert a hook. */
void
cmdq_insert_hook(struct session *s, struct cmdq_item *item,
    struct cmd_find_state *fs, const char *fmt, ...)
{
	struct options			*oo;
	va_list				 ap;
	char				*name;
	struct cmdq_item		*new_item;
	struct options_entry		*o;
	struct options_array_item	*a;
	struct cmd_list			*cmdlist;

	if (item->flags & CMDQ_NOHOOKS)
		return;
	if (s == NULL)
		oo = global_s_options;
	else
		oo = s->options;

	va_start(ap, fmt);
	xvasprintf(&name, fmt, ap);
	va_end(ap);

	o = options_get(oo, name);
	if (o == NULL) {
		free(name);
		return;
	}
	log_debug("running hook %s (parent %p)", name, item);

	a = options_array_first(o);
	while (a != NULL) {
		cmdlist = options_array_item_value(a)->cmdlist;
		if (cmdlist == NULL) {
			a = options_array_next(a);
			continue;
		}

		new_item = cmdq_get_command(cmdlist, fs, NULL, CMDQ_NOHOOKS);
		cmdq_format(new_item, "hook", "%s", name);
		if (item != NULL) {
			cmdq_insert_after(item, new_item);
			item = new_item;
		} else
			cmdq_append(NULL, new_item);

		a = options_array_next(a);
	}

	free(name);
}

/* Continue processing command queue. */
void
cmdq_continue(struct cmdq_item *item)
{
	item->flags &= ~CMDQ_WAITING;
}

/* Remove an item. */
static void
cmdq_remove(struct cmdq_item *item)
{
	if (item->shared != NULL && --item->shared->references == 0) {
		if (item->shared->formats != NULL)
			format_free(item->shared->formats);
		free(item->shared);
	}

	if (item->client != NULL)
		server_client_unref(item->client);

	if (item->cmdlist != NULL)
		cmd_list_free(item->cmdlist);

	TAILQ_REMOVE(item->queue, item, entry);

	free(item->name);
	free(item);
}

/* Remove all subsequent items that match this item's group. */
static void
cmdq_remove_group(struct cmdq_item *item)
{
	struct cmdq_item	*this, *next;

	if (item->group == 0)
		return;
	this = TAILQ_NEXT(item, entry);
	while (this != NULL) {
		next = TAILQ_NEXT(this, entry);
		if (this->group == item->group)
			cmdq_remove(this);
		this = next;
	}
}

/* Get a command for the command queue. */
struct cmdq_item *
cmdq_get_command(struct cmd_list *cmdlist, struct cmd_find_state *current,
    struct mouse_event *m, int flags)
{
	struct cmdq_item	*item, *first = NULL, *last = NULL;
	struct cmd		*cmd;
	struct cmdq_shared	*shared = NULL;
	u_int			 group = 0;

	TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
		if (cmd->group != group) {
			shared = xcalloc(1, sizeof *shared);
			if (current != NULL)
				cmd_find_copy_state(&shared->current, current);
			else
				cmd_find_clear_state(&shared->current, 0);
			if (m != NULL)
				memcpy(&shared->mouse, m, sizeof shared->mouse);
			group = cmd->group;
		}

		item = xcalloc(1, sizeof *item);
		xasprintf(&item->name, "[%s/%p]", cmd->entry->name, item);
		item->type = CMDQ_COMMAND;

		item->group = cmd->group;
		item->flags = flags;

		item->shared = shared;
		item->cmdlist = cmdlist;
		item->cmd = cmd;

		log_debug("%s: %s group %u", __func__, item->name, item->group);

		shared->references++;
		cmdlist->references++;

		if (first == NULL)
			first = item;
		if (last != NULL)
			last->next = item;
		last = item;
	}
	return (first);
}

/* Fill in flag for a command. */
static enum cmd_retval
cmdq_find_flag(struct cmdq_item *item, struct cmd_find_state *fs,
    const struct cmd_entry_flag *flag)
{
	const char	*value;

	if (flag->flag == 0) {
		cmd_find_clear_state(fs, 0);
		return (CMD_RETURN_NORMAL);
	}

	value = args_get(item->cmd->args, flag->flag);
	if (cmd_find_target(fs, item, value, flag->type, flag->flags) != 0) {
		cmd_find_clear_state(fs, 0);
		return (CMD_RETURN_ERROR);
	}
	return (CMD_RETURN_NORMAL);
}

/* Fire command on command queue. */
static enum cmd_retval
cmdq_fire_command(struct cmdq_item *item)
{
	struct client		*c = item->client;
	const char		*name = cmdq_name(c);
	struct cmdq_shared	*shared = item->shared;
	struct cmd		*cmd = item->cmd;
	const struct cmd_entry	*entry = cmd->entry;
	enum cmd_retval		 retval;
	struct cmd_find_state	*fsp, fs;
	int			 flags;
	char			*tmp;

	if (log_get_level() > 1) {
		tmp = cmd_print(cmd);
		log_debug("%s %s: (%u) %s", __func__, name, item->group, tmp);
		free(tmp);
	}

	flags = !!(shared->flags & CMDQ_SHARED_CONTROL);
	cmdq_guard(item, "begin", flags);

	if (item->client == NULL)
		item->client = cmd_find_client(item, NULL, 1);
	retval = cmdq_find_flag(item, &item->source, &entry->source);
	if (retval == CMD_RETURN_ERROR)
		goto out;
	retval = cmdq_find_flag(item, &item->target, &entry->target);
	if (retval == CMD_RETURN_ERROR)
		goto out;

	retval = entry->exec(cmd, item);
	if (retval == CMD_RETURN_ERROR)
		goto out;

	if (entry->flags & CMD_AFTERHOOK) {
		if (cmd_find_valid_state(&item->target))
			fsp = &item->target;
		else if (cmd_find_valid_state(&item->shared->current))
			fsp = &item->shared->current;
		else if (cmd_find_from_client(&fs, item->client, 0) == 0)
			fsp = &fs;
		else
			goto out;
		cmdq_insert_hook(fsp->s, item, fsp, "after-%s", entry->name);
	}

out:
	item->client = c;
	if (retval == CMD_RETURN_ERROR)
		cmdq_guard(item, "error", flags);
	else
		cmdq_guard(item, "end", flags);
	return (retval);
}

/* Get a callback for the command queue. */
struct cmdq_item *
cmdq_get_callback1(const char *name, cmdq_cb cb, void *data)
{
	struct cmdq_item	*item;

	item = xcalloc(1, sizeof *item);
	xasprintf(&item->name, "[%s/%p]", name, item);
	item->type = CMDQ_CALLBACK;

	item->group = 0;
	item->flags = 0;

	item->cb = cb;
	item->data = data;

	return (item);
}

/* Generic error callback. */
static enum cmd_retval
cmdq_error_callback(struct cmdq_item *item, void *data)
{
	char	*error = data;

	cmdq_error(item, "%s", error);
	free(error);

	return (CMD_RETURN_NORMAL);
}

/* Get an error callback for the command queue. */
struct cmdq_item *
cmdq_get_error(const char *error)
{
	return (cmdq_get_callback(cmdq_error_callback, xstrdup(error)));
}

/* Fire callback on callback queue. */
static enum cmd_retval
cmdq_fire_callback(struct cmdq_item *item)
{
	return (item->cb(item, item->data));
}

/* Add a format to command queue. */
void
cmdq_format(struct cmdq_item *item, const char *key, const char *fmt, ...)
{
	struct cmdq_shared	*shared = item->shared;
	va_list			 ap;
	char			*value;

	va_start(ap, fmt);
	xvasprintf(&value, fmt, ap);
	va_end(ap);

	if (shared->formats == NULL)
		shared->formats = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_add(shared->formats, key, "%s", value);

	free(value);
}

/* Process next item on command queue. */
u_int
cmdq_next(struct client *c)
{
	struct cmdq_list	*queue = cmdq_get(c);
	const char		*name = cmdq_name(c);
	struct cmdq_item	*item;
	enum cmd_retval		 retval;
	u_int			 items = 0;
	static u_int		 number;

	if (TAILQ_EMPTY(queue)) {
		log_debug("%s %s: empty", __func__, name);
		return (0);
	}
	if (TAILQ_FIRST(queue)->flags & CMDQ_WAITING) {
		log_debug("%s %s: waiting", __func__, name);
		return (0);
	}

	log_debug("%s %s: enter", __func__, name);
	for (;;) {
		item = TAILQ_FIRST(queue);
		if (item == NULL)
			break;
		log_debug("%s %s: %s (%d), flags %x", __func__, name,
		    item->name, item->type, item->flags);

		/*
		 * Any item with the waiting flag set waits until an external
		 * event clears the flag (for example, a job - look at
		 * run-shell).
		 */
		if (item->flags & CMDQ_WAITING)
			goto waiting;

		/*
		 * Items are only fired once, once the fired flag is set, a
		 * waiting flag can only be cleared by an external event.
		 */
		if (~item->flags & CMDQ_FIRED) {
			item->time = time(NULL);
			item->number = ++number;

			switch (item->type) {
			case CMDQ_COMMAND:
				retval = cmdq_fire_command(item);

				/*
				 * If a command returns an error, remove any
				 * subsequent commands in the same group.
				 */
				if (retval == CMD_RETURN_ERROR)
					cmdq_remove_group(item);
				break;
			case CMDQ_CALLBACK:
				retval = cmdq_fire_callback(item);
				break;
			default:
				retval = CMD_RETURN_ERROR;
				break;
			}
			item->flags |= CMDQ_FIRED;

			if (retval == CMD_RETURN_WAIT) {
				item->flags |= CMDQ_WAITING;
				goto waiting;
			}
			items++;
		}
		cmdq_remove(item);
	}

	log_debug("%s %s: exit (empty)", __func__, name);
	return (items);

waiting:
	log_debug("%s %s: exit (wait)", __func__, name);
	return (items);
}

/* Print a guard line. */
void
cmdq_guard(struct cmdq_item *item, const char *guard, int flags)
{
	struct client	*c = item->client;
	long		 t = item->time;
	u_int		 number = item->number;

	if (c != NULL && (c->flags & CLIENT_CONTROL))
		file_print(c, "%%%s %ld %u %d\n", guard, t, number, flags);
}

/* Show message from command. */
void
cmdq_print(struct cmdq_item *item, const char *fmt, ...)
{
	struct client			*c = item->client;
	struct window_pane		*wp;
	struct window_mode_entry	*wme;
	va_list				 ap;
	char				*tmp, *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	log_debug("%s: %s", __func__, msg);

	if (c == NULL)
		/* nothing */;
	else if (c->session == NULL || (c->flags & CLIENT_CONTROL)) {
		if (~c->flags & CLIENT_UTF8) {
			tmp = msg;
			msg = utf8_sanitize(tmp);
			free(tmp);
		}
		file_print(c, "%s\n", msg);
	} else {
		wp = c->session->curw->window->active;
		wme = TAILQ_FIRST(&wp->modes);
		if (wme == NULL || wme->mode != &window_view_mode)
			window_pane_set_mode(wp, &window_view_mode, NULL, NULL);
		window_copy_add(wp, "%s", msg);
	}

	free(msg);
}

/* Show error from command. */
void
cmdq_error(struct cmdq_item *item, const char *fmt, ...)
{
	struct client	*c = item->client;
	struct cmd	*cmd = item->cmd;
	va_list		 ap;
	char		*msg;
	char		*tmp;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	log_debug("%s: %s", __func__, msg);

	if (c == NULL)
		cfg_add_cause("%s:%u: %s", cmd->file, cmd->line, msg);
	else if (c->session == NULL || (c->flags & CLIENT_CONTROL)) {
		if (~c->flags & CLIENT_UTF8) {
			tmp = msg;
			msg = utf8_sanitize(tmp);
			free(tmp);
		}
		file_error(c, "%s\n", msg);
		c->retval = 1;
	} else {
		*msg = toupper((u_char) *msg);
		status_message_set(c, "%s", msg);
	}

	free(msg);
}
