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

static char	*status_prompt_complete(struct client *, const char *, u_int);
static char	*status_prompt_complete_window_menu(struct client *,
		     struct session *, const char *, u_int, char);

struct status_prompt_menu {
	struct client	 *c;
	u_int		  start;
	u_int		  size;
	char		**list;
	char		  flag;
};

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
	if (s == NULL)
		return (options_get_number(global_s_options, "status"));
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
	if (fg != 8)
		gc.fg = fg;
	bg = options_get_number(s->options, "status-bg");
	if (bg != 8)
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
status_message_set(struct client *c, int delay, int ignore_styles,
    int ignore_keys, const char *fmt, ...)
{
	struct timeval	tv;
	va_list		ap;

	status_message_clear(c);
	status_push_screen(c);

	va_start(ap, fmt);
	xvasprintf(&c->message_string, fmt, ap);
	va_end(ap);

	server_add_message("%s message: %s", c->name, c->message_string);

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
	struct format_tree	*ft;

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

	ft = format_create_defaults(NULL, c, NULL, NULL, NULL);
	style_apply(&gc, s->options, "message-style", ft);
	format_free(ft);

	screen_write_start(&ctx, sl->active);
	screen_write_fast_copy(&ctx, &sl->screen, 0, 0, c->tty.sx, lines - 1);
	screen_write_cursormove(&ctx, 0, lines - 1, 0);
	for (offset = 0; offset < c->tty.sx; offset++)
		screen_write_putc(&ctx, &gc, ' ');
	screen_write_cursormove(&ctx, 0, lines - 1, 0);
	if (c->message_ignore_styles)
		screen_write_nputs(&ctx, len, &gc, "%s", c->message_string);
	else
		format_draw(&ctx, &gc, c->tty.sx, c->message_string, NULL);
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
status_prompt_set(struct client *c, struct cmd_find_state *fs,
    const char *msg, const char *input, prompt_input_cb inputcb,
    prompt_free_cb freecb, void *data, int flags)
{
	struct format_tree	*ft;
	char			*tmp;

	if (fs != NULL)
		ft = format_create_from_state(NULL, c, fs);
	else
		ft = format_create_defaults(NULL, c, NULL, NULL, NULL);

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

	if (flags & PROMPT_INCREMENTAL) {
		c->prompt_last = xstrdup(tmp);
		c->prompt_buffer = utf8_fromcstr("");
	} else {
		c->prompt_last = NULL;
		c->prompt_buffer = utf8_fromcstr(tmp);
	}
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

	if (flags & PROMPT_INCREMENTAL)
		c->prompt_inputcb(c, c->prompt_data, "=", 0);

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

	free(c->prompt_last);
	c->prompt_last = NULL;

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
	struct format_tree	*ft;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_screen, sl->active, sizeof old_screen);

	lines = status_line_size(c);
	if (lines <= 1)
		lines = 1;
	screen_init(sl->active, c->tty.sx, lines, 0);

	ft = format_create_defaults(NULL, c, NULL, NULL, NULL);
	if (c->prompt_mode == PROMPT_COMMAND)
		style_apply(&gc, s->options, "message-command-style", ft);
	else
		style_apply(&gc, s->options, "message-style", ft);
	format_free(ft);

	memcpy(&cursorgc, &gc, sizeof cursorgc);
	cursorgc.attr ^= GRID_ATTR_REVERSE;

	start = screen_write_strlen("%s", c->prompt_string);
	if (start > c->tty.sx)
		start = c->tty.sx;

	screen_write_start(&ctx, sl->active);
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
		*new_key = 'b'|KEYC_META;
		return (1);
	case 'd':
		*new_key = '\025';
		return (1);
	case 'e':
	case 'E':
	case 'w':
	case 'W':
		*new_key = 'f'|KEYC_META;
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

/* Finish completion. */
static int
status_prompt_replace_complete(struct client *c, const char *s)
{
	char			 word[64], *allocated = NULL;
	size_t			 size, n, off, idx, used;
	struct utf8_data	*first, *last, *ud;

	/* Work out where the cursor currently is. */
	idx = c->prompt_index;
	if (idx != 0)
		idx--;
	size = utf8_strlen(c->prompt_buffer);

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
	if (last < first)
		return (0);
	if (s == NULL) {
		used = 0;
		for (ud = first; ud < last; ud++) {
			if (used + ud->size >= sizeof word)
				break;
			memcpy(word + used, ud->data, ud->size);
			used += ud->size;
		}
		if (ud != last)
			return (0);
		word[used] = '\0';
	}

	/* Try to complete it. */
	if (s == NULL) {
		allocated = status_prompt_complete(c, word,
		    first - c->prompt_buffer);
		if (allocated == NULL)
			return (0);
		s = allocated;
	}

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

	free(allocated);
	return (1);
}

/* Handle keys in prompt. */
int
status_prompt_key(struct client *c, key_code key)
{
	struct options		*oo = c->session->options;
	char			*s, *cp, prefix = '=';
	const char		*histstr, *ws = NULL, *keystring;
	size_t			 size, idx;
	struct utf8_data	 tmp;
	int			 keys;

	if (c->prompt_flags & PROMPT_KEY) {
		keystring = key_string_lookup_key(key, 0);
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
	key &= ~KEYC_MASK_FLAGS;

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
		if (status_prompt_replace_complete(c, NULL))
			goto changed;
		break;
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
	case 'f'|KEYC_META:
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
	case 'b'|KEYC_META:
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
		if (~c->prompt_flags & PROMPT_INCREMENTAL)
			break;
		if (c->prompt_buffer[0].size == 0) {
			prefix = '=';
			free (c->prompt_buffer);
			c->prompt_buffer = utf8_fromcstr(c->prompt_last);
			c->prompt_index = utf8_strlen(c->prompt_buffer);
		} else
			prefix = '-';
		goto changed;
	case '\023': /* C-s */
		if (~c->prompt_flags & PROMPT_INCREMENTAL)
			break;
		if (c->prompt_buffer[0].size == 0) {
			prefix = '=';
			free (c->prompt_buffer);
			c->prompt_buffer = utf8_fromcstr(c->prompt_last);
			c->prompt_index = utf8_strlen(c->prompt_buffer);
		} else
			prefix = '+';
		goto changed;
	default:
		goto append_key;
	}

	c->flags |= CLIENT_REDRAWSTATUS;
	return (0);

append_key:
	if (key <= 0x1f || (key >= KEYC_BASE && key < KEYC_BASE_END))
		return (0);
	if (key <= 0x7f)
		utf8_set(&tmp, key);
	else
		utf8_to_data(key, &tmp);

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
		if (utf8_strlen(c->prompt_buffer) != 1)
			status_prompt_clear(c);
		else {
			s = utf8_tocstr(c->prompt_buffer);
			if (c->prompt_inputcb(c, c->prompt_data, s, 1) == 0)
				status_prompt_clear(c);
			free(s);
		}
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
static char **
status_prompt_complete_list(u_int *size, const char *s, int at_start)
{
	char					**list = NULL;
	const char				**layout, *value, *cp;
	const struct cmd_entry			**cmdent;
	const struct options_table_entry	 *oe;
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
		if ((*cmdent)->alias != NULL &&
		    strncmp((*cmdent)->alias, s, slen) == 0) {
			list = xreallocarray(list, (*size) + 1, sizeof *list);
			list[(*size)++] = xstrdup((*cmdent)->alias);
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
	if (at_start)
		return (list);

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
	return (list);
}

/* Find longest prefix. */
static char *
status_prompt_complete_prefix(char **list, u_int size)
{
	char	 *out;
	u_int	  i;
	size_t	  j;

	if (list == NULL || size == 0)
		return (NULL);
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

/* Complete word menu callback. */
static void
status_prompt_menu_callback(__unused struct menu *menu, u_int idx, key_code key,
    void *data)
{
	struct status_prompt_menu	*spm = data;
	struct client			*c = spm->c;
	u_int				 i;
	char				*s;

	if (key != KEYC_NONE) {
		idx += spm->start;
		if (spm->flag == '\0')
			s = xstrdup(spm->list[idx]);
		else
			xasprintf(&s, "-%c%s", spm->flag, spm->list[idx]);
		if (c->prompt_flags & PROMPT_WINDOW) {
			free(c->prompt_buffer);
			c->prompt_buffer = utf8_fromcstr(s);
			c->prompt_index = utf8_strlen(c->prompt_buffer);
			c->flags |= CLIENT_REDRAWSTATUS;
		} else if (status_prompt_replace_complete(c, s))
			c->flags |= CLIENT_REDRAWSTATUS;
		free(s);
	}

	for (i = 0; i < spm->size; i++)
		free(spm->list[i]);
	free(spm->list);
}

/* Show complete word menu. */
static int
status_prompt_complete_list_menu(struct client *c, char **list, u_int size,
    u_int offset, char flag)
{
	struct menu			*menu;
	struct menu_item		 item;
	struct status_prompt_menu	*spm;
	u_int				 lines = status_line_size(c), height, i;
	u_int				 py;

	if (size <= 1)
		return (0);
	if (c->tty.sy - lines < 3)
		return (0);

	spm = xmalloc(sizeof *spm);
	spm->c = c;
	spm->size = size;
	spm->list = list;
	spm->flag = flag;

	height = c->tty.sy - lines - 2;
	if (height > 10)
		height = 10;
	if (height > size)
		height = size;
	spm->start = size - height;

	menu = menu_create("");
	for (i = spm->start; i < size; i++) {
		item.name = list[i];
		item.key = '0' + (i - spm->start);
		item.command = NULL;
		menu_add_item(menu, &item, NULL, NULL, NULL);
	}

	if (options_get_number(c->session->options, "status-position") == 0)
		py = lines;
	else
		py = c->tty.sy - 3 - height;
	offset += utf8_cstrwidth(c->prompt_string);
	if (offset > 2)
		offset -= 2;
	else
		offset = 0;

	if (menu_display(menu, MENU_NOMOUSE|MENU_TAB, NULL, offset,
	    py, c, NULL, status_prompt_menu_callback, spm) != 0) {
		menu_free(menu);
		free(spm);
		return (0);
	}
	return (1);
}

/* Show complete word menu. */
static char *
status_prompt_complete_window_menu(struct client *c, struct session *s,
    const char *word, u_int offset, char flag)
{
	struct menu			 *menu;
	struct menu_item		  item;
	struct status_prompt_menu	 *spm;
	struct winlink			 *wl;
	char				**list = NULL, *tmp;
	u_int				  lines = status_line_size(c), height;
	u_int				  py, size = 0;

	if (c->tty.sy - lines < 3)
		return (NULL);

	spm = xmalloc(sizeof *spm);
	spm->c = c;
	spm->flag = flag;

	height = c->tty.sy - lines - 2;
	if (height > 10)
		height = 10;
	spm->start = 0;

	menu = menu_create("");
	RB_FOREACH(wl, winlinks, &s->windows) {
		if (word != NULL && *word != '\0') {
			xasprintf(&tmp, "%d", wl->idx);
			if (strncmp(tmp, word, strlen(word)) != 0) {
				free(tmp);
				continue;
			}
			free(tmp);
		}

		list = xreallocarray(list, size + 1, sizeof *list);
		if (c->prompt_flags & PROMPT_WINDOW) {
			xasprintf(&tmp, "%d (%s)", wl->idx, wl->window->name);
			xasprintf(&list[size++], "%d", wl->idx);
		} else {
			xasprintf(&tmp, "%s:%d (%s)", s->name, wl->idx,
			    wl->window->name);
			xasprintf(&list[size++], "%s:%d", s->name, wl->idx);
		}
		item.name = tmp;
		item.key = '0' + size - 1;
		item.command = NULL;
		menu_add_item(menu, &item, NULL, NULL, NULL);
		free(tmp);

		if (size == height)
			break;
	}
	if (size == 0) {
		menu_free(menu);
		return (NULL);
	}
	if (size == 1) {
		menu_free(menu);
		if (flag != '\0') {
			xasprintf(&tmp, "-%c%s", flag, list[0]);
			free(list[0]);
		} else
			tmp = list[0];
		free(list);
		return (tmp);
	}
	if (height > size)
		height = size;

	spm->size = size;
	spm->list = list;

	if (options_get_number(c->session->options, "status-position") == 0)
		py = lines;
	else
		py = c->tty.sy - 3 - height;
	offset += utf8_cstrwidth(c->prompt_string);
	if (offset > 2)
		offset -= 2;
	else
		offset = 0;

	if (menu_display(menu, MENU_NOMOUSE|MENU_TAB, NULL, offset,
	    py, c, NULL, status_prompt_menu_callback, spm) != 0) {
		menu_free(menu);
		free(spm);
		return (NULL);
	}
	return (NULL);
}

/* Sort complete list. */
static int
status_prompt_complete_sort(const void *a, const void *b)
{
	const char	**aa = (const char **)a, **bb = (const char **)b;

	return (strcmp(*aa, *bb));
}

/* Complete a session. */
static char *
status_prompt_complete_session(char ***list, u_int *size, const char *s,
    char flag)
{
	struct session	*loop;
	char		*out, *tmp, n[11];

	RB_FOREACH(loop, sessions, &sessions) {
		if (*s == '\0' || strncmp(loop->name, s, strlen(s)) == 0) {
			*list = xreallocarray(*list, (*size) + 2,
			    sizeof **list);
			xasprintf(&(*list)[(*size)++], "%s:", loop->name);
		} else if (*s == '$') {
			xsnprintf(n, sizeof n, "%u", loop->id);
			if (s[1] == '\0' ||
			    strncmp(n, s + 1, strlen(s) - 1) == 0) {
				*list = xreallocarray(*list, (*size) + 2,
				    sizeof **list);
				xasprintf(&(*list)[(*size)++], "$%s:", n);
			}
		}
	}
	out = status_prompt_complete_prefix(*list, *size);
	if (out != NULL && flag != '\0') {
		xasprintf(&tmp, "-%c%s", flag, out);
		free(out);
		out = tmp;
	}
	return (out);
}

/* Complete word. */
static char *
status_prompt_complete(struct client *c, const char *word, u_int offset)
{
	struct session	 *session;
	const char	 *s, *colon;
	char		**list = NULL, *copy = NULL, *out = NULL;
	char		  flag = '\0';
	u_int		  size = 0, i;

	if (*word == '\0' &&
	    ((c->prompt_flags & (PROMPT_TARGET|PROMPT_WINDOW)) == 0))
		return (NULL);

	if (((c->prompt_flags & (PROMPT_TARGET|PROMPT_WINDOW)) == 0) &&
	    strncmp(word, "-t", 2) != 0 &&
	    strncmp(word, "-s", 2) != 0) {
		list = status_prompt_complete_list(&size, word, offset == 0);
		if (size == 0)
			out = NULL;
		else if (size == 1)
			xasprintf(&out, "%s ", list[0]);
		else
			out = status_prompt_complete_prefix(list, size);
		goto found;
	}

	if (c->prompt_flags & (PROMPT_TARGET|PROMPT_WINDOW)) {
		s = word;
		flag = '\0';
	} else {
		s = word + 2;
		flag = word[1];
		offset += 2;
	}

	/* If this is a window completion, open the window menu. */
	if (c->prompt_flags & PROMPT_WINDOW) {
		out = status_prompt_complete_window_menu(c, c->session, s,
		    offset, '\0');
		goto found;
	}
	colon = strchr(s, ':');

	/* If there is no colon, complete as a session. */
	if (colon == NULL) {
		out = status_prompt_complete_session(&list, &size, s, flag);
		goto found;
	}

	/* If there is a colon but no period, find session and show a menu. */
	if (strchr(colon + 1, '.') == NULL) {
		if (*s == ':')
			session = c->session;
		else {
			copy = xstrdup(s);
			*strchr(copy, ':') = '\0';
			session = session_find(copy);
			free(copy);
			if (session == NULL)
				goto found;
		}
		out = status_prompt_complete_window_menu(c, session, colon + 1,
		    offset, flag);
		if (out == NULL)
			return (NULL);
	}

found:
	if (size != 0) {
		qsort(list, size, sizeof *list, status_prompt_complete_sort);
		for (i = 0; i < size; i++)
			log_debug("complete %u: %s", i, list[i]);
	}

	if (out != NULL && strcmp(word, out) == 0) {
		free(out);
		out = NULL;
	}
	if (out != NULL ||
	    !status_prompt_complete_list_menu(c, list, size, offset, flag)) {
		for (i = 0; i < size; i++)
			free(list[i]);
		free(list);
	}
	return (out);
}
