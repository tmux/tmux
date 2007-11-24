/* $Id: input.c,v 1.38 2007-11-24 23:29:49 nicm Exp $ */

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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

#define INPUT_C0CONTROL(ch) 	(ch <= 0x1f)
#define INPUT_INTERMEDIATE(ch)	(ch == 0xa0 || (ch >= 0x20 && ch <= 0x2f))
#define INPUT_PARAMETER(ch)	(ch >= 0x30 && ch <= 0x3f)
#define INPUT_UPPERCASE(ch)	(ch >= 0x40 && ch <= 0x5f)
#define INPUT_LOWERCASE(ch)	(ch >= 0x60 && ch <= 0x7e)
#define INPUT_DELETE(ch)	(ch == 0x7f)
#define INPUT_C1CONTROL(ch)	(ch >= 0x80 && ch <= 0x9f)
#define INPUT_G1DISPLAYABLE(ch)	(ch >= 0xa1 && ch <= 0xfe)
#define INPUT_SPECIAL(ch)	(ch == 0xff)

int	 input_get_argument(struct input_ctx *, u_int, uint16_t *, uint16_t);
int	 input_new_argument(struct input_ctx *);
int	 input_add_argument(struct input_ctx *, u_char ch);

void	*input_state_first(u_char, struct input_ctx *);
void	*input_state_escape(u_char, struct input_ctx *);
void	*input_state_intermediate(u_char, struct input_ctx *);
void	*input_state_title_first(u_char, struct input_ctx *);
void	*input_state_title_second(u_char, struct input_ctx *);
void	*input_state_title_next(u_char, struct input_ctx *);
void	*input_state_sequence_first(u_char, struct input_ctx *);
void	*input_state_sequence_next(u_char, struct input_ctx *);
void	*input_state_sequence_intermediate(u_char, struct input_ctx *);

void	 input_handle_character(u_char, struct input_ctx *);
void	 input_handle_c0_control(u_char, struct input_ctx *);
void	 input_handle_c1_control(u_char, struct input_ctx *);
void	 input_handle_private_two(u_char, struct input_ctx *);
void	 input_handle_standard_two(u_char, struct input_ctx *);
void	 input_handle_sequence(u_char, struct input_ctx *);

void	 input_handle_sequence_cuu(struct input_ctx *);
void	 input_handle_sequence_cud(struct input_ctx *);
void	 input_handle_sequence_cuf(struct input_ctx *);
void	 input_handle_sequence_cub(struct input_ctx *);
void	 input_handle_sequence_dch(struct input_ctx *);
void	 input_handle_sequence_dl(struct input_ctx *);
void	 input_handle_sequence_ich(struct input_ctx *);
void	 input_handle_sequence_il(struct input_ctx *);
void	 input_handle_sequence_vpa(struct input_ctx *);
void	 input_handle_sequence_hpa(struct input_ctx *);
void	 input_handle_sequence_cup(struct input_ctx *);
void	 input_handle_sequence_cup(struct input_ctx *);
void	 input_handle_sequence_ed(struct input_ctx *);
void	 input_handle_sequence_el(struct input_ctx *);
void	 input_handle_sequence_sm(struct input_ctx *);
void	 input_handle_sequence_rm(struct input_ctx *);
void	 input_handle_sequence_decstbm(struct input_ctx *);
void	 input_handle_sequence_sgr(struct input_ctx *);
void	 input_handle_sequence_dsr(struct input_ctx *);

#define input_limit(v, lower, upper) do {	\
	if (v < lower)				\
		v = lower;			\
	if (v > upper)				\
		v = upper;			\
} while (0)

int
input_new_argument(struct input_ctx *ictx)
{
	struct input_arg       *arg;

	ARRAY_EXPAND(&ictx->args, 1);

	arg = &ARRAY_LAST(&ictx->args);
	arg->used = 0;

	return (0);
}

int
input_add_argument(struct input_ctx *ictx, u_char ch)
{
	struct input_arg       *arg;

	if (ARRAY_LENGTH(&ictx->args) == 0)
		return (0);

	arg = &ARRAY_LAST(&ictx->args);
	if (arg->used > (sizeof arg->data) - 1)
		return (-1);
	arg->data[arg->used++] = ch;

	return (0);
}

int
input_get_argument(struct input_ctx *ictx, u_int i, uint16_t *n, uint16_t d)
{
	struct input_arg	*arg;
	const char		*errstr;

	*n = d;
	if (i >= ARRAY_LENGTH(&ictx->args))
		return (0);

	arg = &ARRAY_ITEM(&ictx->args, i);
	if (*arg->data == '\0')
		return (0);

	*n = strtonum(arg->data, 0, UINT16_MAX, &errstr);
	if (errstr != NULL)
		return (-1);
	return (0);
}

void
input_init(struct window *w)
{
	ARRAY_INIT(&w->ictx.args);

	w->ictx.state = input_state_first;
}

void
input_free(struct window *w)
{
	ARRAY_FREE(&w->ictx.args);
}

void
input_parse(struct window *w, struct buffer *b)
{
	struct input_ctx	*ictx = &w->ictx;
	u_char			 ch;

	if (BUFFER_USED(w->in) == 0)
		return;

	ictx->buf = BUFFER_OUT(w->in);
	ictx->len = BUFFER_USED(w->in);
	ictx->off = 0;

	ictx->w = w;
	ictx->b = b;

	log_debug2("entry; buffer=%zu", ictx->len);

	while (ictx->off < ictx->len) {
		ch = ictx->buf[ictx->off++];
		ictx->state = ictx->state(ch, ictx);
	}

	buffer_remove(w->in, ictx->len);
}

void *
input_state_first(u_char ch, struct input_ctx *ictx)
{
	if (INPUT_C0CONTROL(ch)) {
		if (ch == 0x1b)
			return (input_state_escape);
		input_handle_c0_control(ch, ictx);
		return (input_state_first);
	}

	if (INPUT_C1CONTROL(ch)) {
		ch -= 0x40;
		if (ch == '[')
			return (input_state_sequence_first);
		if (ch == ']')
			return (input_state_title_first);
		input_handle_c1_control(ch, ictx);
		return (input_state_first);
	}

	input_handle_character(ch, ictx);
	return (input_state_first);
}
	    
void *
input_state_escape(u_char ch, struct input_ctx *ictx)
{
	/* Treat C1 control and G1 displayable as 7-bit equivalent. */
	if (INPUT_C1CONTROL(ch) || INPUT_G1DISPLAYABLE(ch))
		ch &= 0x7f;

	if (INPUT_C0CONTROL(ch)) {
		input_handle_c0_control(ch, ictx);
		return (input_state_escape);
	}

	if (INPUT_INTERMEDIATE(ch))
		return (input_state_intermediate);

	if (INPUT_PARAMETER(ch)) {
		input_handle_private_two(ch, ictx);
		return (input_state_first);
	}

	if (INPUT_UPPERCASE(ch)) {
		if (ch == '[')
			return (input_state_sequence_first);
		if (ch == ']')
			return (input_state_title_first);
		input_handle_c1_control(ch, ictx);
		return (input_state_first);
	}

	if (INPUT_LOWERCASE(ch)) {
		input_handle_standard_two(ch, ictx);
		return (input_state_first);
	}

	return (input_state_first);
}

void *
input_state_title_first(u_char ch, struct input_ctx *ictx)
{
	if (ch >= '0' && ch <= '9') {
		ictx->title_type = ch - '0';
		return (input_state_title_second);
	}

	return (input_state_first);
}

void *
input_state_title_second(u_char ch, struct input_ctx *ictx)
{
	if (ch == ';') {
		ictx->title_len = 0;
		return (input_state_title_next);
	}

	return (input_state_first);
}

void *
input_state_title_next(u_char ch, struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;

	if (ch == '\007') {
		ictx->title_buf[ictx->title_len++] = '\0';
		switch (ictx->title_type) {
		case 0:
			strlcpy(s->title, ictx->title_buf, sizeof s->title);
			input_store_one(ictx->b, CODE_TITLE, ictx->title_len);
			buffer_write(ictx->b, ictx->title_buf, ictx->title_len);
			break;
		}
		return (input_state_first);
	}

	if (ch >= 0x20) {
		if (ictx->title_len < (sizeof ictx->title_buf) - 1) {
			ictx->title_buf[ictx->title_len++] = ch;
			return (input_state_title_next);
		}
		return (input_state_first);
	}

 	return (input_state_first);
}

void *
input_state_intermediate(u_char ch, struct input_ctx *ictx)
{
	if (INPUT_INTERMEDIATE(ch))
		return (input_state_intermediate);

	if (INPUT_PARAMETER(ch)) {
		input_handle_private_two(ch, ictx);
		return (input_state_first);
	}

	if (INPUT_UPPERCASE(ch) || INPUT_LOWERCASE(ch)) {
		input_handle_standard_two(ch, ictx);
		return (input_state_first);
	}

	return (input_state_first);
}

void *
input_state_sequence_first(u_char ch, struct input_ctx *ictx)
{
	ictx->private = '\0';
	ARRAY_CLEAR(&ictx->args);

	if (INPUT_PARAMETER(ch)) {
		input_new_argument(ictx);	/* XXX extraneous arg if priv */
		if (ch >= 0x3c && ch <= 0x3f) {
			/* Private control sequence. */
			ictx->private = ch;
			return (input_state_sequence_next);
		}
	}		

	/* Pass character on directly. */
	return (input_state_sequence_next(ch, ictx));
}

void *
input_state_sequence_next(u_char ch, struct input_ctx *ictx)
{
	if (INPUT_INTERMEDIATE(ch)) {
		if (input_add_argument(ictx, '\0') != 0)
			return (input_state_first);
		return (input_state_sequence_intermediate);
	}

	if (INPUT_PARAMETER(ch)) {
		if (ch == ';') {
			if (input_add_argument(ictx, '\0') != 0)
				return (input_state_first);
			input_new_argument(ictx);
			return (input_state_sequence_next);
		}
		if (input_add_argument(ictx, ch) != 0)
			return (input_state_first);
		return (input_state_sequence_next);
	}

	if (INPUT_UPPERCASE(ch) || INPUT_LOWERCASE(ch)) {
		if (input_add_argument(ictx, '\0') != 0)
			return (input_state_first);
		input_handle_sequence(ch, ictx);
		return (input_state_first);
	}

	return (input_state_first);
}

void *
input_state_sequence_intermediate(u_char ch, struct input_ctx *ictx)
{
	if (INPUT_INTERMEDIATE(ch))
		return (input_state_sequence_intermediate);

	if (INPUT_UPPERCASE(ch) || INPUT_LOWERCASE(ch)) {
		input_handle_sequence(ch, ictx);
		return (input_state_first);
	}

	return (input_state_first);
}

void
input_handle_character(u_char ch, struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;

	log_debug2("-- ch %zu: %hhu (%c)", ictx->off, ch, ch);
	
	if (s->cx == screen_size_x(s)) {
		if (!screen_hidden(s)) {
			input_store8(ictx->b, '\r');
			input_store8(ictx->b, '\n');
		}

		s->cx = 0;
		screen_display_cursor_down(s);
	} else if (!screen_in_x(s, s->cx) || !screen_in_y(s, s->cy))
		return;

	screen_display_cursor_set(s, ch);
	if (!screen_hidden(s))
		input_store8(ictx->b, ch);

	s->cx++;
}

void
input_handle_c0_control(u_char ch, struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;

	log_debug2("-- c0 %zu: %hhu", ictx->off, ch);

	switch (ch) {
	case '\0':	/* NUL */
		break;
	case '\n':	/* LF */
 		screen_display_cursor_down(s);
		break;
	case '\r':	/* CR */
		s->cx = 0;
		break;
	case '\007':	/* BELL */
		ictx->w->flags |= WINDOW_BELL;
		return;
	case '\010': 	/* BS */
		if (s->cx > 0)
			s->cx--;
		break;
	case '\011': 	/* TAB */
		s->cx = ((s->cx / 8) * 8) + 8;
		if (s->cx > screen_last_x(s)) {
			s->cx = 0;
			screen_display_cursor_down(s);
		}
		if (screen_hidden(s))
			return;
		input_store_two(ictx->b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);
		return;
	case '\016':	/* SO */
		s->attr |= ATTR_DRAWING;
		if (screen_hidden(s))
			return;
		input_store_two(ictx->b, CODE_ATTRIBUTES, s->attr, s->colr);
		return;
	case '\017':	/* SI */
		s->attr &= ~ATTR_DRAWING;
		if (screen_hidden(s))
			return;
		input_store_two(ictx->b, CODE_ATTRIBUTES, s->attr, s->colr);
		return;
	default:
		log_debug("unknown c0: %hhu", ch);
		return;
	}
	if (!screen_hidden(s))
		input_store8(ictx->b, ch);
}

void
input_handle_c1_control(u_char ch, struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;

	log_debug2("-- c1 %zu: %hhu (%c)", ictx->off, ch, ch);

	switch (ch) {
	case 'M':	/* RI */
		screen_display_cursor_up(s);
		if (!screen_hidden(s))
			input_store_zero(ictx->b, CODE_REVERSEINDEX);
		break;
	default:
		log_debug("unknown c1: %hhu", ch);
		break;
	}
}

void
input_handle_private_two(u_char ch, struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;

	log_debug2("-- p2 %zu: %hhu (%c)", ictx->off, ch, ch);

	switch (ch) {
	case '=':	/* DECKPAM */
		if (!screen_hidden(s))
			input_store_zero(ictx->b, CODE_KKEYPADON);
		break;
	case '>':	/* DECKPNM*/
		if (!screen_hidden(s))
			input_store_zero(ictx->b, CODE_KKEYPADOFF);
		break;
	case '7':	/* DECSC */
		s->saved_cx = s->cx;
		s->saved_cy = s->cy;
		s->saved_attr = s->attr;
		s->saved_colr = s->colr;
		s->mode |= MODE_SAVED;
		break;
	case '8':	/* DECRC */
		if (!(s->mode & MODE_SAVED))
			break;
		s->cx = s->saved_cx;
		s->cy = s->saved_cy;
		s->attr = s->saved_attr;
		s->colr = s->saved_colr;
		if (screen_hidden(s))
			break;
		input_store_two(ictx->b, CODE_ATTRIBUTES, s->attr, s->colr);
		input_store_two(ictx->b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);
		break;
	default:
		log_debug("unknown p2: %hhu", ch);
		break;
	}
}

void
input_handle_standard_two(u_char ch, struct input_ctx *ictx)
{
	log_debug2("-- s2 %zu: %hhu (%c)", ictx->off, ch, ch);

	log_debug("unknown s2: %hhu", ch);
}

void
input_handle_sequence(u_char ch, struct input_ctx *ictx)
{
	static const struct {
		u_char	ch;
		void	(*fn)(struct input_ctx *);
	} table[] = {
		{ '@', input_handle_sequence_ich },
		{ 'A', input_handle_sequence_cuu },
		{ 'B', input_handle_sequence_cud },
		{ 'C', input_handle_sequence_cuf },
		{ 'D', input_handle_sequence_cub },
		{ 'G', input_handle_sequence_hpa },
		{ 'H', input_handle_sequence_cup },
		{ 'J', input_handle_sequence_ed },
		{ 'K', input_handle_sequence_el },
		{ 'L', input_handle_sequence_il },
		{ 'M', input_handle_sequence_dl },
		{ 'P', input_handle_sequence_dch },
		{ 'd', input_handle_sequence_vpa },
		{ 'f', input_handle_sequence_cup },
		{ 'h', input_handle_sequence_sm },
		{ 'l', input_handle_sequence_rm },
		{ 'm', input_handle_sequence_sgr },
		{ 'n', input_handle_sequence_dsr },
		{ 'r', input_handle_sequence_decstbm },
	};
	struct screen	 *s = &ictx->w->screen;
	u_int		  i;
	struct input_arg *iarg;
	
	log_debug2("-- sq %zu: %hhu (%c): %u [sx=%u, sy=%u, cx=%u, cy=%u, "
	    "ru=%u, rl=%u]", ictx->off, ch, ch, ARRAY_LENGTH(&ictx->args),
	    screen_size_x(s), screen_size_y(s), s->cx, s->cy, s->rupper,
	    s->rlower);
	for (i = 0; i < ARRAY_LENGTH(&ictx->args); i++) {
		iarg = &ARRAY_ITEM(&ictx->args, i);
		if (*iarg->data != '\0')
			log_debug2("      ++ %u: %s", i, iarg->data);
	}

	/* XXX bsearch? */
	for (i = 0; i < (sizeof table / sizeof table[0]); i++) {
		if (table[i].ch == ch) {
			table[i].fn(ictx);
			return;
		}
	}

	log_debug("unknown sq: %c (%hhu %hhu)", ch, ch, ictx->private);
}

void
input_handle_sequence_cuu(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0 || n > s->cy) {
		log_debug3("cuu: out of range: %hu", n);
		return;
	}

	s->cy -= n;
	if (!screen_hidden(s))
		input_store_one(ictx->b, CODE_CURSORUP, n);
}

void
input_handle_sequence_cud(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_last_y(s) - s->cy);

	s->cy += n;
	if (!screen_hidden(s))
		input_store_one(ictx->b, CODE_CURSORDOWN, n);
}

void
input_handle_sequence_cuf(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_last_x(s) - s->cx);

	s->cx += n;
	if (!screen_hidden(s))
		input_store_one(ictx->b, CODE_CURSORRIGHT, n);
}

void
input_handle_sequence_cub(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0 || n > s->cx) {
		log_debug3("cub: out of range: %hu", n);
		return;
	}

	s->cx -= n;
	if (!screen_hidden(s))
		input_store_one(ictx->b, CODE_CURSORLEFT, n);
}

void
input_handle_sequence_dch(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_last_x(s) - s->cx);

	screen_display_delete_characters(s, s->cx, s->cy, n);
	if (!screen_hidden(s))
		input_store_one(ictx->b, CODE_DELETECHARACTER, n);
}

void
input_handle_sequence_dl(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_last_y(s) - s->cy);

	if (s->cy < s->rupper || s->cy > s->rlower)
		screen_display_delete_lines(s, s->cy, n);
	else
		screen_display_delete_lines_region(s, s->cy, n);
	if (!screen_hidden(s)) 
		input_store_one(ictx->b, CODE_DELETELINE, n);
}

void
input_handle_sequence_ich(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_last_x(s) - s->cx);

	screen_display_insert_characters(s, s->cx, s->cy, n);
	if (!screen_hidden(s))
		input_store_one(ictx->b, CODE_INSERTCHARACTER, n);
}

void
input_handle_sequence_il(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_last_y(s) - s->cy);

	if (s->cy < s->rupper || s->cy > s->rlower)
		screen_display_insert_lines(s, s->cy, n);
	else
		screen_display_insert_lines_region(s, s->cy, n);
	if (!screen_hidden(s))
		input_store_one(ictx->b, CODE_INSERTLINE, n);
}

void
input_handle_sequence_vpa(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_size_y(s));

	s->cy = n - 1;
	if (!screen_hidden(s))
		input_store_two(ictx->b, CODE_CURSORMOVE, n, s->cx + 1);
}

void
input_handle_sequence_hpa(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0)
		return;
	input_limit(n, 1, screen_size_x(s));

	s->cx = n - 1;
	if (!screen_hidden(s))
		input_store_two(ictx->b, CODE_CURSORMOVE, s->cy + 1, n);
}

void
input_handle_sequence_cup(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n, m;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 2)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (input_get_argument(ictx, 1, &m, 1) != 0)
		return;

	input_limit(n, 1, screen_size_y(s));
	input_limit(m, 1, screen_size_x(s));

	s->cx = m - 1;
	s->cy = n - 1;
	if (!screen_hidden(s))
		input_store_two(ictx->b, CODE_CURSORMOVE, n, m);
}

void
input_handle_sequence_ed(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;
	u_int		 i;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (n > 2)
		return;

	switch (n) {
	case 0:
		screen_display_fill_cursor_eos(
		    s, SCREEN_DEFDATA, s->attr, s->colr);

		if (screen_hidden(s))
			break;
		input_store_zero(ictx->b, CODE_CLEARLINE);
		for (i = s->cy + 1; i < screen_size_y(s); i++) {
			input_store_two(ictx->b, CODE_CURSORMOVE, i + 1, 1);
			input_store_zero(ictx->b, CODE_CLEARLINE);
		}
		input_store_two(
		    ictx->b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);
		break;
	case 2:
		screen_display_fill_lines(
		    s, 0, screen_size_y(s), SCREEN_DEFDATA, s->attr, s->colr);

		if (screen_hidden(s))
			break;
		for (i = 0; i < screen_size_y(s); i++) {
			input_store_two(ictx->b, CODE_CURSORMOVE, i + 1, 1);
			input_store_zero(ictx->b, CODE_CLEARLINE);
		}
		input_store_two(
		    ictx->b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);
		break;
	}
}

void
input_handle_sequence_el(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (n > 2)
		return;

	switch (n) {
	case 0:
		screen_display_fill_cursor_eol(
		    s, SCREEN_DEFDATA, s->attr, s->colr);

		if (!screen_hidden(s))
			input_store_zero(ictx->b, CODE_CLEARENDOFLINE);
		break;
	case 1:
		screen_display_fill_cursor_bol(
		    s, SCREEN_DEFDATA, s->attr, s->colr);

		if (!screen_hidden(s))
			input_store_zero(ictx->b, CODE_CLEARSTARTOFLINE);
		break;
	case 2:
		screen_display_fill_line(
		    s, s->cy, SCREEN_DEFDATA, s->attr, s->colr);

		if (!screen_hidden(s))
			input_store_zero(ictx->b, CODE_CLEARLINE);
		break;
	}
}

void
input_handle_sequence_sm(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (ictx->private == '?') {
		switch (n) {
		case 1:		/* GATM */
			s->mode |= MODE_KCURSOR;
			if (!screen_hidden(s))
				input_store_zero(ictx->b, CODE_KCURSORON);
			break;
		case 25:	/* TCEM */
			s->mode |= MODE_CURSOR;
			if (!screen_hidden(s))
				input_store_zero(ictx->b, CODE_CURSORON);
			break;
		default:
			log_debug("unknown SM [%hhu]: %u", ictx->private, n);
			break;
		}
	} else {
		switch (n) {
		case 4:		/* IRM */
			s->mode |= MODE_INSERT;
			if (!screen_hidden(s))
				input_store_zero(ictx->b, CODE_INSERTON);
			break;
		case 34:
			/* Cursor high visibility not supported. */
			break;
		default:
			log_debug("unknown SM [%hhu]: %u", ictx->private, n);
			break;
		}
	}
}

void
input_handle_sequence_rm(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (ictx->private == '?') {
		switch (n) {
		case 1:		/* GATM */
			s->mode &= ~MODE_KCURSOR;
			if (!screen_hidden(s))
				input_store_zero(ictx->b, CODE_KCURSOROFF);
			break;
		case 25:	/* TCEM */
			s->mode &= ~MODE_CURSOR;
			if (!screen_hidden(s))
				input_store_zero(ictx->b, CODE_CURSOROFF);
			break;
		default:
			log_debug("unknown RM [%hhu]: %u", ictx->private, n);
			break;
		}
	} else if (ictx->private == '\0') {
		switch (n) {
		case 4:		/* IRM */
			s->mode &= ~MODE_INSERT;
			if (!screen_hidden(s))
				input_store_zero(ictx->b, CODE_INSERTOFF);
			break;
		case 34:
			/* Cursor high visibility not supported. */
			break;
		default:
			log_debug("unknown RM [%hhu]: %u", ictx->private, n);
			break;
		}
	}
}

void
input_handle_sequence_dsr(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n;
	char		 reply[32];

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (ictx->private == '\0') {
		switch (n) {
		case 6:	/* cursor position */
			xsnprintf(reply, sizeof reply,
			    "\033[%u;%uR", s->cy + 1, s->cx + 1);
			log_debug("cursor request, reply: %s", reply);
			buffer_write(ictx->w->out, reply, strlen(reply));
			break;
		}
	}

}

void
input_handle_sequence_decstbm(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	uint16_t	 n, m;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 2)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;
	if (input_get_argument(ictx, 1, &m, 0) != 0)
		return;

	/* Special case: both zero restores to entire screen. */
	/* XXX this will catch [0;0r and [;r etc too, is this right? */
	if (n == 0 && m == 0) {
		n = 1;
		m = screen_size_y(s);
	}

	input_limit(n, 1, screen_size_y(s));
	input_limit(m, 1, screen_size_y(s));

	if (n > m) {
		log_debug3("decstbm: out of range: %hu,%hu", n, m);
		return;
	}

	/* Cursor moves to top-left. */
	s->cx = 0;
	s->cy = n - 1;

	s->rupper = n - 1;
	s->rlower = m - 1;
	if (!screen_hidden(s))
		input_store_two(ictx->b, CODE_SCROLLREGION, n, m);
}

void
input_handle_sequence_sgr(struct input_ctx *ictx)
{
	struct screen	*s = &ictx->w->screen;
	u_int		 i, n;
	uint16_t	 m;

	n = ARRAY_LENGTH(&ictx->args);
	if (n == 0) {
		s->attr = 0;
		s->colr = SCREEN_DEFCOLR;
	} else {
		for (i = 0; i < n; i++) {
			if (input_get_argument(ictx, i, &m, 0) != 0)
				return;
			switch (m) {
			case 0:
			case 10:
				s->attr = 0;
				s->colr = SCREEN_DEFCOLR;
				break;
			case 1:
				s->attr |= ATTR_BRIGHT;
				break;
			case 2:
				s->attr |= ATTR_DIM;
				break;
			case 3:
				s->attr |= ATTR_ITALICS;
				break;
			case 4:
				s->attr |= ATTR_UNDERSCORE;
				break;
			case 5:
				s->attr |= ATTR_BLINK;
				break;
			case 7:
				s->attr |= ATTR_REVERSE;
				break;
			case 8:
				s->attr |= ATTR_HIDDEN;
				break;
			case 23:
				s->attr &= ~ATTR_ITALICS;
				break;
			case 24:
				s->attr &= ~ATTR_UNDERSCORE;
				break;
			case 30:
			case 31:
			case 32:
			case 33:
			case 34:
			case 35:
			case 36:
			case 37:
				s->colr &= 0x0f;
				s->colr |= (m - 30) << 4;
				break;
			case 39:
				s->colr &= 0x0f;
				s->colr |= 0x80;
				break;
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
				s->colr &= 0xf0;
				s->colr |= m - 40;
				break;
			case 49:
				s->colr &= 0xf0;
				s->colr |= 0x08;
				break;
			}
		}
	}
	if (!screen_hidden(s))
		input_store_two(ictx->b, CODE_ATTRIBUTES, s->attr, s->colr);
}

void
input_store_zero(struct buffer *b, u_char code)
{
	input_store8(b, '\e');
	input_store8(b, code);
}

void
input_store_one(struct buffer *b, u_char code, uint16_t ua)
{
	input_store8(b, '\e');
	input_store8(b, code);
	input_store16(b, ua);
}

void
input_store_two(struct buffer *b, u_char code, uint16_t ua, uint16_t ub)
{
	input_store8(b, '\e');
	input_store8(b, code);
	input_store16(b, ua);
	input_store16(b, ub);
}

void
input_store8(struct buffer *b, uint8_t n)
{
	buffer_ensure(b, 1);
	BUFFER_IN(b)[0] = n;
	buffer_add(b, 1);
}

void
input_store16(struct buffer *b, uint16_t n)
{
	buffer_ensure(b, 2);
	BUFFER_IN(b)[0] = n & 0xff;
	BUFFER_IN(b)[1] = n >> 8;
	buffer_add(b, 2);
}

uint8_t
input_extract8(struct buffer *b)
{
	uint8_t	n;

	n = BUFFER_OUT(b)[0];
	buffer_remove(b, 1);
	return (n);
}

uint16_t
input_extract16(struct buffer *b)
{
	uint16_t	n;

	n = BUFFER_OUT(b)[0] | (BUFFER_OUT(b)[1] << 8);
	buffer_remove(b, 2);
	return (n);
}
