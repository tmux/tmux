/* $Id: input-keys.c,v 1.3 2007-11-21 13:11:41 nicm Exp $ */

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

struct {
	int		 key;
	const char	*data;
} input_keys[] = {
	{ KEYC_DC,     "\e[3~" },
	{ KEYC_DOWN,   "\eOB" },
	{ KEYC_F1,     "\eOP" },
	{ KEYC_F10,    "\e[21~" },
	{ KEYC_F11,    "\e[23~" },
	{ KEYC_F12,    "\e[24~" },
	{ KEYC_F2,     "\eOQ" },
	{ KEYC_F3,     "\eOR" },
	{ KEYC_F4,     "\eOS" },
	{ KEYC_F5,     "\e[15~" },
	{ KEYC_F6,     "\e[17~" },
	{ KEYC_F7,     "\e[18~" },
	{ KEYC_F8,     "\e[19~" },
	{ KEYC_F9,     "\e[20~" },
	{ KEYC_HOME,   "\e[1~" },
	{ KEYC_IC,     "\e[2~" },
	{ KEYC_LEFT,   "\eOD" },
	{ KEYC_LL,     "\e[4~" },
	{ KEYC_NPAGE,  "\e[6~" },
	{ KEYC_PPAGE,  "\e[5~" },
	{ KEYC_RIGHT,  "\eOC" },
	{ KEYC_UP,     "\eOA" }
};
#define NINPUTKEYS (sizeof input_keys / sizeof input_keys[0])

/* Translate a key code from client into an output key sequence. */
void
input_key(struct buffer *b, int key)
{
	u_int	i;

	log_debug2("writing key %d", key);
	if (key != KEYC_NONE && key >= 0) {
		input_store8(b, key);
		return;
	}

	for (i = 0; i < NINPUTKEYS; i++) {
		if (input_keys[i].key == key) {
			log_debug2(
			    "found key %d: \"%s\"", key, input_keys[i].data);
			buffer_write(
			    b, input_keys[i].data, strlen(input_keys[i].data));
			return;
		}
	}
}
