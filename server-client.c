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
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

void		server_client_free(int, short, void *);
void		server_client_check_focus(struct window_pane *);
void		server_client_check_resize(struct window_pane *);
key_code	server_client_check_mouse(struct client *);
void		server_client_repeat_timer(int, short, void *);
void		server_client_check_exit(struct client *);
void		server_client_check_redraw(struct client *);
void		server_client_set_title(struct client *);
void		server_client_reset_state(struct client *);
int		server_client_assume_paste(struct session *);

void		server_client_dispatch(struct imsg *, void *);
void		server_client_dispatch_command(struct client *, struct imsg *);
void		server_client_dispatch_identify(struct client *, struct imsg *);
void		server_client_dispatch_shell(struct client *);

/* Check if this client is inside this server. */
int
server_client_check_nested(struct client *c)
{
	struct environ_entry	*envent;
	struct window_pane	*wp;

	if (c->tty.path == NULL)
		return (0);

	envent = environ_find(c->environ, "TMUX");
	if (envent == NULL || *envent->value == '\0')
		return (0);

	RB_FOREACH(wp, window_pane_tree, &all_window_panes) {
		if (strcmp(wp->tty, c->tty.path) == 0)
			return (1);
	}
	return (0);
}

/* Set client key table. */
void
server_client_set_key_table(struct client *c, const char *name)
{
	if (name == NULL)
		name = server_client_get_key_table(c);

	key_bindings_unref_table(c->keytable);
	c->keytable = key_bindings_get_table(name, 1);
	c->keytable->references++;
}

/* Get default key table. */
const char *
server_client_get_key_table(struct client *c)
{
	struct session	*s = c->session;
	const char	*name;

	if (s == NULL)
		return ("root");

	name = options_get_string(s->options, "key-table");
	if (*name == '\0')
		return ("root");
	return (name);
}

/* Create a new client. */
void
server_client_create(int fd)
{
	struct client	*c;

	setblocking(fd, 0);

	c = xcalloc(1, sizeof *c);
	c->references = 1;
	c->peer = proc_add_peer(server_proc, fd, server_client_dispatch, c);

	if (gettimeofday(&c->creation_time, NULL) != 0)
		fatal("gettimeofday failed");
	memcpy(&c->activity_time, &c->creation_time, sizeof c->activity_time);

	c->environ = environ_create();

	c->fd = -1;
	c->cwd = NULL;

	c->cmdq = cmdq_new(c);
	c->cmdq->client_exit = 1;

	c->stdin_data = evbuffer_new();
	c->stdout_data = evbuffer_new();
	c->stderr_data = evbuffer_new();

	c->tty.fd = -1;
	c->title = NULL;

	c->session = NULL;
	c->last_session = NULL;
	c->tty.sx = 80;
	c->tty.sy = 24;

	screen_init(&c->status, c->tty.sx, 1, 0);

	c->message_string = NULL;
	TAILQ_INIT(&c->message_log);

	c->prompt_string = NULL;
	c->prompt_buffer = NULL;
	c->prompt_index = 0;

	c->flags |= CLIENT_FOCUSED;

	c->keytable = key_bindings_get_table("root", 1);
	c->keytable->references++;

	evtimer_set(&c->repeat_timer, server_client_repeat_timer, c);

	TAILQ_INSERT_TAIL(&clients, c, entry);
	log_debug("new client %p", c);
}

/* Open client terminal if needed. */
int
server_client_open(struct client *c, char **cause)
{
	if (c->flags & CLIENT_CONTROL)
		return (0);

	if (strcmp(c->ttyname, "/dev/tty") == 0) {
		*cause = xstrdup("can't use /dev/tty");
		return (-1);
	}

	if (!(c->flags & CLIENT_TERMINAL)) {
		*cause = xstrdup("not a terminal");
		return (-1);
	}

	if (tty_open(&c->tty, cause) != 0)
		return (-1);

	return (0);
}

/* Lost a client. */
void
server_client_lost(struct client *c)
{
	struct message_entry	*msg, *msg1;

	c->flags |= CLIENT_DEAD;

	status_prompt_clear(c);
	status_message_clear(c);

	if (c->stdin_callback != NULL)
		c->stdin_callback(c, 1, c->stdin_callback_data);

	TAILQ_REMOVE(&clients, c, entry);
	log_debug("lost client %p", c);

	/*
	 * If CLIENT_TERMINAL hasn't been set, then tty_init hasn't been called
	 * and tty_free might close an unrelated fd.
	 */
	if (c->flags & CLIENT_TERMINAL)
		tty_free(&c->tty);
	free(c->ttyname);
	free(c->term);

	evbuffer_free(c->stdin_data);
	evbuffer_free(c->stdout_data);
	if (c->stderr_data != c->stdout_data)
		evbuffer_free(c->stderr_data);

	if (event_initialized(&c->status_timer))
		evtimer_del(&c->status_timer);
	screen_free(&c->status);

	free(c->title);
	free((void *)c->cwd);

	evtimer_del(&c->repeat_timer);

	key_bindings_unref_table(c->keytable);

	if (event_initialized(&c->identify_timer))
		evtimer_del(&c->identify_timer);

	free(c->message_string);
	if (event_initialized(&c->message_timer))
		evtimer_del(&c->message_timer);
	TAILQ_FOREACH_SAFE(msg, &c->message_log, entry, msg1) {
		free(msg->msg);
		TAILQ_REMOVE(&c->message_log, msg, entry);
		free(msg);
	}

	free(c->prompt_string);
	free(c->prompt_buffer);

	c->cmdq->flags |= CMD_Q_DEAD;
	cmdq_free(c->cmdq);
	c->cmdq = NULL;

	environ_free(c->environ);

	proc_remove_peer(c->peer);
	c->peer = NULL;

	server_client_unref(c);

	server_add_accept(0); /* may be more file descriptors now */

	recalculate_sizes();
	server_check_unattached();
	server_update_socket();
}

/* Remove reference from a client. */
void
server_client_unref(struct client *c)
{
	log_debug("unref client %p (%d references)", c, c->references);

	c->references--;
	if (c->references == 0)
		event_once(-1, EV_TIMEOUT, server_client_free, c, NULL);
}

/* Free dead client. */
void
server_client_free(__unused int fd, __unused short events, void *arg)
{
	struct client	*c = arg;

	log_debug("free client %p (%d references)", c, c->references);

	if (c->references == 0)
		free(c);
}

/* Detach a client. */
void
server_client_detach(struct client *c, enum msgtype msgtype)
{
	struct session	*s = c->session;

	if (s == NULL)
		return;

	hooks_run(c->session->hooks, c, NULL, "client-detached");
	proc_send_s(c->peer, msgtype, s->name);
}

/* Check for mouse keys. */
key_code
server_client_check_mouse(struct client *c)
{
	struct session				*s = c->session;
	struct mouse_event			*m = &c->tty.mouse;
	struct window				*w;
	struct window_pane			*wp;
	enum { NOTYPE, DOWN, UP, DRAG, WHEEL }	 type = NOTYPE;
	enum { NOWHERE, PANE, STATUS, BORDER }	 where = NOWHERE;
	u_int					 x, y, b;
	key_code				 key;

	log_debug("mouse %02x at %u,%u (last %u,%u) (%d)", m->b, m->x, m->y,
	    m->lx, m->ly, c->tty.mouse_drag_flag);

	/* What type of event is this? */
	if (MOUSE_DRAG(m->b)) {
		type = DRAG;
		if (c->tty.mouse_drag_flag) {
			x = m->x, y = m->y, b = m->b;
			log_debug("drag update at %u,%u", x, y);
		} else {
			x = m->lx, y = m->ly, b = m->lb;
			log_debug("drag start at %u,%u", x, y);
		}
	} else if (MOUSE_WHEEL(m->b)) {
		type = WHEEL;
		x = m->x, y = m->y, b = m->b;
		log_debug("wheel at %u,%u", x, y);
	} else if (MOUSE_BUTTONS(m->b) == 3) {
		type = UP;
		x = m->x, y = m->y, b = m->lb;
		log_debug("up at %u,%u", x, y);
	} else {
		type = DOWN;
		x = m->x, y = m->y, b = m->b;
		log_debug("down at %u,%u", x, y);
	}
	if (type == NOTYPE)
		return (KEYC_UNKNOWN);

	/* Always save the session. */
	m->s = s->id;

	/* Is this on the status line? */
	m->statusat = status_at_line(c);
	if (m->statusat != -1 && y == (u_int)m->statusat) {
		w = status_get_window_at(c, x);
		if (w == NULL)
			return (KEYC_UNKNOWN);
		m->w = w->id;
		where = STATUS;
	} else
		m->w = -1;

	/* Not on status line. Adjust position and check for border or pane. */
	if (where == NOWHERE) {
		if (m->statusat == 0 && y > 0)
			y--;
		else if (m->statusat > 0 && y >= (u_int)m->statusat)
			y = m->statusat - 1;

		TAILQ_FOREACH(wp, &s->curw->window->panes, entry) {
			if ((wp->xoff + wp->sx == x &&
			    wp->yoff <= 1 + y &&
			    wp->yoff + wp->sy >= y) ||
			    (wp->yoff + wp->sy == y &&
			    wp->xoff <= 1 + x &&
			    wp->xoff + wp->sx >= x))
				break;
		}
		if (wp != NULL)
			where = BORDER;
		else {
			wp = window_get_active_at(s->curw->window, x, y);
			if (wp != NULL) {
				where = PANE;
				log_debug("mouse at %u,%u is on pane %%%u",
				    x, y, wp->id);
			}
		}
		if (where == NOWHERE)
			return (KEYC_UNKNOWN);
		m->wp = wp->id;
		m->w = wp->window->id;
	} else
		m->wp = -1;

	/* Stop dragging if needed. */
	if (type != DRAG && c->tty.mouse_drag_flag) {
		if (c->tty.mouse_drag_release != NULL)
			c->tty.mouse_drag_release(c, m);

		c->tty.mouse_drag_update = NULL;
		c->tty.mouse_drag_release = NULL;

		/*
		 * End a mouse drag by passing a MouseDragEnd key corresponding
		 * to the button that started the drag.
		 */
		switch (c->tty.mouse_drag_flag) {
		case 1:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND1_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND1_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND1_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND2_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND2_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND2_BORDER;
			break;
		case 3:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND3_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND3_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND3_BORDER;
			break;
		default:
			key = KEYC_MOUSE;
			break;
		}
		c->tty.mouse_drag_flag = 0;

		return (key);
	}

	/* Convert to a key binding. */
	key = KEYC_UNKNOWN;
	switch (type) {
	case NOTYPE:
		break;
	case DRAG:
		if (c->tty.mouse_drag_update != NULL)
			c->tty.mouse_drag_update(c, m);
		else {
			switch (MOUSE_BUTTONS(b)) {
			case 0:
				if (where == PANE)
					key = KEYC_MOUSEDRAG1_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG1_STATUS;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG1_BORDER;
				break;
			case 1:
				if (where == PANE)
					key = KEYC_MOUSEDRAG2_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG2_STATUS;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG2_BORDER;
				break;
			case 2:
				if (where == PANE)
					key = KEYC_MOUSEDRAG3_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG3_STATUS;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG3_BORDER;
				break;
			}
		}

		/*
		 * Begin a drag by setting the flag to a non-zero value that
		 * corresponds to the mouse button in use.
		 */
		c->tty.mouse_drag_flag = MOUSE_BUTTONS(b) + 1;
		break;
	case WHEEL:
		if (MOUSE_BUTTONS(b) == MOUSE_WHEEL_UP) {
			if (where == PANE)
				key = KEYC_WHEELUP_PANE;
			if (where == STATUS)
				key = KEYC_WHEELUP_STATUS;
			if (where == BORDER)
				key = KEYC_WHEELUP_BORDER;
		} else {
			if (where == PANE)
				key = KEYC_WHEELDOWN_PANE;
			if (where == STATUS)
				key = KEYC_WHEELDOWN_STATUS;
			if (where == BORDER)
				key = KEYC_WHEELDOWN_BORDER;
		}
		break;
	case UP:
		switch (MOUSE_BUTTONS(b)) {
		case 0:
			if (where == PANE)
				key = KEYC_MOUSEUP1_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP1_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEUP1_BORDER;
			break;
		case 1:
			if (where == PANE)
				key = KEYC_MOUSEUP2_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP2_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEUP2_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_MOUSEUP3_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP3_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEUP3_BORDER;
			break;
		}
		break;
	case DOWN:
		switch (MOUSE_BUTTONS(b)) {
		case 0:
			if (where == PANE)
				key = KEYC_MOUSEDOWN1_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN1_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN1_BORDER;
			break;
		case 1:
			if (where == PANE)
				key = KEYC_MOUSEDOWN2_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN2_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN2_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_MOUSEDOWN3_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN3_STATUS;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN3_BORDER;
			break;
		}
		break;
	}
	if (key == KEYC_UNKNOWN)
		return (KEYC_UNKNOWN);

	/* Apply modifiers if any. */
	if (b & MOUSE_MASK_META)
		key |= KEYC_ESCAPE;
	if (b & MOUSE_MASK_CTRL)
		key |= KEYC_CTRL;
	if (b & MOUSE_MASK_SHIFT)
		key |= KEYC_SHIFT;

	return (key);
}

/* Is this fast enough to probably be a paste? */
int
server_client_assume_paste(struct session *s)
{
	struct timeval	tv;
	int		t;

	if ((t = options_get_number(s->options, "assume-paste-time")) == 0)
		return (0);

	timersub(&s->activity_time, &s->last_activity_time, &tv);
	if (tv.tv_sec == 0 && tv.tv_usec < t * 1000) {
		log_debug("session %s pasting (flag %d)", s->name,
		    !!(s->flags & SESSION_PASTING));
		if (s->flags & SESSION_PASTING)
			return (1);
		s->flags |= SESSION_PASTING;
		return (0);
	}
	log_debug("session %s not pasting", s->name);
	s->flags &= ~SESSION_PASTING;
	return (0);
}

/* Handle data key input from client. */
void
server_client_handle_key(struct client *c, key_code key)
{
	struct mouse_event	*m = &c->tty.mouse;
	struct session		*s = c->session;
	struct window		*w;
	struct window_pane	*wp;
	struct timeval		 tv;
	struct key_table	*table;
	struct key_binding	 bd_find, *bd;
	int			 xtimeout;

	/* Check the client is good to accept input. */
	if (s == NULL || (c->flags & (CLIENT_DEAD|CLIENT_SUSPENDED)) != 0)
		return;
	w = s->curw->window;

	/* Update the activity timer. */
	if (gettimeofday(&c->activity_time, NULL) != 0)
		fatal("gettimeofday failed");
	session_update_activity(s, &c->activity_time);

	/* Number keys jump to pane in identify mode. */
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
		key = server_client_check_mouse(c);
		if (key == KEYC_UNKNOWN)
			return;

		m->valid = 1;
		m->key = key;

		if (!options_get_number(s->options, "mouse"))
			goto forward;
	} else
		m->valid = 0;

	/* Treat everything as a regular key when pasting is detected. */
	if (!KEYC_IS_MOUSE(key) && server_client_assume_paste(s))
		goto forward;

retry:
	/* Try to see if there is a key binding in the current table. */
	bd_find.key = key;
	bd = RB_FIND(key_bindings, &c->keytable->key_bindings, &bd_find);
	if (bd != NULL) {
		/*
		 * Key was matched in this table. If currently repeating but a
		 * non-repeating binding was found, stop repeating and try
		 * again in the root table.
		 */
		if ((c->flags & CLIENT_REPEAT) && !bd->can_repeat) {
			server_client_set_key_table(c, NULL);
			c->flags &= ~CLIENT_REPEAT;
			server_status_client(c);
			goto retry;
		}

		/*
		 * Take a reference to this table to make sure the key binding
		 * doesn't disappear.
		 */
		table = c->keytable;
		table->references++;

		/*
		 * If this is a repeating key, start the timer. Otherwise reset
		 * the client back to the root table.
		 */
		xtimeout = options_get_number(s->options, "repeat-time");
		if (xtimeout != 0 && bd->can_repeat) {
			c->flags |= CLIENT_REPEAT;

			tv.tv_sec = xtimeout / 1000;
			tv.tv_usec = (xtimeout % 1000) * 1000L;
			evtimer_del(&c->repeat_timer);
			evtimer_add(&c->repeat_timer, &tv);
		} else {
			c->flags &= ~CLIENT_REPEAT;
			server_client_set_key_table(c, NULL);
		}
		server_status_client(c);

		/* Dispatch the key binding. */
		key_bindings_dispatch(bd, c, m);
		key_bindings_unref_table(table);
		return;
	}

	/*
	 * No match in this table. If repeating, switch the client back to the
	 * root table and try again.
	 */
	if (c->flags & CLIENT_REPEAT) {
		server_client_set_key_table(c, NULL);
		c->flags &= ~CLIENT_REPEAT;
		server_status_client(c);
		goto retry;
	}

	/* If no match and we're not in the root table, that's it. */
	if (strcmp(c->keytable->name, server_client_get_key_table(c)) != 0) {
		server_client_set_key_table(c, NULL);
		server_status_client(c);
		return;
	}

	/*
	 * No match, but in the root table. Prefix switches to the prefix table
	 * and everything else is passed through.
	 */
	if (key == (key_code)options_get_number(s->options, "prefix") ||
	    key == (key_code)options_get_number(s->options, "prefix2")) {
		server_client_set_key_table(c, "prefix");
		server_status_client(c);
		return;
	}

forward:
	if (c->flags & CLIENT_READONLY)
		return;
	if (KEYC_IS_MOUSE(key))
		wp = cmd_mouse_pane(m, NULL, NULL);
	else
		wp = w->active;
	if (wp != NULL)
		window_pane_key(wp, c, s, key, m);
}

/* Client functions that need to happen every loop. */
void
server_client_loop(void)
{
	struct client		*c;
	struct window		*w;
	struct window_pane	*wp;

	TAILQ_FOREACH(c, &clients, entry) {
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
	RB_FOREACH(w, windows, &windows) {
		w->flags &= ~WINDOW_REDRAW;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd != -1) {
				server_client_check_focus(wp);
				server_client_check_resize(wp);
			}
			wp->flags &= ~PANE_REDRAW;
		}
		check_window_name(w);
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

	if (ioctl(wp->fd, TIOCSWINSZ, &ws) == -1) {
#ifdef __sun
		/*
		 * Some versions of Solaris apparently can return an error when
		 * resizing; don't know why this happens, can't reproduce on
		 * other platforms and ignoring it doesn't seem to cause any
		 * issues.
		 */
		if (errno != EINVAL && errno != ENXIO)
#endif
		fatal("ioctl failed");
	}

	wp->flags &= ~PANE_RESIZE;
}

/* Check whether pane should be focused. */
void
server_client_check_focus(struct window_pane *wp)
{
	struct client	*c;
	int		 push;

	/* Are focus events off? */
	if (!options_get_number(global_options, "focus-events"))
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
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL || !(c->flags & CLIENT_FOCUSED))
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
	struct options		*oo = c->session->options;
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
		o = status && options_get_number(oo, "status-position") == 0;
		tty_cursor(&c->tty, wp->xoff + s->cx, o + wp->yoff + s->cy);
	}

	/*
	 * Set mouse mode if requested. To support dragging, always use button
	 * mode.
	 */
	mode = s->mode;
	if (options_get_number(oo, "mouse"))
		mode = (mode & ~ALL_MOUSE_MODES) | MODE_MOUSE_BUTTON;

	/* Set the terminal mode and reset attributes. */
	tty_update_mode(&c->tty, mode, s);
	tty_reset(&c->tty);
}

/* Repeat time callback. */
void
server_client_repeat_timer(__unused int fd, __unused short events, void *data)
{
	struct client	*c = data;

	if (c->flags & CLIENT_REPEAT) {
		server_client_set_key_table(c, NULL);
		c->flags &= ~CLIENT_REPEAT;
		server_status_client(c);
	}
}

/* Check if client should be exited. */
void
server_client_check_exit(struct client *c)
{
	if (!(c->flags & CLIENT_EXIT))
		return;

	if (EVBUFFER_LENGTH(c->stdin_data) != 0)
		return;
	if (EVBUFFER_LENGTH(c->stdout_data) != 0)
		return;
	if (EVBUFFER_LENGTH(c->stderr_data) != 0)
		return;

	proc_send(c->peer, MSG_EXIT, -1, &c->retval, sizeof c->retval);
	c->flags &= ~CLIENT_EXIT;
}

/* Check for client redraws. */
void
server_client_check_redraw(struct client *c)
{
	struct session		*s = c->session;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	int		 	 flags, redraw;

	if (c->flags & (CLIENT_CONTROL|CLIENT_SUSPENDED))
		return;

	if (c->flags & (CLIENT_REDRAW|CLIENT_STATUS)) {
		if (options_get_number(s->options, "set-titles"))
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

	flags = tty->flags & (TTY_FREEZE|TTY_NOCURSOR);
	tty->flags = (tty->flags & ~TTY_FREEZE) | TTY_NOCURSOR;

	if (c->flags & CLIENT_REDRAW) {
		tty_update_mode(tty, tty->mode, NULL);
		screen_redraw_screen(c, 1, 1, 1);
		c->flags &= ~(CLIENT_STATUS|CLIENT_BORDERS);
	} else if (c->flags & CLIENT_REDRAWWINDOW) {
		tty_update_mode(tty, tty->mode, NULL);
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry)
			screen_redraw_pane(c, wp);
		c->flags &= ~CLIENT_REDRAWWINDOW;
	} else {
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry) {
			if (wp->flags & PANE_REDRAW) {
				tty_update_mode(tty, tty->mode, NULL);
				screen_redraw_pane(c, wp);
			}
		}
	}

	if (c->flags & CLIENT_BORDERS) {
		tty_update_mode(tty, tty->mode, NULL);
		screen_redraw_screen(c, 0, 0, 1);
	}

	if (c->flags & CLIENT_STATUS) {
		tty_update_mode(tty, tty->mode, NULL);
		screen_redraw_screen(c, 0, 1, 0);
	}

	tty->flags = (tty->flags & ~(TTY_FREEZE|TTY_NOCURSOR)) | flags;
	tty_update_mode(tty, tty->mode, NULL);

	c->flags &= ~(CLIENT_REDRAW|CLIENT_BORDERS|CLIENT_STATUS|
	    CLIENT_STATUSFORCE);
}

/* Set client title. */
void
server_client_set_title(struct client *c)
{
	struct session		*s = c->session;
	const char		*template;
	char			*title;
	struct format_tree	*ft;

	template = options_get_string(s->options, "set-titles-string");

	ft = format_create(NULL, 0);
	format_defaults(ft, c, NULL, NULL, NULL);

	title = format_expand_time(ft, template, time(NULL));
	if (c->title == NULL || strcmp(title, c->title) != 0) {
		free(c->title);
		c->title = xstrdup(title);
		tty_set_title(&c->tty, c->title);
	}
	free(title);

	format_free(ft);
}

/* Dispatch message from client. */
void
server_client_dispatch(struct imsg *imsg, void *arg)
{
	struct client		*c = arg;
	struct msg_stdin_data	 stdindata;
	const char		*data;
	ssize_t			 datalen;
	struct session		*s;

	if (c->flags & CLIENT_DEAD)
		return;

	if (imsg == NULL) {
		server_client_lost(c);
		return;
	}

	data = imsg->data;
	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;

	switch (imsg->hdr.type) {
	case MSG_IDENTIFY_FLAGS:
	case MSG_IDENTIFY_TERM:
	case MSG_IDENTIFY_TTYNAME:
	case MSG_IDENTIFY_CWD:
	case MSG_IDENTIFY_STDIN:
	case MSG_IDENTIFY_ENVIRON:
	case MSG_IDENTIFY_CLIENTPID:
	case MSG_IDENTIFY_DONE:
		server_client_dispatch_identify(c, imsg);
		break;
	case MSG_COMMAND:
		server_client_dispatch_command(c, imsg);
		break;
	case MSG_STDIN:
		if (datalen != sizeof stdindata)
			fatalx("bad MSG_STDIN size");
		memcpy(&stdindata, data, sizeof stdindata);

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
		if (c->session != NULL)
			hooks_run(c->session->hooks, c, NULL, "client-resized");
		break;
	case MSG_EXITING:
		if (datalen != 0)
			fatalx("bad MSG_EXITING size");

		c->session = NULL;
		tty_close(&c->tty);
		proc_send(c->peer, MSG_EXITED, -1, NULL, 0);
		break;
	case MSG_WAKEUP:
	case MSG_UNLOCK:
		if (datalen != 0)
			fatalx("bad MSG_WAKEUP size");

		if (!(c->flags & CLIENT_SUSPENDED))
			break;
		c->flags &= ~CLIENT_SUSPENDED;

		if (c->tty.fd == -1) /* exited in the meantime */
			break;
		s = c->session;

		if (gettimeofday(&c->activity_time, NULL) != 0)
			fatal("gettimeofday failed");
		if (s != NULL)
			session_update_activity(s, &c->activity_time);

		tty_start_tty(&c->tty);
		server_redraw_client(c);
		recalculate_sizes();
		break;
	case MSG_SHELL:
		if (datalen != 0)
			fatalx("bad MSG_SHELL size");

		server_client_dispatch_shell(c);
		break;
	}
}

/* Handle command message. */
void
server_client_dispatch_command(struct client *c, struct imsg *imsg)
{
	struct msg_command_data	  data;
	char			 *buf;
	size_t			  len;
	struct cmd_list		 *cmdlist = NULL;
	int			  argc;
	char			**argv, *cause;

	if (imsg->hdr.len - IMSG_HEADER_SIZE < sizeof data)
		fatalx("bad MSG_COMMAND size");
	memcpy(&data, imsg->data, sizeof data);

	buf = (char *)imsg->data + sizeof data;
	len = imsg->hdr.len  - IMSG_HEADER_SIZE - sizeof data;
	if (len > 0 && buf[len - 1] != '\0')
		fatalx("bad MSG_COMMAND string");

	argc = data.argc;
	if (cmd_unpack_argv(buf, len, argc, &argv) != 0) {
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

	if (c != cfg_client || cfg_finished)
		cmdq_run(c->cmdq, cmdlist, NULL);
	else
		cmdq_append(c->cmdq, cmdlist, NULL);
	cmd_list_free(cmdlist);
	return;

error:
	if (cmdlist != NULL)
		cmd_list_free(cmdlist);

	c->flags |= CLIENT_EXIT;
}

/* Handle identify message. */
void
server_client_dispatch_identify(struct client *c, struct imsg *imsg)
{
	const char	*data, *home;
	size_t	 	 datalen;
	int		 flags;

	if (c->flags & CLIENT_IDENTIFIED)
		fatalx("out-of-order identify message");

	data = imsg->data;
	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;

	switch (imsg->hdr.type)	{
	case MSG_IDENTIFY_FLAGS:
		if (datalen != sizeof flags)
			fatalx("bad MSG_IDENTIFY_FLAGS size");
		memcpy(&flags, data, sizeof flags);
		c->flags |= flags;
		log_debug("client %p IDENTIFY_FLAGS %#x", c, flags);
		break;
	case MSG_IDENTIFY_TERM:
		if (datalen == 0 || data[datalen - 1] != '\0')
			fatalx("bad MSG_IDENTIFY_TERM string");
		c->term = xstrdup(data);
		log_debug("client %p IDENTIFY_TERM %s", c, data);
		break;
	case MSG_IDENTIFY_TTYNAME:
		if (datalen == 0 || data[datalen - 1] != '\0')
			fatalx("bad MSG_IDENTIFY_TTYNAME string");
		c->ttyname = xstrdup(data);
		log_debug("client %p IDENTIFY_TTYNAME %s", c, data);
		break;
	case MSG_IDENTIFY_CWD:
		if (datalen == 0 || data[datalen - 1] != '\0')
			fatalx("bad MSG_IDENTIFY_CWD string");
		if (access(data, X_OK) == 0)
			c->cwd = xstrdup(data);
		else if ((home = find_home()) != NULL)
			c->cwd = xstrdup(home);
		else
			c->cwd = xstrdup("/");
		log_debug("client %p IDENTIFY_CWD %s", c, data);
		break;
	case MSG_IDENTIFY_STDIN:
		if (datalen != 0)
			fatalx("bad MSG_IDENTIFY_STDIN size");
		c->fd = imsg->fd;
		log_debug("client %p IDENTIFY_STDIN %d", c, imsg->fd);
		break;
	case MSG_IDENTIFY_ENVIRON:
		if (datalen == 0 || data[datalen - 1] != '\0')
			fatalx("bad MSG_IDENTIFY_ENVIRON string");
		if (strchr(data, '=') != NULL)
			environ_put(c->environ, data);
		log_debug("client %p IDENTIFY_ENVIRON %s", c, data);
		break;
	case MSG_IDENTIFY_CLIENTPID:
		if (datalen != sizeof c->pid)
			fatalx("bad MSG_IDENTIFY_CLIENTPID size");
		memcpy(&c->pid, data, sizeof c->pid);
		log_debug("client %p IDENTIFY_CLIENTPID %ld", c, (long)c->pid);
		break;
	default:
		break;
	}

	if (imsg->hdr.type != MSG_IDENTIFY_DONE)
		return;
	c->flags |= CLIENT_IDENTIFIED;

#ifdef __CYGWIN__
	c->fd = open(c->ttyname, O_RDWR|O_NOCTTY);
#endif

	if (c->flags & CLIENT_CONTROL) {
		c->stdin_callback = control_callback;

		evbuffer_free(c->stderr_data);
		c->stderr_data = c->stdout_data;

		if (c->flags & CLIENT_CONTROLCONTROL)
			evbuffer_add_printf(c->stdout_data, "\033P1000p");
		proc_send(c->peer, MSG_STDIN, -1, NULL, 0);

		c->tty.fd = -1;

		close(c->fd);
		c->fd = -1;

		return;
	}

	if (c->fd == -1)
		return;
	if (tty_init(&c->tty, c, c->fd, c->term) != 0) {
		close(c->fd);
		c->fd = -1;
		return;
	}
	if (c->flags & CLIENT_UTF8)
		c->tty.flags |= TTY_UTF8;
	if (c->flags & CLIENT_256COLOURS)
		c->tty.term_flags |= TERM_256COLOURS;

	tty_resize(&c->tty);

	if (!(c->flags & CLIENT_CONTROL))
		c->flags |= CLIENT_TERMINAL;
}

/* Handle shell message. */
void
server_client_dispatch_shell(struct client *c)
{
	const char	*shell;

	shell = options_get_string(global_s_options, "default-shell");
	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;
	proc_send_s(c->peer, MSG_SHELL, shell);

	proc_kill_peer(c->peer);
}

/* Event callback to push more stdout data if any left. */
static void
server_client_stdout_cb(__unused int fd, __unused short events, void *arg)
{
	struct client	*c = arg;

	if (~c->flags & CLIENT_DEAD)
		server_client_push_stdout(c);
	server_client_unref(c);
}

/* Push stdout to client if possible. */
void
server_client_push_stdout(struct client *c)
{
	struct msg_stdout_data data;
	size_t                 sent, left;

	left = EVBUFFER_LENGTH(c->stdout_data);
	while (left != 0) {
		sent = left;
		if (sent > sizeof data.data)
			sent = sizeof data.data;
		memcpy(data.data, EVBUFFER_DATA(c->stdout_data), sent);
		data.size = sent;

		if (proc_send(c->peer, MSG_STDOUT, -1, &data, sizeof data) != 0)
			break;
		evbuffer_drain(c->stdout_data, sent);

		left = EVBUFFER_LENGTH(c->stdout_data);
		log_debug("%s: client %p, sent %zu, left %zu", __func__, c,
		    sent, left);
	}
	if (left != 0) {
		c->references++;
		event_once(-1, EV_TIMEOUT, server_client_stdout_cb, c, NULL);
		log_debug("%s: client %p, queued", __func__, c);
	}
}

/* Event callback to push more stderr data if any left. */
static void
server_client_stderr_cb(__unused int fd, __unused short events, void *arg)
{
	struct client	*c = arg;

	if (~c->flags & CLIENT_DEAD)
		server_client_push_stderr(c);
	server_client_unref(c);
}

/* Push stderr to client if possible. */
void
server_client_push_stderr(struct client *c)
{
	struct msg_stderr_data data;
	size_t                 sent, left;

	if (c->stderr_data == c->stdout_data) {
		server_client_push_stdout(c);
		return;
	}

	left = EVBUFFER_LENGTH(c->stderr_data);
	while (left != 0) {
		sent = left;
		if (sent > sizeof data.data)
			sent = sizeof data.data;
		memcpy(data.data, EVBUFFER_DATA(c->stderr_data), sent);
		data.size = sent;

		if (proc_send(c->peer, MSG_STDERR, -1, &data, sizeof data) != 0)
			break;
		evbuffer_drain(c->stderr_data, sent);

		left = EVBUFFER_LENGTH(c->stderr_data);
		log_debug("%s: client %p, sent %zu, left %zu", __func__, c,
		    sent, left);
	}
	if (left != 0) {
		c->references++;
		event_once(-1, EV_TIMEOUT, server_client_stderr_cb, c, NULL);
		log_debug("%s: client %p, queued", __func__, c);
	}
}
