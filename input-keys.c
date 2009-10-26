/* $OpenBSD$ */

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

/*
 * This file is rather misleadingly named, it contains the code which takes a
 * key code and translates it into something suitable to be sent to the
 * application running in a pane (similar to input.c does in the other
 * direction with output).
 */

struct input_key_ent {
	int		 key;
	const char	*data;

	int		 flags;
#define INPUTKEY_KEYPAD 0x1	/* keypad key */
#define INPUTKEY_CURSOR 0x2	/* cursor key */
};

struct input_key_ent input_keys[] = {
	/* Backspace key. */
	{ KEYC_BSPACE,		"\177",		0 },

	/* Function keys. */
	{ KEYC_F1,		"\033OP",	0 },
	{ KEYC_F2,		"\033OQ",	0 },
	{ KEYC_F3,		"\033OR",	0 },
	{ KEYC_F4,		"\033OS",	0 },
	{ KEYC_F5,		"\033[15~",	0 },
	{ KEYC_F5|KEYC_CTRL,	"\033[15^",	0 },
	{ KEYC_F6,		"\033[17~",	0 },
	{ KEYC_F6|KEYC_CTRL,	"\033[17^",	0 },
	{ KEYC_F7,		"\033[18~",	0 },
	{ KEYC_F7|KEYC_CTRL,	"\033[18^",	0 },
	{ KEYC_F8,		"\033[19~",	0 },
	{ KEYC_F8|KEYC_CTRL,	"\033[19^",	0 },
	{ KEYC_F9,		"\033[20~",	0 },
	{ KEYC_F9|KEYC_CTRL,	"\033[20^",	0 },
	{ KEYC_F10,		"\033[21~",	0 },
	{ KEYC_F10|KEYC_CTRL,	"\033[21^",	0 },
	{ KEYC_F11,		"\033[23~",	0 },
	{ KEYC_F1|KEYC_CTRL,	"\033[23^",	0 },
	{ KEYC_F12,		"\033[24~",	0 },
	{ KEYC_F12|KEYC_CTRL,	"\033[24^",	0 },
	{ KEYC_F13,		"\033[25~",	0 },
	{ KEYC_F13|KEYC_CTRL,	"\033[25^",	0 },
	{ KEYC_F14,		"\033[26~",	0 },
	{ KEYC_F14|KEYC_CTRL,	"\033[26^",	0 },
	{ KEYC_F15,		"\033[28~",	0 },
	{ KEYC_F15|KEYC_CTRL,	"\033[28^",	0 },
	{ KEYC_F16,		"\033[29~",	0 },
	{ KEYC_F16|KEYC_CTRL,	"\033[29^",	0 },
	{ KEYC_F17,		"\033[31~",	0 },
	{ KEYC_F17|KEYC_CTRL,	"\033[31^",	0 },
	{ KEYC_F18,		"\033[32~",	0 },
	{ KEYC_F18|KEYC_CTRL,	"\033[32^",	0 },
	{ KEYC_F19,		"\033[33~",	0 },
	{ KEYC_F19|KEYC_CTRL,	"\033[33^",	0 },
	{ KEYC_F20,		"\033[34~",	0 },
	{ KEYC_F20|KEYC_CTRL,	"\033[34^",	0 },
	{ KEYC_IC,		"\033[2~",	0 },
	{ KEYC_IC|KEYC_CTRL,	"\033[2^",	0 },
	{ KEYC_DC,		"\033[3~",	0 },
	{ KEYC_DC|KEYC_CTRL,	"\033[3^",	0 },
	{ KEYC_HOME,		"\033[1~",	0 },
	{ KEYC_HOME|KEYC_CTRL,	"\033[1^",	0 },
	{ KEYC_END,		"\033[4~",	0 },
	{ KEYC_END|KEYC_CTRL,	"\033[4^",	0 },
	{ KEYC_NPAGE,		"\033[6~",	0 },
	{ KEYC_NPAGE|KEYC_CTRL,	"\033[6^",	0 },
	{ KEYC_PPAGE,		"\033[5~",	0 },
	{ KEYC_PPAGE|KEYC_CTRL,	"\033[5^",	0 },
	{ KEYC_BTAB,		"\033[Z",	0 },

	/* Arrow keys. Cursor versions must come first. */
	{ KEYC_UP|KEYC_CTRL,	"\033Oa",	0 },
	{ KEYC_DOWN|KEYC_CTRL,	"\033Ob",	0 },
	{ KEYC_RIGHT|KEYC_CTRL,	"\033Oc",	0 },
	{ KEYC_LEFT|KEYC_CTRL,	"\033Od",	0 },
	
	{ KEYC_UP|KEYC_SHIFT,	"\033[a",	0 },
	{ KEYC_DOWN|KEYC_SHIFT,	"\033[b",	0 },
	{ KEYC_RIGHT|KEYC_SHIFT,"\033[c",	0 },
	{ KEYC_LEFT|KEYC_SHIFT,	"\033[d",	0 },

	{ KEYC_UP|KEYC_CTRL,	"\033OA",	INPUTKEY_CURSOR },
	{ KEYC_DOWN|KEYC_CTRL,	"\033OB",	INPUTKEY_CURSOR },
	{ KEYC_RIGHT|KEYC_CTRL,	"\033OC",	INPUTKEY_CURSOR },
	{ KEYC_LEFT|KEYC_CTRL,	"\033OD",	INPUTKEY_CURSOR },
	
	{ KEYC_UP,		"\033OA",	INPUTKEY_CURSOR },
	{ KEYC_DOWN,		"\033OB",	INPUTKEY_CURSOR },
	{ KEYC_RIGHT,		"\033OC",	INPUTKEY_CURSOR },
	{ KEYC_LEFT,		"\033OD",	INPUTKEY_CURSOR },

	{ KEYC_UP|KEYC_CTRL,	"\033OA",	0 },
	{ KEYC_DOWN|KEYC_CTRL,	"\033OB",	0 },
	{ KEYC_RIGHT|KEYC_CTRL,	"\033OC",	0 },
	{ KEYC_LEFT|KEYC_CTRL,	"\033OD",	0 },

	{ KEYC_UP,		"\033[A",	0 },
	{ KEYC_DOWN,		"\033[B",	0 },
	{ KEYC_RIGHT,		"\033[C",	0 },
	{ KEYC_LEFT,		"\033[D",	0 },

	/* Keypad keys. Keypad versions must come first. */
	{ KEYC_KP_SLASH,	"/",		INPUTKEY_KEYPAD },
	{ KEYC_KP_STAR,		"*",		INPUTKEY_KEYPAD },
	{ KEYC_KP_MINUS,	"-",		INPUTKEY_KEYPAD },
	{ KEYC_KP_SEVEN,	"7",		INPUTKEY_KEYPAD },
	{ KEYC_KP_EIGHT,	"8",		INPUTKEY_KEYPAD },
	{ KEYC_KP_NINE,		"9",		INPUTKEY_KEYPAD },
	{ KEYC_KP_PLUS,		"+",		INPUTKEY_KEYPAD },
	{ KEYC_KP_FOUR,		"4",		INPUTKEY_KEYPAD },	
	{ KEYC_KP_FIVE,		"5",		INPUTKEY_KEYPAD },
	{ KEYC_KP_SIX,		"6",		INPUTKEY_KEYPAD },
	{ KEYC_KP_ONE,		"1",		INPUTKEY_KEYPAD },
	{ KEYC_KP_TWO,		"2",		INPUTKEY_KEYPAD },
	{ KEYC_KP_THREE,	"3",		INPUTKEY_KEYPAD },
	{ KEYC_KP_ENTER,	"\n",		INPUTKEY_KEYPAD },
	{ KEYC_KP_ZERO,		"0",		INPUTKEY_KEYPAD },
	{ KEYC_KP_PERIOD,	".",		INPUTKEY_KEYPAD },

	{ KEYC_KP_SLASH,	"\033Oo",	0 },
	{ KEYC_KP_STAR,		"\033Oj",	0 },
	{ KEYC_KP_MINUS,	"\033Om",	0 },
	{ KEYC_KP_SEVEN,	"\033Ow",	0 },
	{ KEYC_KP_EIGHT,	"\033Ox",	0 },
	{ KEYC_KP_NINE,		"\033Oy",	0 },
	{ KEYC_KP_PLUS,		"\033Ok",	0 },
	{ KEYC_KP_FOUR,		"\033Ot",	0 },
	{ KEYC_KP_FIVE,		"\033Ou",	0 },
	{ KEYC_KP_SIX,		"\033Ov",	0 },
	{ KEYC_KP_ONE,		"\033Oq",	0 },
	{ KEYC_KP_TWO,		"\033Or",	0 },
	{ KEYC_KP_THREE,	"\033Os",	0 },
	{ KEYC_KP_ENTER,	"\033OM",	0 },
	{ KEYC_KP_ZERO,		"\033Op",	0 },
	{ KEYC_KP_PERIOD,	"\033On",	0 },
};

/* Translate a key code into an output key sequence. */
void
input_key(struct window_pane *wp, int key)
{
	struct input_key_ent   *ike;
	u_int			i;
	size_t			dlen;

	log_debug2("writing key 0x%x", key);

	/*
	 * If this is a normal 7-bit key, just send it, with a leading escape
	 * if necessary.
	 */
	if (key != KEYC_NONE && (key & ~KEYC_ESCAPE) < 0x100) {
		if (key & KEYC_ESCAPE)
			buffer_write8(wp->out, '\033');
		buffer_write8(wp->out, (uint8_t) (key & ~KEYC_ESCAPE));
		return;
	}

	/* Otherwise look the key up in the table. */
	for (i = 0; i < nitems(input_keys); i++) {
		ike = &input_keys[i];

		if ((ike->flags & INPUTKEY_KEYPAD) &&
		    !(wp->screen->mode & MODE_KKEYPAD))
			continue;
		if ((ike->flags & INPUTKEY_CURSOR) &&
		    !(wp->screen->mode & MODE_KCURSOR))
			continue;

		if ((key & KEYC_ESCAPE) && (ike->key | KEYC_ESCAPE) == key)
			break;
		if (ike->key == key)
			break;
	}
	if (i == nitems(input_keys)) {
		log_debug2("key 0x%x missing", key);
		return;
	}
	dlen = strlen(ike->data);
	log_debug2("found key 0x%x: \"%s\"", key, ike->data);

	/* Prefix a \033 for escape. */
	if (key & KEYC_ESCAPE)
		buffer_write8(wp->out, '\033');
	buffer_write(wp->out, ike->data, dlen);
}

/* Translate mouse and output. */
void
input_mouse(struct window_pane *wp, struct mouse_event *m)
{
	if (wp->screen->mode & MODE_MOUSE) {
		buffer_write(wp->out, "\033[M", 3);
		buffer_write8(wp->out, m->b + 32);
		buffer_write8(wp->out, m->x + 33);
		buffer_write8(wp->out, m->y + 33);
	}
}
