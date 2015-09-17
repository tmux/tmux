/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#endif
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>

#include "tmux.h"

void	 tty_term_override(struct tty_term *, const char *);
char	*tty_term_strip(const char *);

struct tty_terms tty_terms = LIST_HEAD_INITIALIZER(tty_terms);

enum tty_code_type {
	TTYCODE_NONE = 0,
	TTYCODE_STRING,
	TTYCODE_NUMBER,
	TTYCODE_FLAG,
};

struct tty_code {
	enum tty_code_type	type;
	union {
		char	       *string;
		int		number;
		int		flag;
	} value;
};

struct tty_term_code_entry {
	enum tty_code_type	type;
	const char	       *name;
};

const struct tty_term_code_entry tty_term_codes[] = {
	[TTYC_ACSC] = { TTYCODE_STRING, "acsc" },
	[TTYC_AX] = { TTYCODE_FLAG, "AX" },
	[TTYC_BCE] = { TTYCODE_FLAG, "bce" },
	[TTYC_BEL] = { TTYCODE_STRING, "bel" },
	[TTYC_BLINK] = { TTYCODE_STRING, "blink" },
	[TTYC_BOLD] = { TTYCODE_STRING, "bold" },
	[TTYC_CIVIS] = { TTYCODE_STRING, "civis" },
	[TTYC_CLEAR] = { TTYCODE_STRING, "clear" },
	[TTYC_CNORM] = { TTYCODE_STRING, "cnorm" },
	[TTYC_COLORS] = { TTYCODE_NUMBER, "colors" },
	[TTYC_CR] = { TTYCODE_STRING, "Cr" },
	[TTYC_CS] = { TTYCODE_STRING, "Cs" },
	[TTYC_CSR] = { TTYCODE_STRING, "csr" },
	[TTYC_CUB] = { TTYCODE_STRING, "cub" },
	[TTYC_CUB1] = { TTYCODE_STRING, "cub1" },
	[TTYC_CUD] = { TTYCODE_STRING, "cud" },
	[TTYC_CUD1] = { TTYCODE_STRING, "cud1" },
	[TTYC_CUF] = { TTYCODE_STRING, "cuf" },
	[TTYC_CUF1] = { TTYCODE_STRING, "cuf1" },
	[TTYC_CUP] = { TTYCODE_STRING, "cup" },
	[TTYC_CUU] = { TTYCODE_STRING, "cuu" },
	[TTYC_CUU1] = { TTYCODE_STRING, "cuu1" },
	[TTYC_CVVIS] = { TTYCODE_STRING, "cvvis" },
	[TTYC_DCH] = { TTYCODE_STRING, "dch" },
	[TTYC_DCH1] = { TTYCODE_STRING, "dch1" },
	[TTYC_DIM] = { TTYCODE_STRING, "dim" },
	[TTYC_DL] = { TTYCODE_STRING, "dl" },
	[TTYC_DL1] = { TTYCODE_STRING, "dl1" },
	[TTYC_E3] = { TTYCODE_STRING, "E3" },
	[TTYC_ECH] = { TTYCODE_STRING, "ech" },
	[TTYC_EL] = { TTYCODE_STRING, "el" },
	[TTYC_EL1] = { TTYCODE_STRING, "el1" },
	[TTYC_ENACS] = { TTYCODE_STRING, "enacs" },
	[TTYC_FSL] = { TTYCODE_STRING, "fsl" },
	[TTYC_HOME] = { TTYCODE_STRING, "home" },
	[TTYC_HPA] = { TTYCODE_STRING, "hpa" },
	[TTYC_ICH] = { TTYCODE_STRING, "ich" },
	[TTYC_ICH1] = { TTYCODE_STRING, "ich1" },
	[TTYC_IL] = { TTYCODE_STRING, "il" },
	[TTYC_IL1] = { TTYCODE_STRING, "il1" },
	[TTYC_INVIS] = { TTYCODE_STRING, "invis" },
	[TTYC_IS1] = { TTYCODE_STRING, "is1" },
	[TTYC_IS2] = { TTYCODE_STRING, "is2" },
	[TTYC_IS3] = { TTYCODE_STRING, "is3" },
	[TTYC_KCBT] = { TTYCODE_STRING, "kcbt" },
	[TTYC_KCUB1] = { TTYCODE_STRING, "kcub1" },
	[TTYC_KCUD1] = { TTYCODE_STRING, "kcud1" },
	[TTYC_KCUF1] = { TTYCODE_STRING, "kcuf1" },
	[TTYC_KCUU1] = { TTYCODE_STRING, "kcuu1" },
	[TTYC_KDC2] = { TTYCODE_STRING, "kDC" },
	[TTYC_KDC3] = { TTYCODE_STRING, "kDC3" },
	[TTYC_KDC4] = { TTYCODE_STRING, "kDC4" },
	[TTYC_KDC5] = { TTYCODE_STRING, "kDC5" },
	[TTYC_KDC6] = { TTYCODE_STRING, "kDC6" },
	[TTYC_KDC7] = { TTYCODE_STRING, "kDC7" },
	[TTYC_KDCH1] = { TTYCODE_STRING, "kdch1" },
	[TTYC_KDN2] = { TTYCODE_STRING, "kDN" },
	[TTYC_KDN3] = { TTYCODE_STRING, "kDN3" },
	[TTYC_KDN4] = { TTYCODE_STRING, "kDN4" },
	[TTYC_KDN5] = { TTYCODE_STRING, "kDN5" },
	[TTYC_KDN6] = { TTYCODE_STRING, "kDN6" },
	[TTYC_KDN7] = { TTYCODE_STRING, "kDN7" },
	[TTYC_KEND] = { TTYCODE_STRING, "kend" },
	[TTYC_KEND2] = { TTYCODE_STRING, "kEND" },
	[TTYC_KEND3] = { TTYCODE_STRING, "kEND3" },
	[TTYC_KEND4] = { TTYCODE_STRING, "kEND4" },
	[TTYC_KEND5] = { TTYCODE_STRING, "kEND5" },
	[TTYC_KEND6] = { TTYCODE_STRING, "kEND6" },
	[TTYC_KEND7] = { TTYCODE_STRING, "kEND7" },
	[TTYC_KF1] = { TTYCODE_STRING, "kf1" },
	[TTYC_KF10] = { TTYCODE_STRING, "kf10" },
	[TTYC_KF11] = { TTYCODE_STRING, "kf11" },
	[TTYC_KF12] = { TTYCODE_STRING, "kf12" },
	[TTYC_KF13] = { TTYCODE_STRING, "kf13" },
	[TTYC_KF14] = { TTYCODE_STRING, "kf14" },
	[TTYC_KF15] = { TTYCODE_STRING, "kf15" },
	[TTYC_KF16] = { TTYCODE_STRING, "kf16" },
	[TTYC_KF17] = { TTYCODE_STRING, "kf17" },
	[TTYC_KF18] = { TTYCODE_STRING, "kf18" },
	[TTYC_KF19] = { TTYCODE_STRING, "kf19" },
	[TTYC_KF2] = { TTYCODE_STRING, "kf2" },
	[TTYC_KF20] = { TTYCODE_STRING, "kf20" },
	[TTYC_KF21] = { TTYCODE_STRING, "kf21" },
	[TTYC_KF22] = { TTYCODE_STRING, "kf22" },
	[TTYC_KF23] = { TTYCODE_STRING, "kf23" },
	[TTYC_KF24] = { TTYCODE_STRING, "kf24" },
	[TTYC_KF25] = { TTYCODE_STRING, "kf25" },
	[TTYC_KF26] = { TTYCODE_STRING, "kf26" },
	[TTYC_KF27] = { TTYCODE_STRING, "kf27" },
	[TTYC_KF28] = { TTYCODE_STRING, "kf28" },
	[TTYC_KF29] = { TTYCODE_STRING, "kf29" },
	[TTYC_KF3] = { TTYCODE_STRING, "kf3" },
	[TTYC_KF30] = { TTYCODE_STRING, "kf30" },
	[TTYC_KF31] = { TTYCODE_STRING, "kf31" },
	[TTYC_KF32] = { TTYCODE_STRING, "kf32" },
	[TTYC_KF33] = { TTYCODE_STRING, "kf33" },
	[TTYC_KF34] = { TTYCODE_STRING, "kf34" },
	[TTYC_KF35] = { TTYCODE_STRING, "kf35" },
	[TTYC_KF36] = { TTYCODE_STRING, "kf36" },
	[TTYC_KF37] = { TTYCODE_STRING, "kf37" },
	[TTYC_KF38] = { TTYCODE_STRING, "kf38" },
	[TTYC_KF39] = { TTYCODE_STRING, "kf39" },
	[TTYC_KF4] = { TTYCODE_STRING, "kf4" },
	[TTYC_KF40] = { TTYCODE_STRING, "kf40" },
	[TTYC_KF41] = { TTYCODE_STRING, "kf41" },
	[TTYC_KF42] = { TTYCODE_STRING, "kf42" },
	[TTYC_KF43] = { TTYCODE_STRING, "kf43" },
	[TTYC_KF44] = { TTYCODE_STRING, "kf44" },
	[TTYC_KF45] = { TTYCODE_STRING, "kf45" },
	[TTYC_KF46] = { TTYCODE_STRING, "kf46" },
	[TTYC_KF47] = { TTYCODE_STRING, "kf47" },
	[TTYC_KF48] = { TTYCODE_STRING, "kf48" },
	[TTYC_KF49] = { TTYCODE_STRING, "kf49" },
	[TTYC_KF5] = { TTYCODE_STRING, "kf5" },
	[TTYC_KF50] = { TTYCODE_STRING, "kf50" },
	[TTYC_KF51] = { TTYCODE_STRING, "kf51" },
	[TTYC_KF52] = { TTYCODE_STRING, "kf52" },
	[TTYC_KF53] = { TTYCODE_STRING, "kf53" },
	[TTYC_KF54] = { TTYCODE_STRING, "kf54" },
	[TTYC_KF55] = { TTYCODE_STRING, "kf55" },
	[TTYC_KF56] = { TTYCODE_STRING, "kf56" },
	[TTYC_KF57] = { TTYCODE_STRING, "kf57" },
	[TTYC_KF58] = { TTYCODE_STRING, "kf58" },
	[TTYC_KF59] = { TTYCODE_STRING, "kf59" },
	[TTYC_KF6] = { TTYCODE_STRING, "kf6" },
	[TTYC_KF60] = { TTYCODE_STRING, "kf60" },
	[TTYC_KF61] = { TTYCODE_STRING, "kf61" },
	[TTYC_KF62] = { TTYCODE_STRING, "kf62" },
	[TTYC_KF63] = { TTYCODE_STRING, "kf63" },
	[TTYC_KF7] = { TTYCODE_STRING, "kf7" },
	[TTYC_KF8] = { TTYCODE_STRING, "kf8" },
	[TTYC_KF9] = { TTYCODE_STRING, "kf9" },
	[TTYC_KHOM2] = { TTYCODE_STRING, "kHOM" },
	[TTYC_KHOM3] = { TTYCODE_STRING, "kHOM3" },
	[TTYC_KHOM4] = { TTYCODE_STRING, "kHOM4" },
	[TTYC_KHOM5] = { TTYCODE_STRING, "kHOM5" },
	[TTYC_KHOM6] = { TTYCODE_STRING, "kHOM6" },
	[TTYC_KHOM7] = { TTYCODE_STRING, "kHOM7" },
	[TTYC_KHOME] = { TTYCODE_STRING, "khome" },
	[TTYC_KIC2] = { TTYCODE_STRING, "kIC" },
	[TTYC_KIC3] = { TTYCODE_STRING, "kIC3" },
	[TTYC_KIC4] = { TTYCODE_STRING, "kIC4" },
	[TTYC_KIC5] = { TTYCODE_STRING, "kIC5" },
	[TTYC_KIC6] = { TTYCODE_STRING, "kIC6" },
	[TTYC_KIC7] = { TTYCODE_STRING, "kIC7" },
	[TTYC_KICH1] = { TTYCODE_STRING, "kich1" },
	[TTYC_KLFT2] = { TTYCODE_STRING, "kLFT" },
	[TTYC_KLFT3] = { TTYCODE_STRING, "kLFT3" },
	[TTYC_KLFT4] = { TTYCODE_STRING, "kLFT4" },
	[TTYC_KLFT5] = { TTYCODE_STRING, "kLFT5" },
	[TTYC_KLFT6] = { TTYCODE_STRING, "kLFT6" },
	[TTYC_KLFT7] = { TTYCODE_STRING, "kLFT7" },
	[TTYC_KMOUS] = { TTYCODE_STRING, "kmous" },
	[TTYC_KNP] = { TTYCODE_STRING, "knp" },
	[TTYC_KNXT2] = { TTYCODE_STRING, "kNXT" },
	[TTYC_KNXT3] = { TTYCODE_STRING, "kNXT3" },
	[TTYC_KNXT4] = { TTYCODE_STRING, "kNXT4" },
	[TTYC_KNXT5] = { TTYCODE_STRING, "kNXT5" },
	[TTYC_KNXT6] = { TTYCODE_STRING, "kNXT6" },
	[TTYC_KNXT7] = { TTYCODE_STRING, "kNXT7" },
	[TTYC_KPP] = { TTYCODE_STRING, "kpp" },
	[TTYC_KPRV2] = { TTYCODE_STRING, "kPRV" },
	[TTYC_KPRV3] = { TTYCODE_STRING, "kPRV3" },
	[TTYC_KPRV4] = { TTYCODE_STRING, "kPRV4" },
	[TTYC_KPRV5] = { TTYCODE_STRING, "kPRV5" },
	[TTYC_KPRV6] = { TTYCODE_STRING, "kPRV6" },
	[TTYC_KPRV7] = { TTYCODE_STRING, "kPRV7" },
	[TTYC_KRIT2] = { TTYCODE_STRING, "kRIT" },
	[TTYC_KRIT3] = { TTYCODE_STRING, "kRIT3" },
	[TTYC_KRIT4] = { TTYCODE_STRING, "kRIT4" },
	[TTYC_KRIT5] = { TTYCODE_STRING, "kRIT5" },
	[TTYC_KRIT6] = { TTYCODE_STRING, "kRIT6" },
	[TTYC_KRIT7] = { TTYCODE_STRING, "kRIT7" },
	[TTYC_KUP2] = { TTYCODE_STRING, "kUP" },
	[TTYC_KUP3] = { TTYCODE_STRING, "kUP3" },
	[TTYC_KUP4] = { TTYCODE_STRING, "kUP4" },
	[TTYC_KUP5] = { TTYCODE_STRING, "kUP5" },
	[TTYC_KUP6] = { TTYCODE_STRING, "kUP6" },
	[TTYC_KUP7] = { TTYCODE_STRING, "kUP7" },
	[TTYC_MS] = { TTYCODE_STRING, "Ms" },
	[TTYC_OP] = { TTYCODE_STRING, "op" },
	[TTYC_REV] = { TTYCODE_STRING, "rev" },
	[TTYC_RI] = { TTYCODE_STRING, "ri" },
	[TTYC_RMACS] = { TTYCODE_STRING, "rmacs" },
	[TTYC_RMCUP] = { TTYCODE_STRING, "rmcup" },
	[TTYC_RMKX] = { TTYCODE_STRING, "rmkx" },
	[TTYC_SE] = { TTYCODE_STRING, "Se" },
	[TTYC_SETAB] = { TTYCODE_STRING, "setab" },
	[TTYC_SETAF] = { TTYCODE_STRING, "setaf" },
	[TTYC_SGR0] = { TTYCODE_STRING, "sgr0" },
	[TTYC_SITM] = { TTYCODE_STRING, "sitm" },
	[TTYC_SMACS] = { TTYCODE_STRING, "smacs" },
	[TTYC_SMCUP] = { TTYCODE_STRING, "smcup" },
	[TTYC_SMKX] = { TTYCODE_STRING, "smkx" },
	[TTYC_SMSO] = { TTYCODE_STRING, "smso" },
	[TTYC_SMUL] = { TTYCODE_STRING, "smul" },
	[TTYC_SS] = { TTYCODE_STRING, "Ss" },
	[TTYC_TC] = { TTYCODE_FLAG, "Tc" },
	[TTYC_TSL] = { TTYCODE_STRING, "tsl" },
	[TTYC_VPA] = { TTYCODE_STRING, "vpa" },
	[TTYC_XENL] = { TTYCODE_FLAG, "xenl" },
	[TTYC_XT] = { TTYCODE_FLAG, "XT" },
};

u_int
tty_term_ncodes(void)
{
	return (nitems(tty_term_codes));
}

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
tty_term_override(struct tty_term *term, const char *overrides)
{
	const struct tty_term_code_entry	*ent;
	struct tty_code				*code;
	char					*termnext, *termstr;
	char					*entnext, *entstr;
	char					*s, *ptr, *val;
	const char				*errstr;
	u_int					 i;
	int					 n, removeflag;

	s = xstrdup(overrides);

	termnext = s;
	while ((termstr = strsep(&termnext, ",")) != NULL) {
		entnext = termstr;

		entstr = strsep(&entnext, ":");
		if (entstr == NULL || entnext == NULL)
			continue;
		if (fnmatch(entstr, term->name, 0) != 0)
			continue;
		while ((entstr = strsep(&entnext, ":")) != NULL) {
			if (*entstr == '\0')
				continue;

			val = NULL;
			removeflag = 0;
			if ((ptr = strchr(entstr, '=')) != NULL) {
				*ptr++ = '\0';
				val = xstrdup(ptr);
				if (strunvis(val, ptr) == -1) {
					free(val);
					val = xstrdup(ptr);
				}
			} else if (entstr[strlen(entstr) - 1] == '@') {
				entstr[strlen(entstr) - 1] = '\0';
				removeflag = 1;
			} else
				val = xstrdup("");

			log_debug("%s override: %s %s",
			    term->name, entstr, removeflag ? "@" : val);
			for (i = 0; i < tty_term_ncodes(); i++) {
				ent = &tty_term_codes[i];
				if (strcmp(entstr, ent->name) != 0)
					continue;
				code = &term->codes[i];

				if (removeflag) {
					code->type = TTYCODE_NONE;
					continue;
				}
				switch (ent->type) {
				case TTYCODE_NONE:
					break;
				case TTYCODE_STRING:
					if (code->type == TTYCODE_STRING)
						free(code->value.string);
					code->value.string = xstrdup(val);
					code->type = ent->type;
					break;
				case TTYCODE_NUMBER:
					n = strtonum(val, 0, INT_MAX, &errstr);
					if (errstr != NULL)
						break;
					code->value.number = n;
					code->type = ent->type;
					break;
				case TTYCODE_FLAG:
					code->value.flag = 1;
					code->type = ent->type;
					break;
				}
			}

			free(val);
		}
	}

	free(s);
}

struct tty_term *
tty_term_find(char *name, int fd, char **cause)
{
	struct tty_term				*term;
	const struct tty_term_code_entry	*ent;
	struct tty_code				*code;
	u_int					 i;
	int		 			 n, error;
	char					*s;
	const char				*acs;

	LIST_FOREACH(term, &tty_terms, entry) {
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
	term->codes = xcalloc (tty_term_ncodes(), sizeof *term->codes);
	LIST_INSERT_HEAD(&tty_terms, term, entry);

	/* Set up curses terminal. */
	if (setupterm(name, fd, &error) != OK) {
		switch (error) {
		case 1:
			xasprintf(cause, "can't use hardcopy terminal: %s",
			    name);
			break;
		case 0:
			xasprintf(cause, "missing or unsuitable terminal: %s",
			    name);
			break;
		case -1:
			xasprintf(cause, "can't find terminfo database");
			break;
		default:
			xasprintf(cause, "unknown error");
			break;
		}
		goto error;
	}

	/* Fill in codes. */
	for (i = 0; i < tty_term_ncodes(); i++) {
		ent = &tty_term_codes[i];

		code = &term->codes[i];
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
			code->value.flag = n;
			break;
		}
	}

	/* Apply terminal overrides. */
	s = options_get_string(global_options, "terminal-overrides");
	tty_term_override(term, s);

	/* Delete curses data. */
#if !defined(NCURSES_VERSION_MAJOR) || NCURSES_VERSION_MAJOR > 5 || \
    (NCURSES_VERSION_MAJOR == 5 && NCURSES_VERSION_MINOR > 6)
	del_curterm(cur_term);
#endif

	/* These are always required. */
	if (!tty_term_has(term, TTYC_CLEAR)) {
		xasprintf(cause, "terminal does not support clear");
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

	/* Figure out if we have 256. */
	if (tty_term_number(term, TTYC_COLORS) == 256)
		term->flags |= TERM_256COLOURS;

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

	/* Generate ACS table. If none is present, use nearest ASCII. */
	memset(term->acs, 0, sizeof term->acs);
	if (tty_term_has(term, TTYC_ACSC))
		acs = tty_term_string(term, TTYC_ACSC);
	else
		acs = "a#j+k+l+m+n+o-p-q-r-s-t+u+v+w+x|y<z>~.";
	for (; acs[0] != '\0' && acs[1] != '\0'; acs += 2)
		term->acs[(u_char) acs[0]][0] = acs[1];

	/* On terminals with xterm titles (XT), fill in tsl and fsl. */
	if (tty_term_flag(term, TTYC_XT) &&
	    !tty_term_has(term, TTYC_TSL) &&
	    !tty_term_has(term, TTYC_FSL)) {
		code = &term->codes[TTYC_TSL];
		code->value.string = xstrdup("\033]0;");
		code->type = TTYCODE_STRING;
		code = &term->codes[TTYC_FSL];
		code->value.string = xstrdup("\007");
		code->type = TTYCODE_STRING;
	}

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

	LIST_REMOVE(term, entry);

	for (i = 0; i < tty_term_ncodes(); i++) {
		if (term->codes[i].type == TTYCODE_STRING)
			free(term->codes[i].value.string);
	}
	free(term->codes);

	free(term->name);
	free(term);
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
		fatalx("not a string: %d", code);
	return (term->codes[code].value.string);
}

const char *
tty_term_string1(struct tty_term *term, enum tty_code_code code, int a)
{
	return (tparm((char *) tty_term_string(term, code), a, 0, 0, 0, 0, 0, 0, 0, 0));
}

const char *
tty_term_string2(struct tty_term *term, enum tty_code_code code, int a, int b)
{
	return (tparm((char *) tty_term_string(term, code), a, b, 0, 0, 0, 0, 0, 0, 0));
}

const char *
tty_term_ptr1(struct tty_term *term, enum tty_code_code code, const void *a)
{
	return (tparm((char *) tty_term_string(term, code), a, 0, 0, 0, 0, 0, 0, 0, 0));
}

const char *
tty_term_ptr2(struct tty_term *term, enum tty_code_code code, const void *a,
    const void *b)
{
	return (tparm((char *) tty_term_string(term, code), a, b, 0, 0, 0, 0, 0, 0, 0));
}

int
tty_term_number(struct tty_term *term, enum tty_code_code code)
{
	if (!tty_term_has(term, code))
		return (0);
	if (term->codes[code].type != TTYCODE_NUMBER)
		fatalx("not a number: %d", code);
	return (term->codes[code].value.number);
}

int
tty_term_flag(struct tty_term *term, enum tty_code_code code)
{
	if (!tty_term_has(term, code))
		return (0);
	if (term->codes[code].type != TTYCODE_FLAG)
		fatalx("not a flag: %d", code);
	return (term->codes[code].value.flag);
}

const char *
tty_term_describe(struct tty_term *term, enum tty_code_code code)
{
	static char	 s[256];
	char		 out[128];

	switch (term->codes[code].type) {
	case TTYCODE_NONE:
		xsnprintf(s, sizeof s, "%4u: %s: [missing]",
		    code, tty_term_codes[code].name);
		break;
	case TTYCODE_STRING:
		strnvis(out, term->codes[code].value.string, sizeof out,
		    VIS_OCTAL|VIS_TAB|VIS_NL);
		xsnprintf(s, sizeof s, "%4u: %s: (string) %s",
		    code, tty_term_codes[code].name,
		    out);
		break;
	case TTYCODE_NUMBER:
		xsnprintf(s, sizeof s, "%4u: %s: (number) %d",
		    code, tty_term_codes[code].name,
		    term->codes[code].value.number);
		break;
	case TTYCODE_FLAG:
		xsnprintf(s, sizeof s, "%4u: %s: (flag) %s",
		    code, tty_term_codes[code].name,
		    term->codes[code].value.flag ? "true" : "false");
		break;
	}
	return (s);
}
