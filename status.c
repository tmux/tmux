/* $Id$ */

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

char   *status_redraw_get_left(
	    struct client *, time_t, int, struct grid_cell *, size_t *);
char   *status_redraw_get_right(
	    struct client *, time_t, int, struct grid_cell *, size_t *);
char   *status_find_job(struct client *, char **);
void	status_job_free(void *);
void	status_job_callback(struct job *);
char   *status_print(
	    struct client *, struct winlink *, time_t, struct grid_cell *);
void	status_replace1(struct client *, struct session *, struct winlink *,
	    struct window_pane *, char **, char **, char *, size_t, int);
void	status_message_callback(int, short, void *);

const char *status_prompt_up_history(u_int *);
const char *status_prompt_down_history(u_int *);
void	status_prompt_add_history(const char *);
char   *status_prompt_complete(const char *);

/* Status prompt history. */
ARRAY_DECL(, char *) status_prompt_history = ARRAY_INITIALIZER;

/* Status output tree. */
RB_GENERATE(status_out_tree, status_out, entry, status_out_cmp);

/* Output tree comparison function. */
int
status_out_cmp(struct status_out *so1, struct status_out *so2)
{
	return (strcmp(so1->cmd, so2->cmd));
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
status_redraw_get_left(struct client *c,
    time_t t, int utf8flag, struct grid_cell *gc, size_t *size)
{
	struct session	*s = c->session;
	char		*left;
	u_char		 fg, bg, attr;
	size_t		 leftlen;

	fg = options_get_number(&s->options, "status-left-fg");
	if (fg != 8)
		colour_set_fg(gc, fg);
	bg = options_get_number(&s->options, "status-left-bg");
	if (bg != 8)
		colour_set_bg(gc, bg);
	attr = options_get_number(&s->options, "status-left-attr");
	if (attr != 0)
		gc->attr = attr;

	left = status_replace(c, NULL,
	    NULL, NULL, options_get_string(&s->options, "status-left"), t, 1);

	*size = options_get_number(&s->options, "status-left-length");
	leftlen = screen_write_cstrlen(utf8flag, "%s", left);
	if (leftlen < *size)
		*size = leftlen;
	return (left);
}

/* Retrieve options for right string. */
char *
status_redraw_get_right(struct client *c,
    time_t t, int utf8flag, struct grid_cell *gc, size_t *size)
{
	struct session	*s = c->session;
	char		*right;
	u_char		 fg, bg, attr;
	size_t		 rightlen;

	fg = options_get_number(&s->options, "status-right-fg");
	if (fg != 8)
		colour_set_fg(gc, fg);
	bg = options_get_number(&s->options, "status-right-bg");
	if (bg != 8)
		colour_set_bg(gc, bg);
	attr = options_get_number(&s->options, "status-right-attr");
	if (attr != 0)
		gc->attr = attr;

	right = status_replace(c, NULL,
	    NULL, NULL, options_get_string(&s->options, "status-right"), t, 1);

	*size = options_get_number(&s->options, "status-right-length");
	rightlen = screen_write_cstrlen(utf8flag, "%s", right);
	if (rightlen < *size)
		*size = rightlen;
	return (right);
}

/* Set window at window list position. */
void
status_set_window_at(struct client *c, u_int x)
{
	struct session	*s = c->session;
	struct winlink	*wl;

	x += c->wlmouse;
	RB_FOREACH(wl, winlinks, &s->windows) {
		if (x < wl->status_width &&
			session_select(s, wl->idx) == 0) {
			server_redraw_session(s);
		}
		x -= wl->status_width + 1;
	}
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

	/* Update status timer. */
	if (gettimeofday(&c->status_timer, NULL) != 0)
		fatal("gettimeofday failed");
	t = c->status_timer.tv_sec;

	/* Set up default colour. */
	memcpy(&stdgc, &grid_default_cell, sizeof gc);
	colour_set_fg(&stdgc, options_get_number(&s->options, "status-fg"));
	colour_set_bg(&stdgc, options_get_number(&s->options, "status-bg"));
	stdgc.attr |= options_get_number(&s->options, "status-attr");

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
		needed += llen + 1;
	if (rlen != 0)
		needed += rlen + 1;
	if (c->tty.sx == 0 || c->tty.sx <= needed)
		goto out;
	wlavailable = c->tty.sx - needed;

	/* Calculate the total size needed for the window list. */
	wlstart = wloffset = wlwidth = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		if (wl->status_text != NULL)
			xfree(wl->status_text);
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
	if (llen != 0) {
		screen_write_cnputs(&ctx, llen, &lgc, utf8flag, "%s", left);
		screen_write_putc(&ctx, &stdgc, ' ');
	}
	if (larrow != 0) {
		memcpy(&gc, &stdgc, sizeof gc);
		if (larrow == -1)
			gc.attr ^= GRID_ATTR_REVERSE;
		screen_write_putc(&ctx, &gc, '<');
	}

	/* Draw the right string and arrow. */
	if (rarrow != 0) {
		screen_write_cursormove(&ctx, c->tty.sx - rlen - 2, 0);
		memcpy(&gc, &stdgc, sizeof gc);
		if (rarrow == -1)
			gc.attr ^= GRID_ATTR_REVERSE;
		screen_write_putc(&ctx, &gc, '>');
	} else
		screen_write_cursormove(&ctx, c->tty.sx - rlen - 1, 0);
	if (rlen != 0) {
		screen_write_putc(&ctx, &stdgc, ' ');
		screen_write_cnputs(&ctx, rlen, &rgc, utf8flag, "%s", right);
	}

	/* Figure out the offset for the window list. */
	if (llen != 0)
		wloffset = llen + 1;
	else
		wloffset = 0;
	if (wlwidth < wlavailable) {
		switch (options_get_number(&s->options, "status-justify")) {
		case 1:	/* centered */
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
	if (left != NULL)
		xfree(left);
	if (right != NULL)
		xfree(right);

	if (grid_compare(c->status.grid, old_status.grid) == 0) {
		screen_free(&old_status);
		return (0);
	}
	screen_free(&old_status);
	return (1);
}

/* Replace a single special sequence (prefixed by #). */
void
status_replace1(struct client *c, struct session *s, struct winlink *wl,
    struct window_pane *wp, char **iptr, char **optr, char *out,
    size_t outsize, int jobsflag)
{
	char	ch, tmp[256], *ptr, *endptr, *freeptr;
	size_t	ptrlen;
	long	limit;
	u_int	idx;

	if (s == NULL)
		s = c->session;
	if (wl == NULL)
		wl = s->curw;
	if (wp == NULL)
		wp = wl->window->active;

	errno = 0;
	limit = strtol(*iptr, &endptr, 10);
	if ((limit == 0 && errno != EINVAL) ||
	    (limit == LONG_MIN && errno != ERANGE) ||
	    (limit == LONG_MAX && errno != ERANGE) ||
	    limit != 0)
		*iptr = endptr;
	if (limit <= 0)
		limit = LONG_MAX;

	freeptr = NULL;

	switch (*(*iptr)++) {
	case '(':
		if (!jobsflag) {
			ch = ')';
			goto skip_to;
		}
		if ((ptr = status_find_job(c, iptr)) == NULL)
			return;
		goto do_replace;
	case 'D':
		xsnprintf(tmp, sizeof tmp, "%%%u", wp->id);
		ptr = tmp;
		goto do_replace;
	case 'H':
		if (gethostname(tmp, sizeof tmp) != 0)
			fatal("gethostname failed");
		ptr = tmp;
		goto do_replace;
	case 'h':
		if (gethostname(tmp, sizeof tmp) != 0)
			fatal("gethostname failed");
		if ((ptr = strchr(tmp, '.')) != NULL)
			*ptr = '\0';
		ptr = tmp;
		goto do_replace;
	case 'I':
		xsnprintf(tmp, sizeof tmp, "%d", wl->idx);
		ptr = tmp;
		goto do_replace;
	case 'P':
		if (window_pane_index(wp, &idx) != 0)
			fatalx("index not found");
		xsnprintf(
		    tmp, sizeof tmp, "%u", idx);
		ptr = tmp;
		goto do_replace;
	case 'S':
		ptr = s->name;
		goto do_replace;
	case 'T':
		ptr = wp->base.title;
		goto do_replace;
	case 'W':
		ptr = wl->window->name;
		goto do_replace;
	case 'F':
		ptr = window_printable_flags(s, wl);
		freeptr = ptr;
		goto do_replace;
	case '[':
		/*
		 * Embedded style, handled at display time. Leave present and
		 * skip input until ].
		 */
		ch = ']';
		goto skip_to;
	case '#':
		*(*optr)++ = '#';
		break;
	}

	return;

do_replace:
	ptrlen = strlen(ptr);
	if ((size_t) limit < ptrlen)
		ptrlen = limit;

	if (*optr + ptrlen >= out + outsize - 1)
		goto out;
	while (ptrlen > 0 && *ptr != '\0') {
		*(*optr)++ = *ptr++;
		ptrlen--;
	}

out:
	if (freeptr != NULL)
		xfree(freeptr);
	return;

skip_to:
	*(*optr)++ = '#';

	(*iptr)--;	/* include ch */
	while (**iptr != ch && **iptr != '\0') {
		if (*optr >=  out + outsize - 1)
			break;
		*(*optr)++ = *(*iptr)++;
	}
}

/* Replace special sequences in fmt. */
char *
status_replace(struct client *c, struct session *s, struct winlink *wl,
    struct window_pane *wp, const char *fmt, time_t t, int jobsflag)
{
	static char	out[BUFSIZ];
	char		in[BUFSIZ], ch, *iptr, *optr;
	size_t		len;

	len = strftime(in, sizeof in, fmt, localtime(&t));
	in[len] = '\0';

	iptr = in;
	optr = out;

	while (*iptr != '\0') {
		if (optr >= out + (sizeof out) - 1)
			break;
		ch = *iptr++;

		if (ch != '#' || *iptr == '\0') {
			*optr++ = ch;
			continue;
		}
		status_replace1(
		    c, s, wl, wp, &iptr, &optr, out, sizeof out, jobsflag);
	}
	*optr = '\0';

	return (xstrdup(out));
}

/* Figure out job name and get its result, starting it off if necessary. */
char *
status_find_job(struct client *c, char **iptr)
{
	struct status_out	*so, so_find;
	char   			*cmd;
	int			 lastesc;
	size_t			 len;

	if (**iptr == '\0')
		return (NULL);
	if (**iptr == ')') {		/* no command given */
		(*iptr)++;
		return (NULL);
	}

	cmd = xmalloc(strlen(*iptr) + 1);
	len = 0;

	lastesc = 0;
	for (; **iptr != '\0'; (*iptr)++) {
		if (!lastesc && **iptr == ')')
			break;		/* unescaped ) is the end */
		if (!lastesc && **iptr == '\\') {
			lastesc = 1;
			continue;	/* skip \ if not escaped */
		}
		lastesc = 0;
		cmd[len++] = **iptr;
	}
	if (**iptr == '\0')		/* no terminating ) */ {
		xfree(cmd);
		return (NULL);
	}
	(*iptr)++;			/* skip final ) */
	cmd[len] = '\0';

	/* First try in the new tree. */
	so_find.cmd = cmd;
	so = RB_FIND(status_out_tree, &c->status_new, &so_find);
	if (so != NULL && so->out != NULL) {
		xfree(cmd);
		return (so->out);
	}

	/* If not found at all, start the job and add to the tree. */
	if (so == NULL) {
		job_run(cmd, status_job_callback, status_job_free, c);
		c->references++;

		so = xmalloc(sizeof *so);
		so->cmd = xstrdup(cmd);
		so->out = NULL;
		RB_INSERT(status_out_tree, &c->status_new, so);
	}

	/* Lookup in the old tree. */
	so_find.cmd = cmd;
	so = RB_FIND(status_out_tree, &c->status_old, &so_find);
	xfree(cmd);
	if (so != NULL)
		return (so->out);
	return (NULL);
}

/* Free job tree. */
void
status_free_jobs(struct status_out_tree *sotree)
{
	struct status_out	*so, *so_next;

	so_next = RB_MIN(status_out_tree, sotree);
	while (so_next != NULL) {
		so = so_next;
		so_next = RB_NEXT(status_out_tree, sotree, so);

		RB_REMOVE(status_out_tree, sotree, so);
		if (so->out != NULL)
			xfree(so->out);
		xfree(so->cmd);
		xfree(so);
	}
}

/* Update jobs on status interval. */
void
status_update_jobs(struct client *c)
{
	/* Free the old tree. */
	status_free_jobs(&c->status_old);

	/* Move the new to old. */
	memcpy(&c->status_old, &c->status_new, sizeof c->status_old);
	RB_INIT(&c->status_new);
}

/* Free status job. */
void
status_job_free(void *data)
{
	struct client	*c = data;

	c->references--;
}

/* Job has finished: save its result. */
void
status_job_callback(struct job *job)
{
	struct client		*c = job->data;
	struct status_out	*so, so_find;
	char			*line, *buf;
	size_t			 len;

	if (c->flags & CLIENT_DEAD)
		return;

	so_find.cmd = job->cmd;
	so = RB_FIND(status_out_tree, &c->status_new, &so_find);
	if (so == NULL || so->out != NULL)
		return;

	buf = NULL;
	if ((line = evbuffer_readline(job->event->input)) == NULL) {
		len = EVBUFFER_LENGTH(job->event->input);
		buf = xmalloc(len + 1);
		if (len != 0)
			memcpy(buf, EVBUFFER_DATA(job->event->input), len);
		buf[len] = '\0';
	} else
		buf = xstrdup(line);

	so->out = buf;
	server_status_client(c);
}

/* Return winlink status line entry and adjust gc as necessary. */
char *
status_print(
    struct client *c, struct winlink *wl, time_t t, struct grid_cell *gc)
{
	struct options	*oo = &wl->window->options;
	struct session	*s = c->session;
	const char	*fmt;
	char   		*text;
	u_char		 fg, bg, attr;

	fg = options_get_number(oo, "window-status-fg");
	if (fg != 8)
		colour_set_fg(gc, fg);
	bg = options_get_number(oo, "window-status-bg");
	if (bg != 8)
		colour_set_bg(gc, bg);
	attr = options_get_number(oo, "window-status-attr");
	if (attr != 0)
		gc->attr = attr;
	fmt = options_get_string(oo, "window-status-format");
	if (wl == s->curw) {
		fg = options_get_number(oo, "window-status-current-fg");
		if (fg != 8)
			colour_set_fg(gc, fg);
		bg = options_get_number(oo, "window-status-current-bg");
		if (bg != 8)
			colour_set_bg(gc, bg);
		attr = options_get_number(oo, "window-status-current-attr");
		if (attr != 0)
			gc->attr = attr;
		fmt = options_get_string(oo, "window-status-current-format");
	}

	if (wl->flags & WINLINK_BELL) {
		fg = options_get_number(oo, "window-status-bell-fg");
		if (fg != 8)
			colour_set_fg(gc, fg);
		bg = options_get_number(oo, "window-status-bell-bg");
		if (bg != 8)
			colour_set_bg(gc, bg);
		attr = options_get_number(oo, "window-status-bell-attr");
		if (attr != 0)
			gc->attr = attr;
	} else if (wl->flags & WINLINK_CONTENT) {
		fg = options_get_number(oo, "window-status-content-fg");
		if (fg != 8)
			colour_set_fg(gc, fg);
		bg = options_get_number(oo, "window-status-content-bg");
		if (bg != 8)
			colour_set_bg(gc, bg);
		attr = options_get_number(oo, "window-status-content-attr");
		if (attr != 0)
			gc->attr = attr;
	} else if (wl->flags & (WINLINK_ACTIVITY|WINLINK_SILENCE)) {
		fg = options_get_number(oo, "window-status-activity-fg");
		if (fg != 8)
			colour_set_fg(gc, fg);
		bg = options_get_number(oo, "window-status-activity-bg");
		if (bg != 8)
			colour_set_bg(gc, bg);
		attr = options_get_number(oo, "window-status-activity-attr");
		if (attr != 0)
			gc->attr = attr;
	}

	text = status_replace(c, NULL, wl, NULL, fmt, t, 1);
	return (text);
}

/* Set a status line message. */
void printflike2
status_message_set(struct client *c, const char *fmt, ...)
{
	struct timeval		 tv;
	struct session		*s = c->session;
	struct message_entry	*msg;
	va_list			 ap;
	int			 delay;
	u_int			 i, limit;

	status_prompt_clear(c);
	status_message_clear(c);

	va_start(ap, fmt);
	xvasprintf(&c->message_string, fmt, ap);
	va_end(ap);

	ARRAY_EXPAND(&c->message_log, 1);
	msg = &ARRAY_LAST(&c->message_log);
	msg->msg_time = time(NULL);
	msg->msg = xstrdup(c->message_string);

	if (s == NULL)
		limit = 0;
	else
		limit = options_get_number(&s->options, "message-limit");
	if (ARRAY_LENGTH(&c->message_log) > limit) {
		limit = ARRAY_LENGTH(&c->message_log) - limit;
		for (i = 0; i < limit; i++) {
			msg = &ARRAY_FIRST(&c->message_log);
			xfree(msg->msg);
			ARRAY_REMOVE(&c->message_log, 0);
		}
	}

	delay = options_get_number(&c->session->options, "display-time");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	if (event_initialized (&c->message_timer))
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

	xfree(c->message_string);
	c->message_string = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW; /* screen was frozen and may have changed */

	screen_reinit(&c->status);
}

/* Clear status line message after timer expires. */
/* ARGSUSED */
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

	memcpy(&gc, &grid_default_cell, sizeof gc);
	colour_set_fg(&gc, options_get_number(&s->options, "message-fg"));
	colour_set_bg(&gc, options_get_number(&s->options, "message-bg"));
	gc.attr |= options_get_number(&s->options, "message-attr");

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
	int	keys;

	status_message_clear(c);
	status_prompt_clear(c);

	c->prompt_string = status_replace(c, NULL, NULL, NULL, msg,
	    time(NULL), 0);

	if (input == NULL)
		input = "";
	c->prompt_buffer = status_replace(c, NULL, NULL, NULL, input,
	    time(NULL), 0);
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
}

/* Remove status line prompt. */
void
status_prompt_clear(struct client *c)
{
	if (c->prompt_string == NULL)
		return;

	if (c->prompt_freefn != NULL && c->prompt_data != NULL)
		c->prompt_freefn(c->prompt_data);

	xfree(c->prompt_string);
	c->prompt_string = NULL;

	xfree(c->prompt_buffer);
	c->prompt_buffer = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW; /* screen was frozen and may have changed */

	screen_reinit(&c->status);
}

/* Update status line prompt with a new prompt string. */
void
status_prompt_update(struct client *c, const char *msg, const char *input)
{
	xfree(c->prompt_string);
	c->prompt_string = status_replace(c, NULL, NULL, NULL, msg,
	    time(NULL), 0);

	xfree(c->prompt_buffer);
	if (input == NULL)
		input = "";
	c->prompt_buffer = status_replace(c, NULL, NULL, NULL, input,
	    time(NULL), 0);
	c->prompt_index = strlen(c->prompt_buffer);

	c->prompt_hindex = 0;

	c->flags |= CLIENT_STATUS;
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

	memcpy(&gc, &grid_default_cell, sizeof gc);
	/* Change colours for command mode. */
	if (c->prompt_mdata.mode == 1) {
		colour_set_fg(&gc, options_get_number(&s->options, "message-command-fg"));
		colour_set_bg(&gc, options_get_number(&s->options, "message-command-bg"));
		gc.attr |= options_get_number(&s->options, "message-command-attr");
	} else {
		colour_set_fg(&gc, options_get_number(&s->options, "message-fg"));
		colour_set_bg(&gc, options_get_number(&s->options, "message-bg"));
		gc.attr |= options_get_number(&s->options, "message-attr");
	}

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
	const char		*histstr;
	const char		*wsep = NULL;
	u_char			 ch;
	size_t			 size, n, off, idx;

	size = strlen(c->prompt_buffer);
	switch (mode_key_lookup(&c->prompt_mdata, key)) {
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
		if ((s = status_prompt_complete(word)) == NULL)
			break;

		/* Trim out word. */
		n = size - (last - c->prompt_buffer) + 1; /* with \0 */
		memmove(first, last, n);
		size -= last - first;

		/* Insert the new word. */
		size += strlen(s);
		off = first - c->prompt_buffer;
		c->prompt_buffer = xrealloc(c->prompt_buffer, 1, size + 1);
		first = c->prompt_buffer + off;
		memmove(first + strlen(s), first, n);
		memcpy(first, s, strlen(s));

		c->prompt_index = (first - c->prompt_buffer) + strlen(s);
		xfree(s);

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
		if (c->prompt_index != size) {
			memmove(c->prompt_buffer + c->prompt_index,
			    c->prompt_buffer + c->prompt_index + 1,
			    size + 1 - c->prompt_index);
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYEDIT_DELETELINE:
		*c->prompt_buffer = '\0';
		c->prompt_index = 0;
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_DELETETOENDOFLINE:
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
		xfree(c->prompt_buffer);
		c->prompt_buffer = xstrdup(histstr);
		c->prompt_index = strlen(c->prompt_buffer);
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_HISTORYDOWN:
		histstr = status_prompt_down_history(&c->prompt_hindex);
		if (histstr == NULL)
			break;
		xfree(c->prompt_buffer);
		c->prompt_buffer = xstrdup(histstr);
		c->prompt_index = strlen(c->prompt_buffer);
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYEDIT_PASTE:
		if ((pb = paste_get_top(&global_buffers)) == NULL)
			break;
		for (n = 0; n < pb->size; n++) {
			ch = (u_char) pb->data[n];
			if (ch < 32 || ch == 127)
				break;
		}

		c->prompt_buffer = xrealloc(c->prompt_buffer, 1, size + n + 1);
		if (c->prompt_index == size) {
			memcpy(c->prompt_buffer + c->prompt_index, pb->data, n);
			c->prompt_index += n;
			c->prompt_buffer[c->prompt_index] = '\0';
		} else {
			memmove(c->prompt_buffer + c->prompt_index + n,
			    c->prompt_buffer + c->prompt_index,
			    size + 1 - c->prompt_index);
			memcpy(c->prompt_buffer + c->prompt_index, pb->data, n);
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
		c->prompt_buffer = xrealloc(c->prompt_buffer, 1, size + 2);

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
	u_int size;

	/*
	 * History runs from 0 to size - 1.
	 *
	 * Index is from 0 to size. Zero is empty.
	 */

	size = ARRAY_LENGTH(&status_prompt_history);
	if (size == 0 || *idx == size)
		return (NULL);
	(*idx)++;
	return (ARRAY_ITEM(&status_prompt_history, size - *idx));
}

/* Get next line from the history. */
const char *
status_prompt_down_history(u_int *idx)
{
	u_int size;

	size = ARRAY_LENGTH(&status_prompt_history);
	if (size == 0 || *idx == 0)
		return ("");
	(*idx)--;
	if (*idx == 0)
		return ("");
	return (ARRAY_ITEM(&status_prompt_history, size - *idx));
}

/* Add line to the history. */
void
status_prompt_add_history(const char *line)
{
	u_int size;

	size = ARRAY_LENGTH(&status_prompt_history);
	if (size > 0 && strcmp(ARRAY_LAST(&status_prompt_history), line) == 0)
		return;

	if (size == PROMPT_HISTORY) {
		xfree(ARRAY_FIRST(&status_prompt_history));
		ARRAY_REMOVE(&status_prompt_history, 0);
	}

	ARRAY_ADD(&status_prompt_history, xstrdup(line));
}

/* Complete word. */
char *
status_prompt_complete(const char *s)
{
	const struct cmd_entry 	  	       **cmdent;
	const struct options_table_entry	*oe;
	ARRAY_DECL(, const char *)		 list;
	char					*prefix, *s2;
	u_int					 i;
	size_t				 	 j;

	if (*s == '\0')
		return (NULL);

	/* First, build a list of all the possible matches. */
	ARRAY_INIT(&list);
	for (cmdent = cmd_table; *cmdent != NULL; cmdent++) {
		if (strncmp((*cmdent)->name, s, strlen(s)) == 0)
			ARRAY_ADD(&list, (*cmdent)->name);
	}
	for (oe = server_options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, strlen(s)) == 0)
			ARRAY_ADD(&list, oe->name);
	}
	for (oe = session_options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, strlen(s)) == 0)
			ARRAY_ADD(&list, oe->name);
	}
	for (oe = window_options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, strlen(s)) == 0)
			ARRAY_ADD(&list, oe->name);
	}

	/* If none, bail now. */
	if (ARRAY_LENGTH(&list) == 0) {
		ARRAY_FREE(&list);
		return (NULL);
	}

	/* If an exact match, return it, with a trailing space. */
	if (ARRAY_LENGTH(&list) == 1) {
		xasprintf(&s2, "%s ", ARRAY_FIRST(&list));
		ARRAY_FREE(&list);
		return (s2);
	}

	/* Now loop through the list and find the longest common prefix. */
	prefix = xstrdup(ARRAY_FIRST(&list));
	for (i = 1; i < ARRAY_LENGTH(&list); i++) {
		s = ARRAY_ITEM(&list, i);

		j = strlen(s);
		if (j > strlen(prefix))
			j = strlen(prefix);
		for (; j > 0; j--) {
			if (prefix[j - 1] != s[j - 1])
				prefix[j - 1] = '\0';
		}
	}

	ARRAY_FREE(&list);
	return (prefix);
}
