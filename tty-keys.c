/* $Id: tty-keys.c,v 1.3 2008-06-06 17:20:30 nicm Exp $ */

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
	const char	*name;
	int	 	 code;
} tty_keys[] = {
/*	{ "kb",	   KEYC_BACKSPACE }, */
	{ "kBEG",  KEYC_SBEG },
	{ "kCAN",  KEYC_SCANCEL },
	{ "kCMD",  KEYC_SCOMMAND },
	{ "kCPY",  KEYC_SCOPY },
	{ "kCRT",  KEYC_SCREATE },
	{ "kDC",   KEYC_SDC },
	{ "kDL",   KEYC_SDL },
	{ "kEND",  KEYC_SEND },
	{ "kEOL",  KEYC_SEOL },
	{ "kEXT",  KEYC_SEXIT },
	{ "kFND",  KEYC_SFIND },
	{ "kHLP",  KEYC_SHELP },
	{ "kHOM",  KEYC_SHOME },
	{ "kIC",   KEYC_SIC },
	{ "kLFT",  KEYC_SLEFT },
	{ "kMOV",  KEYC_SMOVE },
	{ "kMSG",  KEYC_SMESSAGE },
	{ "kNXT",  KEYC_SNEXT },
	{ "kOPT",  KEYC_SOPTIONS },
	{ "kPRT",  KEYC_SPRINT },
	{ "kPRV",  KEYC_SPREVIOUS },
	{ "kRDO",  KEYC_SREDO },
	{ "kRES",  KEYC_SRSUME },
	{ "kRIT",  KEYC_SRIGHT },
	{ "kRPL",  KEYC_SREPLACE },
	{ "kSAV",  KEYC_SSAVE },
	{ "kSPD",  KEYC_SSUSPEND },
	{ "kUND",  KEYC_SUNDO },
	{ "ka1",   KEYC_A1 },
	{ "ka3",   KEYC_A3 },
	{ "kb2",   KEYC_B2 },
	{ "kbeg",  KEYC_BEG },
	{ "kc1",   KEYC_C1 },
	{ "kc3",   KEYC_C3 },
	{ "kcan",  KEYC_CANCEL },
	{ "kcbt",  KEYC_BTAB },
	{ "kclo",  KEYC_CLOSE },
	{ "kclr",  KEYC_CLEAR },
	{ "kcmd",  KEYC_COMMAND },
	{ "kcpy",  KEYC_COPY },
	{ "kcrt",  KEYC_CREATE },
	{ "kctab", KEYC_CTAB },
	{ "kcub1", KEYC_LEFT },
	{ "kcud1", KEYC_DOWN },
	{ "kcuf1", KEYC_RIGHT },
	{ "kcuu1", KEYC_UP },
	{ "kdch1", KEYC_DC },
	{ "kdl1",  KEYC_DL },
	{ "ked",   KEYC_EOS },
	{ "kel",   KEYC_EOL },
	{ "kend",  KEYC_END },
	{ "kent",  KEYC_ENTER },
	{ "kext",  KEYC_EXIT },
	{ "kf0",   KEYC_F0 },
	{ "kf1",   KEYC_F1 },
	{ "kf10",  KEYC_F10 },
	{ "kf11",  KEYC_F11 },
	{ "kf12",  KEYC_F12 },
	{ "kf13",  KEYC_F13 },
	{ "kf14",  KEYC_F14 },
	{ "kf15",  KEYC_F15 },
	{ "kf16",  KEYC_F16 },
	{ "kf17",  KEYC_F17 },
	{ "kf18",  KEYC_F18 },
	{ "kf19",  KEYC_F19 },
	{ "kf2",   KEYC_F2 },
	{ "kf20",  KEYC_F20 },
	{ "kf21",  KEYC_F21 },
	{ "kf22",  KEYC_F22 },
	{ "kf23",  KEYC_F23 },
	{ "kf24",  KEYC_F24 },
	{ "kf25",  KEYC_F25 },
	{ "kf26",  KEYC_F26 },
	{ "kf27",  KEYC_F27 },
	{ "kf28",  KEYC_F28 },
	{ "kf29",  KEYC_F29 },
	{ "kf3",   KEYC_F3 },
	{ "kf30",  KEYC_F30 },
	{ "kf31",  KEYC_F31 },
	{ "kf32",  KEYC_F32 },
	{ "kf33",  KEYC_F33 },
	{ "kf34",  KEYC_F34 },
	{ "kf35",  KEYC_F35 },
	{ "kf36",  KEYC_F36 },
	{ "kf37",  KEYC_F37 },
	{ "kf38",  KEYC_F38 },
	{ "kf39",  KEYC_F39 },
	{ "kf4",   KEYC_F4 },
	{ "kf40",  KEYC_F40 },
	{ "kf41",  KEYC_F41 },
	{ "kf42",  KEYC_F42 },
	{ "kf43",  KEYC_F43 },
	{ "kf44",  KEYC_F44 },
	{ "kf45",  KEYC_F45 },
	{ "kf46",  KEYC_F46 },
	{ "kf47",  KEYC_F47 },
	{ "kf48",  KEYC_F48 },
	{ "kf49",  KEYC_F49 },
	{ "kf5",   KEYC_F5 },
	{ "kf50",  KEYC_F50 },
	{ "kf51",  KEYC_F51 },
	{ "kf52",  KEYC_F52 },
	{ "kf53",  KEYC_F53 },
	{ "kf54",  KEYC_F54 },
	{ "kf55",  KEYC_F55 },
	{ "kf56",  KEYC_F56 },
	{ "kf57",  KEYC_F57 },
	{ "kf58",  KEYC_F58 },
	{ "kf59",  KEYC_F59 },
	{ "kf6",   KEYC_F6 },
	{ "kf60",  KEYC_F60 },
	{ "kf61",  KEYC_F61 },
	{ "kf62",  KEYC_F62 },
	{ "kf63",  KEYC_F63 },
	{ "kf7",   KEYC_F7 },
	{ "kf8",   KEYC_F8 },
	{ "kf9",   KEYC_F9 },
	{ "kfnd",  KEYC_FIND },
	{ "khlp",  KEYC_HELP },
	{ "khome", KEYC_HOME },
	{ "khts",  KEYC_STAB },
	{ "kich1", KEYC_IC },
	{ "kil1",  KEYC_IL },
	{ "kind",  KEYC_SF },
	{ "kll",   KEYC_LL },
	{ "kmov",  KEYC_MOVE },
	{ "kmrk",  KEYC_MARK },
	{ "kmsg",  KEYC_MESSAGE },
	{ "knp",   KEYC_NPAGE },
	{ "knxt",  KEYC_NEXT },
	{ "kopn",  KEYC_OPEN },
	{ "kopt",  KEYC_OPTIONS },
	{ "kpp",   KEYC_PPAGE },
	{ "kprt",  KEYC_PRINT },
	{ "kprv",  KEYC_PREVIOUS },
	{ "krdo",  KEYC_REDO },
	{ "kref",  KEYC_REFERENCE },
	{ "kres",  KEYC_RESUME },
	{ "krfr",  KEYC_REFRESH },
	{ "kri",   KEYC_SR },
	{ "krmir", KEYC_EIC },
	{ "krpl",  KEYC_REPLACE },
	{ "krst",  KEYC_RESTART },
	{ "ksav",  KEYC_SAVE },
	{ "kslt",  KEYC_SELECT },
	{ "kspd",  KEYC_SUSPEND },
	{ "ktbc",  KEYC_CATAB },
	{ "kund",  KEYC_UNDO },
	{ "pmous", KEYC_MOUSE },
};
#define NTTYKEYS (sizeof tty_keys / sizeof tty_keys[0])

RB_GENERATE(tty_keys, tty_key, entry, tty_keys_cmp);

int
tty_keys_cmp(struct tty_key *k1, struct tty_key *k2)
{
	return (strcmp(k1->string, k2->string));
}

void
tty_keys_init(struct tty *tty)
{
	struct tty_key	*tk;
	u_int		 i;
	char		*s;

	RB_INIT(&tty->ktree);

	tty->ksize = 0;
	for (i = 0; i < NTTYKEYS; i++) {
		s = tigetstr(tty_keys[i].name);
		if (s == (char *) -1 || s == (char *) 0)
			continue;
		if (s[0] != '\e' || s[1] == '\0')
			continue;

		tk = xmalloc(sizeof *tk);
		tk->string = xstrdup(s + 1);
		tk->code = tty_keys[i].code;

		if (strlen(tk->string) > tty->ksize)
			tty->ksize = strlen(tk->string);
		RB_INSERT(tty_keys, &tty->ktree, tk);

		log_debug("found key %d: size now %zu", tk->code, tty->ksize);
	}
}

void
tty_keys_free(struct tty *tty)
{
	struct tty_key	*tk, *tl;

	for (tk = RB_MIN(tty_keys, &tty->ktree); tk != NULL; tk = tl) {
		tl = RB_NEXT(tty_keys, &tty->ktree, tk);
		RB_REMOVE(tty_keys, &tty->ktree, tk);
		xfree(tk->string);
		xfree(tk);
	}
}

int
tty_keys_next(struct tty *tty, int *code)
{
	struct tty_key	*tk, tl;
	size_t		 size;
	char		*s;

	if (BUFFER_USED(tty->in) == 0)
		return (1);
	log_debug("keys have %zu bytes", BUFFER_USED(tty->in));

	if (*BUFFER_OUT(tty->in) != '\e') {
		*code = buffer_read8(tty->in);
		return (0);
	}

	tk = NULL;
	s = xmalloc(tty->ksize + 1);
	for (size = tty->ksize; size > 0; size--) {
		if (size >= BUFFER_USED(tty->in))
			continue;
		memcpy(s, BUFFER_OUT(tty->in) + 1, size);
		s[size] = '\0';

		tl.string = s;
		tk = RB_FIND(tty_keys, &tty->ktree, &tl);
		if (tk != NULL)
			break;
	}
	xfree(s);
	if (tk == NULL) {
		size = tty->ksize;
		if (size > BUFFER_USED(tty->in))
			size = BUFFER_USED(tty->in);
		log_debug(
		    "unmatched key: %.*s", (int) size, BUFFER_OUT(tty->in));
		/*
		 * XXX Pass through unchanged.
		 */
		*code = '\e';
		buffer_remove(tty->in, 1);
		return (0);
	}
	buffer_remove(tty->in, size + 1);

	*code = tk->code;
	return (0);
}
