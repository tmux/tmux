/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

/*
 * This file is rather misleadingly named, it contains the code which takes a
 * key code and translates it into something suitable to be sent to the
 * application running in a pane (similar to input.c does in the other
 * direction with output).
 */

static void	 input_key_mouse(struct window_pane *, struct mouse_event *);
static int input_key_kitty(struct screen *s,
						   struct bufferevent *bev,key_code key);
u_int get_modifier(key_code key);
u_int get_legacy_modifier(key_code key);

/* Entry in the key tree. */
struct input_key_entry {
	key_code			 key;
	const char			*data;

	RB_ENTRY(input_key_entry)	 entry;
};
RB_HEAD(input_key_tree, input_key_entry);

/* Tree of input keys. */
static int	input_key_cmp(struct input_key_entry *,
		    struct input_key_entry *);
RB_GENERATE_STATIC(input_key_tree, input_key_entry, entry, input_key_cmp);
struct input_key_tree input_key_tree = RB_INITIALIZER(&input_key_tree);

/* List of default keys, the tree is built from this. */
static struct input_key_entry input_key_defaults[] = {
	/* Paste keys. */
	{ .key = KEYC_PASTE_START,
	  .data = "\033[200~"
	},
	{ .key = KEYC_PASTE_END,
	  .data = "\033[201~"
	},

	/* Function keys. */
	{ .key = KEYC_F1,
	  .data = "\033OP"
	},
	{ .key = KEYC_F2,
	  .data = "\033OQ"
	},
	{ .key = KEYC_F3,
	  .data = "\033OR"
	},
	{ .key = KEYC_F4,
	  .data = "\033OS"
	},
	{ .key = KEYC_F5,
	  .data = "\033[15~"
	},
	{ .key = KEYC_F6,
	  .data = "\033[17~"
	},
	{ .key = KEYC_F7,
	  .data = "\033[18~"
	},
	{ .key = KEYC_F8,
	  .data = "\033[19~"
	},
	{ .key = KEYC_F9,
	  .data = "\033[20~"
	},
	{ .key = KEYC_F10,
	  .data = "\033[21~"
	},
	{ .key = KEYC_F11,
	  .data = "\033[23~"
	},
	{ .key = KEYC_F12,
	  .data = "\033[24~"
	},
	{ .key = KEYC_IC,
	  .data = "\033[2~"
	},
	{ .key = KEYC_DC,
	  .data = "\033[3~"
	},
	{ .key = KEYC_HOME,
	  .data = "\033[1~"
	},
	{ .key = KEYC_END,
	  .data = "\033[4~"
	},
	{ .key = KEYC_NPAGE,
	  .data = "\033[6~"
	},
	{ .key = KEYC_PPAGE,
	  .data = "\033[5~"
	},
	{ .key = KEYC_BTAB,
	  .data = "\033[Z"
	},

	/* Arrow keys. */
	{ .key = KEYC_UP|KEYC_CURSOR,
	  .data = "\033OA"
	},
	{ .key = KEYC_DOWN|KEYC_CURSOR,
	  .data = "\033OB"
	},
	{ .key = KEYC_RIGHT|KEYC_CURSOR,
	  .data = "\033OC"
	},
	{ .key = KEYC_LEFT|KEYC_CURSOR,
	  .data = "\033OD"
	},
	{ .key = KEYC_UP,
	  .data = "\033[A"
	},
	{ .key = KEYC_DOWN,
	  .data = "\033[B"
	},
	{ .key = KEYC_RIGHT,
	  .data = "\033[C"
	},
	{ .key = KEYC_LEFT,
	  .data = "\033[D"
	},

	/* Keypad keys. */
	{ .key = KEYC_KP_SLASH|KEYC_KEYPAD,
	  .data = "\033Oo"
	},
	{ .key = KEYC_KP_STAR|KEYC_KEYPAD,
	  .data = "\033Oj"
	},
	{ .key = KEYC_KP_MINUS|KEYC_KEYPAD,
	  .data = "\033Om"
	},
	{ .key = KEYC_KP_SEVEN|KEYC_KEYPAD,
	  .data = "\033Ow"
	},
	{ .key = KEYC_KP_EIGHT|KEYC_KEYPAD,
	  .data = "\033Ox"
	},
	{ .key = KEYC_KP_NINE|KEYC_KEYPAD,
	  .data = "\033Oy"
	},
	{ .key = KEYC_KP_PLUS|KEYC_KEYPAD,
	  .data = "\033Ok"
	},
	{ .key = KEYC_KP_FOUR|KEYC_KEYPAD,
	  .data = "\033Ot"
	},
	{ .key = KEYC_KP_FIVE|KEYC_KEYPAD,
	  .data = "\033Ou"
	},
	{ .key = KEYC_KP_SIX|KEYC_KEYPAD,
	  .data = "\033Ov"
	},
	{ .key = KEYC_KP_ONE|KEYC_KEYPAD,
	  .data = "\033Oq"
	},
	{ .key = KEYC_KP_TWO|KEYC_KEYPAD,
	  .data = "\033Or"
	},
	{ .key = KEYC_KP_THREE|KEYC_KEYPAD,
	  .data = "\033Os"
	},
	{ .key = KEYC_KP_ENTER|KEYC_KEYPAD,
	  .data = "\033OM"
	},
	{ .key = KEYC_KP_ZERO|KEYC_KEYPAD,
	  .data = "\033Op"
	},
	{ .key = KEYC_KP_PERIOD|KEYC_KEYPAD,
	  .data = "\033On"
	},
	{ .key = KEYC_KP_SLASH,
	  .data = "/"
	},
	{ .key = KEYC_KP_STAR,
	  .data = "*"
	},
	{ .key = KEYC_KP_MINUS,
	  .data = "-"
	},
	{ .key = KEYC_KP_SEVEN,
	  .data = "7"
	},
	{ .key = KEYC_KP_EIGHT,
	  .data = "8"
	},
	{ .key = KEYC_KP_NINE,
	  .data = "9"
	},
	{ .key = KEYC_KP_PLUS,
	  .data = "+"
	},
	{ .key = KEYC_KP_FOUR,
	  .data = "4"
	},
	{ .key = KEYC_KP_FIVE,
	  .data = "5"
	},
	{ .key = KEYC_KP_SIX,
	  .data = "6"
	},
	{ .key = KEYC_KP_ONE,
	  .data = "1"
	},
	{ .key = KEYC_KP_TWO,
	  .data = "2"
	},
	{ .key = KEYC_KP_THREE,
	  .data = "3"
	},
	{ .key = KEYC_KP_ENTER,
	  .data = "\n"
	},
	{ .key = KEYC_KP_ZERO,
	  .data = "0"
	},
	{ .key = KEYC_KP_PERIOD,
	  .data = "."
	},

	/* Keys with an embedded modifier. */
	{ .key = KEYC_F1|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_P"
	},
	{ .key = KEYC_F2|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_Q"
	},
	{ .key = KEYC_F3|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_R"
	},
	{ .key = KEYC_F4|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_S"
	},
	{ .key = KEYC_F5|KEYC_BUILD_MODIFIERS,
	  .data = "\033[15;_~"
	},
	{ .key = KEYC_F6|KEYC_BUILD_MODIFIERS,
	  .data = "\033[17;_~"
	},
	{ .key = KEYC_F7|KEYC_BUILD_MODIFIERS,
	  .data = "\033[18;_~"
	},
	{ .key = KEYC_F8|KEYC_BUILD_MODIFIERS,
	  .data = "\033[19;_~"
	},
	{ .key = KEYC_F9|KEYC_BUILD_MODIFIERS,
	  .data = "\033[20;_~"
	},
	{ .key = KEYC_F10|KEYC_BUILD_MODIFIERS,
	  .data = "\033[21;_~"
	},
	{ .key = KEYC_F11|KEYC_BUILD_MODIFIERS,
	  .data = "\033[23;_~"
	},
	{ .key = KEYC_F12|KEYC_BUILD_MODIFIERS,
	  .data = "\033[24;_~"
	},
	{ .key = KEYC_UP|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_A"
	},
	{ .key = KEYC_DOWN|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_B"
	},
	{ .key = KEYC_RIGHT|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_C"
	},
	{ .key = KEYC_LEFT|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_D"
	},
	{ .key = KEYC_HOME|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_H"
	},
	{ .key = KEYC_END|KEYC_BUILD_MODIFIERS,
	  .data = "\033[1;_F"
	},
	{ .key = KEYC_PPAGE|KEYC_BUILD_MODIFIERS,
	  .data = "\033[5;_~"
	},
	{ .key = KEYC_NPAGE|KEYC_BUILD_MODIFIERS,
	  .data = "\033[6;_~"
	},
	{ .key = KEYC_IC|KEYC_BUILD_MODIFIERS,
	  .data = "\033[2;_~"
	},
	{ .key = KEYC_DC|KEYC_BUILD_MODIFIERS,
	  .data = "\033[3;_~"
	},

	/* Tab and modifiers. */
	{ .key = '\011'|KEYC_CTRL,
	  .data = "\011"
	},
	{ .key = '\011'|KEYC_CTRL|KEYC_EXTENDED,
	  .data = "\033[9;5u"
	},
	{ .key = '\011'|KEYC_CTRL|KEYC_SHIFT,
	  .data = "\033[Z"
	},
	{ .key = '\011'|KEYC_CTRL|KEYC_SHIFT|KEYC_EXTENDED,
	  .data = "\033[1;5Z"
	}
};
static const key_code input_key_modifiers[] = {
	0,
	0,
	KEYC_SHIFT,
	KEYC_META|KEYC_IMPLIED_META,
	KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META,
	KEYC_CTRL,
	KEYC_SHIFT|KEYC_CTRL,
	KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL,
	KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL
};

/* Input key comparison function. */
static int
input_key_cmp(struct input_key_entry *ike1, struct input_key_entry *ike2)
{
	if (ike1->key < ike2->key)
		return (-1);
	if (ike1->key > ike2->key)
		return (1);
	return (0);
}

/* Look for key in tree. */
static struct input_key_entry *
input_key_get(key_code key)
{
	struct input_key_entry	entry = { .key = key };

	return (RB_FIND(input_key_tree, &input_key_tree, &entry));
}

/* Split a character into two UTF-8 bytes. */
static size_t
input_key_split2(u_int c, u_char *dst)
{
	if (c > 0x7f) {
		dst[0] = (c >> 6) | 0xc0;
		dst[1] = (c & 0x3f) | 0x80;
		return (2);
	}
	dst[0] = c;
	return (1);
}

/* Build input key tree. */
void
input_key_build(void)
{
	struct input_key_entry	*ike, *new;
	u_int			 i, j;
	char			*data;
	key_code		 key;

	for (i = 0; i < nitems(input_key_defaults); i++) {
		ike = &input_key_defaults[i];
		if (~ike->key & KEYC_BUILD_MODIFIERS) {
			RB_INSERT(input_key_tree, &input_key_tree, ike);
			continue;
		}

		for (j = 2; j < nitems(input_key_modifiers); j++) {
			key = (ike->key & ~KEYC_BUILD_MODIFIERS);
			data = xstrdup(ike->data);
			data[strcspn(data, "_")] = '0' + j;

			new = xcalloc(1, sizeof *new);
			new->key = key|input_key_modifiers[j];
			new->data = data;
			RB_INSERT(input_key_tree, &input_key_tree, new);
		}
	}

	RB_FOREACH(ike, input_key_tree, &input_key_tree) {
		log_debug("%s: 0x%llx (%s) is %s", __func__, ike->key,
		    key_string_lookup_key(ike->key, 1), ike->data);
	}
}

/* Translate a key code into an output key sequence for a pane. */
int
input_key_pane(struct window_pane *wp, key_code key, struct mouse_event *m)
{
	if (log_get_level() != 0) {
		log_debug("writing key 0x%llx (%s) to %%%u", key,
		    key_string_lookup_key(key, 1), wp->id);
	}

	if (KEYC_IS_MOUSE(key)) {
		if (m != NULL && m->wp != -1 && (u_int)m->wp == wp->id)
			input_key_mouse(wp, m);
		return (0);
	}
	return (input_key(wp->screen, wp->event, key));
}

static void
input_key_write(const char *from, struct bufferevent *bev, const char *data,
    size_t size)
{
	log_debug("%s: %.*s", from, (int)size, data);
	bufferevent_write(bev, data, size);
}
u_int
get_modifier(key_code key)
{
	char modifier=0;
	if (key & KEYC_SHIFT)
		modifier |= 0x1;
	if (key & KEYC_META)
		modifier |= 0x2;
	if (key & KEYC_CTRL)
		modifier |= 0x4;
	if (key & KEYC_SUPER)
		modifier |= 0x8;
	if (key & KEYC_HYPER)
		modifier |= 0x10;
	if (key & KEYC_REAL_META)
		modifier |= 0x20;
	if (key & KEYC_CAPS_LOCK)
		modifier |= 0x40;
	if (key & KEYC_KEYPAD)
		modifier |= 0x80;
	modifier++;
	return modifier;
}
u_int
get_legacy_modifier(key_code key)
{
	char modifier=0;
	if (key & KEYC_SHIFT)
		modifier |= 0x1;
	if (key & (KEYC_META|KEYC_SUPER|
			   KEYC_HYPER|KEYC_REAL_META))
		modifier |= 0x2;
	if (key & KEYC_CTRL)
		modifier |= 0x4;
	modifier++;
	return modifier;
}


/* 0:succ,-1 fail */
static int
input_key_kitty(struct screen *s, struct bufferevent *bev,key_code key)
{
	key_code onlykey;
	char		final,	 tmp[64];
	u_int number,modifier;
	struct utf8_data	 ud;
	int disambiguate,all_as_escapes;
	enum kitty_kbd_flags flags;

	number = 0;
	flags = s->kitty_kbd.flags[s->kitty_kbd.idx];
	disambiguate = flags & KITTY_KBD_DISAMBIGUATE;
	all_as_escapes = flags & KITTY_KBD_REPORT_ALL;
	onlykey = (key & KEYC_MASK_KEY);
	modifier = get_modifier(key);

    if (!disambiguate) return (-1);

	/*
	 * If this is a normal 7-bit key, just send it,
	 * If it is a UTF-8 key, split it and send it.
	 */
	if (key <= 0x7f) {
		ud.data[0] = key;
		input_key_write(__func__, bev, &ud.data[0], 1);
		return (0);
	}
	if (KEYC_IS_UNICODE(key)) {
		utf8_to_data(key, &ud);
		input_key_write(__func__, bev, ud.data, ud.size);
		return (0);
	}
	if(all_as_escapes)
        goto emit_escapes;

	switch(key & ~(KEYC_META|KEYC_IMPLIED_META|
				   KEYC_CAPS_LOCK|KEYC_MASK_FLAGS)){
	case '\t':
	case '\r':
	case KEYC_BSPACE:
		return -1;
	}
emit_escapes:
	/* CSI 1; modifiers [ABCDEFHPQS] */
	/* CSI [ABCDEFHPQS] */
	/* CSI number ; modifiers ~ */
	/* CSI number ; modifiers u */
	switch(key & (KEYC_MASK_KEY)){
	case KEYC_UP:			number=1;      final='A';break;
	case KEYC_DOWN:			number=1;      final='B';break;
	case KEYC_RIGHT:		number=1;      final='C';break;
	case KEYC_LEFT:			number=1;      final='D';break;
	case KEYC_KP_BEGIN:		number=1;      final='E';break;
	case KEYC_END:			number=1;      final='F';break;
	case KEYC_HOME:			number=1;      final='H';break;
	case KEYC_F1:			number=1;      final='P';break;
	case KEYC_F2:			number=1;      final='Q';break;
	case KEYC_F4:			number=1;      final='S';break;
    case KEYC_IC:           number=2;      final='~'; break;
    case KEYC_DC:           number=3;      final='~'; break;
    case KEYC_PPAGE:        number=5;      final='~'; break;
    case KEYC_NPAGE:        number=6;      final='~'; break;
		/* case KEYC_HOME:         number=7;      final='~'; break; */
		/* case KEYC_END:          number=8;      final='~'; break; */
		/* case KEYC_F1:           number=11;     final='~'; break; */
		/* case KEYC_F2:           number=12;     final='~'; break; */
    case KEYC_F3:           number=13;     final='~'; break;
		/* case KEYC_F4:           number=14;     final='~'; break; */
    case KEYC_F5:           number=15;     final='~'; break;
    case KEYC_F6:           number=17;     final='~'; break;
    case KEYC_F7:           number=18;     final='~'; break;
    case KEYC_F8:           number=19;     final='~'; break;
    case KEYC_F9:           number=20;     final='~'; break;
    case KEYC_F10:          number=21;     final='~'; break;
    case KEYC_F11:          number=23;     final='~'; break;
    case KEYC_F12:          number=24;     final='~'; break;
		/* case KEYC_KP_BEGIN:     number=57427;  final='~'; break; */
	case 9:		            number=9;      final='u'; break;
	case 27:		        number=27;     final='u'; break; /* esc */
	case KEYC_BSPACE:	    number=127;    final='u'; break;
	case ' ':	            number=' ';    final='u'; break;
	case KEYC_CAPLOCK:	    number=57358; final='u'; break;
	case KEYC_SCROLL_LOCK:	number=57359; final='u'; break;
	case KEYC_KP_NUMLOCK:	number=57360; final='u'; break;
	case KEYC_PRINT:	    number=57361; final='u'; break;
	case KEYC_PAUSE:	    number=57362; final='u'; break;
	case KEYC_MENU:	        number=57363; final='u'; break;
	case KEYC_F13:	        number=57376; final='u'; break;
	case KEYC_F14:	        number=57377; final='u'; break;
	case KEYC_F15:	        number=57378; final='u'; break;
	case KEYC_F16:	        number=57379; final='u'; break;
	case KEYC_F17:	        number=57380; final='u'; break;
	case KEYC_F18:	        number=57381; final='u'; break;
	case KEYC_F19:	        number=57382; final='u'; break;
	case KEYC_F20:	        number=57383; final='u'; break;
	case KEYC_F21:	        number=57384; final='u'; break;
	case KEYC_F22:	        number=57385; final='u'; break;
	case KEYC_F23:	        number=57386; final='u'; break;
	case KEYC_F24:	        number=57387; final='u'; break;
	case KEYC_F25:	        number=57388; final='u'; break;
	case KEYC_F26:	        number=57389; final='u'; break;
	case KEYC_F27:	        number=57390; final='u'; break;
	case KEYC_F28:	        number=57391; final='u'; break;
	case KEYC_F29:	        number=57392; final='u'; break;
	case KEYC_F30:	        number=57393; final='u'; break;
	case KEYC_F31:	        number=57394; final='u'; break;
	case KEYC_F32:	        number=57395; final='u'; break;
	case KEYC_F33:	        number=57396; final='u'; break;
	case KEYC_F34:	        number=57397; final='u'; break;
	case KEYC_F35:	        number=57398; final='u'; break;
	case KEYC_KP_ZERO:	    number=57399; final='u'; break;
	case KEYC_KP_ONE:	    number=57400; final='u'; break;
	case KEYC_KP_TWO:	    number=57401; final='u'; break;
	case KEYC_KP_THREE:	    number=57402; final='u'; break;
	case KEYC_KP_FOUR:	    number=57403; final='u'; break;
	case KEYC_KP_FIVE:	    number=57404; final='u'; break;
	case KEYC_KP_SIX:	    number=57405; final='u'; break;
	case KEYC_KP_SEVEN:	    number=57406; final='u'; break;
	case KEYC_KP_EIGHT:	    number=57407; final='u'; break;
	case KEYC_KP_NINE:	    number=57408; final='u'; break;
	case KEYC_KP_PERIOD:	number=57409; final='u'; break;
	case KEYC_KP_SLASH:	    number=57410; final='u'; break;
	case KEYC_KP_STAR:	    number=57411; final='u'; break;
	case KEYC_KP_MINUS:	    number=57412; final='u'; break;
	case KEYC_KP_PLUS:	    number=57413; final='u'; break;
	case KEYC_KP_ENTER:	    number=57414; final='u'; break;
	case KEYC_KP_EQUAL:	    number=57415; final='u'; break;
	case KEYC_KP_SEPARATOR:	number=57416; final='u'; break;
	case KEYC_KP_LEFT:	    number=57417; final='u'; break;
	case KEYC_KP_RIGHT:	    number=57418; final='u'; break;
	case KEYC_KP_UP:	    number=57419; final='u'; break;
	case KEYC_KP_DOWN:	    number=57420; final='u'; break;
	case KEYC_KP_PAGE_UP:	number=57421; final='u'; break;
	case KEYC_KP_PAGE_DOWN:	number=57422; final='u'; break;
	case KEYC_KP_HOME:	    number=57423; final='u'; break;
	case KEYC_KP_END:	    number=57424; final='u'; break;
	case KEYC_KP_INSERT:	number=57425; final='u'; break;
	case KEYC_KP_DELETE:	number=57426; final='u'; break;
    case KEYC_MEDIA_PLAY:		number = 57428; final='u';break;
    case KEYC_MEDIA_PAUSE:		number = 57429; final='u';break;
    case KEYC_MEDIA_PLAY_PAUSE:	number = 57430; final='u';break;
    case KEYC_MEDIA_REVERSE:	number = 57431; final='u';break;
    case KEYC_MEDIA_STOP:		number = 57432; final='u';break;
    case KEYC_MEDIA_FAST_FORWARD:number = 57433; final='u';break;
    case KEYC_MEDIA_REWIND:		number = 57434; final='u';break;
    case KEYC_MEDIA_NEXT:		number = 57435; final='u';break;
    case KEYC_MEDIA_PREVIOUS:	number = 57436; final='u';break;
    case KEYC_MEDIA_RECORD:		number = 57437; final='u';break;
    case KEYC_VOLUME_DOWN:		number = 57438; final='u';break;
    case KEYC_VOLUME_UP:		number = 57439; final='u';break;
    case KEYC_VOLUME_MUTE:		number = 57440; final='u';break;
    case KEYC_SHIFT_L:			number = 57441; final='u';break;
    case KEYC_CONTROL_L:		number = 57442; final='u';break;
    case KEYC_ALT_L:			number = 57443; final='u';break;
    case KEYC_SUPER_L:			number = 57444; final='u';break;
    case KEYC_HYPER_L:			number = 57445; final='u';break;
    case KEYC_META_L:			number = 57446; final='u';break;
    case KEYC_SHIFT_R:			number = 57447; final='u';break;
    case KEYC_CONTROL_R:		number = 57448; final='u';break;
    case KEYC_ALT_R:			number = 57449; final='u';break;
    case KEYC_SUPER_R:			number = 57450; final='u';break;
    case KEYC_HYPER_R:			number = 57451; final='u';break;
    case KEYC_META_R:			number = 57452; final='u';break;
    case KEYC_ISO_LEVEL3_SHIFT:	number = 57453; final='u';break;
    case KEYC_ISO_LEVEL5_SHIFT:	number = 57454; final='u';break;
	default:
		number=onlykey;
		final='u';
	}
    if (final == 'u' || final == '~') {
		/* CSI number ; modifiers ~ */
		/* CSI number ; modifiers u */
		if(modifier==1){
			xsnprintf(tmp, sizeof tmp, "\033[%u%c",number,final);
		}else{
			xsnprintf(tmp, sizeof tmp, "\033[%u;%u%c",number,modifier,final);
		}
		input_key_write(__func__, bev, tmp, strlen(tmp));
		return 0;
	}else{
		/* CSI 1; modifiers [ABCDEFHPQS] */
		/* CSI [ABCDEFHPQS] */
		if(modifier==1){
			xsnprintf(tmp, sizeof tmp, "\033[%c",final);
		}else{
			xsnprintf(tmp, sizeof tmp, "\033[%u;%u%c",number,modifier,final);
		}
		input_key_write(__func__, bev, tmp, strlen(tmp));
		return 0;
	}
	return -1;
}


/* Translate a key code into an output key sequence. */
int
input_key(struct screen *s, struct bufferevent *bev, key_code key)
{
	struct input_key_entry	*ike = NULL;
	key_code		 justkey, newkey, outkey, modifiers;
	struct utf8_data	 ud;
	char			 tmp[64], modifier;
	enum kitty_kbd_flags kitty_flags ;

	/* Mouse keys need a pane. */
	if (KEYC_IS_MOUSE(key))
		return (0);
    kitty_flags = s->kitty_kbd.flags[s->kitty_kbd.idx];
	if (kitty_flags){
		if(input_key_kitty(s,bev,key) == 0)
			return (0);
	}
	/* legacy encoding key events */
	/* treat these modifers as alt */
	if(key & (KEYC_SUPER|KEYC_HYPER|KEYC_REAL_META))
		/* treat super/hyper/meta as alt */
		key |= KEYC_META|KEYC_IMPLIED_META;
	key &= ~(KEYC_SUPER|KEYC_HYPER|KEYC_REAL_META);
	log_debug("legacy key 0x%llx (%s) to %%", key,
			  key_string_lookup_key(key, 1) );

	/* Literal keys go as themselves (can't be more than eight bits). */
	if (key & KEYC_LITERAL) {
		ud.data[0] = (u_char)key;
		input_key_write(__func__, bev, &ud.data[0], 1);
		return (0);
	}

	/* Is this backspace? */
	if ((key & KEYC_MASK_KEY) == KEYC_BSPACE) {
		newkey = options_get_number(global_options, "backspace");
		if (newkey >= 0x7f)
			newkey = '\177';
		key = newkey|(key & (KEYC_MASK_MODIFIERS|KEYC_MASK_FLAGS));
	}

	/*
	 * If this is a normal 7-bit key, just send it, with a leading escape
	 * if necessary. If it is a UTF-8 key, split it and send it.
	 */
	justkey = (key & ~(KEYC_META|KEYC_IMPLIED_META));
	if (justkey <= 0x7f) {
		if (key & KEYC_META)
			input_key_write(__func__, bev, "\033", 1);
		ud.data[0] = justkey;
		input_key_write(__func__, bev, &ud.data[0], 1);
		return (0);
	}
	if (KEYC_IS_UNICODE(justkey)) {
		if (key & KEYC_META)
			input_key_write(__func__, bev, "\033", 1);
		utf8_to_data(justkey, &ud);
		input_key_write(__func__, bev, ud.data, ud.size);
		return (0);
	}

	/*
	 * Look up in the tree. If not in application keypad or cursor mode,
	 * remove the flags from the key.
	 */
	if (~s->mode & MODE_KKEYPAD)
		key &= ~KEYC_KEYPAD;
	if (~s->mode & MODE_KCURSOR)
		key &= ~KEYC_CURSOR;
	if (s->mode & MODE_KEXTENDED)
		ike = input_key_get(key|KEYC_EXTENDED);
	if (ike == NULL)
		ike = input_key_get(key);
	if (ike == NULL && (key & KEYC_META) && (~key & KEYC_IMPLIED_META))
		ike = input_key_get(key & ~KEYC_META);
	if (ike == NULL && (key & KEYC_CURSOR))
		ike = input_key_get(key & ~KEYC_CURSOR);
	if (ike == NULL && (key & KEYC_KEYPAD))
		ike = input_key_get(key & ~KEYC_KEYPAD);
	if (ike == NULL && (key & KEYC_EXTENDED))
		ike = input_key_get(key & ~KEYC_EXTENDED);
	if (ike != NULL) {
		log_debug("found key 0x%llx: \"%s\"", key, ike->data);
		if ((key == KEYC_PASTE_START || key == KEYC_PASTE_END) &&
		    (~s->mode & MODE_BRACKETPASTE))
			return (0);
		if ((key & KEYC_META) && (~key & KEYC_IMPLIED_META))
			input_key_write(__func__, bev, "\033", 1);
		input_key_write(__func__, bev, ike->data, strlen(ike->data));
		return (0);
	}

	/* No builtin key sequence; construct an extended key sequence. */
	if (~s->mode & MODE_KEXTENDED) {
		if ((key & KEYC_MASK_MODIFIERS) != KEYC_CTRL)
			goto missing;
		justkey = (key & KEYC_MASK_KEY);
		switch (justkey) {
		case ' ':
		case '2':
			key = 0|(key & ~KEYC_MASK_KEY);
			break;
		case '|':
			key = 28|(key & ~KEYC_MASK_KEY);
			break;
		case '6':
			key = 30|(key & ~KEYC_MASK_KEY);
			break;
		case '-':
		case '/':
			key = 31|(key & ~KEYC_MASK_KEY);
			break;
		case '?':
			key = 127|(key & ~KEYC_MASK_KEY);
			break;
		default:
			if (justkey >= 'A' && justkey <= '_')
				key = (justkey - 'A')|(key & ~KEYC_MASK_KEY);
			else if (justkey >= 'a' && justkey <= '~')
				key = (justkey - 96)|(key & ~KEYC_MASK_KEY);
			else
				return (0);
			break;
		}
		return (input_key(s, bev, key & ~KEYC_CTRL));
	}
	outkey = (key & KEYC_MASK_KEY);
	modifiers = (key & KEYC_MASK_MODIFIERS);
	if (outkey < 32 && outkey != 9 && outkey != 13 && outkey != 27) {
		outkey = 64 + outkey;
		modifiers |= KEYC_CTRL;
	}
	modifier=get_legacy_modifier(key);
	if(modifier==1)
		goto missing;
	xsnprintf(tmp, sizeof tmp, "\033[%llu;%uu", outkey, modifier);
	input_key_write(__func__, bev, tmp, strlen(tmp));
	return (0);

missing:
	log_debug("key 0x%llx missing", key);
	return (-1);
}

/* Get mouse event string. */
int
input_key_get_mouse(struct screen *s, struct mouse_event *m, u_int x, u_int y,
    const char **rbuf, size_t *rlen)
{
	static char	 buf[40];
	size_t		 len;

	*rbuf = NULL;
	*rlen = 0;

	/* If this pane is not in button or all mode, discard motion events. */
	if (MOUSE_DRAG(m->b) && (s->mode & MOTION_MOUSE_MODES) == 0)
		return (0);
	if ((s->mode & ALL_MOUSE_MODES) == 0)
		return (0);

	/*
	 * If this event is a release event and not in all mode, discard it.
	 * In SGR mode we can tell absolutely because a release is normally
	 * shown by the last character. Without SGR, we check if the last
	 * buttons was also a release.
	 */
	if (m->sgr_type != ' ') {
		if (MOUSE_DRAG(m->sgr_b) &&
		    MOUSE_RELEASE(m->sgr_b) &&
		    (~s->mode & MODE_MOUSE_ALL))
			return (0);
	} else {
		if (MOUSE_DRAG(m->b) &&
		    MOUSE_RELEASE(m->b) &&
		    MOUSE_RELEASE(m->lb) &&
		    (~s->mode & MODE_MOUSE_ALL))
			return (0);
	}

	/*
	 * Use the SGR (1006) extension only if the application requested it
	 * and the underlying terminal also sent the event in this format (this
	 * is because an old style mouse release event cannot be converted into
	 * the new SGR format, since the released button is unknown). Otherwise
	 * pretend that tmux doesn't speak this extension, and fall back to the
	 * UTF-8 (1005) extension if the application requested, or to the
	 * legacy format.
	 */
	if (m->sgr_type != ' ' && (s->mode & MODE_MOUSE_SGR)) {
		len = xsnprintf(buf, sizeof buf, "\033[<%u;%u;%u%c",
		    m->sgr_b, x + 1, y + 1, m->sgr_type);
	} else if (s->mode & MODE_MOUSE_UTF8) {
		if (m->b > MOUSE_PARAM_UTF8_MAX - MOUSE_PARAM_BTN_OFF ||
		    x > MOUSE_PARAM_UTF8_MAX - MOUSE_PARAM_POS_OFF ||
		    y > MOUSE_PARAM_UTF8_MAX - MOUSE_PARAM_POS_OFF)
			return (0);
		len = xsnprintf(buf, sizeof buf, "\033[M");
		len += input_key_split2(m->b + MOUSE_PARAM_BTN_OFF, &buf[len]);
		len += input_key_split2(x + MOUSE_PARAM_POS_OFF, &buf[len]);
		len += input_key_split2(y + MOUSE_PARAM_POS_OFF, &buf[len]);
	} else {
		if (m->b + MOUSE_PARAM_BTN_OFF > MOUSE_PARAM_MAX)
			return (0);

		len = xsnprintf(buf, sizeof buf, "\033[M");
		buf[len++] = m->b + MOUSE_PARAM_BTN_OFF;

		/*
		 * The incoming x and y may be out of the range which can be
		 * supported by the "normal" mouse protocol. Clamp the
		 * coordinates to the supported range.
		 */
		if (x + MOUSE_PARAM_POS_OFF > MOUSE_PARAM_MAX)
			buf[len++] = MOUSE_PARAM_MAX;
		else
			buf[len++] = x + MOUSE_PARAM_POS_OFF;
		if (y + MOUSE_PARAM_POS_OFF > MOUSE_PARAM_MAX)
			buf[len++] = MOUSE_PARAM_MAX;
		else
			buf[len++] = y + MOUSE_PARAM_POS_OFF;
	}

	*rbuf = buf;
	*rlen = len;
	return (1);
}

/* Translate mouse and output. */
static void
input_key_mouse(struct window_pane *wp, struct mouse_event *m)
{
	struct screen	*s = wp->screen;
	u_int		 x, y;
	const char	*buf;
	size_t		 len;

	/* Ignore events if no mouse mode or the pane is not visible. */
	if (m->ignore || (s->mode & ALL_MOUSE_MODES) == 0)
		return;
	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return;
	if (!window_pane_visible(wp))
		return;
	if (!input_key_get_mouse(s, m, x, y, &buf, &len))
		return;
	log_debug("writing mouse %.*s to %%%u", (int)len, buf, wp->id);
	input_key_write(__func__, wp->event, buf, len);
}
