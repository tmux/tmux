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
#include <imsg.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

static void	server_client_free(int, short, void *);
static void	server_client_check_focus(struct window_pane *);
static void	server_client_check_resize(struct window_pane *);
static key_code	server_client_check_mouse(struct client *, struct key_event *);
static void	server_client_repeat_timer(int, short, void *);
static void	server_client_click_timer(int, short, void *);
static void	server_client_check_exit(struct client *);
static void	server_client_check_redraw(struct client *);
static void	server_client_set_title(struct client *);
static void	server_client_reset_state(struct client *);
static int	server_client_assume_paste(struct session *);
static void	server_client_clear_overlay(struct client *);

static void	server_client_dispatch(struct imsg *, void *);
static void	server_client_dispatch_command(struct client *, struct imsg *);
static void	server_client_dispatch_identify(struct client *, struct imsg *);
static void	server_client_dispatch_shell(struct client *);

/* Number of attached clients. */
u_int
server_client_how_many(void)
{
	struct client  	*c;
	u_int		 n;

	n = 0;
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL && (~c->flags & CLIENT_DETACHING))
			n++;
	}
	return (n);
}

/* Overlay timer callback. */
static void
server_client_overlay_timer(__unused int fd, __unused short events, void *data)
{
	server_client_clear_overlay(data);
}

/* Set an overlay on client. */
void
server_client_set_overlay(struct client *c, u_int delay, overlay_draw_cb drawcb,
    overlay_key_cb keycb, overlay_free_cb freecb, void *data)
{
	struct timeval	tv;

	if (c->overlay_draw != NULL)
		server_client_clear_overlay(c);

	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	if (event_initialized(&c->overlay_timer))
		evtimer_del(&c->overlay_timer);
	evtimer_set(&c->overlay_timer, server_client_overlay_timer, c);
	if (delay != 0)
		evtimer_add(&c->overlay_timer, &tv);

	c->overlay_draw = drawcb;
	c->overlay_key = keycb;
	c->overlay_free = freecb;
	c->overlay_data = data;

	c->tty.flags |= (TTY_FREEZE|TTY_NOCURSOR);
	server_redraw_client(c);
}

/* Clear overlay mode on client. */
static void
server_client_clear_overlay(struct client *c)
{
	if (c->overlay_draw == NULL)
		return;

	if (event_initialized(&c->overlay_timer))
		evtimer_del(&c->overlay_timer);

	if (c->overlay_free != NULL)
		c->overlay_free(c);

	c->overlay_draw = NULL;
	c->overlay_key = NULL;

	c->tty.flags &= ~(TTY_FREEZE|TTY_NOCURSOR);
	server_redraw_client(c);
}

/* Check if this client is inside this server. */
int
server_client_check_nested(struct client *c)
{
	struct environ_entry	*envent;
	struct window_pane	*wp;

	envent = environ_find(c->environ, "TMUX");
	if (envent == NULL || *envent->value == '\0')
		return (0);

	RB_FOREACH(wp, window_pane_tree, &all_window_panes) {
		if (strcmp(wp->tty, c->ttyname) == 0)
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

/* Is this table the default key table? */
static int
server_client_is_default_key_table(struct client *c, struct key_table *table)
{
	return (strcmp(table->name, server_client_get_key_table(c)) == 0);
}

/* Create a new client. */
struct client *
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

	TAILQ_INIT(&c->queue);

	c->stdin_data = evbuffer_new();
	if (c->stdin_data == NULL)
		fatalx("out of memory");
	c->stdout_data = evbuffer_new();
	if (c->stdout_data == NULL)
		fatalx("out of memory");
	c->stderr_data = evbuffer_new();
	if (c->stderr_data == NULL)
		fatalx("out of memory");

	c->tty.fd = -1;
	c->title = NULL;

	c->session = NULL;
	c->last_session = NULL;

	c->tty.sx = 80;
	c->tty.sy = 24;

	status_init(c);

	c->message_string = NULL;
	TAILQ_INIT(&c->message_log);

	c->prompt_string = NULL;
	c->prompt_buffer = NULL;
	c->prompt_index = 0;

	c->flags |= CLIENT_FOCUSED;

	c->keytable = key_bindings_get_table("root", 1);
	c->keytable->references++;

	evtimer_set(&c->repeat_timer, server_client_repeat_timer, c);
	evtimer_set(&c->click_timer, server_client_click_timer, c);

	TAILQ_INSERT_TAIL(&clients, c, entry);
	log_debug("new client %p", c);
	return (c);
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

	server_client_clear_overlay(c);
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

	status_free(c);

	free(c->title);
	free((void *)c->cwd);

	evtimer_del(&c->repeat_timer);
	evtimer_del(&c->click_timer);

	key_bindings_unref_table(c->keytable);

	free(c->message_string);
	if (event_initialized(&c->message_timer))
		evtimer_del(&c->message_timer);
	TAILQ_FOREACH_SAFE(msg, &c->message_log, entry, msg1) {
		free(msg->msg);
		TAILQ_REMOVE(&c->message_log, msg, entry);
		free(msg);
	}

	free(c->prompt_saved);
	free(c->prompt_string);
	free(c->prompt_buffer);

	format_lost_client(c);
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
static void
server_client_free(__unused int fd, __unused short events, void *arg)
{
	struct client	*c = arg;

	log_debug("free client %p (%d references)", c, c->references);

	if (!TAILQ_EMPTY(&c->queue))
		fatalx("queue not empty");

	if (c->references == 0) {
		free((void *)c->name);
		free(c);
	}
}

/* Suspend a client. */
void
server_client_suspend(struct client *c)
{
	struct session	*s = c->session;

	if (s == NULL || (c->flags & CLIENT_DETACHING))
		return;

	tty_stop_tty(&c->tty);
	c->flags |= CLIENT_SUSPENDED;
	proc_send(c->peer, MSG_SUSPEND, -1, NULL, 0);
}

/* Detach a client. */
void
server_client_detach(struct client *c, enum msgtype msgtype)
{
	struct session	*s = c->session;

	if (s == NULL || (c->flags & CLIENT_DETACHING))
		return;

	c->flags |= CLIENT_DETACHING;
	notify_client("client-detached", c);
	proc_send(c->peer, msgtype, -1, s->name, strlen(s->name) + 1);
}

/* Execute command to replace a client. */
void
server_client_exec(struct client *c, const char *cmd)
{
	struct session	*s = c->session;
	char		*msg;
	const char	*shell;
	size_t		 cmdsize, shellsize;

	if (*cmd == '\0')
		return;
	cmdsize = strlen(cmd) + 1;

	if (s != NULL)
		shell = options_get_string(s->options, "default-shell");
	else
		shell = options_get_string(global_s_options, "default-shell");
	shellsize = strlen(shell) + 1;

	msg = xmalloc(cmdsize + shellsize);
	memcpy(msg, cmd, cmdsize);
	memcpy(msg + cmdsize, shell, shellsize);

	proc_send(c->peer, MSG_EXEC, -1, msg, cmdsize + shellsize);
	free(msg);
}

/* Check for mouse keys. */
static key_code
server_client_check_mouse(struct client *c, struct key_event *event)
{
	struct mouse_event	*m = &event->m;
	struct session		*s = c->session;
	struct winlink		*wl;
	struct window_pane	*wp;
	u_int			 x, y, b, sx, sy, px, py;
	int			 flag;
	key_code		 key;
	struct timeval		 tv;
	struct style_range	*sr;
	enum { NOTYPE,
	       MOVE,
	       DOWN,
	       UP,
	       DRAG,
	       WHEEL,
	       DOUBLE,
	       TRIPLE } type = NOTYPE;
	enum { NOWHERE,
	       PANE,
	       STATUS,
	       STATUS_LEFT,
	       STATUS_RIGHT,
	       STATUS_DEFAULT,
	       BORDER } where = NOWHERE;

	log_debug("%s mouse %02x at %u,%u (last %u,%u) (%d)", c->name, m->b,
	    m->x, m->y, m->lx, m->ly, c->tty.mouse_drag_flag);

	/* What type of event is this? */
	if ((m->sgr_type != ' ' &&
	    MOUSE_DRAG(m->sgr_b) &&
	    MOUSE_BUTTONS(m->sgr_b) == 3) ||
	    (m->sgr_type == ' ' &&
	    MOUSE_DRAG(m->b) &&
	    MOUSE_BUTTONS(m->b) == 3 &&
	    MOUSE_BUTTONS(m->lb) == 3)) {
		type = MOVE;
		x = m->x, y = m->y, b = 0;
		log_debug("move at %u,%u", x, y);
	} else if (MOUSE_DRAG(m->b)) {
		type = DRAG;
		if (c->tty.mouse_drag_flag) {
			x = m->x, y = m->y, b = m->b;
			if (x == m->lx && y == m->ly)
				return (KEYC_UNKNOWN);
			log_debug("drag update at %u,%u", x, y);
		} else {
			x = m->lx, y = m->ly, b = m->lb;
			log_debug("drag start at %u,%u", x, y);
		}
	} else if (MOUSE_WHEEL(m->b)) {
		type = WHEEL;
		x = m->x, y = m->y, b = m->b;
		log_debug("wheel at %u,%u", x, y);
	} else if (MOUSE_RELEASE(m->b)) {
		type = UP;
		x = m->x, y = m->y, b = m->lb;
		log_debug("up at %u,%u", x, y);
	} else {
		if (c->flags & CLIENT_DOUBLECLICK) {
			evtimer_del(&c->click_timer);
			c->flags &= ~CLIENT_DOUBLECLICK;
			if (m->b == c->click_button) {
				type = DOUBLE;
				x = m->x, y = m->y, b = m->b;
				log_debug("double-click at %u,%u", x, y);
				flag = CLIENT_TRIPLECLICK;
				goto add_timer;
			}
		} else if (c->flags & CLIENT_TRIPLECLICK) {
			evtimer_del(&c->click_timer);
			c->flags &= ~CLIENT_TRIPLECLICK;
			if (m->b == c->click_button) {
				type = TRIPLE;
				x = m->x, y = m->y, b = m->b;
				log_debug("triple-click at %u,%u", x, y);
				goto have_event;
			}
		}

		type = DOWN;
		x = m->x, y = m->y, b = m->b;
		log_debug("down at %u,%u", x, y);
		flag = CLIENT_DOUBLECLICK;

	add_timer:
		if (KEYC_CLICK_TIMEOUT != 0) {
			c->flags |= flag;
			c->click_button = m->b;

			tv.tv_sec = KEYC_CLICK_TIMEOUT / 1000;
			tv.tv_usec = (KEYC_CLICK_TIMEOUT % 1000) * 1000L;
			evtimer_del(&c->click_timer);
			evtimer_add(&c->click_timer, &tv);
		}
	}

have_event:
	if (type == NOTYPE)
		return (KEYC_UNKNOWN);

	/* Save the session. */
	m->s = s->id;
	m->w = -1;

	/* Is this on the status line? */
	m->statusat = status_at_line(c);
	if (m->statusat != -1 &&
	    y >= (u_int)m->statusat &&
	    y < m->statusat + status_line_size(c)) {
		sr = status_get_range(c, x, y - m->statusat);
		if (sr == NULL) {
			where = STATUS_DEFAULT;
		} else {
			switch (sr->type) {
			case STYLE_RANGE_NONE:
				return (KEYC_UNKNOWN);
			case STYLE_RANGE_LEFT:
				where = STATUS_LEFT;
				break;
			case STYLE_RANGE_RIGHT:
				where = STATUS_RIGHT;
				break;
			case STYLE_RANGE_WINDOW:
				wl = winlink_find_by_index(&s->windows, sr->argument);
				if (wl == NULL)
					return (KEYC_UNKNOWN);
				m->w = wl->window->id;

				where = STATUS;
				break;
			}
		}
	}

	/* Not on status line. Adjust position and check for border or pane. */
	if (where == NOWHERE) {
		px = x;
		if (m->statusat == 0 && y > 0)
			py = y - 1;
		else if (m->statusat > 0 && y >= (u_int)m->statusat)
			py = m->statusat - 1;
		else
			py = y;

		tty_window_offset(&c->tty, &m->ox, &m->oy, &sx, &sy);
		log_debug("mouse window @%u at %u,%u (%ux%u)",
		    s->curw->window->id, m->ox, m->oy, sx, sy);
		if (px > sx || py > sy)
			return (KEYC_UNKNOWN);
		px = px + m->ox;
		py = py + m->oy;

		/* Try the pane borders if not zoomed. */
		if (~s->curw->window->flags & WINDOW_ZOOMED) {
			TAILQ_FOREACH(wp, &s->curw->window->panes, entry) {
				if ((wp->xoff + wp->sx == px &&
				    wp->yoff <= 1 + py &&
				    wp->yoff + wp->sy >= py) ||
				    (wp->yoff + wp->sy == py &&
				    wp->xoff <= 1 + px &&
				    wp->xoff + wp->sx >= px))
					break;
			}
			if (wp != NULL)
				where = BORDER;
		}

		/* Otherwise try inside the pane. */
		if (where == NOWHERE) {
			wp = window_get_active_at(s->curw->window, px, py);
			if (wp != NULL)
				where = PANE;
		}

		if (where == NOWHERE)
			return (KEYC_UNKNOWN);
		if (where == PANE)
			log_debug("mouse %u,%u on pane %%%u", x, y, wp->id);
		else if (where == BORDER)
			log_debug("mouse on pane %%%u border", wp->id);
		m->wp = wp->id;
		m->w = wp->window->id;
	} else
		m->wp = -1;

	/* Stop dragging if needed. */
	if (type != DRAG && type != WHEEL && c->tty.mouse_drag_flag) {
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
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND1_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND1_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND1_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND1_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND2_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND2_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND2_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND2_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND2_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND2_BORDER;
			break;
		case 3:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND3_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND3_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND3_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND3_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND3_STATUS_DEFAULT;
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
	case MOVE:
		if (where == PANE)
			key = KEYC_MOUSEMOVE_PANE;
		if (where == STATUS)
			key = KEYC_MOUSEMOVE_STATUS;
		if (where == STATUS_LEFT)
			key = KEYC_MOUSEMOVE_STATUS_LEFT;
		if (where == STATUS_RIGHT)
			key = KEYC_MOUSEMOVE_STATUS_RIGHT;
		if (where == STATUS_DEFAULT)
			key = KEYC_MOUSEMOVE_STATUS_DEFAULT;
		if (where == BORDER)
			key = KEYC_MOUSEMOVE_BORDER;
		break;
	case DRAG:
		if (c->tty.mouse_drag_update != NULL)
			key = KEYC_DRAGGING;
		else {
			switch (MOUSE_BUTTONS(b)) {
			case 0:
				if (where == PANE)
					key = KEYC_MOUSEDRAG1_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG1_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG1_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG1_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG1_STATUS_DEFAULT;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG1_BORDER;
				break;
			case 1:
				if (where == PANE)
					key = KEYC_MOUSEDRAG2_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG2_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG2_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG2_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG2_STATUS_DEFAULT;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG2_BORDER;
				break;
			case 2:
				if (where == PANE)
					key = KEYC_MOUSEDRAG3_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG3_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG3_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG3_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG3_STATUS_DEFAULT;
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
			if (where == STATUS_LEFT)
				key = KEYC_WHEELUP_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_WHEELUP_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_WHEELUP_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_WHEELUP_BORDER;
		} else {
			if (where == PANE)
				key = KEYC_WHEELDOWN_PANE;
			if (where == STATUS)
				key = KEYC_WHEELDOWN_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_WHEELDOWN_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_WHEELDOWN_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_WHEELDOWN_STATUS_DEFAULT;
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
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP1_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP1_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP1_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_MOUSEUP1_BORDER;
			break;
		case 1:
			if (where == PANE)
				key = KEYC_MOUSEUP2_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP2_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP2_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP2_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP2_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_MOUSEUP2_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_MOUSEUP3_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP3_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP3_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP3_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP3_STATUS_DEFAULT;
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
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN1_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN1_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN1_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN1_BORDER;
			break;
		case 1:
			if (where == PANE)
				key = KEYC_MOUSEDOWN2_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN2_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN2_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN2_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN2_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN2_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_MOUSEDOWN3_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN3_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN3_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN3_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN3_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN3_BORDER;
			break;
		}
		break;
	case DOUBLE:
		switch (MOUSE_BUTTONS(b)) {
		case 0:
			if (where == PANE)
				key = KEYC_DOUBLECLICK1_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK1_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK1_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK1_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK1_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK1_BORDER;
			break;
		case 1:
			if (where == PANE)
				key = KEYC_DOUBLECLICK2_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK2_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK2_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK2_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK2_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK2_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_DOUBLECLICK3_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK3_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK3_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK3_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK3_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK3_BORDER;
			break;
		}
		break;
	case TRIPLE:
		switch (MOUSE_BUTTONS(b)) {
		case 0:
			if (where == PANE)
				key = KEYC_TRIPLECLICK1_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK1_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK1_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK1_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK1_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK1_BORDER;
			break;
		case 1:
			if (where == PANE)
				key = KEYC_TRIPLECLICK2_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK2_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK2_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK2_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK2_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK2_BORDER;
			break;
		case 2:
			if (where == PANE)
				key = KEYC_TRIPLECLICK3_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK3_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK3_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK3_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK3_STATUS_DEFAULT;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK3_BORDER;
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
static int
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

/*
 * Handle data key input from client. This owns and can modify the key event it
 * is given and is responsible for freeing it.
 */
static enum cmd_retval
server_client_key_callback(struct cmdq_item *item, void *data)
{
	struct client			*c = item->client;
	struct key_event		*event = data;
	key_code			 key = event->key;
	struct mouse_event		*m = &event->m;
	struct session			*s = c->session;
	struct winlink			*wl;
	struct window_pane		*wp;
	struct window_mode_entry	*wme;
	struct timeval			 tv;
	struct key_table		*table, *first;
	struct key_binding		*bd;
	int				 xtimeout, flags;
	struct cmd_find_state		 fs;
	key_code			 key0;

	/* Check the client is good to accept input. */
	if (s == NULL || (c->flags & (CLIENT_DEAD|CLIENT_SUSPENDED)) != 0)
		goto out;
	wl = s->curw;

	/* Update the activity timer. */
	if (gettimeofday(&c->activity_time, NULL) != 0)
		fatal("gettimeofday failed");
	session_update_activity(s, &c->activity_time);

	/* Handle status line. */
	if (~c->flags & CLIENT_READONLY)
		status_message_clear(c);
	if (c->prompt_string != NULL) {
		if (c->flags & CLIENT_READONLY)
			goto out;
		if (status_prompt_key(c, key) == 0)
			goto out;
	}

	/* Check for mouse keys. */
	m->valid = 0;
	if (key == KEYC_MOUSE) {
		if (c->flags & CLIENT_READONLY)
			goto out;
		key = server_client_check_mouse(c, event);
		if (key == KEYC_UNKNOWN)
			goto out;

		m->valid = 1;
		m->key = key;

		/*
		 * Mouse drag is in progress, so fire the callback (now that
		 * the mouse event is valid).
		 */
		if (key == KEYC_DRAGGING) {
			c->tty.mouse_drag_update(c, m);
			goto out;
		}
	}

	/* Find affected pane. */
	if (!KEYC_IS_MOUSE(key) || cmd_find_from_mouse(&fs, m, 0) != 0)
		cmd_find_from_session(&fs, s, 0);
	wp = fs.wp;

	/* Forward mouse keys if disabled. */
	if (KEYC_IS_MOUSE(key) && !options_get_number(s->options, "mouse"))
		goto forward_key;

	/* Treat everything as a regular key when pasting is detected. */
	if (!KEYC_IS_MOUSE(key) && server_client_assume_paste(s))
		goto forward_key;

	/*
	 * Work out the current key table. If the pane is in a mode, use
	 * the mode table instead of the default key table.
	 */
	if (server_client_is_default_key_table(c, c->keytable) &&
	    wp != NULL &&
	    (wme = TAILQ_FIRST(&wp->modes)) != NULL &&
	    wme->mode->key_table != NULL)
		table = key_bindings_get_table(wme->mode->key_table(wme), 1);
	else
		table = c->keytable;
	first = table;

table_changed:
	/*
	 * The prefix always takes precedence and forces a switch to the prefix
	 * table, unless we are already there.
	 */
	key0 = (key & ~KEYC_XTERM);
	if ((key0 == (key_code)options_get_number(s->options, "prefix") ||
	    key0 == (key_code)options_get_number(s->options, "prefix2")) &&
	    strcmp(table->name, "prefix") != 0) {
		server_client_set_key_table(c, "prefix");
		server_status_client(c);
		goto out;
	}
	flags = c->flags;

try_again:
	/* Log key table. */
	if (wp == NULL)
		log_debug("key table %s (no pane)", table->name);
	else
		log_debug("key table %s (pane %%%u)", table->name, wp->id);
	if (c->flags & CLIENT_REPEAT)
		log_debug("currently repeating");

	/* Try to see if there is a key binding in the current table. */
	bd = key_bindings_get(table, key0);
	if (bd != NULL) {
		/*
		 * Key was matched in this table. If currently repeating but a
		 * non-repeating binding was found, stop repeating and try
		 * again in the root table.
		 */
		if ((c->flags & CLIENT_REPEAT) &&
		    (~bd->flags & KEY_BINDING_REPEAT)) {
			log_debug("found in key table %s (not repeating)",
			    table->name);
			server_client_set_key_table(c, NULL);
			first = table = c->keytable;
			c->flags &= ~CLIENT_REPEAT;
			server_status_client(c);
			goto table_changed;
		}
		log_debug("found in key table %s", table->name);

		/*
		 * Take a reference to this table to make sure the key binding
		 * doesn't disappear.
		 */
		table->references++;

		/*
		 * If this is a repeating key, start the timer. Otherwise reset
		 * the client back to the root table.
		 */
		xtimeout = options_get_number(s->options, "repeat-time");
		if (xtimeout != 0 && (bd->flags & KEY_BINDING_REPEAT)) {
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

		/* Execute the key binding. */
		key_bindings_dispatch(bd, item, c, m, &fs);
		key_bindings_unref_table(table);
		goto out;
	}

	/*
	 * No match, try the ANY key.
	 */
	if (key0 != KEYC_ANY) {
		key0 = KEYC_ANY;
		goto try_again;
	}

	/*
	 * No match in this table. If not in the root table or if repeating,
	 * switch the client back to the root table and try again.
	 */
	log_debug("not found in key table %s", table->name);
	if (!server_client_is_default_key_table(c, table) ||
	    (c->flags & CLIENT_REPEAT)) {
		log_debug("trying in root table");
		server_client_set_key_table(c, NULL);
		table = c->keytable;
		if (c->flags & CLIENT_REPEAT)
			first = table;
		c->flags &= ~CLIENT_REPEAT;
		server_status_client(c);
		goto table_changed;
	}

	/*
	 * No match in the root table either. If this wasn't the first table
	 * tried, don't pass the key to the pane.
	 */
	if (first != table && (~flags & CLIENT_REPEAT)) {
		server_client_set_key_table(c, NULL);
		server_status_client(c);
		goto out;
	}

forward_key:
	if (c->flags & CLIENT_READONLY)
		goto out;
	if (wp != NULL)
		window_pane_key(wp, c, s, wl, key, m);

out:
	free(event);
	return (CMD_RETURN_NORMAL);
}

/* Handle a key event. */
int
server_client_handle_key(struct client *c, struct key_event *event)
{
	struct session		*s = c->session;
	struct cmdq_item	*item;

	/* Check the client is good to accept input. */
	if (s == NULL || (c->flags & (CLIENT_DEAD|CLIENT_SUSPENDED)) != 0)
		return (0);

	/*
	 * Key presses in overlay mode are a special case. The queue might be
	 * blocked so they need to be processed immediately rather than queued.
	 */
	if ((~c->flags & CLIENT_READONLY) && c->overlay_key != NULL) {
		if (c->overlay_key(c, event) != 0)
			server_client_clear_overlay(c);
		return (0);
	}

	/*
	 * Add the key to the queue so it happens after any commands queued by
	 * previous keys.
	 */
	item = cmdq_get_callback(server_client_key_callback, event);
	cmdq_append(c, item);
	return (1);
}

/* Client functions that need to happen every loop. */
void
server_client_loop(void)
{
	struct client		*c;
	struct window		*w;
	struct window_pane	*wp;
	struct winlink		*wl;
	struct session		*s;
	int			 focus;

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
	focus = options_get_number(global_options, "focus-events");
	RB_FOREACH(w, windows, &windows) {
		TAILQ_FOREACH(wl, &w->winlinks, wentry) {
			s = wl->session;
			if (s->attached != 0 && s->curw == wl)
				break;
		}
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wl != NULL && wp->fd != -1) {
				if (focus)
					server_client_check_focus(wp);
				server_client_check_resize(wp);
			}
			wp->flags &= ~PANE_REDRAW;
		}
		check_window_name(w);
	}
}

/* Check if we need to force a resize. */
static int
server_client_resize_force(struct window_pane *wp)
{
	struct timeval	tv = { .tv_usec = 100000 };
	struct winsize	ws;

	/*
	 * If we are resizing to the same size as when we entered the loop
	 * (that is, to the same size the application currently thinks it is),
	 * tmux may have gone through several resizes internally and thrown
	 * away parts of the screen. So we need the application to actually
	 * redraw even though its final size has not changed.
	 */

	if (wp->flags & PANE_RESIZEFORCE) {
		wp->flags &= ~PANE_RESIZEFORCE;
		return (0);
	}

	if (wp->sx != wp->osx ||
	    wp->sy != wp->osy ||
	    wp->sx <= 1 ||
	    wp->sy <= 1)
		return (0);

	memset(&ws, 0, sizeof ws);
	ws.ws_col = wp->sx;
	ws.ws_row = wp->sy - 1;
	if (wp->fd != -1 && ioctl(wp->fd, TIOCSWINSZ, &ws) == -1)
		fatal("ioctl failed");
	log_debug("%s: %%%u forcing resize", __func__, wp->id);

	evtimer_add(&wp->resize_timer, &tv);
	wp->flags |= PANE_RESIZEFORCE;
	return (1);
}

/* Resize timer event. */
static void
server_client_resize_event(__unused int fd, __unused short events, void *data)
{
	struct window_pane	*wp = data;
	struct winsize		 ws;

	evtimer_del(&wp->resize_timer);

	if (!(wp->flags & PANE_RESIZE))
		return;
	if (server_client_resize_force(wp))
		return;

	memset(&ws, 0, sizeof ws);
	ws.ws_col = wp->sx;
	ws.ws_row = wp->sy;
	if (wp->fd != -1 && ioctl(wp->fd, TIOCSWINSZ, &ws) == -1)
		fatal("ioctl failed");
	log_debug("%s: %%%u resize to %u,%u", __func__, wp->id, wp->sx, wp->sy);

	wp->flags &= ~PANE_RESIZE;

	wp->osx = wp->sx;
	wp->osy = wp->sy;
}

/* Check if pane should be resized. */
static void
server_client_check_resize(struct window_pane *wp)
{
	struct timeval	 tv = { .tv_usec = 250000 };

	if (!(wp->flags & PANE_RESIZE))
		return;
	log_debug("%s: %%%u resize to %u,%u", __func__, wp->id, wp->sx, wp->sy);

	if (!event_initialized(&wp->resize_timer))
		evtimer_set(&wp->resize_timer, server_client_resize_event, wp);

	/*
	 * The first resize should happen immediately, so if the timer is not
	 * running, do it now.
	 */
	if (!evtimer_pending(&wp->resize_timer, NULL))
		server_client_resize_event(-1, 0, wp);

	/*
	 * If the pane is in the alternate screen, let the timer expire and
	 * resize to give the application a chance to redraw. If not, keep
	 * pushing the timer back.
	 */
	if (wp->saved_grid != NULL && evtimer_pending(&wp->resize_timer, NULL))
		return;
	evtimer_del(&wp->resize_timer);
	evtimer_add(&wp->resize_timer, &tv);
}

/* Check whether pane should be focused. */
static void
server_client_check_focus(struct window_pane *wp)
{
	struct client	*c;
	int		 push;

	/* Do we need to push the focus state? */
	push = wp->flags & PANE_FOCUSPUSH;
	wp->flags &= ~PANE_FOCUSPUSH;

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
		if (c->session->attached == 0)
			continue;

		if (c->session->curw->window == wp->window)
			goto focused;
	}

not_focused:
	if (push || (wp->flags & PANE_FOCUSED)) {
		if (wp->base.mode & MODE_FOCUSON)
			bufferevent_write(wp->event, "\033[O", 3);
		notify_pane("pane-focus-out", wp);
	}
	wp->flags &= ~PANE_FOCUSED;
	return;

focused:
	if (push || !(wp->flags & PANE_FOCUSED)) {
		if (wp->base.mode & MODE_FOCUSON)
			bufferevent_write(wp->event, "\033[I", 3);
		notify_pane("pane-focus-in", wp);
		session_update_activity(c->session, NULL);
	}
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
static void
server_client_reset_state(struct client *c)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp = w->active, *loop;
	struct screen		*s = wp->screen;
	struct options		*oo = c->session->options;
	int			 mode, cursor = 0;
	u_int			 cx = 0, cy = 0, ox, oy, sx, sy;

	if (c->flags & (CLIENT_CONTROL|CLIENT_SUSPENDED))
		return;
	if (c->overlay_draw != NULL)
		return;
	mode = s->mode;

	tty_region_off(&c->tty);
	tty_margin_off(&c->tty);

	/* Move cursor to pane cursor and offset. */
	cursor = 0;
	tty_window_offset(&c->tty, &ox, &oy, &sx, &sy);
	if (wp->xoff + s->cx >= ox && wp->xoff + s->cx <= ox + sx &&
	    wp->yoff + s->cy >= oy && wp->yoff + s->cy <= oy + sy) {
		cursor = 1;

		cx = wp->xoff + s->cx - ox;
		cy = wp->yoff + s->cy - oy;

		if (status_at_line(c) == 0)
			cy += status_line_size(c);
	}
	if (!cursor)
		mode &= ~MODE_CURSOR;
	tty_cursor(&c->tty, cx, cy);

	/*
	 * Set mouse mode if requested. To support dragging, always use button
	 * mode.
	 */
	if (options_get_number(oo, "mouse")) {
		mode &= ~ALL_MOUSE_MODES;
		TAILQ_FOREACH(loop, &w->panes, entry) {
			if (loop->screen->mode & MODE_MOUSE_ALL)
				mode |= MODE_MOUSE_ALL;
		}
		if (~mode & MODE_MOUSE_ALL)
			mode |= MODE_MOUSE_BUTTON;
	}

	/* Clear bracketed paste mode if at the prompt. */
	if (c->prompt_string != NULL)
		mode &= ~MODE_BRACKETPASTE;

	/* Set the terminal mode and reset attributes. */
	tty_update_mode(&c->tty, mode, s);
	tty_reset(&c->tty);
}

/* Repeat time callback. */
static void
server_client_repeat_timer(__unused int fd, __unused short events, void *data)
{
	struct client	*c = data;

	if (c->flags & CLIENT_REPEAT) {
		server_client_set_key_table(c, NULL);
		c->flags &= ~CLIENT_REPEAT;
		server_status_client(c);
	}
}

/* Double-click callback. */
static void
server_client_click_timer(__unused int fd, __unused short events, void *data)
{
	struct client	*c = data;

	c->flags &= ~(CLIENT_DOUBLECLICK|CLIENT_TRIPLECLICK);
}

/* Check if client should be exited. */
static void
server_client_check_exit(struct client *c)
{
	if (~c->flags & CLIENT_EXIT)
		return;
	if (c->flags & CLIENT_EXITED)
		return;

	if (EVBUFFER_LENGTH(c->stdin_data) != 0)
		return;
	if (EVBUFFER_LENGTH(c->stdout_data) != 0)
		return;
	if (EVBUFFER_LENGTH(c->stderr_data) != 0)
		return;

	if (c->flags & CLIENT_ATTACHED)
		notify_client("client-detached", c);
	proc_send(c->peer, MSG_EXIT, -1, &c->retval, sizeof c->retval);
	c->flags |= CLIENT_EXITED;
}

/* Redraw timer callback. */
static void
server_client_redraw_timer(__unused int fd, __unused short events,
    __unused void* data)
{
	log_debug("redraw timer fired");
}

/* Check for client redraws. */
static void
server_client_check_redraw(struct client *c)
{
	struct session		*s = c->session;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	int			 needed, flags;
	struct timeval		 tv = { .tv_usec = 1000 };
	static struct event	 ev;
	size_t			 left;

	if (c->flags & (CLIENT_CONTROL|CLIENT_SUSPENDED))
		return;
	if (c->flags & CLIENT_ALLREDRAWFLAGS) {
		log_debug("%s: redraw%s%s%s%s", c->name,
		    (c->flags & CLIENT_REDRAWWINDOW) ? " window" : "",
		    (c->flags & CLIENT_REDRAWSTATUS) ? " status" : "",
		    (c->flags & CLIENT_REDRAWBORDERS) ? " borders" : "",
		    (c->flags & CLIENT_REDRAWOVERLAY) ? " overlay" : "");
	}

	/*
	 * If there is outstanding data, defer the redraw until it has been
	 * consumed. We can just add a timer to get out of the event loop and
	 * end up back here.
	 */
	needed = 0;
	if (c->flags & CLIENT_ALLREDRAWFLAGS)
		needed = 1;
	else {
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry) {
			if (wp->flags & PANE_REDRAW) {
				needed = 1;
				break;
			}
		}
	}
	if (needed && (left = EVBUFFER_LENGTH(tty->out)) != 0) {
		log_debug("%s: redraw deferred (%zu left)", c->name, left);
		if (!evtimer_initialized(&ev))
			evtimer_set(&ev, server_client_redraw_timer, NULL);
		if (!evtimer_pending(&ev, NULL)) {
			log_debug("redraw timer started");
			evtimer_add(&ev, &tv);
		}

		/*
		 * We may have got here for a single pane redraw, but force a
		 * full redraw next time in case other panes have been updated.
		 */
		c->flags |= CLIENT_ALLREDRAWFLAGS;
		return;
	} else if (needed)
		log_debug("%s: redraw needed", c->name);

	flags = tty->flags & (TTY_BLOCK|TTY_FREEZE|TTY_NOCURSOR);
	tty->flags = (tty->flags & ~(TTY_BLOCK|TTY_FREEZE)) | TTY_NOCURSOR;

	if (~c->flags & CLIENT_REDRAWWINDOW) {
		/*
		 * If not redrawing the entire window, check whether each pane
		 * needs to be redrawn.
		 */
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry) {
			if (wp->flags & PANE_REDRAW) {
				tty_update_mode(tty, tty->mode, NULL);
				screen_redraw_pane(c, wp);
			}
		}
	}

	if (c->flags & CLIENT_ALLREDRAWFLAGS) {
		if (options_get_number(s->options, "set-titles"))
			server_client_set_title(c);
		screen_redraw_screen(c);
	}

	tty->flags = (tty->flags & ~(TTY_FREEZE|TTY_NOCURSOR)) | flags;
	tty_update_mode(tty, tty->mode, NULL);

	c->flags &= ~(CLIENT_ALLREDRAWFLAGS|CLIENT_STATUSFORCE);

	if (needed) {
		/*
		 * We would have deferred the redraw unless the output buffer
		 * was empty, so we can record how many bytes the redraw
		 * generated.
		 */
		c->redraw = EVBUFFER_LENGTH(tty->out);
		log_debug("%s: redraw added %zu bytes", c->name, c->redraw);
	}
}

/* Set client title. */
static void
server_client_set_title(struct client *c)
{
	struct session		*s = c->session;
	const char		*template;
	char			*title;
	struct format_tree	*ft;

	template = options_get_string(s->options, "set-titles-string");

	ft = format_create(c, NULL, FORMAT_NONE, 0);
	format_defaults(ft, c, NULL, NULL, NULL);

	title = format_expand_time(ft, template);
	if (c->title == NULL || strcmp(title, c->title) != 0) {
		free(c->title);
		c->title = xstrdup(title);
		tty_set_title(&c->tty, c->title);
	}
	free(title);

	format_free(ft);
}

/* Dispatch message from client. */
static void
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
		c->stdin_callback(c, c->stdin_closed, c->stdin_callback_data);
		break;
	case MSG_RESIZE:
		if (datalen != 0)
			fatalx("bad MSG_RESIZE size");

		if (c->flags & CLIENT_CONTROL)
			break;
		server_client_clear_overlay(c);
		tty_resize(&c->tty);
		recalculate_sizes();
		server_redraw_client(c);
		if (c->session != NULL)
			notify_client("client-resized", c);
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

		tty_start_tty(&c->tty);
		server_redraw_client(c);
		recalculate_sizes();

		if (s != NULL)
			session_update_activity(s, &c->activity_time);
		break;
	case MSG_SHELL:
		if (datalen != 0)
			fatalx("bad MSG_SHELL size");

		server_client_dispatch_shell(c);
		break;
	}
}

/* Callback when command is done. */
static enum cmd_retval
server_client_command_done(struct cmdq_item *item, __unused void *data)
{
	struct client	*c = item->client;

	if (~c->flags & CLIENT_ATTACHED)
		c->flags |= CLIENT_EXIT;
	return (CMD_RETURN_NORMAL);
}

/* Handle command message. */
static void
server_client_dispatch_command(struct client *c, struct imsg *imsg)
{
	struct msg_command_data	  data;
	char			 *buf;
	size_t			  len;
	int			  argc;
	char			**argv, *cause;
	struct cmd_parse_result	 *pr;

	if (c->flags & CLIENT_EXIT)
		return;

	if (imsg->hdr.len - IMSG_HEADER_SIZE < sizeof data)
		fatalx("bad MSG_COMMAND size");
	memcpy(&data, imsg->data, sizeof data);

	buf = (char *)imsg->data + sizeof data;
	len = imsg->hdr.len  - IMSG_HEADER_SIZE - sizeof data;
	if (len > 0 && buf[len - 1] != '\0')
		fatalx("bad MSG_COMMAND string");

	argc = data.argc;
	if (cmd_unpack_argv(buf, len, argc, &argv) != 0) {
		cause = xstrdup("command too long");
		goto error;
	}

	if (argc == 0) {
		argc = 1;
		argv = xcalloc(1, sizeof *argv);
		*argv = xstrdup("new-session");
	}

	pr = cmd_parse_from_arguments(argc, argv, NULL);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		cause = xstrdup("empty command");
		goto error;
	case CMD_PARSE_ERROR:
		cause = pr->error;
		goto error;
	case CMD_PARSE_SUCCESS:
		break;
	}
	cmd_free_argv(argc, argv);

	cmdq_append(c, cmdq_get_command(pr->cmdlist, NULL, NULL, 0));
	cmdq_append(c, cmdq_get_callback(server_client_command_done, NULL));

	cmd_list_free(pr->cmdlist);
	return;

error:
	cmd_free_argv(argc, argv);

	cmdq_append(c, cmdq_get_error(cause));
	free(cause);

	c->flags |= CLIENT_EXIT;
}

/* Handle identify message. */
static void
server_client_dispatch_identify(struct client *c, struct imsg *imsg)
{
	const char	*data, *home;
	size_t		 datalen;
	int		 flags;
	char		*name;

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

	if (*c->ttyname != '\0')
		name = xstrdup(c->ttyname);
	else
		xasprintf(&name, "client-%ld", (long)c->pid);
	c->name = name;
	log_debug("client %p name is %s", c, c->name);

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
static void
server_client_dispatch_shell(struct client *c)
{
	const char	*shell;

	shell = options_get_string(global_s_options, "default-shell");
	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;
	proc_send(c->peer, MSG_SHELL, -1, shell, strlen(shell) + 1);

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
	size_t		       sent, left;

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
	size_t		       sent, left;

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

/* Add to client message log. */
void
server_client_add_message(struct client *c, const char *fmt, ...)
{
	struct message_entry	*msg, *msg1;
	char			*s;
	va_list			 ap;
	u_int			 limit;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);

	log_debug("message %s (client %p)", s, c);

	msg = xcalloc(1, sizeof *msg);
	msg->msg_time = time(NULL);
	msg->msg_num = c->message_next++;
	msg->msg = s;
	TAILQ_INSERT_TAIL(&c->message_log, msg, entry);

	limit = options_get_number(global_options, "message-limit");
	TAILQ_FOREACH_SAFE(msg, &c->message_log, entry, msg1) {
		if (msg->msg_num + limit >= c->message_next)
			break;
		free(msg->msg);
		TAILQ_REMOVE(&c->message_log, msg, entry);
		free(msg);
	}
}

/* Get client working directory. */
const char *
server_client_get_cwd(struct client *c, struct session *s)
{
	const char	*home;

	if (!cfg_finished && cfg_client != NULL)
		return (cfg_client->cwd);
	if (c != NULL && c->session == NULL && c->cwd != NULL)
		return (c->cwd);
	if (s != NULL && s->cwd != NULL)
		return (s->cwd);
	if (c != NULL && (s = c->session) != NULL && s->cwd != NULL)
		return (s->cwd);
	if ((home = find_home()) != NULL)
		return (home);
	return ("/");
}

/* Resolve an absolute path or relative to client working directory. */
char *
server_client_get_path(struct client *c, const char *file)
{
	char	*path, resolved[PATH_MAX];

	if (*file == '/')
		path = xstrdup(file);
	else
		xasprintf(&path, "%s/%s", server_client_get_cwd(c, NULL), file);
	if (realpath(path, resolved) == NULL)
		return (path);
	free(path);
	return (xstrdup(resolved));
}
