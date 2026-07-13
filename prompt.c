/* $OpenBSD: prompt.c,v 1.4 2026/06/26 14:40:30 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

struct prompt {
	char			 *string;
	struct utf8_data	 *buffer;
	struct cmd_find_state	  state;
	char			 *last;
	size_t			  index;

	prompt_input_cb		  inputcb;
	prompt_free_cb		  freecb;
	void			 *data;

	char			 *message_format;
	int			  keys;
	char			 *word_separators;
	struct grid_cell	  style;
	struct grid_cell	  command_style;
	enum screen_cursor_style  cstyle;
	enum screen_cursor_style  command_cstyle;
	int			  ccolour;
	int			  command_ccolour;
	int			  cmode;
	int			  command_cmode;

	enum prompt_type	  type;
	int			  flags;
	int			  closed;

	u_int			  hindex[PROMPT_NTYPES];
	struct utf8_data	 *copied;

	char			**complete_list;
	u_int			  complete_size;
	char			 *complete_display;
};

static char	*prompt_complete(struct prompt *, const char *, u_int);
static void	 prompt_clear_complete(struct prompt *);
static char	*prompt_expand(struct prompt *);
static int	 prompt_replace_complete(struct prompt *, const char *);
static u_int	 prompt_width(struct prompt *, u_int);

/* Get prompt flags as a string. */
static const char *
prompt_flags_to_string(int flags)
{
	static char	tmp[256];

	*tmp = '\0';
	if (flags & PROMPT_SINGLE)
		strlcat(tmp, "SINGLE,", sizeof tmp);
	if (flags & PROMPT_NUMERIC)
		strlcat(tmp, "NUMERIC,", sizeof tmp);
	if (flags & PROMPT_INCREMENTAL)
		strlcat(tmp, "INCREMENTAL,", sizeof tmp);
	if (flags & PROMPT_NOFORMAT)
		strlcat(tmp, "NOFORMAT,", sizeof tmp);
	if (flags & PROMPT_KEY)
		strlcat(tmp, "KEY,", sizeof tmp);
	if (flags & PROMPT_ACCEPT)
		strlcat(tmp, "ACCEPT,", sizeof tmp);
	if (flags & PROMPT_QUOTENEXT)
		strlcat(tmp, "QUOTENEXT,", sizeof tmp);
	if (flags & PROMPT_BSPACE_EXIT)
		strlcat(tmp, "BSPACE_EXIT,", sizeof tmp);
	if (flags & PROMPT_NOFREEZE)
		strlcat(tmp, "NOFREEZE,", sizeof tmp);
	if (flags & PROMPT_COMMANDMODE)
		strlcat(tmp, "COMMANDMODE,", sizeof tmp);
	if (flags & PROMPT_ISPANE)
		strlcat(tmp, "ISPANE,", sizeof tmp);
	if (flags & PROMPT_ISMODE)
		strlcat(tmp, "ISMODE,", sizeof tmp);
	if (flags & PROMPT_EDITARROWS)
		strlcat(tmp, "EDITARROWS,", sizeof tmp);
	if (*tmp != '\0')
		tmp[strlen(tmp) - 1] = '\0';
	return (tmp);
}

/* Set prompt options from session options. */
void
prompt_set_options(struct prompt_create_data *pd, struct session *s)
{
	struct options		*oo;
	struct grid_cell	 gc;
	u_int			 n;

	if (s != NULL)
		oo = s->options;
	else
		oo = global_s_options;

	style_apply(&pd->style, oo, "message-style", NULL);
	style_apply(&pd->command_style, oo, "message-command-style", NULL);
	n = options_get_number(oo, "prompt-cursor-style");
	screen_set_cursor_style(n, &pd->cstyle, &pd->cmode);
	n = options_get_number(oo, "prompt-command-cursor-style");
	screen_set_cursor_style(n, &pd->command_cstyle, &pd->command_cmode);
	style_apply(&gc, oo, "prompt-cursor-colour", NULL);
	pd->ccolour = gc.fg;
	style_apply(&gc, oo, "prompt-command-cursor-colour", NULL);
	pd->command_ccolour = gc.fg;
	pd->message_format = options_get_string(oo, "message-format");
	pd->keys = options_get_number(oo, "status-keys");
	pd->word_separators = options_get_string(oo, "word-separators");
}

/* Create prompt. */
struct prompt *
prompt_create(const struct prompt_create_data *pd)
{
	struct prompt		*pr;
	struct format_tree	*ft;
	const char		*input = pd->input;
	char			*tmp;

	pr = xcalloc(1, sizeof *pr);

	if (pd->fs != NULL) {
		ft = format_create_from_state(NULL, NULL, pd->fs);
		cmd_find_copy_state(&pr->state, pd->fs);
	} else {
		ft = format_create_defaults(NULL, NULL, NULL, NULL, NULL);
		cmd_find_clear_state(&pr->state, 0);
	}

	if (input == NULL)
		input = "";
	pr->string = xstrdup(pd->prompt);
	if (pd->flags & PROMPT_NOFORMAT)
		tmp = xstrdup(input);
	else
		tmp = format_expand_time(ft, input);
	if (pd->flags & PROMPT_INCREMENTAL) {
		pr->last = xstrdup(tmp);
		pr->buffer = utf8_fromcstr("");
	} else {
		pr->last = NULL;
		pr->buffer = utf8_fromcstr(tmp);
	}
	pr->index = utf8_strlen(pr->buffer);
	free(tmp);

	pr->inputcb = pd->inputcb;
	pr->freecb = pd->freecb;
	pr->data = pd->data;

	pr->flags = pd->flags;
	pr->type = pd->type;

	memcpy(&pr->style, &pd->style, sizeof pr->style);
	memcpy(&pr->command_style, &pd->command_style,
	    sizeof pr->command_style);
	pr->cstyle = pd->cstyle;
	pr->command_cstyle = pd->command_cstyle;
	pr->ccolour = pd->ccolour;
	pr->command_ccolour = pd->command_ccolour;
	pr->cmode = pd->cmode;
	pr->command_cmode = pd->command_cmode;
	pr->message_format = xstrdup(pd->message_format);
	pr->keys = pd->keys;
	pr->word_separators = xstrdup(pd->word_separators);

	format_free(ft);
	return (pr);
}

/* Free prompt. */
void
prompt_free(struct prompt *pr)
{
	if (pr != NULL) {
		if (pr->freecb != NULL && pr->data != NULL)
			pr->freecb(pr->data);
		free(pr->message_format);
		free(pr->word_separators);
		free(pr->last);
		free(pr->string);
		free(pr->buffer);
		free(pr->copied);
		prompt_clear_complete(pr);
		free(pr);
	}
}

/*
 * Fire the input callback. Returns one if the prompt is finished or zero if
 * still open.
 */
static int
prompt_fire_callback(struct prompt *pr, const char *s,
    enum prompt_key_result type, int *redraw)
{
	enum prompt_result	result;

	result = pr->inputcb(pr->data, s, type);
	if (result == PROMPT_CLOSE) {
		pr->closed = 1;
		return (1);
	}
	if (redraw != NULL)
		*redraw = 1;
	return (0);
}

/* Start incremental prompt. */
void
prompt_incremental_start(struct prompt *pr)
{
	char	*tmp, *cp;

	if (pr->flags & PROMPT_INCREMENTAL) {
		tmp = utf8_tocstr(pr->buffer);
		xasprintf(&cp, "=%s", tmp);
		prompt_fire_callback(pr, cp, PROMPT_KEY_HANDLED, NULL);
		free(cp);
		free(tmp);
	}
}

/* Update prompt. */
void
prompt_update(struct prompt *pr, const char *msg, const char *input)
{
	struct format_tree	*ft;
	char			*tmp;

	if (cmd_find_valid_state(&pr->state))
		ft = format_create_from_state(NULL, NULL, &pr->state);
	else
		ft = format_create_defaults(NULL, NULL, NULL, NULL, NULL);

	free(pr->string);
	pr->string = xstrdup(msg);

	if (input == NULL)
		input = "";
	free(pr->buffer);
	if (pr->flags & PROMPT_NOFORMAT)
		tmp = xstrdup(input);
	else
		tmp = format_expand_time(ft, input);
	pr->buffer = utf8_fromcstr(tmp);
	pr->index = utf8_strlen(pr->buffer);
	free(tmp);

	memset(pr->hindex, 0, sizeof pr->hindex);
	pr->closed = 0;
	prompt_clear_complete(pr);

	format_free(ft);
}

/* Is this prompt closed? */
int
prompt_closed(struct prompt *pr)
{
	return (pr->closed);
}

/* Redraw character. Return 1 if can continue redrawing, 0 otherwise. */
static int
prompt_redraw_character(struct screen_write_ctx *ctx, u_int offset,
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
prompt_redraw_quote(const struct prompt *pr, u_int pcursor,
    struct screen_write_ctx *ctx, u_int offset, u_int pwidth, u_int *width,
    struct grid_cell *gc)
{
	struct utf8_data	ud;

	if (pr->flags & PROMPT_QUOTENEXT && ctx->s->cx == pcursor + 1) {
		utf8_set(&ud, '^');
		return (prompt_redraw_character(ctx, offset, pwidth,
		    width, gc, &ud));
	}
	return (1);
}

/* Draw the stored completion matches. */
static void
prompt_draw_complete(struct prompt *pr, struct screen_write_ctx *ctx, u_int ax,
    u_int aw, u_int cx, u_int py, const struct grid_cell *base)
{
	struct grid_cell	 gc;
	struct utf8_data	*ud;
	u_int			 avail, width, i;

	if (pr->complete_display == NULL)
		return;
	if (pr->index != utf8_strlen(pr->buffer))
		return;
	if (cx < ax || cx - ax >= aw)
		return;
	avail = aw - (cx - ax);

	memcpy(&gc, base, sizeof gc);
	gc.attr |= GRID_ATTR_UNDERSCORE;
	screen_write_cursormove(ctx, cx, py, 0);

	width = 0;
	ud = utf8_fromcstr(pr->complete_display);
	for (i = 0; ud[i].size != 0; i++) {
		if (width + ud[i].width > avail)
			break;
		utf8_copy(&gc.data, &ud[i]);
		screen_write_cell(ctx, &gc);
		width += ud[i].width;
	}
	free(ud);
}

/* Expand prompt string using the current input. */
static char *
prompt_expand(struct prompt *pr)
{
	struct format_tree	*ft;
	char			*expanded, *prompt, *tmp;

	if (cmd_find_valid_state(&pr->state))
		ft = format_create_from_state(NULL, NULL, &pr->state);
	else
		ft = format_create_defaults(NULL, NULL, NULL, NULL, NULL);
	tmp = utf8_tocstr(pr->buffer);
	format_add(ft, "prompt_input", "%s", tmp);
	free(tmp);

	format_add(ft, "prompt_flags", "%s", prompt_flags_to_string(pr->flags));
	format_add(ft, "prompt_type", "%s", prompt_type_string(pr->type));
	prompt = format_expand_time(ft, pr->string);
	format_add(ft, "message", "%s", prompt);
	if (pr->flags & PROMPT_COMMANDMODE)
		format_add(ft, "command_prompt", "1");
	else
		format_add(ft, "command_prompt", "0");
	expanded = format_expand_time(ft, pr->message_format);
	free(prompt);
	format_free(ft);
	return (expanded);
}

/* Work out the width used by the prompt string. */
static u_int
prompt_width(struct prompt *pr, u_int aw)
{
	char	*expanded;
	u_int	 start;

	expanded = prompt_expand(pr);
	start = format_width(expanded);
	if (start > aw)
		start = aw;
	free(expanded);
	return (start);
}

/* Choose a completion from a mouse position. */
static enum prompt_key_result
prompt_mouse_complete(struct prompt *pr, u_int x, u_int cx, u_int ax, u_int aw,
    int *redraw)
{
	char	*replace;
	u_int	 avail, clicked, end, i, start, width;

	if (pr->complete_display == NULL || pr->complete_size == 0)
		return (PROMPT_KEY_NOT_HANDLED);
	if (pr->index != utf8_strlen(pr->buffer))
		return (PROMPT_KEY_NOT_HANDLED);
	if (cx < ax || cx - ax >= aw || x < cx)
		return (PROMPT_KEY_NOT_HANDLED);

	avail = aw - (cx - ax);
	clicked = x - cx;
	width = utf8_cstrwidth(pr->complete_display);
	if (width > avail)
		width = avail;
	if (clicked >= width)
		return (PROMPT_KEY_NOT_HANDLED);

	end = 0;
	for (i = 0; i < pr->complete_size; i++) {
		start = end + 1;
		end = start + utf8_cstrwidth(pr->complete_list[i]);
		if (clicked < start || clicked >= end)
			continue;

		xasprintf(&replace, "%s ", pr->complete_list[i]);
		if (prompt_replace_complete(pr, replace)) {
			prompt_clear_complete(pr);
			if (redraw != NULL)
				*redraw = 1;
		}
		free(replace);
		return (PROMPT_KEY_HANDLED);
	}
	return (PROMPT_KEY_HANDLED);
}

/* Draw prompt. */
void
prompt_draw(struct prompt *pr, struct prompt_draw_data *pd)
{
	struct screen_write_ctx	*ctx = pd->ctx;
	struct screen		*s = ctx->s;
	u_int			 ax = pd->area_x, py = pd->prompt_line;
	u_int			 aw = pd->area_width, *cx = pd->cursor_x;
	struct grid_cell	 gc;
	u_int			 i, offset, left, start, width;
	u_int			 pcursor, pwidth;
	char			*expanded;

	/* Choose the cursor colour and style for this prompt. */
	if (pr->flags & PROMPT_COMMANDMODE) {
		memcpy(&gc, &pr->command_style, sizeof gc);
		s->default_cstyle = pr->command_cstyle;
		s->default_mode = pr->command_cmode;
		s->default_ccolour = pr->command_ccolour;
	} else {
		memcpy(&gc, &pr->style, sizeof gc);
		s->default_cstyle = pr->cstyle;
		s->default_mode = pr->cmode;
		s->default_ccolour = pr->ccolour;
	}

	expanded = prompt_expand(pr);
	start = format_width(expanded);
	if (start > aw)
		start = aw;
	*cx = ax + start;

	screen_write_cursormove(ctx, ax, py, 0);
	format_draw(ctx, &gc, aw, expanded, NULL, 0);
	screen_write_cursormove(ctx, ax + start, py, 0);
	free(expanded);

	left = aw - start;
	if (left == 0)
		return;

	pcursor = utf8_strwidth(pr->buffer, pr->index);
	pwidth = utf8_strwidth(pr->buffer, -1);
	if (pr->flags & PROMPT_QUOTENEXT)
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
	*cx = ax + start + pcursor - offset;

	width = 0;
	for (i = 0; pr->buffer[i].size != 0; i++) {
		if (!prompt_redraw_quote(pr, pcursor, ctx, offset, pwidth,
		    &width, &gc))
			break;
		if (!prompt_redraw_character(ctx, offset, pwidth, &width, &gc,
		    &pr->buffer[i]))
			break;
	}
	prompt_redraw_quote(pr, pcursor, ctx, offset, pwidth, &width, &gc);

	prompt_draw_complete(pr, ctx, ax, aw, *cx, py, &gc);
}

/* Move cursor in prompt from a mouse position. */
enum prompt_key_result
prompt_mouse(struct prompt *pr, u_int x, u_int ax, u_int aw, int *redraw)
{
	struct utf8_data	*ud;
	enum prompt_key_result	 result;
	u_int			 cx, start, left, pcursor, pwidth, offset, width;
	u_int			 target;
	size_t			 idx;

	if (x < ax || x >= ax + aw)
		return (PROMPT_KEY_NOT_HANDLED);

	start = prompt_width(pr, aw);
	left = aw - start;
	if (left == 0)
		return (PROMPT_KEY_HANDLED);

	pcursor = utf8_strwidth(pr->buffer, pr->index);
	pwidth = utf8_strwidth(pr->buffer, -1);
	if (pr->flags & PROMPT_QUOTENEXT)
		pwidth++;
	if (pcursor >= left)
		offset = (pcursor - left) + 1;
	else
		offset = 0;

	cx = ax + start + pcursor - offset;
	result = prompt_mouse_complete(pr, x, cx, ax, aw, redraw);
	if (result != PROMPT_KEY_NOT_HANDLED)
		return (result);

	if (x <= ax + start)
		target = offset;
	else
		target = offset + x - (ax + start);
	if (target > pwidth)
		target = pwidth;

	width = 0;
	for (idx = 0; pr->buffer[idx].size != 0; idx++) {
		ud = &pr->buffer[idx];
		if (width >= target)
			break;
		width += ud->width;
	}
	if (idx == pr->index)
		return (PROMPT_KEY_HANDLED);

	pr->index = idx;
	prompt_clear_complete(pr);
	if (redraw != NULL)
		*redraw = 1;

	return (PROMPT_KEY_HANDLED);
}

/* Is this a separator? */
static int
prompt_in_list(const char *ws, const struct utf8_data *ud)
{
	if (ud->size != 1 || ud->width != 1)
		return (0);
	return (strchr(ws, *ud->data) != NULL);
}

/* Is this a space? */
static int
prompt_space(const struct utf8_data *ud)
{
	if (ud->size != 1 || ud->width != 1)
		return (0);
	return (*ud->data == ' ');
}

/* Is this a keypad key? */
static key_code
prompt_keypad_key(key_code key)
{
	if (key & KEYC_MASK_MODIFIERS)
		return (key);

	switch (key) {
	case KEYC_KP_SLASH:
		return ('/');
	case KEYC_KP_STAR:
		return ('*');
	case KEYC_KP_MINUS:
		return ('-');
	case KEYC_KP_SEVEN:
		return ('7');
	case KEYC_KP_EIGHT:
		return ('8');
	case KEYC_KP_NINE:
		return ('9');
	case KEYC_KP_PLUS:
		return ('+');
	case KEYC_KP_FOUR:
		return ('4');
	case KEYC_KP_FIVE:
		return ('5');
	case KEYC_KP_SIX:
		return ('6');
	case KEYC_KP_ONE:
		return ('1');
	case KEYC_KP_TWO:
		return ('2');
	case KEYC_KP_THREE:
		return ('3');
	case KEYC_KP_ENTER:
		return ('\r');
	case KEYC_KP_ZERO:
		return ('0');
	case KEYC_KP_PERIOD:
		return ('.');
	}
	return (key);
}

/*
 * Translate key from vi to emacs. Return 0 to drop key, 1 to process the key
 * as an emacs key; return 2 to append to the buffer. Set *redraw if the
 * translation changed something the host needs to redraw (such as switching
 * between insert and command mode).
 */
static int
prompt_translate_key(struct prompt *pr, key_code key, key_code *new_key,
    int *redraw)
{
	if (~pr->flags & PROMPT_COMMANDMODE) {
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
		case '['|KEYC_CTRL:
			pr->flags |= PROMPT_COMMANDMODE;
			if (pr->index != 0)
				pr->index--;
			*redraw = 1;
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
		pr->flags &= ~PROMPT_COMMANDMODE;
		*redraw = 1;
		break; /* switch mode and... */
	case 'S':
		pr->flags &= ~PROMPT_COMMANDMODE;
		*redraw = 1;
		*new_key = 'u'|KEYC_CTRL;
		return (1);
	case 'i':
		pr->flags &= ~PROMPT_COMMANDMODE;
		*redraw = 1;
		return (0);
	case '\033': /* Escape */
	case '['|KEYC_CTRL:
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
prompt_paste(struct prompt *pr)
{
	struct paste_buffer	*pb;
	const char		*bufdata;
	size_t			 size, n, bufsize;
	u_int			 i;
	struct utf8_data	*ud, *udp;
	enum utf8_state		 more;

	size = utf8_strlen(pr->buffer);
	if (pr->copied != NULL) {
		ud = pr->copied;
		n = utf8_strlen(pr->copied);
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
		pr->buffer = xreallocarray(pr->buffer, size + n + 1,
		    sizeof *pr->buffer);
		if (pr->index == size) {
			memcpy(pr->buffer + pr->index, ud,
			    n * sizeof *pr->buffer);
			pr->index += n;
			pr->buffer[pr->index].size = 0;
		} else {
			memmove(pr->buffer + pr->index + n,
			    pr->buffer + pr->index,
			    (size + 1 - pr->index) *
			    sizeof *pr->buffer);
			memcpy(pr->buffer + pr->index, ud,
			    n * sizeof *pr->buffer);
			pr->index += n;
		}
	}
	if (ud != pr->copied)
		free(ud);
	return (1);
}

/* Finish completion. */
static int
prompt_replace_complete(struct prompt *pr, const char *s)
{
	char			 word[64], *allocated = NULL;
	size_t			 size, n, off, idx, used;
	struct utf8_data	*first, *last, *ud;

	/* Work out where the cursor currently is. */
	idx = pr->index;
	if (idx != 0)
		idx--;
	size = utf8_strlen(pr->buffer);

	/* Find the word we are in. */
	first = &pr->buffer[idx];
	while (first > pr->buffer && !prompt_space(first))
		first--;
	while (first->size != 0 && prompt_space(first))
		first++;
	last = &pr->buffer[idx];
	while (last->size != 0 && !prompt_space(last))
		last++;
	while (last > pr->buffer && prompt_space(last))
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
		allocated = prompt_complete(pr, word, first - pr->buffer);
		if (allocated == NULL)
			return (0);
		s = allocated;
	}

	/* Trim out word. */
	n = size - (last - pr->buffer) + 1; /* with \0 */
	memmove(first, last, n * sizeof *pr->buffer);
	size -= last - first;

	/* Insert the new word. */
	size += strlen(s);
	off = first - pr->buffer;
	pr->buffer = xreallocarray(pr->buffer, size + 1,
	    sizeof *pr->buffer);
	first = pr->buffer + off;
	memmove(first + strlen(s), first, n * sizeof *pr->buffer);
	for (idx = 0; idx < strlen(s); idx++)
		utf8_set(&first[idx], s[idx]);
	pr->index = (first - pr->buffer) + strlen(s);

	free(allocated);
	return (1);
}

/* Prompt forward to the next beginning of a word. */
static void
prompt_forward_word(struct prompt *pr, size_t size, int vi,
    const char *separators)
{
	size_t		 idx = pr->index;
	int		 word_is_separators;

	/* In emacs mode, skip until the first non-whitespace character. */
	if (!vi) {
		while (idx != size && prompt_space(&pr->buffer[idx]))
			idx++;
	}

	/* Can't move forward if we're already at the end. */
	if (idx == size) {
		pr->index = idx;
		return;
	}

	/* Determine the current character class (separators or not). */
	word_is_separators = prompt_in_list(separators, &pr->buffer[idx]) &&
	    !prompt_space(&pr->buffer[idx]);

	/* Skip ahead until the first space or opposite character class. */
	do {
		idx++;
		if (prompt_space(&pr->buffer[idx])) {
			/* In vi mode, go to the start of the next word. */
			if (vi) {
				while (idx != size &&
				    prompt_space(&pr->buffer[idx]))
					idx++;
			}
			break;
		}
	} while (idx != size && word_is_separators == prompt_in_list(
	    separators, &pr->buffer[idx]));

	pr->index = idx;
}

/* Prompt forward to the next end of a word. */
static void
prompt_end_word(struct prompt *pr, size_t size, const char *separators)
{
	size_t		 idx = pr->index;
	int		 word_is_separators;

	/* Can't move forward if we're already at the end. */
	if (idx == size)
		return;

	/* Find the next word. */
	do {
		idx++;
		if (idx == size) {
			pr->index = idx;
			return;
		}
	} while (prompt_space(&pr->buffer[idx]));

	/* Determine the character class (separators or not). */
	word_is_separators = prompt_in_list(separators,
	    &pr->buffer[idx]);

	/* Skip ahead until the next space or opposite character class. */
	do {
		idx++;
		if (idx == size)
			break;
	} while (!prompt_space(&pr->buffer[idx]) &&
	    word_is_separators == prompt_in_list(separators, &pr->buffer[idx]));

	/* Back up to the previous character to stop at the end of the word. */
	pr->index = idx - 1;
}

/* Prompt backward to the previous beginning of a word. */
static void
prompt_backward_word(struct prompt *pr, const char *separators)
{
	size_t	idx = pr->index;
	int	word_is_separators;

	/* Find non-whitespace. */
	while (idx != 0) {
		--idx;
		if (!prompt_space(&pr->buffer[idx]))
			break;
	}
	word_is_separators = prompt_in_list(separators,
	    &pr->buffer[idx]);

	/* Find the character before the beginning of the word. */
	while (idx != 0) {
		--idx;
		if (prompt_space(&pr->buffer[idx]) ||
		    word_is_separators != prompt_in_list(separators,
		    &pr->buffer[idx])) {
			/* Go back to the word. */
			idx++;
			break;
		}
	}
	pr->index = idx;
}

/* Fire input callback when done. */
static enum prompt_key_result
prompt_done(struct prompt *pr, const char *s, int *redraw)
{
	if (prompt_fire_callback(pr, s, PROMPT_KEY_CLOSE, redraw))
		return (PROMPT_KEY_CLOSE);
	return (PROMPT_KEY_HANDLED);
}

/* Check for a movement key. */
static enum prompt_key_result
prompt_check_move(struct prompt *pr, key_code key)
{
	char	*s;

	if (~pr->flags & PROMPT_INCREMENTAL)
		return (PROMPT_KEY_NOT_HANDLED);
	switch (key) {
	case KEYC_UP:
	case KEYC_DOWN:
	case KEYC_PPAGE:
	case KEYC_NPAGE:
		break;
	case KEYC_LEFT:
	case KEYC_RIGHT:
		if (pr->flags & PROMPT_EDITARROWS)
			return (PROMPT_KEY_NOT_HANDLED);
		break;
	default:
		return (PROMPT_KEY_NOT_HANDLED);
	}
	s = utf8_tocstr(pr->buffer);
	if (prompt_fire_callback(pr, s, PROMPT_KEY_MOVE, NULL)) {
		free(s);
		return (PROMPT_KEY_CLOSE);
	}
	free(s);
	return (PROMPT_KEY_MOVE);
}

/* Handle keys in prompt. */
enum prompt_key_result
prompt_key(struct prompt *pr, key_code key, int *redraw)
{
	char			*s, *cp, prefix = '=';
	const char		*histstr, *ks;
	size_t			 size, idx;
	struct utf8_data	 tmp;
	enum prompt_key_result	 result = PROMPT_KEY_HANDLED;
	int			 word_is_separators;

	pr->closed = 0;

	/*
	 * Drop any inline completion matches; the Tab handler rebuilds them if
	 * completion is still applicable.
	 */
	prompt_clear_complete(pr);

	if (pr->flags & PROMPT_KEY) {
		ks = key_string_lookup_key(key, 0);
		if (!prompt_fire_callback(pr, ks, PROMPT_KEY_CLOSE, NULL))
			pr->closed = 1;
		return (PROMPT_KEY_CLOSE);
	}
	size = utf8_strlen(pr->buffer);

	key &= ~KEYC_MASK_FLAGS;
	key = prompt_keypad_key(key);

	if (pr->flags & PROMPT_NUMERIC) {
		if (key >= '0' && key <= '9')
			goto append_key;
		s = utf8_tocstr(pr->buffer);
		if (!prompt_fire_callback(pr, s, PROMPT_KEY_CLOSE, NULL))
			pr->closed = 1;
		free(s);
		return (PROMPT_KEY_NOT_HANDLED);
	}

	if (pr->flags & (PROMPT_SINGLE|PROMPT_QUOTENEXT)) {
		if ((key & KEYC_MASK_KEY) == KEYC_BSPACE)
			key = 0x7f;
		else if ((key & KEYC_MASK_KEY) > 0x7f) {
			if (!KEYC_IS_UNICODE(key))
				return (PROMPT_KEY_HANDLED);
			key &= KEYC_MASK_KEY;
		} else
			key &= (key & KEYC_CTRL) ? 0x1f : KEYC_MASK_KEY;
		pr->flags &= ~PROMPT_QUOTENEXT;
		goto append_key;
	}

	if (pr->keys == MODEKEY_VI) {
		switch (prompt_translate_key(pr, key, &key, redraw)) {
		case 1:
			goto process_key;
		case 2:
			goto append_key;
		default:
			return (PROMPT_KEY_HANDLED);
		}
	}

process_key:
	result = prompt_check_move(pr, key);
	if (result != PROMPT_KEY_NOT_HANDLED)
		return (result);
	result = PROMPT_KEY_HANDLED;

	switch (key) {
	case KEYC_LEFT:
	case 'b'|KEYC_CTRL:
		if (pr->index > 0) {
			pr->index--;
			break;
		}
		break;
	case KEYC_RIGHT:
	case 'f'|KEYC_CTRL:
		if (pr->index < size) {
			pr->index++;
			break;
		}
		break;
	case KEYC_HOME:
	case 'a'|KEYC_CTRL:
		if (pr->index != 0) {
			pr->index = 0;
			break;
		}
		break;
	case KEYC_END:
	case 'e'|KEYC_CTRL:
		if (pr->index != size) {
			pr->index = size;
			break;
		}
		break;
	case '\011': /* Tab */
		if (prompt_replace_complete(pr, NULL))
			goto changed;
		break;
	case KEYC_BSPACE:
	case 'h'|KEYC_CTRL:
		if (pr->flags & PROMPT_BSPACE_EXIT && size == 0)
			return (prompt_done(pr, NULL, redraw));
		if (pr->index != 0) {
			if (pr->index == size)
				pr->buffer[--pr->index].size = 0;
			else {
				memmove(pr->buffer + pr->index - 1,
				    pr->buffer + pr->index,
				    (size + 1 - pr->index) *
				    sizeof *pr->buffer);
				pr->index--;
			}
			goto changed;
		}
		break;
	case KEYC_DC:
	case 'd'|KEYC_CTRL:
		if (pr->index != size) {
			memmove(pr->buffer + pr->index,
			    pr->buffer + pr->index + 1,
			    (size + 1 - pr->index) *
			    sizeof *pr->buffer);
			goto changed;
		}
		break;
	case 'u'|KEYC_CTRL:
		pr->buffer[0].size = 0;
		pr->index = 0;
		goto changed;
	case 'k'|KEYC_CTRL:
		if (pr->index < size) {
			pr->buffer[pr->index].size = 0;
			goto changed;
		}
		break;
	case 'w'|KEYC_CTRL:
		/* Find non-whitespace. */
		idx = pr->index;
		while (idx != 0) {
			idx--;
			if (!prompt_space(&pr->buffer[idx]))
				break;
		}
		word_is_separators = prompt_in_list(pr->word_separators,
		    &pr->buffer[idx]);

		/* Find the character before the beginning of the word. */
		while (idx != 0) {
			idx--;
			if (prompt_space(&pr->buffer[idx]) ||
			    word_is_separators != prompt_in_list(
			    pr->word_separators, &pr->buffer[idx])) {
				/* Go back to the word. */
				idx++;
				break;
			}
		}

		free(pr->copied);
		pr->copied = xcalloc(sizeof *pr->buffer,
		    (pr->index - idx) + 1);
		memcpy(pr->copied, pr->buffer + idx,
		    (pr->index - idx) * sizeof *pr->buffer);

		memmove(pr->buffer + idx, pr->buffer + pr->index,
		    (size + 1 - pr->index) * sizeof *pr->buffer);
		memset(pr->buffer + size - (pr->index - idx), '\0',
		    (pr->index - idx) * sizeof *pr->buffer);
		pr->index = idx;

		goto changed;
	case KEYC_RIGHT|KEYC_CTRL:
	case 'f'|KEYC_META:
		prompt_forward_word(pr, size, 0, pr->word_separators);
		goto changed;
	case 'E'|KEYC_VI:
		prompt_end_word(pr, size, "");
		goto changed;
	case 'e'|KEYC_VI:
		prompt_end_word(pr, size, pr->word_separators);
		goto changed;
	case 'W'|KEYC_VI:
		prompt_forward_word(pr, size, 1, "");
		goto changed;
	case 'w'|KEYC_VI:
		prompt_forward_word(pr, size, 1, pr->word_separators);
		goto changed;
	case 'B'|KEYC_VI:
		prompt_backward_word(pr, "");
		goto changed;
	case KEYC_LEFT|KEYC_CTRL:
	case 'b'|KEYC_META:
		prompt_backward_word(pr, pr->word_separators);
		goto changed;
	case KEYC_UP:
	case 'p'|KEYC_CTRL:
		histstr = prompt_up_history(pr->hindex,
		    pr->type);
		if (histstr == NULL)
			break;
		free(pr->buffer);
		pr->buffer = utf8_fromcstr(histstr);
		pr->index = utf8_strlen(pr->buffer);
		goto changed;
	case KEYC_DOWN:
	case 'n'|KEYC_CTRL:
		histstr = prompt_down_history(pr->hindex, pr->type);
		if (histstr == NULL)
			break;
		free(pr->buffer);
		pr->buffer = utf8_fromcstr(histstr);
		pr->index = utf8_strlen(pr->buffer);
		goto changed;
	case 'y'|KEYC_CTRL:
		if (prompt_paste(pr))
			goto changed;
		break;
	case 't'|KEYC_CTRL:
		idx = pr->index;
		if (idx < size)
			idx++;
		if (idx >= 2) {
			utf8_copy(&tmp, &pr->buffer[idx - 2]);
			utf8_copy(&pr->buffer[idx - 2], &pr->buffer[idx - 1]);
			utf8_copy(&pr->buffer[idx - 1], &tmp);
			pr->index = idx;
			goto changed;
		}
		break;
	case '\r':
	case '\n':
		s = utf8_tocstr(pr->buffer);
		if (*s != '\0')
			prompt_add_history(s, pr->type);
		result = prompt_done(pr, s, redraw);
		free(s);
		return (result);
	case '\033': /* Escape */
	case '['|KEYC_CTRL:
	case 'c'|KEYC_CTRL:
	case 'g'|KEYC_CTRL:
		return (prompt_done(pr, NULL, redraw));
	case 'r'|KEYC_CTRL:
		if (~pr->flags & PROMPT_INCREMENTAL)
			break;
		if (pr->buffer[0].size == 0) {
			prefix = '=';
			free(pr->buffer);
			pr->buffer = utf8_fromcstr(pr->last);
			pr->index = utf8_strlen(pr->buffer);
		} else
			prefix = '-';
		goto changed;
	case 's'|KEYC_CTRL:
		if (~pr->flags & PROMPT_INCREMENTAL)
			break;
		if (pr->buffer[0].size == 0) {
			prefix = '=';
			free(pr->buffer);
			pr->buffer = utf8_fromcstr(pr->last);
			pr->index = utf8_strlen(pr->buffer);
		} else
			prefix = '+';
		goto changed;
	case 'v'|KEYC_CTRL:
		pr->flags |= PROMPT_QUOTENEXT;
		break;
	default:
		goto append_key;
	}

	*redraw = 1;
	return (PROMPT_KEY_HANDLED);

append_key:
	if (key <= 0x7f) {
		utf8_set(&tmp, key);
		if (key <= 0x1f || key == 0x7f)
			tmp.width = 2;
	} else if (KEYC_IS_UNICODE(key))
		utf8_to_data(key, &tmp);
	else
		return (PROMPT_KEY_HANDLED);

	pr->buffer = xreallocarray(pr->buffer, size + 2,
	    sizeof *pr->buffer);

	if (pr->index == size) {
		utf8_copy(&pr->buffer[pr->index], &tmp);
		pr->index++;
		pr->buffer[pr->index].size = 0;
	} else {
		memmove(pr->buffer + pr->index + 1,
		    pr->buffer + pr->index,
		    (size + 1 - pr->index) *
		    sizeof *pr->buffer);
		utf8_copy(&pr->buffer[pr->index], &tmp);
		pr->index++;
	}

	if (pr->flags & PROMPT_SINGLE) {
		if (utf8_strlen(pr->buffer) != 1) {
			pr->closed = 1;
			result = PROMPT_KEY_CLOSE;
		} else {
			s = utf8_tocstr(pr->buffer);
			result = prompt_done(pr, s, redraw);
			free(s);
		}
	}

changed:
	*redraw = 1;
	if (pr->flags & PROMPT_INCREMENTAL) {
		s = utf8_tocstr(pr->buffer);
		xasprintf(&cp, "%c%s", prefix, s);
		prompt_fire_callback(pr, cp, PROMPT_KEY_HANDLED, NULL);
		free(cp);
		free(s);
	}
	return (result);
}

/* Add to completion list. */
static void
prompt_complete_add(char ***list, u_int *size, const char *s)
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
prompt_complete_commands(u_int *size, const char *s)
{
	char				**list = NULL, *tmp;
	const char			*value, *cp;
	const struct cmd_entry		**cmdent;
	size_t				 slen = strlen(s), valuelen;
	struct options_entry		*o;
	struct options_array_item	*a;

	*size = 0;
	for (cmdent = cmd_table; *cmdent != NULL; cmdent++) {
		if (strncmp((*cmdent)->name, s, slen) == 0)
			prompt_complete_add(&list, size, (*cmdent)->name);
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
			prompt_complete_add(&list, size, tmp);
			free(tmp);

		next:
			a = options_array_next(a);
		}
	}
	return (list);
}

/* Find longest prefix. */
static char *
prompt_complete_prefix(char **list, u_int size)
{
	char	 *out;
	u_int	  i;
	size_t	  j;

	if (list == NULL || size == 0)
		return (NULL);
	out = xstrdup(list[0]);
	for (i = 1; i < size; i++) {
		for (j = 0; out[j] != '\0' && list[i][j] != '\0'; j++) {
			if (out[j] != list[i][j])
				break;
		}
		out[j] = '\0';
	}
	return (out);
}

/* Sort complete list. */
static int
prompt_complete_sort(const void *a, const void *b)
{
	const char	**aa = (const char **)a, **bb = (const char **)b;

	return (strcmp(*aa, *bb));
}

/* Free the stored inline completion matches. */
static void
prompt_clear_complete(struct prompt *pr)
{
	u_int	i;

	for (i = 0; i < pr->complete_size; i++)
		free(pr->complete_list[i]);
	free(pr->complete_list);
	pr->complete_list = NULL;
	pr->complete_size = 0;

	free(pr->complete_display);
	pr->complete_display = NULL;
}

/*
 * Store the match list for inline display and build the dim suffix string: a
 * leading space then the matches separated by spaces.
 */
static void
prompt_store_complete(struct prompt *pr, char **list, u_int size)
{
	char	*display, *cp;
	u_int	 i;

	prompt_clear_complete(pr);
	pr->complete_list = list;
	pr->complete_size = size;

	display = xstrdup("");
	for (i = 0; i < size; i++) {
		xasprintf(&cp, "%s %s", display, list[i]);
		free(display);
		display = cp;
	}
	pr->complete_display = display;
}

/*
 * Complete word. Returns the text to insert when a unique match or a longer
 * common prefix is available; otherwise stores the match list for inline
 * display (and returns NULL) or returns NULL if there is nothing to do.
 */
static char *
prompt_complete(struct prompt *pr, const char *word, u_int offset)
{
	char	**list = NULL, *out = NULL;
	u_int	  size = 0, i;

	if (pr->type != PROMPT_TYPE_COMMAND || offset != 0 ||
	    *word == '\0')
		return (NULL);

	list = prompt_complete_commands(&size, word);
	if (size == 0) {
		free(list);
		return (NULL);
	}
	qsort(list, size, sizeof *list, prompt_complete_sort);
	for (i = 0; i < size; i++)
		log_debug("complete %u: %s", i, list[i]);

	if (size == 1)
		xasprintf(&out, "%s ", list[0]);
	else
		out = prompt_complete_prefix(list, size);
	if (out != NULL && strcmp(word, out) == 0) {
		free(out);
		out = NULL;
	}

	if (out != NULL || size <= 1) {
		/* Inserting (or nothing to show): drop the list. */
		for (i = 0; i < size; i++)
			free(list[i]);
		free(list);
		return (out);
	}

	/* Multiple matches but nothing to insert: keep them for redraw. */
	prompt_store_complete(pr, list, size);
	return (NULL);
}


/* Return the type of the prompt as an enum. */
enum prompt_type
prompt_type(const char *type)
{
	u_int	i;

	for (i = 0; i < PROMPT_NTYPES; i++) {
		if (strcmp(type, prompt_type_string(i)) == 0)
			return (i);
	}
	return (PROMPT_TYPE_INVALID);
}

/* Get prompt type as a string. */
const char *
prompt_type_string(enum prompt_type type)
{
	switch (type) {
	case PROMPT_TYPE_COMMAND:
		return ("command");
	case PROMPT_TYPE_SEARCH:
		return ("search");
	case PROMPT_TYPE_INVALID:
		return ("invalid");
	}
	return ("unknown");
}
