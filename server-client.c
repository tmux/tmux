/* $Id$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

void	server_client_handle_key(int, struct mouse_event *, void *);
void	server_client_repeat_timer(int, short, void *);
void	server_client_check_exit(struct client *);
void	server_client_check_backoff(struct client *);
void	server_client_check_redraw(struct client *);
void	server_client_set_title(struct client *);
void	server_client_reset_state(struct client *);
void	server_client_in_callback(struct bufferevent *, short, void *);
void	server_client_out_callback(struct bufferevent *, short, void *);
void	server_client_err_callback(struct bufferevent *, short, void *);

int	server_client_msg_dispatch(struct client *);
void	server_client_msg_command(struct client *, struct msg_command_data *);
void	server_client_msg_identify(
	    struct client *, struct msg_identify_data *, int);
void	server_client_msg_shell(struct client *);

void printflike2 server_client_msg_error(struct cmd_ctx *, const char *, ...);
void printflike2 server_client_msg_print(struct cmd_ctx *, const char *, ...);
void printflike2 server_client_msg_info(struct cmd_ctx *, const char *, ...);

/* Create a new client. */
void
server_client_create(int fd)
{
	struct client	*c;
	u_int		 i;

	setblocking(fd, 0);

	c = xcalloc(1, sizeof *c);
	c->references = 0;
	imsg_init(&c->ibuf, fd);
	server_update_event(c);

	if (gettimeofday(&c->creation_time, NULL) != 0)
		fatal("gettimeofday failed");
	memcpy(&c->activity_time, &c->creation_time, sizeof c->activity_time);

	c->stdin_event = NULL;
	c->stdout_event = NULL;
	c->stderr_event = NULL;

	c->tty.fd = -1;
	c->title = NULL;

	c->session = NULL;
	c->last_session = NULL;
	c->tty.sx = 80;
	c->tty.sy = 24;

	screen_init(&c->status, c->tty.sx, 1, 0);
	RB_INIT(&c->status_new);
	RB_INIT(&c->status_old);

	c->message_string = NULL;
	ARRAY_INIT(&c->message_log);

	c->prompt_string = NULL;
	c->prompt_buffer = NULL;
	c->prompt_index = 0;

	c->last_mouse.b = MOUSE_UP;
	c->last_mouse.x = c->last_mouse.y = -1;

	evtimer_set(&c->repeat_timer, server_client_repeat_timer, c);

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == NULL) {
			ARRAY_SET(&clients, i, c);
			return;
		}
	}
	ARRAY_ADD(&clients, c);
	log_debug("new client %d", fd);
}

/* Lost a client. */
void
server_client_lost(struct client *c)
{
	struct message_entry	*msg;
	u_int			 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == c)
			ARRAY_SET(&clients, i, NULL);
	}
	log_debug("lost client %d", c->ibuf.fd);

	/*
	 * If CLIENT_TERMINAL hasn't been set, then tty_init hasn't been called
	 * and tty_free might close an unrelated fd.
	 */
	if (c->flags & CLIENT_TERMINAL)
		tty_free(&c->tty);

	if (c->stdin_fd != -1) {
		setblocking(c->stdin_fd, 1);
		close(c->stdin_fd);
	}
	if (c->stdin_event != NULL)
		bufferevent_free(c->stdin_event);
	if (c->stdout_fd != -1) {
		setblocking(c->stdout_fd, 1);
		close(c->stdout_fd);
	}
	if (c->stdout_event != NULL)
		bufferevent_free(c->stdout_event);
	if (c->stderr_fd != -1) {
		setblocking(c->stderr_fd, 1);
		close(c->stderr_fd);
	}
	if (c->stderr_event != NULL)
		bufferevent_free(c->stderr_event);

	status_free_jobs(&c->status_new);
	status_free_jobs(&c->status_old);
	screen_free(&c->status);

	if (c->title != NULL)
		xfree(c->title);

	evtimer_del(&c->repeat_timer);

	evtimer_del(&c->identify_timer);

	if (c->message_string != NULL)
		xfree(c->message_string);
	evtimer_del(&c->message_timer);
	for (i = 0; i < ARRAY_LENGTH(&c->message_log); i++) {
		msg = &ARRAY_ITEM(&c->message_log, i);
		xfree(msg->msg);
	}
	ARRAY_FREE(&c->message_log);

	if (c->prompt_string != NULL)
		xfree(c->prompt_string);
	if (c->prompt_buffer != NULL)
		xfree(c->prompt_buffer);

	if (c->cwd != NULL)
		xfree(c->cwd);

	environ_free(&c->environ);

	close(c->ibuf.fd);
	imsg_clear(&c->ibuf);
	event_del(&c->event);

	for (i = 0; i < ARRAY_LENGTH(&dead_clients); i++) {
		if (ARRAY_ITEM(&dead_clients, i) == NULL) {
			ARRAY_SET(&dead_clients, i, c);
			break;
		}
	}
	if (i == ARRAY_LENGTH(&dead_clients))
		ARRAY_ADD(&dead_clients, c);
	c->flags |= CLIENT_DEAD;

	recalculate_sizes();
	server_check_unattached();
	server_update_socket();
}

/* Process a single client event. */
void
server_client_callback(int fd, short events, void *data)
{
	struct client	*c = data;

	if (c->flags & CLIENT_DEAD)
		return;

	if (fd == c->ibuf.fd) {
		if (events & EV_WRITE && msgbuf_write(&c->ibuf.w) < 0)
			goto client_lost;

		if (c->flags & CLIENT_BAD) {
			if (c->ibuf.w.queued == 0)
				goto client_lost;
			return;
		}

		if (events & EV_READ && server_client_msg_dispatch(c) != 0)
			goto client_lost;
	}

	server_update_event(c);
	return;

client_lost:
	server_client_lost(c);
}

/* Handle client status timer. */
void
server_client_status_timer(void)
{
	struct client	*c;
	struct session	*s;
	struct timeval	 tv;
	u_int		 i;
	int		 interval;
	time_t		 difference;

	if (gettimeofday(&tv, NULL) != 0)
		fatal("gettimeofday failed");

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->message_string != NULL || c->prompt_string != NULL) {
			/*
			 * Don't need timed redraw for messages/prompts so bail
			 * now. The status timer isn't reset when they are
			 * redrawn anyway.
			 */
			continue;
		}
		s = c->session;

		if (!options_get_number(&s->options, "status"))
			continue;
		interval = options_get_number(&s->options, "status-interval");

		difference = tv.tv_sec - c->status_timer.tv_sec;
		if (difference >= interval) {
			status_update_jobs(c);
			c->flags |= CLIENT_STATUS;
		}
	}
}

/* Handle data key input from client. */
void
server_client_handle_key(int key, struct mouse_event *mouse, void *data)
{
	struct client		*c = data;
	struct session		*s;
	struct window		*w;
	struct window_pane	*wp;
	struct options		*oo;
	struct timeval		 tv;
	struct key_binding	*bd;
	int		      	 xtimeout, isprefix;

	/* Check the client is good to accept input. */
	if ((c->flags & (CLIENT_DEAD|CLIENT_SUSPENDED)) != 0)
		return;
	if (c->session == NULL)
		return;
	s = c->session;

	/* Update the activity timer. */
	if (gettimeofday(&c->activity_time, NULL) != 0)
		fatal("gettimeofday failed");
	memcpy(&s->activity_time, &c->activity_time, sizeof s->activity_time);

	w = c->session->curw->window;
	wp = w->active;
	oo = &c->session->options;

	/* Special case: number keys jump to pane in identify mode. */
	if (c->flags & CLIENT_IDENTIFY && key >= '0' && key <= '9') {
		if (c->flags & CLIENT_READONLY)
			return;
		wp = window_pane_at_index(w, key - '0');
		if (wp != NULL && window_pane_visible(wp))
			window_set_active_pane(w, wp);
		server_clear_identify(c);
		return;
	}

	/* Handle status line. */
	if (!(c->flags & CLIENT_READONLY)) {
		status_message_clear(c);
		server_clear_identify(c);
	}
	if (c->prompt_string != NULL) {
		if (!(c->flags & CLIENT_READONLY))
			status_prompt_key(c, key);
		return;
	}

	/* Check for mouse keys. */
	if (key == KEYC_MOUSE) {
		if (c->flags & CLIENT_READONLY)
			return;
		if (options_get_number(oo, "mouse-select-pane") &&
		    (!(options_get_number(oo, "status") &&
		       mouse->y + 1 == c->tty.sy)) &&
		    ((!(mouse->b & MOUSE_DRAG) && mouse->b != MOUSE_UP) ||
		    wp->mode != &window_copy_mode)) {
			/*
			 * Allow pane switching in copy mode only by mouse down
			 * (click).
			 */
			window_set_active_at(w, mouse->x, mouse->y);
			server_redraw_window_borders(w);
			wp = w->active;
		}
		if (mouse->y + 1 == c->tty.sy &&
		    options_get_number(oo, "mouse-select-window") &&
		    options_get_number(oo, "status")) {
			if (mouse->b == MOUSE_UP &&
			    c->last_mouse.b != MOUSE_UP) {
				status_set_window_at(c, mouse->x);
				return;
			}
			if (mouse->b & MOUSE_45) {
				if ((mouse->b & MOUSE_BUTTON) == MOUSE_1) {
					session_previous(c->session, 0);
					server_redraw_session(s);
				}
				if ((mouse->b & MOUSE_BUTTON) == MOUSE_2) {
					session_next(c->session, 0);
					server_redraw_session(s);
				}
				return;
			}
		}
		if (options_get_number(oo, "mouse-resize-pane"))
			layout_resize_pane_mouse(c, mouse);
		memcpy(&c->last_mouse, mouse, sizeof c->last_mouse);
		window_pane_mouse(wp, c->session, mouse);
		return;
	}

	/* Is this a prefix key? */
	if (key == options_get_number(&c->session->options, "prefix"))
		isprefix = 1;
	else if (key == options_get_number(&c->session->options, "prefix2"))
		isprefix = 1;
	else
		isprefix = 0;

	/* No previous prefix key. */
	if (!(c->flags & CLIENT_PREFIX)) {
		if (isprefix)
			c->flags |= CLIENT_PREFIX;
		else {
			/* Try as a non-prefix key binding. */
			if ((bd = key_bindings_lookup(key)) == NULL) {
				if (!(c->flags & CLIENT_READONLY))
					window_pane_key(wp, c->session, key);
			} else
				key_bindings_dispatch(bd, c);
		}
		return;
	}

	/* Prefix key already pressed. Reset prefix and lookup key. */
	c->flags &= ~CLIENT_PREFIX;
	if ((bd = key_bindings_lookup(key | KEYC_PREFIX)) == NULL) {
		/* If repeating, treat this as a key, else ignore. */
		if (c->flags & CLIENT_REPEAT) {
			c->flags &= ~CLIENT_REPEAT;
			if (isprefix)
				c->flags |= CLIENT_PREFIX;
			else if (!(c->flags & CLIENT_READONLY))
				window_pane_key(wp, c->session, key);
		}
		return;
	}

	/* If already repeating, but this key can't repeat, skip it. */
	if (c->flags & CLIENT_REPEAT && !bd->can_repeat) {
		c->flags &= ~CLIENT_REPEAT;
		if (isprefix)
			c->flags |= CLIENT_PREFIX;
		else if (!(c->flags & CLIENT_READONLY))
			window_pane_key(wp, c->session, key);
		return;
	}

	/* If this key can repeat, reset the repeat flags and timer. */
	xtimeout = options_get_number(&c->session->options, "repeat-time");
	if (xtimeout != 0 && bd->can_repeat) {
		c->flags |= CLIENT_PREFIX|CLIENT_REPEAT;

		tv.tv_sec = xtimeout / 1000;
		tv.tv_usec = (xtimeout % 1000) * 1000L;
		evtimer_del(&c->repeat_timer);
		evtimer_add(&c->repeat_timer, &tv);
	}

	/* Dispatch the command. */
	key_bindings_dispatch(bd, c);
}

/* Client functions that need to happen every loop. */
void
server_client_loop(void)
{
	struct client		*c;
	struct window		*w;
	struct window_pane	*wp;
	u_int		 	 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL)
			continue;

		server_client_check_exit(c);
		if (c->session != NULL) {
			server_client_check_redraw(c);
			server_client_reset_state(c);
		}
	}

	/*
	 * Any windows will have been redrawn as part of clients, so clear
	 * their flags now.
	 */
	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		w->flags &= ~WINDOW_REDRAW;
		TAILQ_FOREACH(wp, &w->panes, entry)
			wp->flags &= ~PANE_REDRAW;
	}
}

/*
 * Update cursor position and mode settings. The scroll region and attributes
 * are cleared when idle (waiting for an event) as this is the most likely time
 * a user may interrupt tmux, for example with ~^Z in ssh(1). This is a
 * compromise between excessive resets and likelihood of an interrupt.
 *
 * tty_region/tty_reset/tty_update_mode already take care of not resetting
 * things that are already in their default state.
 */
void
server_client_reset_state(struct client *c)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp = w->active;
	struct screen		*s = wp->screen;
	struct options		*oo = &c->session->options;
	struct options		*wo = &w->options;
	int			 status, mode;

	if (c->flags & CLIENT_SUSPENDED)
		return;

	tty_region(&c->tty, 0, c->tty.sy - 1);

	status = options_get_number(oo, "status");
	if (!window_pane_visible(wp) || wp->yoff + s->cy >= c->tty.sy - status)
		tty_cursor(&c->tty, 0, 0);
	else
		tty_cursor(&c->tty, wp->xoff + s->cx, wp->yoff + s->cy);

	/*
	 * Resizing panes with the mouse requires at least button mode to give
	 * a smooth appearance.
	 */
	mode = s->mode;
	if ((c->last_mouse.b & MOUSE_RESIZE_PANE) &&
	    !(mode & (MODE_MOUSE_BUTTON|MODE_MOUSE_ANY)))
		mode |= MODE_MOUSE_BUTTON;

	/*
	 * Any mode will do for mouse-select-pane, but set standard mode if
	 * none.
	 */
	if ((mode & ALL_MOUSE_MODES) == 0) {
		if (TAILQ_NEXT(TAILQ_FIRST(&w->panes), entry) != NULL &&
		    options_get_number(oo, "mouse-select-pane"))
			mode |= MODE_MOUSE_STANDARD;
		else if (options_get_number(oo, "mouse-resize-pane"))
			mode |= MODE_MOUSE_STANDARD;
		else if (options_get_number(oo, "mouse-select-window"))
			mode |= MODE_MOUSE_STANDARD;
		else if (options_get_number(wo, "mode-mouse"))
			mode |= MODE_MOUSE_STANDARD;
	}

	/*
	 * Set UTF-8 mouse input if required. If the terminal is UTF-8, the
	 * user has set mouse-utf8 and any mouse mode is in effect, turn on
	 * UTF-8 mouse input. If the receiving terminal hasn't requested it
	 * (that is, it isn't in s->mode), then it'll be converted in
	 * input_mouse.
	 */
	if ((c->tty.flags & TTY_UTF8) &&
	    (mode & ALL_MOUSE_MODES) && options_get_number(oo, "mouse-utf8"))
		mode |= MODE_MOUSE_UTF8;
	else
		mode &= ~MODE_MOUSE_UTF8;

	/* Set the terminal mode and reset attributes. */
	tty_update_mode(&c->tty, mode, s);
	tty_reset(&c->tty);
}

/* Repeat time callback. */
/* ARGSUSED */
void
server_client_repeat_timer(unused int fd, unused short events, void *data)
{
	struct client	*c = data;

	if (c->flags & CLIENT_REPEAT)
		c->flags &= ~(CLIENT_PREFIX|CLIENT_REPEAT);
}

/* Check if client should be exited. */
void
server_client_check_exit(struct client *c)
{
	struct msg_exit_data	exitdata;

	if (!(c->flags & CLIENT_EXIT))
		return;

	if (c->stdout_fd != -1 && c->stdout_event != NULL &&
	    EVBUFFER_LENGTH(c->stdout_event->output) != 0)
		return;
	if (c->stderr_fd != -1 && c->stderr_event != NULL &&
	    EVBUFFER_LENGTH(c->stderr_event->output) != 0)
		return;

	exitdata.retcode = c->retcode;
	server_write_client(c, MSG_EXIT, &exitdata, sizeof exitdata);

	c->flags &= ~CLIENT_EXIT;
}

/* Check for client redraws. */
void
server_client_check_redraw(struct client *c)
{
	struct session		*s = c->session;
	struct window_pane	*wp;
	int		 	 flags, redraw;

	flags = c->tty.flags & TTY_FREEZE;
	c->tty.flags &= ~TTY_FREEZE;

	if (c->flags & (CLIENT_REDRAW|CLIENT_STATUS)) {
		if (options_get_number(&s->options, "set-titles"))
			server_client_set_title(c);

		if (c->message_string != NULL)
			redraw = status_message_redraw(c);
		else if (c->prompt_string != NULL)
			redraw = status_prompt_redraw(c);
		else
			redraw = status_redraw(c);
		if (!redraw)
			c->flags &= ~CLIENT_STATUS;
	}

	if (c->flags & CLIENT_REDRAW) {
		screen_redraw_screen(c, 0, 0);
		c->flags &= ~(CLIENT_STATUS|CLIENT_BORDERS);
	} else if (c->flags & CLIENT_REDRAWWINDOW) {
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry)
			screen_redraw_pane(c, wp);
		c->flags &= ~CLIENT_REDRAWWINDOW;
	} else {
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry) {
			if (wp->flags & PANE_REDRAW)
				screen_redraw_pane(c, wp);
		}
	}

	if (c->flags & CLIENT_BORDERS)
		screen_redraw_screen(c, 0, 1);

	if (c->flags & CLIENT_STATUS)
		screen_redraw_screen(c, 1, 0);

	c->tty.flags |= flags;

	c->flags &= ~(CLIENT_REDRAW|CLIENT_STATUS|CLIENT_BORDERS);
}

/* Set client title. */
void
server_client_set_title(struct client *c)
{
	struct session	*s = c->session;
	const char	*template;
	char		*title;

	template = options_get_string(&s->options, "set-titles-string");

	title = status_replace(c, NULL, NULL, NULL, template, time(NULL), 1);
	if (c->title == NULL || strcmp(title, c->title) != 0) {
		if (c->title != NULL)
			xfree(c->title);
		c->title = xstrdup(title);
		tty_set_title(&c->tty, c->title);
	}
	xfree(title);
}

/*
 * Error callback for client stdin. Caller must increase reference count when
 * enabling event!
 */
void
server_client_in_callback(
    unused struct bufferevent *bufev, unused short what, void *data)
{
	struct client	*c = data;

	c->references--;
	if (c->flags & CLIENT_DEAD)
		return;

	bufferevent_disable(c->stdin_event, EV_READ|EV_WRITE);
	setblocking(c->stdin_fd, 1);
	close(c->stdin_fd);
	c->stdin_fd = -1;

	if (c->stdin_callback != NULL)
		c->stdin_callback(c, c->stdin_data);
}

/* Error callback for client stdout. */
void
server_client_out_callback(
    unused struct bufferevent *bufev, unused short what, unused void *data)
{
	struct client	*c = data;

	bufferevent_disable(c->stdout_event, EV_READ|EV_WRITE);
	setblocking(c->stdout_fd, 1);
	close(c->stdout_fd);
	c->stdout_fd = -1;
}

/* Error callback for client stderr. */
void
server_client_err_callback(
    unused struct bufferevent *bufev, unused short what, unused void *data)
{
	struct client	*c = data;

	bufferevent_disable(c->stderr_event, EV_READ|EV_WRITE);
	setblocking(c->stderr_fd, 1);
	close(c->stderr_fd);
	c->stderr_fd = -1;
}

/* Dispatch message from client. */
int
server_client_msg_dispatch(struct client *c)
{
	struct imsg		 imsg;
	struct msg_command_data	 commanddata;
	struct msg_identify_data identifydata;
	struct msg_environ_data	 environdata;
	ssize_t			 n, datalen;

	if ((n = imsg_read(&c->ibuf)) == -1 || n == 0)
		return (-1);

	for (;;) {
		if ((n = imsg_get(&c->ibuf, &imsg)) == -1)
			return (-1);
		if (n == 0)
			return (0);
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		if (imsg.hdr.peerid != PROTOCOL_VERSION) {
			server_write_client(c, MSG_VERSION, NULL, 0);
			c->flags |= CLIENT_BAD;
			imsg_free(&imsg);
			continue;
		}

		log_debug("got %d from client %d", imsg.hdr.type, c->ibuf.fd);
		switch (imsg.hdr.type) {
		case MSG_COMMAND:
			if (datalen != sizeof commanddata)
				fatalx("bad MSG_COMMAND size");
			memcpy(&commanddata, imsg.data, sizeof commanddata);

			server_client_msg_command(c, &commanddata);
			break;
		case MSG_IDENTIFY:
			if (datalen != sizeof identifydata)
				fatalx("bad MSG_IDENTIFY size");
			if (imsg.fd == -1)
				fatalx("MSG_IDENTIFY missing fd");
			memcpy(&identifydata, imsg.data, sizeof identifydata);

			c->stdin_fd = imsg.fd;
			c->stdin_event = bufferevent_new(c->stdin_fd,
			    NULL, NULL, server_client_in_callback, c);
			if (c->stdin_event == NULL)
				fatalx("failed to create stdin event");
			setblocking(c->stdin_fd, 0);

			server_client_msg_identify(c, &identifydata, imsg.fd);
			break;
		case MSG_STDOUT:
			if (datalen != 0)
				fatalx("bad MSG_STDOUT size");
			if (imsg.fd == -1)
				fatalx("MSG_STDOUT missing fd");

			c->stdout_fd = imsg.fd;
			c->stdout_event = bufferevent_new(c->stdout_fd,
			    NULL, NULL, server_client_out_callback, c);
			if (c->stdout_event == NULL)
				fatalx("failed to create stdout event");
			setblocking(c->stdout_fd, 0);

			break;
		case MSG_STDERR:
			if (datalen != 0)
				fatalx("bad MSG_STDERR size");
			if (imsg.fd == -1)
				fatalx("MSG_STDERR missing fd");

			c->stderr_fd = imsg.fd;
			c->stderr_event = bufferevent_new(c->stderr_fd,
			    NULL, NULL, server_client_err_callback, c);
			if (c->stderr_event == NULL)
				fatalx("failed to create stderr event");
			setblocking(c->stderr_fd, 0);

			break;
		case MSG_RESIZE:
			if (datalen != 0)
				fatalx("bad MSG_RESIZE size");

			if (tty_resize(&c->tty)) {
				recalculate_sizes();
				server_redraw_client(c);
			}
			break;
		case MSG_EXITING:
			if (datalen != 0)
				fatalx("bad MSG_EXITING size");

			c->session = NULL;
			tty_close(&c->tty);
			server_write_client(c, MSG_EXITED, NULL, 0);
			break;
		case MSG_WAKEUP:
		case MSG_UNLOCK:
			if (datalen != 0)
				fatalx("bad MSG_WAKEUP size");

			if (!(c->flags & CLIENT_SUSPENDED))
				break;
			c->flags &= ~CLIENT_SUSPENDED;

			if (gettimeofday(&c->activity_time, NULL) != 0)
				fatal("gettimeofday");
			if (c->session != NULL)
				session_update_activity(c->session);

			tty_start_tty(&c->tty);
			server_redraw_client(c);
			recalculate_sizes();
			break;
		case MSG_ENVIRON:
			if (datalen != sizeof environdata)
				fatalx("bad MSG_ENVIRON size");
			memcpy(&environdata, imsg.data, sizeof environdata);

			environdata.var[(sizeof environdata.var) - 1] = '\0';
			if (strchr(environdata.var, '=') != NULL)
				environ_put(&c->environ, environdata.var);
			break;
		case MSG_SHELL:
			if (datalen != 0)
				fatalx("bad MSG_SHELL size");

			server_client_msg_shell(c);
			break;
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
	}
}

/* Callback to send error message to client. */
void printflike2
server_client_msg_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	evbuffer_add_vprintf(ctx->cmdclient->stderr_event->output, fmt, ap);
	va_end(ap);

	bufferevent_write(ctx->cmdclient->stderr_event, "\n", 1);
	ctx->cmdclient->retcode = 1;
}

/* Callback to send print message to client. */
void printflike2
server_client_msg_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	evbuffer_add_vprintf(ctx->cmdclient->stdout_event->output, fmt, ap);
	va_end(ap);

	bufferevent_write(ctx->cmdclient->stdout_event, "\n", 1);
}

/* Callback to send print message to client, if not quiet. */
void printflike2
server_client_msg_info(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;

	if (options_get_number(&global_options, "quiet"))
		return;

	va_start(ap, fmt);
	evbuffer_add_vprintf(ctx->cmdclient->stdout_event->output, fmt, ap);
	va_end(ap);

	bufferevent_write(ctx->cmdclient->stdout_event, "\n", 1);
}

/* Handle command message. */
void
server_client_msg_command(struct client *c, struct msg_command_data *data)
{
	struct cmd_ctx	 ctx;
	struct cmd_list	*cmdlist = NULL;
	int		 argc;
	char	       **argv, *cause;

	ctx.error = server_client_msg_error;
	ctx.print = server_client_msg_print;
	ctx.info = server_client_msg_info;

	ctx.msgdata = data;
	ctx.curclient = NULL;

	ctx.cmdclient = c;

	argc = data->argc;
	data->argv[(sizeof data->argv) - 1] = '\0';
	if (cmd_unpack_argv(data->argv, sizeof data->argv, argc, &argv) != 0) {
		server_client_msg_error(&ctx, "command too long");
		goto error;
	}

	if (argc == 0) {
		argc = 1;
		argv = xcalloc(1, sizeof *argv);
		*argv = xstrdup("new-session");
	}

	if ((cmdlist = cmd_list_parse(argc, argv, &cause)) == NULL) {
		server_client_msg_error(&ctx, "%s", cause);
		cmd_free_argv(argc, argv);
		goto error;
	}
	cmd_free_argv(argc, argv);

	if (cmd_list_exec(cmdlist, &ctx) != 1)
		c->flags |= CLIENT_EXIT;
	cmd_list_free(cmdlist);
	return;

error:
	if (cmdlist != NULL)
		cmd_list_free(cmdlist);
	c->flags |= CLIENT_EXIT;
}

/* Handle identify message. */
void
server_client_msg_identify(
    struct client *c, struct msg_identify_data *data, int fd)
{
	int	tty_fd;

	c->cwd = NULL;
	data->cwd[(sizeof data->cwd) - 1] = '\0';
	if (*data->cwd != '\0')
		c->cwd = xstrdup(data->cwd);

	if (!isatty(fd))
	    return;
	if ((tty_fd = dup(fd)) == -1)
		fatal("dup failed");
	data->term[(sizeof data->term) - 1] = '\0';
	tty_init(&c->tty, tty_fd, data->term);
	if (data->flags & IDENTIFY_UTF8)
		c->tty.flags |= TTY_UTF8;
	if (data->flags & IDENTIFY_256COLOURS)
		c->tty.term_flags |= TERM_256COLOURS;
	else if (data->flags & IDENTIFY_88COLOURS)
		c->tty.term_flags |= TERM_88COLOURS;
	c->tty.key_callback = server_client_handle_key;
	c->tty.key_data = c;

	tty_resize(&c->tty);

	c->flags |= CLIENT_TERMINAL;
}

/* Handle shell message. */
void
server_client_msg_shell(struct client *c)
{
	struct msg_shell_data	 data;
	const char		*shell;

	shell = options_get_string(&global_s_options, "default-shell");

	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;
	if (strlcpy(data.shell, shell, sizeof data.shell) >= sizeof data.shell)
		strlcpy(data.shell, _PATH_BSHELL, sizeof data.shell);

	server_write_client(c, MSG_SHELL, &data, sizeof data);
	c->flags |= CLIENT_BAD;	/* it will die after exec */
}
