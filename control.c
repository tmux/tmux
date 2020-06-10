/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2012 George Nachman <tmux@georgester.com>
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

#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Block of data to output. Each client has one "all" queue of blocks and
 * another queue for each pane (in struct client_offset). %output blocks are
 * added to both queues and other output lines (notifications) added only to
 * the client queue.
 *
 * When a client becomes writeable, data from blocks on the pane queue are sent
 * up to the maximum size (CLIENT_BUFFER_HIGH). If a block is entirely written,
 * it is removed from both pane and client queues and if this means non-%output
 * blocks are now at the head of the client queue, they are written.
 *
 * This means a %output block holds up any subsequent non-%output blocks until
 * it is written which enforces ordering even if the client cannot accept the
 * entire block in one go.
 */
struct control_block {
	size_t				 size;
	char				*line;
	uint64_t			 t;

	TAILQ_ENTRY(control_block)	 entry;
	TAILQ_ENTRY(control_block)	 all_entry;
};

/* Control client pane. */
struct control_pane {
	u_int				 pane;

	/*
	 * Offsets into the pane data. The first (offset) is the data we have
	 * written; the second (queued) the data we have queued (pointed to by
	 * a block).
	 */
	struct window_pane_offset	 offset;
	struct window_pane_offset	 queued;

	int				 flags;
#define CONTROL_PANE_OFF 0x1
#define CONTROL_PANE_PAUSED 0x2

	int				 pending_flag;
	TAILQ_ENTRY(control_pane)	 pending_entry;

	TAILQ_HEAD(, control_block)	 blocks;

	RB_ENTRY(control_pane)		 entry;
};
RB_HEAD(control_panes, control_pane);

/* Control client state. */
struct control_state {
	struct control_panes		 panes;

	TAILQ_HEAD(, control_pane)	 pending_list;
	u_int				 pending_count;

	TAILQ_HEAD(, control_block)	 all_blocks;

	struct bufferevent		*read_event;
	struct bufferevent		*write_event;
};

/* Low watermark. */
#define CONTROL_BUFFER_LOW 512
#define CONTROL_BUFFER_HIGH 8192

/* Minimum to write to each client. */
#define CONTROL_WRITE_MINIMUM 32

/* Flags to ignore client. */
#define CONTROL_IGNORE_FLAGS \
	(CLIENT_CONTROL_NOOUTPUT| \
	 CLIENT_UNATTACHEDFLAGS)

/* Compare client panes. */
static int
control_pane_cmp(struct control_pane *cp1, struct control_pane *cp2)
{
	if (cp1->pane < cp2->pane)
		return (-1);
	if (cp1->pane > cp2->pane)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(control_panes, control_pane, entry, control_pane_cmp);

/* Free a block. */
static void
control_free_block(struct control_state *cs, struct control_block *cb)
{
	free(cb->line);
	TAILQ_REMOVE(&cs->all_blocks, cb, all_entry);
	free(cb);
}

/* Get pane offsets for this client. */
static struct control_pane *
control_get_pane(struct client *c, struct window_pane *wp)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	 cp = { .pane = wp->id };

	return (RB_FIND(control_panes, &cs->panes, &cp));
}

/* Add pane offsets for this client. */
static struct control_pane *
control_add_pane(struct client *c, struct window_pane *wp)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;

	cp = control_get_pane(c, wp);
	if (cp != NULL)
		return (cp);

	cp = xcalloc(1, sizeof *cp);
	cp->pane = wp->id;
	RB_INSERT(control_panes, &cs->panes, cp);

	memcpy(&cp->offset, &wp->offset, sizeof cp->offset);
	memcpy(&cp->queued, &wp->offset, sizeof cp->queued);
	TAILQ_INIT(&cp->blocks);

	return (cp);
}

/* Discard output for a pane. */
static void
control_discard_pane(struct client *c, struct control_pane *cp)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *cb1;

	TAILQ_FOREACH_SAFE(cb, &cp->blocks, entry, cb1) {
		TAILQ_REMOVE(&cp->blocks, cb, entry);
		control_free_block(cs, cb);
	}
}

/* Get actual pane for this client. */
static struct window_pane *
control_window_pane(struct client *c, u_int pane)
{
	struct window_pane	*wp;

	if (c->session == NULL)
		return (NULL);
	if ((wp = window_pane_find_by_id(pane)) == NULL)
		return (NULL);
	if (winlink_find_by_window(&c->session->windows, wp->window) == NULL)
		return (NULL);
	return (wp);
}

/* Reset control offsets. */
void
control_reset_offsets(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp, *cp1;

	RB_FOREACH_SAFE(cp, control_panes, &cs->panes, cp1) {
		RB_REMOVE(control_panes, &cs->panes, cp);
		free(cp);
	}

	TAILQ_INIT(&cs->pending_list);
	cs->pending_count = 0;
}

/* Get offsets for client. */
struct window_pane_offset *
control_pane_offset(struct client *c, struct window_pane *wp, int *off)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;

	if (c->flags & CLIENT_CONTROL_NOOUTPUT) {
		*off = 0;
		return (NULL);
	}

	cp = control_get_pane(c, wp);
	if (cp == NULL || (cp->flags & CONTROL_PANE_PAUSED)) {
		*off = 0;
		return (NULL);
	}
	if (cp->flags & CONTROL_PANE_OFF) {
		*off = 1;
		return (NULL);
	}
	*off = (EVBUFFER_LENGTH(cs->write_event->output) >= CONTROL_BUFFER_LOW);
	return (&cp->offset);
}

/* Set pane as on. */
void
control_set_pane_on(struct client *c, struct window_pane *wp)
{
	struct control_pane	*cp;

	cp = control_get_pane(c, wp);
	if (cp != NULL && (cp->flags & CONTROL_PANE_OFF)) {
		cp->flags &= ~CONTROL_PANE_OFF;
		memcpy(&cp->offset, &wp->offset, sizeof cp->offset);
		memcpy(&cp->queued, &wp->offset, sizeof cp->queued);
	}
}

/* Set pane as off. */
void
control_set_pane_off(struct client *c, struct window_pane *wp)
{
	struct control_pane	*cp;

	cp = control_add_pane(c, wp);
	cp->flags |= CONTROL_PANE_OFF;
}

/* Continue a paused pane. */
void
control_continue_pane(struct client *c, struct window_pane *wp)
{
	struct control_pane	*cp;

	cp = control_get_pane(c, wp);
	if (cp != NULL && (cp->flags & CONTROL_PANE_PAUSED)) {
		cp->flags &= ~CONTROL_PANE_PAUSED;
		memcpy(&cp->offset, &wp->offset, sizeof cp->offset);
		memcpy(&cp->queued, &wp->offset, sizeof cp->queued);
		control_write(c, "%%continue %%%u", wp->id);
	}
}

/* Write a line. */
static void
control_vwrite(struct client *c, const char *fmt, va_list ap)
{
	struct control_state	*cs = c->control_state;
	char			*s;

	xvasprintf(&s, fmt, ap);
	log_debug("%s: %s: writing line: %s", __func__, c->name, s);

	bufferevent_write(cs->write_event, s, strlen(s));
	bufferevent_write(cs->write_event, "\n", 1);

	bufferevent_enable(cs->write_event, EV_WRITE);
	free(s);
}

/* Write a line. */
void
control_write(struct client *c, const char *fmt, ...)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb;
	va_list			 ap;

	va_start(ap, fmt);

	if (TAILQ_EMPTY(&cs->all_blocks)) {
		control_vwrite(c, fmt, ap);
		va_end(ap);
		return;
	}

	cb = xcalloc(1, sizeof *cb);
	xvasprintf(&cb->line, fmt, ap);
	TAILQ_INSERT_TAIL(&cs->all_blocks, cb, all_entry);
	cb->t = get_timer();

	log_debug("%s: %s: storing line: %s", __func__, c->name, cb->line);
	bufferevent_enable(cs->write_event, EV_WRITE);

	va_end(ap);
}

/* Write output from a pane. */
void
control_write_output(struct client *c, struct window_pane *wp)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;
	struct control_block	*cb;
	size_t			 new_size;
	uint64_t		 t;

	if (winlink_find_by_window(&c->session->windows, wp->window) == NULL)
		return;

	if (c->flags & CONTROL_IGNORE_FLAGS) {
		cp = control_get_pane(c, wp);
		if (cp != NULL)
			goto ignore;
		return;
	}
	cp = control_add_pane(c, wp);
	if (cp->flags & (CONTROL_PANE_OFF|CONTROL_PANE_PAUSED))
		goto ignore;
	if (c->flags & CLIENT_CONTROL_PAUSEAFTER) {
		cb = TAILQ_FIRST(&cp->blocks);
		if (cb != NULL) {
			t = get_timer();
			log_debug("%s: %s: %%%u is %lld behind", __func__,
			    c->name, wp->id, (long long)t - cb->t);
			if (cb->t < t - c->pause_age) {
				cp->flags |= CONTROL_PANE_PAUSED;
				control_discard_pane(c, cp);
				control_write(c, "%%pause %%%u", wp->id);
				return;
			}
		}
	}

	window_pane_get_new_data(wp, &cp->queued, &new_size);
	if (new_size == 0)
		return;
	window_pane_update_used_data(wp, &cp->queued, new_size);

	cb = xcalloc(1, sizeof *cb);
	cb->size = new_size;
	TAILQ_INSERT_TAIL(&cs->all_blocks, cb, all_entry);
	cb->t = get_timer();

	TAILQ_INSERT_TAIL(&cp->blocks, cb, entry);
	log_debug("%s: %s: new output block of %zu for %%%u", __func__, c->name,
	    cb->size, wp->id);

	if (!cp->pending_flag) {
		log_debug("%s: %s: %%%u now pending", __func__, c->name,
		    wp->id);
		TAILQ_INSERT_TAIL(&cs->pending_list, cp, pending_entry);
		cp->pending_flag = 1;
		cs->pending_count++;
	}
	bufferevent_enable(cs->write_event, EV_WRITE);
	return;

ignore:
	log_debug("%s: %s: ignoring pane %%%u", __func__, c->name, wp->id);
	window_pane_update_used_data(wp, &cp->offset, SIZE_MAX);
	window_pane_update_used_data(wp, &cp->queued, SIZE_MAX);
}

/* Control client error callback. */
static enum cmd_retval
control_error(struct cmdq_item *item, void *data)
{
	struct client	*c = cmdq_get_client(item);
	char		*error = data;

	cmdq_guard(item, "begin", 1);
	control_write(c, "parse error: %s", error);
	cmdq_guard(item, "error", 1);

	free(error);
	return (CMD_RETURN_NORMAL);
}

/* Control client error callback. */
static void
control_error_callback(__unused struct bufferevent *bufev,
    __unused short what, void *data)
{
	struct client	*c = data;

	c->flags |= CLIENT_EXIT;
}

/* Control client input callback. Read lines and fire commands. */
static void
control_read_callback(__unused struct bufferevent *bufev, void *data)
{
	struct client		*c = data;
	struct control_state	*cs = c->control_state;
	struct evbuffer		*buffer = cs->read_event->input;
	char			*line, *error;
	struct cmdq_state	*state;
	enum cmd_parse_status	 status;

	for (;;) {
		line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_LF);
		if (line == NULL)
			break;
		log_debug("%s: %s: %s", __func__, c->name, line);
		if (*line == '\0') { /* empty line detach */
			free(line);
			c->flags |= CLIENT_EXIT;
			break;
		}

		state = cmdq_new_state(NULL, NULL, CMDQ_STATE_CONTROL);
		status = cmd_parse_and_append(line, NULL, c, state, &error);
		if (status == CMD_PARSE_ERROR)
			cmdq_append(c, cmdq_get_callback(control_error, error));
		cmdq_free_state(state);

		free(line);
	}
}

/* Does this control client have outstanding data to write? */
int
control_all_done(struct client *c)
{
	struct control_state	*cs = c->control_state;

	if (!TAILQ_EMPTY(&cs->all_blocks))
		return (0);
	return (EVBUFFER_LENGTH(cs->write_event->output) == 0);
}

/* Flush all blocks until output. */
static void
control_flush_all_blocks(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *cb1;

	TAILQ_FOREACH_SAFE(cb, &cs->all_blocks, all_entry, cb1) {
		if (cb->size != 0)
			break;
		log_debug("%s: %s: flushing line: %s", __func__, c->name,
		    cb->line);

		bufferevent_write(cs->write_event, cb->line, strlen(cb->line));
		bufferevent_write(cs->write_event, "\n", 1);
		control_free_block(cs, cb);
	}
}

/* Append data to buffer. */
static struct evbuffer *
control_append_data(struct client *c, struct control_pane *cp, uint64_t age,
    struct evbuffer *message, struct window_pane *wp, size_t size)
{
	u_char	*new_data;
	size_t	 new_size;
	u_int	 i;

	if (message == NULL) {
		message = evbuffer_new();
		if (message == NULL)
			fatalx("out of memory");
		if (c->flags & CLIENT_CONTROL_PAUSEAFTER) {
			evbuffer_add_printf(message,
			    "%%extended-output %%%u %llu : ", wp->id,
			    (unsigned long long)age);
		} else
			evbuffer_add_printf(message, "%%output %%%u ", wp->id);
	}

	new_data = window_pane_get_new_data(wp, &cp->offset, &new_size);
	if (new_size < size)
		fatalx("not enough data: %zu < %zu", new_size, size);
	for (i = 0; i < size; i++) {
		if (new_data[i] < ' ' || new_data[i] == '\\')
			evbuffer_add_printf(message, "\\%03o", new_data[i]);
		else
			evbuffer_add_printf(message, "%c", new_data[i]);
	}
	window_pane_update_used_data(wp, &cp->offset, size);
	return (message);
}

/* Write buffer. */
static void
control_write_data(struct client *c, struct evbuffer *message)
{
	struct control_state	*cs = c->control_state;

	log_debug("%s: %s: %.*s", __func__, c->name,
	    (int)EVBUFFER_LENGTH(message), EVBUFFER_DATA(message));

	evbuffer_add(message, "\n", 1);
	bufferevent_write_buffer(cs->write_event, message);
	evbuffer_free(message);
}

/* Write output to client. */
static int
control_write_pending(struct client *c, struct control_pane *cp, size_t limit)
{
	struct control_state	*cs = c->control_state;
	struct window_pane	*wp = NULL;
	struct evbuffer		*message = NULL;
	size_t			 used = 0, size;
	struct control_block	*cb, *cb1;
	uint64_t		 age, t = get_timer();

	wp = control_window_pane(c, cp->pane);
	if (wp == NULL) {
		TAILQ_FOREACH_SAFE(cb, &cp->blocks, entry, cb1) {
			TAILQ_REMOVE(&cp->blocks, cb, entry);
			control_free_block(cs, cb);
		}
		control_flush_all_blocks(c);
		return (0);
	}

	while (used != limit && !TAILQ_EMPTY(&cp->blocks)) {
		cb = TAILQ_FIRST(&cp->blocks);
		if (cb->t < t)
			age = t - cb->t;
		else
			age = 0;
		log_debug("%s: %s: output block %zu (age %llu) for %%%u "
		    "(used %zu/%zu)", __func__, c->name, cb->size, age,
		    cp->pane, used, limit);

		size = cb->size;
		if (size > limit - used)
			size = limit - used;
		used += size;

		message = control_append_data(c, cp, age, message, wp, size);

		cb->size -= size;
		if (cb->size == 0) {
			TAILQ_REMOVE(&cp->blocks, cb, entry);
			control_free_block(cs, cb);

			cb = TAILQ_FIRST(&cs->all_blocks);
			if (cb != NULL && cb->size == 0) {
				if (wp != NULL && message != NULL) {
					control_write_data(c, message);
					message = NULL;
				}
				control_flush_all_blocks(c);
			}
		}
	}
	if (message != NULL)
		control_write_data(c, message);
	return (!TAILQ_EMPTY(&cp->blocks));
}

/* Control client write callback. */
static void
control_write_callback(__unused struct bufferevent *bufev, void *data)
{
	struct client		*c = data;
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp, *cp1;
	struct evbuffer		*evb = cs->write_event->output;
	size_t			 space, limit;

	control_flush_all_blocks(c);

	while (EVBUFFER_LENGTH(evb) < CONTROL_BUFFER_HIGH) {
		if (cs->pending_count == 0)
			break;
		space = CONTROL_BUFFER_HIGH - EVBUFFER_LENGTH(evb);
		log_debug("%s: %s: %zu bytes available, %u panes", __func__,
		    c->name, space, cs->pending_count);

		limit = (space / cs->pending_count / 3); /* 3 bytes for \xxx */
		if (limit < CONTROL_WRITE_MINIMUM)
			limit = CONTROL_WRITE_MINIMUM;

		TAILQ_FOREACH_SAFE(cp, &cs->pending_list, pending_entry, cp1) {
			if (EVBUFFER_LENGTH(evb) >= CONTROL_BUFFER_HIGH)
				break;
			if (control_write_pending(c, cp, limit))
				continue;
			TAILQ_REMOVE(&cs->pending_list, cp, pending_entry);
			cp->pending_flag = 0;
			cs->pending_count--;
		}
	}
	if (EVBUFFER_LENGTH(evb) == 0)
		bufferevent_disable(cs->write_event, EV_WRITE);
}

/* Initialize for control mode. */
void
control_start(struct client *c)
{
	struct control_state	*cs;

	if (c->flags & CLIENT_CONTROLCONTROL) {
		close(c->out_fd);
		c->out_fd = -1;
	} else
		setblocking(c->out_fd, 0);
	setblocking(c->fd, 0);

	cs = c->control_state = xcalloc(1, sizeof *cs);
	RB_INIT(&cs->panes);
	TAILQ_INIT(&cs->pending_list);
	TAILQ_INIT(&cs->all_blocks);

	cs->read_event = bufferevent_new(c->fd, control_read_callback,
	    control_write_callback, control_error_callback, c);
	bufferevent_enable(cs->read_event, EV_READ);

	if (c->flags & CLIENT_CONTROLCONTROL)
		cs->write_event = cs->read_event;
	else {
		cs->write_event = bufferevent_new(c->out_fd, NULL,
		    control_write_callback, control_error_callback, c);
	}
	bufferevent_setwatermark(cs->write_event, EV_WRITE, CONTROL_BUFFER_LOW,
	    0);

	if (c->flags & CLIENT_CONTROLCONTROL) {
		bufferevent_write(cs->write_event, "\033P1000p", 7);
		bufferevent_enable(cs->write_event, EV_WRITE);
	}
}

/* Discard all output for a client. */
void
control_discard(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;

	RB_FOREACH(cp, control_panes, &cs->panes)
		control_discard_pane(c, cp);
}

/* Stop control mode. */
void
control_stop(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *cb1;

	if (~c->flags & CLIENT_CONTROLCONTROL)
		bufferevent_free(cs->write_event);
	bufferevent_free(cs->read_event);

	TAILQ_FOREACH_SAFE(cb, &cs->all_blocks, all_entry, cb1)
		control_free_block(cs, cb);
	control_reset_offsets(c);

	free(cs);
}
