/* $Id: mode-key.c,v 1.6 2009-01-15 19:27:31 nicm Exp $ */

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

#include "tmux.h"

struct mode_key_entry {
	enum mode_key	mkey;
	int		key;
};

const struct mode_key_entry mode_key_table_vi[] = {
	{ MODEKEY_BOL, '0' },
	{ MODEKEY_BOL, '^' },
	{ MODEKEY_CLEARSEL, '\033' },
	{ MODEKEY_COPYSEL, '\r' },
	{ MODEKEY_DOWN, 'j' },
	{ MODEKEY_DOWN, KEYC_DOWN },
	{ MODEKEY_ENTER, '\r' },
	{ MODEKEY_EOL, '$' },
	{ MODEKEY_LEFT, 'h' },
	{ MODEKEY_LEFT, KEYC_LEFT },
	{ MODEKEY_NPAGE, '\006' },
	{ MODEKEY_NPAGE, KEYC_NPAGE },
	{ MODEKEY_NWORD, 'w' },
	{ MODEKEY_PPAGE, '\025' },
	{ MODEKEY_PPAGE, KEYC_PPAGE },
	{ MODEKEY_PWORD, 'b' },
	{ MODEKEY_QUIT, 'q' },
	{ MODEKEY_RIGHT, 'l' },
	{ MODEKEY_RIGHT, KEYC_RIGHT },
	{ MODEKEY_STARTSEL, ' ' },
	{ MODEKEY_UP, 'k' },
	{ MODEKEY_UP, KEYC_UP },
};

const struct mode_key_entry mode_key_table_emacs[] = {
	{ MODEKEY_BOL, '\001' },
	{ MODEKEY_CLEARSEL, '\007' },
	{ MODEKEY_COPYSEL, '\027' },
	{ MODEKEY_COPYSEL, KEYC_ADDESC('w') },
	{ MODEKEY_DOWN, KEYC_DOWN },
	{ MODEKEY_ENTER, '\r' },
	{ MODEKEY_EOL, '\005' },
	{ MODEKEY_LEFT, '\002' },
	{ MODEKEY_LEFT, KEYC_LEFT },
	{ MODEKEY_NPAGE, KEYC_NPAGE },
	{ MODEKEY_NWORD, KEYC_ADDESC('f') },
	{ MODEKEY_PPAGE, KEYC_PPAGE },
	{ MODEKEY_PWORD, KEYC_ADDESC('b') },
	{ MODEKEY_QUIT, '\033' },
	{ MODEKEY_QUIT, 'q' },
	{ MODEKEY_RIGHT, '\006' },
	{ MODEKEY_RIGHT, KEYC_RIGHT },
	{ MODEKEY_STARTSEL, '\000' },
	{ MODEKEY_UP, KEYC_UP },
};

enum mode_key
mode_key_lookup(int table, int key)
{
	const struct mode_key_entry   	*ptr;
	u_int				 i, n;

	if (table == MODEKEY_EMACS) {
		ptr = mode_key_table_emacs;
		n = nitems(mode_key_table_emacs);
	} else if (table == MODEKEY_VI) {
		ptr = mode_key_table_vi;
		n = nitems(mode_key_table_vi);
	} else
		return (MODEKEY_NONE);

	for (i = 0; i < n; i++) {
		if (ptr[i].key == key)
			return (ptr[i].mkey);
	}
	return (MODEKEY_NONE);
}
