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

char   *status_replace(struct session *, char *, time_t);
char   *status_replace_popen(char **);
size_t	status_width(struct winlink *);
char   *status_print(struct session *, struct winlink *, struct grid_cell *);

void	status_prompt_add_history(struct client *);
char   *status_prompt_complete(const char *);

/* Draw status for client on the last lines of given context. */
int
status_redraw(struct client *c)
{
	struct screen_write_ctx		ctx;
	struct session		       *s = c->session;
	struct winlink		       *wl;
	struct window_pane	       *wp;
	struct screen		       *sc = NULL, old_status;
	char		 	       *left, *right, *text, *ptr;
	size_t				llen, rlen, offset, xx, yy, sy;
	size_t				size, start, width;
	struct grid_cell	        stdgc, gc;
	int				larrow, rarrow;

	left = right = NULL;

	/* Create the target screen. */
	memcpy(&old_status, &c->status, sizeof old_status);
	screen_init(&c->status, c->tty.sx, 1, 0);

	/* No status line? */
	if (c->tty.sy == 0 || !options_get_number(&s->options, "status"))
		goto off;
	larrow = rarrow = 0;

	if (gettimeofday(&c->status_timer, NULL) != 0)
		fatal("gettimeofday");
	memcpy(&stdgc, &grid_default_cell, sizeof gc);
	stdgc.bg = options_get_number(&s->options, "status-fg");
	stdgc.fg = options_get_number(&s->options, "status-bg");
	stdgc.attr |= options_get_number(&s->options, "status-attr");

	yy = c->tty.sy - 1;
	if (yy == 0)
		goto blank;

	/* Work out the left and right strings. */
	left = status_replace(s, options_get_string(
	    &s->options, "status-left"), c->status_timer.tv_sec);
	llen = options_get_number(&s->options, "status-left-length");
	if (strlen(left) < llen)
		llen = strlen(left);
	left[llen] = '\0';

	right = status_replace(s, options_get_string(
	    &s->options, "status-right"), c->status_timer.tv_sec);
	rlen = options_get_number(&s->options, "status-right-length");
	if (strlen(right) < rlen)
		rlen = strlen(right);
	right[rlen] = '\0';

	/*
	 * Figure out how much space we have for the window list. If there isn't
	 * enough space, just wimp out.
	 */
	xx = 0;
	if (llen != 0)
		xx += llen + 1;
	if (rlen != 0)
		xx += rlen + 1;
	if (c->tty.sx == 0 || c->tty.sx <= xx)
		goto blank;
	xx = c->tty.sx - xx;

	/*
	 * Right. We have xx characters to fill. Find out how much is to go in
	 * them and the offset of the current window (it must be on screen).
	 */
	width = offset = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		size = status_width(wl) + 1;
		if (wl == s->curw)
			offset = width;
		width += size;
	}
	start = 0;

	/* If there is enough space for the total width, all is gravy. */
	if (width <= xx)
		goto draw;

	/* Find size of current window text. */
	size = status_width(s->curw);

	/*
	 * If the offset is already on screen, we're good to draw from the
	 * start and just leave off the end.
	 */
	if (offset + size < xx) {
		if (xx > 0) {
			rarrow = 1;
			xx--;
		}

		width = xx;
		goto draw;
	}

	/*
	 * Work out how many characters we need to omit from the start. There
	 * are xx characters to fill, and offset + size must be the last. So,
	 * the start character is offset + size - xx.
	 */
	if (xx > 0) {
		larrow = 1;
		xx--;
	}

	start = offset + size - xx;
 	if (xx > 0 && width > start + xx + 1) { /* + 1, eh? */
 		rarrow = 1;
 		start++;
 		xx--;
 	}
 	width = xx;

draw:
	/* Bail here if anything is too small too. XXX. */
	if (width == 0 || xx == 0)
		goto blank;

 	/* Begin drawing and move to the starting position. */
	screen_write_start(&ctx, NULL, &c->status);
	if (llen != 0) {
 		screen_write_cursormove(&ctx, 0, yy);
		screen_write_puts(&ctx, &stdgc, "%s ", left);
		if (larrow)
			screen_write_putc(&ctx, &stdgc, ' ');
	} else {
		if (larrow)
			screen_write_cursormove(&ctx, 1, yy);
		else
			screen_write_cursormove(&ctx, 0, yy);
	}

	/* Draw each character in succession. */
	offset = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		memcpy(&gc, &stdgc, sizeof gc);
		text = status_print(s, wl, &gc);

		if (larrow == 1 && offset < start) {
			if (session_alert_has(s, wl, WINDOW_ACTIVITY))
				larrow = -1;
			else if (session_alert_has(s, wl, WINDOW_BELL))
				larrow = -1;
			else if (session_alert_has(s, wl, WINDOW_CONTENT))
				larrow = -1;
		}

 		for (ptr = text; *ptr != '\0'; ptr++) {
			if (offset >= start && offset < start + width)
				screen_write_putc(&ctx, &gc, *ptr);
			offset++;
		}

		if (rarrow == 1 && offset > start + width) {
			if (session_alert_has(s, wl, WINDOW_ACTIVITY))
				rarrow = -1;
			else if (session_alert_has(s, wl, WINDOW_BELL))
				rarrow = -1;
			else if (session_alert_has(s, wl, WINDOW_CONTENT))
				rarrow = -1;
		}

		if (offset < start + width) {
			if (offset >= start) {
				screen_write_putc(&ctx, &stdgc, ' ');
			}
			offset++;
		}

		xfree(text);
	}

	/* Fill the remaining space if any. */
 	while (offset++ < xx)
		screen_write_putc(&ctx, &stdgc, ' ');

	/* Draw the last item. */
	if (rlen != 0) {
		screen_write_cursormove(&ctx, c->tty.sx - rlen - 1, yy);
		screen_write_puts(&ctx, &stdgc, " %s", right);
	}

	/* Draw the arrows. */
	if (larrow != 0) {
		memcpy(&gc, &stdgc, sizeof gc);
		if (larrow == -1)
			gc.attr ^= GRID_ATTR_REVERSE;
		if (llen != 0)
			screen_write_cursormove(&ctx, llen + 1, yy);
		else
			screen_write_cursormove(&ctx, 0, yy);
		screen_write_putc(&ctx, &gc, '<');
	}
	if (rarrow != 0) {
		memcpy(&gc, &stdgc, sizeof gc);
		if (rarrow == -1)
			gc.attr ^= GRID_ATTR_REVERSE;
		if (rlen != 0)
			screen_write_cursormove(&ctx, c->tty.sx - rlen - 2, yy);
		else
			screen_write_cursormove(&ctx, c->tty.sx - 1, yy);
		screen_write_putc(&ctx, &gc, '>');
	}

	goto out;

blank:
 	/* Just draw the whole line as blank. */
	screen_write_start(&ctx, NULL, &c->status);
	screen_write_cursormove(&ctx, 0, yy);
	for (offset = 0; offset < c->tty.sx; offset++)
		screen_write_putc(&ctx, &stdgc, ' ');

	goto out;

off:
	/*
	 * Draw the real window last line. Necessary to wipe over message if
	 * status is off. Not sure this is the right place for this.
	 */
	memcpy(&stdgc, &grid_default_cell, sizeof stdgc);
	screen_write_start(&ctx, NULL, &c->status);

	sy = 0;
	TAILQ_FOREACH(wp, &s->curw->window->panes, entry) {
		sy += wp->sy + 1;
		sc = wp->screen;
	}

	screen_write_cursormove(&ctx, 0, 0);
	if (sy < c->tty.sy) {
		/* If the screen is too small, use blank. */
 		for (offset = 0; offset < c->tty.sx; offset++)
 			screen_write_putc(&ctx, &stdgc, ' ');
	} else {
		screen_write_copy(&ctx,
		    sc, 0, sc->grid->hsize + screen_size_y(sc) - 1, c->tty.sx, 1);
	}

out:
	screen_write_stop(&ctx);

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

char *
status_replace(struct session *s, char *fmt, time_t t)
{
	struct winlink *wl = s->curw;
	static char	out[BUFSIZ];
	char		in[BUFSIZ], tmp[256], ch, *iptr, *optr, *ptr, *endptr;
	char           *savedptr;
	size_t		len;
	long		n;

	strftime(in, sizeof in, fmt, localtime(&t));
	in[(sizeof in) - 1] = '\0';

	iptr = in;
	optr = out;
	savedptr = NULL;

	while (*iptr != '\0') {
		if (optr >= out + (sizeof out) - 1)
			break;
		switch (ch = *iptr++) {
		case '#':
			errno = 0;
			n = strtol(iptr, &endptr, 10);
			if ((n == 0 && errno != EINVAL) ||
			    (n == LONG_MIN && errno != ERANGE) ||
			    (n == LONG_MAX && errno != ERANGE) ||
			    n != 0)
				iptr = endptr;
			if (n <= 0)
				n = LONG_MAX;

			ptr = NULL;
			switch (*iptr++) {
			case '(':
				if (ptr == NULL) {
					ptr = status_replace_popen(&iptr);
					if (ptr == NULL)
						break;
					savedptr = ptr;
				}
				/* FALLTHROUGH */
			case 'H':
				if (ptr == NULL) {
					if (gethostname(tmp, sizeof tmp) != 0)
						fatal("gethostname");
					ptr = tmp;
				}
				/* FALLTHROUGH */
			case 'S':
				if (ptr == NULL)
					ptr = s->name;
				/* FALLTHROUGH */
			case 'T':
				if (ptr == NULL)
					ptr = wl->window->active->base.title;
				len = strlen(ptr);
				if ((size_t) n < len)
					len = n;
				if (optr + len >= out + (sizeof out) - 1)
					break;
				while (len > 0 && *ptr != '\0') {
					*optr++ = *ptr++;
					len--;
				}
				break;
			case '#':
				*optr++ = '#';
				break;
			}
			if (savedptr != NULL) {
				xfree(savedptr);
				savedptr = NULL;
			}
			break;
		default:
			*optr++ = ch;
			break;
		}
	}
	*optr = '\0';

	return (xstrdup(out));
}

char *
status_replace_popen(char **iptr)
{
	FILE	*f;
	char	*buf, *cmd, *ptr;
	int	lastesc;
	size_t	len;

	if (**iptr == '\0')
		return (NULL);
	if (**iptr == ')') {		/* no command given */
		(*iptr)++;
		return (NULL);
	}

	buf = NULL;

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
	if (**iptr == '\0')		/* no terminating ) */
		goto out;
	(*iptr)++;			/* skip final ) */
	cmd[len] = '\0';

	if ((f = popen(cmd, "r")) == NULL)
		goto out;

	if ((buf = fgetln(f, &len)) == NULL) {
		pclose(f);
		goto out;
	}
	if (buf[len - 1] == '\n') {
		buf[len - 1] = '\0';
		buf = xstrdup(buf);
	} else {
		ptr = xmalloc(len + 1);
		memcpy(ptr, buf, len);
		ptr[len] = '\0';
		buf = ptr;
	}
	pclose(f);

out:
	xfree(cmd);
	return (buf);
}

size_t
status_width(struct winlink *wl)
{
	return (xsnprintf(NULL, 0, "%d:%s ", wl->idx, wl->window->name));
}

char *
status_print(struct session *s, struct winlink *wl, struct grid_cell *gc)
{
	char   *text, flag;
	u_char	fg, bg, attr;

	fg = options_get_number(&wl->window->options, "window-status-fg");
	if (fg != 8)
		gc->fg = fg;
	bg = options_get_number(&wl->window->options, "window-status-bg");
	if (bg != 8)
		gc->bg = bg;
	attr = options_get_number(&wl->window->options, "window-status-attr");
	if (attr != 0)
		gc->attr = attr;

	flag = ' ';
 	if (wl == SLIST_FIRST(&s->lastw))
		flag = '-';
	if (wl == s->curw)
		flag = '*';

	if (session_alert_has(s, wl, WINDOW_ACTIVITY)) {
		flag = '#';
		gc->attr ^= GRID_ATTR_REVERSE;
	} else if (session_alert_has(s, wl, WINDOW_BELL)) {
		flag = '!';
		gc->attr ^= GRID_ATTR_REVERSE;
	} else if (session_alert_has(s, wl, WINDOW_CONTENT)) {
		flag = '+';
		gc->attr ^= GRID_ATTR_REVERSE;
	}

	xasprintf(&text, "%d:%s%c", wl->idx, wl->window->name, flag);
	return (text);
}

void
status_message_set(struct client *c, const char *msg)
{
	struct timeval	tv;
	int		delay;

	delay = options_get_number(&c->session->options, "display-time");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	c->message_string = xstrdup(msg);
	if (gettimeofday(&c->message_timer, NULL) != 0)
		fatal("gettimeofday");
	timeradd(&c->message_timer, &tv, &c->message_timer);

	c->tty.flags |= (TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_STATUS;
}

void
status_message_clear(struct client *c)
{
	if (c->message_string == NULL)
		return;

	xfree(c->message_string);
	c->message_string = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW;
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

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_status, &c->status, sizeof old_status);
	screen_init(&c->status, c->tty.sx, 1, 0);

	len = strlen(c->message_string);
	if (len > c->tty.sx)
		len = c->tty.sx;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.bg = options_get_number(&s->options, "message-fg");
	gc.fg = options_get_number(&s->options, "message-bg");
	gc.attr |= options_get_number(&s->options, "message-attr");

	screen_write_start(&ctx, NULL, &c->status);

	screen_write_cursormove(&ctx, 0, 0);
	screen_write_puts(&ctx, &gc, "%.*s", (int) len, c->message_string);
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

void
status_prompt_set(struct client *c,
    const char *msg, int (*fn)(void *, const char *), void *data, int flags)
{
	c->prompt_string = xstrdup(msg);

	c->prompt_buffer = xstrdup("");
	c->prompt_index = 0;

	c->prompt_callback = fn;
	c->prompt_data = data;

	c->prompt_hindex = 0;

	c->prompt_flags = flags;

	mode_key_init(&c->prompt_mdata,
	    options_get_number(&c->session->options, "status-keys"),
	    MODEKEY_CANEDIT);

	c->tty.flags |= (TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_STATUS;
}

void
status_prompt_clear(struct client *c)
{
	if (c->prompt_string == NULL)
		return;

	mode_key_free(&c->prompt_mdata);

	xfree(c->prompt_string);
	c->prompt_string = NULL;

	xfree(c->prompt_buffer);
	c->prompt_buffer = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_REDRAW;
}

/* Draw client prompt on status line of present else on last line. */
int
status_prompt_redraw(struct client *c)
{
	struct screen_write_ctx	ctx;
	struct session		       *s = c->session;
	struct screen		        old_status;
	size_t			        i, size, left, len, offset, n;
	char				ch;
	struct grid_cell		gc;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_status, &c->status, sizeof old_status);
	screen_init(&c->status, c->tty.sx, 1, 0);
	offset = 0;

	len = strlen(c->prompt_string);
	if (len > c->tty.sx)
		len = c->tty.sx;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.bg = options_get_number(&s->options, "message-fg");
	gc.fg = options_get_number(&s->options, "message-bg");
	gc.attr |= options_get_number(&s->options, "message-attr");

	screen_write_start(&ctx, NULL, &c->status);

	screen_write_cursormove(&ctx, 0, 0);
	screen_write_puts(&ctx, &gc, "%.*s", (int) len, c->prompt_string);

	left = c->tty.sx - len;
	if (left != 0) {
		if (c->prompt_index < left)
			size = strlen(c->prompt_buffer);
		else {
			offset = c->prompt_index - left - 1;
			if (c->prompt_index == strlen(c->prompt_buffer))
				left--;
			size = left;
		}
		if (c->prompt_flags & PROMPT_HIDDEN) {
			n = strlen(c->prompt_buffer);
			if (n > left)
				n = left;
			for (i = 0; i < n; i++)
				screen_write_putc(&ctx, &gc, '*');
		} else {
			screen_write_puts(&ctx, &gc,
			    "%.*s", (int) left, c->prompt_buffer + offset);
		}

		for (i = len + size; i < c->tty.sx; i++)
			screen_write_putc(&ctx, &gc, ' ');
	}

	/* Draw a fake cursor. */
	screen_write_cursormove(&ctx, len + c->prompt_index - offset, 0);
	if (c->prompt_index == strlen(c->prompt_buffer))
		ch = ' ';
	else {
		if (c->prompt_flags & PROMPT_HIDDEN)
			ch = '*';
		else
			ch = c->prompt_buffer[c->prompt_index];
	}
	if (ch == '\0')
		ch = ' ';
	gc.attr ^= GRID_ATTR_REVERSE;
	screen_write_putc(&ctx, &gc, ch);

	screen_write_stop(&ctx);

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
	struct paste_buffer	*pb;
	char   			*s, *first, *last, word[64];
	size_t			 size, n, off, idx;

	size = strlen(c->prompt_buffer);
	switch (mode_key_lookup(&c->prompt_mdata, key)) {
	case MODEKEYCMD_LEFT:
		if (c->prompt_index > 0) {
			c->prompt_index--;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYCMD_RIGHT:
		if (c->prompt_index < size) {
			c->prompt_index++;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYCMD_STARTOFLINE:
		if (c->prompt_index != 0) {
			c->prompt_index = 0;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYCMD_ENDOFLINE:
		if (c->prompt_index != size) {
			c->prompt_index = size;
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYCMD_COMPLETE:
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

		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYCMD_BACKSPACE:
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
 	case MODEKEYCMD_DELETE:
		if (c->prompt_index != size) {
			memmove(c->prompt_buffer + c->prompt_index,
			    c->prompt_buffer + c->prompt_index + 1,
			    size + 1 - c->prompt_index);
			c->flags |= CLIENT_STATUS;
		}
		break;
	case MODEKEYCMD_UP:
		if (server_locked)
			break;

		if (ARRAY_LENGTH(&c->prompt_hdata) == 0)
			break;
	       	xfree(c->prompt_buffer);

		c->prompt_buffer = xstrdup(ARRAY_ITEM(&c->prompt_hdata,
		    ARRAY_LENGTH(&c->prompt_hdata) - 1 - c->prompt_hindex));
		if (c->prompt_hindex != ARRAY_LENGTH(&c->prompt_hdata) - 1)
			c->prompt_hindex++;

		c->prompt_index = strlen(c->prompt_buffer);
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYCMD_DOWN:
		if (server_locked)
			break;

		xfree(c->prompt_buffer);

		if (c->prompt_hindex != 0) {
			c->prompt_hindex--;
			c->prompt_buffer = xstrdup(ARRAY_ITEM(
			    &c->prompt_hdata, ARRAY_LENGTH(
			    &c->prompt_hdata) - 1 - c->prompt_hindex));
		} else
			c->prompt_buffer = xstrdup("");

		c->prompt_index = strlen(c->prompt_buffer);
		c->flags |= CLIENT_STATUS;
		break;
	case MODEKEYCMD_PASTE:
		if ((pb = paste_get_top(&c->session->buffers)) == NULL)
			break;
		if ((last = strchr(pb->data, '\n')) == NULL)
			last = strchr(pb->data, '\0');
		n = last - pb->data;

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
 	case MODEKEYCMD_CHOOSE:
		if (*c->prompt_buffer != '\0') {
			status_prompt_add_history(c);
			if (c->prompt_callback(
			    c->prompt_data, c->prompt_buffer) == 0)
				status_prompt_clear(c);
			break;
		}
		/* FALLTHROUGH */
	case MODEKEYCMD_QUIT:
		if (c->prompt_callback(c->prompt_data, NULL) == 0)
			status_prompt_clear(c);
		break;
	case MODEKEYCMD_OTHERKEY:
		if (key < 32 || key > 126)
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
			if (c->prompt_callback(
			    c->prompt_data, c->prompt_buffer) == 0)
				status_prompt_clear(c);
		}

		c->flags |= CLIENT_STATUS;
		break;
	default:
		break;
	}
}

/* Add line to the history. */
void
status_prompt_add_history(struct client *c)
{
	if (server_locked)
		return;

	if (ARRAY_LENGTH(&c->prompt_hdata) > 0 &&
	    strcmp(ARRAY_LAST(&c->prompt_hdata), c->prompt_buffer) == 0)
		return;

	if (ARRAY_LENGTH(&c->prompt_hdata) == PROMPT_HISTORY) {
		xfree(ARRAY_FIRST(&c->prompt_hdata));
		ARRAY_REMOVE(&c->prompt_hdata, 0);
	}

	ARRAY_ADD(&c->prompt_hdata, xstrdup(c->prompt_buffer));
}

/* Complete word. */
char *
status_prompt_complete(const char *s)
{
	const struct cmd_entry 	      **cmdent;
	const struct set_option_entry  *optent;
	ARRAY_DECL(, const char *)	list;
	char			       *prefix, *s2;
	u_int			 	i;
	size_t			 	j;

	if (*s == '\0')
		return (NULL);

	/* First, build a list of all the possible matches. */
	ARRAY_INIT(&list);
	for (cmdent = cmd_table; *cmdent != NULL; cmdent++) {
		if (strncmp((*cmdent)->name, s, strlen(s)) == 0)
			ARRAY_ADD(&list, (*cmdent)->name);
	}
	for (i = 0; i < NSETOPTION; i++) {
		optent = &set_option_table[i];
		if (strncmp(optent->name, s, strlen(s)) == 0)
			ARRAY_ADD(&list, optent->name);
	}
	for (i = 0; i < NSETWINDOWOPTION; i++) {
		optent = &set_window_option_table[i];
		if (strncmp(optent->name, s, strlen(s)) == 0)
			ARRAY_ADD(&list, optent->name);
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
