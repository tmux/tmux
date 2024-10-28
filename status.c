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
static const char *status_prompt_up_history(u_int *, u_int);
static const char *status_prompt_down_history(u_int *, u_int);
static void	 status_prompt_add_history(const char *, u_int);

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

static const char	*prompt_type_strings[] = {
	"command",
	"search",
	"target",
	"window-target"
};

/* Status prompt history. */
char		**status_prompt_hlist[PROMPT_NTYPES];
u_int		  status_prompt_hsize[PROMPT_NTYPES];

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

/* Add loaded history item to the appropriate list. */
static void
status_prompt_add_typed_history(char *line)
{
	char			*typestr;
	enum prompt_type	 type = PROMPT_TYPE_INVALID;

	typestr = strsep(&line, ":");
	if (line != NULL)
		type = status_prompt_type(typestr);
	if (type == PROMPT_TYPE_INVALID) {
		/*
		 * Invalid types are not expected, but this provides backward
		 * compatibility with old history files.
		 */
		if (line != NULL)
			*(--line) = ':';
		status_prompt_add_history(typestr, PROMPT_TYPE_COMMAND);
	} else
		status_prompt_add_history(line, type);
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
				status_prompt_add_typed_history(line);
			} else {
				tmp = xmalloc(length + 1);
				memcpy(tmp, line, length);
				tmp[length] = '\0';
				status_prompt_add_typed_history(tmp);
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
	u_int	 i, type;
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

	for (type = 0; type < PROMPT_NTYPES; type++) {
		for (i = 0; i < status_prompt_hsize[type]; i++) {
			fputs(prompt_type_strings[type], f);
			fputc(':', f);
			fputs(status_prompt_hlist[type][i], f);
			fputc('\n', f);
		}
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

/* Get the prompt line number for client's session. 1 means at the bottom. */
static u_int
status_prompt_line_at(struct client *c)
{
	struct session	*s = c->session;

	if (c->flags & (CLIENT_STATUSOFF|CLIENT_CONTROL))
		return (1);
	return (options_get_number(s->options, "message-line"));
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

/* Set a status line message. */
void
status_message_set(struct client *c, int delay, int ignore_styles,
    int ignore_keys, const char *fmt, ...)
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
	u_int			 lines, offset, messageline;
	struct grid_cell	 gc;
	struct format_tree	*ft;

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

	len = screen_write_strlen("%s", c->message_string);
	if (len > c->tty.sx)
		len = c->tty.sx;

	ft = format_create_defaults(NULL, c, NULL, NULL, NULL);
	style_apply(&gc, s->options, "message-style", ft);
	format_free(ft);

	screen_write_start(&ctx, sl->active);
	screen_write_fast_copy(&ctx, &sl->screen, 0, 0, c->tty.sx, lines);
	screen_write_cursormove(&ctx, 0, messageline, 0);
	for (offset = 0; offset < c->tty.sx; offset++)
		screen_write_putc(&ctx, &gc, ' ');
	screen_write_cursormove(&ctx, 0, messageline, 0);
	if (c->message_ignore_styles)
		screen_write_nputs(&ctx, len, &gc, "%s", c->message_string);
	else
		format_draw(&ctx, &gc, c->tty.sx, c->message_string, NULL, 0);
	screen_write_stop(&ctx);

	if (grid_compare(sl->active->grid, old_screen.grid) == 0) {
		screen_free(&old_screen);
		return (0);
	}
	screen_free(&old_screen);
	return (1);
}

/* Accept prompt immediately. */
static enum cmd_retval
status_prompt_accept(__unused struct cmdq_item *item, void *data)
{
	struct client	*c = data;

	if (c->prompt_string != NULL) {
		c->prompt_inputcb(c, c->prompt_data, "y", 1);
		status_prompt_clear(c);
	}
	return (CMD_RETURN_NORMAL);
}

/* Enable status line prompt. */
void
status_prompt_set(struct client *c, struct cmd_find_state *fs,
    const char *msg, const char *input, prompt_input_cb inputcb,
    prompt_free_cb freecb, void *data, int flags, enum prompt_type prompt_type)
{
	struct format_tree	*ft;
	char			*tmp;

	server_client_clear_overlay(c);

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

	memset(c->prompt_hindex, 0, sizeof c->prompt_hindex);

	c->prompt_flags = flags;
	c->prompt_type = prompt_type;
	c->prompt_mode = PROMPT_ENTRY;

	if (~flags & PROMPT_INCREMENTAL)
		c->tty.flags |= TTY_FREEZE;
	c->flags |= CLIENT_REDRAWSTATUS;

	if (flags & PROMPT_INCREMENTAL)
		c->prompt_inputcb(c, c->prompt_data, "=", 0);

	free(tmp);
	format_free(ft);

	if ((flags & PROMPT_SINGLE) && (flags & PROMPT_ACCEPT))
		cmdq_append(c, cmdq_get_callback(status_prompt_accept, c));
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

	memset(c->prompt_hindex, 0, sizeof c->prompt_hindex);

	c->flags |= CLIENT_REDRAWSTATUS;

	free(tmp);
	format_free(ft);
}

/* Redraw character. Return 1 if can continue redrawing, 0 otherwise. */
static int
status_prompt_redraw_character(struct screen_write_ctx *ctx, u_int offset,
    u_int pwidth, u_int *width, struct grid_cell *gc,
    const struct utf8_data *ud)
{
	u_char	ch;

	if (*width < offset) {
		*width += ud->width;
		return (1);
	}
	if (*width >= offset + pwidth)
		return (0);
	*width += ud->width;
	if (*width > offset + pwidth)
		return (0);

	ch = *ud->data;
	if (ud->size == 1 && (ch <= 0x1f || ch == 0x7f)) {
		gc->data.data[0] = '^';
		gc->data.data[1] = (ch == 0x7f) ? '?' : ch|0x40;
		gc->data.size = gc->data.have = 2;
		gc->data.width = 2;
	} else
		utf8_copy(&gc->data, ud);
	screen_write_cell(ctx, gc);
	return (1);
}

/*
 * Redraw quote indicator '^' if necessary. Return 1 if can continue redrawing,
 * 0 otherwise.
 */
static int
status_prompt_redraw_quote(const struct client *c, u_int pcursor,
    struct screen_write_ctx *ctx, u_int offset, u_int pwidth, u_int *width,
    struct grid_cell *gc)
{
	struct utf8_data	ud;

	if (c->prompt_flags & PROMPT_QUOTENEXT && ctx->s->cx == pcursor + 1) {
		utf8_set(&ud, '^');
		return (status_prompt_redraw_character(ctx, offset, pwidth,
		    width, gc, &ud));
	}
	return (1);
}

/* Draw client prompt on status line of present else on last line. */
int
status_prompt_redraw(struct client *c)
{
	struct status_line	*sl = &c->status;
	struct screen_write_ctx	 ctx;
	struct session		*s = c->session;
	struct screen		 old_screen;
	u_int			 i, lines, offset, left, start, width, n;
	u_int			 pcursor, pwidth, promptline;
	struct grid_cell	 gc;
	struct format_tree	*ft;

	if (c->tty.sx == 0 || c->tty.sy == 0)
		return (0);
	memcpy(&old_screen, sl->active, sizeof old_screen);

	lines = status_line_size(c);
	if (lines <= 1)
		lines = 1;
	screen_init(sl->active, c->tty.sx, lines, 0);

	n = options_get_number(s->options, "prompt-cursor-colour");
	sl->active->default_ccolour = n;
	n = options_get_number(s->options, "prompt-cursor-style");
	screen_set_cursor_style(n, &sl->active->default_cstyle,
	    &sl->active->default_mode);

	promptline = status_prompt_line_at(c);
	if (promptline > lines - 1)
		promptline = lines - 1;

	ft = format_create_defaults(NULL, c, NULL, NULL, NULL);
	if (c->prompt_mode == PROMPT_COMMAND)
		style_apply(&gc, s->options, "message-command-style", ft);
	else
		style_apply(&gc, s->options, "message-style", ft);
	format_free(ft);

	start = format_width(c->prompt_string);
	if (start > c->tty.sx)
		start = c->tty.sx;

	screen_write_start(&ctx, sl->active);
	screen_write_fast_copy(&ctx, &sl->screen, 0, 0, c->tty.sx, lines);
	screen_write_cursormove(&ctx, 0, promptline, 0);
	for (offset = 0; offset < c->tty.sx; offset++)
		screen_write_putc(&ctx, &gc, ' ');
	screen_write_cursormove(&ctx, 0, promptline, 0);
	format_draw(&ctx, &gc, start, c->prompt_string, NULL, 0);
	screen_write_cursormove(&ctx, start, promptline, 0);

	left = c->tty.sx - start;
	if (left == 0)
		goto finished;

	pcursor = utf8_strwidth(c->prompt_buffer, c->prompt_index);
	pwidth = utf8_strwidth(c->prompt_buffer, -1);
	if (c->prompt_flags & PROMPT_QUOTENEXT)
		pwidth++;
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
	c->prompt_cursor = start + pcursor - offset;

	width = 0;
	for (i = 0; c->prompt_buffer[i].size != 0; i++) {
		if (!status_prompt_redraw_quote(c, pcursor, &ctx, offset,
		    pwidth, &width, &gc))
			break;
		if (!status_prompt_redraw_character(&ctx, offset, pwidth,
		    &width, &gc, &c->prompt_buffer[i]))
			break;
	}
	status_prompt_redraw_quote(c, pcursor, &ctx, offset, pwidth, &width,
	    &gc);

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
 * Translate key from vi to emacs. Return 0 to drop key, 1 to process the key
 * as an emacs key; return 2 to append to the buffer.
 */
static int
status_prompt_translate_key(struct client *c, key_code key, key_code *new_key)
{
	if (c->prompt_mode == PROMPT_ENTRY) {
		switch (key) {
		case 'a'|KEYC_CTRL:
		case 'c'|KEYC_CTRL:
		case 'e'|KEYC_CTRL:
		case 'g'|KEYC_CTRL:
		case 'h'|KEYC_CTRL:
		case '\011': /* Tab */
		case 'k'|KEYC_CTRL:
		case 'n'|KEYC_CTRL:
		case 'p'|KEYC_CTRL:
		case 't'|KEYC_CTRL:
		case 'u'|KEYC_CTRL:
		case 'v'|KEYC_CTRL:
		case 'w'|KEYC_CTRL:
		case 'y'|KEYC_CTRL:
		case '\n':
		case '\r':
		case KEYC_LEFT|KEYC_CTRL:
		case KEYC_RIGHT|KEYC_CTRL:
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
	case KEYC_BSPACE:
		*new_key = KEYC_LEFT;
		return (1);
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
		*new_key = 'u'|KEYC_CTRL;
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
		*new_key = 'k'|KEYC_CTRL;
		return (1);
	case KEYC_BSPACE:
	case 'X':
		*new_key = KEYC_BSPACE;
		return (1);
	case 'b':
		*new_key = 'b'|KEYC_META;
		return (1);
	case 'B':
		*new_key = 'B'|KEYC_VI;
		return (1);
	case 'd':
		*new_key = 'u'|KEYC_CTRL;
		return (1);
	case 'e':
		*new_key = 'e'|KEYC_VI;
		return (1);
	case 'E':
		*new_key = 'E'|KEYC_VI;
		return (1);
	case 'w':
		*new_key = 'w'|KEYC_VI;
		return (1);
	case 'W':
		*new_key = 'W'|KEYC_VI;
		return (1);
	case 'p':
		*new_key = 'y'|KEYC_CTRL;
		return (1);
	case 'q':
		*new_key = 'c'|KEYC_CTRL;
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
	case 'h'|KEYC_CTRL:
	case 'c'|KEYC_CTRL:
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
		ud = udp = xreallocarray(NULL, bufsize + 1, sizeof *ud);
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
	if (n != 0) {
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
			    (size + 1 - c->prompt_index) *
			    sizeof *c->prompt_buffer);
			memcpy(c->prompt_buffer + c->prompt_index, ud,
			    n * sizeof *c->prompt_buffer);
			c->prompt_index += n;
		}
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

/* Prompt forward to the next beginning of a word. */
static void
status_prompt_forward_word(struct client *c, size_t size, int vi,
    const char *separators)
{
	size_t		 idx = c->prompt_index;
	int		 word_is_separators;

	/* In emacs mode, skip until the first non-whitespace character. */
	if (!vi)
		while (idx != size &&
		    status_prompt_space(&c->prompt_buffer[idx]))
			idx++;

	/* Can't move forward if we're already at the end. */
	if (idx == size) {
		c->prompt_index = idx;
		return;
	}

	/* Determine the current character class (separators or not). */
	word_is_separators = status_prompt_in_list(separators,
	    &c->prompt_buffer[idx]) &&
	    !status_prompt_space(&c->prompt_buffer[idx]);

	/* Skip ahead until the first space or opposite character class. */
	do {
		idx++;
		if (status_prompt_space(&c->prompt_buffer[idx])) {
			/* In vi mode, go to the start of the next word. */
			if (vi)
				while (idx != size &&
				    status_prompt_space(&c->prompt_buffer[idx]))
					idx++;
			break;
		}
	} while (idx != size && word_is_separators == status_prompt_in_list(
	    separators, &c->prompt_buffer[idx]));

	c->prompt_index = idx;
}

/* Prompt forward to the next end of a word. */
static void
status_prompt_end_word(struct client *c, size_t size, const char *separators)
{
	size_t		 idx = c->prompt_index;
	int		 word_is_separators;

	/* Can't move forward if we're already at the end. */
	if (idx == size)
		return;

	/* Find the next word. */
	do {
		idx++;
		if (idx == size) {
			c->prompt_index = idx;
			return;
		}
	} while (status_prompt_space(&c->prompt_buffer[idx]));

	/* Determine the character class (separators or not). */
	word_is_separators = status_prompt_in_list(separators,
	    &c->prompt_buffer[idx]);

	/* Skip ahead until the next space or opposite character class. */
	do {
		idx++;
		if (idx == size)
			break;
	} while (!status_prompt_space(&c->prompt_buffer[idx]) &&
	    word_is_separators == status_prompt_in_list(separators,
	    &c->prompt_buffer[idx]));

	/* Back up to the previous character to stop at the end of the word. */
	c->prompt_index = idx - 1;
}

/* Prompt backward to the previous beginning of a word. */
static void
status_prompt_backward_word(struct client *c, const char *separators)
{
	size_t	idx = c->prompt_index;
	int	word_is_separators;

	/* Find non-whitespace. */
	while (idx != 0) {
		--idx;
		if (!status_prompt_space(&c->prompt_buffer[idx]))
			break;
	}
	word_is_separators = status_prompt_in_list(separators,
	    &c->prompt_buffer[idx]);

	/* Find the character before the beginning of the word. */
	while (idx != 0) {
		--idx;
		if (status_prompt_space(&c->prompt_buffer[idx]) ||
		    word_is_separators != status_prompt_in_list(separators,
		    &c->prompt_buffer[idx])) {
			/* Go back to the word. */
			idx++;
			break;
		}
	}
	c->prompt_index = idx;
}

/* Handle keys in prompt. */
int
status_prompt_key(struct client *c, key_code key)
{
	struct options		*oo = c->session->options;
	char			*s, *cp, prefix = '=';
	const char		*histstr, *separators = NULL, *keystring;
	size_t			 size, idx;
	struct utf8_data	 tmp;
	int			 keys, word_is_separators;

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

	if (c->prompt_flags & (PROMPT_SINGLE|PROMPT_QUOTENEXT)) {
		if ((key & KEYC_MASK_KEY) == KEYC_BSPACE)
			key = 0x7f;
		else if ((key & KEYC_MASK_KEY) > 0x7f) {
			if (!KEYC_IS_UNICODE(key))
				return (0);
			key &= KEYC_MASK_KEY;
		} else
			key &= (key & KEYC_CTRL) ? 0x1f : KEYC_MASK_KEY;
		c->prompt_flags &= ~PROMPT_QUOTENEXT;
		goto append_key;
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
	case 'b'|KEYC_CTRL:
		if (c->prompt_index > 0) {
			c->prompt_index--;
			break;
		}
		break;
	case KEYC_RIGHT:
	case 'f'|KEYC_CTRL:
		if (c->prompt_index < size) {
			c->prompt_index++;
			break;
		}
		break;
	case KEYC_HOME:
	case 'a'|KEYC_CTRL:
		if (c->prompt_index != 0) {
			c->prompt_index = 0;
			break;
		}
		break;
	case KEYC_END:
	case 'e'|KEYC_CTRL:
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
	case 'h'|KEYC_CTRL:
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
	case 'd'|KEYC_CTRL:
		if (c->prompt_index != size) {
			memmove(c->prompt_buffer + c->prompt_index,
			    c->prompt_buffer + c->prompt_index + 1,
			    (size + 1 - c->prompt_index) *
			    sizeof *c->prompt_buffer);
			goto changed;
		}
		break;
	case 'u'|KEYC_CTRL:
		c->prompt_buffer[0].size = 0;
		c->prompt_index = 0;
		goto changed;
	case 'k'|KEYC_CTRL:
		if (c->prompt_index < size) {
			c->prompt_buffer[c->prompt_index].size = 0;
			goto changed;
		}
		break;
	case 'w'|KEYC_CTRL:
		separators = options_get_string(oo, "word-separators");
		idx = c->prompt_index;

		/* Find non-whitespace. */
		while (idx != 0) {
			idx--;
			if (!status_prompt_space(&c->prompt_buffer[idx]))
				break;
		}
		word_is_separators = status_prompt_in_list(separators,
		    &c->prompt_buffer[idx]);

		/* Find the character before the beginning of the word. */
		while (idx != 0) {
			idx--;
			if (status_prompt_space(&c->prompt_buffer[idx]) ||
			    word_is_separators != status_prompt_in_list(
			    separators, &c->prompt_buffer[idx])) {
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
	case KEYC_RIGHT|KEYC_CTRL:
	case 'f'|KEYC_META:
		separators = options_get_string(oo, "word-separators");
		status_prompt_forward_word(c, size, 0, separators);
		goto changed;
	case 'E'|KEYC_VI:
		status_prompt_end_word(c, size, "");
		goto changed;
	case 'e'|KEYC_VI:
		separators = options_get_string(oo, "word-separators");
		status_prompt_end_word(c, size, separators);
		goto changed;
	case 'W'|KEYC_VI:
		status_prompt_forward_word(c, size, 1, "");
		goto changed;
	case 'w'|KEYC_VI:
		separators = options_get_string(oo, "word-separators");
		status_prompt_forward_word(c, size, 1, separators);
		goto changed;
	case 'B'|KEYC_VI:
		status_prompt_backward_word(c, "");
		goto changed;
	case KEYC_LEFT|KEYC_CTRL:
	case 'b'|KEYC_META:
		separators = options_get_string(oo, "word-separators");
		status_prompt_backward_word(c, separators);
		goto changed;
	case KEYC_UP:
	case 'p'|KEYC_CTRL:
		histstr = status_prompt_up_history(c->prompt_hindex,
		    c->prompt_type);
		if (histstr == NULL)
			break;
		free(c->prompt_buffer);
		c->prompt_buffer = utf8_fromcstr(histstr);
		c->prompt_index = utf8_strlen(c->prompt_buffer);
		goto changed;
	case KEYC_DOWN:
	case 'n'|KEYC_CTRL:
		histstr = status_prompt_down_history(c->prompt_hindex,
		    c->prompt_type);
		if (histstr == NULL)
			break;
		free(c->prompt_buffer);
		c->prompt_buffer = utf8_fromcstr(histstr);
		c->prompt_index = utf8_strlen(c->prompt_buffer);
		goto changed;
	case 'y'|KEYC_CTRL:
		if (status_prompt_paste(c))
			goto changed;
		break;
	case 't'|KEYC_CTRL:
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
			status_prompt_add_history(s, c->prompt_type);
		if (c->prompt_inputcb(c, c->prompt_data, s, 1) == 0)
			status_prompt_clear(c);
		free(s);
		break;
	case '\033': /* Escape */
	case 'c'|KEYC_CTRL:
	case 'g'|KEYC_CTRL:
		if (c->prompt_inputcb(c, c->prompt_data, NULL, 1) == 0)
			status_prompt_clear(c);
		break;
	case 'r'|KEYC_CTRL:
		if (~c->prompt_flags & PROMPT_INCREMENTAL)
			break;
		if (c->prompt_buffer[0].size == 0) {
			prefix = '=';
			free(c->prompt_buffer);
			c->prompt_buffer = utf8_fromcstr(c->prompt_last);
			c->prompt_index = utf8_strlen(c->prompt_buffer);
		} else
			prefix = '-';
		goto changed;
	case 's'|KEYC_CTRL:
		if (~c->prompt_flags & PROMPT_INCREMENTAL)
			break;
		if (c->prompt_buffer[0].size == 0) {
			prefix = '=';
			free(c->prompt_buffer);
			c->prompt_buffer = utf8_fromcstr(c->prompt_last);
			c->prompt_index = utf8_strlen(c->prompt_buffer);
		} else
			prefix = '+';
		goto changed;
	case 'v'|KEYC_CTRL:
		c->prompt_flags |= PROMPT_QUOTENEXT;
		break;
	default:
		goto append_key;
	}

	c->flags |= CLIENT_REDRAWSTATUS;
	return (0);

append_key:
	if (key <= 0x7f) {
		utf8_set(&tmp, key);
		if (key <= 0x1f || key == 0x7f)
			tmp.width = 2;
	} else if (KEYC_IS_UNICODE(key))
		utf8_to_data(key, &tmp);
	else
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
status_prompt_up_history(u_int *idx, u_int type)
{
	/*
	 * History runs from 0 to size - 1. Index is from 0 to size. Zero is
	 * empty.
	 */

	if (status_prompt_hsize[type] == 0 ||
	    idx[type] == status_prompt_hsize[type])
		return (NULL);
	idx[type]++;
	return (status_prompt_hlist[type][status_prompt_hsize[type] - idx[type]]);
}

/* Get next line from the history. */
static const char *
status_prompt_down_history(u_int *idx, u_int type)
{
	if (status_prompt_hsize[type] == 0 || idx[type] == 0)
		return ("");
	idx[type]--;
	if (idx[type] == 0)
		return ("");
	return (status_prompt_hlist[type][status_prompt_hsize[type] - idx[type]]);
}

/* Add line to the history. */
static void
status_prompt_add_history(const char *line, u_int type)
{
	u_int	i, oldsize, newsize, freecount, hlimit, new = 1;
	size_t	movesize;

	oldsize = status_prompt_hsize[type];
	if (oldsize > 0 &&
	    strcmp(status_prompt_hlist[type][oldsize - 1], line) == 0)
		new = 0;

	hlimit = options_get_number(global_options, "prompt-history-limit");
	if (hlimit > oldsize) {
		if (new == 0)
			return;
		newsize = oldsize + new;
	} else {
		newsize = hlimit;
		freecount = oldsize + new - newsize;
		if (freecount > oldsize)
			freecount = oldsize;
		if (freecount == 0)
			return;
		for (i = 0; i < freecount; i++)
			free(status_prompt_hlist[type][i]);
		movesize = (oldsize - freecount) *
		    sizeof *status_prompt_hlist[type];
		if (movesize > 0) {
			memmove(&status_prompt_hlist[type][0],
			    &status_prompt_hlist[type][freecount], movesize);
		}
	}

	if (newsize == 0) {
		free(status_prompt_hlist[type]);
		status_prompt_hlist[type] = NULL;
	} else if (newsize != oldsize) {
		status_prompt_hlist[type] =
		    xreallocarray(status_prompt_hlist[type], newsize,
			sizeof *status_prompt_hlist[type]);
	}

	if (new == 1 && newsize > 0)
		status_prompt_hlist[type][newsize - 1] = xstrdup(line);
	status_prompt_hsize[type] = newsize;
}

/* Add to completion list. */
static void
status_prompt_add_list(char ***list, u_int *size, const char *s)
{
	u_int	i;

	for (i = 0; i < *size; i++) {
		if (strcmp((*list)[i], s) == 0)
			return;
	}
	*list = xreallocarray(*list, (*size) + 1, sizeof **list);
	(*list)[(*size)++] = xstrdup(s);
}

/* Build completion list. */
static char **
status_prompt_complete_list(u_int *size, const char *s, int at_start)
{
	char					**list = NULL, *tmp;
	const char				**layout, *value, *cp;
	const struct cmd_entry			**cmdent;
	const struct options_table_entry	 *oe;
	size_t					  slen = strlen(s), valuelen;
	struct options_entry			 *o;
	struct options_array_item		 *a;
	const char				 *layouts[] = {
		"even-horizontal", "even-vertical",
		"main-horizontal", "main-horizontal-mirrored",
		"main-vertical", "main-vertical-mirrored", "tiled", NULL
	};

	*size = 0;
	for (cmdent = cmd_table; *cmdent != NULL; cmdent++) {
		if (strncmp((*cmdent)->name, s, slen) == 0)
			status_prompt_add_list(&list, size, (*cmdent)->name);
		if ((*cmdent)->alias != NULL &&
		    strncmp((*cmdent)->alias, s, slen) == 0)
			status_prompt_add_list(&list, size, (*cmdent)->alias);
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

			xasprintf(&tmp, "%.*s", (int)valuelen, value);
			status_prompt_add_list(&list, size, tmp);
			free(tmp);

		next:
			a = options_array_next(a);
		}
	}
	if (at_start)
		return (list);
	for (oe = options_table; oe->name != NULL; oe++) {
		if (strncmp(oe->name, s, slen) == 0)
			status_prompt_add_list(&list, size, oe->name);
	}
	for (layout = layouts; *layout != NULL; layout++) {
		if (strncmp(*layout, s, slen) == 0)
			status_prompt_add_list(&list, size, *layout);
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
		if (c->prompt_type == PROMPT_TYPE_WINDOW_TARGET) {
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
		menu_add_item(menu, &item, NULL, c, NULL);
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

	if (menu_display(menu, MENU_NOMOUSE|MENU_TAB, 0, NULL, offset, py, c,
	    BOX_LINES_DEFAULT, NULL, NULL, NULL, NULL,
	    status_prompt_menu_callback, spm) != 0) {
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
		if (c->prompt_type == PROMPT_TYPE_WINDOW_TARGET) {
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
		menu_add_item(menu, &item, NULL, c, NULL);
		free(tmp);

		if (size == height)
			break;
	}
	if (size == 0) {
		menu_free(menu);
		free(spm);
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
		free(spm);
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

	if (menu_display(menu, MENU_NOMOUSE|MENU_TAB, 0, NULL, offset, py, c,
	    BOX_LINES_DEFAULT, NULL, NULL, NULL, NULL,
	    status_prompt_menu_callback, spm) != 0) {
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
	    c->prompt_type != PROMPT_TYPE_TARGET &&
	    c->prompt_type != PROMPT_TYPE_WINDOW_TARGET)
		return (NULL);

	if (c->prompt_type != PROMPT_TYPE_TARGET &&
	    c->prompt_type != PROMPT_TYPE_WINDOW_TARGET &&
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

	if (c->prompt_type == PROMPT_TYPE_TARGET ||
	    c->prompt_type == PROMPT_TYPE_WINDOW_TARGET) {
		s = word;
		flag = '\0';
	} else {
		s = word + 2;
		flag = word[1];
		offset += 2;
	}

	/* If this is a window completion, open the window menu. */
	if (c->prompt_type == PROMPT_TYPE_WINDOW_TARGET) {
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

/* Return the type of the prompt as an enum. */
enum prompt_type
status_prompt_type(const char *type)
{
	u_int	i;

	for (i = 0; i < PROMPT_NTYPES; i++) {
		if (strcmp(type, status_prompt_type_string(i)) == 0)
			return (i);
	}
	return (PROMPT_TYPE_INVALID);
}

/* Accessor for prompt_type_strings. */
const char *
status_prompt_type_string(u_int type)
{
	if (type >= PROMPT_NTYPES)
		return ("invalid");
	return (prompt_type_strings[type]);
}
