/* $OpenBSD: tty-term.c,v 1.3 2009/07/14 06:30:45 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <curses.h>
#include <string.h>
#include <term.h>

#include "tmux.h"

void	 tty_term_quirks(struct tty_term *);
char	*tty_term_strip(const char *);

struct tty_terms tty_terms = SLIST_HEAD_INITIALIZER(tty_terms);

struct tty_term_code_entry tty_term_codes[NTTYCODE] = {
	{ TTYC_AX, TTYCODE_FLAG, "AX" },
	{ TTYC_ACSC, TTYCODE_STRING, "acsc" },
	{ TTYC_BEL, TTYCODE_STRING, "bel" },
	{ TTYC_BLINK, TTYCODE_STRING, "blink" },
	{ TTYC_BOLD, TTYCODE_STRING, "bold" },
	{ TTYC_CIVIS, TTYCODE_STRING, "civis" },
	{ TTYC_CLEAR, TTYCODE_STRING, "clear" },
	{ TTYC_CNORM, TTYCODE_STRING, "cnorm" },
	{ TTYC_COLORS, TTYCODE_NUMBER, "colors" },
	{ TTYC_CSR, TTYCODE_STRING, "csr" },
	{ TTYC_CUD, TTYCODE_STRING, "cud" },
	{ TTYC_CUD1, TTYCODE_STRING, "cud1" },
	{ TTYC_CUP, TTYCODE_STRING, "cup" },
	{ TTYC_DCH, TTYCODE_STRING, "dch" },
	{ TTYC_DCH1, TTYCODE_STRING, "dch1" },
	{ TTYC_DIM, TTYCODE_STRING, "dim" },
	{ TTYC_DL, TTYCODE_STRING, "dl" },
	{ TTYC_DL1, TTYCODE_STRING, "dl1" },
	{ TTYC_EL, TTYCODE_STRING, "el" },
	{ TTYC_EL1, TTYCODE_STRING, "el1" },
	{ TTYC_ENACS, TTYCODE_STRING, "enacs" },
	{ TTYC_ICH, TTYCODE_STRING, "ich" },
	{ TTYC_ICH1, TTYCODE_STRING, "ich1" },
	{ TTYC_IL, TTYCODE_STRING, "il" },
	{ TTYC_IL1, TTYCODE_STRING, "il1" },
	{ TTYC_INVIS, TTYCODE_STRING, "invis" },
	{ TTYC_IS1, TTYCODE_STRING, "is1" },
	{ TTYC_IS2, TTYCODE_STRING, "is2" },
	{ TTYC_IS3, TTYCODE_STRING, "is3" },
	{ TTYC_KCBT, TTYCODE_STRING, "kcbt" },
	{ TTYC_KCUB1, TTYCODE_STRING, "kcub1" },
	{ TTYC_KCUD1, TTYCODE_STRING, "kcud1" },
	{ TTYC_KCUF1, TTYCODE_STRING, "kcuf1" },
	{ TTYC_KCUU1, TTYCODE_STRING, "kcuu1" },
	{ TTYC_KDCH1, TTYCODE_STRING, "kdch1" },
	{ TTYC_KEND, TTYCODE_STRING, "kend" },
	{ TTYC_KF1, TTYCODE_STRING, "kf1" },
	{ TTYC_KF10, TTYCODE_STRING, "kf10" },
	{ TTYC_KF11, TTYCODE_STRING, "kf11" },
	{ TTYC_KF12, TTYCODE_STRING, "kf12" },
	{ TTYC_KF13, TTYCODE_STRING, "kf13" },
	{ TTYC_KF14, TTYCODE_STRING, "kf14" },
	{ TTYC_KF15, TTYCODE_STRING, "kf15" },
	{ TTYC_KF16, TTYCODE_STRING, "kf16" },
	{ TTYC_KF17, TTYCODE_STRING, "kf17" },
	{ TTYC_KF18, TTYCODE_STRING, "kf18" },
	{ TTYC_KF19, TTYCODE_STRING, "kf19" },
	{ TTYC_KF20, TTYCODE_STRING, "kf20" },
	{ TTYC_KF2, TTYCODE_STRING, "kf2" },
	{ TTYC_KF3, TTYCODE_STRING, "kf3" },
	{ TTYC_KF4, TTYCODE_STRING, "kf4" },
	{ TTYC_KF5, TTYCODE_STRING, "kf5" },
	{ TTYC_KF6, TTYCODE_STRING, "kf6" },
	{ TTYC_KF7, TTYCODE_STRING, "kf7" },
	{ TTYC_KF8, TTYCODE_STRING, "kf8" },
	{ TTYC_KF9, TTYCODE_STRING, "kf9" },
	{ TTYC_KHOME, TTYCODE_STRING, "khome" },
	{ TTYC_KICH1, TTYCODE_STRING, "kich1" },
	{ TTYC_KMOUS, TTYCODE_STRING, "kmous" },
	{ TTYC_KNP, TTYCODE_STRING, "knp" },
	{ TTYC_KPP, TTYCODE_STRING, "kpp" },
	{ TTYC_OP, TTYCODE_STRING, "op" },
	{ TTYC_REV, TTYCODE_STRING, "rev" },
	{ TTYC_RI, TTYCODE_STRING, "ri" },
	{ TTYC_RMACS, TTYCODE_STRING, "rmacs" },
	{ TTYC_RMCUP, TTYCODE_STRING, "rmcup" },
	{ TTYC_RMIR, TTYCODE_STRING, "rmir" },
	{ TTYC_RMKX, TTYCODE_STRING, "rmkx" },
	{ TTYC_SETAB, TTYCODE_STRING, "setab" },
	{ TTYC_SETAF, TTYCODE_STRING, "setaf" },
	{ TTYC_SGR0, TTYCODE_STRING, "sgr0" },
	{ TTYC_SMACS, TTYCODE_STRING, "smacs" },
	{ TTYC_SMCUP, TTYCODE_STRING, "smcup" },
	{ TTYC_SMIR, TTYCODE_STRING, "smir" },
	{ TTYC_SMKX, TTYCODE_STRING, "smkx" },
	{ TTYC_SMSO, TTYCODE_STRING, "smso" },
	{ TTYC_SMUL, TTYCODE_STRING, "smul" },
	{ TTYC_XENL, TTYCODE_FLAG, "xenl" },
};

char *
tty_term_strip(const char *s)
{
	const char     *ptr;
	static char	buf[BUFSIZ];
	size_t		len;

	/* Ignore strings with no padding. */
	if (strchr(s, '$') == NULL)
		return (xstrdup(s));

	len = 0;
	for (ptr = s; *ptr != '\0'; ptr++) {
		if (*ptr == '$' && *(ptr + 1) == '<') {
			while (*ptr != '\0' && *ptr != '>')
				ptr++;
			if (*ptr == '>')
				ptr++;
		}

		buf[len++] = *ptr;
		if (len == (sizeof buf) - 1)
			break;
	}
	buf[len] = '\0';

	return (xstrdup(buf));
}

void
tty_term_quirks(struct tty_term *term)
{
	if (strncmp(term->name, "rxvt", 4) == 0) {
		/* rxvt supports dch1 but some termcap files do not have it. */
		if (!tty_term_has(term, TTYC_DCH1)) {
			term->codes[TTYC_DCH1].type = TTYCODE_STRING;
			term->codes[TTYC_DCH1].value.string = xstrdup("\033[P");
		}
	}

	if (strncmp(term->name, "xterm", 5) == 0) {
		/* xterm supports ich1 but some termcaps omit it. */
		if (!tty_term_has(term, TTYC_ICH1)) {
			term->codes[TTYC_ICH1].type = TTYCODE_STRING;
			term->codes[TTYC_ICH1].value.string = xstrdup("\033[@");
		}
	}
}

struct tty_term *
tty_term_find(char *name, int fd, char **cause)
{
	struct tty_term			*term;
	struct tty_term_code_entry	*ent;
	struct tty_code			*code;
	u_int				 i;
	int		 		 n, error;
	char				*s;

	SLIST_FOREACH(term, &tty_terms, entry) {
		if (strcmp(term->name, name) == 0) {
			term->references++;
			return (term);
		}
	}

	log_debug("new term: %s", name);
	term = xmalloc(sizeof *term);
	term->name = xstrdup(name);
	term->references = 1;
	term->flags = 0;
	SLIST_INSERT_HEAD(&tty_terms, term, entry);

	/* Set up curses terminal. */
	if (setupterm(name, fd, &error) != OK) {
		switch (error) {
		case 0:
			xasprintf(cause, "can't use hardcopy terminal");
			break;
		case 1:
			xasprintf(cause, "missing or unsuitable terminal");
			break;
		case 2:
			xasprintf(cause, "can't find terminfo database");
			break;
		default:
			xasprintf(cause, "unknown error");
			break;
		}
		goto error;
	}

	/* Fill in codes. */
	memset(&term->codes, 0, sizeof term->codes);
	for (i = 0; i < NTTYCODE; i++) {
		ent = &tty_term_codes[i];

		code = &term->codes[ent->code];
		code->type = TTYCODE_NONE;
		switch (ent->type) {
		case TTYCODE_NONE:
			break;
		case TTYCODE_STRING:
			s = tigetstr((char *) ent->name);
			if (s == NULL || s == (char *) -1)
				break;
			code->type = TTYCODE_STRING;
			code->value.string = tty_term_strip(s);
			break;
		case TTYCODE_NUMBER:
			n = tigetnum((char *) ent->name);
			if (n == -1 || n == -2)
				break;
			code->type = TTYCODE_NUMBER;
			code->value.number = n;
			break;
		case TTYCODE_FLAG:
			n = tigetflag((char *) ent->name);
			if (n == -1)
				break;
			code->type = TTYCODE_FLAG;
			code->value.number = n;
			break;
		}
	}
	tty_term_quirks(term);

	/* Delete curses data. */
	del_curterm(cur_term);

	/* These are always required. */
	if (!tty_term_has(term, TTYC_CLEAR)) {
		xasprintf(cause, "terminal does not support clear");
		goto error;
	}
	if (!tty_term_has(term, TTYC_RI)) {
		xasprintf(cause, "terminal does not support ri");
		goto error;
	}
	if (!tty_term_has(term, TTYC_CUP)) {
		xasprintf(cause, "terminal does not support cup");
		goto error;
	}

	/* These can be emulated so one of the two is required. */
	if (!tty_term_has(term, TTYC_CUD1) && !tty_term_has(term, TTYC_CUD)) {
		xasprintf(cause, "terminal does not support cud1 or cud");
		goto error;
	}
	if (!tty_term_has(term, TTYC_IL1) && !tty_term_has(term, TTYC_IL)) {
		xasprintf(cause, "terminal does not support il1 or il");
		goto error;
	}
	if (!tty_term_has(term, TTYC_DL1) && !tty_term_has(term, TTYC_DL)) {
		xasprintf(cause, "terminal does not support dl1 or dl");
		goto error;
	}
	if (!tty_term_has(term, TTYC_ICH1) &&
	    !tty_term_has(term, TTYC_ICH) && (!tty_term_has(term, TTYC_SMIR) ||
	    !tty_term_has(term, TTYC_RMIR))) {
		xasprintf(cause,
		    "terminal does not support ich1 or ich or smir and rmir");
		goto error;
	}
	if (!tty_term_has(term, TTYC_DCH1) && !tty_term_has(term, TTYC_DCH)) {
		xasprintf(cause, "terminal does not support dch1 or dch");
		goto error;
	}

	/*
	 * Figure out if terminal support default colours. AX is a screen
	 * extension which indicates this. Also check if op (orig_pair) uses
	 * the default colours - if it does, this is a good indication the
	 * terminal supports them.
	 */
	if (tty_term_flag(term, TTYC_AX))
		term->flags |= TERM_HASDEFAULTS;
	if (strcmp(tty_term_string(term, TTYC_OP), "\033[39;49m") == 0)
		term->flags |= TERM_HASDEFAULTS;

	/*
	 * Try to figure out if we have 256 or 88 colours. The standard xterm
	 * definitions are broken (well, or the way they are parsed is: in any
	 * case they end up returning 8). So also do a hack.
	 */
	if (tty_term_number(term, TTYC_COLORS) == 256)
		term->flags |= TERM_256COLOURS;
	if (strstr(name, "256col") != NULL) /* XXX HACK */
		term->flags |= TERM_256COLOURS;
	if (tty_term_number(term, TTYC_COLORS) == 88)
		term->flags |= TERM_88COLOURS;
	if (strstr(name, "88col") != NULL) /* XXX HACK */
		term->flags |= TERM_88COLOURS;

	/*
	 * Terminals without xenl (eat newline glitch) wrap at at $COLUMNS - 1
	 * rather than $COLUMNS (the cursor can never be beyond $COLUMNS - 1).
	 *
	 * This is irritating, most notably because it is impossible to write
	 * to the very bottom-right of the screen without scrolling.
	 *
	 * Flag the terminal here and apply some workarounds in other places to
	 * do the best possible.
	 */
	if (!tty_term_flag(term, TTYC_XENL))
		term->flags |= TERM_EARLYWRAP;

	return (term);

error:
	tty_term_free(term);
	return (NULL);
}

void
tty_term_free(struct tty_term *term)
{
	u_int	i;

	if (--term->references != 0)
		return;

	SLIST_REMOVE(&tty_terms, term, tty_term, entry);

	for (i = 0; i < NTTYCODE; i++) {
		if (term->codes[i].type == TTYCODE_STRING)
			xfree(term->codes[i].value.string);
	}
	xfree(term->name);
	xfree(term);
}

int
tty_term_has(struct tty_term *term, enum tty_code_code code)
{
	return (term->codes[code].type != TTYCODE_NONE);
}

const char *
tty_term_string(struct tty_term *term, enum tty_code_code code)
{
	if (!tty_term_has(term, code))
		return ("");
	if (term->codes[code].type != TTYCODE_STRING)
		log_fatalx("not a string: %d", code);
	return (term->codes[code].value.string);
}

/* No vtparm. Fucking curses. */
const char *
tty_term_string1(struct tty_term *term, enum tty_code_code code, int a)
{
	return (tparm((char *) tty_term_string(term, code), a));
}

const char *
tty_term_string2(struct tty_term *term, enum tty_code_code code, int a, int b)
{
	return (tparm((char *) tty_term_string(term, code), a, b));
}

int
tty_term_number(struct tty_term *term, enum tty_code_code code)
{
	if (!tty_term_has(term, code))
		return (0);
	if (term->codes[code].type != TTYCODE_NUMBER)
		log_fatalx("not a number: %d", code);
	return (term->codes[code].value.number);
}

int
tty_term_flag(struct tty_term *term, enum tty_code_code code)
{
	if (!tty_term_has(term, code))
		return (0);
	if (term->codes[code].type != TTYCODE_FLAG)
		log_fatalx("not a flag: %d", code);
	return (term->codes[code].value.flag);
}
