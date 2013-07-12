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
#include <sys/ioctl.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

void	server_client_check_focus(struct window_pane *);
void	server_client_check_resize(struct window_pane *);
void	server_client_check_mouse(struct client *, struct window_pane *);
void	server_client_repeat_timer(int, short, void *);
void	server_client_check_exit(struct client *);
void	server_client_check_redraw(struct client *);
void	server_client_set_title(struct client *);
void	server_client_reset_state(struct client *);
int	server_client_assume_paste(struct session *);

int	server_client_msg_dispatch(struct client *);
void	server_client_msg_command(struct client *, struct msg_command_data *);
void	server_client_msg_identify(
	    struct client *, struct msg_identify_data *, int);
void	server_client_msg_shell(struct client *);

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

	c->cmdq = cmdq_new(c);
	c->cmdq->client_exit = 1;

	c->stdin_data = evbuffer_new ();
	c->stdout_data = evbuffer_new ();
	c->stderr_data = evbuffer_new ();

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

	c->tty.mouse.xb = c->tty.mouse.button = 3;
	c->tty.mouse.x = c->tty.mouse.y = -1;
	c->tty.mouse.lx = c->tty.mouse.ly = -1;
	c->tty.mouse.sx = c->tty.mouse.sy = -1;
	c->tty.mouse.event = MOUSE_EVENT_UP;
	c->tty.mouse.flags = 0;

	c->flags |= CLIENT_FOCUSED;

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

/* Open client terminal if needed. */
int
server_client_open(struct client *c, struct session *s, char **cause)
{
	struct options	*oo = s != NULL ? &s->options : &global_s_options;
	char		*overrides;

	if (c->flags & CLIENT_CONTROL)
		return (0);

	if (!(c->flags & CLIENT_TERMINAL)) {
		*cause = xstrdup ("not a terminal");
		return (-1);
	}

	overrides = options_get_string(oo, "terminal-overrides");
	if (tty_open(&c->tty, overrides, cause) != 0)
		return (-1);

	return (0);
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

	evbuffer_free (c->stdin_data);
	evbuffer_free (c->stdout_data);
	if (c->stderr_data != c->stdout_data)
		evbuffer_free (c->stderr_data);

	status_free_jobs(&c->status_new);
	status_free_jobs(&c->status_old);
	screen_free(&c->status);

	free(c->title);

	evtimer_del(&c->repeat_timer);

	if (event_initialized(&c->identify_timer))
		evtimer_del(&c->identify_timer);

	free(c->message_string);
	if (event_initialized (&c->message_timer))
		evtimer_del(&c->message_timer);
	for (i = 0; i < ARRAY_LENGTH(&c->message_log); i++) {
		msg = &ARRAY_ITEM(&c->message_log, i);
		free(msg->msg);
	}
	ARRAY_FREE(&c->message_log);

	free(c->prompt_string);
	free(c->prompt_buffer);
	free(c->cwd);

	c->cmdq->dead = 1;
	cmdq_free(c->cmdq);
	c->cmdq = NULL;

	environ_free(&c->environ);

	close(c->ibuf.fd);
	imsg_clear(&c->ibuf);
	if (event_initialized(&c->event))
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

	server_add_accept(0); /* may be more file descriptors now */

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

	server_push_stdout(c);
	server_push_stderr(c);

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

/* Check for mouse keys. */
void
server_client_check_mouse(struct client *c, struct window_pane *wp)
{
	struct session		*s = c->session;
	struct options		*oo = &s->options;
	struct mouse_event	*m = &c->tty.mouse;
	int			 statusat;

	statusat = status_at_line(c);

	/* Is this a window selection click on the status line? */
	if (statusat != -1 && m->y == (u_int)statusat &&
	    options_get_number(oo, "mouse-select-window")) {
		if (m->event & MOUSE_EVENT_CLICK) {
			status_set_window_at(c, m->x);
		} else if (m->event == MOUSE_EVENT_WHEEL) {
			if (m->wheel == MOUSE_WHEEL_UP)
				session_previous(c->session, 0);
			else if (m->wheel == MOUSE_WHEEL_DOWN)
				session_next(c->session, 0);
			server_redraw_session(s);
		}
		recalculate_sizes();
		return;
	}

	/*
	 * Not on status line - adjust mouse position if status line is at the
	 * top and limit if at the bottom. From here on a struct mouse
	 * represents the offset onto the window itself.
	 */
	if (statusat == 0 && m->y > 0)
		m->y--;
	else if (statusat > 0 && m->y >= (u_int)statusat)
		m->y = statusat - 1;

	/* Is this a pane selection? Allow down only in copy mode. */
	if (options_get_number(oo, "mouse-select-pane") &&
	    (m->event == MOUSE_EVENT_DOWN || wp->mode != &window_copy_mode)) {
		window_set_active_at(wp->window, m->x, m->y);
		server_redraw_window_borders(wp->window);
		wp = wp->window->active; /* may have changed */
	}

	/* Check if trying to resize pane. */
	if (options_get_number(oo, "mouse-resize-pane"))
		layout_resize_pane_mouse(c);

	/* Update last and pass through to client. */
	window_pane_mouse(wp, c->session, m);
}

/* Is this fast enough to probably be a paste? */
int
server_client_assume_paste(struct session *s)
{
	struct timeval	tv;
	int		t;

	if ((t = options_get_number(&s->options, "assume-paste-time")) == 0)
		return (0);

	timersub(&s->activity_time, &s->last_activity_time, &tv);
	if (tv.tv_sec == 0 && tv.tv_usec < t * 1000)
		return (1);
	return (0);
}

/* Handle data key input from client. */
void
server_client_handle_key(struct client *c, int key)
{
	struct session		*s;
	struct window		*w;
	struct window_pane	*wp;
	struct timeval		 tv;
	struct key_binding	*bd;
	int		      	 xtimeout, isprefix, ispaste;

	/* Check the client is good to accept input. */
	if ((c->flags & (CLIENT_DEAD|CLIENT_SUSPENDED)) != 0)
		return;

	if (c->session == NULL)
		return;
	s = c->session;

	/* Update the activity timer. */
	if (gettimeofday(&c->activity_time, NULL) != 0)
		fatal("gettimeofday failed");

	memcpy(&s->last_activity_time, &s->activity_time,
	    sizeof s->last_activity_time);
	memcpy(&s->activity_time, &c->activity_time, sizeof s->activity_time);

	w = c->session->curw->window;
	wp = w->active;

	/* Special case: number keys jump to pane in identify mode. */
	if (c->flags & CLIENT_IDENTIFY && key >= '0' && key <= '9') {
		if (c->flags & CLIENT_READONLY)
			return;
		window_unzoom(w);
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
		server_client_check_mouse(c, wp);
		return;
	}

	/* Is this a prefix key? */
	if (key == options_get_number(&s->options, "prefix"))
		isprefix = 1;
	else if (key == options_get_number(&s->options, "prefix2"))
		isprefix = 1;
	else
		isprefix = 0;

	/* Treat prefix as a regular key when pasting is detected. */
	ispaste = server_client_assume_paste(s);
	if (ispaste)
		isprefix = 0;

	/* No previous prefix key. */
	if (!(c->flags & CLIENT_PREFIX)) {
		if (isprefix) {
			c->flags |= CLIENT_PREFIX;
			server_status_client(c);
			return;
		}

		/* Try as a non-prefix key binding. */
		if (ispaste || (bd = key_bindings_lookup(key)) == NULL) {
			if (!(c->flags & CLIENT_READONLY))
				window_pane_key(wp, s, key);
		} else
			key_bindings_dispatch(bd, c);
		return;
	}

	/* Prefix key already pressed. Reset prefix and lookup key. */
	c->flags &= ~CLIENT_PREFIX;
	server_status_client(c);
	if ((bd = key_bindings_lookup(key | KEYC_PREFIX)) == NULL) {
		/* If repeating, treat this as a key, else ignore. */
		if (c->flags & CLIENT_REPEAT) {
			c->flags &= ~CLIENT_REPEAT;
			if (isprefix)
				c->flags |= CLIENT_PREFIX;
			else if (!(c->flags & CLIENT_READONLY))
				window_pane_key(wp, s, key);
		}
		return;
	}

	/* If already repeating, but this key can't repeat, skip it. */
	if (c->flags & CLIENT_REPEAT && !bd->can_repeat) {
		c->flags &= ~CLIENT_REPEAT;
		if (isprefix)
			c->flags |= CLIENT_PREFIX;
		else if (!(c->flags & CLIENT_READONLY))
			window_pane_key(wp, s, key);
		return;
	}

	/* If this key can repeat, reset the repeat flags and timer. */
	xtimeout = options_get_number(&s->options, "repeat-time");
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
	 * their flags now. Also check pane focus and resize.
	 */
	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		w->flags &= ~WINDOW_REDRAW;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd != -1) {
				server_client_check_focus(wp);
				server_client_check_resize(wp);
			}
			wp->flags &= ~PANE_REDRAW;
		}
	}
}

/* Check if pane should be resized. */
void
server_client_check_resize(struct window_pane *wp)
{
	struct winsize	ws;

	if (!(wp->flags & PANE_RESIZE))
		return;

	memset(&ws, 0, sizeof ws);
	ws.ws_col = wp->sx;
	ws.ws_row = wp->sy;

	if (ioctl(wp->fd, TIOCSWINSZ, &ws) == -1)
		fatal("ioctl failed");

	wp->flags &= ~PANE_RESIZE;
}

/* Check whether pane should be focused. */
void
server_client_check_focus(struct window_pane *wp)
{
	u_int		 i;
	struct client	*c;
	int		 push;

	/* Are focus events off? */
	if (!options_get_number(&global_options, "focus-events"))
		return;

	/* Do we need to push the focus state? */
	push = wp->flags & PANE_FOCUSPUSH;
	wp->flags &= ~PANE_FOCUSPUSH;

	/* If we don't care about focus, forget it. */
	if (!(wp->base.mode & MODE_FOCUSON))
		return;

	/* If we're not the active pane in our window, we're not focused. */
	if (wp->window->active != wp)
		goto not_focused;

	/* If we're in a mode, we're not focused. */
	if (wp->screen != &wp->base)
		goto not_focused;

	/*
	 * If our window is the current window in any focused clients with an
	 * attached session, we're focused.
	 */
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		if (!(c->flags & CLIENT_FOCUSED))
			continue;
		if (c->session->flags & SESSION_UNATTACHED)
			continue;

		if (c->session->curw->window == wp->window)
			goto focused;
	}

not_focused:
	if (push || (wp->flags & PANE_FOCUSED))
		bufferevent_write(wp->event, "\033[O", 3);
	wp->flags &= ~PANE_FOCUSED;
	return;

focused:
	if (push || !(wp->flags & PANE_FOCUSED))
		bufferevent_write(wp->event, "\033[I", 3);
	wp->flags |= PANE_FOCUSED;
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
	int			 status, mode, o;

	if (c->flags & CLIENT_SUSPENDED)
		return;

	if (c->flags & CLIENT_CONTROL)
		return;

	tty_region(&c->tty, 0, c->tty.sy - 1);

	status = options_get_number(oo, "status");
	if (!window_pane_visible(wp) || wp->yoff + s->cy >= c->tty.sy - status)
		tty_cursor(&c->tty, 0, 0);
	else {
		o = status && options_get_number (oo, "status-position") == 0;
		tty_cursor(&c->tty, wp->xoff + s->cx, o + wp->yoff + s->cy);
	}

	/*
	 * Resizing panes with the mouse requires at least button mode to give
	 * a smooth appearance.
	 */
	mode = s->mode;
	if ((c->tty.mouse.flags & MOUSE_RESIZE_PANE) &&
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
void
server_client_repeat_timer(unused int fd, unused short events, void *data)
{
	struct client	*c = data;

	if (c->flags & CLIENT_REPEAT) {
		if (c->flags & CLIENT_PREFIX)
			server_status_client(c);
		c->flags &= ~(CLIENT_PREFIX|CLIENT_REPEAT);
	}
}

/* Check if client should be exited. */
void
server_client_check_exit(struct client *c)
{
	struct msg_exit_data	exitdata;

	if (!(c->flags & CLIENT_EXIT))
		return;

	if (EVBUFFER_LENGTH(c->stdin_data) != 0)
		return;
	if (EVBUFFER_LENGTH(c->stdout_data) != 0)
		return;
	if (EVBUFFER_LENGTH(c->stderr_data) != 0)
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

	if (c->flags & (CLIENT_CONTROL|CLIENT_SUSPENDED))
		return;

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
		free(c->title);
		c->title = xstrdup(title);
		tty_set_title(&c->tty, c->title);
	}
	free(title);
}

/* Dispatch message from client. */
int
server_client_msg_dispatch(struct client *c)
{
	struct imsg		 imsg;
	struct msg_command_data	 commanddata;
	struct msg_identify_data identifydata;
	struct msg_environ_data	 environdata;
	struct msg_stdin_data	 stdindata;
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
			memcpy(&identifydata, imsg.data, sizeof identifydata);
#ifdef __CYGWIN__
			imsg.fd = open(identifydata.ttyname, O_RDWR|O_NOCTTY);
#endif
			if (imsg.fd == -1)
				fatalx("MSG_IDENTIFY missing fd");

			server_client_msg_identify(c, &identifydata, imsg.fd);
			break;
		case MSG_STDIN:
			if (datalen != sizeof stdindata)
				fatalx("bad MSG_STDIN size");
			memcpy(&stdindata, imsg.data, sizeof stdindata);

			if (c->stdin_callback == NULL)
				break;
			if (stdindata.size <= 0)
				c->stdin_closed = 1;
			else {
				evbuffer_add(c->stdin_data, stdindata.data,
				    stdindata.size);
			}
			c->stdin_callback(c, c->stdin_closed,
			    c->stdin_callback_data);
			break;
		case MSG_RESIZE:
			if (datalen != 0)
				fatalx("bad MSG_RESIZE size");

			if (c->flags & CLIENT_CONTROL)
				break;
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

/* Handle command message. */
void
server_client_msg_command(struct client *c, struct msg_command_data *data)
{
	struct cmd_list	*cmdlist = NULL;
	int		 argc;
	char	       **argv, *cause;

	argc = data->argc;
	data->argv[(sizeof data->argv) - 1] = '\0';
	if (cmd_unpack_argv(data->argv, sizeof data->argv, argc, &argv) != 0) {
		cmdq_error(c->cmdq, "command too long");
		goto error;
	}

	if (argc == 0) {
		argc = 1;
		argv = xcalloc(1, sizeof *argv);
		*argv = xstrdup("new-session");
	}

	if ((cmdlist = cmd_list_parse(argc, argv, NULL, 0, &cause)) == NULL) {
		cmdq_error(c->cmdq, "%s", cause);
		cmd_free_argv(argc, argv);
		goto error;
	}
	cmd_free_argv(argc, argv);

	cmdq_run(c->cmdq, cmdlist);
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
	c->cwd = NULL;
	data->cwd[(sizeof data->cwd) - 1] = '\0';
	if (*data->cwd != '\0')
		c->cwd = xstrdup(data->cwd);

	if (data->flags & IDENTIFY_CONTROL) {
		c->stdin_callback = control_callback;
		evbuffer_free(c->stderr_data);
		c->stderr_data = c->stdout_data;
		c->flags |= CLIENT_CONTROL;
		if (data->flags & IDENTIFY_TERMIOS)
			evbuffer_add_printf(c->stdout_data, "\033P1000p");
		server_write_client(c, MSG_STDIN, NULL, 0);

		c->tty.fd = -1;
		c->tty.log_fd = -1;

		close(fd);
		return;
	}

	if (!isatty(fd)) {
		close(fd);
		return;
	}
	data->term[(sizeof data->term) - 1] = '\0';
	tty_init(&c->tty, c, fd, data->term);
	if (data->flags & IDENTIFY_UTF8)
		c->tty.flags |= TTY_UTF8;
	if (data->flags & IDENTIFY_256COLOURS)
		c->tty.term_flags |= TERM_256COLOURS;

	tty_resize(&c->tty);

	if (!(data->flags & IDENTIFY_CONTROL))
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
