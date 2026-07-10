/* $OpenBSD: status.c,v 1.273 2026/07/06 14:29:10 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/time.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

static void	 status_message_area(struct client *, u_int *, u_int *);
static void	 status_message_callback(int, short, void *);
static void	 status_timer_callback(int, short, void *);

/* Status timer callback. */
static void
status_timer_callback(__unused int fd, __unused short events, void *arg)
{
	struct client	*c = arg;
	struct session	*s = c->session;
	struct timeval	 tv;

	evtimer_del(&c->status.timer);

	if (s == NULL)
		return;

	if (c->message_string == NULL && c->prompt == NULL)
		c->flags |= CLIENT_REDRAWSTATUS;

	timerclear(&tv);
	tv.tv_sec = options_get_number(s->options, "status-interval");

	if (tv.tv_sec != 0)
		evtimer_add(&c->status.timer, &tv);
	log_debug("client %p, status interval %d", c, (int)tv.tv_sec);
}

/* Start status timer for client. */
void
status_timer_start(struct client *c)
{
	struct session	*s = c->session;

	if (event_initialized(&c->status.timer))
		evtimer_del(&c->status.timer);
	else
		evtimer_set(&c->status.timer, status_timer_callback, c);

	if (s != NULL && options_get_number(s->options, "status"))
		status_timer_callback(-1, 0, c);
}

/* Start status timer for all clients. */
void
status_timer_start_all(void)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry)
		status_timer_start(c);
}

/* Update status cache. */
void
status_update_cache(struct session *s)
{
	s->statuslines = options_get_number(s->options, "status");
	if (s->statuslines == 0)
		s->statusat = -1;
	else if (options_get_number(s->options, "status-position") == 0)
		s->statusat = 0;
	else
		s->statusat = 1;
}

/* Get screen line of status line. -1 means off. */
int
status_at_line(struct client *c)
{
	struct session	*s = c->session;

	if (c->flags & (CLIENT_STATUSOFF|CLIENT_CONTROL))
		return (-1);
	if (s->statusat != 1)
		return (s->statusat);
	return (c->tty.sy - status_line_size(c));
}

/* Get size of status line for client's session. 0 means off. */
u_int
status_line_size(struct client *c)
{
	struct session	*s = c->session;

	if (c->flags & (CLIENT_STATUSOFF|CLIENT_CONTROL))
		return (0);
	if (s == NULL)
		return (options_get_number(global_s_options, "status"));
	return (s->statuslines);
}

/* Get the prompt line number for client's session. 1 means at the bottom. */
u_int
status_prompt_line_at(struct client *c)
{
	struct session	*s = c->session;
	u_int		 line, lines;

	lines = status_line_size(c);
	if (lines == 0)
		return (0);
	line = options_get_number(s->options, "message-line");
	if (line >= lines)
		return (lines - 1);
	return (line);
}

/* Get window at window list position. */
struct style_range *
status_get_range(struct client *c, u_int x, u_int y)
{
	struct status_line	*sl = &c->status;

	if (y >= nitems(sl->entries))
		return (NULL);
	return (style_ranges_get_range(&sl->entries[y].ranges, x));
}

/* Save old status line. */
static void
status_push_screen(struct client *c)
{
	struct status_line *sl = &c->status;

	if (sl->active == &sl->screen) {
		sl->active = xmalloc(sizeof *sl->active);
		screen_init(sl->active, c->tty.sx, status_line_size(c), 0);
	}
	sl->references++;
}

/* Restore old status line. */
static void
status_pop_screen(struct client *c)
{
	struct status_line *sl = &c->status;

	if (--sl->references == 0) {
		screen_free(sl->active);
		free(sl->active);
		sl->active = &sl->screen;
	}
}

/* Initialize status line. */
void
status_init(struct client *c)
{
	struct status_line	*sl = &c->status;
	u_int			 i;

	for (i = 0; i < nitems(sl->entries); i++)
		style_ranges_init(&sl->entries[i].ranges);

	screen_init(&sl->screen, c->tty.sx, 1, 0);
	sl->active = &sl->screen;
}

/* Free status line. */
void
status_free(struct client *c)
{
	struct status_line	*sl = &c->status;
	u_int			 i;

	for (i = 0; i < nitems(sl->entries); i++) {
		style_ranges_free(&sl->entries[i].ranges);
		free((void *)sl->entries[i].expanded);
	}

	if (event_initialized(&sl->timer))
		evtimer_del(&sl->timer);

	if (sl->active != &sl->screen) {
		screen_free(sl->active);
		free(sl->active);
	}
	screen_free(&sl->screen);
}

/* Draw status line for client. */
int
status_redraw(struct client *c)
{
	struct status_line		*sl = &c->status;
	struct style_line_entry		*sle;
	struct session			*s = c->session;
	struct screen_write_ctx		 ctx;
	struct grid_cell		 gc;
	u_int				 lines, i, n, width = c->tty.sx;
	int				 flags, force = 0, changed = 0, fg, bg;
	struct options_entry		*o;
	union options_value		*ov;
	struct format_tree		*ft;
	char				*expanded;

	log_debug("%s enter", __func__);

	/* Shouldn't get here if not the active screen. */
	if (sl->active != &sl->screen)
		fatalx("not the active screen");

	/* No status line? */
	lines = status_line_size(c);
	if (c->tty.sy == 0 || lines == 0)
		return (1);

	/* Create format tree. */
	flags = FORMAT_STATUS;
	if (c->flags & CLIENT_STATUSFORCE)
		flags |= FORMAT_FORCE;
	ft = format_create(c, NULL, FORMAT_NONE, flags);
	format_defaults(ft, c, NULL, NULL, NULL);

	/* Set up default colour. */
	style_apply(&gc, s->options, "status-style", ft);
	fg = options_get_number(s->options, "status-fg");
	if (!COLOUR_DEFAULT(fg))
		gc.fg = fg;
	bg = options_get_number(s->options, "status-bg");
	if (!COLOUR_DEFAULT(bg))
		gc.bg = bg;
	if (!grid_cells_equal(&gc, &sl->style)) {
		force = 1;
		memcpy(&sl->style, &gc, sizeof sl->style);
	}

	/* Resize the target screen. */
	if (screen_size_x(&sl->screen) != width ||
	    screen_size_y(&sl->screen) != lines) {
		screen_resize(&sl->screen, width, lines, 0);
		changed = force = 1;
	}
	screen_write_start(&ctx, &sl->screen);

	/* Write the status lines. */
	o = options_get(s->options, "status-format");
	if (o == NULL) {
		for (n = 0; n < width * lines; n++)
			screen_write_putc(&ctx, &gc, ' ');
	} else {
		for (i = 0; i < lines; i++) {
			screen_write_cursormove(&ctx, 0, i, 0);

			ov = options_array_getv(o, "%u", i);
			if (ov == NULL) {
				for (n = 0; n < width; n++)
					screen_write_putc(&ctx, &gc, ' ');
				continue;
			}
			sle = &sl->entries[i];

			expanded = format_expand_time(ft, ov->string);
			if (!force &&
			    sle->expanded != NULL &&
			    strcmp(expanded, sle->expanded) == 0) {
				free(expanded);
				continue;
			}
			changed = 1;

			for (n = 0; n < width; n++)
				screen_write_putc(&ctx, &gc, ' ');
			screen_write_cursormove(&ctx, 0, i, 0);

			style_ranges_free(&sle->ranges);
			format_draw(&ctx, &gc, width, expanded, &sle->ranges,
			    0);

			free(sle->expanded);
			sle->expanded = expanded;
		}
	}
	screen_write_stop(&ctx);

	/* Free the format tree. */
	format_free(ft);

	/* Return if the status line has changed. */
	log_debug("%s exit: force=%d, changed=%d", __func__, force, changed);
	return (force || changed);
}

/* Escape # characters in a string so format_draw treats them as literal. */
static char *
status_message_escape(const char *s)
{
	const char	*cp;
	char		*out, *p;
	size_t		 n = 0;

	for (cp = s; *cp != '\0'; cp++) {
		if (*cp == '#')
			n++;
	}
	p = out = xmalloc(strlen(s) + n + 1);
	for (cp = s; *cp != '\0'; cp++) {
		if (*cp == '#')
			*p++ = '#';
		*p++ = *cp;
	}
	*p = '\0';
	return (out);
}

/* Set a status line message. */
void
status_message_set(struct client *c, int delay, int ignore_styles,
    int ignore_keys, int no_freeze, const char *fmt, ...)
{
	struct timeval	 tv;
	va_list		 ap;
	char		*s;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);

	log_debug("%s: %s", __func__, s);

	if (c == NULL) {
		server_add_message("message: %s", s);
		free(s);
		return;
	}

	status_message_clear(c);
	status_push_screen(c);
	c->message_string = s;
	server_add_message("%s message: %s", c->name, s);

	/*
	 * With delay -1, the display-time option is used; zero means wait for
	 * key press; more than zero is the actual delay time in milliseconds.
	 */
	if (delay == -1)
		delay = options_get_number(c->session->options, "display-time");
	if (delay > 0) {
		tv.tv_sec = delay / 1000;
		tv.tv_usec = (delay % 1000) * 1000L;

		if (event_initialized(&c->message_timer))
			evtimer_del(&c->message_timer);
		evtimer_set(&c->message_timer, status_message_callback, c);

		evtimer_add(&c->message_timer, &tv);
	}

	if (delay != 0)
		c->message_ignore_keys = ignore_keys;
	c->message_ignore_styles = ignore_styles;

	if (!no_freeze)
		c->tty.flags |= TTY_FREEZE;
	c->tty.flags |= TTY_NOCURSOR;
	c->flags |= CLIENT_REDRAWSTATUS;
}

/* Clear status line message. */
void
status_message_clear(struct client *c)
{
	if (c->message_string == NULL)
		return;

	free(c->message_string);
	c->message_string = NULL;

	if (c->prompt == NULL)
		c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_ALLREDRAWFLAGS; /* was frozen and may have changed */

	status_pop_screen(c);
}

/*
 * Calculate prompt/message area geometry from the style's width and align
 * directives: x offset and available width within the status line.
 */
static void
status_message_area(struct client *c, u_int *area_x, u_int *area_w)
{
	struct session		*s = c->session;
	struct style		*sy;
	u_int			 w;

	/* Get width from message-style's width directive. */
	sy = options_string_to_style(s->options, "message-style", NULL);
	if (sy != NULL && sy->width >= 0) {
		if (sy->width_percentage)
			w = (c->tty.sx * (u_int)sy->width) / 100;
		else
			w = (u_int)sy->width;
	} else
		w = c->tty.sx;
	if (w == 0 || w > c->tty.sx)
		w = c->tty.sx;

	/* Get horizontal position from message-style's align directive. */
	if (sy != NULL) {
		switch (sy->align) {
		case STYLE_ALIGN_CENTRE:
		case STYLE_ALIGN_ABSOLUTE_CENTRE:
			*area_x = (c->tty.sx - w) / 2;
			break;
		case STYLE_ALIGN_RIGHT:
			*area_x = c->tty.sx - w;
			break;
		default:
			*area_x = 0;
			break;
		}
	} else
		*area_x = 0;

	*area_w = w;
}

/* Clear status line message after timer expires. */
static void
status_message_callback(__unused int fd, __unused short event, void *data)
{
	struct client	*c = data;

	status_message_clear(c);
}

/* Draw client message on status line of present else on last line. */
int
status_message_redraw(struct client *c)
{
	struct status_line	*sl = &c->status;
	struct screen_write_ctx	 ctx;
	struct session		*s = c->session;
	struct screen		 old_screen;
	u_int			 lines, messageline;
	u_int			 ax, aw;
	struct grid_cell	 gc;
	struct format_tree	*ft;
	const char		*msgfmt;
	char			*expanded, *msg;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_screen, sl->active, sizeof old_screen);

	lines = status_line_size(c);
	if (lines <= 1)
		lines = 1;
	screen_init(sl->active, c->tty.sx, lines, 0);

	messageline = status_prompt_line_at(c);
	if (messageline > lines - 1)
		messageline = lines - 1;

	status_message_area(c, &ax, &aw);

	ft = format_create_defaults(NULL, c, NULL, NULL, NULL);
	style_apply(&gc, s->options, "message-style", ft);

	/*
	 * Set #{message} in the format tree. If styles should be ignored in
	 * the message content, escape # characters so format_draw treats them
	 * as literal text.
	 */
	if (c->message_ignore_styles) {
		msg = status_message_escape(c->message_string);
		format_add(ft, "message", "%s", msg);
		free(msg);
	} else
		format_add(ft, "message", "%s", c->message_string);
	format_add(ft, "command_prompt", "%d", 0);

	msgfmt = options_get_string(s->options, "message-format");
	expanded = format_expand_time(ft, msgfmt);
	format_free(ft);

	screen_write_start(&ctx, sl->active);
	screen_write_fast_copy(&ctx, &sl->screen, 0, 0, c->tty.sx, lines);
	screen_write_cursormove(&ctx, ax, messageline, 0);
	format_draw(&ctx, &gc, aw, expanded, NULL, 0);
	screen_write_stop(&ctx);

	free(expanded);

	if (grid_compare(sl->active->grid, old_screen.grid) == 0) {
		screen_free(&old_screen);
		return (0);
	}
	screen_free(&old_screen);
	return (1);
}


struct status_prompt_data {
	struct client		*c;
	status_prompt_input_cb	 inputcb;
	prompt_free_cb		 freecb;
	void			*data;
};

static enum prompt_result
status_prompt_input_callback(void *data, const char *s,
    enum prompt_key_result key)
{
	struct status_prompt_data	*spd = data;
	struct client			*c = spd->c;
	status_prompt_input_cb		 inputcb = spd->inputcb;
	void				*arg = spd->data;

	if (inputcb != NULL)
		return (inputcb(c, arg, s, key));
	return (PROMPT_CLOSE);
}

static void
status_prompt_free_callback(void *data)
{
	struct status_prompt_data	*spd = data;
	prompt_free_cb			 freecb = spd->freecb;
	void				*arg = spd->data;

	if (freecb != NULL)
		freecb(arg);
	free(spd);
}

/* Accept prompt immediately. */
static enum cmd_retval
status_prompt_accept(__unused struct cmdq_item *item, void *data)
{
	struct client	*c = data;

	if (c->prompt != NULL)
		status_prompt_key(c, 'y', NULL);
	return (CMD_RETURN_NORMAL);
}

/* Enable status line prompt. */
void
status_prompt_set(struct client *c, struct cmd_find_state *fs,
    const char *msg, const char *input, status_prompt_input_cb inputcb,
    prompt_free_cb freecb, void *data, int flags, enum prompt_type prompt_type)
{
	struct prompt_create_data	pd;
	struct status_prompt_data	*spd;

	server_client_clear_overlay(c);

	status_message_clear(c);
	status_prompt_clear(c);
	status_push_screen(c);

	spd = xcalloc(1, sizeof *spd);
	spd->c = c;
	spd->inputcb = inputcb;
	spd->freecb = freecb;
	spd->data = data;

	memset(&pd, 0, sizeof pd);
	prompt_set_options(&pd, c->session);
	pd.fs = fs;
	pd.prompt = msg;
	pd.input = input;
	pd.type = prompt_type;
	pd.flags = flags;
	pd.inputcb = status_prompt_input_callback;
	pd.freecb = status_prompt_free_callback;
	pd.data = spd;
	c->prompt = prompt_create(&pd);

	if ((~flags & PROMPT_INCREMENTAL) && (~flags & PROMPT_NOFREEZE))
		c->tty.flags |= TTY_FREEZE;
	c->flags |= CLIENT_REDRAWSTATUS;

	prompt_incremental_start(c->prompt);

	if ((flags & PROMPT_SINGLE) && (flags & PROMPT_ACCEPT))
		cmdq_append(c, cmdq_get_callback(status_prompt_accept, c));
}

/* Remove status line prompt. */
void
status_prompt_clear(struct client *c)
{
	if (c->prompt == NULL)
		return;

	prompt_free(c->prompt);
	c->prompt = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_ALLREDRAWFLAGS; /* was frozen and may have changed */

	status_pop_screen(c);
}

/* Update status line prompt with a new prompt string. */
void
status_prompt_update(struct client *c, const char *msg, const char *input)
{
	if (c->prompt == NULL)
		return;
	prompt_update(c->prompt, msg, input);
	c->flags |= CLIENT_REDRAWSTATUS;
}

/* Get the screen line on which the prompt is drawn. */
static u_int
status_prompt_screen_line(struct client *c)
{
	struct tty	*tty = &c->tty;
	u_int		 n;

	if (options_get_number(c->session->options, "status-position") == 0)
		return (status_prompt_line_at(c));
	n = status_line_size(c) - status_prompt_line_at(c);
	if (n <= tty->sy)
		return (tty->sy - n);
	return (tty->sy - 1);
}

/* Draw client prompt on status line of present else on last line. */
int
status_prompt_redraw(struct client *c)
{
	struct status_line	*sl = &c->status;
	struct screen_write_ctx	 ctx;
	struct screen		 old_screen;
	struct prompt_draw_data	 pdd;
	u_int			 lines, ax, aw, promptline;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_screen, sl->active, sizeof old_screen);

	lines = status_line_size(c);
	if (lines <= 1)
		lines = 1;
	screen_init(sl->active, c->tty.sx, lines, 0);

	promptline = status_prompt_line_at(c);
	if (promptline > lines - 1)
		promptline = lines - 1;

	status_message_area(c, &ax, &aw);

	screen_write_start(&ctx, sl->active);
	screen_write_fast_copy(&ctx, &sl->screen, 0, 0, c->tty.sx, lines);

	pdd.ctx = &ctx;
	pdd.area_x = ax;
	pdd.area_width = aw;
	pdd.prompt_line = promptline;
	pdd.cursor_x = &sl->prompt_cx;
	prompt_draw(c->prompt, &pdd);

	screen_write_stop(&ctx);

	if (grid_compare(sl->active->grid, old_screen.grid) == 0) {
		screen_free(&old_screen);
		return (0);
	}
	screen_free(&old_screen);
	return (1);
}

/* Work out the tty cursor position for the prompt. */
void
status_prompt_cursor(struct client *c, u_int *cx, u_int *cy)
{
	*cy = status_prompt_screen_line(c);
	*cx = c->status.prompt_cx;
}

/* Handle keys in prompt. */
enum prompt_key_result
status_prompt_key(struct client *c, key_code key, struct mouse_event *m)
{
	enum prompt_key_result	result;
	u_int			ax, aw;
	int			redraw = 0;

	if (KEYC_IS_MOUSE(key)) {
		if (m == NULL || MOUSE_BUTTONS(m->b) != MOUSE_BUTTON_1 ||
		    MOUSE_DRAG(m->b) || MOUSE_RELEASE(m->b) ||
		    m->y != status_prompt_screen_line(c))
			return (PROMPT_KEY_NOT_HANDLED);
		status_message_area(c, &ax, &aw);
		result = prompt_mouse(c->prompt, m->x, ax, aw, &redraw);
	} else
		result = prompt_key(c->prompt, key, &redraw);
	if (redraw && c->prompt != NULL)
		c->flags |= CLIENT_REDRAWSTATUS;
	if (c->prompt != NULL && prompt_closed(c->prompt))
		status_prompt_clear(c);
	return (result);
}
