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

#include <errno.h>
#include <event.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * A queued block of output. Each client has one "all" queue and a per-pane
 * queue; a block on both holds up any later block until it is written, which
 * keeps output ordered against notifications. LINE is a verbatim notification
 * line; PANE is pane output of size bytes read at cp->offset; REPAINT is a
 * resync repaint, rendered lazily into its own buffer when the drain loop first
 * reaches it and escaped from there.
 */
enum control_block_type {
	CONTROL_BLOCK_LINE,
	CONTROL_BLOCK_PANE,
	CONTROL_BLOCK_REPAINT
};
struct control_block {
	enum control_block_type		 type;
	uint64_t			 t;

	size_t				 size;		/* PANE */
	char				*line;		/* LINE */
	struct evbuffer			*repaint;	/* REPAINT */

	/* History sequence when a PANE block was created (resync watermark). */
	struct grid_history_state	 history;

	TAILQ_ENTRY(control_block)	 entry;
	TAILQ_ENTRY(control_block)	 all_entry;
};

/*
 * Exclusive pane state. ON streams normally; OFF is disabled and holds the
 * pane read gate; PAUSED has been paused with %pause; RESYNC has discarded its
 * backlog and owes a repaint (queued as a REPAINT block) while it keeps running
 * with its output discarded.
 */
enum control_pane_state {
	CONTROL_PANE_ON,
	CONTROL_PANE_OFF,
	CONTROL_PANE_PAUSED,
	CONTROL_PANE_RESYNC
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

	enum control_pane_state		 state;

	/*
	 * History sequence and pane width when the client fell behind, used to
	 * bound the RESYNC scrollback replay; a discontinuity or width change
	 * degrades it to a repaint only.
	 */
	struct grid_history_state	 resync_state;
	u_int				 resync_sx;

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

	struct monitor_set		*subs;

	/*
	 * Output watchdog. resync_last_progress is stamped whenever the write
	 * buffer actually drains; the timer recovers panes that make no progress
	 * for too long, the case where both the pane-read and socket-write paths
	 * are stalled and no other callback fires. See control_client_stalled.
	 */
	struct event			 output_timer;
	uint64_t			 resync_last_progress;
};

/* Low and high watermarks. */
#define CONTROL_BUFFER_LOW 512
#define CONTROL_BUFFER_HIGH 8192

/* Minimum to write to each client. */
#define CONTROL_WRITE_MINIMUM 32

/* Maximum age for clients that are not using pause mode. */
#define CONTROL_MAXIMUM_AGE 300000

/* No-drain-progress window before a stalled client's panes are resynced. */
#define CONTROL_RESYNC_TIMEOUT 5000

/*
 * Byte backstop for output that never scrolls into history: alt-screen and
 * cursor-addressed redraws (progress bars, curses apps) accumulate bytes with
 * zero scrolled lines, so the line trigger never fires, yet the backlog grows
 * without bound. A repaint of such a pane is lossless of its final state - only
 * ephemeral intermediate frames are dropped - so resync eagerly once the
 * backlog passes this. Memory protection only; the primary trigger is missed
 * lines against the history limit. Sized at one full scrollback of typical
 * output (~64 bytes/line * ~2000 lines * some slack), not a tunable.
 */
#define CONTROL_RESYNC_BACKLOG 262144

/* Flags to ignore client. */
#define CONTROL_IGNORE_FLAGS \
	(CLIENT_CONTROL_NOOUTPUT| \
	 CLIENT_UNATTACHEDFLAGS)

static void	control_pane_resync(struct client *, struct window_pane *,
		    struct control_pane *);
static void	control_update_resyncs(struct client *);
static void	control_arm_output_timer(struct control_state *);
static struct evbuffer *control_build_repaint(struct window_pane *,
		    struct control_pane *);

/*
 * The control-resync choice: off (0) keeps the legacy kill; on (1) resyncs
 * everything, preempting pause mode; keep-pause (2) resyncs plain clients but
 * pauses pause-after clients with %pause.
 */
#define CONTROL_RESYNC_OFF 0
#define CONTROL_RESYNC_ON 1
#define CONTROL_RESYNC_KEEP_PAUSE 2

/* Is resync enabled for this server (any non-off value)? */
static int
control_resync_enabled(void)
{
	return (options_get_number(global_options, "control-resync") !=
	    CONTROL_RESYNC_OFF);
}

/*
 * Does this client recover with a resync rather than a pause? Plain clients
 * always do when resync is enabled; a pause-after client does too unless the
 * option is keep-pause, in which case it stays on the %pause path.
 */
static int
control_client_uses_resync(struct client *c)
{
	int	mode = options_get_number(global_options, "control-resync");

	if (mode == CONTROL_RESYNC_OFF)
		return (0);
	if (c->flags & CLIENT_CONTROL_PAUSEAFTER)
		return (mode == CONTROL_RESYNC_ON);
	return (1);
}

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
	if (cb->type == CONTROL_BLOCK_LINE)
		free(cb->line);
	else if (cb->type == CONTROL_BLOCK_REPAINT && cb->repaint != NULL)
		evbuffer_free(cb->repaint);
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

/* Remove a pane from the pending list if it is on it. */
static void
control_pane_clear_pending(struct control_state *cs, struct control_pane *cp)
{
	if (!cp->pending_flag)
		return;
	TAILQ_REMOVE(&cs->pending_list, cp, pending_entry);
	cp->pending_flag = 0;
	cs->pending_count--;
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
		control_discard_pane(c, cp);
		RB_REMOVE(control_panes, &cs->panes, cp);
		free(cp);
	}

	TAILQ_INIT(&cs->pending_list);
	cs->pending_count = 0;
}

/*
 * A client is stalled when it has made no drain progress for the timeout while
 * output is still buffered. This is derived rather than latched, so updating
 * resync_last_progress on the next write clears it. A stalled client abstains
 * from the pane read gate for panes with nothing queued, which is what
 * re-enables the pty in the deadlock where the backlog is entirely in the write
 * buffer and there is nothing left to act on per pane.
 */
static int
control_client_stalled(struct control_state *cs)
{
	uint64_t	now = get_timer();

	if (EVBUFFER_LENGTH(cs->write_event->output) == 0)
		return (0);
	return (now - cs->resync_last_progress >= CONTROL_RESYNC_TIMEOUT);
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
	if (cp == NULL) {
		*off = 0;
		return (NULL);
	}

	switch (cp->state) {
	case CONTROL_PANE_OFF:
		*off = 1;
		return (NULL);
	case CONTROL_PANE_PAUSED:
	case CONTROL_PANE_RESYNC:
		*off = 0;
		return (NULL);
	case CONTROL_PANE_ON:
		break;
	}

	/*
	 * A caught-up pane (nothing queued) of a stalled client abstains too, so
	 * the pty is not held shut by output that will never drain.
	 */
	if (TAILQ_EMPTY(&cp->blocks) && control_client_stalled(cs)) {
		*off = 0;
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
	if (cp != NULL && cp->state == CONTROL_PANE_OFF) {
		cp->state = CONTROL_PANE_ON;
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
	control_discard_pane(c, cp);
	memcpy(&cp->offset, &wp->offset, sizeof cp->offset);
	memcpy(&cp->queued, &wp->offset, sizeof cp->queued);
	cp->state = CONTROL_PANE_OFF;
}

/* Continue a paused pane. */
void
control_continue_pane(struct client *c, struct window_pane *wp)
{
	struct control_pane	*cp;

	cp = control_get_pane(c, wp);
	if (cp != NULL && cp->state == CONTROL_PANE_PAUSED) {
		cp->state = CONTROL_PANE_ON;
		memcpy(&cp->offset, &wp->offset, sizeof cp->offset);
		memcpy(&cp->queued, &wp->offset, sizeof cp->queued);
		control_write(c, "%%continue %%%u", wp->id);
	}
}

/* Pause a pane. */
void
control_pause_pane(struct client *c, struct window_pane *wp)
{
	struct control_pane	*cp;

	cp = control_add_pane(c, wp);
	if (cp->state != CONTROL_PANE_PAUSED) {
		cp->state = CONTROL_PANE_PAUSED;
		control_discard_pane(c, cp);
		control_write(c, "%%pause %%%u", wp->id);
	}
}

/*
 * Enter resync for a pane: discard the backlog, fast-forward the offsets to the
 * parser position, and queue a repaint block owed to the client (rendered
 * lazily when the drain loop reaches it). The pane keeps running with its
 * output discarded until the repaint has drained and a writable callback
 * resumes it (control_update_resyncs). A pause-after client is preempted here
 * rather than paused.
 */
static void
control_pane_resync(struct client *c, struct window_pane *wp,
    struct control_pane *cp)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *first;

	if (cp->state == CONTROL_PANE_RESYNC)
		return;

	/*
	 * Tag with the oldest queued block's history if there is one: it is the
	 * first output the client has not seen, so replaying from it can only
	 * under-replay, never duplicate.
	 */
	first = TAILQ_FIRST(&cp->blocks);
	if (first != NULL && first->type == CONTROL_BLOCK_PANE)
		cp->resync_state = first->history;
	else
		grid_history_get_state(wp->base.grid, &cp->resync_state);
	cp->resync_sx = screen_size_x(&wp->base);

	if (c->flags & CLIENT_CONTROL_PAUSEAFTER)
		log_debug("%s: %s: preempting pause-after on %%%u", __func__,
		    c->name, wp->id);
	log_debug("%s: %s: resync %%%u", __func__, c->name, wp->id);

	cp->state = CONTROL_PANE_RESYNC;
	control_discard_pane(c, cp);
	control_pane_clear_pending(cs, cp);
	memcpy(&cp->offset, &wp->offset, sizeof cp->offset);
	memcpy(&cp->queued, &wp->offset, sizeof cp->queued);

	cb = xcalloc(1, sizeof *cb);
	cb->type = CONTROL_BLOCK_REPAINT;
	cb->t = get_timer();
	TAILQ_INSERT_TAIL(&cs->all_blocks, cb, all_entry);
	TAILQ_INSERT_TAIL(&cp->blocks, cb, entry);
	TAILQ_INSERT_TAIL(&cs->pending_list, cp, pending_entry);
	cp->pending_flag = 1;
	cs->pending_count++;

	bufferevent_enable(cs->write_event, EV_WRITE);
	control_arm_output_timer(cs);
}

/* Write a line. */
static void printflike(2, 0)
control_vwrite(struct client *c, const char *fmt, va_list ap)
{
	struct control_state	*cs = c->control_state;
	char			*s;

	xvasprintf(&s, fmt, ap);
	log_debug("%s: %s: writing line: %s", __func__, c->name, s);

	bufferevent_write(cs->write_event, s, strlen(s));
	bufferevent_write(cs->write_event, "\n", 1);

	bufferevent_enable(cs->write_event, EV_WRITE);
	control_arm_output_timer(cs);
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
	cb->type = CONTROL_BLOCK_LINE;
	xvasprintf(&cb->line, fmt, ap);
	TAILQ_INSERT_TAIL(&cs->all_blocks, cb, all_entry);
	cb->t = get_timer();

	log_debug("%s: %s: storing line: %s", __func__, c->name, cb->line);
	bufferevent_enable(cs->write_event, EV_WRITE);
	control_arm_output_timer(cs);

	va_end(ap);
}

/*
 * Whether a pane has fallen far enough behind for a resync. The trigger is
 * missed scrolled lines, not buffered bytes: while the client has missed no
 * more lines than the history keeps, a repaint reconstructs everything it
 * missed, so a resync is lossless and buffering buys nothing; once it has
 * missed more, the history no longer covers those lines and only a wholly
 * buffered client keeps growing. The floor keeps a tiny or empty history from
 * resyncing once per scrolled line - a resync should stand for at least a
 * couple of screenfuls. CONTROL_RESYNC_BACKLOG is the byte backstop for output
 * that never scrolls into history.
 */
static int
control_pane_behind(struct control_pane *cp, struct window_pane *wp,
    size_t backlog)
{
	struct grid		*gd = wp->base.grid;
	struct control_block	*first;
	u_int			 missed, threshold, sy = screen_size_y(&wp->base);

	if (backlog > CONTROL_RESYNC_BACKLOG)
		return (1);

	first = TAILQ_FIRST(&cp->blocks);
	if (first == NULL || first->type != CONTROL_BLOCK_PANE)
		return (0);

	missed = grid_history_missed(gd, &first->history);
	threshold = (gd->hlimit > 2 * sy) ? gd->hlimit : 2 * sy;
	return (missed >= threshold);
}

/* Write output from a pane. */
void
control_write_output(struct client *c, struct window_pane *wp)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;
	struct control_block	*cb;
	size_t			 new_size;

	if (winlink_find_by_window(&c->session->windows, wp->window) == NULL)
		return;

	if (c->flags & (CONTROL_IGNORE_FLAGS|CLIENT_EXIT)) {
		cp = control_get_pane(c, wp);
		if (cp != NULL)
			goto ignore;
		return;
	}
	cp = control_add_pane(c, wp);
	if (cp->state != CONTROL_PANE_ON)
		goto ignore;

	/*
	 * While the client is stalled do not queue this batch: it would be
	 * trimmed while the client abstains from the buffer minimum. Enter resync
	 * so the pane owes a repaint and keeps fast-forwarding instead. Otherwise
	 * enter resync once the client has fallen behind the history frontier (or
	 * the byte backstop), rather than letting the pane buffer grow without
	 * bound.
	 */
	if (control_client_uses_resync(c)) {
		window_pane_get_new_data(wp, &cp->offset, &new_size);
		if (control_client_stalled(cs)) {
			control_pane_resync(c, wp, cp);
			goto ignore;
		}
		if (control_pane_behind(cp, wp, new_size)) {
			log_debug("%s: %s: %%%u behind, backlog %zu", __func__,
			    c->name, wp->id, new_size);
			control_pane_resync(c, wp, cp);
			goto ignore;
		}
	}

	window_pane_get_new_data(wp, &cp->queued, &new_size);
	if (new_size == 0)
		return;
	window_pane_update_used_data(wp, &cp->queued, new_size);

	cb = xcalloc(1, sizeof *cb);
	cb->type = CONTROL_BLOCK_PANE;
	cb->size = new_size;
	grid_history_get_state(wp->base.grid, &cb->history);
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
	control_arm_output_timer(cs);
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

/*
 * Wait for the terminal to send an empty line or close, used by a control
 * client after printing %exit so a wrapping terminal (such as iTerm2) can
 * finish reading.
 */
void
control_wait_exit(int fd)
{
	struct pollfd	 pfd;
	struct evbuffer	*evb;
	char		*line;
	int		 n;

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");

	for (;;) {
		line = evbuffer_readln(evb, NULL, EVBUFFER_EOL_LF);
		if (line != NULL) {
			if (*line == '\0') { /* empty line, stop */
				free(line);
				break;
			}
			free(line);
			continue; /* drain buffered lines first */
		}

		memset(&pfd, 0, sizeof pfd);
		pfd.fd = fd;
		pfd.events = POLLIN;
		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EINTR)
				continue;
			break;
		}

		n = evbuffer_read(evb, fd, -1);
		if (n == 0)
			break;
		if (n == -1 && errno != EAGAIN && errno != EINTR)
			break;
	}

	evbuffer_free(evb);
}

/* Flush all blocks until output. */
static void
control_flush_all_blocks(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *cb1;

	TAILQ_FOREACH_SAFE(cb, &cs->all_blocks, all_entry, cb1) {
		if (cb->type != CONTROL_BLOCK_LINE)
			break;
		log_debug("%s: %s: flushing line: %s", __func__, c->name,
		    cb->line);

		bufferevent_write(cs->write_event, cb->line, strlen(cb->line));
		bufferevent_write(cs->write_event, "\n", 1);
		control_free_block(cs, cb);
	}
}

/* Append escaped bytes to a message, as expected in a %output line. */
static void
control_append_escaped(struct evbuffer *message, const u_char *data,
    size_t size)
{
	size_t	i, start;

	for (i = 0; i < size; i++) {
		if (data[i] < ' ' || data[i] == '\\') {
			evbuffer_add_printf(message, "\\%03o", data[i]);
		} else {
			start = i;
			while (i + 1 < size &&
			    data[i + 1] >= ' ' &&
			    data[i + 1] != '\\')
				i++;
			evbuffer_add(message, data + start, i - start + 1);
		}
	}
}

/* Start a new %output (or %extended-output) message. */
static struct evbuffer *
control_message_start(struct client *c, struct window_pane *wp, uint64_t age)
{
	struct evbuffer	*message;

	message = evbuffer_new();
	if (message == NULL)
		fatalx("out of memory");
	if (c->flags & CLIENT_CONTROL_PAUSEAFTER) {
		evbuffer_add_printf(message, "%%extended-output %%%u %llu : ",
		    wp->id, (unsigned long long)age);
	} else
		evbuffer_add_printf(message, "%%output %%%u ", wp->id);
	return (message);
}

/* Append data to buffer. */
static struct evbuffer *
control_append_data(struct client *c, struct control_pane *cp, uint64_t age,
    struct evbuffer *message, struct window_pane *wp, size_t size)
{
	u_char	*new_data;
	size_t	 new_size;

	if (message == NULL)
		message = control_message_start(c, wp, age);

	new_data = window_pane_get_new_data(wp, &cp->offset, &new_size);
	if (new_size < size)
		fatalx("not enough data: %zu < %zu", new_size, size);
	control_append_escaped(message, new_data, size);
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
	size_t			 used = 0, size, remaining;
	struct control_block	*cb, *cb1;
	uint64_t		 age, t = get_timer();
	int			 done;

	wp = control_window_pane(c, cp->pane);
	if (wp == NULL || wp->fd == -1) {
		TAILQ_FOREACH_SAFE(cb, &cp->blocks, entry, cb1) {
			TAILQ_REMOVE(&cp->blocks, cb, entry);
			control_free_block(cs, cb);
		}
		control_flush_all_blocks(c);
		return (0);
	}

	while (used != limit && !TAILQ_EMPTY(&cp->blocks)) {
		cb = TAILQ_FIRST(&cp->blocks);
		age = (cb->t < t) ? t - cb->t : 0;

		/* Render a repaint the first time the drain loop reaches it. */
		if (cb->type == CONTROL_BLOCK_REPAINT) {
			if (cb->repaint == NULL)
				cb->repaint = control_build_repaint(wp, cp);
			remaining = EVBUFFER_LENGTH(cb->repaint);
		} else
			remaining = cb->size;

		size = remaining;
		if (size > limit - used)
			size = limit - used;
		used += size;

		if (message == NULL)
			message = control_message_start(c, wp, age);
		if (cb->type == CONTROL_BLOCK_REPAINT) {
			control_append_escaped(message, EVBUFFER_DATA(cb->repaint),
			    size);
			evbuffer_drain(cb->repaint, size);
			done = (EVBUFFER_LENGTH(cb->repaint) == 0);
		} else {
			control_append_data(c, cp, age, message, wp, size);
			cb->size -= size;
			done = (cb->size == 0);
		}

		if (done) {
			TAILQ_REMOVE(&cp->blocks, cb, entry);
			control_free_block(cs, cb);

			cb = TAILQ_FIRST(&cs->all_blocks);
			if (cb != NULL && cb->type == CONTROL_BLOCK_LINE) {
				control_write_data(c, message);
				message = NULL;
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

	/*
	 * This callback only fires when the socket accepted data, so reaching
	 * it means output drained: stamp progress (which is what makes
	 * control_client_stalled false again) and finish any resync whose
	 * repaint has now gone out.
	 */
	cs->resync_last_progress = get_timer();

	control_flush_all_blocks(c);
	control_update_resyncs(c);

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
			control_pane_clear_pending(cs, cp);
		}
	}

	if (EVBUFFER_LENGTH(evb) == 0)
		bufferevent_disable(cs->write_event, EV_WRITE);
}

/* Is there output outstanding that the resync watchdog should watch? */
static int
control_output_pending(struct control_state *cs)
{
	struct control_pane	*cp;

	if (!TAILQ_EMPTY(&cs->all_blocks) ||
	    cs->pending_count != 0 ||
	    EVBUFFER_LENGTH(cs->write_event->output) != 0)
		return (1);

	/*
	 * A pane in resync is owed a repaint or waiting for room to resume; keep
	 * watching so the last-resort age kill still fires for a client that has
	 * gone quiet after its repaint drained.
	 */
	RB_FOREACH(cp, control_panes, &cs->panes) {
		if (cp->state == CONTROL_PANE_RESYNC)
			return (1);
	}
	return (0);
}

/*
 * Arm the watchdog for one second while there is output to watch. Called when
 * output is first queued and re-called from the timer itself, so an idle client
 * has no periodic wakeup.
 */
static void
control_arm_output_timer(struct control_state *cs)
{
	struct timeval	tv = { .tv_sec = 1 };

	if (evtimer_initialized(&cs->output_timer) &&
	    control_output_pending(cs) &&
	    !evtimer_pending(&cs->output_timer, NULL))
		evtimer_add(&cs->output_timer, &tv);
}

/*
 * Watchdog for a client that makes no progress draining its output. Both the
 * pane-read and socket-write callbacks are dead when the sole consumer is a
 * stalled client, so this is the only path that can recover the pane. It closes
 * a client that drains nothing for the maximum age; otherwise, per pane, it
 * resyncs a resync client that has fallen behind the history frontier (or byte
 * backstop) or that still holds queued blocks once stalled, and pauses a
 * pause-after client kept on the %pause path once its output has aged past
 * pause-after. Panes with nothing queued recover through the stalled abstention
 * in control_pane_offset.
 */
static void
control_output_timer(__unused int fd, __unused short events, void *data)
{
	struct client		*c = data;
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;
	struct control_block	*cb;
	struct window_pane	*wp;
	uint64_t		 now = get_timer(), age;
	size_t			 lag;
	int			 uses_resync, stalled;

	if (!control_output_pending(cs))
		return;

	age = (now > cs->resync_last_progress) ?
	    now - cs->resync_last_progress : 0;
	if (age >= CONTROL_MAXIMUM_AGE) {
		log_debug("%s: %s: no progress for maximum age, exiting",
		    __func__, c->name);
		c->exit_message = xstrdup("too far behind");
		c->flags |= CLIENT_EXIT;
		control_discard(c);
		return;
	}
	if (!control_resync_enabled()) {
		control_arm_output_timer(cs);
		return;
	}

	uses_resync = control_client_uses_resync(c);
	stalled = control_client_stalled(cs);

	RB_FOREACH(cp, control_panes, &cs->panes) {
		if (cp->state != CONTROL_PANE_ON)
			continue;
		wp = control_window_pane(c, cp->pane);
		if (wp == NULL || wp->fd == -1)
			continue;
		if (uses_resync) {
			window_pane_get_new_data(wp, &cp->offset, &lag);
			if (control_pane_behind(cp, wp, lag)) {
				log_debug("%s: %s: %%%u behind, backlog %zu",
				    __func__, c->name, cp->pane, lag);
				control_pane_resync(c, wp, cp);
			} else if (stalled && !TAILQ_EMPTY(&cp->blocks))
				control_pane_resync(c, wp, cp);
		} else {
			cb = TAILQ_FIRST(&cp->blocks);
			if (age >= (uint64_t)c->pause_age ||
			    (cb != NULL && now > cb->t &&
			    now - cb->t >= (uint64_t)c->pause_age))
				control_pause_pane(c, wp);
		}
	}

	control_arm_output_timer(cs);
}

/* Build the repaint byte stream owed to a resynced pane. */
static struct evbuffer *
control_build_repaint(struct window_pane *wp, struct control_pane *cp)
{
	struct evbuffer	*msg;

	msg = evbuffer_new();
	if (msg == NULL)
		fatalx("out of memory");
	screen_repaint(&wp->base, &cp->resync_state, cp->resync_sx, msg);
	return (msg);
}

/*
 * Finish any resync whose repaint has fully drained. Reaching this on a
 * writable callback means the client has room again, so resume normal streaming
 * from the current parser position; output produced while discarding is skipped
 * (a gap, never a duplicate). A pane whose repaint is still queued stays in
 * resync.
 */
static void
control_update_resyncs(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;
	struct window_pane	*wp;

	RB_FOREACH(cp, control_panes, &cs->panes) {
		if (cp->state != CONTROL_PANE_RESYNC || !TAILQ_EMPTY(&cp->blocks))
			continue;
		wp = control_window_pane(c, cp->pane);
		if (wp != NULL && wp->fd != -1) {
			log_debug("%s: %s: resync of %%%u delivered", __func__,
			    c->name, cp->pane);
			memcpy(&cp->offset, &wp->offset, sizeof cp->offset);
			memcpy(&cp->queued, &wp->offset, sizeof cp->queued);
		} else {
			log_debug("%s: %s: %%%u gone, ending resync", __func__,
			    c->name, cp->pane);
		}
		cp->state = CONTROL_PANE_ON;
	}
}

/* Write a subscription change. */
static void
control_sub_change(struct monitor_change *change, __unused void *data)
{
	struct client		*c = change->c;
	struct session		*s = change->s;
	struct winlink		*wl = change->wl;
	struct window_pane	*wp = change->wp;
	struct window		*w;

	if (wp != NULL) {
		w = wp->window;
		control_write(c, "%%subscription-changed %s $%u @%u %u %%%u : %s",
		    change->name, s->id, w->id, wl->idx, wp->id, change->value);
	} else if (wl != NULL) {
		w = wl->window;
		control_write(c, "%%subscription-changed %s $%u @%u %u - : %s",
		    change->name, s->id, w->id, wl->idx, change->value);
	} else {
		control_write(c, "%%subscription-changed %s $%u - - - : %s",
		    change->name, s->id, change->value);
	}
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
	cs->subs = monitor_create_client(c, control_sub_change, NULL);

	evtimer_set(&cs->output_timer, control_output_timer, c);
	cs->resync_last_progress = get_timer();

	cs->read_event = bufferevent_new(c->fd, control_read_callback,
	    control_write_callback, control_error_callback, c);
	if (cs->read_event == NULL)
		fatalx("out of memory");

	if (c->flags & CLIENT_CONTROLCONTROL)
		cs->write_event = cs->read_event;
	else {
		cs->write_event = bufferevent_new(c->out_fd, NULL,
		    control_write_callback, control_error_callback, c);
		if (cs->write_event == NULL)
			fatalx("out of memory");
	}
	bufferevent_setwatermark(cs->write_event, EV_WRITE, CONTROL_BUFFER_LOW,
	    0);

	if (c->flags & CLIENT_CONTROLCONTROL) {
		bufferevent_write(cs->write_event, "\033P1000p", 7);
		bufferevent_enable(cs->write_event, EV_WRITE);
	}
}

/* Control client ready. */
void
control_ready(struct client *c)
{
	bufferevent_enable(c->control_state->read_event, EV_READ);
}

/* Discard all output for a client. */
void
control_discard(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_pane	*cp;

	RB_FOREACH(cp, control_panes, &cs->panes)
		control_discard_pane(c, cp);
	bufferevent_disable(cs->read_event, EV_READ);
}

/* Stop control mode. */
void
control_stop(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *cb1;

	if (cs == NULL)
		return;

	monitor_destroy(cs->subs);

	if (evtimer_initialized(&cs->output_timer))
		evtimer_del(&cs->output_timer);

	if (~c->flags & CLIENT_CONTROLCONTROL)
		bufferevent_free(cs->write_event);
	bufferevent_free(cs->read_event);

	control_reset_offsets(c);
	TAILQ_FOREACH_SAFE(cb, &cs->all_blocks, all_entry, cb1)
		control_free_block(cs, cb);

	c->control_state = NULL;
	free(cs);
}

/* Add a subscription. */
void
control_add_sub(struct client *c, const char *name, enum monitor_type type,
    int id, const char *format)
{
	struct control_state	*cs = c->control_state;

	monitor_add(cs->subs, name, type, id, format, MONITOR_NOTIFY_INITIAL);
}

/* Remove a subscription. */
void
control_remove_sub(struct client *c, const char *name)
{
	struct control_state	*cs = c->control_state;

	monitor_remove(cs->subs, name);
}
