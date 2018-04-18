/* $OpenBSD$ */

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

static char	*status_redraw_get_left(struct client *, time_t,
		     struct grid_cell *, size_t *);
static char	*status_redraw_get_right(struct client *, time_t,
		     struct grid_cell *, size_t *);
static char	*status_print(struct client *, struct winlink *, time_t,
		     struct grid_cell *);
static char	*status_replace(struct client *, struct winlink *, const char *,
		     time_t);
static void	 status_message_callback(int, short, void *);
static void	 status_timer_callback(int, short, void *);

static char	*status_prompt_find_history_file(void);
static const char *status_prompt_up_history(u_int *);
static const char *status_prompt_down_history(u_int *);
static void	 status_prompt_add_history(const char *);

static const char **status_prompt_complete_list(u_int *, const char *);
static char	*status_prompt_complete_prefix(const char **, u_int);
static char	*status_prompt_complete(struct session *, const char *);

/* Status prompt history. */
#define PROMPT_HISTORY 100
static char	**status_prompt_hlist;
static u_int	  status_prompt_hsize;

/* Find the history file to load/save from/to. */
static char *
status_prompt_find_history_file(void)
{
	const char	*home, *history_file;
	char		*path;

	history_file = options_get_string(global_options, "history-file");
	if (*history_file == '\0')
		return (NULL);
	if (*history_file == '/')
		return (xstrdup(history_file));

	if (history_file[0] != '~' || history_file[1] != '/')
		return (NULL);
	if ((home = find_home()) == NULL)
		return (NULL);
	xasprintf(&path, "%s%s", home, history_file + 1);
	return (path);
}

/* Load status prompt history from file. */
void
status_prompt_load_history(void)
{
	FILE	*f;
	char	*history_file, *line, *tmp;
	size_t	 length;

	if ((history_file = status_prompt_find_history_file()) == NULL)
		return;
	log_debug("loading history from %s", history_file);

	f = fopen(history_file, "r");
	if (f == NULL) {
		log_debug("%s: %s", history_file, strerror(errno));
		free(history_file);
		return;
	}
	free(history_file);

	for (;;) {
		if ((line = fgetln(f, &length)) == NULL)
			break;

		if (length > 0) {
			if (line[length - 1] == '\n') {
				line[length - 1] = '\0';
				status_prompt_add_history(line);
			} else {
				tmp = xmalloc(length + 1);
				memcpy(tmp, line, length);
				tmp[length] = '\0';
				status_prompt_add_history(tmp);
				free(tmp);
			}
		}
	}
	fclose(f);
}

/* Save status prompt history to file. */
void
status_prompt_save_history(void)
{
	FILE	*f;
	u_int	 i;
	char	*history_file;

	if ((history_file = status_prompt_find_history_file()) == NULL)
		return;
	log_debug("saving history to %s", history_file);

	f = fopen(history_file, "w");
	if (f == NULL) {
		log_debug("%s: %s", history_file, strerror(errno));
		free(history_file);
		return;
	}
	free(history_file);

	for (i = 0; i < status_prompt_hsize; i++) {
		fputs(status_prompt_hlist[i], f);
		fputc('\n', f);
	}
	fclose(f);

}

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

	if (c->message_string == NULL && c->prompt_string == NULL)
		c->flags |= CLIENT_STATUS;

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
status_update_saved(struct session *s)
{
	if (!options_get_number(s->options, "status"))
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

	if (c->flags & CLIENT_STATUSOFF)
		return (-1);
	if (s->statusat != 1)
		return (s->statusat);
	return (c->tty.sy - status_line_size(s));
}

/*
 * Get size of status line for session. 0 means off. Note that status line may
 * be forced off for an individual client if it is too small (the
 * CLIENT_STATUSOFF flag is set for this).
 */
u_int
status_line_size(struct session *s)
{
	if (s->statusat == -1)
		return (0);
	return (1);
}

/* Retrieve options for left string. */
static char *
status_redraw_get_left(struct client *c, time_t t, struct grid_cell *gc,
    size_t *size)
{
	struct session	*s = c->session;
	const char	*template;
	char		*left;
	size_t		 leftlen;

	style_apply_update(gc, s->options, "status-left-style");

	template = options_get_string(s->options, "status-left");
	left = status_replace(c, NULL, template, t);

	*size = options_get_number(s->options, "status-left-length");
	leftlen = screen_write_cstrlen("%s", left);
	if (leftlen < *size)
		*size = leftlen;
	return (left);
}

/* Retrieve options for right string. */
static char *
status_redraw_get_right(struct client *c, time_t t, struct grid_cell *gc,
    size_t *size)
{
	struct session	*s = c->session;
	const char	*template;
	char		*right;
	size_t		 rightlen;

	style_apply_update(gc, s->options, "status-right-style");

	template = options_get_string(s->options, "status-right");
	right = status_replace(c, NULL, template, t);

	*size = options_get_number(s->options, "status-right-length");
	rightlen = screen_write_cstrlen("%s", right);
	if (rightlen < *size)
		*size = rightlen;
	return (right);
}

/* Get window at window list position. */
struct window *
status_get_window_at(struct client *c, u_int x)
{
	struct session	*s = c->session;
	struct winlink	*wl;
	struct options	*oo;
	const char	*sep;
	size_t		 seplen;

	x += c->wlmouse;
	RB_FOREACH(wl, winlinks, &s->windows) {
		oo = wl->window->options;

		sep = options_get_string(oo, "window-status-separator");
		seplen = screen_write_cstrlen("%s", sep);

		if (x < wl->status_width)
			return (wl->window);
		x -= wl->status_width + seplen;
	}
	return (NULL);
}

/* Draw status for client on the last lines of given context. */
int
status_redraw(struct client *c)
{
	struct screen_write_ctx	 ctx;
	struct session		*s = c->session;
	struct winlink		*wl;
	struct screen		 old_status, window_list;
	struct grid_cell	 stdgc, lgc, rgc, gc;
	struct options		*oo;
	time_t			 t;
	char			*left, *right;
	const char		*sep;
	u_int			 offset, needed, lines;
	u_int			 wlstart, wlwidth, wlavailable, wloffset, wlsize;
	size_t			 llen, rlen, seplen;
	int			 larrow, rarrow;

	/* Delete the saved status line, if any. */
	if (c->status.old_status != NULL) {
		screen_free(c->status.old_status);
		free(c->status.old_status);
		c->status.old_status = NULL;
	}

	/* No status line? */
	lines = status_line_size(s);
	if (c->tty.sy == 0 || lines == 0)
		return (1);
	left = right = NULL;
	larrow = rarrow = 0;

	/* Store current time. */
	t = time(NULL);

	/* Set up default colour. */
	style_apply(&stdgc, s->options, "status-style");

	/* Create the target screen. */
	memcpy(&old_status, &c->status.status, sizeof old_status);
	screen_init(&c->status.status, c->tty.sx, lines, 0);
	screen_write_start(&ctx, NULL, &c->status.status);
	for (offset = 0; offset < lines * c->tty.sx; offset++)
		screen_write_putc(&ctx, &stdgc, ' ');
	screen_write_stop(&ctx);

	/* If the height is too small, blank status line. */
	if (c->tty.sy < lines)
		goto out;

	/* Work out left and right strings. */
	memcpy(&lgc, &stdgc, sizeof lgc);
	left = status_redraw_get_left(c, t, &lgc, &llen);
	memcpy(&rgc, &stdgc, sizeof rgc);
	right = status_redraw_get_right(c, t, &rgc, &rlen);

	/*
	 * Figure out how much space we have for the window list. If there
	 * isn't enough space, just show a blank status line.
	 */
	needed = 0;
	if (llen != 0)
		needed += llen;
	if (rlen != 0)
		needed += rlen;
	if (c->tty.sx == 0 || c->tty.sx <= needed)
		goto out;
	wlavailable = c->tty.sx - needed;

	/* Calculate the total size needed for the window list. */
	wlstart = wloffset = wlwidth = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		free(wl->status_text);
		memcpy(&wl->status_cell, &stdgc, sizeof wl->status_cell);
		wl->status_text = status_print(c, wl, t, &wl->status_cell);
		wl->status_width = screen_write_cstrlen("%s", wl->status_text);

		if (wl == s->curw)
			wloffset = wlwidth;

		oo = wl->window->options;
		sep = options_get_string(oo, "window-status-separator");
		seplen = screen_write_cstrlen("%s", sep);
		wlwidth += wl->status_width + seplen;
	}

	/* Create a new screen for the window list. */
	screen_init(&window_list, wlwidth, 1, 0);

	/* And draw the window list into it. */
	screen_write_start(&ctx, NULL, &window_list);
	RB_FOREACH(wl, winlinks, &s->windows) {
		screen_write_cnputs(&ctx, -1, &wl->status_cell, "%s",
		    wl->status_text);

		oo = wl->window->options;
		sep = options_get_string(oo, "window-status-separator");
		screen_write_cnputs(&ctx, -1, &stdgc, "%s", sep);
	}
	screen_write_stop(&ctx);

	/* If there is enough space for the total width, skip to draw now. */
	if (wlwidth <= wlavailable)
		goto draw;

	/* Find size of current window text. */
	wlsize = s->curw->status_width;

	/*
	 * If the current window is already on screen, good to draw from the
	 * start and just leave off the end.
	 */
	if (wloffset + wlsize < wlavailable) {
		if (wlavailable > 0) {
			rarrow = 1;
			wlavailable--;
		}
		wlwidth = wlavailable;
	} else {
		/*
		 * Work out how many characters we need to omit from the
		 * start. There are wlavailable characters to fill, and
		 * wloffset + wlsize must be the last. So, the start character
		 * is wloffset + wlsize - wlavailable.
		 */
		if (wlavailable > 0) {
			larrow = 1;
			wlavailable--;
		}

		wlstart = wloffset + wlsize - wlavailable;
		if (wlavailable > 0 && wlwidth > wlstart + wlavailable + 1) {
			rarrow = 1;
			wlstart++;
			wlavailable--;
		}
		wlwidth = wlavailable;
	}

	/* Bail if anything is now too small too. */
	if (wlwidth == 0 || wlavailable == 0) {
		screen_free(&window_list);
		goto out;
	}

	/*
	 * Now the start position is known, work out the state of the left and
	 * right arrows.
	 */
	offset = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		if (wl->flags & WINLINK_ALERTFLAGS &&
		    larrow == 1 && offset < wlstart)
			larrow = -1;

		offset += wl->status_width;

		if (wl->flags & WINLINK_ALERTFLAGS &&
		    rarrow == 1 && offset > wlstart + wlwidth)
			rarrow = -1;
	}

draw:
	/* Begin drawing. */
	screen_write_start(&ctx, NULL, &c->status.status);

	/* Draw the left string and arrow. */
	screen_write_cursormove(&ctx, 0, 0);
	if (llen != 0)
		screen_write_cnputs(&ctx, llen, &lgc, "%s", left);
	if (larrow != 0) {
		memcpy(&gc, &stdgc, sizeof gc);
		if (larrow == -1)
			gc.attr ^= GRID_ATTR_REVERSE;
		screen_write_putc(&ctx, &gc, '<');
	}

	/* Draw the right string and arrow. */
	if (rarrow != 0) {
		screen_write_cursormove(&ctx, c->tty.sx - rlen - 1, 0);
		memcpy(&gc, &stdgc, sizeof gc);
		if (rarrow == -1)
			gc.attr ^= GRID_ATTR_REVERSE;
		screen_write_putc(&ctx, &gc, '>');
	} else
		screen_write_cursormove(&ctx, c->tty.sx - rlen, 0);
	if (rlen != 0)
		screen_write_cnputs(&ctx, rlen, &rgc, "%s", right);

	/* Figure out the offset for the window list. */
	if (llen != 0)
		wloffset = llen;
	else
		wloffset = 0;
	if (wlwidth < wlavailable) {
		switch (options_get_number(s->options, "status-justify")) {
		case 1:	/* centred */
			wloffset += (wlavailable - wlwidth) / 2;
			break;
		case 2:	/* right */
			wloffset += (wlavailable - wlwidth);
			break;
		}
	}
	if (larrow != 0)
		wloffset++;

	/* Copy the window list. */
	c->wlmouse = -wloffset + wlstart;
	screen_write_cursormove(&ctx, wloffset, 0);
	screen_write_fast_copy(&ctx, &window_list, wlstart, 0, wlwidth, 1);
	screen_free(&window_list);

	screen_write_stop(&ctx);

out:
	free(left);
	free(right);

	if (grid_compare(c->status.status.grid, old_status.grid) == 0) {
		screen_free(&old_status);
		return (0);
	}
	screen_free(&old_status);
	return (1);
}

/* Replace special sequences in fmt. */
static char *
status_replace(struct client *c, struct winlink *wl, const char *fmt, time_t t)
{
	struct format_tree	*ft;
	char			*expanded;
	u_int			 tag;

	if (fmt == NULL)
		return (xstrdup(""));

	if (wl != NULL)
		tag = FORMAT_WINDOW|wl->window->id;
	else
		tag = FORMAT_NONE;
	if (c->flags & CLIENT_STATUSFORCE)
		ft = format_create(c, NULL, tag, FORMAT_STATUS|FORMAT_FORCE);
	else
		ft = format_create(c, NULL, tag, FORMAT_STATUS);
	format_defaults(ft, c, NULL, wl, NULL);

	expanded = format_expand_time(ft, fmt, t);

	format_free(ft);
	return (expanded);
}

/* Return winlink status line entry and adjust gc as necessary. */
static char *
status_print(struct client *c, struct winlink *wl, time_t t,
    struct grid_cell *gc)
{
	struct options	*oo = wl->window->options;
	struct session	*s = c->session;
	const char	*fmt;
	char   		*text;

	style_apply_update(gc, oo, "window-status-style");
	fmt = options_get_string(oo, "window-status-format");
	if (wl == s->curw) {
		style_apply_update(gc, oo, "window-status-current-style");
		fmt = options_get_string(oo, "window-status-current-format");
	}
	if (wl == TAILQ_FIRST(&s->lastw))
		style_apply_update(gc, oo, "window-status-last-style");

	if (wl->flags & WINLINK_BELL)
		style_apply_update(gc, oo, "window-status-bell-style");
	else if (wl->flags & (WINLINK_ACTIVITY|WINLINK_SILENCE))
		style_apply_update(gc, oo, "window-status-activity-style");

	text = status_replace(c, wl, fmt, t);
	return (text);
}

/* Set a status line message. */
void
status_message_set(struct client *c, const char *fmt, ...)
{
	struct timeval	tv;
	va_list		ap;
	int		delay;

	status_message_clear(c);

	if (c->status.old_status == NULL) {
		c->status.old_status = xmalloc(sizeof *c->status.old_status);
		memcpy(c->status.old_status, &c->status.status,
		    sizeof *c->status.old_status);
		screen_init(&c->status.status, c->tty.sx, 1, 0);
	}

	va_start(ap, fmt);
	xvasprintf(&c->message_string, fmt, ap);
	va_end(ap);

	server_client_add_message(c, "%s", c->message_string);

	delay = options_get_number(c->session->options, "display-time");
	if (delay > 0) {
		tv.tv_sec = delay / 1000;
		tv.tv_usec = (delay % 1000) * 1000L;

		if (event_initialized(&c->message_timer))
			evtimer_del(&c->message_timer);
		evtimer_set(&c->message_timer, status_message_callback, c);
		evtimer_add(&c->message_timer, &tv);
	}

	c->tty.flags |= (TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_STATUS;
}

/* Clear status line message. */
void
status_message_clear(struct client *c)
{
	if (c->message_string == NULL)
		return;

	free(c->message_string);
	c->message_string = NULL;

	if (c->prompt_string == NULL)
		c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW; /* screen was frozen and may have changed */

	screen_reinit(&c->status.status);
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
	struct screen_write_ctx		ctx;
	struct session		       *s = c->session;
	struct screen		        old_status;
	size_t			        len;
	struct grid_cell		gc;
	u_int				lines, offset;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_status, &c->status.status, sizeof old_status);

	lines = status_line_size(c->session);
	if (lines <= 1) {
		lines = 1;
		screen_init(&c->status.status, c->tty.sx, 1, 0);
	} else
		screen_init(&c->status.status, c->tty.sx, lines, 0);

	len = screen_write_strlen("%s", c->message_string);
	if (len > c->tty.sx)
		len = c->tty.sx;

	style_apply(&gc, s->options, "message-style");

	screen_write_start(&ctx, NULL, &c->status.status);
	screen_write_cursormove(&ctx, 0, 0);
	for (offset = 0; offset < lines * c->tty.sx; offset++)
		screen_write_putc(&ctx, &gc, ' ');
	screen_write_cursormove(&ctx, 0, lines - 1);
	screen_write_nputs(&ctx, len, &gc, "%s", c->message_string);
	screen_write_stop(&ctx);

	if (grid_compare(c->status.status.grid, old_status.grid) == 0) {
		screen_free(&old_status);
		return (0);
	}
	screen_free(&old_status);
	return (1);
}

/* Enable status line prompt. */
void
status_prompt_set(struct client *c, const char *msg, const char *input,
    prompt_input_cb inputcb, prompt_free_cb freecb, void *data, int flags)
{
	struct format_tree	*ft;
	time_t			 t;
	char			*tmp, *cp;

	ft = format_create(c, NULL, FORMAT_NONE, 0);
	format_defaults(ft, c, NULL, NULL, NULL);
	t = time(NULL);

	if (input == NULL)
		input = "";
	if (flags & PROMPT_NOFORMAT)
		tmp = xstrdup(input);
	else
		tmp = format_expand_time(ft, input, t);

	status_message_clear(c);
	status_prompt_clear(c);

	if (c->status.old_status == NULL) {
		c->status.old_status = xmalloc(sizeof *c->status.old_status);
		memcpy(c->status.old_status, &c->status.status,
		    sizeof *c->status.old_status);
		screen_init(&c->status.status, c->tty.sx, 1, 0);
	}

	c->prompt_string = format_expand_time(ft, msg, t);

	c->prompt_buffer = utf8_fromcstr(tmp);
	c->prompt_index = utf8_strlen(c->prompt_buffer);

	c->prompt_inputcb = inputcb;
	c->prompt_freecb = freecb;
	c->prompt_data = data;

	c->prompt_hindex = 0;

	c->prompt_flags = flags;
	c->prompt_mode = PROMPT_ENTRY;

	if (~flags & PROMPT_INCREMENTAL)
		c->tty.flags |= (TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_STATUS;

	if ((flags & PROMPT_INCREMENTAL) && *tmp != '\0') {
		xasprintf(&cp, "=%s", tmp);
		c->prompt_inputcb(c, c->prompt_data, cp, 0);
		free(cp);
	}

	free(tmp);
	format_free(ft);
}

/* Remove status line prompt. */
void
status_prompt_clear(struct client *c)
{
	if (c->prompt_string == NULL)
		return;

	if (c->prompt_freecb != NULL && c->prompt_data != NULL)
		c->prompt_freecb(c->prompt_data);

	free(c->prompt_string);
	c->prompt_string = NULL;

	free(c->prompt_buffer);
	c->prompt_buffer = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW; /* screen was frozen and may have changed */

	screen_reinit(&c->status.status);
}

/* Update status line prompt with a new prompt string. */
void
status_prompt_update(struct client *c, const char *msg, const char *input)
{
	struct format_tree	*ft;
	time_t			 t;
	char			*tmp;

	ft = format_create(c, NULL, FORMAT_NONE, 0);
	format_defaults(ft, c, NULL, NULL, NULL);

	t = time(NULL);
	tmp = format_expand_time(ft, input, t);

	free(c->prompt_string);
	c->prompt_string = format_expand_time(ft, msg, t);

	free(c->prompt_buffer);
	c->prompt_buffer = utf8_fromcstr(tmp);
	c->prompt_index = utf8_strlen(c->prompt_buffer);

	c->prompt_hindex = 0;

	c->flags |= CLIENT_STATUS;

	free(tmp);
	format_free(ft);
}

/* Draw client prompt on status line of present else on last line. */
int
status_prompt_redraw(struct client *c)
{
	struct screen_write_ctx	 ctx;
	struct session		*s = c->session;
	struct screen		 old_status;
	u_int			 i, offset, left, start, pcursor, pwidth, width;
	u_int			 lines;
	struct grid_cell	 gc, cursorgc;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_status, &c->status.status, sizeof old_status);

	lines = status_line_size(c->session);
	if (lines <= 1) {
		lines = 1;
		screen_init(&c->status.status, c->tty.sx, 1, 0);
	} else
		screen_init(&c->status.status, c->tty.sx, lines, 0);

	if (c->prompt_mode == PROMPT_COMMAND)
		style_apply(&gc, s->options, "message-command-style");
	else
		style_apply(&gc, s->options, "message-style");

	memcpy(&cursorgc, &gc, sizeof cursorgc);
	cursorgc.attr ^= GRID_ATTR_REVERSE;

	start = screen_write_strlen("%s", c->prompt_string);
	if (start > c->tty.sx)
		start = c->tty.sx;

	screen_write_start(&ctx, NULL, &c->status.status);
	screen_write_cursormove(&ctx, 0, 0);
	for (offset = 0; offset < lines * c->tty.sx; offset++)
		screen_write_putc(&ctx, &gc, ' ');
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_nputs(&ctx, start, &gc, "%s", c->prompt_string);
	screen_write_cursormove(&ctx, start, 0);

	left = c->tty.sx - start;
	if (left == 0)
		goto finished;

	pcursor = utf8_strwidth(c->prompt_buffer, c->prompt_index);
	pwidth = utf8_strwidth(c->prompt_buffer, -1);
	if (pcursor >= left) {
		/*
		 * The cursor would be outside the screen so start drawing
		 * with it on the right.
		 */
		offset = (pcursor - left) + 1;
		pwidth = left;
	} else
		offset = 0;
	if (pwidth > left)
		pwidth = left;

	width = 0;
	for (i = 0; c->prompt_buffer[i].size != 0; i++) {
		if (width < offset) {
			width += c->prompt_buffer[i].width;
			continue;
		}
		if (width >= offset + pwidth)
			break;
		width += c->prompt_buffer[i].width;
		if (width > offset + pwidth)
			break;

		if (i != c->prompt_index) {
			utf8_copy(&gc.data, &c->prompt_buffer[i]);
			screen_write_cell(&ctx, &gc);
		} else {
			utf8_copy(&cursorgc.data, &c->prompt_buffer[i]);
			screen_write_cell(&ctx, &cursorgc);
		}
	}
	if (c->status.status.cx < screen_size_x(&c->status.status) &&
	    c->prompt_index >= i)
		screen_write_putc(&ctx, &cursorgc, ' ');

finished:
	screen_write_stop(&ctx);

	if (grid_compare(c->status.status.grid, old_status.grid) == 0) {
		screen_free(&old_status);
		return (0);
	}
	screen_free(&old_status);
	return (1);
}

/* Is this a separator? */
static int
status_prompt_in_list(const char *ws, const struct utf8_data *ud)
{
	if (ud->size != 1 || ud->width != 1)
		return (0);
	return (strchr(ws, *ud->data) != NULL);
}

/* Is this a space? */
static int
status_prompt_space(const struct utf8_data *ud)
{
	if (ud->size != 1 || ud->width != 1)
		return (0);
	return (*ud->data == ' ');
}

/*
 * Translate key from emacs to vi. Return 0 to drop key, 1 to process the key
 * as an emacs key; return 2 to append to the buffer.
 */
static int
status_prompt_translate_key(struct client *c, key_code key, key_code *new_key)
{
	if (c->prompt_mode == PROMPT_ENTRY) {
		switch (key) {
		case '\003': /* C-c */
		case '\010': /* C-h */
		case '\011': /* Tab */
		case '\025': /* C-u */
		case '\027': /* C-w */
		case '\n':
		case '\r':
		case KEYC_BSPACE:
		case KEYC_DC:
		case KEYC_DOWN:
		case KEYC_END:
		case KEYC_HOME:
		case KEYC_LEFT:
		case KEYC_RIGHT:
		case KEYC_UP:
			*new_key = key;
			return (1);
		case '\033': /* Escape */
			c->prompt_mode = PROMPT_COMMAND;
			c->flags |= CLIENT_STATUS;
			return (0);
		}
		*new_key = key;
		return (2);
	}

	switch (key) {
	case 'A':
	case 'I':
	case 'C':
	case 's':
	case 'a':
		c->prompt_mode = PROMPT_ENTRY;
		c->flags |= CLIENT_STATUS;
		break; /* switch mode and... */
	case 'S':
		c->prompt_mode = PROMPT_ENTRY;
		c->flags |= CLIENT_STATUS;
		*new_key = '\025'; /* C-u */
		return (1);
	case 'i':
	case '\033': /* Escape */
		c->prompt_mode = PROMPT_ENTRY;
		c->flags |= CLIENT_STATUS;
		return (0);
	}

	switch (key) {
	case 'A':
	case '$':
		*new_key = KEYC_END;
		return (1);
	case 'I':
	case '0':
	case '^':
		*new_key = KEYC_HOME;
		return (1);
	case 'C':
	case 'D':
		*new_key = '\013'; /* C-k */
		return (1);
	case KEYC_BSPACE:
	case 'X':
		*new_key = KEYC_BSPACE;
		return (1);
	case 'b':
	case 'B':
		*new_key = 'b'|KEYC_ESCAPE;
		return (1);
	case 'd':
		*new_key = '\025';
		return (1);
	case 'e':
	case 'E':
	case 'w':
	case 'W':
		*new_key = 'f'|KEYC_ESCAPE;
		return (1);
	case 'p':
		*new_key = '\031'; /* C-y */
		return (1);
	case 's':
	case KEYC_DC:
	case 'x':
		*new_key = KEYC_DC;
		return (1);
	case KEYC_DOWN:
	case 'j':
		*new_key = KEYC_DOWN;
		return (1);
	case KEYC_LEFT:
	case 'h':
		*new_key = KEYC_LEFT;
		return (1);
	case 'a':
	case KEYC_RIGHT:
	case 'l':
		*new_key = KEYC_RIGHT;
		return (1);
	case KEYC_UP:
	case 'k':
		*new_key = KEYC_UP;
		return (1);
	case '\010' /* C-h */:
	case '\003' /* C-c */:
	case '\n':
	case '\r':
		return (1);
	}
	return (0);
}

/* Handle keys in prompt. */
int
status_prompt_key(struct client *c, key_code key)
{
	struct options		*oo = c->session->options;
	struct paste_buffer	*pb;
	char			*s, *cp, word[64], prefix = '=';
	const char		*histstr, *bufdata, *ws = NULL;
	u_char			 ch;
	size_t			 size, n, off, idx, bufsize, used;
	struct utf8_data	 tmp, *first, *last, *ud;
	int			 keys;

	size = utf8_strlen(c->prompt_buffer);

	if (c->prompt_flags & PROMPT_NUMERIC) {
		if (key >= '0' && key <= '9')
			goto append_key;
		s = utf8_tocstr(c->prompt_buffer);
		c->prompt_inputcb(c, c->prompt_data, s, 1);
		status_prompt_clear(c);
		free(s);
		return (1);
	}

	keys = options_get_number(c->session->options, "status-keys");
	if (keys == MODEKEY_VI) {
		switch (status_prompt_translate_key(c, key, &key)) {
		case 1:
			goto process_key;
		case 2:
			goto append_key;
		default:
			return (0);
		}
	}

process_key:
	switch (key) {
	case KEYC_LEFT:
	case '\002': /* C-b */
		if (c->prompt_index > 0) {
			c->prompt_index--;
			break;
		}
		break;
	case KEYC_RIGHT:
	case '\006': /* C-f */
		if (c->prompt_index < size) {
			c->prompt_index++;
			break;
		}
		break;
	case KEYC_HOME:
	case '\001': /* C-a */
		if (c->prompt_index != 0) {
			c->prompt_index = 0;
			break;
		}
		break;
	case KEYC_END:
	case '\005': /* C-e */
		if (c->prompt_index != size) {
			c->prompt_index = size;
			break;
		}
		break;
	case '\011': /* Tab */
		if (c->prompt_buffer[0].size == 0)
			break;

		idx = c->prompt_index;
		if (idx != 0)
			idx--;

		/* Find the word we are in. */
		first = &c->prompt_buffer[idx];
		while (first > c->prompt_buffer && !status_prompt_space(first))
			first--;
		while (first->size != 0 && status_prompt_space(first))
			first++;
		last = &c->prompt_buffer[idx];
		while (last->size != 0 && !status_prompt_space(last))
			last++;
		while (last > c->prompt_buffer && status_prompt_space(last))
			last--;
		if (last->size != 0)
			last++;
		if (last <= first)
			break;

		used = 0;
		for (ud = first; ud < last; ud++) {
			if (used + ud->size >= sizeof word)
				break;
			memcpy(word + used, ud->data, ud->size);
			used += ud->size;
		}
		if (ud != last)
			break;
		word[used] = '\0';

		/* And try to complete it. */
		if ((s = status_prompt_complete(c->session, word)) == NULL)
			break;

		/* Trim out word. */
		n = size - (last - c->prompt_buffer) + 1; /* with \0 */
		memmove(first, last, n * sizeof *c->prompt_buffer);
		size -= last - first;

		/* Insert the new word. */
		size += strlen(s);
		off = first - c->prompt_buffer;
		c->prompt_buffer = xreallocarray(c->prompt_buffer, size + 1,
		    sizeof *c->prompt_buffer);
		first = c->prompt_buffer + off;
		memmove(first + strlen(s), first, n * sizeof *c->prompt_buffer);
		for (idx = 0; idx < strlen(s); idx++)
			utf8_set(&first[idx], s[idx]);

		c->prompt_index = (first - c->prompt_buffer) + strlen(s);
		free(s);

		goto changed;
	case KEYC_BSPACE:
	case '\010': /* C-h */
		if (c->prompt_index != 0) {
			if (c->prompt_index == size)
				c->prompt_buffer[--c->prompt_index].size = 0;
			else {
				memmove(c->prompt_buffer + c->prompt_index - 1,
				    c->prompt_buffer + c->prompt_index,
				    (size + 1 - c->prompt_index) *
				    sizeof *c->prompt_buffer);
				c->prompt_index--;
			}
			goto changed;
		}
		break;
	case KEYC_DC:
	case '\004': /* C-d */
		if (c->prompt_index != size) {
			memmove(c->prompt_buffer + c->prompt_index,
			    c->prompt_buffer + c->prompt_index + 1,
			    (size + 1 - c->prompt_index) *
			    sizeof *c->prompt_buffer);
			goto changed;
		}
		break;
	case '\025': /* C-u */
		c->prompt_buffer[0].size = 0;
		c->prompt_index = 0;
		goto changed;
	case '\013': /* C-k */
		if (c->prompt_index < size) {
			c->prompt_buffer[c->prompt_index].size = 0;
			goto changed;
		}
		break;
	case '\027': /* C-w */
		ws = options_get_string(oo, "word-separators");
		idx = c->prompt_index;

		/* Find a non-separator. */
		while (idx != 0) {
			idx--;
			if (!status_prompt_in_list(ws, &c->prompt_buffer[idx]))
				break;
		}

		/* Find the separator at the beginning of the word. */
		while (idx != 0) {
			idx--;
			if (status_prompt_in_list(ws, &c->prompt_buffer[idx])) {
				/* Go back to the word. */
				idx++;
				break;
			}
		}

		memmove(c->prompt_buffer + idx,
		    c->prompt_buffer + c->prompt_index,
		    (size + 1 - c->prompt_index) *
		    sizeof *c->prompt_buffer);
		memset(c->prompt_buffer + size - (c->prompt_index - idx),
		    '\0', (c->prompt_index - idx) * sizeof *c->prompt_buffer);
		c->prompt_index = idx;

		goto changed;
	case 'f'|KEYC_ESCAPE:
		ws = options_get_string(oo, "word-separators");

		/* Find a word. */
		while (c->prompt_index != size) {
			idx = ++c->prompt_index;
			if (!status_prompt_in_list(ws, &c->prompt_buffer[idx]))
				break;
		}

		/* Find the separator at the end of the word. */
		while (c->prompt_index != size) {
			idx = ++c->prompt_index;
			if (status_prompt_in_list(ws, &c->prompt_buffer[idx]))
				break;
		}

		/* Back up to the end-of-word like vi. */
		if (options_get_number(oo, "status-keys") == MODEKEY_VI &&
		    c->prompt_index != 0)
			c->prompt_index--;

		goto changed;
	case 'b'|KEYC_ESCAPE:
		ws = options_get_string(oo, "word-separators");

		/* Find a non-separator. */
		while (c->prompt_index != 0) {
			idx = --c->prompt_index;
			if (!status_prompt_in_list(ws, &c->prompt_buffer[idx]))
				break;
		}

		/* Find the separator at the beginning of the word. */
		while (c->prompt_index != 0) {
			idx = --c->prompt_index;
			if (status_prompt_in_list(ws, &c->prompt_buffer[idx])) {
				/* Go back to the word. */
				c->prompt_index++;
				break;
			}
		}
		goto changed;
	case KEYC_UP:
	case '\020': /* C-p */
		histstr = status_prompt_up_history(&c->prompt_hindex);
		if (histstr == NULL)
			break;
		free(c->prompt_buffer);
		c->prompt_buffer = utf8_fromcstr(histstr);
		c->prompt_index = utf8_strlen(c->prompt_buffer);
		goto changed;
	case KEYC_DOWN:
	case '\016': /* C-n */
		histstr = status_prompt_down_history(&c->prompt_hindex);
		if (histstr == NULL)
			break;
		free(c->prompt_buffer);
		c->prompt_buffer = utf8_fromcstr(histstr);
		c->prompt_index = utf8_strlen(c->prompt_buffer);
		goto changed;
	case '\031': /* C-y */
		if ((pb = paste_get_top(NULL)) == NULL)
			break;
		bufdata = paste_buffer_data(pb, &bufsize);
		for (n = 0; n < bufsize; n++) {
			ch = (u_char)bufdata[n];
			if (ch < 32 || ch >= 127)
				break;
		}

		c->prompt_buffer = xreallocarray(c->prompt_buffer, size + n + 1,
		    sizeof *c->prompt_buffer);
		if (c->prompt_index == size) {
			for (idx = 0; idx < n; idx++) {
				ud = &c->prompt_buffer[c->prompt_index + idx];
				utf8_set(ud, bufdata[idx]);
			}
			c->prompt_index += n;
			c->prompt_buffer[c->prompt_index].size = 0;
		} else {
			memmove(c->prompt_buffer + c->prompt_index + n,
			    c->prompt_buffer + c->prompt_index,
			    (size + 1 - c->prompt_index) *
			    sizeof *c->prompt_buffer);
			for (idx = 0; idx < n; idx++) {
				ud = &c->prompt_buffer[c->prompt_index + idx];
				utf8_set(ud, bufdata[idx]);
			}
			c->prompt_index += n;
		}
		goto changed;
	case '\024': /* C-t */
		idx = c->prompt_index;
		if (idx < size)
			idx++;
		if (idx >= 2) {
			utf8_copy(&tmp, &c->prompt_buffer[idx - 2]);
			utf8_copy(&c->prompt_buffer[idx - 2],
			    &c->prompt_buffer[idx - 1]);
			utf8_copy(&c->prompt_buffer[idx - 1], &tmp);
			c->prompt_index = idx;
			goto changed;
		}
		break;
	case '\r':
	case '\n':
		s = utf8_tocstr(c->prompt_buffer);
		if (*s != '\0')
			status_prompt_add_history(s);
		if (c->prompt_inputcb(c, c->prompt_data, s, 1) == 0)
			status_prompt_clear(c);
		free(s);
		break;
	case '\033': /* Escape */
	case '\003': /* C-c */
	case '\007': /* C-g */
		if (c->prompt_inputcb(c, c->prompt_data, NULL, 1) == 0)
			status_prompt_clear(c);
		break;
	case '\022': /* C-r */
		if (c->prompt_flags & PROMPT_INCREMENTAL) {
			prefix = '-';
			goto changed;
		}
		break;
	case '\023': /* C-s */
		if (c->prompt_flags & PROMPT_INCREMENTAL) {
			prefix = '+';
			goto changed;
		}
		break;
	default:
		goto append_key;
	}

	c->flags |= CLIENT_STATUS;
	return (0);

append_key:
	if (key <= 0x1f || key >= KEYC_BASE)
		return (0);
	if (utf8_split(key, &tmp) != UTF8_DONE)
		return (0);

	c->prompt_buffer = xreallocarray(c->prompt_buffer, size + 2,
	    sizeof *c->prompt_buffer);

	if (c->prompt_index == size) {
		utf8_copy(&c->prompt_buffer[c->prompt_index], &tmp);
		c->prompt_index++;
		c->prompt_buffer[c->prompt_index].size = 0;
	} else {
		memmove(c->prompt_buffer + c->prompt_index + 1,
		    c->prompt_buffer + c->prompt_index,
		    (size + 1 - c->prompt_index) *
		    sizeof *c->prompt_buffer);
		utf8_copy(&c->prompt_buffer[c->prompt_index], &tmp);
		c->prompt_index++;
	}

	if (c->prompt_flags & PROMPT_SINGLE) {
		s = utf8_tocstr(c->prompt_buffer);
		if (strlen(s) != 1)
			status_prompt_clear(c);
		else if (c->prompt_inputcb(c, c->prompt_data, s, 1) == 0)
			status_prompt_clear(c);
		free(s);
	}

changed:
	c->flags |= CLIENT_STATUS;
	if (c->prompt_flags & PROMPT_INCREMENTAL) {
		s = utf8_tocstr(c->prompt_buffer);
		xasprintf(&cp, "%c%s", prefix, s);
		c->prompt_inputcb(c, c->prompt_data, cp, 0);
		free(cp);
		free(s);
	}
	return (0);
}

/* Get previous line from the history. */
static const char *
status_prompt_up_history(u_int *idx)
{
	/*
	 * History runs from 0 to size - 1. Index is from 0 to size. Zero is
	 * empty.
	 */

	if (status_prompt_hsize == 0 || *idx == status_prompt_hsize)
		return (NULL);
	(*idx)++;
	return (status_prompt_hlist[status_prompt_hsize - *idx]);
}

/* Get next line from the history. */
static const char *
status_prompt_down_history(u_int *idx)
{
	if (status_prompt_hsize == 0 || *idx == 0)
		return ("");
	(*idx)--;
	if (*idx == 0)
		return ("");
	return (status_prompt_hlist[status_prompt_hsize - *idx]);
}

/* Add line to the history. */
static void
status_prompt_add_history(const char *line)
{
	size_t	size;

	if (status_prompt_hsize > 0 &&
	    strcmp(status_prompt_hlist[status_prompt_hsize - 1], line) == 0)
		return;

	if (status_prompt_hsize == PROMPT_HISTORY) {
		free(status_prompt_hlist[0]);

		size = (PROMPT_HISTORY - 1) * sizeof *status_prompt_hlist;
		memmove(&status_prompt_hlist[0], &status_prompt_hlist[1], size);

		status_prompt_hlist[status_prompt_hsize - 1] = xstrdup(line);
		return;
	}

	status_prompt_hlist = xreallocarray(status_prompt_hlist,
	    status_prompt_hsize + 1, sizeof *status_prompt_hlist);
	status_prompt_hlist[status_prompt_hsize++] = xstrdup(line);
}

/* Build completion list. */
static const char **
status_prompt_complete_list(u_int *size, const char *s)
{
	const char				**list = NULL, **layout;
	const struct cmd_entry			**cmdent;
	const struct options_table_entry	 *oe;
	const char				 *layouts[] = {
		"even-horizontal", "even-vertical", "main-horizontal",
		"main-vertical", "tiled", NULL
	};

	*size = 0;
	for (cmdent = cmd_table; *cmdent != NULL; cmdent++) {
		if (strncmp((*cmdent)->name, s, strlen(s)) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = (*cmdent)->name;
		}
	}
	for (oe = options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, strlen(s)) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = oe->name;
		}
	}
	for (layout = layouts; *layout != NULL; layout++) {
		if (strncmp(*layout, s, strlen(s)) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = *layout;
		}
	}
	return (list);
}

/* Find longest prefix. */
static char *
status_prompt_complete_prefix(const char **list, u_int size)
{
	char	 *out;
	u_int	  i;
	size_t	  j;

	out = xstrdup(list[0]);
	for (i = 1; i < size; i++) {
		j = strlen(list[i]);
		if (j > strlen(out))
			j = strlen(out);
		for (; j > 0; j--) {
			if (out[j - 1] != list[i][j - 1])
				out[j - 1] = '\0';
		}
	}
	return (out);
}

/* Complete word. */
static char *
status_prompt_complete(struct session *session, const char *s)
{
	const char	**list = NULL, *colon;
	u_int		  size = 0, i;
	struct session	 *s_loop;
	struct winlink	 *wl;
	struct window	 *w;
	char		 *copy, *out, *tmp;

	if (*s == '\0')
		return (NULL);
	out = NULL;

	if (strncmp(s, "-t", 2) != 0 && strncmp(s, "-s", 2) != 0) {
		list = status_prompt_complete_list(&size, s);
		if (size == 0)
			out = NULL;
		else if (size == 1)
			xasprintf(&out, "%s ", list[0]);
		else
			out = status_prompt_complete_prefix(list, size);
		free(list);
		return (out);
	}
	copy = xstrdup(s);

	colon = ":";
	if (copy[strlen(copy) - 1] == ':')
		copy[strlen(copy) - 1] = '\0';
	else
		colon = "";
	s = copy + 2;

	RB_FOREACH(s_loop, sessions, &sessions) {
		if (strncmp(s_loop->name, s, strlen(s)) == 0) {
			list = xreallocarray(list, size + 2, sizeof *list);
			list[size++] = s_loop->name;
		}
	}
	if (size == 1) {
		out = xstrdup(list[0]);
		if (session_find(list[0]) != NULL)
			colon = ":";
	} else if (size != 0)
		out = status_prompt_complete_prefix(list, size);
	if (out != NULL) {
		xasprintf(&tmp, "-%c%s%s", copy[1], out, colon);
		free(out);
		out = tmp;
		goto found;
	}

	colon = "";
	if (*s == ':') {
		RB_FOREACH(wl, winlinks, &session->windows) {
			xasprintf(&tmp, ":%s", wl->window->name);
			if (strncmp(tmp, s, strlen(s)) == 0){
				list = xreallocarray(list, size + 1,
				    sizeof *list);
				list[size++] = tmp;
				continue;
			}
			free(tmp);

			xasprintf(&tmp, ":%d", wl->idx);
			if (strncmp(tmp, s, strlen(s)) == 0) {
				list = xreallocarray(list, size + 1,
				    sizeof *list);
				list[size++] = tmp;
				continue;
			}
			free(tmp);
		}
	} else {
		RB_FOREACH(s_loop, sessions, &sessions) {
			RB_FOREACH(wl, winlinks, &s_loop->windows) {
				w = wl->window;

				xasprintf(&tmp, "%s:%s", s_loop->name, w->name);
				if (strncmp(tmp, s, strlen(s)) == 0) {
					list = xreallocarray(list, size + 1,
					    sizeof *list);
					list[size++] = tmp;
					continue;
				}
				free(tmp);

				xasprintf(&tmp, "%s:%d", s_loop->name, wl->idx);
				if (strncmp(tmp, s, strlen(s)) == 0) {
					list = xreallocarray(list, size + 1,
					    sizeof *list);
					list[size++] = tmp;
					continue;
				}
				free(tmp);
			}
		}
	}
	if (size == 1) {
		out = xstrdup(list[0]);
		colon = " ";
	} else if (size != 0)
		out = status_prompt_complete_prefix(list, size);
	if (out != NULL) {
		xasprintf(&tmp, "-%c%s%s", copy[1], out, colon);
		out = tmp;
	}

	for (i = 0; i < size; i++)
		free((void *)list[i]);

found:
	free(copy);
	free(list);
	return (out);
}
