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
static struct cmd_q_list global_queue = TAILQ_HEAD_INITIALIZER(global_queue);

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
static struct cmd_q_list *
cmdq_get(struct client *c)
{
	if (c == NULL)
		return (&global_queue);
	return (&c->queue);
}

/* Append an item. */
void
cmdq_append(struct client *c, struct cmd_q *cmdq)
{
	struct cmd_q_list	*queue = cmdq_get(c);
	struct cmd_q		*next;

	do {
		next = cmdq->next;
		cmdq->next = NULL;

		if (c != NULL)
			c->references++;
		cmdq->client = c;

		cmdq->queue = queue;
		TAILQ_INSERT_TAIL(queue, cmdq, entry);

		cmdq = next;
	} while (cmdq != NULL);
}

/* Insert an item. */
void
cmdq_insert_after(struct cmd_q *after, struct cmd_q *cmdq)
{
	struct client		*c = after->client;
	struct cmd_q_list	*queue = after->queue;
	struct cmd_q		*next;

	do {
		next = cmdq->next;
		cmdq->next = NULL;

		if (c != NULL)
			c->references++;
		cmdq->client = c;

		cmdq->queue = queue;
		if (after->next != NULL)
			TAILQ_INSERT_AFTER(queue, after->next, cmdq, entry);
		else
			TAILQ_INSERT_AFTER(queue, after, cmdq, entry);
		after->next = cmdq;

		cmdq = next;
	} while (cmdq != NULL);
}

/* Remove an item. */
static void
cmdq_remove(struct cmd_q *cmdq)
{
	free((void *)cmdq->hook);

	if (cmdq->client != NULL)
		server_client_unref(cmdq->client);

	if (cmdq->type == CMD_Q_COMMAND)
		cmd_list_free(cmdq->cmdlist);

	TAILQ_REMOVE(cmdq->queue, cmdq, entry);
	free(cmdq);
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
cmdq_remove_group(struct cmd_q *cmdq)
{
	struct cmd_q	*this, *next;

	this = TAILQ_NEXT(cmdq, entry);
	while (this != NULL) {
		next = TAILQ_NEXT(this, entry);
		if (this->group == cmdq->group)
			cmdq_remove(this);
		this = next;
	}
}

/* Get a command for the command queue. */
struct cmd_q *
cmdq_get_command(struct cmd_list *cmdlist, struct cmd_find_state *current,
    struct mouse_event *m, int flags)
{
	struct cmd_q	*cmdq, *first = NULL, *last = NULL;
	struct cmd	*cmd;
	u_int		 group = cmdq_next_group();

	TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
		cmdq = xcalloc(1, sizeof *cmdq);
		cmdq->type = CMD_Q_COMMAND;
		cmdq->group = group;
		cmdq->flags = flags;

		cmdq->cmdlist = cmdlist;
		cmdq->cmd = cmd;

		if (current != NULL)
			cmd_find_copy_state(&cmdq->current, current);
		if (m != NULL)
			memcpy(&cmdq->mouse, m, sizeof cmdq->mouse);
		cmdlist->references++;

		if (first == NULL)
			first = cmdq;
		if (last != NULL)
			last->next = cmdq;
		last = cmdq;
	}
	return (first);
}

/* Fire command on command queue. */
static enum cmd_retval
cmdq_fire_command(struct cmd_q *cmdq)
{
	struct client		*c = cmdq->client;
	struct cmd		*cmd = cmdq->cmd;
	enum cmd_retval		 retval;
	const char		*name;
	struct cmd_find_state	*fsp, fs;
	int			 flags;

	flags = !!(cmd->flags & CMD_CONTROL);
	cmdq_guard(cmdq, "begin", flags);

	if (cmd_prepare_state(cmd, cmdq) != 0) {
		retval = CMD_RETURN_ERROR;
		goto out;
	}
	if (cmdq->client == NULL)
		cmdq->client = cmd_find_client(cmdq, NULL, CMD_FIND_QUIET);

	retval = cmd->entry->exec(cmd, cmdq);
	if (retval == CMD_RETURN_ERROR)
		goto out;

	if (cmd->entry->flags & CMD_AFTERHOOK) {
		name = cmd->entry->name;
		if (cmd_find_valid_state(&cmdq->state.tflag))
			fsp = &cmdq->state.tflag;
		else {
			if (cmd_find_current(&fs, cmdq, CMD_FIND_QUIET) != 0)
				goto out;
			fsp = &fs;
		}
		hooks_insert(fsp->s->hooks, cmdq, fsp, "after-%s", name);
	}

out:
	cmdq->client = c;
	if (retval == CMD_RETURN_ERROR)
		cmdq_guard(cmdq, "error", flags);
	else
		cmdq_guard(cmdq, "end", flags);
	return (retval);
}

/* Get a callback for the command queue. */
struct cmd_q *
cmdq_get_callback(cmd_q_cb cb, void *data)
{
	struct cmd_q	*cmdq;

	cmdq = xcalloc(1, sizeof *cmdq);
	cmdq->type = CMD_Q_CALLBACK;
	cmdq->group = 0;
	cmdq->flags = 0;

	cmdq->cb = cb;
	cmdq->data = data;

	return (cmdq);
}

/* Fire callback on callback queue. */
static enum cmd_retval
cmdq_fire_callback(struct cmd_q *cmdq)
{
	return (cmdq->cb(cmdq, cmdq->data));
}

/* Process next item on command queue. */
u_int
cmdq_next(struct client *c)
{
	struct cmd_q_list	*queue = cmdq_get(c);
	const char		*name = cmdq_name(c);
	struct cmd_q		*cmdq;
	enum cmd_retval		 retval;
	u_int			 items = 0;
	static u_int		 number;

	if (TAILQ_EMPTY(queue)) {
		log_debug("%s %s: empty", __func__, name);
		return (0);
	}
	if (TAILQ_FIRST(queue)->flags & CMD_Q_WAITING) {
		log_debug("%s %s: waiting", __func__, name);
		return (0);
	}

	log_debug("%s %s: enter", __func__, name);
	for (;;) {
		cmdq = TAILQ_FIRST(queue);
		if (cmdq == NULL)
			break;
		log_debug("%s %s: type %d, flags %x", __func__, name,
		    cmdq->type, cmdq->flags);

		/*
		 * Any item with the waiting flag set waits until an external
		 * event clears the flag (for example, a job - look at
		 * run-shell).
		 */
		if (cmdq->flags & CMD_Q_WAITING)
			goto waiting;

		/*
		 * Items are only fired once, once the fired flag is set, a
		 * waiting flag can only be cleared by an external event.
		 */
		if (~cmdq->flags & CMD_Q_FIRED) {
			cmdq->time = time(NULL);
			cmdq->number = ++number;

			switch (cmdq->type)
			{
			case CMD_Q_COMMAND:
				retval = cmdq_fire_command(cmdq);

				/*
				 * If a command returns an error, remove any
				 * subsequent commands in the same group.
				 */
				if (retval == CMD_RETURN_ERROR)
					cmdq_remove_group(cmdq);
				break;
			case CMD_Q_CALLBACK:
				retval = cmdq_fire_callback(cmdq);
				break;
			default:
				retval = CMD_RETURN_ERROR;
				break;
			}
			cmdq->flags |= CMD_Q_FIRED;

			if (retval == CMD_RETURN_WAIT) {
				cmdq->flags |= CMD_Q_WAITING;
				goto waiting;
			}
			items++;
		}
		cmdq_remove(cmdq);
	}

	log_debug("%s %s: exit (empty)", __func__, name);
	return (items);

waiting:
	log_debug("%s %s: exit (wait)", __func__, name);
	return (items);
}

/* Print a guard line. */
void
cmdq_guard(struct cmd_q *cmdq, const char *guard, int flags)
{
	struct client	*c = cmdq->client;

	if (c == NULL || !(c->flags & CLIENT_CONTROL))
		return;

	evbuffer_add_printf(c->stdout_data, "%%%s %ld %u %d\n", guard,
	    (long)cmdq->time, cmdq->number, flags);
	server_client_push_stdout(c);
}

/* Show message from command. */
void
cmdq_print(struct cmd_q *cmdq, const char *fmt, ...)
{
	struct client	*c = cmdq->client;
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
			window_pane_set_mode(w->active, &window_copy_mode);
			window_copy_init_for_output(w->active);
		}
		window_copy_vadd(w->active, fmt, ap);
	}

	va_end(ap);
}

/* Show error from command. */
void
cmdq_error(struct cmd_q *cmdq, const char *fmt, ...)
{
	struct client	*c = cmdq->client;
	struct cmd	*cmd = cmdq->cmd;
	va_list		 ap;
	char		*msg;
	size_t		 msglen;
	char		*tmp;

	va_start(ap, fmt);
	msglen = xvasprintf(&msg, fmt, ap);
	va_end(ap);

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
