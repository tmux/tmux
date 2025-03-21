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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

enum mouse_where {
	NOWHERE,
	PANE,
	STATUS,
	STATUS_LEFT,
	STATUS_RIGHT,
	STATUS_DEFAULT,
	BORDER,
	SCROLLBAR_UP,
	SCROLLBAR_SLIDER,
	SCROLLBAR_DOWN
};

static void	server_client_free(int, short, void *);
static void	server_client_check_pane_resize(struct window_pane *);
static void	server_client_check_pane_buffer(struct window_pane *);
static void	server_client_check_window_resize(struct window *);
static key_code	server_client_check_mouse(struct client *, struct key_event *);
static void	server_client_repeat_timer(int, short, void *);
static void	server_client_click_timer(int, short, void *);
static void	server_client_check_exit(struct client *);
static void	server_client_check_redraw(struct client *);
static void	server_client_check_modes(struct client *);
static void	server_client_set_title(struct client *);
static void	server_client_set_path(struct client *);
static void	server_client_reset_state(struct client *);
static void	server_client_update_latest(struct client *);
static void	server_client_dispatch(struct imsg *, void *);
static void	server_client_dispatch_command(struct client *, struct imsg *);
static void	server_client_dispatch_identify(struct client *, struct imsg *);
static void	server_client_dispatch_shell(struct client *);
static void	server_client_report_theme(struct client *, enum client_theme);

/* Compare client windows. */
static int
server_client_window_cmp(struct client_window *cw1,
    struct client_window *cw2)
{
	if (cw1->window < cw2->window)
		return (-1);
	if (cw1->window > cw2->window)
		return (1);
	return (0);
}
RB_GENERATE(client_windows, client_window, entry, server_client_window_cmp);

/* Number of attached clients. */
u_int
server_client_how_many(void)
{
	struct client	*c;
	u_int		 n;

	n = 0;
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL && (~c->flags & CLIENT_UNATTACHEDFLAGS))
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
server_client_set_overlay(struct client *c, u_int delay,
    overlay_check_cb checkcb, overlay_mode_cb modecb,
    overlay_draw_cb drawcb, overlay_key_cb keycb, overlay_free_cb freecb,
    overlay_resize_cb resizecb, void *data)
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

	c->overlay_check = checkcb;
	c->overlay_mode = modecb;
	c->overlay_draw = drawcb;
	c->overlay_key = keycb;
	c->overlay_free = freecb;
	c->overlay_resize = resizecb;
	c->overlay_data = data;

	if (c->overlay_check == NULL)
		c->tty.flags |= TTY_FREEZE;
	if (c->overlay_mode == NULL)
		c->tty.flags |= TTY_NOCURSOR;
	window_update_focus(c->session->curw->window);
	server_redraw_client(c);
}

/* Clear overlay mode on client. */
void
server_client_clear_overlay(struct client *c)
{
	if (c->overlay_draw == NULL)
		return;

	if (event_initialized(&c->overlay_timer))
		evtimer_del(&c->overlay_timer);

	if (c->overlay_free != NULL)
		c->overlay_free(c, c->overlay_data);

	c->overlay_check = NULL;
	c->overlay_mode = NULL;
	c->overlay_draw = NULL;
	c->overlay_key = NULL;
	c->overlay_free = NULL;
	c->overlay_resize = NULL;
	c->overlay_data = NULL;

	c->tty.flags &= ~(TTY_FREEZE|TTY_NOCURSOR);
	if (c->session != NULL)
		window_update_focus(c->session->curw->window);
	server_redraw_client(c);
}

/*
 * Given overlay position and dimensions, return parts of the input range which
 * are visible.
 */
void
server_client_overlay_range(u_int x, u_int y, u_int sx, u_int sy, u_int px,
    u_int py, u_int nx, struct overlay_ranges *r)
{
	u_int	ox, onx;

	/* Return up to 2 ranges. */
	r->px[2] = 0;
	r->nx[2] = 0;

	/* Trivial case of no overlap in the y direction. */
	if (py < y || py > y + sy - 1) {
		r->px[0] = px;
		r->nx[0] = nx;
		r->px[1] = 0;
		r->nx[1] = 0;
		return;
	}

	/* Visible bit to the left of the popup. */
	if (px < x) {
		r->px[0] = px;
		r->nx[0] = x - px;
		if (r->nx[0] > nx)
			r->nx[0] = nx;
	} else {
		r->px[0] = 0;
		r->nx[0] = 0;
	}

	/* Visible bit to the right of the popup. */
	ox = x + sx;
	if (px > ox)
		ox = px;
	onx = px + nx;
	if (onx > ox) {
		r->px[1] = ox;
		r->nx[1] = onx - ox;
	} else {
		r->px[1] = 0;
		r->nx[1] = 0;
	}
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
	if (gettimeofday(&c->keytable->activity_time, NULL) != 0)
		fatal("gettimeofday failed");
}

static uint64_t
server_client_key_table_activity_diff(struct client *c)
{
	struct timeval	diff;

	timersub(&c->activity_time, &c->keytable->activity_time, &diff);
	return ((diff.tv_sec * 1000ULL) + (diff.tv_usec / 1000ULL));
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
	c->out_fd = -1;

	c->queue = cmdq_new();
	RB_INIT(&c->windows);
	RB_INIT(&c->files);

	c->tty.sx = 80;
	c->tty.sy = 24;
	c->theme = THEME_UNKNOWN;

	status_init(c);
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
	const char	*ttynam = _PATH_TTY;

	if (c->flags & CLIENT_CONTROL)
		return (0);

	if (strcmp(c->ttyname, ttynam) == 0||
	    ((isatty(STDIN_FILENO) &&
	    (ttynam = ttyname(STDIN_FILENO)) != NULL &&
	    strcmp(c->ttyname, ttynam) == 0) ||
	    (isatty(STDOUT_FILENO) &&
	    (ttynam = ttyname(STDOUT_FILENO)) != NULL &&
	    strcmp(c->ttyname, ttynam) == 0) ||
	    (isatty(STDERR_FILENO) &&
	    (ttynam = ttyname(STDERR_FILENO)) != NULL &&
	    strcmp(c->ttyname, ttynam) == 0))) {
		xasprintf(cause, "can't use %s", c->ttyname);
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

/* Lost an attached client. */
static void
server_client_attached_lost(struct client *c)
{
	struct session	*s;
	struct window	*w;
	struct client	*loop;
	struct client	*found;

	log_debug("lost attached client %p", c);

	/*
	 * By this point the session in the client has been cleared so walk all
	 * windows to find any with this client as the latest.
	 */
	RB_FOREACH(w, windows, &windows) {
		if (w->latest != c)
			continue;

		found = NULL;
		TAILQ_FOREACH(loop, &clients, entry) {
			s = loop->session;
			if (loop == c || s == NULL || s->curw->window != w)
				continue;
			if (found == NULL || timercmp(&loop->activity_time,
			    &found->activity_time, >))
				found = loop;
		}
		if (found != NULL)
			server_client_update_latest(found);
	}
}

/* Set client session. */
void
server_client_set_session(struct client *c, struct session *s)
{
	struct session	*old = c->session;

	if (s != NULL && c->session != NULL && c->session != s)
		c->last_session = c->session;
	else if (s == NULL)
		c->last_session = NULL;
	c->session = s;
	c->flags |= CLIENT_FOCUSED;

	if (old != NULL && old->curw != NULL)
		window_update_focus(old->curw->window);
	if (s != NULL) {
		recalculate_sizes();
		window_update_focus(s->curw->window);
		session_update_activity(s, NULL);
		session_theme_changed(s);
		gettimeofday(&s->last_attached_time, NULL);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;
		s->curw->window->latest = c;
		alerts_check_session(s);
		tty_update_client_offset(c);
		status_timer_start(c);
		notify_client("client-session-changed", c);
		server_redraw_client(c);
	}

	server_check_unattached();
	server_update_socket();
}

/* Lost a client. */
void
server_client_lost(struct client *c)
{
	struct client_file	*cf, *cf1;
	struct client_window	*cw, *cw1;

	c->flags |= CLIENT_DEAD;

	server_client_clear_overlay(c);
	status_prompt_clear(c);
	status_message_clear(c);

	RB_FOREACH_SAFE(cf, client_files, &c->files, cf1) {
		cf->error = EINTR;
		file_fire_done(cf);
	}
	RB_FOREACH_SAFE(cw, client_windows, &c->windows, cw1) {
		RB_REMOVE(client_windows, &c->windows, cw);
		free(cw);
	}

	TAILQ_REMOVE(&clients, c, entry);
	log_debug("lost client %p", c);

	if (c->flags & CLIENT_ATTACHED) {
		server_client_attached_lost(c);
		notify_client("client-detached", c);
	}

	if (c->flags & CLIENT_CONTROL)
		control_stop(c);
	if (c->flags & CLIENT_TERMINAL)
		tty_free(&c->tty);
	free(c->ttyname);
	free(c->clipboard_panes);

	free(c->term_name);
	free(c->term_type);
	tty_term_free_list(c->term_caps, c->term_ncaps);

	status_free(c);

	free(c->title);
	free((void *)c->cwd);

	evtimer_del(&c->repeat_timer);
	evtimer_del(&c->click_timer);

	key_bindings_unref_table(c->keytable);

	free(c->message_string);
	if (event_initialized(&c->message_timer))
		evtimer_del(&c->message_timer);

	free(c->prompt_saved);
	free(c->prompt_string);
	free(c->prompt_buffer);

	format_lost_client(c);
	environ_free(c->environ);

	proc_remove_peer(c->peer);
	c->peer = NULL;

	if (c->out_fd != -1)
		close(c->out_fd);
	if (c->fd != -1) {
		close(c->fd);
		c->fd = -1;
	}
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

	cmdq_free(c->queue);

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

	if (s == NULL || (c->flags & CLIENT_UNATTACHEDFLAGS))
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

	if (s == NULL || (c->flags & CLIENT_NODETACHFLAGS))
		return;

	c->flags |= CLIENT_EXIT;

	c->exit_type = CLIENT_EXIT_DETACH;
	c->exit_msgtype = msgtype;
	c->exit_session = xstrdup(s->name);
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
	if (!checkshell(shell))
		shell = _PATH_BSHELL;
	shellsize = strlen(shell) + 1;

	msg = xmalloc(cmdsize + shellsize);
	memcpy(msg, cmd, cmdsize);
	memcpy(msg + cmdsize, shell, shellsize);

	proc_send(c->peer, MSG_EXEC, -1, msg, cmdsize + shellsize);
	free(msg);
}

static enum mouse_where
server_client_check_mouse_in_pane(struct window_pane *wp, u_int px, u_int py,
    u_int *sl_mpos)
{
	struct window		*w = wp->window;
	struct options		*wo = w->options;
	struct window_pane	*fwp;
	int			 pane_status, sb, sb_pos, sb_w, sb_pad;
	u_int			 line, sl_top, sl_bottom;

	sb = options_get_number(wo, "pane-scrollbars");
	sb_pos = options_get_number(wo, "pane-scrollbars-position");
	pane_status = options_get_number(wo, "pane-border-status");
	sb_pos = options_get_number(wo, "pane-scrollbars-position");

	if (window_pane_show_scrollbar(wp, sb)) {
		sb_w = wp->scrollbar_style.width;
		sb_pad = wp->scrollbar_style.pad;
	} else {
		sb_w = 0;
		sb_pad = 0;
	}
	if (pane_status == PANE_STATUS_TOP)
		line = wp->yoff - 1;
	else if (pane_status == PANE_STATUS_BOTTOM)
		line = wp->yoff + wp->sy;

	/* Check if point is within the pane or scrollbar. */
	if (((pane_status != PANE_STATUS_OFF && py != line) ||
	    (wp->yoff == 0 && py < wp->sy) ||
	    (py >= wp->yoff && py < wp->yoff + wp->sy)) &&
	    ((sb_pos == PANE_SCROLLBARS_RIGHT &&
	    px < wp->xoff + wp->sx + sb_pad + sb_w) ||
	    (sb_pos == PANE_SCROLLBARS_LEFT &&
	    px < wp->xoff + wp->sx - sb_pad - sb_w))) {
		/* Check if in the scrollbar. */
		if ((sb_pos == PANE_SCROLLBARS_RIGHT &&
		    (px >= wp->xoff + wp->sx + sb_pad &&
		    px < wp->xoff + wp->sx + sb_pad + sb_w)) ||
		    (sb_pos == PANE_SCROLLBARS_LEFT &&
		    (px >= wp->xoff - sb_pad - sb_w &&
		    px < wp->xoff - sb_pad))) {
			/* Check where inside the scrollbar. */
			sl_top = wp->yoff + wp->sb_slider_y;
			sl_bottom = (wp->yoff + wp->sb_slider_y +
			    wp->sb_slider_h - 1);
			if (py < sl_top)
				return (SCROLLBAR_UP);
			else if (py >= sl_top &&
				 py <= sl_bottom) {
				*sl_mpos = (py - wp->sb_slider_y - wp->yoff);
				return (SCROLLBAR_SLIDER);
			} else /* py > sl_bottom */
				return (SCROLLBAR_DOWN);
		} else {
			/* Must be inside the pane. */
			return (PANE);
		}
	} else if (~w->flags & WINDOW_ZOOMED) {
		/* Try the pane borders if not zoomed. */
		TAILQ_FOREACH(fwp, &w->panes, entry) {
			if ((((sb_pos == PANE_SCROLLBARS_RIGHT &&
			    fwp->xoff + fwp->sx + sb_pad + sb_w == px) ||
			    (sb_pos == PANE_SCROLLBARS_LEFT &&
			    fwp->xoff + fwp->sx == px)) &&
			    fwp->yoff <= 1 + py &&
			    fwp->yoff + fwp->sy >= py) ||
			    (fwp->yoff + fwp->sy == py &&
			    fwp->xoff <= 1 + px &&
			    fwp->xoff + fwp->sx >= px))
				break;
		}
		if (fwp != NULL) {
			wp = fwp;
			return (BORDER);
		}
	}
	return (NOWHERE);
}

/* Check for mouse keys. */
static key_code
server_client_check_mouse(struct client *c, struct key_event *event)
{
	struct mouse_event	*m = &event->m;
	struct session		*s = c->session, *fs;
	struct window		*w = s->curw->window;
	struct winlink		*fwl;
	struct window_pane	*wp, *fwp;
	u_int			 x, y, b, sx, sy, px, py, sl_mpos = 0;
	int			 ignore = 0;
	key_code		 key;
	struct timeval		 tv;
	struct style_range	*sr;
	enum { NOTYPE,
	       MOVE,
	       DOWN,
	       UP,
	       DRAG,
	       WHEEL,
	       SECOND,
	       DOUBLE,
	       TRIPLE } type = NOTYPE;
	enum mouse_where where = NOWHERE;

	log_debug("%s mouse %02x at %u,%u (last %u,%u) (%d)", c->name, m->b,
	    m->x, m->y, m->lx, m->ly, c->tty.mouse_drag_flag);

	/* What type of event is this? */
	if (event->key == KEYC_DOUBLECLICK) {
		type = DOUBLE;
		x = m->x, y = m->y, b = m->b;
		ignore = 1;
		log_debug("double-click at %u,%u", x, y);
	} else if ((m->sgr_type != ' ' &&
	    MOUSE_DRAG(m->sgr_b) &&
	    MOUSE_RELEASE(m->sgr_b)) ||
	    (m->sgr_type == ' ' &&
	    MOUSE_DRAG(m->b) &&
	    MOUSE_RELEASE(m->b) &&
	    MOUSE_RELEASE(m->lb))) {
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
		if (m->sgr_type == 'm')
			b = m->sgr_b;
		log_debug("up at %u,%u", x, y);
	} else {
		if (c->flags & CLIENT_DOUBLECLICK) {
			evtimer_del(&c->click_timer);
			c->flags &= ~CLIENT_DOUBLECLICK;
			if (m->b == c->click_button) {
				type = SECOND;
				x = m->x, y = m->y, b = m->b;
				log_debug("second-click at %u,%u", x, y);
				c->flags |= CLIENT_TRIPLECLICK;
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

		/* DOWN is the only remaining event type. */
		if (type == NOTYPE) {
			type = DOWN;
			x = m->x, y = m->y, b = m->b;
			log_debug("down at %u,%u", x, y);
			c->flags |= CLIENT_DOUBLECLICK;
		}

		if (KEYC_CLICK_TIMEOUT != 0) {
			memcpy(&c->click_event, m, sizeof c->click_event);
			c->click_button = m->b;

			log_debug("click timer started");
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
	m->wp = -1;
	m->ignore = ignore;

	/* Is this on the status line? */
	m->statusat = status_at_line(c);
	m->statuslines = status_line_size(c);
	if (m->statusat != -1 &&
	    y >= (u_int)m->statusat &&
	    y < m->statusat + m->statuslines) {
		sr = status_get_range(c, x, y - m->statusat);
		if (sr == NULL) {
			where = STATUS_DEFAULT;
		} else {
			switch (sr->type) {
			case STYLE_RANGE_NONE:
				return (KEYC_UNKNOWN);
			case STYLE_RANGE_LEFT:
				log_debug("mouse range: left");
				where = STATUS_LEFT;
				break;
			case STYLE_RANGE_RIGHT:
				log_debug("mouse range: right");
				where = STATUS_RIGHT;
				break;
			case STYLE_RANGE_PANE:
				fwp = window_pane_find_by_id(sr->argument);
				if (fwp == NULL)
					return (KEYC_UNKNOWN);
				m->wp = sr->argument;

				log_debug("mouse range: pane %%%u", m->wp);
				where = STATUS;
				break;
			case STYLE_RANGE_WINDOW:
				fwl = winlink_find_by_index(&s->windows,
				    sr->argument);
				if (fwl == NULL)
					return (KEYC_UNKNOWN);
				m->w = fwl->window->id;

				log_debug("mouse range: window @%u", m->w);
				where = STATUS;
				break;
			case STYLE_RANGE_SESSION:
				fs = session_find_by_id(sr->argument);
				if (fs == NULL)
					return (KEYC_UNKNOWN);
				m->s = sr->argument;

				log_debug("mouse range: session $%u", m->s);
				where = STATUS;
				break;
			case STYLE_RANGE_USER:
				where = STATUS;
				break;
			}
		}
	}

	/*
	 * Not on status line. Adjust position and check for border, pane, or
	 * scrollbar.
	 */
	if (where == NOWHERE) {
		if (c->tty.mouse_scrolling_flag)
			where = SCROLLBAR_SLIDER;
		else {
			px = x;
			if (m->statusat == 0 && y >= m->statuslines)
				py = y - m->statuslines;
			else if (m->statusat > 0 && y >= (u_int)m->statusat)
				py = m->statusat - 1;
			else
				py = y;

			tty_window_offset(&c->tty, &m->ox, &m->oy, &sx, &sy);
			log_debug("mouse window @%u at %u,%u (%ux%u)",
				  w->id, m->ox, m->oy, sx, sy);
			if (px > sx || py > sy)
				return (KEYC_UNKNOWN);
			px = px + m->ox;
			py = py + m->oy;

			/* Try inside the pane. */
			wp = window_get_active_at(w, px, py);
			if (wp == NULL)
				return (KEYC_UNKNOWN);
			where = server_client_check_mouse_in_pane(wp, px, py,
			    &sl_mpos);

			if (where == PANE) {
				log_debug("mouse %u,%u on pane %%%u", x, y,
				    wp->id);
			} else if (where == BORDER)
				log_debug("mouse on pane %%%u border", wp->id);
			else if (where == SCROLLBAR_UP ||
			    where == SCROLLBAR_SLIDER ||
			    where == SCROLLBAR_DOWN) {
				log_debug("mouse on pane %%%u scrollbar",
				    wp->id);
			}
			m->wp = wp->id;
			m->w = wp->window->id;
		}
	} else
		m->wp = -1;

	/* Stop dragging if needed. */
	if (type != DRAG &&
	    type != WHEEL &&
	    type != DOUBLE &&
	    type != TRIPLE &&
	    c->tty.mouse_drag_flag != 0) {
		if (c->tty.mouse_drag_release != NULL)
			c->tty.mouse_drag_release(c, m);

		c->tty.mouse_drag_update = NULL;
		c->tty.mouse_drag_release = NULL;
		c->tty.mouse_scrolling_flag = 0;

		/*
		 * End a mouse drag by passing a MouseDragEnd key corresponding
		 * to the button that started the drag.
		 */
		switch (c->tty.mouse_drag_flag - 1) {
		case MOUSE_BUTTON_1:
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
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND1_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND1_BORDER;
			break;
		case MOUSE_BUTTON_2:
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
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND2_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND2_BORDER;
			break;
		case MOUSE_BUTTON_3:
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
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND3_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND3_BORDER;
			break;
		case MOUSE_BUTTON_6:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND6_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND6_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND6_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND6_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND6_STATUS_DEFAULT;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND6_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND6_BORDER;
			break;
		case MOUSE_BUTTON_7:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND7_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND7_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND7_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND7_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND7_STATUS_DEFAULT;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND7_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND7_BORDER;
			break;
		case MOUSE_BUTTON_8:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND8_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND8_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND8_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND8_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND8_STATUS_DEFAULT;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND8_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND8_BORDER;
			break;
		case MOUSE_BUTTON_9:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND9_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND9_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND9_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND9_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND9_STATUS_DEFAULT;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND9_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND9_BORDER;
			break;
		case MOUSE_BUTTON_10:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND10_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND10_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND10_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND10_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND10_STATUS_DEFAULT;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND10_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND10_BORDER;
			break;
		case MOUSE_BUTTON_11:
			if (where == PANE)
				key = KEYC_MOUSEDRAGEND11_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDRAGEND11_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDRAGEND11_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDRAGEND11_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDRAGEND11_STATUS_DEFAULT;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDRAGEND11_SCROLLBAR_SLIDER;
			if (where == BORDER)
				key = KEYC_MOUSEDRAGEND11_BORDER;
			break;
		default:
			key = KEYC_MOUSE;
			break;
		}
		c->tty.mouse_drag_flag = 0;
		c->tty.mouse_slider_mpos = -1;
		goto out;
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
			case MOUSE_BUTTON_1:
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
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG1_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG1_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG1_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG1_BORDER;
				break;
			case MOUSE_BUTTON_2:
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
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG2_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG2_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG2_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG2_BORDER;
				break;
			case MOUSE_BUTTON_3:
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
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG3_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG3_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG3_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG3_BORDER;
				break;
			case MOUSE_BUTTON_6:
				if (where == PANE)
					key = KEYC_MOUSEDRAG6_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG6_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG6_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG6_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG6_STATUS_DEFAULT;
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG6_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG6_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG6_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG6_BORDER;
				break;
			case MOUSE_BUTTON_7:
				if (where == PANE)
					key = KEYC_MOUSEDRAG7_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG7_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG7_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG7_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG7_STATUS_DEFAULT;
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG7_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG7_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG7_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG7_BORDER;
				break;
			case MOUSE_BUTTON_8:
				if (where == PANE)
					key = KEYC_MOUSEDRAG8_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG8_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG8_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG8_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG8_STATUS_DEFAULT;
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG8_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG8_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG8_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG8_BORDER;
				break;
			case MOUSE_BUTTON_9:
				if (where == PANE)
					key = KEYC_MOUSEDRAG9_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG9_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG9_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG9_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG9_STATUS_DEFAULT;
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG9_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG9_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG9_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG9_BORDER;
				break;
			case MOUSE_BUTTON_10:
				if (where == PANE)
					key = KEYC_MOUSEDRAG10_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG10_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG10_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG10_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG10_STATUS_DEFAULT;
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG10_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG10_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG10_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG10_BORDER;
				break;
			case MOUSE_BUTTON_11:
				if (where == PANE)
					key = KEYC_MOUSEDRAG11_PANE;
				if (where == STATUS)
					key = KEYC_MOUSEDRAG11_STATUS;
				if (where == STATUS_LEFT)
					key = KEYC_MOUSEDRAG11_STATUS_LEFT;
				if (where == STATUS_RIGHT)
					key = KEYC_MOUSEDRAG11_STATUS_RIGHT;
				if (where == STATUS_DEFAULT)
					key = KEYC_MOUSEDRAG11_STATUS_DEFAULT;
				if (where == SCROLLBAR_UP)
					key = KEYC_MOUSEDRAG11_SCROLLBAR_UP;
				if (where == SCROLLBAR_SLIDER)
					key = KEYC_MOUSEDRAG11_SCROLLBAR_SLIDER;
				if (where == SCROLLBAR_DOWN)
					key = KEYC_MOUSEDRAG11_SCROLLBAR_DOWN;
				if (where == BORDER)
					key = KEYC_MOUSEDRAG11_BORDER;
				break;
			}
		}

		/*
		 * Begin a drag by setting the flag to a non-zero value that
		 * corresponds to the mouse button in use. If starting to drag
		 * the scrollbar, store the relative position in the slider
		 * where the user grabbed.
		 */
		c->tty.mouse_drag_flag = MOUSE_BUTTONS(b) + 1;
		if (c->tty.mouse_scrolling_flag == 0 &&
		    where == SCROLLBAR_SLIDER) {
			c->tty.mouse_scrolling_flag = 1;
			c->tty.mouse_slider_mpos = sl_mpos;
		}
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
		case MOUSE_BUTTON_1:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP1_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP1_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP1_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP1_BORDER;
			break;
		case MOUSE_BUTTON_2:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP2_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP2_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP2_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP2_BORDER;
			break;
		case MOUSE_BUTTON_3:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP3_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP3_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP3_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP3_BORDER;
			break;
		case MOUSE_BUTTON_6:
			if (where == PANE)
				key = KEYC_MOUSEUP6_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP6_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP6_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP6_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP6_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP6_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP6_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP6_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP6_BORDER;
			break;
		case MOUSE_BUTTON_7:
			if (where == PANE)
				key = KEYC_MOUSEUP7_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP7_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP7_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP7_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP7_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP7_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP7_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP7_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP7_BORDER;
			break;
		case MOUSE_BUTTON_8:
			if (where == PANE)
				key = KEYC_MOUSEUP8_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP8_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP8_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP8_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP8_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP8_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP8_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP8_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP8_BORDER;
			break;
		case MOUSE_BUTTON_9:
			if (where == PANE)
				key = KEYC_MOUSEUP9_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP9_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP9_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP9_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP9_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP9_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP9_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP9_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP9_BORDER;
			break;
		case MOUSE_BUTTON_10:
			if (where == PANE)
				key = KEYC_MOUSEUP1_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP1_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP1_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP1_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP10_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP10_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP10_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP1_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP1_BORDER;
			break;
		case MOUSE_BUTTON_11:
			if (where == PANE)
				key = KEYC_MOUSEUP11_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEUP11_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEUP11_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEUP11_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEUP11_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEUP11_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEUP11_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEUP11_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEUP11_BORDER;
			break;
		}
		break;
	case DOWN:
		switch (MOUSE_BUTTONS(b)) {
		case MOUSE_BUTTON_1:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN1_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN1_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN1_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN1_BORDER;
			break;
		case MOUSE_BUTTON_2:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN2_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN2_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN2_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN2_BORDER;
			break;
		case MOUSE_BUTTON_3:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN3_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN3_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN3_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN3_BORDER;
			break;
		case MOUSE_BUTTON_6:
			if (where == PANE)
				key = KEYC_MOUSEDOWN6_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN6_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN6_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN6_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN6_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN6_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN6_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN6_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN6_BORDER;
			break;
		case MOUSE_BUTTON_7:
			if (where == PANE)
				key = KEYC_MOUSEDOWN7_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN7_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN7_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN7_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN7_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN7_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN7_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN7_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN7_BORDER;
			break;
		case MOUSE_BUTTON_8:
			if (where == PANE)
				key = KEYC_MOUSEDOWN8_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN8_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN8_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN8_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN8_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN8_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN8_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN8_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN8_BORDER;
			break;
		case MOUSE_BUTTON_9:
			if (where == PANE)
				key = KEYC_MOUSEDOWN9_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN9_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN9_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN9_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN9_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN9_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN9_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN9_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN9_BORDER;
			break;
		case MOUSE_BUTTON_10:
			if (where == PANE)
				key = KEYC_MOUSEDOWN10_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN10_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN10_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN10_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN10_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN10_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN10_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN10_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN10_BORDER;
			break;
		case MOUSE_BUTTON_11:
			if (where == PANE)
				key = KEYC_MOUSEDOWN11_PANE;
			if (where == STATUS)
				key = KEYC_MOUSEDOWN11_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_MOUSEDOWN11_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_MOUSEDOWN11_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_MOUSEDOWN11_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_MOUSEDOWN11_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_MOUSEDOWN11_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_MOUSEDOWN11_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_MOUSEDOWN11_BORDER;
			break;
		}
		break;
	case SECOND:
		switch (MOUSE_BUTTONS(b)) {
		case MOUSE_BUTTON_1:
			if (where == PANE)
				key = KEYC_SECONDCLICK1_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK1_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK1_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK1_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK1_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK1_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK1_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK1_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK1_BORDER;
			break;
		case MOUSE_BUTTON_2:
			if (where == PANE)
				key = KEYC_SECONDCLICK2_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK2_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK2_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK2_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK2_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK2_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK2_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK2_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK2_BORDER;
			break;
		case MOUSE_BUTTON_3:
			if (where == PANE)
				key = KEYC_SECONDCLICK3_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK3_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK3_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK3_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK3_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK3_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK3_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK3_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK3_BORDER;
			break;
		case MOUSE_BUTTON_6:
			if (where == PANE)
				key = KEYC_SECONDCLICK6_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK6_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK6_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK6_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK6_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK6_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK6_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK6_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK6_BORDER;
			break;
		case MOUSE_BUTTON_7:
			if (where == PANE)
				key = KEYC_SECONDCLICK7_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK7_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK7_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK7_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK7_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK7_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK7_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK7_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK7_BORDER;
			break;
		case MOUSE_BUTTON_8:
			if (where == PANE)
				key = KEYC_SECONDCLICK8_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK8_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK8_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK8_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK8_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK8_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK8_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK8_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK8_BORDER;
			break;
		case MOUSE_BUTTON_9:
			if (where == PANE)
				key = KEYC_SECONDCLICK9_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK9_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK9_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK9_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK9_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK9_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK9_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK9_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK9_BORDER;
			break;
		case MOUSE_BUTTON_10:
			if (where == PANE)
				key = KEYC_SECONDCLICK10_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK10_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK10_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK10_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK10_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK10_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK10_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK10_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK10_BORDER;
			break;
		case MOUSE_BUTTON_11:
			if (where == PANE)
				key = KEYC_SECONDCLICK11_PANE;
			if (where == STATUS)
				key = KEYC_SECONDCLICK11_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_SECONDCLICK11_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_SECONDCLICK11_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_SECONDCLICK11_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_SECONDCLICK11_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_SECONDCLICK11_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_SECONDCLICK11_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_SECONDCLICK11_BORDER;
			break;
		}
		break;
	case DOUBLE:
		switch (MOUSE_BUTTONS(b)) {
		case MOUSE_BUTTON_1:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK1_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK1_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK1_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK1_BORDER;
			break;
		case MOUSE_BUTTON_2:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK2_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK2_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK2_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK2_BORDER;
			break;
		case MOUSE_BUTTON_3:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK3_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK3_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK3_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK3_BORDER;
			break;
		case MOUSE_BUTTON_6:
			if (where == PANE)
				key = KEYC_DOUBLECLICK6_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK6_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK6_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK6_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK6_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK6_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK6_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK6_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK6_BORDER;
			break;
		case MOUSE_BUTTON_7:
			if (where == PANE)
				key = KEYC_DOUBLECLICK7_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK7_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK7_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK7_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK7_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK7_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK7_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK7_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK7_BORDER;
			break;
		case MOUSE_BUTTON_8:
			if (where == PANE)
				key = KEYC_DOUBLECLICK8_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK8_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK8_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK8_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK8_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK8_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK8_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK8_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK8_BORDER;
			break;
		case MOUSE_BUTTON_9:
			if (where == PANE)
				key = KEYC_DOUBLECLICK9_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK9_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK9_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK9_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK9_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK9_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK9_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK9_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK9_BORDER;
			break;
		case MOUSE_BUTTON_10:
			if (where == PANE)
				key = KEYC_DOUBLECLICK10_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK10_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK10_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK10_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK10_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK10_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK10_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK10_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK10_BORDER;
			break;
		case MOUSE_BUTTON_11:
			if (where == PANE)
				key = KEYC_DOUBLECLICK11_PANE;
			if (where == STATUS)
				key = KEYC_DOUBLECLICK11_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_DOUBLECLICK11_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_DOUBLECLICK11_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_DOUBLECLICK11_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_DOUBLECLICK11_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_DOUBLECLICK11_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_DOUBLECLICK11_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_DOUBLECLICK11_BORDER;
			break;
		}
		break;
	case TRIPLE:
		switch (MOUSE_BUTTONS(b)) {
		case MOUSE_BUTTON_1:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK1_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK1_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK1_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK1_BORDER;
			break;
		case MOUSE_BUTTON_2:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK2_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK2_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK2_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK2_BORDER;
			break;
		case MOUSE_BUTTON_3:
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
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK3_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK3_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK3_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK3_BORDER;
			break;
		case MOUSE_BUTTON_6:
			if (where == PANE)
				key = KEYC_TRIPLECLICK6_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK6_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK6_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK6_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK6_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK6_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK6_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK6_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK6_BORDER;
			break;
		case MOUSE_BUTTON_7:
			if (where == PANE)
				key = KEYC_TRIPLECLICK7_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK7_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK7_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK7_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK7_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK7_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK7_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK7_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK7_BORDER;
			break;
		case MOUSE_BUTTON_8:
			if (where == PANE)
				key = KEYC_TRIPLECLICK8_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK8_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK8_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK8_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK8_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK8_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK8_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK8_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK8_BORDER;
			break;
		case MOUSE_BUTTON_9:
			if (where == PANE)
				key = KEYC_TRIPLECLICK9_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK9_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK9_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK9_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK9_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK9_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK9_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK9_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK9_BORDER;
			break;
		case MOUSE_BUTTON_10:
			if (where == PANE)
				key = KEYC_TRIPLECLICK10_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK10_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK10_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK10_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK10_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK10_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK10_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK10_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK10_BORDER;
			break;
		case MOUSE_BUTTON_11:
			if (where == PANE)
				key = KEYC_TRIPLECLICK11_PANE;
			if (where == STATUS)
				key = KEYC_TRIPLECLICK11_STATUS;
			if (where == STATUS_LEFT)
				key = KEYC_TRIPLECLICK11_STATUS_LEFT;
			if (where == STATUS_RIGHT)
				key = KEYC_TRIPLECLICK11_STATUS_RIGHT;
			if (where == STATUS_DEFAULT)
				key = KEYC_TRIPLECLICK11_STATUS_DEFAULT;
			if (where == SCROLLBAR_UP)
				key = KEYC_TRIPLECLICK11_SCROLLBAR_UP;
			if (where == SCROLLBAR_SLIDER)
				key = KEYC_TRIPLECLICK11_SCROLLBAR_SLIDER;
			if (where == SCROLLBAR_DOWN)
				key = KEYC_TRIPLECLICK11_SCROLLBAR_DOWN;
			if (where == BORDER)
				key = KEYC_TRIPLECLICK11_BORDER;
			break;
		}
		break;
	}
	if (key == KEYC_UNKNOWN)
		return (KEYC_UNKNOWN);

out:
	/* Apply modifiers if any. */
	if (b & MOUSE_MASK_META)
		key |= KEYC_META;
	if (b & MOUSE_MASK_CTRL)
		key |= KEYC_CTRL;
	if (b & MOUSE_MASK_SHIFT)
		key |= KEYC_SHIFT;

	if (log_get_level() != 0)
		log_debug("mouse key is %s", key_string_lookup_key (key, 1));
	return (key);
}

/* Is this a bracket paste key? */
static int
server_client_is_bracket_paste(struct client *c, key_code key)
{
	if ((key & KEYC_MASK_KEY) == KEYC_PASTE_START) {
		c->flags |= CLIENT_BRACKETPASTING;
		log_debug("%s: bracket paste on", c->name);
		return (0);
	}

	if ((key & KEYC_MASK_KEY) == KEYC_PASTE_END) {
		c->flags &= ~CLIENT_BRACKETPASTING;
		log_debug("%s: bracket paste off", c->name);
		return (0);
	}

	return !!(c->flags & CLIENT_BRACKETPASTING);
}

/* Is this fast enough to probably be a paste? */
static int
server_client_is_assume_paste(struct client *c)
{
	struct session	*s = c->session;
	struct timeval	 tv;
	int		 t;

	if (c->flags & CLIENT_BRACKETPASTING)
		return (0);
	if ((t = options_get_number(s->options, "assume-paste-time")) == 0)
		return (0);

	timersub(&c->activity_time, &c->last_activity_time, &tv);
	if (tv.tv_sec == 0 && tv.tv_usec < t * 1000) {
		if (c->flags & CLIENT_ASSUMEPASTING)
			return (1);
		c->flags |= CLIENT_ASSUMEPASTING;
		log_debug("%s: assume paste on", c->name);
		return (0);
	}
	if (c->flags & CLIENT_ASSUMEPASTING) {
		c->flags &= ~CLIENT_ASSUMEPASTING;
		log_debug("%s: assume paste off", c->name);
	}
	return (0);
}

/* Has the latest client changed? */
static void
server_client_update_latest(struct client *c)
{
	struct window	*w;

	if (c->session == NULL)
		return;
	w = c->session->curw->window;

	if (w->latest == c)
		return;
	w->latest = c;

	if (options_get_number(w->options, "window-size") == WINDOW_SIZE_LATEST)
		recalculate_size(w, 0);

	notify_client("client-active", c);
}

/* Get repeat time. */
static u_int
server_client_repeat_time(struct client *c, struct key_binding *bd)
{
	struct session	*s = c->session;
	u_int		 repeat, initial;

	if (~bd->flags & KEY_BINDING_REPEAT)
		return (0);
	repeat = options_get_number(s->options, "repeat-time");
	if (repeat == 0)
		return (0);
	if ((~c->flags & CLIENT_REPEAT) || bd->key != c->last_key) {
		initial = options_get_number(s->options, "initial-repeat-time");
		if (initial != 0)
			repeat = initial;
	}
	return (repeat);
}

/*
 * Handle data key input from client. This owns and can modify the key event it
 * is given and is responsible for freeing it.
 */
static enum cmd_retval
server_client_key_callback(struct cmdq_item *item, void *data)
{
	struct client			*c = cmdq_get_client(item);
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
	u_int				 repeat;
	uint64_t			 flags, prefix_delay;
	struct cmd_find_state		 fs;
	key_code			 key0, prefix, prefix2;

	/* Check the client is good to accept input. */
	if (s == NULL || (c->flags & CLIENT_UNATTACHEDFLAGS))
		goto out;
	wl = s->curw;

	/* Update the activity timer. */
	memcpy(&c->last_activity_time, &c->activity_time,
	    sizeof c->last_activity_time);
	if (gettimeofday(&c->activity_time, NULL) != 0)
		fatal("gettimeofday failed");
	session_update_activity(s, &c->activity_time);

	/* Check for mouse keys. */
	m->valid = 0;
	if (key == KEYC_MOUSE || key == KEYC_DOUBLECLICK) {
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
		if ((key & KEYC_MASK_KEY) == KEYC_DRAGGING) {
			c->tty.mouse_drag_update(c, m);
			goto out;
		}
		event->key = key;
	}

	/* Handle theme reporting keys. */
	if (key == KEYC_REPORT_LIGHT_THEME) {
		server_client_report_theme(c, THEME_LIGHT);
		goto out;
	}
	if (key == KEYC_REPORT_DARK_THEME) {
		server_client_report_theme(c, THEME_DARK);
		goto out;
	}

	/* Find affected pane. */
	if (!KEYC_IS_MOUSE(key) || cmd_find_from_mouse(&fs, m, 0) != 0)
		cmd_find_from_client(&fs, c, 0);
	wp = fs.wp;

	/* Forward mouse keys if disabled. */
	if (KEYC_IS_MOUSE(key) && !options_get_number(s->options, "mouse"))
		goto forward_key;

	/* Forward if bracket pasting. */
	if (server_client_is_bracket_paste (c, key))
		goto paste_key;

	/* Treat everything as a regular key when pasting is detected. */
	if (!KEYC_IS_MOUSE(key) &&
	    key != KEYC_FOCUS_IN &&
	    key != KEYC_FOCUS_OUT &&
	    (~key & KEYC_SENT) &&
	    server_client_is_assume_paste(c))
		goto paste_key;

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
	prefix = (key_code)options_get_number(s->options, "prefix");
	prefix2 = (key_code)options_get_number(s->options, "prefix2");
	key0 = (key & (KEYC_MASK_KEY|KEYC_MASK_MODIFIERS));
	if ((key0 == (prefix & (KEYC_MASK_KEY|KEYC_MASK_MODIFIERS)) ||
	    key0 == (prefix2 & (KEYC_MASK_KEY|KEYC_MASK_MODIFIERS))) &&
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

	bd = key_bindings_get(table, key0);

	/*
	 * If prefix-timeout is enabled and we're in the prefix table, see if
	 * the timeout has been exceeded. Revert to the root table if so.
	 */
	prefix_delay = options_get_number(global_options, "prefix-timeout");
	if (prefix_delay > 0 &&
	    strcmp(table->name, "prefix") == 0 &&
	    server_client_key_table_activity_diff(c) > prefix_delay) {
		/*
		 * If repeating is active and this is a repeating binding,
		 * ignore the timeout.
		 */
		if (bd != NULL &&
		    (c->flags & CLIENT_REPEAT) &&
		    (bd->flags & KEY_BINDING_REPEAT)) {
			log_debug("prefix timeout ignored, repeat is active");
		} else {
			log_debug("prefix timeout exceeded");
			server_client_set_key_table(c, NULL);
			first = table = c->keytable;
			server_status_client(c);
			goto table_changed;
		}
	}

	/* Try to see if there is a key binding in the current table. */
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
		repeat = server_client_repeat_time(c, bd);
		if (repeat != 0) {
			c->flags |= CLIENT_REPEAT;
			c->last_key = bd->key;

			tv.tv_sec = repeat / 1000;
			tv.tv_usec = (repeat % 1000) * 1000L;
			evtimer_del(&c->repeat_timer);
			evtimer_add(&c->repeat_timer, &tv);
		} else {
			c->flags &= ~CLIENT_REPEAT;
			server_client_set_key_table(c, NULL);
		}
		server_status_client(c);

		/* Execute the key binding. */
		key_bindings_dispatch(bd, item, c, event, &fs);
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
	 * Binding movement keys is useless since we only turn them on when the
	 * application requests, so don't let them exit the prefix table.
	 */
	if (key == KEYC_MOUSEMOVE_PANE ||
	    key == KEYC_MOUSEMOVE_STATUS ||
	    key == KEYC_MOUSEMOVE_STATUS_LEFT ||
	    key == KEYC_MOUSEMOVE_STATUS_RIGHT ||
	    key == KEYC_MOUSEMOVE_STATUS_DEFAULT ||
	    key == KEYC_MOUSEMOVE_BORDER)
		goto forward_key;

	/*
	 * No match in this table. If not in the root table or if repeating
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
	goto out;

paste_key:
	if (c->flags & CLIENT_READONLY)
		goto out;
	if (event->buf != NULL)
		window_pane_paste(wp, key, event->buf, event->len);
	key = KEYC_NONE;
	goto out;

out:
	if (s != NULL && key != KEYC_FOCUS_OUT)
		server_client_update_latest(c);
	free(event->buf);
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
	if (s == NULL || (c->flags & CLIENT_UNATTACHEDFLAGS))
		return (0);

	/*
	 * Key presses in overlay mode and the command prompt are a special
	 * case. The queue might be blocked so they need to be processed
	 * immediately rather than queued.
	 */
	if (~c->flags & CLIENT_READONLY) {
		if (c->message_string != NULL) {
			if (c->message_ignore_keys)
				return (0);
			status_message_clear(c);
		}
		if (c->overlay_key != NULL) {
			switch (c->overlay_key(c, c->overlay_data, event)) {
			case 0:
				return (0);
			case 1:
				server_client_clear_overlay(c);
				return (0);
			}
		}
		server_client_clear_overlay(c);
		if (c->prompt_string != NULL) {
			if (status_prompt_key(c, event->key) == 0)
				return (0);
		}
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

	/* Check for window resize. This is done before redrawing. */
	RB_FOREACH(w, windows, &windows)
		server_client_check_window_resize(w);

	/* Check clients. */
	TAILQ_FOREACH(c, &clients, entry) {
		server_client_check_exit(c);
		if (c->session != NULL) {
			server_client_check_modes(c);
			server_client_check_redraw(c);
			server_client_reset_state(c);
		}
	}

	/*
	 * Any windows will have been redrawn as part of clients, so clear
	 * their flags now.
	 */
	RB_FOREACH(w, windows, &windows) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd != -1) {
				server_client_check_pane_resize(wp);
				server_client_check_pane_buffer(wp);
			}
			wp->flags &= ~(PANE_REDRAW|PANE_REDRAWSCROLLBAR);
		}
		check_window_name(w);
	}

	/* Send theme updates. */
	RB_FOREACH(w, windows, &windows) {
		TAILQ_FOREACH(wp, &w->panes, entry)
			window_pane_send_theme_update(wp);
	}
}

/* Check if window needs to be resized. */
static void
server_client_check_window_resize(struct window *w)
{
	struct winlink	*wl;

	if (~w->flags & WINDOW_RESIZE)
		return;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session->attached != 0 && wl->session->curw == wl)
			break;
	}
	if (wl == NULL)
		return;

	log_debug("%s: resizing window @%u", __func__, w->id);
	resize_window(w, w->new_sx, w->new_sy, w->new_xpixel, w->new_ypixel);
}

/* Resize timer event. */
static void
server_client_resize_timer(__unused int fd, __unused short events, void *data)
{
	struct window_pane	*wp = data;

	log_debug("%s: %%%u resize timer expired", __func__, wp->id);
	evtimer_del(&wp->resize_timer);
}

/* Check if pane should be resized. */
static void
server_client_check_pane_resize(struct window_pane *wp)
{
	struct window_pane_resize	*r;
	struct window_pane_resize	*r1;
	struct window_pane_resize	*first;
	struct window_pane_resize	*last;
	struct timeval			 tv = { .tv_usec = 250000 };

	if (TAILQ_EMPTY(&wp->resize_queue))
		return;

	if (!event_initialized(&wp->resize_timer))
		evtimer_set(&wp->resize_timer, server_client_resize_timer, wp);
	if (evtimer_pending(&wp->resize_timer, NULL))
		return;

	log_debug("%s: %%%u needs to be resized", __func__, wp->id);
	TAILQ_FOREACH(r, &wp->resize_queue, entry) {
		log_debug("queued resize: %ux%u -> %ux%u", r->osx, r->osy,
		    r->sx, r->sy);
	}

	/*
	 * There are three cases that matter:
	 *
	 * - Only one resize. It can just be applied.
	 *
	 * - Multiple resizes and the ending size is different from the
	 *   starting size. We can discard all resizes except the most recent.
	 *
	 * - Multiple resizes and the ending size is the same as the starting
	 *   size. We must resize at least twice to force the application to
	 *   redraw. So apply the first and leave the last on the queue for
	 *   next time.
	 */
	first = TAILQ_FIRST(&wp->resize_queue);
	last = TAILQ_LAST(&wp->resize_queue, window_pane_resizes);
	if (first == last) {
		/* Only one resize. */
		window_pane_send_resize(wp, first->sx, first->sy);
		TAILQ_REMOVE(&wp->resize_queue, first, entry);
		free(first);
	} else if (last->sx != first->osx || last->sy != first->osy) {
		/* Multiple resizes ending up with a different size. */
		window_pane_send_resize(wp, last->sx, last->sy);
		TAILQ_FOREACH_SAFE(r, &wp->resize_queue, entry, r1) {
			TAILQ_REMOVE(&wp->resize_queue, r, entry);
			free(r);
		}
	} else {
		/*
		 * Multiple resizes ending up with the same size. There will
		 * not be more than one to the same size in succession so we
		 * can just use the last-but-one on the list and leave the last
		 * for later. We reduce the time until the next check to avoid
		 * a long delay between the resizes.
		 */
		r = TAILQ_PREV(last, window_pane_resizes, entry);
		window_pane_send_resize(wp, r->sx, r->sy);
		TAILQ_FOREACH_SAFE(r, &wp->resize_queue, entry, r1) {
			if (r == last)
				break;
			TAILQ_REMOVE(&wp->resize_queue, r, entry);
			free(r);
		}
		tv.tv_usec = 10000;
	}
	evtimer_add(&wp->resize_timer, &tv);
}

/* Check pane buffer size. */
static void
server_client_check_pane_buffer(struct window_pane *wp)
{
	struct evbuffer			*evb = wp->event->input;
	size_t				 minimum;
	struct client			*c;
	struct window_pane_offset	*wpo;
	int				 off = 1, flag;
	u_int				 attached_clients = 0;
	size_t				 new_size;

	/*
	 * Work out the minimum used size. This is the most that can be removed
	 * from the buffer.
	 */
	minimum = wp->offset.used;
	if (wp->pipe_fd != -1 && wp->pipe_offset.used < minimum)
		minimum = wp->pipe_offset.used;
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL)
			continue;
		attached_clients++;

		if (~c->flags & CLIENT_CONTROL) {
			off = 0;
			continue;
		}
		wpo = control_pane_offset(c, wp, &flag);
		if (wpo == NULL) {
			if (!flag)
				off = 0;
			continue;
		}
		if (!flag)
			off = 0;

		window_pane_get_new_data(wp, wpo, &new_size);
		log_debug("%s: %s has %zu bytes used and %zu left for %%%u",
		    __func__, c->name, wpo->used - wp->base_offset, new_size,
		    wp->id);
		if (wpo->used < minimum)
			minimum = wpo->used;
	}
	if (attached_clients == 0)
		off = 0;
	minimum -= wp->base_offset;
	if (minimum == 0)
		goto out;

	/* Drain the buffer. */
	log_debug("%s: %%%u has %zu minimum (of %zu) bytes used", __func__,
	    wp->id, minimum, EVBUFFER_LENGTH(evb));
	evbuffer_drain(evb, minimum);

	/*
	 * Adjust the base offset. If it would roll over, all the offsets into
	 * the buffer need to be adjusted.
	 */
	if (wp->base_offset > SIZE_MAX - minimum) {
		log_debug("%s: %%%u base offset has wrapped", __func__, wp->id);
		wp->offset.used -= wp->base_offset;
		if (wp->pipe_fd != -1)
			wp->pipe_offset.used -= wp->base_offset;
		TAILQ_FOREACH(c, &clients, entry) {
			if (c->session == NULL || (~c->flags & CLIENT_CONTROL))
				continue;
			wpo = control_pane_offset(c, wp, &flag);
			if (wpo != NULL && !flag)
				wpo->used -= wp->base_offset;
		}
		wp->base_offset = minimum;
	} else
		wp->base_offset += minimum;

out:
	/*
	 * If there is data remaining, and there are no clients able to consume
	 * it, do not read any more. This is true when there are attached
	 * clients, all of which are control clients which are not able to
	 * accept any more data.
	 */
	log_debug("%s: pane %%%u is %s", __func__, wp->id, off ? "off" : "on");
	if (off)
		bufferevent_disable(wp->event, EV_READ);
	else
		bufferevent_enable(wp->event, EV_READ);
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
	struct tty		*tty = &c->tty;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp = server_client_get_pane(c), *loop;
	struct screen		*s = NULL;
	struct options		*oo = c->session->options;
	int			 mode = 0, cursor, flags, n;
	u_int			 cx = 0, cy = 0, ox, oy, sx, sy;

	if (c->flags & (CLIENT_CONTROL|CLIENT_SUSPENDED))
		return;

	/* Disable the block flag. */
	flags = (tty->flags & TTY_BLOCK);
	tty->flags &= ~TTY_BLOCK;

	/* Get mode from overlay if any, else from screen. */
	if (c->overlay_draw != NULL) {
		if (c->overlay_mode != NULL)
			s = c->overlay_mode(c, c->overlay_data, &cx, &cy);
	} else if (c->prompt_string == NULL)
		s = wp->screen;
	else
		s = c->status.active;
	if (s != NULL)
		mode = s->mode;
	if (log_get_level() != 0) {
		log_debug("%s: client %s mode %s", __func__, c->name,
		    screen_mode_to_string(mode));
	}

	/* Reset region and margin. */
	tty_region_off(tty);
	tty_margin_off(tty);

	/* Move cursor to pane cursor and offset. */
	if (c->prompt_string != NULL) {
		n = options_get_number(oo, "status-position");
		if (n == 0)
			cy = 0;
		else {
			n = status_line_size(c);
			if (n == 0)
				cy = tty->sy - 1;
			else
				cy = tty->sy - n;
		}
		cx = c->prompt_cursor;
	} else if (c->overlay_draw == NULL) {
		cursor = 0;
		tty_window_offset(tty, &ox, &oy, &sx, &sy);
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
	}
	log_debug("%s: cursor to %u,%u", __func__, cx, cy);
	tty_cursor(tty, cx, cy);

	/*
	 * Set mouse mode if requested. To support dragging, always use button
	 * mode.
	 */
	if (options_get_number(oo, "mouse")) {
		if (c->overlay_draw == NULL) {
			mode &= ~ALL_MOUSE_MODES;
			TAILQ_FOREACH(loop, &w->panes, entry) {
				if (loop->screen->mode & MODE_MOUSE_ALL)
					mode |= MODE_MOUSE_ALL;
			}
		}
		if (~mode & MODE_MOUSE_ALL)
			mode |= MODE_MOUSE_BUTTON;
	}

	/* Clear bracketed paste mode if at the prompt. */
	if (c->overlay_draw == NULL && c->prompt_string != NULL)
		mode &= ~MODE_BRACKETPASTE;

	/* Set the terminal mode and reset attributes. */
	tty_update_mode(tty, mode, s);
	tty_reset(tty);

	/* All writing must be done, send a sync end (if it was started). */
	tty_sync_end(tty);
	tty->flags |= flags;
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
	struct client		*c = data;
	struct key_event	*event;

	log_debug("click timer expired");

	if (c->flags & CLIENT_TRIPLECLICK) {
		/*
		 * Waiting for a third click that hasn't happened, so this must
		 * have been a double click.
		 */
		event = xcalloc(1, sizeof *event);
		event->key = KEYC_DOUBLECLICK;
		memcpy(&event->m, &c->click_event, sizeof event->m);
		if (!server_client_handle_key(c, event)) {
			free(event->buf);
			free(event);
		}
	}
	c->flags &= ~(CLIENT_DOUBLECLICK|CLIENT_TRIPLECLICK);
}

/* Check if client should be exited. */
static void
server_client_check_exit(struct client *c)
{
	struct client_file	*cf;
	const char		*name = c->exit_session;
	char			*data;
	size_t			 size, msize;

	if (c->flags & (CLIENT_DEAD|CLIENT_EXITED))
		return;
	if (~c->flags & CLIENT_EXIT)
		return;

	if (c->flags & CLIENT_CONTROL) {
		control_discard(c);
		if (!control_all_done(c))
			return;
	}
	RB_FOREACH(cf, client_files, &c->files) {
		if (EVBUFFER_LENGTH(cf->buffer) != 0)
			return;
	}
	c->flags |= CLIENT_EXITED;

	switch (c->exit_type) {
	case CLIENT_EXIT_RETURN:
		if (c->exit_message != NULL)
			msize = strlen(c->exit_message) + 1;
		else
			msize = 0;
		size = (sizeof c->retval) + msize;
		data = xmalloc(size);
		memcpy(data, &c->retval, sizeof c->retval);
		if (c->exit_message != NULL)
			memcpy(data + sizeof c->retval, c->exit_message, msize);
		proc_send(c->peer, MSG_EXIT, -1, data, size);
		free(data);
		break;
	case CLIENT_EXIT_SHUTDOWN:
		proc_send(c->peer, MSG_SHUTDOWN, -1, NULL, 0);
		break;
	case CLIENT_EXIT_DETACH:
		proc_send(c->peer, c->exit_msgtype, -1, name, strlen(name) + 1);
		break;
	}
	free(c->exit_session);
	free(c->exit_message);
}

/* Redraw timer callback. */
static void
server_client_redraw_timer(__unused int fd, __unused short events,
    __unused void *data)
{
	log_debug("redraw timer fired");
}

/*
 * Check if modes need to be updated. Only modes in the current window are
 * updated and it is done when the status line is redrawn.
 */
static void
server_client_check_modes(struct client *c)
{
	struct window			*w = c->session->curw->window;
	struct window_pane		*wp;
	struct window_mode_entry	*wme;

	if (c->flags & (CLIENT_CONTROL|CLIENT_SUSPENDED))
		return;
	if (~c->flags & CLIENT_REDRAWSTATUS)
		return;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		wme = TAILQ_FIRST(&wp->modes);
		if (wme != NULL && wme->mode->update != NULL)
			wme->mode->update(wme);
	}
}

/* Check for client redraws. */
static void
server_client_check_redraw(struct client *c)
{
	struct session		*s = c->session;
	struct tty		*tty = &c->tty;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	int			 needed, tty_flags, mode = tty->mode;
	uint64_t		 client_flags = 0;
	int			 redraw_pane, redraw_scrollbar_only;
	u_int			 bit = 0;
	struct timeval		 tv = { .tv_usec = 1000 };
	static struct event	 ev;
	size_t			 left;

	if (c->flags & (CLIENT_CONTROL|CLIENT_SUSPENDED))
		return;
	if (c->flags & CLIENT_ALLREDRAWFLAGS) {
		log_debug("%s: redraw%s%s%s%s%s%s", c->name,
		    (c->flags & CLIENT_REDRAWWINDOW) ? " window" : "",
		    (c->flags & CLIENT_REDRAWSTATUS) ? " status" : "",
		    (c->flags & CLIENT_REDRAWBORDERS) ? " borders" : "",
		    (c->flags & CLIENT_REDRAWOVERLAY) ? " overlay" : "",
		    (c->flags & CLIENT_REDRAWPANES) ? " panes" : "",
		    (c->flags & CLIENT_REDRAWSCROLLBARS) ? " scrollbars" : "");
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
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->flags & PANE_REDRAW) {
				needed = 1;
				client_flags |= CLIENT_REDRAWPANES;
				break;
			}
			if (wp->flags & PANE_REDRAWSCROLLBAR) {
				needed = 1;
				client_flags |= CLIENT_REDRAWSCROLLBARS;
				/* no break - later panes may need redraw */
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

		if (~c->flags & CLIENT_REDRAWWINDOW) {
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (wp->flags & (PANE_REDRAW)) {
					log_debug("%s: pane %%%u needs redraw",
					    c->name, wp->id);
					c->redraw_panes |= (1 << bit);
				} else if (wp->flags & PANE_REDRAWSCROLLBAR) {
					log_debug("%s: pane %%%u scrollbar "
					    "needs redraw", c->name, wp->id);
					c->redraw_scrollbars |= (1 << bit);
				}
				if (++bit == 64) {
					/*
					 * If more that 64 panes, give up and
					 * just redraw the window.
					 */
					client_flags &= ~(CLIENT_REDRAWPANES|
					    CLIENT_REDRAWSCROLLBARS);
					client_flags |= CLIENT_REDRAWWINDOW;
					break;
				}
			}
			if (c->redraw_panes != 0)
				c->flags |= CLIENT_REDRAWPANES;
			if (c->redraw_scrollbars != 0)
				c->flags |= CLIENT_REDRAWSCROLLBARS;
		}
		c->flags |= client_flags;
		return;
	} else if (needed)
		log_debug("%s: redraw needed", c->name);

	tty_flags = tty->flags & (TTY_BLOCK|TTY_FREEZE|TTY_NOCURSOR);
	tty->flags = (tty->flags & ~(TTY_BLOCK|TTY_FREEZE))|TTY_NOCURSOR;

	if (~c->flags & CLIENT_REDRAWWINDOW) {
		/*
		 * If not redrawing the entire window, check whether each pane
		 * needs to be redrawn.
		 */
		TAILQ_FOREACH(wp, &w->panes, entry) {
			redraw_pane = 0;
			redraw_scrollbar_only = 0;
			if (wp->flags & PANE_REDRAW)
				redraw_pane = 1;
			else if (c->flags & CLIENT_REDRAWPANES) {
				if (c->redraw_panes & (1 << bit))
					redraw_pane = 1;
			} else if (c->flags & CLIENT_REDRAWSCROLLBARS) {
				if (c->redraw_scrollbars & (1 << bit))
					redraw_scrollbar_only = 1;
			}
			bit++;
			if (!redraw_pane && !redraw_scrollbar_only)
				continue;
			if (redraw_scrollbar_only) {
				log_debug("%s: redrawing (scrollbar only) pane "
				    "%%%u", __func__, wp->id);
			} else {
				log_debug("%s: redrawing pane %%%u", __func__,
				    wp->id);
			}
			screen_redraw_pane(c, wp, redraw_scrollbar_only);
		}
		c->redraw_panes = 0;
		c->redraw_scrollbars = 0;
		c->flags &= ~(CLIENT_REDRAWPANES|CLIENT_REDRAWSCROLLBARS);
	}

	if (c->flags & CLIENT_ALLREDRAWFLAGS) {
		if (options_get_number(s->options, "set-titles")) {
			server_client_set_title(c);
			server_client_set_path(c);
		}
		screen_redraw_screen(c);
	}

	tty->flags = (tty->flags & ~TTY_NOCURSOR)|(tty_flags & TTY_NOCURSOR);
	tty_update_mode(tty, mode, NULL);
	tty->flags = (tty->flags & ~(TTY_BLOCK|TTY_FREEZE|TTY_NOCURSOR))|
	    tty_flags;

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

/* Set client path. */
static void
server_client_set_path(struct client *c)
{
	struct session	*s = c->session;
	const char	*path;

	if (s->curw == NULL)
		return;
	if (s->curw->window->active->base.path == NULL)
		path = "";
	else
		path = s->curw->window->active->base.path;
	if (c->path == NULL || strcmp(path, c->path) != 0) {
		free(c->path);
		c->path = xstrdup(path);
		tty_set_path(&c->tty, c->path);
	}
}

/* Dispatch message from client. */
static void
server_client_dispatch(struct imsg *imsg, void *arg)
{
	struct client	*c = arg;
	ssize_t		 datalen;
	struct session	*s;

	if (c->flags & CLIENT_DEAD)
		return;

	if (imsg == NULL) {
		server_client_lost(c);
		return;
	}

	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;

	switch (imsg->hdr.type) {
	case MSG_IDENTIFY_CLIENTPID:
	case MSG_IDENTIFY_CWD:
	case MSG_IDENTIFY_ENVIRON:
	case MSG_IDENTIFY_FEATURES:
	case MSG_IDENTIFY_FLAGS:
	case MSG_IDENTIFY_LONGFLAGS:
	case MSG_IDENTIFY_STDIN:
	case MSG_IDENTIFY_STDOUT:
	case MSG_IDENTIFY_TERM:
	case MSG_IDENTIFY_TERMINFO:
	case MSG_IDENTIFY_TTYNAME:
	case MSG_IDENTIFY_DONE:
		server_client_dispatch_identify(c, imsg);
		break;
	case MSG_COMMAND:
		server_client_dispatch_command(c, imsg);
		break;
	case MSG_RESIZE:
		if (datalen != 0)
			fatalx("bad MSG_RESIZE size");

		if (c->flags & CLIENT_CONTROL)
			break;
		server_client_update_latest(c);
		tty_resize(&c->tty);
		tty_repeat_requests(&c->tty);
		recalculate_sizes();
		if (c->overlay_resize == NULL)
			server_client_clear_overlay(c);
		else
			c->overlay_resize(c, c->overlay_data);
		server_redraw_client(c);
		if (c->session != NULL)
			notify_client("client-resized", c);
		break;
	case MSG_EXITING:
		if (datalen != 0)
			fatalx("bad MSG_EXITING size");
		server_client_set_session(c, NULL);
		recalculate_sizes();
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

		if (c->fd == -1 || c->session == NULL) /* exited already */
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
	case MSG_WRITE_READY:
		file_write_ready(&c->files, imsg);
		break;
	case MSG_READ:
		file_read_data(&c->files, imsg);
		break;
	case MSG_READ_DONE:
		file_read_done(&c->files, imsg);
		break;
	}
}

/* Callback when command is not allowed. */
static enum cmd_retval
server_client_read_only(struct cmdq_item *item, __unused void *data)
{
	cmdq_error(item, "client is read-only");
	return (CMD_RETURN_ERROR);
}

/* Callback when command is done. */
static enum cmd_retval
server_client_command_done(struct cmdq_item *item, __unused void *data)
{
	struct client	*c = cmdq_get_client(item);

	if (~c->flags & CLIENT_ATTACHED)
		c->flags |= CLIENT_EXIT;
	else if (~c->flags & CLIENT_EXIT) {
		if (c->flags & CLIENT_CONTROL)
			control_ready(c);
		tty_send_requests(&c->tty);
	}
	return (CMD_RETURN_NORMAL);
}

/* Handle command message. */
static void
server_client_dispatch_command(struct client *c, struct imsg *imsg)
{
	struct msg_command	  data;
	char			 *buf;
	size_t			  len;
	int			  argc;
	char			**argv, *cause;
	struct cmd_parse_result	 *pr;
	struct args_value	 *values;
	struct cmdq_item	 *new_item;
	struct cmd_list		 *cmdlist;

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
		cmdlist = cmd_list_copy(options_get_command(global_options,
		    "default-client-command"), 0, NULL);
	} else {
		values = args_from_vector(argc, argv);
		pr = cmd_parse_from_arguments(values, argc, NULL);
		switch (pr->status) {
		case CMD_PARSE_ERROR:
			cause = pr->error;
			goto error;
		case CMD_PARSE_SUCCESS:
			break;
		}
		args_free_values(values, argc);
		free(values);
		cmd_free_argv(argc, argv);
		cmdlist = pr->cmdlist;
	}

	if ((c->flags & CLIENT_READONLY) &&
	    !cmd_list_all_have(cmdlist, CMD_READONLY))
		new_item = cmdq_get_callback(server_client_read_only, NULL);
	else
		new_item = cmdq_get_command(cmdlist, NULL);
	cmdq_append(c, new_item);
	cmdq_append(c, cmdq_get_callback(server_client_command_done, NULL));

	cmd_list_free(cmdlist);
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
	int		 flags, feat;
	uint64_t	 longflags;
	char		*name;

	if (c->flags & CLIENT_IDENTIFIED)
		fatalx("out-of-order identify message");

	data = imsg->data;
	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;

	switch (imsg->hdr.type)	{
	case MSG_IDENTIFY_FEATURES:
		if (datalen != sizeof feat)
			fatalx("bad MSG_IDENTIFY_FEATURES size");
		memcpy(&feat, data, sizeof feat);
		c->term_features |= feat;
		log_debug("client %p IDENTIFY_FEATURES %s", c,
		    tty_get_features(feat));
		break;
	case MSG_IDENTIFY_FLAGS:
		if (datalen != sizeof flags)
			fatalx("bad MSG_IDENTIFY_FLAGS size");
		memcpy(&flags, data, sizeof flags);
		c->flags |= flags;
		log_debug("client %p IDENTIFY_FLAGS %#x", c, flags);
		break;
	case MSG_IDENTIFY_LONGFLAGS:
		if (datalen != sizeof longflags)
			fatalx("bad MSG_IDENTIFY_LONGFLAGS size");
		memcpy(&longflags, data, sizeof longflags);
		c->flags |= longflags;
		log_debug("client %p IDENTIFY_LONGFLAGS %#llx", c,
		    (unsigned long long)longflags);
		break;
	case MSG_IDENTIFY_TERM:
		if (datalen == 0 || data[datalen - 1] != '\0')
			fatalx("bad MSG_IDENTIFY_TERM string");
		if (*data == '\0')
			c->term_name = xstrdup("unknown");
		else
			c->term_name = xstrdup(data);
		log_debug("client %p IDENTIFY_TERM %s", c, data);
		break;
	case MSG_IDENTIFY_TERMINFO:
		if (datalen == 0 || data[datalen - 1] != '\0')
			fatalx("bad MSG_IDENTIFY_TERMINFO string");
		c->term_caps = xreallocarray(c->term_caps, c->term_ncaps + 1,
		    sizeof *c->term_caps);
		c->term_caps[c->term_ncaps++] = xstrdup(data);
		log_debug("client %p IDENTIFY_TERMINFO %s", c, data);
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
		c->fd = imsg_get_fd(imsg);
		log_debug("client %p IDENTIFY_STDIN %d", c, c->fd);
		break;
	case MSG_IDENTIFY_STDOUT:
		if (datalen != 0)
			fatalx("bad MSG_IDENTIFY_STDOUT size");
		c->out_fd = imsg_get_fd(imsg);
		log_debug("client %p IDENTIFY_STDOUT %d", c, c->out_fd);
		break;
	case MSG_IDENTIFY_ENVIRON:
		if (datalen == 0 || data[datalen - 1] != '\0')
			fatalx("bad MSG_IDENTIFY_ENVIRON string");
		if (strchr(data, '=') != NULL)
			environ_put(c->environ, data, 0);
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

#ifdef __CYGWIN__
	c->fd = open(c->ttyname, O_RDWR|O_NOCTTY);
	c->out_fd = dup(c->fd);
#endif

	if (c->flags & CLIENT_CONTROL)
		control_start(c);
	else if (c->fd != -1) {
		if (tty_init(&c->tty, c) != 0) {
			close(c->fd);
			c->fd = -1;
		} else {
			tty_resize(&c->tty);
			c->flags |= CLIENT_TERMINAL;
		}
		close(c->out_fd);
		c->out_fd = -1;
	}

	/*
	 * If this is the first client, load configuration files. Any later
	 * clients are allowed to continue with their command even if the
	 * config has not been loaded - they might have been run from inside it
	 */
	if ((~c->flags & CLIENT_EXIT) &&
	     !cfg_finished &&
	     c == TAILQ_FIRST(&clients))
		start_cfg();
}

/* Handle shell message. */
static void
server_client_dispatch_shell(struct client *c)
{
	const char	*shell;

	shell = options_get_string(global_s_options, "default-shell");
	if (!checkshell(shell))
		shell = _PATH_BSHELL;
	proc_send(c->peer, MSG_SHELL, -1, shell, strlen(shell) + 1);

	proc_kill_peer(c->peer);
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

/* Get control client flags. */
static uint64_t
server_client_control_flags(struct client *c, const char *next)
{
	if (strcmp(next, "pause-after") == 0) {
		c->pause_age = 0;
		return (CLIENT_CONTROL_PAUSEAFTER);
	}
	if (sscanf(next, "pause-after=%u", &c->pause_age) == 1) {
		c->pause_age *= 1000;
		return (CLIENT_CONTROL_PAUSEAFTER);
	}
	if (strcmp(next, "no-output") == 0)
		return (CLIENT_CONTROL_NOOUTPUT);
	if (strcmp(next, "wait-exit") == 0)
		return (CLIENT_CONTROL_WAITEXIT);
	return (0);
}

/* Set client flags. */
void
server_client_set_flags(struct client *c, const char *flags)
{
	char	*s, *copy, *next;
	uint64_t flag;
	int	 not;

	s = copy = xstrdup(flags);
	while ((next = strsep(&s, ",")) != NULL) {
		not = (*next == '!');
		if (not)
			next++;

		if (c->flags & CLIENT_CONTROL)
			flag = server_client_control_flags(c, next);
		else
			flag = 0;
		if (strcmp(next, "read-only") == 0)
			flag = CLIENT_READONLY;
		else if (strcmp(next, "ignore-size") == 0)
			flag = CLIENT_IGNORESIZE;
		else if (strcmp(next, "active-pane") == 0)
			flag = CLIENT_ACTIVEPANE;
		else if (strcmp(next, "no-detach-on-destroy") == 0)
			flag = CLIENT_NO_DETACH_ON_DESTROY;
		if (flag == 0)
			continue;

		log_debug("client %s set flag %s", c->name, next);
		if (not) {
			if (c->flags & CLIENT_READONLY)
				flag &= ~CLIENT_READONLY;
			c->flags &= ~flag;
		} else
			c->flags |= flag;
		if (flag == CLIENT_CONTROL_NOOUTPUT)
			control_reset_offsets(c);
	}
	free(copy);
	proc_send(c->peer, MSG_FLAGS, -1, &c->flags, sizeof c->flags);
}

/* Get client flags. This is only flags useful to show to users. */
const char *
server_client_get_flags(struct client *c)
{
	static char	s[256];
	char		tmp[32];

	*s = '\0';
	if (c->flags & CLIENT_ATTACHED)
		strlcat(s, "attached,", sizeof s);
	if (c->flags & CLIENT_FOCUSED)
		strlcat(s, "focused,", sizeof s);
	if (c->flags & CLIENT_CONTROL)
		strlcat(s, "control-mode,", sizeof s);
	if (c->flags & CLIENT_IGNORESIZE)
		strlcat(s, "ignore-size,", sizeof s);
	if (c->flags & CLIENT_NO_DETACH_ON_DESTROY)
		strlcat(s, "no-detach-on-destroy,", sizeof s);
	if (c->flags & CLIENT_CONTROL_NOOUTPUT)
		strlcat(s, "no-output,", sizeof s);
	if (c->flags & CLIENT_CONTROL_WAITEXIT)
		strlcat(s, "wait-exit,", sizeof s);
	if (c->flags & CLIENT_CONTROL_PAUSEAFTER) {
		xsnprintf(tmp, sizeof tmp, "pause-after=%u,",
		    c->pause_age / 1000);
		strlcat(s, tmp, sizeof s);
	}
	if (c->flags & CLIENT_READONLY)
		strlcat(s, "read-only,", sizeof s);
	if (c->flags & CLIENT_ACTIVEPANE)
		strlcat(s, "active-pane,", sizeof s);
	if (c->flags & CLIENT_SUSPENDED)
		strlcat(s, "suspended,", sizeof s);
	if (c->flags & CLIENT_UTF8)
		strlcat(s, "UTF-8,", sizeof s);
	if (*s != '\0')
		s[strlen(s) - 1] = '\0';
	return (s);
}

/* Get client window. */
struct client_window *
server_client_get_client_window(struct client *c, u_int id)
{
	struct client_window	cw = { .window = id };

	return (RB_FIND(client_windows, &c->windows, &cw));
}

/* Add client window. */
struct client_window *
server_client_add_client_window(struct client *c, u_int id)
{
	struct client_window	*cw;

	cw = server_client_get_client_window(c, id);
	if (cw == NULL) {
		cw = xcalloc(1, sizeof *cw);
		cw->window = id;
		RB_INSERT(client_windows, &c->windows, cw);
	}
	return (cw);
}

/* Get client active pane. */
struct window_pane *
server_client_get_pane(struct client *c)
{
	struct session		*s = c->session;
	struct client_window	*cw;

	if (s == NULL)
		return (NULL);

	if (~c->flags & CLIENT_ACTIVEPANE)
		return (s->curw->window->active);
	cw = server_client_get_client_window(c, s->curw->window->id);
	if (cw == NULL)
		return (s->curw->window->active);
	return (cw->pane);
}

/* Set client active pane. */
void
server_client_set_pane(struct client *c, struct window_pane *wp)
{
	struct session		*s = c->session;
	struct client_window	*cw;

	if (s == NULL)
		return;

	cw = server_client_add_client_window(c, s->curw->window->id);
	cw->pane = wp;
	log_debug("%s pane now %%%u", c->name, wp->id);
}

/* Remove pane from client lists. */
void
server_client_remove_pane(struct window_pane *wp)
{
	struct client		*c;
	struct window		*w = wp->window;
	struct client_window	*cw;

	TAILQ_FOREACH(c, &clients, entry) {
		cw = server_client_get_client_window(c, w->id);
		if (cw != NULL && cw->pane == wp) {
			RB_REMOVE(client_windows, &c->windows, cw);
			free(cw);
		}
	}
}

/* Print to a client. */
void
server_client_print(struct client *c, int parse, struct evbuffer *evb)
{
	void				*data = EVBUFFER_DATA(evb);
	size_t				 size = EVBUFFER_LENGTH(evb);
	struct window_pane		*wp;
	struct window_mode_entry	*wme;
	char				*sanitized, *msg, *line, empty = '\0';

	if (!parse) {
		utf8_stravisx(&msg, data, size,
		    VIS_OCTAL|VIS_CSTYLE|VIS_NOSLASH);
	} else {
		if (size == 0)
			msg = &empty;
		else {
			msg = EVBUFFER_DATA(evb);
			if (msg[size - 1] != '\0')
				evbuffer_add(evb, "", 1);
		}
	}
	log_debug("%s: %s", __func__, msg);

	if (c == NULL)
		goto out;

	if (c->session == NULL || (c->flags & CLIENT_CONTROL)) {
		if (~c->flags & CLIENT_UTF8) {
			sanitized = utf8_sanitize(msg);
			if (c->flags & CLIENT_CONTROL)
				control_write(c, "%s", sanitized);
			else
				file_print(c, "%s\n", sanitized);
			free(sanitized);
		} else {
			if (c->flags & CLIENT_CONTROL)
				control_write(c, "%s", msg);
			else
				file_print(c, "%s\n", msg);
		}
		goto out;
	}

	wp = server_client_get_pane(c);
	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->mode != &window_view_mode)
		window_pane_set_mode(wp, NULL, &window_view_mode, NULL, NULL);
	if (parse) {
		do {
			line = evbuffer_readln(evb, NULL, EVBUFFER_EOL_LF);
			if (line != NULL) {
				window_copy_add(wp, 1, "%s", line);
				free(line);
			}
		} while (line != NULL);

		size = EVBUFFER_LENGTH(evb);
		if (size != 0) {
			line = EVBUFFER_DATA(evb);
			window_copy_add(wp, 1, "%.*s", (int)size, line);
		}
	} else
		window_copy_add(wp, 0, "%s", msg);

out:
	if (!parse)
		free(msg);
}

static void
server_client_report_theme(struct client *c, enum client_theme theme)
{
	if (theme == THEME_LIGHT) {
		c->theme = THEME_LIGHT;
		notify_client("client-light-theme", c);
	} else {
		c->theme = THEME_DARK;
		notify_client("client-dark-theme", c);
	}

	/*
	 * Request foreground and background colour again. Don't forward 2031 to
	 * panes until a response is received.
	 */
	tty_puts(&c->tty, "\033]10;?\033\\\033]11;?\033\\");
}
