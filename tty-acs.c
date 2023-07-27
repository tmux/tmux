/* $OpenBSD$ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Table mapping ACS entries to UTF-8. */
struct tty_acs_entry {
	u_char		 key;
	const char	*string;
};
static const struct tty_acs_entry tty_acs_table[] = {
	{ '+', "\342\206\222" },	/* arrow pointing right */
	{ ',', "\342\206\220" },	/* arrow pointing left */
	{ '-', "\342\206\221" },	/* arrow pointing up */
	{ '.', "\342\206\223" },	/* arrow pointing down */
	{ '0', "\342\226\256" },	/* solid square block */
	{ '`', "\342\227\206" },	/* diamond */
	{ 'a', "\342\226\222" },	/* checker board (stipple) */
	{ 'b', "\342\220\211" },
	{ 'c', "\342\220\214" },
	{ 'd', "\342\220\215" },
	{ 'e', "\342\220\212" },
	{ 'f', "\302\260" },		/* degree symbol */
	{ 'g', "\302\261" },		/* plus/minus */
	{ 'h', "\342\220\244" },
	{ 'i', "\342\220\213" },
	{ 'j', "\342\224\230" },	/* lower right corner */
	{ 'k', "\342\224\220" },	/* upper right corner */
	{ 'l', "\342\224\214" },	/* upper left corner */
	{ 'm', "\342\224\224" },	/* lower left corner */
	{ 'n', "\342\224\274" },	/* large plus or crossover */
	{ 'o', "\342\216\272" },	/* scan line 1 */
	{ 'p', "\342\216\273" },	/* scan line 3 */
	{ 'q', "\342\224\200" },	/* horizontal line */
	{ 'r', "\342\216\274" },	/* scan line 7 */
	{ 's', "\342\216\275" },	/* scan line 9 */
	{ 't', "\342\224\234" },	/* tee pointing right */
	{ 'u', "\342\224\244" },	/* tee pointing left */
	{ 'v', "\342\224\264" },	/* tee pointing up */
	{ 'w', "\342\224\254" },	/* tee pointing down */
	{ 'x', "\342\224\202" },	/* vertical line */
	{ 'y', "\342\211\244" },	/* less-than-or-equal-to */
	{ 'z', "\342\211\245" },	/* greater-than-or-equal-to */
	{ '{', "\317\200" },		/* greek pi */
	{ '|', "\342\211\240" },	/* not-equal */
	{ '}', "\302\243" },		/* UK pound sign */
	{ '~', "\302\267" }		/* bullet */
};

/* Table mapping UTF-8 to ACS entries. */
struct tty_acs_reverse_entry {
	const char	*string;
	u_char		 key;
};
static const struct tty_acs_reverse_entry tty_acs_reverse2[] = {
	{ "\302\267", '~' }
};
static const struct tty_acs_reverse_entry tty_acs_reverse3[] = {
	{ "\342\224\200", 'q' },
	{ "\342\224\201", 'q' },
	{ "\342\224\202", 'x' },
	{ "\342\224\203", 'x' },
	{ "\342\224\214", 'l' },
	{ "\342\224\217", 'k' },
	{ "\342\224\220", 'k' },
	{ "\342\224\223", 'l' },
	{ "\342\224\224", 'm' },
	{ "\342\224\227", 'm' },
	{ "\342\224\230", 'j' },
	{ "\342\224\233", 'j' },
	{ "\342\224\234", 't' },
	{ "\342\224\243", 't' },
	{ "\342\224\244", 'u' },
	{ "\342\224\253", 'u' },
	{ "\342\224\263", 'w' },
	{ "\342\224\264", 'v' },
	{ "\342\224\273", 'v' },
	{ "\342\224\274", 'n' },
	{ "\342\225\213", 'n' },
	{ "\342\225\220", 'q' },
	{ "\342\225\221", 'x' },
	{ "\342\225\224", 'l' },
	{ "\342\225\227", 'k' },
	{ "\342\225\232", 'm' },
	{ "\342\225\235", 'j' },
	{ "\342\225\240", 't' },
	{ "\342\225\243", 'u' },
	{ "\342\225\246", 'w' },
	{ "\342\225\251", 'v' },
	{ "\342\225\254", 'n' },
};

/* UTF-8 double borders. */
static const struct utf8_data tty_acs_double_borders_list[] = {
	{ "", 0, 0, 0 },
	{ "\342\225\221", 0, 3, 1 }, /* U+2551 */
	{ "\342\225\220", 0, 3, 1 }, /* U+2550 */
	{ "\342\225\224", 0, 3, 1 }, /* U+2554 */
	{ "\342\225\227", 0, 3, 1 }, /* U+2557 */
	{ "\342\225\232", 0, 3, 1 }, /* U+255A */
	{ "\342\225\235", 0, 3, 1 }, /* U+255D */
	{ "\342\225\246", 0, 3, 1 }, /* U+2566 */
	{ "\342\225\251", 0, 3, 1 }, /* U+2569 */
	{ "\342\225\240", 0, 3, 1 }, /* U+2560 */
	{ "\342\225\243", 0, 3, 1 }, /* U+2563 */
	{ "\342\225\254", 0, 3, 1 }, /* U+256C */
	{ "\302\267",	  0, 2, 1 }  /* U+00B7 */
};

/* UTF-8 heavy borders. */
static const struct utf8_data tty_acs_heavy_borders_list[] = {
	{ "", 0, 0, 0 },
	{ "\342\224\203", 0, 3, 1 }, /* U+2503 */
	{ "\342\224\201", 0, 3, 1 }, /* U+2501 */
	{ "\342\224\217", 0, 3, 1 }, /* U+250F */
	{ "\342\224\223", 0, 3, 1 }, /* U+2513 */
	{ "\342\224\227", 0, 3, 1 }, /* U+2517 */
	{ "\342\224\233", 0, 3, 1 }, /* U+251B */
	{ "\342\224\263", 0, 3, 1 }, /* U+2533 */
	{ "\342\224\273", 0, 3, 1 }, /* U+253B */
	{ "\342\224\243", 0, 3, 1 }, /* U+2523 */
	{ "\342\224\253", 0, 3, 1 }, /* U+252B */
	{ "\342\225\213", 0, 3, 1 }, /* U+254B */
	{ "\302\267",	  0, 2, 1 }  /* U+00B7 */
};

/* UTF-8 rounded borders. */
static const struct utf8_data tty_acs_rounded_borders_list[] = {
	{ "", 0, 0, 0 },
	{ "\342\224\202", 0, 3, 1 }, /* U+2502 */
	{ "\342\224\200", 0, 3, 1 }, /* U+2500 */
	{ "\342\225\255", 0, 3, 1 }, /* U+256D */
	{ "\342\225\256", 0, 3, 1 }, /* U+256E */
	{ "\342\225\260", 0, 3, 1 }, /* U+2570 */
	{ "\342\225\257", 0, 3, 1 }, /* U+256F */
	{ "\342\224\263", 0, 3, 1 }, /* U+2533 */
	{ "\342\224\273", 0, 3, 1 }, /* U+253B */
	{ "\342\224\234", 0, 3, 1 }, /* U+2524 */
	{ "\342\224\244", 0, 3, 1 }, /* U+251C */
	{ "\342\225\213", 0, 3, 1 }, /* U+254B */
	{ "\302\267",	  0, 2, 1 }  /* U+00B7 */
};

/* Get cell border character for double style. */
const struct utf8_data *
tty_acs_double_borders(int cell_type)
{
	return (&tty_acs_double_borders_list[cell_type]);
}

/* Get cell border character for heavy style. */
const struct utf8_data *
tty_acs_heavy_borders(int cell_type)
{
	return (&tty_acs_heavy_borders_list[cell_type]);
}

/* Get cell border character for rounded style. */
const struct utf8_data *
tty_acs_rounded_borders(int cell_type)
{
	return (&tty_acs_rounded_borders_list[cell_type]);
}

static int
tty_acs_cmp(const void *key, const void *value)
{
	const struct tty_acs_entry	*entry = value;
	int				 test = *(u_char *)key;

	return (test - entry->key);
}

static int
tty_acs_reverse_cmp(const void *key, const void *value)
{
	const struct tty_acs_reverse_entry	*entry = value;
	const char				*test = key;

	return (strcmp(test, entry->string));
}

/* Should this terminal use ACS instead of UTF-8 line drawing? */
int
tty_acs_needed(struct tty *tty)
{
	if (tty == NULL)
		return (0);

	/*
	 * If the U8 flag is present, it marks whether a terminal supports
	 * UTF-8 and ACS together.
	 *
	 * If it is present and zero, we force ACS - this gives users a way to
	 * turn off UTF-8 line drawing.
	 *
	 * If it is nonzero, we can fall through to the default and use UTF-8
	 * line drawing on UTF-8 terminals.
	 */
	if (tty_term_has(tty->term, TTYC_U8) &&
	    tty_term_number(tty->term, TTYC_U8) == 0)
		return (1);

	if (tty->client->flags & CLIENT_UTF8)
		return (0);
	return (1);
}

/* Retrieve ACS to output as UTF-8. */
const char *
tty_acs_get(struct tty *tty, u_char ch)
{
	const struct tty_acs_entry	*entry;

	/* Use the ACS set instead of UTF-8 if needed. */
	if (tty_acs_needed(tty)) {
		if (tty->term->acs[ch][0] == '\0')
			return (NULL);
		return (&tty->term->acs[ch][0]);
	}

	/* Otherwise look up the UTF-8 translation. */
	entry = bsearch(&ch, tty_acs_table, nitems(tty_acs_table),
	    sizeof tty_acs_table[0], tty_acs_cmp);
	if (entry == NULL)
		return (NULL);
	return (entry->string);
}

/* Reverse UTF-8 into ACS. */
int
tty_acs_reverse_get(__unused struct tty *tty, const char *s, size_t slen)
{
	const struct tty_acs_reverse_entry	*table, *entry;
	u_int					 items;

	if (slen == 2) {
		table = tty_acs_reverse2;
		items = nitems(tty_acs_reverse2);
	} else if (slen == 3) {
		table = tty_acs_reverse3;
		items = nitems(tty_acs_reverse3);
	} else
		return (-1);
	entry = bsearch(s, table, items, sizeof table[0], tty_acs_reverse_cmp);
	if (entry == NULL)
		return (-1);
	return (entry->key);
}
