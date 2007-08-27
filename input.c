/* $Id: input.c,v 1.3 2007-08-27 11:05:21 nicm Exp $ */

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

size_t	input_sequence(
	    u_char *, size_t, u_char *, u_char *, uint16_t **, u_int *);
int	input_control(
	    u_char **, size_t *, struct buffer *, struct screen *, u_char);
int	input_pair_private(
	    u_char **, size_t *, struct buffer *, struct screen *, u_char);
int	input_pair_standard(
	    u_char **, size_t *, struct buffer *, struct screen *, u_char);
int	input_pair_control(
	    u_char **, size_t *, struct buffer *, struct screen *, u_char);
int	input_control_sequence(
	    u_char **, size_t *, struct buffer *, struct screen *);
int	input_check_one(uint16_t *, u_int, uint16_t *, uint16_t);
int	input_check_one2(
	    uint16_t *, u_int, uint16_t *, uint16_t, uint16_t, uint16_t);
int	input_check_two(
	    uint16_t *, u_int, uint16_t *, uint16_t, uint16_t *, uint16_t);

struct input_key {
	int		 key;
	const char	*string;
};

struct input_key input_keys[] = {
	{ KEYC_BACKSPACE, "" },
	{ KEYC_DC,     "[3~" },
	{ KEYC_DOWN,   "OB" },
	{ KEYC_F1,     "OP" },
	{ KEYC_F10,    "[21~" },
	{ KEYC_F11,    "[23~" },
	{ KEYC_F12,    "[24~" },
	{ KEYC_F2,     "OQ" },
	{ KEYC_F3,     "OR" },
	{ KEYC_F4,     "OS" },
	{ KEYC_F5,     "[15~" },
	{ KEYC_F6,     "[17~" },
	{ KEYC_F7,     "[18~" },
	{ KEYC_F8,     "[19~" },
	{ KEYC_F9,     "[20~" },
	{ KEYC_HOME,   "[1~" },
	{ KEYC_IC,     "[2~" },
	{ KEYC_LEFT,   "OD" },
	{ KEYC_LL,     "[4~" },
	{ KEYC_NPAGE,  "[6~" },
	{ KEYC_PPAGE,  "[5~" },
	{ KEYC_RIGHT,  "OC" },
	{ KEYC_UP,     "OA" }
};

/*
 * This parses CSI escape sequences into a code and a block of uint16_t
 * arguments. buf must be the next byte after the \e[ and len the remaining
 * data.
 */
size_t
input_sequence(u_char *buf, size_t len,
    u_char *code, u_char *private, uint16_t **argv, u_int *argc)
{
	char		 ch;
	u_char		*ptr, *saved;
	const char	*errstr;

	*code = 0;

	*argc = 0;
	*argv = NULL;

	if (len == 0)
		return (0);
	saved = buf;

	/*
	 * 0x3c (<) to 0x3f (?) mark private sequences when appear as the first
	 * character.
	 */
	*private = '\0';
	if (*buf >= '<' && *buf <= '?') {
		*private = *buf;
		buf++; len--;
	} else if (*buf < '0' || *buf > ';')
		goto complete;

	while (len > 0) {
		/*
		 * Every parameter substring is bytes from 0x30 (0) to 0x3a (:),
		 * terminated by 0x3b (;). 0x3a is an internal seperator.
		 */

		/* Find the end of the substring. */
		ptr = buf;
		while (len != 0 && *ptr >= '0' && *ptr <= '9') {
			ptr++;
			len--;
		}
		if (len == 0)
			break;

		/* An 0x3a is unsupported. */
		if (*ptr == ':')
			goto invalid;

		/* Create a new argument. */
		(*argc)++;
		*argv = xrealloc(*argv, *argc, sizeof **argv);

		/* Fill in argument value. */
		errstr = NULL;
		if (ptr == buf)
			(*argv)[*argc - 1] = UINT16_MAX;
		else {
			ch = *ptr; *ptr = '\0';
			(*argv)[*argc - 1] =
			    strtonum(buf, 0, UINT16_MAX - 1, &errstr);
			*ptr = ch;
		}
		buf = ptr;

		/* If the conversion had errors, abort now. */
		if (errstr != NULL)
			goto invalid;

		/* Break for any non-terminator. */
		if (*buf != ';')
			goto complete;
		buf++; len--;
	}
	if (len == 0)
		goto incomplete;

complete:
	/* Valid final characters are 0x40 (@) to 0x7e (~). */
	if (*buf < '@' || *buf > '~')
		goto invalid;

	*code = *buf;
	return (buf - saved + 1);

invalid:
	if (*argv != NULL) {
		xfree(*argv);
		*argv = NULL;
	}
	*argc = 0;

	/* Invalid. Consume until a valid terminator. */
	while (len > 0) {
		if (*buf >= '@' && *buf <= '~')
			break;
		buf++; len--;
	}
	if (len == 0)
		goto incomplete;

	*code = '\0';
	return (buf - saved + 1);

incomplete:
	if (*argv != NULL) {
		xfree(*argv);
		*argv = NULL;
	}
	*argc = 0;

	*code = '\0';
	return (0);
}

/* Translate a key code into an output key sequence. */
void
input_key(struct buffer *b, int key)
{
	struct input_key	*ak;
	u_int		 i;

	log_debug("writing key %d", key);
	if (key != KEYC_NONE && key >= 0) {
		input_store8(b, key);
		return;
	}

	for (i = 0; i < (sizeof input_keys / sizeof input_keys[0]); i++) {
		ak = input_keys + i;
		if (ak->key == key) {
			log_debug("found key %d: \"%s\"", key, ak->string);
			buffer_write(b, ak->string, strlen(ak->string));
			return;
		}
	}
}

/*
 * Parse a block of data and normalise escape sequences into a \e, a single
 * character code and the correct number of arguments. This includes adding
 * missing arguments and default values, and enforcing limits. Returns the
 * number of bytes consumed. The screen is updated with the data and used
 * to fill in current cursor positions and sizes.
 */
size_t
input_parse(u_char *buf, size_t len, struct buffer *b, struct screen *s)
{
	u_char	*saved, ch;
	size_t	 size;
	FILE	*f;

	saved = buf;

	if (debug_level > 1) {
		f = fopen("tmux-in.log", "a+");
		fwrite(buf, len, 1, f);
		fclose(f);
	}

	while (len > 0) {
		ch = *buf++; len--;

		/* Handle control characters. */
		if (ch != '\e') {
			if (ch < ' ') {
				if (input_control(&buf, &len, b, s, ch) == 1) {
					*--buf = ch;
					break;
				}
			} else {
				log_debug("character: %c (%hhu)", ch, ch);
				screen_character(s, ch);
				input_store8(b, ch);
			}
			continue;
		}
		if (len == 0) {
			*--buf = '\e';
			break;
		}

		/* Read the first character. */
		ch = *buf++; len--;

		/* Ignore delete. */
		if (ch == '\177') {
			if (len == 0) {
				*--buf = '\e';
				break;
			}
			ch = *buf++; len--;
		}

		/* Interpret C0 immediately. */
		if (ch < ' ') {
			if (input_control(&buf, &len, b, s, ch) == 1) {
				*--buf = ch;
				break;
			}

			if (len == 0) {
				*--buf = '\e';
				break;
			}
			ch = *buf++; len--;
		}

		/*
		 * Save used size to work out how much to pass to
		 * screen_sequence later.
		 */
		size = BUFFER_USED(b);

		/* Skip until the end of intermediate strings. */
		if (ch >= ' ' && ch <= '/') {
			while (len != 0) {
				if (ch >= 0x30 && ch <= 0x3f)
					break;
				if (ch >= 0x40 && ch <= 0x5f)
					break;
				ch = *buf++; len--;
			}
			continue;
		}

		/* Handle two-character sequences. */
		if (ch >= '0' && ch <= '?') {
			if (input_pair_private(&buf, &len, b, s, ch) == 1)
				goto incomplete;
			goto next;
		}
		if (ch >= '`' && ch <= '~') {
			if (input_pair_standard(&buf, &len, b, s, ch) == 1)
				goto incomplete;
			goto next;
		}
		if (ch >= '@' && ch <= '_' && ch != '[') {
			if (input_pair_control(&buf, &len, b, s, ch) == 1)
				goto incomplete;
			goto next;
		}

		/* If not CSI at this point, invalid. */
		if (ch != '[')
			continue;

		if (input_control_sequence(&buf, &len, b, s) == 1)
			goto incomplete;

	next:
		size = BUFFER_USED(b) - size;
		log_debug("output is %zu bytes", size);
		if (size > 0) /* XXX only one command? */
			screen_sequence(s, BUFFER_IN(b) - size);
		log_debug("remaining data %zu bytes", len);
	}

	return (buf - saved);

incomplete:
	*--buf = ch;
	*--buf = '\e';
	return (buf - saved);
}

/* Handle single control characters. */
int
input_control(unused u_char **buf, unused size_t *len,
    struct buffer *b, struct screen *s, u_char ch)
{
	switch (ch) {
	case '\0':	/* NUL */
		break;
	case '\n':	/* LF */
	case '\r':	/* CR */
	case '\010': 	/* BS */
		log_debug("control:   %hhu", ch);
		screen_character(s, ch);
		input_store8(b, ch);
		break;
	default:
		log_debug("unknown control: %c (%hhu)", ch, ch);
		break;
	}

	return (0);
}

/* Translate a private two-character sequence. */
int
input_pair_private(unused u_char **buf, unused size_t *len,
    unused struct buffer *b, unused struct screen *s, unused u_char ch)
{
	log_debug("private2:  %c (%hhu)", ch, ch);

	switch (ch) {
	case '=':	/* DECKPAM */
		input_store_zero(b, CODE_KKEYPADON);
		break;
	case '>':	/* DECKPNM*/
		input_store_zero(b, CODE_KKEYPADOFF);
		break;
	default:
		log_debug("unknown private2: %c (%hhu)", ch, ch);
		break;
	}

	return (0);
}

/* Translate a standard two-character sequence. */
int
input_pair_standard(unused u_char **buf, unused size_t *len,
    unused struct buffer *b, unused struct screen *s, u_char ch)
{
	log_debug("unknown standard2: %c (%hhu)", ch, ch);

	return (0);
}

/* Translate a control two-character sequence. */
int
input_pair_control(u_char **buf, size_t *len,
    struct buffer *b, unused struct screen *s, u_char ch)
{
	u_char 	*ptr;
	size_t	 size;

	log_debug("control2:  %c (%hhu)", ch, ch);

	switch (ch) {
	case ']':	/* window title */
		if (*len < 3)
			return (1);
		ch = *(*buf)++; (*len)--;

		/*
		 * Search MAXTITLELEN + 1 to allow space for the ;. The
		 * \007 is also included, but space is needed for a \0 so
		 * it doesn't need to be compensated for.
		 */
		size = *len > MAXTITLELEN + 1 ? MAXTITLELEN + 1 : *len;
		if ((ptr = memchr(*buf, '\007', size)) == NULL) {
			log_debug("title not found in %zu bytes", size);
			if (*len >= MAXTITLELEN + 1)
				break;
			(*buf)--; (*len)++;
			return (1);
		}
		size = ptr - *buf;

		/* A zero size means no ;, just skip the \007 and return. */
		if (size == 0) {
			(*buf)++; (*len)--;
			break;
		}

		/* Set the title if appropriate. */
		if (**buf == ';' && (ch == '0' || ch == '1')) {
			log_debug("title found, length %zu bytes: %.*s",
			    size - 1, (int) size - 1, *buf + 1);
			if (size > 1) {
				input_store_one(b, CODE_TITLE, size - 1);
				buffer_write(b, *buf + 1, size - 1);
			}
		}

		/* Skip the title; add one for the \007. */
		(*buf) += size + 1;
		(*len) -= size + 1;
		break;
	case 'M':	/* RI */
		input_store_one(b, CODE_CURSORUPSCROLL, 1);
		break;
	default:
		log_debug("unknown control2: %c (%hhu)", ch, ch);
		break;
	}

	return (0);
}

/* Translate a control sequence. */
int
input_control_sequence(
    u_char **buf, size_t *len, struct buffer *b, struct screen *s)
{
	u_char		 code, private;
	size_t		 used;
	uint16_t	*argv, ua, ub;
	u_int		 argc, i;

	used = input_sequence(*buf, *len, &code, &private, &argv, &argc);
	if (used == 0)		/* incomplete */
		return (1);

	(*buf) += used;
	(*len) -= used;

	if (code == '\0')	/* invalid */
		return (-1);

	log_debug(
	    "sequence:  %c (%hhu) [%c] [cx %u, cy %u, sx %u, sy %u]: %u", code,
	    code, private, s->cx, s->cy, s->sx, s->sy, argc);
	for (i = 0; i < argc; i++)
		log_debug("\targument %u: %u", i, argv[i]);

	switch (code) {
	case 'A':	/* CUU */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_CURSORUP, ua);
		break;
	case 'B':	/* CUD */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_CURSORDOWN, ua);
		break;
	case 'C':	/* CUF */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_CURSORRIGHT, ua);
		break;
	case 'D':	/* CUB */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_CURSORLEFT, ua);
		break;
	case 'P':	/* DCH */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_DELETECHARACTER, ua);
		break;
	case 'M':	/* DL */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_DELETELINE, ua);
		break;
	case '@':	/* ICH */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_INSERTCHARACTER, ua);
		break;
	case 'L':	/* IL */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_one(b, CODE_INSERTLINE, ua);
		break;
	case 'd':	/* VPA */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
				break;
		if (ua == 0)
			break;
		input_store_two(b, CODE_CURSORMOVE, ua, s->cx + 1);
		break;
	case 'G':	/* HPA */
		if (private != '\0')
			break;
		if (input_check_one(argv, argc, &ua, 1) != 0)
			break;
		if (ua == 0)
			break;
		input_store_two(b, CODE_CURSORMOVE, s->cy + 1, ua);
		break;
	case 'H':	/* CUP */
	case 'f':	/* HVP */
		if (private != '\0')
			break;
		if (input_check_two(argv, argc, &ua, 1, &ub, 1) != 0)
			break;
		if (ua == 0 || ub == 0)
			break;
		input_store_two(b, CODE_CURSORMOVE, ua, ub);
		break;
	case 'J':	/* ED */
		if (private != '\0')
			break;
		if (input_check_one2(argv, argc, &ua, 0, 0, 2) != 0)
			break;
		switch (ua) {
		case 0:
			input_store_zero(b, CODE_CLEARENDOFSCREEN);
			break;
		case 2:
			input_store_zero(b, CODE_CLEARSCREEN);
			break;
		}
		break;
	case 'K':	/* EL */
		if (private != '\0')
			break;
		if (input_check_one2(argv, argc, &ua, 0, 0, 2) != 0)
			break;
		switch (ua) {
		case 0:
			input_store_zero(b, CODE_CLEARENDOFLINE);
			break;
		case 1:
			input_store_zero(b, CODE_CLEARSTARTOFLINE);
			break;
		case 2:
			input_store_zero(b, CODE_CLEARLINE);
			break;
		}
		break;
	case 'h':	/* SM */
		if (input_check_one(argv, argc, &ua, 0) != 0)
			break;
		switch (private) {
		case '?':
			switch (ua) {
			case 1:		/* GATM */
				input_store_zero(b, CODE_KCURSORON);
				break;
			case 25:	/* TCEM */
				input_store_zero(b, CODE_CURSORON);
				break;
			default:
				log_debug("unknown SM [%d]: %u", private, ua);
			}
			break;
		case '\0':
			switch (ua) {
			case 4:		/* IRM */
				input_store_zero(b, CODE_INSERTON);
				break;
			case 34:
				/* Cursor high visibility not supported. */
				break;
			default:
				log_debug("unknown SM [%d]: %u", private, ua);
				break;
			}
			break;
		}
		break;
	case 'l':	/* RM */
		if (input_check_one(argv, argc, &ua, 0) != 0)
			break;
		switch (private) {
		case '?':
			switch (ua) {
			case 1:		/* GATM */
				input_store_zero(b, CODE_KCURSOROFF);
				break;
			case 25:	/* TCEM */
				input_store_zero(b, CODE_CURSOROFF);
				break;
			default:
				log_debug("unknown RM [%d]: %u", private, ua);
			}
			break;
		case '\0':
			switch (ua) {
			case 4:		/* IRM */
				input_store_zero(b, CODE_INSERTOFF);
				break;
			case 34:
				/* Cursor high visibility not supported. */
				break;
			default:
				log_debug("unknown RM [%d]: %u", private, ua);
				break;
			}
			break;
		}
		break;
	case 'r':	/* DECSTBM */
		if (private != '\0')
			break;
		if (input_check_two(argv, argc,
		    &ua, s->ry_upper + 1, &ub, s->ry_lower + 1) != 0)
			break;
		if (ua == 0 || ub == 0 || ub < ua)
			break;
		input_store_two(b, CODE_SCROLLREGION, ua, ub);
		break;
	case 'm':	/* SGR */
		input_store_zero(b, CODE_ATTRIBUTES);
		if (argc == 0) {
			input_store16(b, 1);
			input_store16(b, 0);
		} else {
			input_store16(b, argc);
			for (i = 0; i < argc; i++) {
				if (argv[i] == UINT16_MAX)
					argv[i] = 0;
				input_store16(b, argv[i]);
			}
		}
		break;
	default:
		log_debug("unknown sequence: %c (%hhu)", code, code);
		break;
	}

	if (argv != NULL) {
		xfree(argv);
		argv = NULL;
	}

	return (0);
}

/* Check for one argument. */
int
input_check_one(uint16_t *argv, u_int argc, uint16_t *a, uint16_t ad)
{
	*a = ad;
	if (argc == 1) {
		if (argv[0] != UINT16_MAX)
			*a = argv[0];
	} else if (argc != 0)
		return (-1);
	return (0);
}

/* Check for one argument with limits. */
int
input_check_one2(uint16_t *argv, u_int argc,
    uint16_t *a, uint16_t ad, uint16_t al, uint16_t au)
{
	*a = ad;
	if (argc == 1) {
		if (argv[0] != UINT16_MAX)
			*a = argv[0];
	} else if (argc != 0)
		return (-1);
	if (*a < al || *a > au)
		return (-1);
	return (0);
}

/* Check for two arguments. */
int
input_check_two(uint16_t *argv, u_int argc,
    uint16_t *a, uint16_t ad, uint16_t *b, uint16_t bd)
{
	*a = ad;
	*b = bd;
	if (argc == 1) {
		if (argv[0] != UINT16_MAX)
			*a = argv[0];
	} else if (argc == 2) {
		if (argv[0] != UINT16_MAX)
			*a = argv[0];
		if (argv[1] != UINT16_MAX)
			*b = argv[1];
	} else if (argc != 0)
		return (-1);
	return (0);
}

/* Store a code without arguments. */
void
input_store_zero(struct buffer *b, u_char code)
{
	input_store8(b, '\e');
	input_store8(b, code);
}

/* Store a code with a single argument. */
void
input_store_one(struct buffer *b, u_char code, uint16_t ua)
{
	input_store8(b, '\e');
	input_store8(b, code);
	input_store16(b, ua);
}

/* Store a code with two arguments. */
void
input_store_two(struct buffer *b, u_char code, uint16_t ua, uint16_t ub)
{
	input_store8(b, '\e');
	input_store8(b, code);
	input_store16(b, ua);
	input_store16(b, ub);
}

/* Write an 8-bit quantity to a buffer. */
void
input_store8(struct buffer *b, uint8_t n)
{
	buffer_write(b, &n, sizeof n);
}

/* Write a 16-bit argument to a buffer. */
void
input_store16(struct buffer *b, uint16_t n)
{
	buffer_write(b, &n, sizeof n);
}

/* Extract an 8-bit quantity from a buffer. */
uint8_t
input_extract8(struct buffer *b)
{
	uint8_t	n;

	buffer_read(b, &n, sizeof n);
	return (n);
}

/* Extract a 16-bit argument from a pointer. */
uint16_t
input_extract16(struct buffer *b)
{
	uint16_t	n;

	buffer_read(b, &n, sizeof n);
	return (n);
}
