/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

char   *status_redraw_get_left(struct client *, time_t, int, struct grid_cell *,
	    size_t *);
char   *status_redraw_get_right(struct client *, time_t, int,
	    struct grid_cell *, size_t *);
char   *status_print(struct client *, struct winlink *, time_t,
	    struct grid_cell *);
char   *status_replace(struct client *, struct winlink *, const char *, time_t);
void	status_message_callback(int, short, void *);
void	status_timer_callback(int, short, void *);

const char *status_prompt_up_history(u_int *);
const char *status_prompt_down_history(u_int *);
void	status_prompt_add_history(const char *);

const char **status_prompt_complete_list(u_int *, const char *);
char   *status_prompt_complete_prefix(const char **, u_int);
char   *status_prompt_complete(struct session *, const char *);

char   *status_prompt_find_history_file(void);

/* Status prompt history. */
#define PROMPT_HISTORY 100
char	**status_prompt_hlist;
u_int	  status_prompt_hsize;

/* Find the history file to load/save from/to. */
char *
status_prompt_find_history_file(void)
{
	const char	*home, *history_file;
	char		*path;

	history_file = options_get_string(&global_options, "history-file");
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
void
status_timer_callback(unused int fd, unused short events, void *arg)
{
	struct client	*c = arg;
	struct session	*s = c->session;
	struct timeval	 tv;

	evtimer_del(&c->status_timer);

	if (s == NULL)
		return;

	if (c->message_string == NULL && c->prompt_string == NULL)
		c->flags |= CLIENT_STATUS;

	timerclear(&tv);
	tv.tv_sec = options_get_number(&s->options, "status-interval");

	if (tv.tv_sec != 0)
		evtimer_add(&c->status_timer, &tv);
	log_debug("client %d, status interval %d", c->ibuf.fd, (int)tv.tv_sec);
}

/* Start status timer for client. */
void
status_timer_start(struct client *c)
{
	struct session	*s = c->session;

	if (event_initialized(&c->status_timer))
		evtimer_del(&c->status_timer);
	else
		evtimer_set(&c->status_timer, status_timer_callback, c);

	if (s != NULL && options_get_number(&s->options, "status"))
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

/* Get screen line of status line. -1 means off. */
int
status_at_line(struct client *c)
{
	struct session	*s = c->session;

	if (!options_get_number(&s->options, "status"))
		return (-1);

	if (options_get_number(&s->options, "status-position") == 0)
		return (0);
	return (c->tty.sy - 1);
}

/* Retrieve options for left string. */
char *
status_redraw_get_left(struct client *c, time_t t, int utf8flag,
    struct grid_cell *gc, size_t *size)
{
	struct session	*s = c->session;
	const char	*template;
	char		*left;
	size_t		 leftlen;

	style_apply_update(gc, &s->options, "status-left-style");

	template = options_get_string(&s->options, "status-left");
	left = status_replace(c, NULL, template, t);

	*size = options_get_number(&s->options, "status-left-length");
	leftlen = screen_write_cstrlen(utf8flag, "%s", left);
	if (leftlen < *size)
		*size = leftlen;
	return (left);
}

/* Retrieve options for right string. */
char *
status_redraw_get_right(struct client *c, time_t t, int utf8flag,
    struct grid_cell *gc, size_t *size)
{
	struct session	*s = c->session;
	const char	*template;
	char		*right;
	size_t		 rightlen;

	style_apply_update(gc, &s->options, "status-right-style");

	template = options_get_string(&s->options, "status-right");
	right = status_replace(c, NULL, template, t);

	*size = options_get_number(&s->options, "status-right-length");
	rightlen = screen_write_cstrlen(utf8flag, "%s", right);
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
	size_t		 len;

	x += c->wlmouse;
	RB_FOREACH(wl, winlinks, &s->windows) {
		oo = &wl->window->options;
		len = strlen(options_get_string(oo, "window-status-separator"));

		if (x < wl->status_width)
			return (wl->window);
		x -= wl->status_width + len;
	}
	return (NULL);
}

/* Draw status for client on the last lines of given context. */
int
status_redraw(struct client *c)
{
	struct screen_write_ctx	ctx;
	struct session	       *s = c->session;
	struct winlink	       *wl;
	struct screen		old_status, window_list;
	struct grid_cell	stdgc, lgc, rgc, gc;
	struct options	       *oo;
	time_t			t;
	char		       *left, *right, *sep;
	u_int			offset, needed;
	u_int			wlstart, wlwidth, wlavailable, wloffset, wlsize;
	size_t			llen, rlen, seplen;
	int			larrow, rarrow, utf8flag;

	/* No status line? */
	if (c->tty.sy == 0 || !options_get_number(&s->options, "status"))
		return (1);
	left = right = NULL;
	larrow = rarrow = 0;

	/* Store current time. */
	t = time(NULL);

	/* Set up default colour. */
	style_apply(&stdgc, &s->options, "status-style");

	/* Create the target screen. */
	memcpy(&old_status, &c->status, sizeof old_status);
	screen_init(&c->status, c->tty.sx, 1, 0);
	screen_write_start(&ctx, NULL, &c->status);
	for (offset = 0; offset < c->tty.sx; offset++)
		screen_write_putc(&ctx, &stdgc, ' ');
	screen_write_stop(&ctx);

	/* If the height is one line, blank status line. */
	if (c->tty.sy <= 1)
		goto out;

	/* Get UTF-8 flag. */
	utf8flag = options_get_number(&s->options, "status-utf8");

	/* Work out left and right strings. */
	memcpy(&lgc, &stdgc, sizeof lgc);
	left = status_redraw_get_left(c, t, utf8flag, &lgc, &llen);
	memcpy(&rgc, &stdgc, sizeof rgc);
	right = status_redraw_get_right(c, t, utf8flag, &rgc, &rlen);

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
		wl->status_width =
		    screen_write_cstrlen(utf8flag, "%s", wl->status_text);

		if (wl == s->curw)
			wloffset = wlwidth;

		oo = &wl->window->options;
		sep = options_get_string(oo, "window-status-separator");
		seplen = screen_write_strlen(utf8flag, "%s", sep);
		wlwidth += wl->status_width + seplen;
	}

	/* Create a new screen for the window list. */
	screen_init(&window_list, wlwidth, 1, 0);

	/* And draw the window list into it. */
	screen_write_start(&ctx, NULL, &window_list);
	RB_FOREACH(wl, winlinks, &s->windows) {
		screen_write_cnputs(&ctx,
		    -1, &wl->status_cell, utf8flag, "%s", wl->status_text);

		oo = &wl->window->options;
		sep = options_get_string(oo, "window-status-separator");
		screen_write_nputs(&ctx, -1, &stdgc, utf8flag, "%s", sep);
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
	screen_write_start(&ctx, NULL, &c->status);

	/* Draw the left string and arrow. */
	screen_write_cursormove(&ctx, 0, 0);
	if (llen != 0)
		screen_write_cnputs(&ctx, llen, &lgc, utf8flag, "%s", left);
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
		screen_write_cnputs(&ctx, rlen, &rgc, utf8flag, "%s", right);

	/* Figure out the offset for the window list. */
	if (llen != 0)
		wloffset = llen;
	else
		wloffset = 0;
	if (wlwidth < wlavailable) {
		switch (options_get_number(&s->options, "status-justify")) {
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
	screen_write_copy(&ctx, &window_list, wlstart, 0, wlwidth, 1);
	screen_free(&window_list);

	screen_write_stop(&ctx);

out:
	free(left);
	free(right);

	if (grid_compare(c->status.grid, old_status.grid) == 0) {
		screen_free(&old_status);
		return (0);
	}
	screen_free(&old_status);
	return (1);
}

/* Replace special sequences in fmt. */
char *
status_replace(struct client *c, struct winlink *wl, const char *fmt, time_t t)
{
	struct format_tree	*ft;
	char			*expanded;

	if (fmt == NULL)
		return (xstrdup(""));

	if (c->flags & CLIENT_STATUSFORCE)
		ft = format_create_flags(FORMAT_STATUS|FORMAT_FORCE);
	else
		ft = format_create_flags(FORMAT_STATUS);
	format_defaults(ft, c, NULL, wl, NULL);

	expanded = format_expand_time(ft, fmt, t);

	format_free(ft);
	return (expanded);
}

/* Return winlink status line entry and adjust gc as necessary. */
char *
status_print(struct client *c, struct winlink *wl, time_t t,
    struct grid_cell *gc)
{
	struct options	*oo = &wl->window->options;
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
	struct timeval		 tv;
	struct message_entry	*msg, *msg1;
	va_list			 ap;
	int			 delay;
	u_int			 first, limit;

	limit = options_get_number(&global_options, "message-limit");

	status_prompt_clear(c);
	status_message_clear(c);

	va_start(ap, fmt);
	xvasprintf(&c->message_string, fmt, ap);
	va_end(ap);

	msg = xcalloc(1, sizeof *msg);
	msg->msg_time = time(NULL);
	msg->msg_num = c->message_next++;
	msg->msg = xstrdup(c->message_string);
	TAILQ_INSERT_TAIL(&c->message_log, msg, entry);

	first = c->message_next - limit;
	TAILQ_FOREACH_SAFE(msg, &c->message_log, entry, msg1) {
		if (msg->msg_num >= first)
			continue;
		free(msg->msg);
		TAILQ_REMOVE(&c->message_log, msg, entry);
		free(msg);
	}

	delay = options_get_number(&c->session->options, "display-time");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	if (event_initialized(&c->message_timer))
		evtimer_del(&c->message_timer);
	evtimer_set(&c->message_timer, status_message_callback, c);
	evtimer_add(&c->message_timer, &tv);

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

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW; /* screen was frozen and may have changed */

	screen_reinit(&c->status);
}

/* Clear status line message after timer expires. */
void
status_message_callback(unused int fd, unused short event, void *data)
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
	int				utf8flag;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_status, &c->status, sizeof old_status);
	screen_init(&c->status, c->tty.sx, 1, 0);

	utf8flag = options_get_number(&s->options, "status-utf8");

	len = screen_write_strlen(utf8flag, "%s", c->message_string);
	if (len > c->tty.sx)
		len = c->tty.sx;

	style_apply(&gc, &s->options, "message-style");

	screen_write_start(&ctx, NULL, &c->status);

	screen_write_cursormove(&ctx, 0, 0);
	screen_write_nputs(&ctx, len, &gc, utf8flag, "%s", c->message_string);
	for (; len < c->tty.sx; len++)
		screen_write_putc(&ctx, &gc, ' ');

	screen_write_stop(&ctx);

	if (grid_compare(c->status.grid, old_status.grid) == 0) {
		screen_free(&old_status);
		return (0);
	}
	screen_free(&old_status);
	return (1);
}

/* Enable status line prompt. */
void
status_prompt_set(struct client *c, const char *msg, const char *input,
    int (*callbackfn)(void *, const char *), void (*freefn)(void *),
    void *data, int flags)
{
	struct format_tree	*ft;
	int			 keys;
	time_t			 t;

	ft = format_create();
	format_defaults(ft, c, NULL, NULL, NULL);
	t = time(NULL);

	status_message_clear(c);
	status_prompt_clear(c);

	c->prompt_string = format_expand_time(ft, msg, t);

	c->prompt_buffer = format_expand_time(ft, input, t);
	c->prompt_index = strlen(c->prompt_buffer);

	c->prompt_callbackfn = callbackfn;
	c->prompt_freefn = freefn;
	c->prompt_data = data;

	c->prompt_hindex = 0;

	c->prompt_flags = flags;

	keys = options_get_number(&c->session->options, "status-keys");
	if (keys == MODEKEY_EMACS)
		mode_key_init(&c->prompt_mdata, &mode_key_tree_emacs_edit);
	else
		mode_key_init(&c->prompt_mdata, &mode_key_tree_vi_edit);

	c->tty.flags |= (TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_STATUS;

	format_free(ft);
}

/* Remove status line prompt. */
void
status_prompt_clear(struct client *c)
{
	if (c->prompt_string == NULL)
		return;

	if (c->prompt_freefn != NULL && c->prompt_data != NULL)
		c->prompt_freefn(c->prompt_data);

	free(c->prompt_string);
	c->prompt_string = NULL;

	free(c->prompt_buffer);
	c->prompt_buffer = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW; /* screen was frozen and may have changed */

	screen_reinit(&c->status);
}

/* Update status line prompt with a new prompt string. */
void
status_prompt_update(struct client *c, const char *msg, const char *input)
{
	struct format_tree	*ft;
	time_t			 t;

	ft = format_create();
	format_defaults(ft, c, NULL, NULL, NULL);
	t = time(NULL);

	free(c->prompt_string);
	c->prompt_string = format_expand_time(ft, msg, t);

	free(c->prompt_buffer);
	c->prompt_buffer = format_expand_time(ft, input, t);
	c->prompt_index = strlen(c->prompt_buffer);

	c->prompt_hindex = 0;

	c->flags |= CLIENT_STATUS;

	format_free(ft);
}

/* Draw client prompt on status line of present else on last line. */
int
status_prompt_redraw(struct client *c)
{
	struct screen_write_ctx		ctx;
	struct session		       *s = c->session;
	struct screen		        old_status;
	size_t			        i, size, left, len, off;
	struct grid_cell		gc, *gcp;
	int				utf8flag;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_status, &c->status, sizeof old_status);
	screen_init(&c->status, c->tty.sx, 1, 0);

	utf8flag = options_get_number(&s->options, "status-utf8");

	len = screen_write_strlen(utf8flag, "%s", c->prompt_string);
	if (len > c->tty.sx)
		len = c->tty.sx;
	off = 0;

	/* Change colours for command mode. */
	if (c->prompt_mdata.mode == 1)
		style_apply(&gc, &s->options, "message-command-style");
	else
		style_apply(&gc, &s->options, "message-style");

	screen_write_start(&ctx, NULL, &c->status);

	screen_write_cursormove(&ctx, 0, 0);
	screen_write_nputs(&ctx, len, &gc, utf8flag, "%s", c->prompt_string);

	left = c->tty.sx - len;
	if (left != 0) {
		size = screen_write_strlen(utf8flag, "%s", c->prompt_buffer);
		if (c->prompt_index >= left) {
			off = c->prompt_index - left + 1;
			if (c->prompt_index == size)
				left--;
			size = left;
		}
		screen_write_nputs(
		    &ctx, left, &gc, utf8flag, "%s", c->prompt_buffer + off);

		for (i = len + size; i < c->tty.sx; i++)
			screen_write_putc(&ctx, &gc, ' ');
	}

	screen_write_stop(&ctx);

	/* Apply fake cursor. */
	off = len + c->prompt_index - off;
	gcp = grid_view_get_cell(c->status.grid, off, 0);
	gcp->attr ^= GRID_ATTR_REVERSE;

	if (grid_compare(c->status.grid, old_status.grid) == 0) {
		screen_free(&old_status);
		return (0);
	}
	screen_free(&old_status);
	return (1);
}

/* Handle keys in prompt. */
void
status_prompt_key(struct client *c, int key)
{
	struct session		*sess = c->session;
	struct options		*oo = &sess->options;
	struct paste_buffer	*pb;
	char			*s, *first, *last, word[64], swapc;
	const char		*histstr, *bufdata, *wsep = NULL;
	u_char			 ch;
	size_t			 size, n, off, idx, bufsize;

	size = strlen(c->prompt_buffer);
	switch (mode_key_lookup(&c->prompt_mdata, key, NULL)) {
	case MODEKEYEDIT_CURSORLEFT:
		if (c->prompt_index > 0) {
			c->prompt_index--;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_SWITCHMODE:
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_SWITCHMODEAPPEND:
		c->flags |= CLIENT_STATUS;
		/* FALLTHROUGH */
	case MODEKEYEDIT_CURSORRIGHT:
		if (c->prompt_index < size) {
			c->prompt_index++;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_SWITCHMODEBEGINLINE:
		c->flags |= CLIENT_STATUS;
		/* FALLTHROUGH */
	case MODEKEYEDIT_STARTOFLINE:
		if (c->prompt_index != 0) {
			c->prompt_index = 0;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_SWITCHMODEAPPENDLINE:
		c->flags |= CLIENT_STATUS;
		/* FALLTHROUGH */
	case MODEKEYEDIT_ENDOFLINE:
		if (c->prompt_index != size) {
			c->prompt_index = size;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_COMPLETE:
		if (*c->prompt_buffer == '\0')
			break;

		idx = c->prompt_index;
		if (idx != 0)
			idx--;

		/* Find the word we are in. */
		first = c->prompt_buffer + idx;
		while (first > c->prompt_buffer && *first != ' ')
			first--;
		while (*first == ' ')
			first++;
		last = c->prompt_buffer + idx;
		while (*last != '\0' && *last != ' ')
			last++;
		while (*last == ' ')
			last--;
		if (*last != '\0')
			last++;
		if (last <= first ||
		    ((size_t) (last - first)) > (sizeof word) - 1)
			break;
		memcpy(word, first, last - first);
		word[last - first] = '\0';

		/* And try to complete it. */
		if ((s = status_prompt_complete(sess, word)) == NULL)
			break;

		/* Trim out word. */
		n = size - (last - c->prompt_buffer) + 1; /* with \0 */
		memmove(first, last, n);
		size -= last - first;

		/* Insert the new word. */
		size += strlen(s);
		off = first - c->prompt_buffer;
		c->prompt_buffer = xrealloc(c->prompt_buffer, size + 1);
		first = c->prompt_buffer + off;
		memmove(first + strlen(s), first, n);
		memcpy(first, s, strlen(s));

		c->prompt_index = (first - c->prompt_buffer) + strlen(s);
		free(s);

		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_BACKSPACE:
		if (c->prompt_index != 0) {
			if (c->prompt_index == size)
				c->prompt_buffer[--c->prompt_index] = '\0';
			else {
				memmove(c->prompt_buffer + c->prompt_index - 1,
				    c->prompt_buffer + c->prompt_index,
				    size + 1 - c->prompt_index);
				c->prompt_index--;
			}
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_DELETE:
	case MODEKEYEDIT_SWITCHMODESUBSTITUTE:
		if (c->prompt_index != size) {
			memmove(c->prompt_buffer + c->prompt_index,
			    c->prompt_buffer + c->prompt_index + 1,
			    size + 1 - c->prompt_index);
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_DELETELINE:
	case MODEKEYEDIT_SWITCHMODESUBSTITUTELINE:
		*c->prompt_buffer = '\0';
		c->prompt_index = 0;
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_DELETETOENDOFLINE:
	case MODEKEYEDIT_SWITCHMODECHANGELINE:
		if (c->prompt_index < size) {
			c->prompt_buffer[c->prompt_index] = '\0';
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_DELETEWORD:
		wsep = options_get_string(oo, "word-separators");
		idx = c->prompt_index;

		/* Find a non-separator. */
		while (idx != 0) {
			idx--;
			if (!strchr(wsep, c->prompt_buffer[idx]))
				break;
		}

		/* Find the separator at the beginning of the word. */
		while (idx != 0) {
			idx--;
			if (strchr(wsep, c->prompt_buffer[idx])) {
				/* Go back to the word. */
				idx++;
				break;
			}
		}

		memmove(c->prompt_buffer + idx,
		    c->prompt_buffer + c->prompt_index,
		    size + 1 - c->prompt_index);
		memset(c->prompt_buffer + size - (c->prompt_index - idx),
		    '\0', c->prompt_index - idx);
		c->prompt_index = idx;
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_NEXTSPACE:
		wsep = " ";
		/* FALLTHROUGH */
	case MODEKEYEDIT_NEXTWORD:
		if (wsep == NULL)
			wsep = options_get_string(oo, "word-separators");

		/* Find a separator. */
		while (c->prompt_index != size) {
			c->prompt_index++;
			if (strchr(wsep, c->prompt_buffer[c->prompt_index]))
				break;
		}

		/* Find the word right after the separation. */
		while (c->prompt_index != size) {
			c->prompt_index++;
			if (!strchr(wsep, c->prompt_buffer[c->prompt_index]))
				break;
		}

		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_NEXTSPACEEND:
		wsep = " ";
		/* FALLTHROUGH */
	case MODEKEYEDIT_NEXTWORDEND:
		if (wsep == NULL)
			wsep = options_get_string(oo, "word-separators");

		/* Find a word. */
		while (c->prompt_index != size) {
			c->prompt_index++;
			if (!strchr(wsep, c->prompt_buffer[c->prompt_index]))
				break;
		}

		/* Find the separator at the end of the word. */
		while (c->prompt_index != size) {
			c->prompt_index++;
			if (strchr(wsep, c->prompt_buffer[c->prompt_index]))
				break;
		}

		/* Back up to the end-of-word like vi. */
		if (options_get_number(oo, "status-keys") == MODEKEY_VI &&
		    c->prompt_index != 0)
			c->prompt_index--;

		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_PREVIOUSSPACE:
		wsep = " ";
		/* FALLTHROUGH */
	case MODEKEYEDIT_PREVIOUSWORD:
		if (wsep == NULL)
			wsep = options_get_string(oo, "word-separators");

		/* Find a non-separator. */
		while (c->prompt_index != 0) {
			c->prompt_index--;
			if (!strchr(wsep, c->prompt_buffer[c->prompt_index]))
				break;
		}

		/* Find the separator at the beginning of the word. */
		while (c->prompt_index != 0) {
			c->prompt_index--;
			if (strchr(wsep, c->prompt_buffer[c->prompt_index])) {
				/* Go back to the word. */
				c->prompt_index++;
				break;
			}
		}

		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_HISTORYUP:
		histstr = status_prompt_up_history(&c->prompt_hindex);
		if (histstr == NULL)
			break;
		free(c->prompt_buffer);
		c->prompt_buffer = xstrdup(histstr);
		c->prompt_index = strlen(c->prompt_buffer);
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_HISTORYDOWN:
		histstr = status_prompt_down_history(&c->prompt_hindex);
		if (histstr == NULL)
			break;
		free(c->prompt_buffer);
		c->prompt_buffer = xstrdup(histstr);
		c->prompt_index = strlen(c->prompt_buffer);
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_PASTE:
		if ((pb = paste_get_top(NULL)) == NULL)
			break;
		bufdata = paste_buffer_data(pb, &bufsize);
		for (n = 0; n < bufsize; n++) {
			ch = (u_char)bufdata[n];
			if (ch < 32 || ch == 127)
				break;
		}

		c->prompt_buffer = xrealloc(c->prompt_buffer, size + n + 1);
		if (c->prompt_index == size) {
			memcpy(c->prompt_buffer + c->prompt_index, bufdata, n);
			c->prompt_index += n;
			c->prompt_buffer[c->prompt_index] = '\0';
		} else {
			memmove(c->prompt_buffer + c->prompt_index + n,
			    c->prompt_buffer + c->prompt_index,
			    size + 1 - c->prompt_index);
			memcpy(c->prompt_buffer + c->prompt_index, bufdata, n);
			c->prompt_index += n;
		}

		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_TRANSPOSECHARS:
		idx = c->prompt_index;
		if (idx < size)
			idx++;
		if (idx >= 2) {
			swapc = c->prompt_buffer[idx - 2];
			c->prompt_buffer[idx - 2] = c->prompt_buffer[idx - 1];
			c->prompt_buffer[idx - 1] = swapc;
			c->prompt_index = idx;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_ENTER:
		if (*c->prompt_buffer != '\0')
			status_prompt_add_history(c->prompt_buffer);
		if (c->prompt_callbackfn(c->prompt_data, c->prompt_buffer) == 0)
			status_prompt_clear(c);
		break;
	case MODEKEYEDIT_CANCEL:
		if (c->prompt_callbackfn(c->prompt_data, NULL) == 0)
			status_prompt_clear(c);
		break;
	case MODEKEY_OTHER:
		if ((key & 0xff00) != 0 || key < 32 || key == 127)
			break;
		c->prompt_buffer = xrealloc(c->prompt_buffer, size + 2);

		if (c->prompt_index == size) {
			c->prompt_buffer[c->prompt_index++] = key;
			c->prompt_buffer[c->prompt_index] = '\0';
		} else {
			memmove(c->prompt_buffer + c->prompt_index + 1,
			    c->prompt_buffer + c->prompt_index,
			    size + 1 - c->prompt_index);
			c->prompt_buffer[c->prompt_index++] = key;
		}

		if (c->prompt_flags & PROMPT_SINGLE) {
			if (c->prompt_callbackfn(
			    c->prompt_data, c->prompt_buffer) == 0)
				status_prompt_clear(c);
		}

		c->flags |= CLIENT_STATUS;
		break;
	default:
		break;
	}
}

/* Get previous line from the history. */
const char *
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
const char *
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
void
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
const char **
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
	for (oe = server_options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, strlen(s)) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = oe->name;
		}
	}
	for (oe = session_options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, strlen(s)) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = oe->name;
		}
	}
	for (oe = window_options_table; oe->name != NULL; oe++) {
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
char *
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
char *
status_prompt_complete(struct session *sess, const char *s)
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
		out = tmp;
		goto found;
	}

	colon = "";
	if (*s == ':') {
		RB_FOREACH(wl, winlinks, &sess->windows) {
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
