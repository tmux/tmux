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
	static char	s[32];

	if (c == NULL)
		return ("<global>");
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
		item->next = NULL;

		if (c != NULL)
			c->references++;
		item->client = c;

		item->queue = queue;
		if (after->next != NULL)
			TAILQ_INSERT_AFTER(queue, after->next, item, entry);
		else
			TAILQ_INSERT_AFTER(queue, after, item, entry);
		after->next = item;

		item = next;
	} while (item != NULL);
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

	if (item->type == CMDQ_COMMAND)
		cmd_list_free(item->cmdlist);

	TAILQ_REMOVE(item->queue, item, entry);

	free((void *)item->name);
	free(item);
}

/* Set command group. */
static u_int
cmdq_next_group(void)
{
	static u_int	group;

	return (++group);
}

/* Remove all subsequent items that match this item's group. */
static void
cmdq_remove_group(struct cmdq_item *item)
{
	struct cmdq_item	*this, *next;

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
	u_int			 group = cmdq_next_group();
	char			*tmp;
	struct cmdq_shared	*shared;

	shared = xcalloc(1, sizeof *shared);
	if (current != NULL)
		cmd_find_copy_state(&shared->current, current);
	else
		cmd_find_clear_state(&shared->current, 0);
	if (m != NULL)
		memcpy(&shared->mouse, m, sizeof shared->mouse);

	TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
		xasprintf(&tmp, "command[%s]", cmd->entry->name);

		item = xcalloc(1, sizeof *item);
		item->name = tmp;
		item->type = CMDQ_COMMAND;

		item->group = group;
		item->flags = flags;

		item->shared = shared;
		item->cmdlist = cmdlist;
		item->cmd = cmd;

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
	struct cmd		*cmd = item->cmd;
	const struct cmd_entry	*entry = cmd->entry;
	enum cmd_retval		 retval;
	struct cmd_find_state	*fsp, fs;
	int			 flags;

	flags = !!(cmd->flags & CMD_CONTROL);
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
		hooks_insert(fsp->s->hooks, item, fsp, "after-%s", entry->name);
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
	char			*tmp;

	xasprintf(&tmp, "callback[%s]", name);

	item = xcalloc(1, sizeof *item);
	item->name = tmp;
	item->type = CMDQ_CALLBACK;

	item->group = 0;
	item->flags = 0;

	item->cb = cb;
	item->data = data;

	return (item);
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

	if (c == NULL || !(c->flags & CLIENT_CONTROL))
		return;

	evbuffer_add_printf(c->stdout_data, "%%%s %ld %u %d\n", guard,
	    (long)item->time, item->number, flags);
	server_client_push_stdout(c);
}

/* Show message from command. */
void
cmdq_print(struct cmdq_item *item, const char *fmt, ...)
{
	struct client	*c = item->client;
	struct window	*w;
	va_list		 ap;
	char		*tmp, *msg;

	va_start(ap, fmt);

	if (c == NULL)
		/* nothing */;
	else if (c->session == NULL || (c->flags & CLIENT_CONTROL)) {
		if (~c->flags & CLIENT_UTF8) {
			xvasprintf(&tmp, fmt, ap);
			msg = utf8_sanitize(tmp);
			free(tmp);
			evbuffer_add(c->stdout_data, msg, strlen(msg));
			free(msg);
		} else
			evbuffer_add_vprintf(c->stdout_data, fmt, ap);
		evbuffer_add(c->stdout_data, "\n", 1);
		server_client_push_stdout(c);
	} else {
		w = c->session->curw->window;
		if (w->active->mode != &window_copy_mode) {
			window_pane_reset_mode(w->active);
			window_pane_set_mode(w->active, &window_copy_mode, NULL,
			    NULL);
			window_copy_init_for_output(w->active);
		}
		window_copy_vadd(w->active, fmt, ap);
	}

	va_end(ap);
}

/* Show error from command. */
void
cmdq_error(struct cmdq_item *item, const char *fmt, ...)
{
	struct client	*c = item->client;
	struct cmd	*cmd = item->cmd;
	va_list		 ap;
	char		*msg;
	size_t		 msglen;
	char		*tmp;

	va_start(ap, fmt);
	msglen = xvasprintf(&msg, fmt, ap);
	va_end(ap);

	log_debug("%s: %s", __func__, msg);

	if (c == NULL)
		cfg_add_cause("%s:%u: %s", cmd->file, cmd->line, msg);
	else if (c->session == NULL || (c->flags & CLIENT_CONTROL)) {
		if (~c->flags & CLIENT_UTF8) {
			tmp = msg;
			msg = utf8_sanitize(tmp);
			free(tmp);
			msglen = strlen(msg);
		}
		evbuffer_add(c->stderr_data, msg, msglen);
		evbuffer_add(c->stderr_data, "\n", 1);
		server_client_push_stderr(c);
		c->retval = 1;
	} else {
		*msg = toupper((u_char) *msg);
		status_message_set(c, "%s", msg);
	}

	free(msg);
}
