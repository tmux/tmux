/* $Id: input.c,v 1.6 2007-09-28 22:47:21 nicm Exp $ */

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
#include <string.h>
#include <stdlib.h>

#include "tmux.h"

struct {
	u_char		 first;
	u_char		 last;
	enum input_class class;
} input_table[] = {
	{ 0x00, 0x1f, INPUT_C0CONTROL },
	{ 0x20, 0x20, INPUT_SPACE },
	{ 0x21, 0x2F, INPUT_INTERMEDIATE },	/* also INPUT_SPACE */
	{ 0x30, 0x3F, INPUT_PARAMETER },
	{ 0x40, 0x5F, INPUT_UPPERCASE },
	{ 0x60, 0x7E, INPUT_LOWERCASE },
	{ 0x7F, 0x7F, INPUT_DELETE },
	{ 0x80, 0x9F, INPUT_C1CONTROL },
	{ 0xA0, 0xA0, INPUT_SPACE },
	{ 0xA1, 0xFE, INPUT_G1DISPLAYABLE },
	{ 0xFF, 0xFF, INPUT_SPECIAL },
};
#define NINPUTCLASS (sizeof input_table / sizeof input_table[0])

enum input_class input_lookup_class(u_char);
int	 input_get_argument(struct input_ctx *, u_int, uint16_t *, uint16_t);

void	*input_state_first(u_char, enum input_class, struct input_ctx *);
void	*input_state_escape(u_char, enum input_class, struct input_ctx *);
void	*input_state_intermediate(u_char, enum input_class, struct input_ctx *);
void	*input_state_sequence_first(
	     u_char, enum input_class, struct input_ctx *);
void	*input_state_sequence_next(
    	     u_char, enum input_class, struct input_ctx *);
void	*input_state_sequence_intermediate(
    	     u_char, enum input_class, struct input_ctx *);

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

enum input_class
input_lookup_class(u_char ch)
{
	enum input_class	iclass;
	u_int			i;

	iclass = INPUT_SPACE;
	for (i = 0; i < NINPUTCLASS; i++) {
		if (ch >= input_table[i].first && ch <= input_table[i].last) {
			iclass = input_table[i].class;
			break;
		}
	}
	if (i == NINPUTCLASS)
		fatalx("character without class");

	return (iclass);
}

int
input_get_argument(struct input_ctx *ictx, u_int i, uint16_t *n, uint16_t d)
{
	struct input_arg	*arg;
	char			 tmp[64];
	const char		*errstr;

	*n = d;
	if (i >= ARRAY_LENGTH(&ictx->args))
		return (0);

	arg = &ARRAY_ITEM(&ictx->args, i);
	if (arg->len == 0)
		return (0);

	if (arg->len > sizeof tmp - 1)
		return (-1);
	memcpy(tmp, ictx->buf + arg->off, arg->len);
	tmp[arg->len] = '\0';

	*n = strtonum(tmp, 0, UINT16_MAX, &errstr);
	if (errstr != NULL)
		return (-1);
	return (0);
}

void
input_init(struct input_ctx *ictx, struct screen *s)
{
	ictx->s = s;

	ictx->intoff = 0;
	ictx->intlen = 0;

	ARRAY_INIT(&ictx->args);

	ictx->state = input_state_first;
}

void
input_free(struct input_ctx *ictx)
{
	ARRAY_FREE(&ictx->args);
}

size_t
input_parse(struct input_ctx *ictx, u_char *buf, size_t len, struct buffer *b)
{
	enum input_class	iclass;
	u_char			ch;

	ictx->buf = buf;
	ictx->len = len;
	ictx->off = 0;

	ictx->b = b;

	log_debug2("entry; buffer=%zu", ictx->len);

	while (ictx->off < ictx->len) {
		ch = ictx->buf[ictx->off++];
		iclass = input_lookup_class(ch);
		ictx->state = ictx->state(ch, iclass, ictx);
	}

	return (ictx->len);
}

void *
input_state_first(u_char ch, enum input_class iclass, struct input_ctx *ictx)
{
	log_debug2("first (%hhu); sx=%u, sy=%u, cx=%u, cy=%u",
	    ch, ictx->s->sx, ictx->s->sy, ictx->s->cx, ictx->s->cy);

	switch (iclass) {
	case INPUT_C0CONTROL:
		if (ch == 0x1b)
			return (input_state_escape);
		input_handle_c0_control(ch, ictx);
		break;
	case INPUT_C1CONTROL:
		ch -= 0x40;
		if (ch == '[')
			return (input_state_sequence_first);
		input_handle_c1_control(ch, ictx);
		break;
	case INPUT_SPACE:
	case INPUT_INTERMEDIATE:
	case INPUT_PARAMETER:
	case INPUT_UPPERCASE:
	case INPUT_LOWERCASE:
	case INPUT_DELETE:
	case INPUT_G1DISPLAYABLE:
	case INPUT_SPECIAL:
		input_handle_character(ch, ictx);
		break;
	}
	return (input_state_first);
}

void *
input_state_escape(u_char ch, enum input_class iclass, struct input_ctx *ictx)
{
	if (iclass == INPUT_C1CONTROL || iclass == INPUT_G1DISPLAYABLE) {
		/* Treat as 7-bit equivalent. */
		ch &= 0x7f;
		iclass = input_lookup_class(ch);
	}

	switch (iclass) {
	case INPUT_C0CONTROL:
		input_handle_c0_control(ch, ictx);
		return (input_state_escape);
	case INPUT_SPACE:
	case INPUT_INTERMEDIATE:
		ictx->intoff = ictx->off;
		ictx->intlen = 1;
		return (input_state_intermediate);
	case INPUT_PARAMETER:
		input_handle_private_two(ch, ictx);
		break;
	case INPUT_UPPERCASE:
		if (ch == '[')
			return (input_state_sequence_first);
		input_handle_c1_control(ch, ictx);
		break;
	case INPUT_LOWERCASE:
		input_handle_standard_two(ch, ictx);
		break;
	case INPUT_DELETE:
	case INPUT_SPECIAL:
	case INPUT_C1CONTROL:
	case INPUT_G1DISPLAYABLE:
		break;
	}	
	return (input_state_first);
}

void *
input_state_intermediate(
    u_char ch, enum input_class iclass, struct input_ctx *ictx)
{
	switch (iclass) {
	case INPUT_SPACE:
	case INPUT_INTERMEDIATE:
		ictx->intlen++;
		return (input_state_intermediate);
	case INPUT_PARAMETER:
		input_handle_private_two(ch, ictx);
		break;
	case INPUT_UPPERCASE:
	case INPUT_LOWERCASE:
		input_handle_standard_two(ch, ictx);
		break;
	case INPUT_C0CONTROL:
	case INPUT_DELETE:
	case INPUT_SPECIAL:
	case INPUT_C1CONTROL:
	case INPUT_G1DISPLAYABLE:
		break;
	}	
	return (input_state_first);
}

void *
input_state_sequence_first(
    u_char ch, enum input_class iclass, struct input_ctx *ictx)
{
	ictx->private = '\0';

	switch (iclass) {
	case INPUT_PARAMETER:
		if (ch >= 0x3c && ch <= 0x3f) {
			/* Private control sequence. */
			ictx->private = ch;
			
			ictx->saved = ictx->off;
			return (input_state_sequence_next);
		}
		break;
	case INPUT_C0CONTROL:
	case INPUT_C1CONTROL:
	case INPUT_SPACE:
	case INPUT_INTERMEDIATE:
	case INPUT_UPPERCASE:
	case INPUT_LOWERCASE:
	case INPUT_DELETE:
	case INPUT_G1DISPLAYABLE:
	case INPUT_SPECIAL:
		break;
	}		

	/* Pass this character to next state directly. */
	ictx->saved = ictx->off - 1;
	return (input_state_sequence_next(ch, iclass, ictx));
}

void *
input_state_sequence_next(
    u_char ch, enum input_class iclass, struct input_ctx *ictx)
{
	struct input_arg	*iarg;

	switch (iclass) {
	case INPUT_SPACE:
	case INPUT_INTERMEDIATE:
		if (ictx->saved != ictx->off) {
			ARRAY_EXPAND(&ictx->args, 1);
			iarg = &ARRAY_LAST(&ictx->args);
			iarg->off = ictx->saved;
			iarg->len = ictx->off - ictx->saved - 1;
		}
		ictx->intoff = ictx->off;
		ictx->intlen = 1;
		return (input_state_sequence_intermediate);
	case INPUT_PARAMETER:
		if (ch == ';') {
			ARRAY_EXPAND(&ictx->args, 1);
			iarg = &ARRAY_LAST(&ictx->args);
			iarg->off = ictx->saved;
			iarg->len = ictx->off - ictx->saved - 1;

			ictx->saved = ictx->off;
			return (input_state_sequence_next);
		}
		return (input_state_sequence_next);
	case INPUT_UPPERCASE:
	case INPUT_LOWERCASE:
		if (ictx->saved != ictx->off) {
			ARRAY_EXPAND(&ictx->args, 1);
			iarg = &ARRAY_LAST(&ictx->args);
			iarg->off = ictx->saved;
			iarg->len = ictx->off - ictx->saved - 1;
		}
		input_handle_sequence(ch, ictx);
		break;
	case INPUT_C0CONTROL:
	case INPUT_C1CONTROL:
	case INPUT_DELETE:
	case INPUT_SPECIAL:
	case INPUT_G1DISPLAYABLE:
		break;
	}	
	return (input_state_first);
}

void *
input_state_sequence_intermediate(
    u_char ch, enum input_class iclass, struct input_ctx *ictx)
{
	switch (iclass) {
	case INPUT_SPACE:
	case INPUT_INTERMEDIATE:
		ictx->intlen++;
		return (input_state_sequence_intermediate);
	case INPUT_UPPERCASE:
	case INPUT_LOWERCASE:
		input_handle_sequence(ch, ictx);
		break;
	case INPUT_PARAMETER:
	case INPUT_C0CONTROL:
	case INPUT_DELETE:
	case INPUT_SPECIAL:
	case INPUT_C1CONTROL:
	case INPUT_G1DISPLAYABLE:
		break;
	}	
	return (input_state_first);
}

void
input_handle_character(u_char ch, struct input_ctx *ictx)
{
	log_debug2("-- ch %zu: %hhu (%c)", ictx->off, ch, ch);

	screen_write_character(ictx->s, ch);
	input_store8(ictx->b, ch);
}

void
input_handle_c0_control(u_char ch, struct input_ctx *ictx)
{
	log_debug2("-- c0 %zu: %hhu", ictx->off, ch);

	switch (ch) {
	case '\0':	/* NUL */
		break;
	case '\n':	/* LF */
 		screen_cursor_down_scroll(ictx->s);
		break;
	case '\r':	/* CR */
		ictx->s->cx = 0;
		break;
	case '\010': 	/* BS */
		if (ictx->s->cx > 0)
			ictx->s->cx--;
		break;
	default:
		log_debug("unknown c0: %hhu", ch);
		return;
	}
	input_store8(ictx->b, ch);
}

void
input_handle_c1_control(u_char ch, struct input_ctx *ictx)
{
	log_debug2("-- c1 %zu: %hhu (%c)", ictx->off, ch, ch);

	/* XXX window title */
	switch (ch) {
	case 'M':	/* RI */
		screen_cursor_up_scroll(ictx->s);
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
	log_debug2("-- p2 %zu: %hhu (%c) (%zu, %zu)",
	    ictx->off, ch, ch, ictx->intoff, ictx->intlen);

	switch (ch) {
	case '=':	/* DECKPAM */
		input_store_zero(ictx->b, CODE_KKEYPADON);
		break;
	case '>':	/* DECKPNM*/
		input_store_zero(ictx->b, CODE_KKEYPADOFF);
		break;
	default:
		log_debug("unknown p2: %hhu", ch);
		break;
	}
}

void
input_handle_standard_two(u_char ch, struct input_ctx *ictx)
{
	log_debug2("-- s2 %zu: %hhu (%c) (%zu,%zu)",
	    ictx->off, ch, ch, ictx->intoff, ictx->intlen);

	log_debug("unknown s2: %hhu", ch);
}

void
input_handle_sequence(u_char ch, struct input_ctx *ictx)
{
	static const struct {
		u_char	ch;
		void	(*fn)(struct input_ctx *);
	} table[] = {
		{ 'A', input_handle_sequence_cuu },
		{ 'B', input_handle_sequence_cud },
		{ 'C', input_handle_sequence_cuf },
		{ 'D', input_handle_sequence_cub },
		{ 'P', input_handle_sequence_dch },
		{ 'M', input_handle_sequence_dl },
		{ '@', input_handle_sequence_ich },
		{ 'L', input_handle_sequence_il },
		{ 'd', input_handle_sequence_vpa },
		{ 'G', input_handle_sequence_hpa },
		{ 'H', input_handle_sequence_cup },
		{ 'f', input_handle_sequence_cup },
		{ 'J', input_handle_sequence_ed },
		{ 'K', input_handle_sequence_el },
		{ 'h', input_handle_sequence_sm },
		{ 'l', input_handle_sequence_rm },
		{ 'r', input_handle_sequence_decstbm },
		{ 'm', input_handle_sequence_sgr },
	};
	u_int	i;
	struct input_arg *iarg;
	
	log_debug2("-- sq %zu: %hhu (%c) (%zu,%zu): %u",
	    ictx->off, ch, ch, ictx->intoff, ictx->intlen, 
	    ARRAY_LENGTH(&ictx->args));
	for (i = 0; i < ARRAY_LENGTH(&ictx->args); i++) {
		iarg = &ARRAY_ITEM(&ictx->args, i);
		if (iarg->len > 0) {
			log_debug("      ++ %u: (%zu) %.*s", i,
			    iarg->len, (int) iarg->len, ictx->buf + iarg->off);
		}
	}

	/* XXX bsearch? */
	for (i = 0; i < (sizeof table / sizeof table[0]); i++) {
		if (table[i].ch == ch) {
			table[i].fn(ictx);
			ARRAY_CLEAR(&ictx->args);
			return;
		}
	}
	ARRAY_CLEAR(&ictx->args);

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

	if (n == 0 || n > ictx->s->cy) {
		log_debug3("cuu: out of range: %hu", n);
		return;
	}

	ictx->s->cy -= n;
	input_store_one(ictx->b, CODE_CURSORUP, n);
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

	if (n == 0 || n > ictx->s->sy - ictx->s->cy - 1) {
		log_debug3("cud: out of range: %hu", n);
		return;
	}

	ictx->s->cy += n;
	input_store_one(ictx->b, CODE_CURSORDOWN, n);
}

void
input_handle_sequence_cuf(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0 || n > ictx->s->sx - ictx->s->cx - 1) {
		log_debug3("cuf: out of range: %hu", n);
		return;
	}

	ictx->s->cx += n;
	input_store_one(ictx->b, CODE_CURSORRIGHT, n);
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

	if (n == 0 || n > ictx->s->cx) {
		log_debug3("cub: out of range: %hu", n);
		return;
	}

	ictx->s->cx -= n;
	input_store_one(ictx->b, CODE_CURSORLEFT, n);
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

	if (n == 0 || n > ictx->s->sx - ictx->s->cx - 1) {
		log_debug3("dch: out of range: %hu", n);
		return;
	}

	screen_delete_characters(ictx->s, ictx->s->cx, ictx->s->cy, n);
	input_store_one(ictx->b, CODE_DELETECHARACTER, n);
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

	if (n == 0 || n > ictx->s->sy - ictx->s->cy - 1) {
		log_debug3("dl: out of range: %hu", n);
		return;
	}

	screen_delete_lines(ictx->s, ictx->s->cy, n);
	input_store_one(ictx->b, CODE_DELETELINE, n);
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

	if (n == 0 || n > ictx->s->sx - ictx->s->cx - 1) {
		log_debug3("ich: out of range: %hu", n);
		return;
	}

	screen_insert_characters(ictx->s, ictx->s->cx, ictx->s->cy, n);
	input_store_one(ictx->b, CODE_INSERTCHARACTER, n);
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

	if (n == 0 || n > ictx->s->sy - ictx->s->cy - 1) {
		log_debug3("il: out of range: %hu", n);
		return;
	}

	screen_insert_lines(ictx->s, ictx->s->cy, n);
	input_store_one(ictx->b, CODE_INSERTLINE, n);
}

void
input_handle_sequence_vpa(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0 || n > ictx->s->sy) {
		log_debug3("vpa: out of range: %hu", n);
		return;
	}

	ictx->s->cy = n - 1;
	input_store_two(ictx->b, CODE_CURSORMOVE, n, ictx->s->cx + 1);
}

void
input_handle_sequence_hpa(struct input_ctx *ictx)
{
	uint16_t	n;

	if (ictx->private != '\0')
		return;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 1) != 0)
		return;

	if (n == 0 || n > ictx->s->sx) {
		log_debug3("hpa: out of range: %hu", n);
		return;
	}

	ictx->s->cx = n - 1;
	input_store_two(ictx->b, CODE_CURSORMOVE, ictx->s->cy + 1, n);
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

	if (n == 0 || n > ictx->s->sy || m == 0 || m > ictx->s->sx) {
		log_debug3("cup: out of range: %hu", n);
		return;
	}

	ictx->s->cx = m - 1;
	ictx->s->cy = n - 1;
	input_store_two(ictx->b, CODE_CURSORMOVE, n, m);
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
		screen_fill_end_of_screen(ictx->s, ictx->s->cx, ictx->s->cy,
		    SCREEN_DEFDATA, ictx->s->attr, ictx->s->colr);
		input_store_zero(ictx->b, CODE_CLEARENDOFSCREEN);
		break;
	case 2:
		screen_fill_screen(
		    ictx->s, SCREEN_DEFDATA, ictx->s->attr, ictx->s->colr);
		input_store_zero(ictx->b, CODE_CLEARSCREEN);
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
		screen_fill_end_of_line(ictx->s, ictx->s->cx, ictx->s->cy,
		    SCREEN_DEFDATA, ictx->s->attr, ictx->s->colr);
		input_store_zero(ictx->b, CODE_CLEARENDOFLINE);
		break;
	case 1:
		screen_fill_start_of_line(ictx->s, ictx->s->cx, ictx->s->cy,
		    SCREEN_DEFDATA, ictx->s->attr, ictx->s->colr);
		input_store_zero(ictx->b, CODE_CLEARSTARTOFLINE);
		break;
	case 2:
		screen_fill_line(ictx->s, ictx->s->cy,
		    SCREEN_DEFDATA, ictx->s->attr, ictx->s->colr);
		input_store_zero(ictx->b, CODE_CLEARLINE);
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
			ictx->s->mode |= MODE_KCURSOR;
			input_store_zero(ictx->b, CODE_KCURSORON);
			break;
		case 25:	/* TCEM */
			ictx->s->mode |= MODE_CURSOR;
			input_store_zero(ictx->b, CODE_CURSORON);
			break;
		default:
			log_debug("unknown SM [%hhu]: %u", ictx->private, n);
			break;
		}
	} else {
		switch (n) {
		case 4:		/* IRM */
			ictx->s->mode |= MODE_INSERT;
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
	uint16_t	n;

	if (ARRAY_LENGTH(&ictx->args) > 1)
		return;
	if (input_get_argument(ictx, 0, &n, 0) != 0)
		return;

	if (ictx->private == '?') {
		switch (n) {
		case 1:		/* GATM */
			ictx->s->mode &= ~MODE_KCURSOR;
			input_store_zero(ictx->b, CODE_KCURSOROFF);
			break;
		case 25:	/* TCEM */
			ictx->s->mode &= ~MODE_CURSOR;
			input_store_zero(ictx->b, CODE_CURSOROFF);
			break;
		default:
			log_debug("unknown RM [%hhu]: %u", ictx->private, n);
			break;
		}
	} else if (ictx->private == '\0') {
		switch (n) {
		case 4:		/* IRM */
			ictx->s->mode &= ~MODE_INSERT;
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
input_handle_sequence_decstbm(struct input_ctx *ictx)
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

	if (n == 0 || n > ictx->s->sy - 1 || m == 0 || m > ictx->s->sx - 1) {
		log_debug3("decstbm: out of range: %hu,%hu", m, n);
		return;
	}
	if (m > n) {
		log_debug3("decstbm: out of range: %hu,%hu", m, n);
		return;
	}

	ictx->s->ry_upper = n - 1;
	ictx->s->ry_lower = m - 1;
	input_store_two(ictx->b, CODE_SCROLLREGION, n, m);
}

void
input_handle_sequence_sgr(struct input_ctx *ictx)
{
	u_int		i, n;
	uint16_t	m;

	n = ARRAY_LENGTH(&ictx->args);

	input_store_zero(ictx->b, CODE_ATTRIBUTES);
	if (n == 0) {
		ictx->s->attr = 0;
		ictx->s->colr = SCREEN_DEFCOLR;
		input_store16(ictx->b, 1);
		input_store16(ictx->b, 0);
	} else {
		input_store16(ictx->b, n);
		for (i = 0; i < n; i++) {
			if (input_get_argument(ictx, i, &m, 0) != 0) {
				/* XXX XXX partial write to client */
				return;
			}
			switch (m) {
			case 0:
			case 10:
				ictx->s->attr = 0;
				ictx->s->colr = SCREEN_DEFCOLR;
				break;
			case 1:
				ictx->s->attr |= ATTR_BRIGHT;
				break;
			case 2:
				ictx->s->attr |= ATTR_DIM;
				break;
			case 3:
				ictx->s->attr |= ATTR_ITALICS;
				break;
			case 4:
				ictx->s->attr |= ATTR_UNDERSCORE;
				break;
			case 5:
				ictx->s->attr |= ATTR_BLINK;
				break;
			case 7:
				ictx->s->attr |= ATTR_REVERSE;
				break;
			case 8:
				ictx->s->attr |= ATTR_HIDDEN;
				break;
			case 23:
				ictx->s->attr &= ~ATTR_ITALICS;
				break;
			case 24:
				ictx->s->attr &= ~ATTR_UNDERSCORE;
				break;
			case 30:
			case 31:
			case 32:
			case 33:
			case 34:
			case 35:
			case 36:
			case 37:
				ictx->s->colr &= 0x0f;
				ictx->s->colr |= (m - 30) << 4;
				break;
			case 39:
				ictx->s->colr &= 0x0f;
				ictx->s->colr |= 0x80;
				break;
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
				ictx->s->colr &= 0xf0;
				ictx->s->colr |= m - 40;
				break;
			case 49:
				ictx->s->colr &= 0xf0;
				ictx->s->colr |= 0x08;
				break;
			}
			input_store16(ictx->b, m);
		}
	}
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
	buffer_write(b, &n, sizeof n);
}

void
input_store16(struct buffer *b, uint16_t n)
{
	buffer_write(b, &n, sizeof n);
}

uint8_t
input_extract8(struct buffer *b)
{
	uint8_t	n;

	buffer_read(b, &n, sizeof n);
	return (n);
}

uint16_t
input_extract16(struct buffer *b)
{
	uint16_t	n;

	buffer_read(b, &n, sizeof n);
	return (n);
}
