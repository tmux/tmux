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

static void	 status_message_callback(int, short, void *);
static void	 status_timer_callback(int, short, void *);

static char	*status_prompt_find_history_file(void);
static const char *status_prompt_up_history(u_int *);
static const char *status_prompt_down_history(u_int *);
static void	 status_prompt_add_history(const char *);

static char    **status_prompt_complete_list(u_int *, const char *);
static char	*status_prompt_complete_prefix(char **, u_int);
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
	return (s->statuslines);
}

/* Get window at window list position. */
struct style_range *
status_get_range(struct client *c, u_int x, u_int y)
{
	struct status_line	*sl = &c->status;
	struct style_range	*sr;

	if (y >= nitems(sl->entries))
		return (NULL);
	TAILQ_FOREACH(sr, &sl->entries[y].ranges, entry) {
		if (x >= sr->start && x < sr->end)
			return (sr);
	}
	return (NULL);
}

/* Free all ranges. */
static void
status_free_ranges(struct style_ranges *srs)
{
	struct style_range	*sr, *sr1;

	TAILQ_FOREACH_SAFE(sr, srs, entry, sr1) {
		TAILQ_REMOVE(srs, sr, entry);
		free(sr);
	}
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
		TAILQ_INIT(&sl->entries[i].ranges);

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
		status_free_ranges(&sl->entries[i].ranges);
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
	struct status_line_entry	*sle;
	struct session			*s = c->session;
	struct screen_write_ctx		 ctx;
	struct grid_cell		 gc;
	u_int				 lines, i, n, width = c->tty.sx;
	int				 flags, force = 0, changed = 0;
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

	/* Set up default colour. */
	style_apply(&gc, s->options, "status-style");
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
	screen_write_start(&ctx, NULL, &sl->screen);

	/* Create format tree. */
	flags = FORMAT_STATUS;
	if (c->flags & CLIENT_STATUSFORCE)
		flags |= FORMAT_FORCE;
	ft = format_create(c, NULL, FORMAT_NONE, flags);
	format_defaults(ft, c, NULL, NULL, NULL);

	/* Write the status lines. */
	o = options_get(s->options, "status-format");
	if (o == NULL) {
		for (n = 0; n < width * lines; n++)
			screen_write_putc(&ctx, &gc, ' ');
	} else {
		for (i = 0; i < lines; i++) {
			screen_write_cursormove(&ctx, 0, i, 0);

			ov = options_array_get(o, i);
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

			status_free_ranges(&sle->ranges);
			format_draw(&ctx, &gc, width, expanded, &sle->ranges);

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

/* Set a status line message. */
void
status_message_set(struct client *c, const char *fmt, ...)
{
	struct timeval	tv;
	va_list		ap;
	int		delay;

	status_message_clear(c);
	status_push_screen(c);

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

	if (c->prompt_string == NULL)
		c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_ALLREDRAWFLAGS; /* was frozen and may have changed */

	status_pop_screen(c);
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
	size_t			 len;
	u_int			 lines, offset;
	struct grid_cell	 gc;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_screen, sl->active, sizeof old_screen);

	lines = status_line_size(c);
	if (lines <= 1)
		lines = 1;
	screen_init(sl->active, c->tty.sx, lines, 0);

	len = screen_write_strlen("%s", c->message_string);
	if (len > c->tty.sx)
		len = c->tty.sx;

	style_apply(&gc, s->options, "message-style");

	screen_write_start(&ctx, NULL, sl->active);
	screen_write_fast_copy(&ctx, &sl->screen, 0, 0, c->tty.sx, lines - 1);
	screen_write_cursormove(&ctx, 0, lines - 1, 0);
	for (offset = 0; offset < c->tty.sx; offset++)
		screen_write_putc(&ctx, &gc, ' ');
	screen_write_cursormove(&ctx, 0, lines - 1, 0);
	screen_write_nputs(&ctx, len, &gc, "%s", c->message_string);
	screen_write_stop(&ctx);

	if (grid_compare(sl->active->grid, old_screen.grid) == 0) {
		screen_free(&old_screen);
		return (0);
	}
	screen_free(&old_screen);
	return (1);
}

/* Enable status line prompt. */
void
status_prompt_set(struct client *c, const char *msg, const char *input,
    prompt_input_cb inputcb, prompt_free_cb freecb, void *data, int flags)
{
	struct format_tree	*ft;
	char			*tmp, *cp;

	ft = format_create(c, NULL, FORMAT_NONE, 0);
	format_defaults(ft, c, NULL, NULL, NULL);

	if (input == NULL)
		input = "";
	if (flags & PROMPT_NOFORMAT)
		tmp = xstrdup(input);
	else
		tmp = format_expand_time(ft, input);

	status_message_clear(c);
	status_prompt_clear(c);
	status_push_screen(c);

	c->prompt_string = format_expand_time(ft, msg);

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
	c->flags |= CLIENT_REDRAWSTATUS;

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

	free(c->prompt_saved);
	c->prompt_saved = NULL;

	c->tty.flags &= ~(TTY_NOCURSOR|TTY_FREEZE);
	c->flags |= CLIENT_ALLREDRAWFLAGS; /* was frozen and may have changed */

	status_pop_screen(c);
}

/* Update status line prompt with a new prompt string. */
void
status_prompt_update(struct client *c, const char *msg, const char *input)
{
	struct format_tree	*ft;
	char			*tmp;

	ft = format_create(c, NULL, FORMAT_NONE, 0);
	format_defaults(ft, c, NULL, NULL, NULL);

	tmp = format_expand_time(ft, input);

	free(c->prompt_string);
	c->prompt_string = format_expand_time(ft, msg);

	free(c->prompt_buffer);
	c->prompt_buffer = utf8_fromcstr(tmp);
	c->prompt_index = utf8_strlen(c->prompt_buffer);

	c->prompt_hindex = 0;

	c->flags |= CLIENT_REDRAWSTATUS;

	free(tmp);
	format_free(ft);
}

/* Draw client prompt on status line of present else on last line. */
int
status_prompt_redraw(struct client *c)
{
	struct status_line	*sl = &c->status;
	struct screen_write_ctx	 ctx;
	struct session		*s = c->session;
	struct screen		 old_screen;
	u_int			 i, lines, offset, left, start, width;
	u_int			 pcursor, pwidth;
	struct grid_cell	 gc, cursorgc;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_screen, sl->active, sizeof old_screen);

	lines = status_line_size(c);
	if (lines <= 1)
		lines = 1;
	screen_init(sl->active, c->tty.sx, lines, 0);

	if (c->prompt_mode == PROMPT_COMMAND)
		style_apply(&gc, s->options, "message-command-style");
	else
		style_apply(&gc, s->options, "message-style");

	memcpy(&cursorgc, &gc, sizeof cursorgc);
	cursorgc.attr ^= GRID_ATTR_REVERSE;

	start = screen_write_strlen("%s", c->prompt_string);
	if (start > c->tty.sx)
		start = c->tty.sx;

	screen_write_start(&ctx, NULL, sl->active);
	screen_write_fast_copy(&ctx, &sl->screen, 0, 0, c->tty.sx, lines - 1);
	screen_write_cursormove(&ctx, 0, lines - 1, 0);
	for (offset = 0; offset < c->tty.sx; offset++)
		screen_write_putc(&ctx, &gc, ' ');
	screen_write_cursormove(&ctx, 0, lines - 1, 0);
	screen_write_nputs(&ctx, start, &gc, "%s", c->prompt_string);
	screen_write_cursormove(&ctx, start, lines - 1, 0);

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
	if (sl->active->cx < screen_size_x(sl->active) && c->prompt_index >= i)
		screen_write_putc(&ctx, &cursorgc, ' ');

finished:
	screen_write_stop(&ctx);

	if (grid_compare(sl->active->grid, old_screen.grid) == 0) {
		screen_free(&old_screen);
		return (0);
	}
	screen_free(&old_screen);
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
		case '\007': /* C-g */
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
			c->flags |= CLIENT_REDRAWSTATUS;
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
		c->flags |= CLIENT_REDRAWSTATUS;
		break; /* switch mode and... */
	case 'S':
		c->prompt_mode = PROMPT_ENTRY;
		c->flags |= CLIENT_REDRAWSTATUS;
		*new_key = '\025'; /* C-u */
		return (1);
	case 'i':
	case '\033': /* Escape */
		c->prompt_mode = PROMPT_ENTRY;
		c->flags |= CLIENT_REDRAWSTATUS;
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
	case 'q':
		*new_key = '\003'; /* C-c */
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

/* Paste into prompt. */
static int
status_prompt_paste(struct client *c)
{
	struct paste_buffer	*pb;
	const char		*bufdata;
	size_t			 size, n, bufsize;
	u_int			 i;
	struct utf8_data	*ud, *udp;
	enum utf8_state		 more;

	size = utf8_strlen(c->prompt_buffer);
	if (c->prompt_saved != NULL) {
		ud = c->prompt_saved;
		n = utf8_strlen(c->prompt_saved);
	} else {
		if ((pb = paste_get_top(NULL)) == NULL)
			return (0);
		bufdata = paste_buffer_data(pb, &bufsize);
		ud = xreallocarray(NULL, bufsize + 1, sizeof *ud);
		udp = ud;
		for (i = 0; i != bufsize; /* nothing */) {
			more = utf8_open(udp, bufdata[i]);
			if (more == UTF8_MORE) {
				while (++i != bufsize && more == UTF8_MORE)
					more = utf8_append(udp, bufdata[i]);
				if (more == UTF8_DONE) {
					udp++;
					continue;
				}
				i -= udp->have;
			}
			if (bufdata[i] <= 31 || bufdata[i] >= 127)
				break;
			utf8_set(udp, bufdata[i]);
			udp++;
			i++;
		}
		udp->size = 0;
		n = udp - ud;
	}
	if (n == 0)
		return (0);

	c->prompt_buffer = xreallocarray(c->prompt_buffer, size + n + 1,
	    sizeof *c->prompt_buffer);
	if (c->prompt_index == size) {
		memcpy(c->prompt_buffer + c->prompt_index, ud,
		    n * sizeof *c->prompt_buffer);
		c->prompt_index += n;
		c->prompt_buffer[c->prompt_index].size = 0;
	} else {
		memmove(c->prompt_buffer + c->prompt_index + n,
		    c->prompt_buffer + c->prompt_index,
		    (size + 1 - c->prompt_index) * sizeof *c->prompt_buffer);
		memcpy(c->prompt_buffer + c->prompt_index, ud,
		    n * sizeof *c->prompt_buffer);
		c->prompt_index += n;
	}

	if (ud != c->prompt_saved)
		free(ud);
	return (1);
}

/* Handle keys in prompt. */
int
status_prompt_key(struct client *c, key_code key)
{
	struct options		*oo = c->session->options;
	char			*s, *cp, word[64], prefix = '=';
	const char		*histstr, *ws = NULL, *keystring;
	size_t			 size, n, off, idx, used;
	struct utf8_data	 tmp, *first, *last, *ud;
	int			 keys;

	if (c->prompt_flags & PROMPT_KEY) {
		keystring = key_string_lookup_key(key);
		c->prompt_inputcb(c, c->prompt_data, keystring, 1);
		status_prompt_clear(c);
		return (0);
	}
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
	key &= ~KEYC_XTERM;

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

		free(c->prompt_saved);
		c->prompt_saved = xcalloc(sizeof *c->prompt_buffer,
		    (c->prompt_index - idx) + 1);
		memcpy(c->prompt_saved, c->prompt_buffer + idx,
		    (c->prompt_index - idx) * sizeof *c->prompt_buffer);

		memmove(c->prompt_buffer + idx,
		    c->prompt_buffer + c->prompt_index,
		    (size + 1 - c->prompt_index) *
		    sizeof *c->prompt_buffer);
		memset(c->prompt_buffer + size - (c->prompt_index - idx),
		    '\0', (c->prompt_index - idx) * sizeof *c->prompt_buffer);
		c->prompt_index = idx;

		goto changed;
	case 'f'|KEYC_ESCAPE:
	case KEYC_RIGHT|KEYC_CTRL:
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
	case KEYC_LEFT|KEYC_CTRL:
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
		if (status_prompt_paste(c))
			goto changed;
		break;
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

	c->flags |= CLIENT_REDRAWSTATUS;
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
	c->flags |= CLIENT_REDRAWSTATUS;
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
char **
status_prompt_complete_list(u_int *size, const char *s)
{
	char					**list = NULL;
	const char				**layout, *value, *cp;
	const struct cmd_entry			**cmdent;
	const struct options_table_entry	 *oe;
	u_int					  idx;
	size_t					  slen = strlen(s), valuelen;
	struct options_entry			 *o;
	struct options_array_item		 *a;
	const char				 *layouts[] = {
		"even-horizontal", "even-vertical", "main-horizontal",
		"main-vertical", "tiled", NULL
	};

	*size = 0;
	for (cmdent = cmd_table; *cmdent != NULL; cmdent++) {
		if (strncmp((*cmdent)->name, s, slen) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = xstrdup((*cmdent)->name);
		}
	}
	for (oe = options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, slen) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = xstrdup(oe->name);
		}
	}
	for (layout = layouts; *layout != NULL; layout++) {
		if (strncmp(*layout, s, slen) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = xstrdup(*layout);
		}
	}
	o = options_get_only(global_options, "command-alias");
	if (o != NULL) {
		a = options_array_first(o);
		while (a != NULL) {
			value = options_array_item_value(a)->string;
			if ((cp = strchr(value, '=')) == NULL)
				goto next;
			valuelen = cp - value;
			if (slen > valuelen || strncmp(value, s, slen) != 0)
				goto next;

			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = xstrndup(value, valuelen);

		next:
			a = options_array_next(a);
		}
	}
	for (idx = 0; idx < (*size); idx++)
		log_debug("complete %u: %s", idx, list[idx]);
	return (list);
}

/* Find longest prefix. */
static char *
status_prompt_complete_prefix(char **list, u_int size)
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
	char		**list = NULL;
	const char	 *colon;
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
		for (i = 0; i < size; i++)
			free(list[i]);
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
