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

static enum cmd_retval	cmdq_continue_one(struct cmd_q *);

/* Create new command queue. */
struct cmd_q *
cmdq_new(struct client *c)
{
	struct cmd_q	*cmdq;

	cmdq = xcalloc(1, sizeof *cmdq);
	cmdq->references = 1;
	cmdq->flags = 0;

	cmdq->client = c;
	cmdq->client_exit = -1;

	TAILQ_INIT(&cmdq->queue);
	cmdq->item = NULL;
	cmdq->cmd = NULL;

	cmd_find_clear_state(&cmdq->current, NULL, 0);
	cmdq->parent = NULL;

	return (cmdq);
}

/* Free command queue */
int
cmdq_free(struct cmd_q *cmdq)
{
	if (--cmdq->references != 0) {
		if (cmdq->flags & CMD_Q_DEAD)
			return (1);
		return (0);
	}

	cmdq_flush(cmdq);
	free(cmdq);
	return (1);
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
			vasprintf(&tmp, fmt, ap);
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

/* Print a guard line. */
void
cmdq_guard(struct cmd_q *cmdq, const char *guard, int flags)
{
	struct client	*c = cmdq->client;

	if (c == NULL || !(c->flags & CLIENT_CONTROL))
		return;

	evbuffer_add_printf(c->stdout_data, "%%%s %ld %u %d\n", guard,
	    (long) cmdq->time, cmdq->number, flags);
	server_client_push_stdout(c);
}

/* Add command list to queue and begin processing if needed. */
void
cmdq_run(struct cmd_q *cmdq, struct cmd_list *cmdlist, struct mouse_event *m)
{
	cmdq_append(cmdq, cmdlist, m);

	if (cmdq->item == NULL) {
		cmdq->cmd = NULL;
		cmdq_continue(cmdq);
	}
}

/* Add command list to queue. */
void
cmdq_append(struct cmd_q *cmdq, struct cmd_list *cmdlist, struct mouse_event *m)
{
	struct cmd_q_item	*item;

	item = xcalloc(1, sizeof *item);
	item->cmdlist = cmdlist;
	TAILQ_INSERT_TAIL(&cmdq->queue, item, qentry);
	cmdlist->references++;

	if (m != NULL)
		memcpy(&item->mouse, m, sizeof item->mouse);
	else
		item->mouse.valid = 0;
}

/* Process one command. */
static enum cmd_retval
cmdq_continue_one(struct cmd_q *cmdq)
{
	struct cmd	*cmd = cmdq->cmd;
	enum cmd_retval	 retval;
	char		*tmp;
	int		 flags = !!(cmd->flags & CMD_CONTROL);

	tmp = cmd_print(cmd);
	log_debug("cmdq %p: %s", cmdq, tmp);
	free(tmp);

	cmdq->time = time(NULL);
	cmdq->number++;

	cmdq_guard(cmdq, "begin", flags);

	if (cmd_prepare_state(cmd, cmdq, NULL) != 0)
		goto error;
	retval = cmd->entry->exec(cmd, cmdq);
	if (retval == CMD_RETURN_ERROR)
		goto error;

	cmdq_guard(cmdq, "end", flags);
	return (retval);

error:
	cmdq_guard(cmdq, "error", flags);
	return (CMD_RETURN_ERROR);
}

/* Continue processing command queue. Returns 1 if finishes empty. */
int
cmdq_continue(struct cmd_q *cmdq)
{
	struct client		*c = cmdq->client;
	struct cmd_q_item	*next;
	enum cmd_retval		 retval;
	int			 empty;

	cmdq->references++;
	notify_disable();

	log_debug("continuing cmdq %p: flags %#x, client %p", cmdq, cmdq->flags,
	    c);

	empty = TAILQ_EMPTY(&cmdq->queue);
	if (empty)
		goto empty;

	if (cmdq->item == NULL) {
		cmdq->item = TAILQ_FIRST(&cmdq->queue);
		cmdq->cmd = TAILQ_FIRST(&cmdq->item->cmdlist->list);
	} else
		cmdq->cmd = TAILQ_NEXT(cmdq->cmd, qentry);

	do {
		while (cmdq->cmd != NULL) {
			retval = cmdq_continue_one(cmdq);
			if (retval == CMD_RETURN_ERROR)
				break;
			if (retval == CMD_RETURN_WAIT)
				goto out;
			if (retval == CMD_RETURN_STOP) {
				cmdq_flush(cmdq);
				goto empty;
			}
			cmdq->cmd = TAILQ_NEXT(cmdq->cmd, qentry);
		}
		next = TAILQ_NEXT(cmdq->item, qentry);

		TAILQ_REMOVE(&cmdq->queue, cmdq->item, qentry);
		cmd_list_free(cmdq->item->cmdlist);
		free(cmdq->item);

		cmdq->item = next;
		if (cmdq->item != NULL)
			cmdq->cmd = TAILQ_FIRST(&cmdq->item->cmdlist->list);
	} while (cmdq->item != NULL);

empty:
	if (cmdq->client_exit > 0)
		cmdq->client->flags |= CLIENT_EXIT;
	if (cmdq->emptyfn != NULL)
		cmdq->emptyfn(cmdq);
	empty = 1;

out:
	notify_enable();
	cmdq_free(cmdq);

	return (empty);
}

/* Flush command queue. */
void
cmdq_flush(struct cmd_q *cmdq)
{
	struct cmd_q_item	*item, *item1;

	TAILQ_FOREACH_SAFE(item, &cmdq->queue, qentry, item1) {
		TAILQ_REMOVE(&cmdq->queue, item, qentry);
		cmd_list_free(item->cmdlist);
		free(item);
	}
	cmdq->item = NULL;
}

