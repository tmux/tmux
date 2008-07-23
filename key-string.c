/* $Id: key-string.c,v 1.7 2008-07-23 23:44:50 nicm Exp $ */

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

#include <string.h>

#include "tmux.h"

struct {
	const char *string;
	int	 key;
} key_string_table[] = {
/*	{ "A1",		KEYC_A1 }, */
/*	{ "A3",		KEYC_A3 }, */
/*	{ "B2",		KEYC_B2 }, */
	{ "BEG",	KEYC_BEG },
	{ "BTAB",	KEYC_BTAB },
/*	{ "C1",		KEYC_C1 }, */
/*	{ "C3",		KEYC_C3 }, */
	{ "CANCEL",	KEYC_CANCEL },
	{ "CATAB",	KEYC_CATAB },
	{ "CLEAR",	KEYC_CLEAR },
	{ "CLOSE",	KEYC_CLOSE },
	{ "COMMAND",	KEYC_COMMAND },
	{ "COPY",	KEYC_COPY },
	{ "CREATE",	KEYC_CREATE },
	{ "CTAB",	KEYC_CTAB },
	{ "DC",		KEYC_DC },
	{ "DL",		KEYC_DL },
	{ "DOWN",	KEYC_DOWN},
	{ "EIC",	KEYC_EIC },
	{ "END",	KEYC_END },
/*	{ "ENTER",	KEYC_ENTER }, */
	{ "EOL",	KEYC_EOL },
	{ "EOS",	KEYC_EOS },
	{ "EXIT",	KEYC_EXIT },
	{ "F0",		KEYC_F0 },
	{ "F1",		KEYC_F1 },
	{ "F10",	KEYC_F10 },
	{ "F11",	KEYC_F11 },
	{ "F12",	KEYC_F12 },
	{ "F13",	KEYC_F13 },
	{ "F14",	KEYC_F14 },
	{ "F15",	KEYC_F15 },
	{ "F16",	KEYC_F16 },
	{ "F17",	KEYC_F17 },
	{ "F18",	KEYC_F18 },
	{ "F19",	KEYC_F19 },
	{ "F2",		KEYC_F2 },
	{ "F20",	KEYC_F20 },
	{ "F21",	KEYC_F21 },
	{ "F22",	KEYC_F22 },
	{ "F23",	KEYC_F23 },
	{ "F24",	KEYC_F24 },
	{ "F25",	KEYC_F25 },
	{ "F26",	KEYC_F26 },
	{ "F27",	KEYC_F27 },
	{ "F28",	KEYC_F28 },
	{ "F29",	KEYC_F29 },
	{ "F3",		KEYC_F3 },
	{ "F30",	KEYC_F30 },
	{ "F31",	KEYC_F31 },
	{ "F32",	KEYC_F32 },
	{ "F33",	KEYC_F33 },
	{ "F34",	KEYC_F34 },
	{ "F35",	KEYC_F35 },
	{ "F36",	KEYC_F36 },
	{ "F37",	KEYC_F37 },
	{ "F38",	KEYC_F38 },
	{ "F39",	KEYC_F39 },
	{ "F4",		KEYC_F4 },
	{ "F40",	KEYC_F40 },
	{ "F41",	KEYC_F41 },
	{ "F42",	KEYC_F42 },
	{ "F43",	KEYC_F43 },
	{ "F44",	KEYC_F44 },
	{ "F45",	KEYC_F45 },
	{ "F46",	KEYC_F46 },
	{ "F47",	KEYC_F47 },
	{ "F48",	KEYC_F48 },
	{ "F49",	KEYC_F49 },
	{ "F5",		KEYC_F5 },
	{ "F50",	KEYC_F50 },
	{ "F51",	KEYC_F51 },
	{ "F52",	KEYC_F52 },
	{ "F53",	KEYC_F53 },
	{ "F54",	KEYC_F54 },
	{ "F55",	KEYC_F55 },
	{ "F56",	KEYC_F56 },
	{ "F57",	KEYC_F57 },
	{ "F58",	KEYC_F58 },
	{ "F59",	KEYC_F59 },
	{ "F6",		KEYC_F6 },
	{ "F60",	KEYC_F60 },
	{ "F61",	KEYC_F61 },
	{ "F62",	KEYC_F62 },
	{ "F63",	KEYC_F63 },
	{ "F7",		KEYC_F7 },
	{ "F8",		KEYC_F8 },
	{ "F9",		KEYC_F9 },
	{ "FIND",	KEYC_FIND },
	{ "HELP",	KEYC_HELP },
	{ "HOME",	KEYC_HOME },
	{ "IC",		KEYC_IC },
	{ "IL",		KEYC_IL },
	{ "LEFT",	KEYC_LEFT },
	{ "LL",		KEYC_LL },
	{ "MARK",	KEYC_MARK },
	{ "MESSAGE",	KEYC_MESSAGE },
	{ "MOVE",	KEYC_MOVE },
	{ "NEXT",	KEYC_NEXT },
	{ "NPAGE",	KEYC_NPAGE },
	{ "OPEN",	KEYC_OPEN },
	{ "OPTIONS",	KEYC_OPTIONS },
	{ "PPAGE",	KEYC_PPAGE },
	{ "PREVIOUS",	KEYC_PREVIOUS },
	{ "PRINT",	KEYC_PRINT },
	{ "REDO",	KEYC_REDO },
	{ "REFERENCE",	KEYC_REFERENCE },
	{ "REFRESH",	KEYC_REFRESH },
	{ "REPLACE",	KEYC_REPLACE },
	{ "RESTART",	KEYC_RESTART },
	{ "RESUME",	KEYC_RESUME },
	{ "RIGHT",	KEYC_RIGHT },
	{ "SAVE",	KEYC_SAVE },
	{ "SBEG",	KEYC_SBEG },
	{ "SCANCEL",	KEYC_SCANCEL },
	{ "SCOMMAND",	KEYC_SCOMMAND },
	{ "SCOPY",	KEYC_SCOPY },
	{ "SCREATE",	KEYC_SCREATE },
	{ "SDC",	KEYC_SDC },
	{ "SDL",	KEYC_SDL },
	{ "SELECT",	KEYC_SELECT },
	{ "SEND",	KEYC_SEND },
	{ "SEOL",	KEYC_SEOL },
	{ "SEXIT",	KEYC_SEXIT },
	{ "SF",		KEYC_SF },
	{ "SFIND",	KEYC_SFIND },
	{ "SHELP",	KEYC_SHELP },
	{ "SHOME",	KEYC_SHOME },
	{ "SIC",	KEYC_SIC },
	{ "SLEFT",	KEYC_SLEFT },
	{ "SMESSAGE",	KEYC_SMESSAGE },
	{ "SMOVE",	KEYC_SMOVE },
	{ "SNEXT",	KEYC_SNEXT },
	{ "SOPTIONS",	KEYC_SOPTIONS },
	{ "SPREVIOUS",	KEYC_SPREVIOUS },
	{ "SPRINT",	KEYC_SPRINT },
	{ "SR",		KEYC_SR },
	{ "SREDO",	KEYC_SREDO },
	{ "SREPLACE",	KEYC_SREPLACE },
	{ "SRIGHT",	KEYC_SRIGHT },
	{ "SRSUME",	KEYC_SRSUME },
	{ "SSAVE",	KEYC_SSAVE },
	{ "SSUSPEND",	KEYC_SSUSPEND },
	{ "STAB",	KEYC_STAB },
	{ "SUNDO",	KEYC_SUNDO },
	{ "SUSPEND",	KEYC_SUSPEND },
	{ "UNDO",	KEYC_UNDO },
	{ "UP",		KEYC_UP },

	/* Numeric keypad. */
	{ "KP/", 	KEYC_KP0_1 },
	{ "KP*",	KEYC_KP0_2 },
	{ "KP-",	KEYC_KP0_3 },
	{ "KP7",	KEYC_KP1_0 },
	{ "KP8",	KEYC_KP1_1 },
	{ "KP9",	KEYC_KP1_2 },
	{ "KP+",	KEYC_KP1_3 },
	{ "KP4",	KEYC_KP2_0 },
	{ "KP5",	KEYC_KP2_1 },
	{ "KP6",	KEYC_KP2_2 },
	{ "KP1",	KEYC_KP3_0 },
	{ "KP2",	KEYC_KP3_1 },
	{ "KP3",	KEYC_KP3_2 },
	{ "KPENTER",	KEYC_KP3_3 },
	{ "KP0",	KEYC_KP4_0 },
	{ "KP.",	KEYC_KP4_2 },
};
#define NKEYSTRINGS (sizeof key_string_table / sizeof key_string_table[0])

int
key_string_lookup_string(const char *string)
{
	u_int	i;
	int	key;

	if (string[0] == '\0')
		return (KEYC_NONE);
	if (string[1] == '\0')
		return (string[0]);

	if (string[0] == 'C' && string[1] == '-') {
		if (string[2] == '\0' || string[3] != '\0')
			return (KEYC_NONE);
		if (string[2] >= 64 && string[2] <= 95)
			return (string[2] - 64);
		if (string[2] >= 97 && string[2] <= 122)
			return (string[2] - 96);
		return (KEYC_NONE);
	}

	if (string[0] == '^') {
		if (string[1] == '\0' || string[2] != '\0')
			return (KEYC_NONE);
		if (string[1] >= 64 && string[1] <= 95)
			return (string[1] - 64);
		if (string[1] >= 97 && string[1] <= 122)
			return (string[1] - 96);
		return (KEYC_NONE);
	}

	if (string[0] == 'M' && string[1] == '-') {
		if ((key = key_string_lookup_string(string + 2)) == KEYC_NONE)
			return (KEYC_NONE);
		return (KEYC_ADDESCAPE(key));
	}

	for (i = 0; i < NKEYSTRINGS; i++) {
		if (strcasecmp(string, key_string_table[i].string) == 0)
			return (key_string_table[i].key);
	}
	return (KEYC_NONE);
}

const char *
key_string_lookup_key(int key)
{
	static char tmp[24], tmp2[24];
	const char *s;
	u_int	    i;

	if (key == 127)
		return (NULL);

	if (KEYC_ISESCAPE(key)) {
		if ((s = key_string_lookup_key(KEYC_REMOVEESCAPE(key))) == NULL)
			return (NULL);
		xsnprintf(tmp2, sizeof tmp2, "M-%s", s);
		return (tmp2);
	}

	if (key >= 32 && key <= 255) {
		tmp[0] = key;
		tmp[1] = '\0';
		return (tmp);
	}

	if (key >= 0 && key <= 32) {
		if (key == 0 || key > 26)
			xsnprintf(tmp, sizeof tmp, "C-%c", 64 + key);
		else
			xsnprintf(tmp, sizeof tmp, "C-%c", 96 + key);
		return (tmp);
	}

	for (i = 0; i < NKEYSTRINGS; i++) {
		if (key == key_string_table[i].key)
			return (key_string_table[i].string);
	}
	return (NULL);
}
