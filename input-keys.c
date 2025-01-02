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

/*
 * Encode and write an extended key escape sequence in one of the two
 * possible formats, depending on the configured output mode.
 */
static int
input_key_extended(struct bufferevent *bev, key_code key)
{
	char		 tmp[64], modifier;
	struct utf8_data ud;
	wchar_t		 wc;

	switch (key & KEYC_MASK_MODIFIERS) {
	case KEYC_SHIFT:
		modifier = '2';
		break;
	case KEYC_META:
		modifier = '3';
		break;
	case KEYC_SHIFT|KEYC_META:
		modifier = '4';
		break;
	case KEYC_CTRL:
		modifier = '5';
		break;
	case KEYC_SHIFT|KEYC_CTRL:
		modifier = '6';
		break;
	case KEYC_META|KEYC_CTRL:
		modifier = '7';
		break;
	case KEYC_SHIFT|KEYC_META|KEYC_CTRL:
		modifier = '8';
		break;
	default:
		return (-1);
	}

	if (KEYC_IS_UNICODE(key)) {
		utf8_to_data(key & KEYC_MASK_KEY, &ud);
		if (utf8_towc(&ud, &wc) == UTF8_DONE)
			key = wc;
		else
			return (-1);
	} else
		key &= KEYC_MASK_KEY;

	if (options_get_number(global_options, "extended-keys-format") == 1)
		xsnprintf(tmp, sizeof tmp, "\033[27;%c;%llu~", modifier, key);
	else
		xsnprintf(tmp, sizeof tmp, "\033[%llu;%cu", key, modifier);

	input_key_write(__func__, bev, tmp, strlen(tmp));
	return (0);
}

/*
 * Outputs the key in the "standard" mode. This is by far the most
 * complicated output mode, with a lot of remapping in order to
 * emulate quirks of terminals that today can be only found in museums.
 */
static int
input_key_vt10x(struct bufferevent *bev, key_code key)
{
	struct utf8_data	 ud;
	key_code		 onlykey;
	char			*p;
	static const char	*standard_map[2] = {
		"1!9(0)=+;:'\",<.>/-8? 2",
		"119900=+;;'',,..\x1f\x1f\x7f\x7f\0\0",
	};

	log_debug("%s: key in %llx", __func__, key);

	if (key & KEYC_META)
		input_key_write(__func__, bev, "\033", 1);

	/*
	 * There's no way to report modifiers for unicode keys in standard mode
	 * so lose the modifiers.
	 */
	if (KEYC_IS_UNICODE(key)) {
		utf8_to_data(key, &ud);
                input_key_write(__func__, bev, ud.data, ud.size);
		return (0);
	}

	/*
	 * Prevent TAB, CR and LF from being swallowed by the C0 remapping
	 * logic.
	 */
	onlykey = key & KEYC_MASK_KEY;
	if (onlykey == '\r' || onlykey == '\n' || onlykey == '\t')
		key &= ~KEYC_CTRL;

	/*
	 * Convert keys with Ctrl modifier into corresponding C0 control codes,
	 * with the exception of *some* keys, which are remapped into printable
	 * ASCII characters.
	 *
	 * There is no special handling for Shift modifier, which is pretty
	 * much redundant anyway, as no terminal will send <base key>|SHIFT,
	 * but only <shifted key>|SHIFT.
	 */
	if (key & KEYC_CTRL) {
		p = strchr(standard_map[0], onlykey);
		if (p != NULL)
			key = standard_map[1][p - standard_map[0]];
		else if (onlykey >= '3' && onlykey <= '7')
			key = onlykey - '\030';
		else if (onlykey >= '@' && onlykey <= '~')
			key = onlykey & 0x1f;
		else
			return (-1);
	}

	log_debug("%s: key out %llx", __func__, key);

	ud.data[0] = key & 0x7f;
	input_key_write(__func__, bev, &ud.data[0], 1);
	return (0);
}

/* Pick keys that are reported as vt10x keys in modifyOtherKeys=1 mode. */
static int
input_key_mode1(struct bufferevent *bev, key_code key)
{
	key_code	 onlykey;

	log_debug("%s: key in %llx", __func__, key);

	/* A regular or shifted key + Meta. */
	if ((key & (KEYC_CTRL | KEYC_META)) == KEYC_META)
		return (input_key_vt10x(bev, key));

	/*
	 * As per
	 * https://invisible-island.net/xterm/modified-keys-us-pc105.html.
	 */
	onlykey = key & KEYC_MASK_KEY;
	if ((key & KEYC_CTRL) &&
	    (onlykey == ' ' ||
	     onlykey == '/' ||
	     onlykey == '@' ||
	     onlykey == '^' ||
	     (onlykey >= '2' && onlykey <= '8') ||
	     (onlykey >= '@' && onlykey <= '~')))
		return (input_key_vt10x(bev, key));

	return (-1);
}

/* Translate a key code into an output key sequence. */
int
input_key(struct screen *s, struct bufferevent *bev, key_code key)
{
	struct input_key_entry	*ike = NULL;
	key_code		 newkey;
	struct utf8_data	 ud;

	/* Mouse keys need a pane. */
	if (KEYC_IS_MOUSE(key))
		return (0);

	/* Literal keys go as themselves (can't be more than eight bits). */
	if (key & KEYC_LITERAL) {
		ud.data[0] = (u_char)key;
		input_key_write(__func__, bev, &ud.data[0], 1);
		return (0);
	}

	/* Is this backspace? */
	if ((key & KEYC_MASK_KEY) == KEYC_BSPACE) {
		newkey = options_get_number(global_options, "backspace");
		log_debug("%s: key 0x%llx is backspace -> 0x%llx", __func__,
		    key, newkey);
		if ((key & KEYC_MASK_MODIFIERS) == 0) {
			ud.data[0] = 255;
			if ((newkey & KEYC_MASK_MODIFIERS) == 0)
				ud.data[0] = newkey;
			else if ((newkey & KEYC_MASK_MODIFIERS) == KEYC_CTRL) {
				newkey &= KEYC_MASK_KEY;
				if (newkey >= 'A' && newkey <= 'Z')
					ud.data[0] = newkey - 0x40;
				else if (newkey >= 'a' && newkey <= 'z')
					ud.data[0] = newkey - 0x60;
			}
			if (ud.data[0] != 255)
				input_key_write(__func__, bev, &ud.data[0], 1);
			return (0);
		}
		key = newkey|(key & (KEYC_MASK_FLAGS|KEYC_MASK_MODIFIERS));
	}

	/* Is this backtab? */
	if ((key & KEYC_MASK_KEY) == KEYC_BTAB) {
		if (s->mode & MODE_KEYS_EXTENDED_2) {
			/* When in xterm extended mode, remap into S-Tab. */
			key = '\011' | (key & ~KEYC_MASK_KEY) | KEYC_SHIFT;
		} else {
			/* Otherwise clear modifiers. */
			key &= ~KEYC_MASK_MODIFIERS;
		}
	}

	/*
	 * A trivial case, that is a 7-bit key, excluding C0 control characters
	 * that can't be entered from the keyboard, and no modifiers; or a UTF-8
	 * key and no modifiers.
	 */
	if (!(key & ~KEYC_MASK_KEY)) {
		if (key == C0_HT ||
		    key == C0_CR ||
		    key == C0_ESC ||
		    (key >= 0x20 && key <= 0x7f)) {
			ud.data[0] = key;
			input_key_write(__func__, bev, &ud.data[0], 1);
			return (0);
		}
		if (KEYC_IS_UNICODE(key)) {
			utf8_to_data(key, &ud);
			input_key_write(__func__, bev, ud.data, ud.size);
			return (0);
		}
	}

	/*
	 * Look up the standard VT10x keys in the tree. If not in application
	 * keypad or cursor mode, remove the respective flags from the key.
	 */
	if (~s->mode & MODE_KKEYPAD)
		key &= ~KEYC_KEYPAD;
	if (~s->mode & MODE_KCURSOR)
		key &= ~KEYC_CURSOR;
	if (ike == NULL)
		ike = input_key_get(key);
	if (ike == NULL && (key & KEYC_META) && (~key & KEYC_IMPLIED_META))
		ike = input_key_get(key & ~KEYC_META);
	if (ike == NULL && (key & KEYC_CURSOR))
		ike = input_key_get(key & ~KEYC_CURSOR);
	if (ike == NULL && (key & KEYC_KEYPAD))
		ike = input_key_get(key & ~KEYC_KEYPAD);
	if (ike != NULL) {
		log_debug("%s: found key 0x%llx: \"%s\"", __func__, key,
		    ike->data);
		if (KEYC_IS_PASTE(key) && (~s->mode & MODE_BRACKETPASTE))
			return (0);
		if ((key & KEYC_META) && (~key & KEYC_IMPLIED_META))
			input_key_write(__func__, bev, "\033", 1);
		input_key_write(__func__, bev, ike->data, strlen(ike->data));
		return (0);
	}

	/* Ignore internal function key codes. */
	if ((key >= KEYC_BASE && key < KEYC_BASE_END) ||
	    (key >= KEYC_USER && key < KEYC_USER_END)) {
		log_debug("%s: ignoring key 0x%llx", __func__, key);
		return (0);
	}

	/*
	 * No builtin key sequence; construct an extended key sequence
	 * depending on the client mode.
	 *
	 * If something invalid reaches here, an invalid output may be
	 * produced. For example Ctrl-Shift-2 is invalid (as there's
	 * no way to enter it). The correct form is Ctrl-Shift-@, at
	 * least in US English keyboard layout.
	 */
	switch (s->mode & EXTENDED_KEY_MODES) {
	case MODE_KEYS_EXTENDED_2:
		/*
		 * The simplest mode to handle - *all* modified keys are
		 * reported in the extended form.
		 */
		return (input_key_extended(bev, key));
        case MODE_KEYS_EXTENDED:
		/*
		 * Some keys are still reported in standard mode, to maintain
		 * compatibility with applications unaware of extended keys.
		 */
		if (input_key_mode1(bev, key) == -1)
			return (input_key_extended(bev, key));
		return (0);
	default:
		/* The standard mode. */
		return (input_key_vt10x(bev, key));
	}
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
