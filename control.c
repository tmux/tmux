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
	/* exactly one of `line` and `buffer` must be nonnull */
	char				*line;
	struct evbuffer			*buffer;
	uint64_t			 t;

	TAILQ_ENTRY(control_block)	 entry;
	TAILQ_ENTRY(control_block)	 all_entry;
};

/* Control client pane. */
struct control_target {
	/* kind determines whether pane or job is meaningful. */
    	enum {
	    CONTROL_TARGET_PANE,
	    CONTROL_TARGET_JOB
	} kind;

	u_int				 pane;
	u_int				 job;

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
	TAILQ_ENTRY(control_target)	 pending_entry;

	TAILQ_HEAD(, control_block)	 blocks;

	RB_ENTRY(control_target)	 entry;
};
RB_HEAD(control_targets, control_target);

/* Subscription pane. */
struct control_sub_pane {
	u_int				 pane;
	u_int				 idx;
	char				*last;

	RB_ENTRY(control_sub_pane)	 entry;
};
RB_HEAD(control_sub_panes, control_sub_pane);

/* Subscription window. */
struct control_sub_window {
	u_int				 window;
	u_int				 idx;
	char				*last;

	RB_ENTRY(control_sub_window)	 entry;
};
RB_HEAD(control_sub_windows, control_sub_window);

/* Control client subscription. */
struct control_sub {
	char				*name;
	char				*format;

	enum control_sub_type		 type;
	u_int				 id;

	char				*last;
	struct control_sub_panes	 panes;
	struct control_sub_windows	 windows;

	RB_ENTRY(control_sub)		 entry;
};
RB_HEAD(control_subs, control_sub);

/* Control client state. */
struct control_state {
	struct control_targets		 targets;

	TAILQ_HEAD(, control_target)	 pending_list;
	u_int				 pending_count;

	TAILQ_HEAD(, control_block)	 all_blocks;

	struct bufferevent		*read_event;
	struct bufferevent		*write_event;

	struct control_subs		 subs;
	struct event			 subs_timer;
};

/* Low and high watermarks. */
#define CONTROL_BUFFER_LOW 512
#define CONTROL_BUFFER_HIGH 8192

/* Minimum to write to each client. */
#define CONTROL_WRITE_MINIMUM 32

/* Maximum age for clients that are not using pause mode. */
#define CONTROL_MAXIMUM_AGE 300000

/* Flags to ignore client. */
#define CONTROL_IGNORE_FLAGS \
	(CLIENT_CONTROL_NOOUTPUT| \
	 CLIENT_UNATTACHEDFLAGS)

/* Compare client panes. */
static int
control_target_cmp(struct control_target *ct1, struct control_target *ct2)
{
	if (ct1->kind < ct2->kind)
		return (-1);
	if (ct1->kind > ct2->kind)
		return (1);

	switch (ct1->kind) {
	    case CONTROL_TARGET_PANE:
		if (ct1->pane < ct2->pane)
			return (-1);
		if (ct1->pane > ct2->pane)
			return (1);
		break;
	    case CONTROL_TARGET_JOB:
		if (ct1->job < ct2->job)
			return (-1);
		if (ct1->job > ct2->job)
			return (1);
		break;
	}
	return (0);
}

RB_GENERATE_STATIC(control_targets, control_target, entry, control_target_cmp);

/* Compare client subs. */
static int
control_sub_cmp(struct control_sub *csub1, struct control_sub *csub2)
{
	return (strcmp(csub1->name, csub2->name));
}
RB_GENERATE_STATIC(control_subs, control_sub, entry, control_sub_cmp);

/* Compare client subscription panes. */
static int
control_sub_pane_cmp(struct control_sub_pane *csp1,
    struct control_sub_pane *csp2)
{
	if (csp1->pane < csp2->pane)
		return (-1);
	if (csp1->pane > csp2->pane)
		return (1);
	if (csp1->idx < csp2->idx)
		return (-1);
	if (csp1->idx > csp2->idx)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(control_sub_panes, control_sub_pane, entry,
    control_sub_pane_cmp);

/* Compare client subscription windows. */
static int
control_sub_window_cmp(struct control_sub_window *csw1,
    struct control_sub_window *csw2)
{
	if (csw1->window < csw2->window)
		return (-1);
	if (csw1->window > csw2->window)
		return (1);
	if (csw1->idx < csw2->idx)
		return (-1);
	if (csw1->idx > csw2->idx)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(control_sub_windows, control_sub_window, entry,
    control_sub_window_cmp);

/* Free a subscription. */
static void
control_free_sub(struct control_state *cs, struct control_sub *csub)
{
	struct control_sub_pane		*csp, *csp1;
	struct control_sub_window	*csw, *csw1;

	RB_FOREACH_SAFE(csp, control_sub_panes, &csub->panes, csp1) {
		RB_REMOVE(control_sub_panes, &csub->panes, csp);
		free(csp);
	}
	RB_FOREACH_SAFE(csw, control_sub_windows, &csub->windows, csw1) {
		RB_REMOVE(control_sub_windows, &csub->windows, csw);
		free(csw);
	}
	free(csub->last);

	RB_REMOVE(control_subs, &cs->subs, csub);
	free(csub->name);
	free(csub->format);
	free(csub);
}

/* Free a block. */
static void
control_free_block(struct control_state *cs, struct control_block *cb)
{
	free(cb->line);
	if (cb->buffer != NULL)
		evbuffer_free(cb->buffer);
	TAILQ_REMOVE(&cs->all_blocks, cb, all_entry);
	free(cb);
}

/* Get pane offsets for this client. */
static struct control_target *
control_get_pane(struct client *c, struct window_pane *wp)
{
	struct control_state	*cs = c->control_state;
	struct control_target	 ct = {
		.kind = CONTROL_TARGET_PANE,
		.pane = wp->id
	};

	return (RB_FIND(control_targets, &cs->targets, &ct));
}

/* Get job offsets for this client. */
static struct control_target *
control_get_job(struct client *c, u_int j)
{
	struct control_state	*cs = c->control_state;
	struct control_target	 ct = {
		.kind = CONTROL_TARGET_JOB,
		.job = j
	};

	return (RB_FIND(control_targets, &cs->targets, &ct));
}

static struct control_target *
control_get_target(struct client *c, struct window_pane *wp, struct job *job)
{
    if (wp != NULL)
	return control_get_pane(c, wp);
    return control_get_job(c, job->id);
}

/* Add pane offsets for this client. */
static struct control_target *
control_add_pane_or_job(struct client *c, struct window_pane *wp,
			struct job *job, struct window_pane_offset *offset)
{
	struct control_state	*cs = c->control_state;
	struct control_target	*ct;

	ct = control_get_target(c, wp, job);
	if (ct != NULL)
		return (ct);

	ct = xcalloc(1, sizeof *ct);
	if (wp != NULL) {
		ct->kind = CONTROL_TARGET_PANE;
		ct->pane = wp->id;
		ct->job = 0;
	} else {
		ct->kind = CONTROL_TARGET_JOB;
		ct->job = job->id;
		ct->pane = 0;
	}
	RB_INSERT(control_targets, &cs->targets, ct);

	memcpy(&ct->offset, offset, sizeof ct->offset);
	memcpy(&ct->queued, offset, sizeof ct->queued);
	TAILQ_INIT(&ct->blocks);

	return (ct);
}

/* Discard output for a control target. */
static void
control_discard_target(struct client *c, struct control_target *ct)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *cb1;

	TAILQ_FOREACH_SAFE(cb, &ct->blocks, entry, cb1) {
		TAILQ_REMOVE(&ct->blocks, cb, entry);
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
	struct control_target	*ct, *ct1;

	RB_FOREACH_SAFE(ct, control_targets, &cs->targets, ct1) {
		RB_REMOVE(control_targets, &cs->targets, ct);
		free(ct);
	}

	TAILQ_INIT(&cs->pending_list);
	cs->pending_count = 0;
}

/* Get offsets for client. */
struct window_pane_offset *
control_pane_or_job_offset(struct client *c, struct window_pane *wp,
			   struct job *job, int *off)
{
	struct control_state	*cs = c->control_state;
	struct control_target	*ct;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	if (c->flags & CLIENT_CONTROL_NOOUTPUT) {
		*off = 0;
		return (NULL);
	}

	ct = control_get_target(c, wp, job);
	if (ct == NULL || (ct->flags & CONTROL_PANE_PAUSED)) {
		*off = 0;
		return (NULL);
	}
	if (ct->flags & CONTROL_PANE_OFF) {
		*off = 1;
		return (NULL);
	}
	*off = (EVBUFFER_LENGTH(cs->write_event->output) >= CONTROL_BUFFER_LOW);
	return (&ct->offset);
}

/* Set pane as on. */
void
control_set_pane_or_job_on(struct client *c, struct window_pane *wp,
			   struct job *job, struct window_pane_offset *offset)
{
	struct control_target	*ct;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	ct = control_get_target(c, wp, job);
	if (ct != NULL && (ct->flags & CONTROL_PANE_OFF)) {
		ct->flags &= ~CONTROL_PANE_OFF;
		memcpy(&ct->offset, offset, sizeof ct->offset);
		memcpy(&ct->queued, offset, sizeof ct->queued);
	}
}

/* Set pane as off. */
void
control_set_pane_or_job_off(struct client *c, struct window_pane *wp,
			    struct job *job, struct window_pane_offset *offset)
{
	struct control_target	*ct;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	ct = control_add_pane_or_job(c, wp, job, offset);
	ct->flags |= CONTROL_PANE_OFF;
}

/* Continue a paused pane/job. */
void
control_continue_pane_or_job(struct client *c, struct window_pane *wp,
			     struct job *job, struct window_pane_offset *offset)
{
	struct control_target	*ct;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	ct = control_get_target(c, wp, job);
	if (ct != NULL && (ct->flags & CONTROL_PANE_PAUSED)) {
		ct->flags &= ~CONTROL_PANE_PAUSED;
		memcpy(&ct->offset, offset, sizeof ct->offset);
		memcpy(&ct->queued, offset, sizeof ct->queued);
		if (wp != NULL)
			control_write(c, "%%continue %%%u", wp->id);
		else if (c->flags & CLIENT_CONTROL_REPORTPOPUPS)
			control_write(c, "%%continue ^%u", job->id);
	}
}

/* Pause a pane. */
void
control_pause_pane_or_job(struct client *c, struct window_pane *wp,
			  struct job *job, struct window_pane_offset *offset)
{
	struct control_target	*ct;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	ct = control_add_pane_or_job(c, wp, job, offset);
	if (~ct->flags & CONTROL_PANE_PAUSED) {
		ct->flags |= CONTROL_PANE_PAUSED;
		control_discard_target(c, ct);
		if (wp != NULL)
			control_write(c, "%%pause %%%u", wp->id);
		else if (c->flags & CLIENT_CONTROL_REPORTPOPUPS)
			control_write(c, "%%pause ^%u", job->id);
	}
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
	free(s);
}

static void
control_vwrite_buffer(struct client *c, struct evbuffer *buffer)
{
	struct control_state	*cs = c->control_state;

	log_debug("%s: %s: writing buffer", __func__, c->name);

	bufferevent_write_buffer(cs->write_event, buffer);
	bufferevent_write(cs->write_event, "\n", 1);

	bufferevent_enable(cs->write_event, EV_WRITE);
}

/* Frees line and buffer after using them asynchronously. If non-null, line
 * will be freed eventually. */
static void
control_enqueue(struct client *c, struct control_state *cs, char *line,
		struct evbuffer *buffer)
{
	struct control_block *cb = xcalloc(1, sizeof *cb);

	if (line != NULL) {
		cb->line = line;
		log_debug("%s: %s: storing line: %s", __func__, c->name, cb->line);
	} else {
		log_debug("%s: %s: storing buffer", __func__, c->name);
		cb->buffer = buffer;
	}

	TAILQ_INSERT_TAIL(&cs->all_blocks, cb, all_entry);
	cb->t = get_timer();

	bufferevent_enable(cs->write_event, EV_WRITE);
}

/* Write a buffer. */
void
control_write_buffer(struct client *c, struct evbuffer *buffer)
{
	struct control_state	*cs = c->control_state;

	if (TAILQ_EMPTY(&cs->all_blocks)) {
		control_vwrite_buffer(c, buffer);
		return;
	}

	control_enqueue(c, cs, NULL, buffer);
}

/* Write a line. */
void
control_write(struct client *c, const char *fmt, ...)
{
	struct control_state	*cs = c->control_state;
	va_list			 ap;
	char			*line;

	va_start(ap, fmt);

	if (TAILQ_EMPTY(&cs->all_blocks)) {
		control_vwrite(c, fmt, ap);
		va_end(ap);
		return;
	}

	xvasprintf(&line, fmt, ap);
	control_enqueue(c, cs, line, NULL);

	va_end(ap);
}

/* Check age for this pane/job. */
static int
control_check_age(struct client *c, struct window_pane *wp,
		  struct job *job, struct control_target *ct)
{
	struct control_block	*cb;
	uint64_t		 t, age;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	cb = TAILQ_FIRST(&ct->blocks);
	if (cb == NULL)
		return (0);
	t = get_timer();
	if (cb->t >= t)
		return (0);

	age = t - cb->t;
	if (wp != NULL)
		log_debug("%s: %s: %%%u is %llu behind", __func__, c->name, wp->id,
		    (unsigned long long)age);
	else
		log_debug("%s: %s: job ^%u is %llu behind", __func__, c->name, job->id,
		    (unsigned long long)age);

	if (c->flags & CLIENT_CONTROL_PAUSEAFTER) {
		if (age < c->pause_age)
			return (0);
		ct->flags |= CONTROL_PANE_PAUSED;
		control_discard_target(c, ct);
		if (wp != NULL)
		    control_write(c, "%%pause %%%u", wp->id);
		else if (c->flags & CLIENT_CONTROL_REPORTPOPUPS)
		    control_write(c, "%%pause ^%u", job->id);
	} else {
		if (age < CONTROL_MAXIMUM_AGE)
			return (0);
		c->exit_message = xstrdup("too far behind");
		c->flags |= CLIENT_EXIT;
		control_discard(c);
	}
	return (1);
}

/* Write output from a pane/job. */
void
control_write_output(struct client *c, struct window_pane *wp, struct job *job,
		     struct window_pane_offset *offset)
{
	struct control_state	*cs = c->control_state;
	struct control_target	*ct;
	struct control_block	*cb;
	size_t			 new_size;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	if (wp != NULL &&
	    winlink_find_by_window(&c->session->windows, wp->window) == NULL)
		return;

	if (c->flags & CONTROL_IGNORE_FLAGS) {
		ct = control_get_target(c, wp, job);
		if (ct != NULL)
			goto ignore;
		return;
	}
	ct = control_add_pane_or_job(c, wp, job, offset);
	if (ct->flags & (CONTROL_PANE_OFF|CONTROL_PANE_PAUSED))
		goto ignore;
	if (control_check_age(c, wp, job, ct))
		return;

	if (wp != NULL)
		window_pane_get_new_data(wp, &ct->queued, &new_size);
	else
		job_get_new_data(job, &ct->queued, &new_size);
	if (new_size == 0)
		return;
	if (wp != NULL)
		window_pane_update_used_data(wp, &ct->queued, new_size);
	else
		job_update_used_data(job, &ct->queued, new_size);

	cb = xcalloc(1, sizeof *cb);
	cb->size = new_size;
	TAILQ_INSERT_TAIL(&cs->all_blocks, cb, all_entry);
	cb->t = get_timer();

	TAILQ_INSERT_TAIL(&ct->blocks, cb, entry);
	if (wp != NULL)
		log_debug("%s: %s: new output block of %zu for %%%u", __func__, c->name,
	    	cb->size, wp->id);
	else
		log_debug("%s: %s: new output block of %zu for job ^%u", __func__, c->name,
	    	cb->size, job->id);

	if (!ct->pending_flag) {
		if (wp != NULL)
			log_debug("%s: %s: %%%u now pending", __func__, c->name,
		    	wp->id);
		else
			log_debug("%s: %s: job ^%u now pending", __func__, c->name,
		    	job->id);
		TAILQ_INSERT_TAIL(&cs->pending_list, ct, pending_entry);
		ct->pending_flag = 1;
		cs->pending_count++;
	}
	bufferevent_enable(cs->write_event, EV_WRITE);
	return;

ignore:
	if (wp != NULL) {
		log_debug("%s: %s: ignoring pane %%%u", __func__, c->name, wp->id);
		window_pane_update_used_data(wp, &ct->offset, SIZE_MAX);
		window_pane_update_used_data(wp, &ct->queued, SIZE_MAX);
	} else {
		log_debug("%s: %s: ignoring pane job ^%u", __func__, c->name, job->id);
		job_update_used_data(job, &ct->offset, SIZE_MAX);
		job_update_used_data(job, &ct->queued, SIZE_MAX);
	}
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

void
control_escape(struct evbuffer *message, char *s, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		if (s[i] < ' ' || s[i] == '\\')
			evbuffer_add_printf(message, "\\%03o", s[i]);
		else
			evbuffer_add_printf(message, "%c", s[i]);
	}
}

/* Append data to buffer. If wp is NULL use job. */
static struct evbuffer *
control_append_data(struct client *c, struct control_target *ct, uint64_t age,
    struct evbuffer *message, struct window_pane *wp, struct job *job,
    size_t size)
{
	u_char	*new_data;
	size_t	 new_size;

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	if (message == NULL) {
		message = evbuffer_new();
		if (message == NULL)
			fatalx("out of memory");
		if (c->flags & CLIENT_CONTROL_PAUSEAFTER) {
			if (wp != NULL) {
			    evbuffer_add_printf(message,
				"%%extended-output %%%u %llu : ", wp->id,
				(unsigned long long)age);
			} else if (c->flags & CLIENT_CONTROL_REPORTPOPUPS) {
			    evbuffer_add_printf(message,
				"%%extended-output ^%u %llu : ", job->id,
				(unsigned long long)age);
			} else {
				evbuffer_free(message);
				message = NULL;
			}
		} else if (wp != NULL)
			evbuffer_add_printf(message, "%%output %%%u ", wp->id);
		else if (c->flags & CLIENT_CONTROL_REPORTPOPUPS)
			evbuffer_add_printf(message, "%%output ^%u ", job->id);
		else {
			evbuffer_free(message);
			message = NULL;
		}
	}

	if (wp != NULL)
		new_data = window_pane_get_new_data(wp, &ct->offset, &new_size);
	else {
		new_data = job_get_new_data(job, &ct->offset, &new_size);
	}
	if (new_size < size)
		fatalx("not enough data: %zu < %zu", new_size, size);
	if (message != NULL)
		control_escape(message, new_data, size);
	if (wp != NULL)
		window_pane_update_used_data(wp, &ct->offset, size);
	else
		job_update_used_data(job, &ct->offset, size);

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
control_write_pending(struct client *c, struct control_target *ct, size_t limit)
{
	struct control_state	*cs = c->control_state;
	struct window_pane	*wp = NULL;
	struct evbuffer		*message = NULL;
	size_t			 used = 0, size;
	struct control_block	*cb, *cb1;
	uint64_t		 age, t = get_timer();
	int			 valid;
	struct job		*job;

	switch (ct->kind) {
	    case CONTROL_TARGET_PANE:
		wp = control_window_pane(c, ct->pane);
		job = NULL;
		if (wp == NULL || wp->fd == -1)
			valid = 0;
		else
			valid = 1;
		break;

	    case CONTROL_TARGET_JOB:
		if (c->session == NULL)
			valid = 0;
		else {
			job = job_find_by_id(ct->job);
			valid = (job != NULL);
			wp = NULL;
		}
		break;
	}
	if (!valid) {
		TAILQ_FOREACH_SAFE(cb, &ct->blocks, entry, cb1) {
			TAILQ_REMOVE(&ct->blocks, cb, entry);
			control_free_block(cs, cb);
		}
		control_flush_all_blocks(c);
		return (0);
	}

	if ((job == NULL) == (wp == NULL))
		fatalx("Exactly one of job and wp must be nonnull");

	while (used != limit && !TAILQ_EMPTY(&ct->blocks)) {
		if (control_check_age(c, wp, job, ct)) {
			if (message != NULL)
				evbuffer_free(message);
			message = NULL;
			break;
		}

		cb = TAILQ_FIRST(&ct->blocks);
		if (cb->t < t)
			age = t - cb->t;
		else
			age = 0;
		if (wp != NULL)
			log_debug("%s: %s: output block %zu (age %llu) for %%%u "
		    	"(used %zu/%zu)", __func__, c->name, cb->size,
		    	(unsigned long long)age, ct->pane, used, limit);
		else
		    	log_debug("%s: %s: output block %zu (age %llu) for job ^%u "
		    	"(used %zu/%zu)", __func__, c->name, cb->size,
		    	(unsigned long long)age, ct->job, used, limit);

		size = cb->size;
		if (size > limit - used)
			size = limit - used;
		used += size;

		message = control_append_data(c, ct, age, message, wp, job, size);

		cb->size -= size;
		if (cb->size == 0) {
			TAILQ_REMOVE(&ct->blocks, cb, entry);
			control_free_block(cs, cb);

			cb = TAILQ_FIRST(&cs->all_blocks);
			if (cb != NULL && cb->size == 0) {
				if (message != NULL) {
					control_write_data(c, message);
					message = NULL;
				}
				control_flush_all_blocks(c);
			}
		}
	}
	if (message != NULL)
		control_write_data(c, message);
	return (!TAILQ_EMPTY(&ct->blocks));
}

/* Control client write callback. */
static void
control_write_callback(__unused struct bufferevent *bufev, void *data)
{
	struct client		*c = data;
	struct control_state	*cs = c->control_state;
	struct control_target	*ct, *ct1;
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

		TAILQ_FOREACH_SAFE(ct, &cs->pending_list, pending_entry, ct1) {
			if (EVBUFFER_LENGTH(evb) >= CONTROL_BUFFER_HIGH)
				break;
			if (control_write_pending(c, ct, limit))
				continue;
			TAILQ_REMOVE(&cs->pending_list, ct, pending_entry);
			ct->pending_flag = 0;
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
	RB_INIT(&cs->targets);
	TAILQ_INIT(&cs->pending_list);
	TAILQ_INIT(&cs->all_blocks);
	RB_INIT(&cs->subs);

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
	struct control_target	*ct;

	RB_FOREACH(ct, control_targets, &cs->targets)
		control_discard_target(c, ct);
	bufferevent_disable(cs->read_event, EV_READ);
}

/* Stop control mode. */
void
control_stop(struct client *c)
{
	struct control_state	*cs = c->control_state;
	struct control_block	*cb, *cb1;
	struct control_sub	*csub, *csub1;

	if (~c->flags & CLIENT_CONTROLCONTROL)
		bufferevent_free(cs->write_event);
	bufferevent_free(cs->read_event);

	RB_FOREACH_SAFE(csub, control_subs, &cs->subs, csub1)
		control_free_sub(cs, csub);
	if (evtimer_initialized(&cs->subs_timer))
		evtimer_del(&cs->subs_timer);

	TAILQ_FOREACH_SAFE(cb, &cs->all_blocks, all_entry, cb1)
		control_free_block(cs, cb);
	control_reset_offsets(c);

	free(cs);
}

/* Check session subscription. */
static void
control_check_subs_session(struct client *c, struct control_sub *csub)
{
	struct session		*s = c->session;
	struct format_tree	*ft;
	char			*value;

	ft = format_create_defaults(NULL, c, s, NULL, NULL);
	value = format_expand(ft, csub->format);
	format_free(ft);

	if (csub->last != NULL && strcmp(value, csub->last) == 0) {
		free(value);
		return;
	}
	control_write(c,
	    "%%subscription-changed %s $%u - - - : %s",
	    csub->name, s->id, value);
	free(csub->last);
	csub->last = value;
}

/* Check pane subscription. */
static void
control_check_subs_pane(struct client *c, struct control_sub *csub)
{
	struct session		*s = c->session;
	struct window_pane	*wp;
	struct window		*w;
	struct winlink		*wl;
	struct format_tree	*ft;
	char			*value;
	struct control_sub_pane	*csp, find;

	wp = window_pane_find_by_id(csub->id);
	if (wp == NULL || wp->fd == -1)
		return;
	w = wp->window;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session != s)
			continue;

		ft = format_create_defaults(NULL, c, s, wl, wp);
		value = format_expand(ft, csub->format);
		format_free(ft);

		find.pane = wp->id;
		find.idx = wl->idx;

		csp = RB_FIND(control_sub_panes, &csub->panes, &find);
		if (csp == NULL) {
			csp = xcalloc(1, sizeof *csp);
			csp->pane = wp->id;
			csp->idx = wl->idx;
			RB_INSERT(control_sub_panes, &csub->panes, csp);
		}

		if (csp->last != NULL && strcmp(value, csp->last) == 0) {
			free(value);
			continue;
		}
		control_write(c,
		    "%%subscription-changed %s $%u @%u %u %%%u : %s",
		    csub->name, s->id, w->id, wl->idx, wp->id, value);
		free(csp->last);
		csp->last = value;
	}
}

/* Check all panes subscription. */
static void
control_check_subs_all_panes(struct client *c, struct control_sub *csub)
{
	struct session		*s = c->session;
	struct window_pane	*wp;
	struct window		*w;
	struct winlink		*wl;
	struct format_tree	*ft;
	char			*value;
	struct control_sub_pane	*csp, find;

	RB_FOREACH(wl, winlinks, &s->windows) {
		w = wl->window;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			ft = format_create_defaults(NULL, c, s, wl, wp);
			value = format_expand(ft, csub->format);
			format_free(ft);

			find.pane = wp->id;
			find.idx = wl->idx;

			csp = RB_FIND(control_sub_panes, &csub->panes, &find);
			if (csp == NULL) {
				csp = xcalloc(1, sizeof *csp);
				csp->pane = wp->id;
				csp->idx = wl->idx;
				RB_INSERT(control_sub_panes, &csub->panes, csp);
			}

			if (csp->last != NULL &&
			    strcmp(value, csp->last) == 0) {
				free(value);
				continue;
			}
			control_write(c,
			    "%%subscription-changed %s $%u @%u %u %%%u : %s",
			    csub->name, s->id, w->id, wl->idx, wp->id, value);
			free(csp->last);
			csp->last = value;
		}
	}
}

/* Check window subscription. */
static void
control_check_subs_window(struct client *c, struct control_sub *csub)
{
	struct session			*s = c->session;
	struct window			*w;
	struct winlink			*wl;
	struct format_tree		*ft;
	char				*value;
	struct control_sub_window	*csw, find;

	w = window_find_by_id(csub->id);
	if (w == NULL)
		return;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session != s)
			continue;

		ft = format_create_defaults(NULL, c, s, wl, NULL);
		value = format_expand(ft, csub->format);
		format_free(ft);

		find.window = w->id;
		find.idx = wl->idx;

		csw = RB_FIND(control_sub_windows, &csub->windows, &find);
		if (csw == NULL) {
			csw = xcalloc(1, sizeof *csw);
			csw->window = w->id;
			csw->idx = wl->idx;
			RB_INSERT(control_sub_windows, &csub->windows, csw);
		}

		if (csw->last != NULL && strcmp(value, csw->last) == 0) {
			free(value);
			continue;
		}
		control_write(c,
		    "%%subscription-changed %s $%u @%u %u - : %s",
		    csub->name, s->id, w->id, wl->idx, value);
		free(csw->last);
		csw->last = value;
	}
}

/* Check all windows subscription. */
static void
control_check_subs_all_windows(struct client *c, struct control_sub *csub)
{
	struct session			*s = c->session;
	struct window			*w;
	struct winlink			*wl;
	struct format_tree		*ft;
	char				*value;
	struct control_sub_window	*csw, find;

	RB_FOREACH(wl, winlinks, &s->windows) {
		w = wl->window;

		ft = format_create_defaults(NULL, c, s, wl, NULL);
		value = format_expand(ft, csub->format);
		format_free(ft);

		find.window = w->id;
		find.idx = wl->idx;

		csw = RB_FIND(control_sub_windows, &csub->windows, &find);
		if (csw == NULL) {
			csw = xcalloc(1, sizeof *csw);
			csw->window = w->id;
			csw->idx = wl->idx;
			RB_INSERT(control_sub_windows, &csub->windows, csw);
		}

		if (csw->last != NULL && strcmp(value, csw->last) == 0) {
			free(value);
			continue;
		}
		control_write(c,
		    "%%subscription-changed %s $%u @%u %u - : %s",
		    csub->name, s->id, w->id, wl->idx, value);
		free(csw->last);
		csw->last = value;
	}
}

/* Check subscriptions timer. */
static void
control_check_subs_timer(__unused int fd, __unused short events, void *data)
{
	struct client		*c = data;
	struct control_state	*cs = c->control_state;
	struct control_sub	*csub, *csub1;
	struct timeval		 tv = { .tv_sec = 1 };

	log_debug("%s: timer fired", __func__);
	evtimer_add(&cs->subs_timer, &tv);

	RB_FOREACH_SAFE(csub, control_subs, &cs->subs, csub1) {
		switch (csub->type) {
		case CONTROL_SUB_SESSION:
			control_check_subs_session(c, csub);
			break;
		case CONTROL_SUB_PANE:
			control_check_subs_pane(c, csub);
			break;
		case CONTROL_SUB_ALL_PANES:
			control_check_subs_all_panes(c, csub);
			break;
		case CONTROL_SUB_WINDOW:
			control_check_subs_window(c, csub);
			break;
		case CONTROL_SUB_ALL_WINDOWS:
			control_check_subs_all_windows(c, csub);
			break;
		}
	}
}

/* Add a subscription. */
void
control_add_sub(struct client *c, const char *name, enum control_sub_type type,
    int id, const char *format)
{
	struct control_state	*cs = c->control_state;
	struct control_sub	*csub, find;
	struct timeval		 tv = { .tv_sec = 1 };

	find.name = (char *)name;
	if ((csub = RB_FIND(control_subs, &cs->subs, &find)) != NULL)
		control_free_sub(cs, csub);

	csub = xcalloc(1, sizeof *csub);
	csub->name = xstrdup(name);
	csub->type = type;
	csub->id = id;
	csub->format = xstrdup(format);
	RB_INSERT(control_subs, &cs->subs, csub);

	RB_INIT(&csub->panes);
	RB_INIT(&csub->windows);

	if (!evtimer_initialized(&cs->subs_timer))
		evtimer_set(&cs->subs_timer, control_check_subs_timer, c);
	if (!evtimer_pending(&cs->subs_timer, NULL))
		evtimer_add(&cs->subs_timer, &tv);
}

/* Remove a subscription. */
void
control_remove_sub(struct client *c, const char *name)
{
	struct control_state	*cs = c->control_state;
	struct control_sub	*csub, find;

	find.name = (char *)name;
	if ((csub = RB_FIND(control_subs, &cs->subs, &find)) != NULL)
		control_free_sub(cs, csub);
	if (RB_EMPTY(&cs->subs))
		evtimer_del(&cs->subs_timer);
}
