/* $Id: input-keys.c,v 1.18 2009-01-09 16:45:58 nicm Exp $ */

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

struct input_key_ent {
	int		 key;
	const char	*data;

	int		 flags;
#define INPUTKEY_KEYPAD 0x1	/* keypad key */
#define INPUTKEY_CURSOR 0x2	/* cursor key */
#define INPUTKEY_MODIFIER 0x4	/* may be adjusted by modifiers */
#define INPUTKEY_XTERM 0x4	/* may have xterm argument appended */
};

struct input_key_ent input_keys[] = {
	/* Function keys. */
	{ KEYC_F1,     "\033OP", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F2,     "\033OQ", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F3,     "\033OR", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F4,     "\033OS", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F5,     "\033[15~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F6,     "\033[17~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F7,     "\033[18~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F8,     "\033[19~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F9,     "\033[20~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F10,    "\033[21~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F11,    "\033[23~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_F12,    "\033[24~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_IC,     "\033[2~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_DC,     "\033[3~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_HOME,   "\033[1~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_END,    "\033[4~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_NPAGE,  "\033[6~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },
	{ KEYC_PPAGE,  "\033[5~", INPUTKEY_MODIFIER|INPUTKEY_XTERM },

	/* Arrow keys. Cursor versions must come first. */
	{ KEYC_UP,     "\033OA", INPUTKEY_MODIFIER|INPUTKEY_CURSOR },
	{ KEYC_DOWN,   "\033OB", INPUTKEY_MODIFIER|INPUTKEY_CURSOR },
	{ KEYC_LEFT,   "\033OD", INPUTKEY_MODIFIER|INPUTKEY_CURSOR },
	{ KEYC_RIGHT,  "\033OC", INPUTKEY_MODIFIER|INPUTKEY_CURSOR },
	{ KEYC_UP,     "\033[A", INPUTKEY_MODIFIER },
	{ KEYC_DOWN,   "\033[B", INPUTKEY_MODIFIER },
	{ KEYC_LEFT,   "\033[D", INPUTKEY_MODIFIER },
	{ KEYC_RIGHT,  "\033[C", INPUTKEY_MODIFIER },

	/* Keypad keys. Keypad versions must come first. */
	{ KEYC_KP0_1,  "/", INPUTKEY_KEYPAD },
	{ KEYC_KP0_2,  "*", INPUTKEY_KEYPAD },
	{ KEYC_KP0_3,  "-", INPUTKEY_KEYPAD },
	{ KEYC_KP1_0,  "7", INPUTKEY_KEYPAD },
	{ KEYC_KP1_1,  "8", INPUTKEY_KEYPAD },
	{ KEYC_KP1_2,  "9", INPUTKEY_KEYPAD },
	{ KEYC_KP1_3,  "+", INPUTKEY_KEYPAD },
	{ KEYC_KP2_0,  "4", INPUTKEY_KEYPAD },
	{ KEYC_KP2_1,  "5", INPUTKEY_KEYPAD },
	{ KEYC_KP2_2,  "6", INPUTKEY_KEYPAD },
	{ KEYC_KP3_0,  "1", INPUTKEY_KEYPAD },
	{ KEYC_KP3_1,  "2", INPUTKEY_KEYPAD },
	{ KEYC_KP3_2,  "3", INPUTKEY_KEYPAD },
	{ KEYC_KP3_3,  "\n", INPUTKEY_KEYPAD }, /* this can be CRLF too? */
	{ KEYC_KP4_0,  "0", INPUTKEY_KEYPAD },
	{ KEYC_KP4_2,  ".", INPUTKEY_KEYPAD },
	{ KEYC_KP0_1,  "\033Oo", 0 },
	{ KEYC_KP0_2,  "\033Oj", 0 },
	{ KEYC_KP0_3,  "\033Om", 0 },
	{ KEYC_KP1_0,  "\033Ow", 0 },
	{ KEYC_KP1_1,  "\033Ox", 0 },
	{ KEYC_KP1_2,  "\033Oy", 0 },
	{ KEYC_KP1_3,  "\033Ok", 0 },
	{ KEYC_KP2_0,  "\033Ot", 0 },
	{ KEYC_KP2_1,  "\033Ou", 0 },
	{ KEYC_KP2_2,  "\033Ov", 0 },
	{ KEYC_KP3_0,  "\033Oq", 0 },
	{ KEYC_KP3_1,  "\033Or", 0 },
	{ KEYC_KP3_2,  "\033Os", 0 },
	{ KEYC_KP3_3,  "\033OM", 0 },
	{ KEYC_KP4_0,  "\033Op", 0 },
	{ KEYC_KP4_2,  "\033On", 0 },
};

/* Translate a key code from client into an output key sequence. */
void
input_key(struct window *w, int key)
{
	struct input_key_ent   *ike;
	u_int			i;
	char			ch;
	size_t			dlen;

	log_debug2("writing key 0x%x", key);

	if (key != KEYC_NONE && KEYC_REMOVEESC(key) < KEYC_OFFSET) {
		if (KEYC_ISESC(key))
			buffer_write8(w->out, '\033');
		buffer_write8(w->out, (uint8_t) KEYC_REMOVEESC(key));
		return;
	}

	for (i = 0; i < nitems(input_keys); i++) {
		ike = &input_keys[i];

		if ((ike->flags & INPUTKEY_KEYPAD) &&
		    !(w->screen->mode & MODE_KKEYPAD))
			continue;
		if ((ike->flags & INPUTKEY_CURSOR) &&
		    !(w->screen->mode & MODE_KCURSOR))
			continue;
		
		if (ike->flags & INPUTKEY_MODIFIER) {
			if (KEYC_ISCTL(key) && KEYC_ADDCTL(ike->key) == key)
				break;
			if (KEYC_ISESC(key) && KEYC_ADDESC(ike->key) == key)
				break;
			if (KEYC_ISSFT(key) && KEYC_ADDSFT(ike->key) == key)
				break;
		}
		if (ike->key == key)
			break;
	}
	if (i == nitems(input_keys)) {
		log_debug2("key 0x%x missing", key);
		return;
	}
	dlen = strlen(ike->data);

	log_debug2("found key 0x%x: \"%s\"", key, ike->data);

	if (ike->flags & INPUTKEY_XTERM && 
	    options_get_number(&w->options, "xterm-keys")) {
		/* In xterm keys mode, append modifier argument. */
		ch = '\0';
		if (KEYC_ISSFT(key) && KEYC_ISESC(key) && KEYC_ISCTL(key))
			ch = '8';
		else if (KEYC_ISESC(key) && KEYC_ISCTL(key))
			ch = '7';
		else if (KEYC_ISSFT(key) && KEYC_ISCTL(key))
			ch = '6';
		else if (KEYC_ISCTL(key))
			ch = '5';
		else if (KEYC_ISSFT(key) && KEYC_ISESC(key))
			ch = '4';
		else if (KEYC_ISESC(key))
			ch = '3';
		else if (KEYC_ISSFT(key))
			ch = '2';
		if (ch != '\0') {
			log_debug("output argument is: %c", ch);
			buffer_write(w->out, ike->data, dlen - 1);
			buffer_write8(w->out, ';');
			buffer_write8(w->out, ch);
			buffer_write8(w->out, ike->data[dlen - 1]);
		} else
			buffer_write(w->out, ike->data, dlen);
		return;
	}
	if (ike->flags & INPUTKEY_MODIFIER) {
		/* 
		 * If not in xterm keys or not an xterm key handle escape and
		 * control (shift not supported). 
		 */
		if (KEYC_ISESC(key))
			buffer_write8(w->out, '\033');
		if (!KEYC_ISCTL(key)) {
			buffer_write(w->out, ike->data, dlen);
			return;
		}
		buffer_write(w->out, ike->data, dlen - 1);
		buffer_write8(w->out, ike->data[dlen - 1] ^ 0x20);
		return;
	}

	buffer_write(w->out, ike->data, dlen);
}
