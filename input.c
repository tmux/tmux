/* $OpenBSD: input.c,v 1.2 2009/06/03 19:33:04 nicm Exp $ */

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
int	 input_add_argument(struct input_ctx *, u_char);

void	 input_start_string(struct input_ctx *, int);
void	 input_abort_string(struct input_ctx *);
int	 input_add_string(struct input_ctx *, u_char);
char	*input_get_string(struct input_ctx *);

void	 input_state(struct input_ctx *, void *);

void	 input_state_first(u_char, struct input_ctx *);
void	 input_state_escape(u_char, struct input_ctx *);
void	 input_state_intermediate(u_char, struct input_ctx *);
void	 input_state_sequence_first(u_char, struct input_ctx *);
void	 input_state_sequence_next(u_char, struct input_ctx *);
void	 input_state_sequence_intermediate(u_char, struct input_ctx *);
void	 input_state_string_next(u_char, struct input_ctx *);
void	 input_state_string_escape(u_char, struct input_ctx *);
void	 input_state_utf8(u_char, struct input_ctx *);

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

int	 input_sequence_cmp(const void *, const void *);

struct input_sequence_entry {
	u_char	ch;
	void	(*fn)(struct input_ctx *);
};
const struct input_sequence_entry input_sequence_table[] = {
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

int
input_sequence_cmp(const void *a, const void *b)
{
	int	ai = ((const struct input_sequence_entry *) a)->ch;
	int	bi = ((const struct input_sequence_entry *) b)->ch;

	return (ai - bi);
}

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
input_start_string(struct input_ctx *ictx, int type)
{
	ictx->string_type = type;
	ictx->string_len = 0;
}

void
input_abort_string(struct input_ctx *ictx)
{
	if (ictx->string_buf != NULL)
		xfree(ictx->string_buf);
	ictx->string_buf = NULL;
}

int
input_add_string(struct input_ctx *ictx, u_char ch)
{
	ictx->string_buf = xrealloc(ictx->string_buf, 1, ictx->string_len + 1);
	ictx->string_buf[ictx->string_len++] = ch;

	if (ictx->string_len >= MAXSTRINGLEN) {
		input_abort_string(ictx);
		return (1);
	}

	return (0);
}

char *
input_get_string(struct input_ctx *ictx)
{
	char	*s;

	if (ictx->string_buf == NULL || input_add_string(ictx, '\0') != 0)
		return (xstrdup(""));

	s = ictx->string_buf;
	ictx->string_buf = NULL;
	return (s);
}

void
input_state(struct input_ctx *ictx, void *state)
{
	ictx->state = state;
}

void
input_init(struct window_pane *wp)
{
	struct input_ctx	*ictx = &wp->ictx;

	ARRAY_INIT(&ictx->args);

	ictx->string_len = 0;
	ictx->string_buf = NULL;

 	memcpy(&ictx->cell, &grid_default_cell, sizeof ictx->cell);

	memcpy(&ictx->saved_cell, &grid_default_cell, sizeof ictx->saved_cell);
	ictx->saved_cx = 0;
	ictx->saved_cy = 0;

	input_state(ictx, input_state_first);
}

void
input_free(struct window_pane *wp)
{
	if (wp->ictx.string_buf != NULL)
		xfree(wp->ictx.string_buf);

	ARRAY_FREE(&wp->ictx.args);
}

void
input_parse(struct window_pane *wp)
{
	struct input_ctx	*ictx = &wp->ictx;
	u_char			 ch;

	if (BUFFER_USED(wp->in) == 0)
		return;

	ictx->buf = BUFFER_OUT(wp->in);
	ictx->len = BUFFER_USED(wp->in);
	ictx->off = 0;

	ictx->wp = wp;

	log_debug2("entry; buffer=%zu", ictx->len);

	if (wp->mode == NULL)
		screen_write_start(&ictx->ctx, wp, &wp->base);
	else
		screen_write_start(&ictx->ctx, NULL, &wp->base);

	if (ictx->off != ictx->len)
		wp->window->flags |= WINDOW_ACTIVITY;
	while (ictx->off < ictx->len) {
		ch = ictx->buf[ictx->off++];
		ictx->state(ch, ictx);
	}

	screen_write_stop(&ictx->ctx);

	buffer_remove(wp->in, ictx->len);
}

void
input_state_first(u_char ch, struct input_ctx *ictx)
{
	ictx->intermediate = '\0';

	if (INPUT_C0CONTROL(ch)) {
		if (ch == 0x1b)
			input_state(ictx, input_state_escape);
		else
			input_handle_c0_control(ch, ictx);
		return;
	}

#if 0
  	if (INPUT_C1CONTROL(ch)) {
		ch -= 0x40;
		if (ch == '[')
			input_state(ictx, input_state_sequence_first);
		else if (ch == ']') {
			input_start_string(ictx, STRING_SYSTEM);
			input_state(ictx, input_state_string_next);
		} else if (ch == '_') {
			input_start_string(ictx, STRING_APPLICATION);
			input_state(ictx, input_state_string_next);
		} else
			input_handle_c1_control(ch, ictx);
		return;
	}
#endif

	if (INPUT_DELETE(ch))
		return;

	input_handle_character(ch, ictx);
}

void
input_state_escape(u_char ch, struct input_ctx *ictx)
{
	/* Treat C1 control and G1 displayable as 7-bit equivalent. */
	if (INPUT_C1CONTROL(ch) || INPUT_G1DISPLAYABLE(ch))
		ch &= 0x7f;

	if (INPUT_C0CONTROL(ch)) {
		input_handle_c0_control(ch, ictx);
		return;
	}

	if (INPUT_INTERMEDIATE(ch)) {
		log_debug2(":: in1 %zu: %hhu (%c)", ictx->off, ch, ch);
		ictx->intermediate = ch;
		input_state(ictx, input_state_intermediate);
		return;
	}

	if (INPUT_PARAMETER(ch)) {
		input_state(ictx, input_state_first);
		input_handle_private_two(ch, ictx);
		return;
	}

	if (INPUT_UPPERCASE(ch)) {
		if (ch == '[')
			input_state(ictx, input_state_sequence_first);
		else if (ch == ']') {
			input_start_string(ictx, STRING_SYSTEM);
			input_state(ictx, input_state_string_next);
		} else if (ch == '_') {
			input_start_string(ictx, STRING_APPLICATION);
			input_state(ictx, input_state_string_next);
		} else {
			input_state(ictx, input_state_first);
			input_handle_c1_control(ch, ictx);
		}
		return;
	}

	if (INPUT_LOWERCASE(ch)) {
		input_state(ictx, input_state_first);
		input_handle_standard_two(ch, ictx);
		return;
	}

	input_state(ictx, input_state_first);
}

void
input_state_intermediate(u_char ch, struct input_ctx *ictx)
{
	if (INPUT_INTERMEDIATE(ch)) {
		/* Multiple intermediates currently ignored. */
		log_debug2(":: in2 %zu: %hhu (%c)", ictx->off, ch, ch);
		return;
	}

	if (INPUT_PARAMETER(ch)) {
		input_state(ictx, input_state_first);
		input_handle_private_two(ch, ictx);
		return;
	}

	if (INPUT_UPPERCASE(ch) || INPUT_LOWERCASE(ch)) {
		input_state(ictx, input_state_first);
		input_handle_standard_two(ch, ictx);
		return;
	}

	input_state(ictx, input_state_first);
}

void
input_state_sequence_first(u_char ch, struct input_ctx *ictx)
{
	ictx->private = '\0';
	ARRAY_CLEAR(&ictx->args);

	input_state(ictx, input_state_sequence_next);

	if (INPUT_PARAMETER(ch)) {
		input_new_argument(ictx);
		if (ch >= 0x3c && ch <= 0x3f) {
			/* Private control sequence. */
			ictx->private = ch;
			return;
		}
	}

	/* Pass character on directly. */
	input_state_sequence_next(ch, ictx);
}

void
input_state_sequence_next(u_char ch, struct input_ctx *ictx)
{
	if (INPUT_INTERMEDIATE(ch)) {
		if (input_add_argument(ictx, '\0') != 0)
			input_state(ictx, input_state_first);
		else {
			log_debug2(":: si1 %zu: %hhu (%c)", ictx->off, ch, ch);
			input_state(ictx, input_state_sequence_intermediate);
		}
		return;
	}

	if (INPUT_PARAMETER(ch)) {
		if (ch == ';') {
			if (input_add_argument(ictx, '\0') != 0)
				input_state(ictx, input_state_first);
			else
				input_new_argument(ictx);
		} else if (input_add_argument(ictx, ch) != 0)
			input_state(ictx, input_state_first);
		return;
	}

	if (INPUT_UPPERCASE(ch) || INPUT_LOWERCASE(ch)) {
		if (input_add_argument(ictx, '\0') != 0)
			input_state(ictx, input_state_first);
		else {
			input_state(ictx, input_state_first);
			input_handle_sequence(ch, ictx);
		}
		return;
	}

	input_state(ictx, input_state_first);
}

void
input_state_sequence_intermediate(u_char ch, struct input_ctx *ictx)
{
	if (INPUT_INTERMEDIATE(ch)) {
		log_debug2(":: si2 %zu: %hhu (%c)", ictx->off, ch, ch);
		return;
	}

	if (INPUT_UPPERCASE(ch) || INPUT_LOWERCASE(ch)) {
		input_state(ictx, input_state_first);
		input_handle_sequence(ch, ictx);
		return;
	}

	input_state(ictx, input_state_first);
}

void
input_state_string_next(u_char ch, struct input_ctx *ictx)
{
	if (ch == 0x1b) {
		input_state(ictx, input_state_string_escape);
		return;
	}
	if (ch == 0x07) {
		input_state_string_escape(ch, ictx);
		return;
	}

	if (ch >= 0x20) {
		if (input_add_string(ictx, ch) != 0)
			input_state(ictx, input_state_first);
		return;
	}
}

void
input_state_string_escape(u_char ch, struct input_ctx *ictx)
{
	char	*s;

	if (ch == '\007' || ch == '\\') {
		input_state(ictx, input_state_first);
		switch (ictx->string_type) {
		case STRING_SYSTEM:
			if (ch != '\007')
				return;
			s = input_get_string(ictx);
			if ((s[0] != '0' && s[0] != '2') || s[1] != ';') {
				xfree(s);
				return;
			}
			screen_set_title(ictx->ctx.s, s + 2);
			server_status_window(ictx->wp->window);
			xfree(s);
			break;
		case STRING_APPLICATION:
			if (ch != '\\')
				return;
			s = input_get_string(ictx);
			screen_set_title(ictx->ctx.s, s);
			server_status_window(ictx->wp->window);
			xfree(s);
			break;
		case STRING_NAME:
			if (ch != '\\')
				return;
			xfree(ictx->wp->window->name);
			ictx->wp->window->name = input_get_string(ictx);
			server_status_window(ictx->wp->window);
			break;
		}
		return;
	}

	input_state(ictx, input_state_string_next);
	input_state_string_next(ch, ictx);
}

void
input_state_utf8(u_char ch, struct input_ctx *ictx)
{
	log_debug2("-- un %zu: %hhu (%c)", ictx->off, ch, ch);

	ictx->utf8_buf[ictx->utf8_off++] = ch;
	if (--ictx->utf8_len != 0)
		return;
	input_state(ictx, input_state_first);

	ictx->cell.flags |= GRID_FLAG_UTF8;
	screen_write_cell(&ictx->ctx, &ictx->cell, ictx->utf8_buf);
	ictx->cell.flags &= ~GRID_FLAG_UTF8;
}

void
input_handle_character(u_char ch, struct input_ctx *ictx)
{
	struct window_pane	*wp = ictx->wp;

	if (ch > 0x7f && options_get_number(&wp->window->options, "utf8")) {
		/*
		 * UTF-8 sequence.
		 *
		 * 11000010-11011111 C2-DF start of 2-byte sequence
		 * 11100000-11101111 E0-EF start of 3-byte sequence
		 * 11110000-11110100 F0-F4 start of 4-byte sequence
		 */
		memset(ictx->utf8_buf, 0xff, sizeof ictx->utf8_buf);
		ictx->utf8_buf[0] = ch;
		ictx->utf8_off = 1;

		if (ch >= 0xc2 && ch <= 0xdf) {
			log_debug2("-- u2 %zu: %hhu (%c)", ictx->off, ch, ch);
			input_state(ictx, input_state_utf8);
			ictx->utf8_len = 1;
			return;
		}
		if (ch >= 0xe0 && ch <= 0xef) {
			log_debug2("-- u3 %zu: %hhu (%c)", ictx->off, ch, ch);
			input_state(ictx, input_state_utf8);
			ictx->utf8_len = 2;
			return;
		}
		if (ch >= 0xf0 && ch <= 0xf4) {
			log_debug2("-- u4 %zu: %hhu (%c)", ictx->off, ch, ch);
			input_state(ictx, input_state_utf8);
			ictx->utf8_len = 3;
			return;
		}
	}
	log_debug2("-- ch %zu: %hhu (%c)", ictx->off, ch, ch);

	ictx->cell.data = ch;
	screen_write_cell(&ictx->ctx, &ictx->cell, ictx->utf8_buf);
}

void
input_handle_c0_control(u_char ch, struct input_ctx *ictx)
{
	struct screen	*s = ictx->ctx.s;

	log_debug2("-- c0 %zu: %hhu", ictx->off, ch);

	switch (ch) {
	case '\0':	/* NUL */
		break;
	case '\n':	/* LF */
		screen_write_linefeed(&ictx->ctx);
		break;
	case '\r':	/* CR */
		screen_write_carriagereturn(&ictx->ctx);
		break;
	case '\007':	/* BELL */
		ictx->wp->window->flags |= WINDOW_BELL;
		break;
	case '\010': 	/* BS */
		screen_write_cursorleft(&ictx->ctx, 1);
		break;
	case '\011': 	/* TAB */
		s->cx = ((s->cx / 8) * 8) + 8;
		if (s->cx > screen_size_x(s) - 1) {
			s->cx = 0;
			screen_write_cursordown(&ictx->ctx, 1);
		}
		screen_write_cursormove(&ictx->ctx, s->cx, s->cy);
		break;
	case '\016':	/* SO */
		ictx->cell.attr |= GRID_ATTR_CHARSET;
		break;
	case '\017':	/* SI */
		ictx->cell.attr &= ~GRID_ATTR_CHARSET;
		break;
	default:
		log_debug("unknown c0: %hhu", ch);
		break;
	}
}

void
input_handle_c1_control(u_char ch, struct input_ctx *ictx)
{
	log_debug2("-- c1 %zu: %hhu (%c)", ictx->off, ch, ch);

	switch (ch) {
	case 'E': 	/* NEL */
		screen_write_carriagereturn(&ictx->ctx);
		screen_write_linefeed(&ictx->ctx);
		break;
	case 'M':	/* RI */
		screen_write_reverseindex(&ictx->ctx);
		break;
	default:
		log_debug("unknown c1: %hhu", ch);
		break;
	}
}

void
input_handle_private_two(u_char ch, struct input_ctx *ictx)
{
	struct screen	*s = ictx->ctx.s;

	log_debug2(
	    "-- p2 %zu: %hhu (%c) %hhu", ictx->off, ch, ch, ictx->intermediate);

	switch (ch) {
	case '0':	/* Dscs (graphics) */
		/*
		 * Not really supported, but fake it up enough for those that
		 * use it to switch character sets (by redefining G0 to
		 * graphics set, rather than switching to G1).
		 */
		switch (ictx->intermediate) {
		case '(':	/* G0 */
			ictx->cell.attr |= GRID_ATTR_CHARSET;
			break;
		}
		break;
	case '=':	/* DECKPAM */
		screen_write_kkeypadmode(&ictx->ctx, 1);
		log_debug("kkeypad on (application mode)");
		break;
	case '>':	/* DECKPNM */
		screen_write_kkeypadmode(&ictx->ctx, 0);
		log_debug("kkeypad off (number mode)");
		break;
	case '7':	/* DECSC */
		memcpy(&ictx->saved_cell, &ictx->cell, sizeof ictx->saved_cell);
		ictx->saved_cx = s->cx;
		ictx->saved_cy = s->cy;
		break;
	case '8':	/* DECRC */
		memcpy(&ictx->cell, &ictx->saved_cell, sizeof ictx->cell);
		screen_write_cursormove(
		    &ictx->ctx, ictx->saved_cx, ictx->saved_cy);
		break;
	default:
		log_debug("unknown p2: %hhu", ch);
		break;
	}
}

void
input_handle_standard_two(u_char ch, struct input_ctx *ictx)
{
	log_debug2(
	    "-- s2 %zu: %hhu (%c) %hhu", ictx->off, ch, ch, ictx->intermediate);

	switch (ch) {
	case 'B':	/* Dscs (ASCII) */
		/*
		 * Not really supported, but fake it up enough for those that
		 * use it to switch character sets (by redefining G0 to
		 * graphics set, rather than switching to G1).
		 */
		switch (ictx->intermediate) {
		case '(':	/* G0 */
			ictx->cell.attr &= ~GRID_ATTR_CHARSET;
			break;
		}
		break;
	case 'c':	/* RIS */
		memcpy(&ictx->cell, &grid_default_cell, sizeof ictx->cell);

		memcpy(&ictx->saved_cell, &ictx->cell, sizeof ictx->saved_cell);
		ictx->saved_cx = 0;
		ictx->saved_cy = 0;

		screen_write_scrollregion(
		    &ictx->ctx, 0, screen_size_y(ictx->ctx.s) - 1);

		screen_write_insertmode(&ictx->ctx, 0);
		screen_write_kcursormode(&ictx->ctx, 0);
		screen_write_kkeypadmode(&ictx->ctx, 0);
		screen_write_mousemode(&ictx->ctx, 0);

		screen_write_clearscreen(&ictx->ctx);
		screen_write_cursormove(&ictx->ctx, 0, 0);
		break;
	case 'k':
		input_start_string(ictx, STRING_NAME);
		input_state(ictx, input_state_string_next);
		break;
	default:
		log_debug("unknown s2: %hhu", ch);
		break;
	}
}

void
input_handle_sequence(u_char ch, struct input_ctx *ictx)
{
	struct input_sequence_entry	*entry, find;
	struct screen	 		*s = ictx->ctx.s;
	u_int			         i;
	struct input_arg 		*iarg;

	log_debug2("-- sq %zu: %hhu (%c): %u [sx=%u, sy=%u, cx=%u, cy=%u, "
	    "ru=%u, rl=%u]", ictx->off, ch, ch, ARRAY_LENGTH(&ictx->args),
	    screen_size_x(s), screen_size_y(s), s->cx, s->cy, s->rupper,
	    s->rlower);
	for (i = 0; i < ARRAY_LENGTH(&ictx->args); i++) {
		iarg = &ARRAY_ITEM(&ictx->args, i);
		if (*iarg->data != '\0')
			log_debug2("      ++ %u: %s", i, iarg->data);
	}

	find.ch = ch;
	entry = bsearch(&find,
	    input_sequence_table, nitems(input_sequence_table),
	    sizeof input_sequence_table[0], input_sequence_cmp);
	if (entry != NULL)
		entry->fn(ictx);
	else
		log_debug("unknown sq: %c (%hhu %hhu)", ch, ch, ictx->private);
}

void
input_handle_sequence_cuu(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_cursorup(&ictx->ctx, n);
}

void
input_handle_sequence_cud(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_cursordown(&ictx->ctx, n);
}

void
input_handle_sequence_cuf(struct input_ctx *ictx)
{
	uint16_t n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_cursorright(&ictx->ctx, n);
}

void
input_handle_sequence_cub(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_cursorleft(&ictx->ctx, n);
}

void
input_handle_sequence_dch(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_deletecharacter(&ictx->ctx, n);
}

void
input_handle_sequence_dl(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_deleteline(&ictx->ctx, n);
}

void
input_handle_sequence_ich(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_insertcharacter(&ictx->ctx, n);
}

void
input_handle_sequence_il(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_insertline(&ictx->ctx, n);
}

void
input_handle_sequence_vpa(struct input_ctx *ictx)
{
	struct screen  *s = ictx->ctx.s;
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_cursormove(&ictx->ctx, s->cx, n - 1);
}

void
input_handle_sequence_hpa(struct input_ctx *ictx)
{
	struct screen  *s = ictx->ctx.s;
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (n == 0)
		n = 1;

	screen_write_cursormove(&ictx->ctx, n - 1, s->cy);
}

void
input_handle_sequence_cup(struct input_ctx *ictx)
{
	uint16_t	n, m;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 2)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;
	if (input_get_argument(ictx, 1, &m, 1) != 0)
		return;
	if (n == 0)
		n = 1;
	if (m == 0)
		m = 1;

	screen_write_cursormove(&ictx->ctx, m - 1, n - 1);
}

void
input_handle_sequence_ed(struct input_ctx *ictx)
{
	uint16_t	n;

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
		screen_write_clearendofscreen(&ictx->ctx);
		break;
	case 1:
		screen_write_clearstartofscreen(&ictx->ctx);
		break;
	case 2:
		screen_write_clearscreen(&ictx->ctx);
		break;
	}
}

void
input_handle_sequence_el(struct input_ctx *ictx)
{
	uint16_t	n;

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
		screen_write_clearendofline(&ictx->ctx);
		break;
	case 1:
		screen_write_clearstartofline(&ictx->ctx);
		break;
	case 2:
		screen_write_clearline(&ictx->ctx);
		break;
	}
}

void
input_handle_sequence_sm(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (ictx->private == '?') {
		switch (n) {
		case 1:		/* GATM */
			screen_write_kcursormode(&ictx->ctx, 1);
			log_debug("kcursor on");
			break;
		case 25:	/* TCEM */
			screen_write_cursormode(&ictx->ctx, 1);
			log_debug("cursor on");
			break;
		case 1000:
			screen_write_mousemode(&ictx->ctx, 1);
			log_debug("mouse on");
			break;
		default:
			log_debug("unknown SM [%hhu]: %u", ictx->private, n);
			break;
		}
	} else {
		switch (n) {
		case 4:		/* IRM */
			screen_write_insertmode(&ictx->ctx, 1);
			log_debug("insert on");
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
	uint16_t	 n;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (ictx->private == '?') {
		switch (n) {
		case 1:		/* GATM */
			screen_write_kcursormode(&ictx->ctx, 0);
			log_debug("kcursor off");
			break;
		case 25:	/* TCEM */
			screen_write_cursormode(&ictx->ctx, 0);
			log_debug("cursor off");
			break;
		case 1000:
			screen_write_mousemode(&ictx->ctx, 0);
			log_debug("mouse off");
			break;
		default:
			log_debug("unknown RM [%hhu]: %u", ictx->private, n);
			break;
		}
	} else if (ictx->private == '\0') {
		switch (n) {
		case 4:		/* IRM */
			screen_write_insertmode(&ictx->ctx, 0);
			log_debug("insert off");
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
	struct screen  *s = ictx->ctx.s;
	uint16_t	n;
	char		reply[32];

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
			buffer_write(ictx->wp->out, reply, strlen(reply));
			break;
		}
	}

}

void
input_handle_sequence_decstbm(struct input_ctx *ictx)
{
	struct screen  *s = ictx->ctx.s;
	uint16_t	n, m;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 2)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;
	if (input_get_argument(ictx, 1, &m, 0) != 0)
		return;
	if (n == 0)
		n = 1;
	if (m == 0)
		m = screen_size_y(s);

	screen_write_scrollregion(&ictx->ctx, n - 1, m - 1);
}

void
input_handle_sequence_sgr(struct input_ctx *ictx)
{
	struct grid_cell       *gc = &ictx->cell;
	u_int			i;
	uint16_t		m, o;
	u_char			attr;

	if (ARRAY_LENGTH(&ictx->args) == 0) {
		attr = gc->attr;
		memcpy(gc, &grid_default_cell, sizeof *gc);
 		gc->attr |= (attr & GRID_ATTR_CHARSET);
		return;
	}

	for (i = 0; i < ARRAY_LENGTH(&ictx->args); i++) {
		if (input_get_argument(ictx, i, &m, 0) != 0)
			return;

		if (m == 38 || m == 48) {
			i++;
			if (input_get_argument(ictx, i, &o, 0) != 0)
				return;
			if (o != 5)
				continue;

			i++;
			if (input_get_argument(ictx, i, &o, 0) != 0)
				return;
			if (m == 38) {
				gc->flags |= GRID_FLAG_FG256;
				gc->fg = o;
			} else if (m == 48) {
				gc->flags |= GRID_FLAG_BG256;
				gc->bg = o;
			}
			continue;
		}

		switch (m) {
		case 0:
		case 10:
			attr = gc->attr;
			memcpy(gc, &grid_default_cell, sizeof *gc);
			gc->attr |= (attr & GRID_ATTR_CHARSET);
			break;
		case 1:
			gc->attr |= GRID_ATTR_BRIGHT;
			break;
		case 2:
			gc->attr |= GRID_ATTR_DIM;
			break;
		case 3:
			gc->attr |= GRID_ATTR_ITALICS;
			break;
		case 4:
			gc->attr |= GRID_ATTR_UNDERSCORE;
			break;
		case 5:
			gc->attr |= GRID_ATTR_BLINK;
			break;
		case 7:
			gc->attr |= GRID_ATTR_REVERSE;
			break;
		case 8:
			gc->attr |= GRID_ATTR_HIDDEN;
			break;
		case 22:
			gc->attr &= ~(GRID_ATTR_BRIGHT|GRID_ATTR_DIM);
			break;
		case 23:
			gc->attr &= ~GRID_ATTR_ITALICS;
			break;
		case 24:
			gc->attr &= ~GRID_ATTR_UNDERSCORE;
			break;
		case 25:
			gc->attr &= ~GRID_ATTR_BLINK;
			break;
		case 27:
			gc->attr &= ~GRID_ATTR_REVERSE;
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			gc->flags &= ~GRID_FLAG_FG256;
			gc->fg = m - 30;
			break;
		case 39:
			gc->flags &= ~GRID_FLAG_FG256;
			gc->fg = 8;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			gc->flags &= ~GRID_FLAG_BG256;
			gc->bg = m - 40;
			break;
		case 49:
			gc->flags &= ~GRID_FLAG_BG256;
			gc->bg = 8;
			break;
		}
	}
}
