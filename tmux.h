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

#ifndef TMUX_H
#define TMUX_H

#include <sys/time.h>
#include <sys/uio.h>

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <termios.h>

#ifdef HAVE_UTEMPTER
#include <utempter.h>
#endif

#include "compat.h"
#include "tmux-protocol.h"
#include "xmalloc.h"

extern char   **environ;

struct args;
struct args_command_state;
struct client;
struct cmd;
struct cmd_find_state;
struct cmdq_item;
struct cmdq_list;
struct cmdq_state;
struct cmds;
struct control_state;
struct environ;
struct format_job_tree;
struct format_tree;
struct hyperlinks_uri;
struct hyperlinks;
struct input_ctx;
struct job;
struct menu_data;
struct mode_tree_data;
struct mouse_event;
struct options;
struct options_array_item;
struct options_entry;
struct screen_write_citem;
struct screen_write_cline;
struct screen_write_ctx;
struct session;

#ifdef ENABLE_SIXEL
struct sixel_image;
#endif

struct tty_ctx;
struct tty_code;
struct tty_key;
struct tmuxpeer;
struct tmuxproc;
struct winlink;

/* Default configuration files and socket paths. */
#ifndef TMUX_CONF
#define TMUX_CONF "/etc/tmux.conf:~/.tmux.conf"
#endif
#ifndef TMUX_SOCK
#define TMUX_SOCK "$TMUX_TMPDIR:" _PATH_TMP
#endif
#ifndef TMUX_TERM
#define TMUX_TERM "screen"
#endif
#ifndef TMUX_LOCK_CMD
#define TMUX_LOCK_CMD "lock -np"
#endif

/* Minimum layout cell size, NOT including border lines. */
#define PANE_MINIMUM 1

/* Minimum and maximum window size. */
#define WINDOW_MINIMUM PANE_MINIMUM
#define WINDOW_MAXIMUM 10000

/* Automatic name refresh interval, in microseconds. Must be < 1 second. */
#define NAME_INTERVAL 500000

/* Default pixel cell sizes. */
#define DEFAULT_XPIXEL 16
#define DEFAULT_YPIXEL 32

/* Attribute to make GCC check printf-like arguments. */
#define printflike(a, b) __attribute__ ((format (printf, a, b)))

/* Number of items in array. */
#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

/* Alert option values. */
#define ALERT_NONE 0
#define ALERT_ANY 1
#define ALERT_CURRENT 2
#define ALERT_OTHER 3

/* Visual option values. */
#define VISUAL_OFF 0
#define VISUAL_ON 1
#define VISUAL_BOTH 2

/* No key or unknown key. */
#define KEYC_NONE            0x000ff000000000ULL
#define KEYC_UNKNOWN         0x000fe000000000ULL

/*
 * Base for special (that is, not Unicode) keys. An enum must be at most a
 * signed int, so these are based in the highest Unicode PUA.
 */
#define KEYC_BASE            0x0000000010e000ULL
#define KEYC_USER            0x0000000010f000ULL

/* Key modifier bits. */
#define KEYC_META            0x00100000000000ULL
#define KEYC_CTRL            0x00200000000000ULL
#define KEYC_SHIFT           0x00400000000000ULL

/* Key flag bits. */
#define KEYC_LITERAL	     0x01000000000000ULL
#define KEYC_KEYPAD	     0x02000000000000ULL
#define KEYC_CURSOR	     0x04000000000000ULL
#define KEYC_IMPLIED_META    0x08000000000000ULL
#define KEYC_BUILD_MODIFIERS 0x10000000000000ULL
#define KEYC_VI		     0x20000000000000ULL
#define KEYC_EXTENDED	     0x40000000000000ULL
#define KEYC_SENT	     0x80000000000000ULL

/* Masks for key bits. */
#define KEYC_MASK_MODIFIERS  0x00f00000000000ULL
#define KEYC_MASK_FLAGS      0xff000000000000ULL
#define KEYC_MASK_KEY        0x000fffffffffffULL

/* Available user keys. */
#define KEYC_NUSER 1000

/* Is this a mouse key? */
#define KEYC_IS_MOUSE(key) \
	(((key) & KEYC_MASK_KEY) >= KEYC_MOUSE && \
	 ((key) & KEYC_MASK_KEY) < KEYC_BSPACE)

/* Is this a Unicode key? */
#define KEYC_IS_UNICODE(key) \
	(((key) & KEYC_MASK_KEY) > 0x7f && \
	 (((key) & KEYC_MASK_KEY) < KEYC_BASE || \
	  ((key) & KEYC_MASK_KEY) >= KEYC_BASE_END) && \
	 (((key) & KEYC_MASK_KEY) < KEYC_USER || \
	  ((key) & KEYC_MASK_KEY) >= KEYC_USER + KEYC_NUSER))

/* Multiple click timeout. */
#define KEYC_CLICK_TIMEOUT 300

/* Mouse key codes. */
#define KEYC_MOUSE_KEY(name)					\
	KEYC_ ## name ## _PANE,					\
	KEYC_ ## name ## _STATUS,				\
	KEYC_ ## name ## _STATUS_LEFT,				\
	KEYC_ ## name ## _STATUS_RIGHT,				\
	KEYC_ ## name ## _STATUS_DEFAULT,			\
	KEYC_ ## name ## _BORDER
#define KEYC_MOUSE_STRING(name, s)				\
	{ #s "Pane", KEYC_ ## name ## _PANE },			\
	{ #s "Status", KEYC_ ## name ## _STATUS },		\
	{ #s "StatusLeft", KEYC_ ## name ## _STATUS_LEFT },	\
	{ #s "StatusRight", KEYC_ ## name ## _STATUS_RIGHT },	\
	{ #s "StatusDefault", KEYC_ ## name ## _STATUS_DEFAULT }, \
	{ #s "Border", KEYC_ ## name ## _BORDER }

/*
 * A single key. This can be ASCII or Unicode or one of the keys between
 * KEYC_BASE and KEYC_BASE_END.
 */
typedef unsigned long long key_code;

/* Special key codes. */
enum {
	/* Focus events. */
	KEYC_FOCUS_IN = KEYC_BASE,
	KEYC_FOCUS_OUT,

	/* "Any" key, used if not found in key table. */
	KEYC_ANY,

	/* Paste brackets. */
	KEYC_PASTE_START,
	KEYC_PASTE_END,

	/* Mouse keys. */
	KEYC_MOUSE, /* unclassified mouse event */
	KEYC_DRAGGING, /* dragging in progress */
	KEYC_DOUBLECLICK, /* double click complete */
	KEYC_MOUSE_KEY(MOUSEMOVE),
	KEYC_MOUSE_KEY(MOUSEDOWN1),
	KEYC_MOUSE_KEY(MOUSEDOWN2),
	KEYC_MOUSE_KEY(MOUSEDOWN3),
	KEYC_MOUSE_KEY(MOUSEDOWN6),
	KEYC_MOUSE_KEY(MOUSEDOWN7),
	KEYC_MOUSE_KEY(MOUSEDOWN8),
	KEYC_MOUSE_KEY(MOUSEDOWN9),
	KEYC_MOUSE_KEY(MOUSEDOWN10),
	KEYC_MOUSE_KEY(MOUSEDOWN11),
	KEYC_MOUSE_KEY(MOUSEUP1),
	KEYC_MOUSE_KEY(MOUSEUP2),
	KEYC_MOUSE_KEY(MOUSEUP3),
	KEYC_MOUSE_KEY(MOUSEUP6),
	KEYC_MOUSE_KEY(MOUSEUP7),
	KEYC_MOUSE_KEY(MOUSEUP8),
	KEYC_MOUSE_KEY(MOUSEUP9),
	KEYC_MOUSE_KEY(MOUSEUP10),
	KEYC_MOUSE_KEY(MOUSEUP11),
	KEYC_MOUSE_KEY(MOUSEDRAG1),
	KEYC_MOUSE_KEY(MOUSEDRAG2),
	KEYC_MOUSE_KEY(MOUSEDRAG3),
	KEYC_MOUSE_KEY(MOUSEDRAG6),
	KEYC_MOUSE_KEY(MOUSEDRAG7),
	KEYC_MOUSE_KEY(MOUSEDRAG8),
	KEYC_MOUSE_KEY(MOUSEDRAG9),
	KEYC_MOUSE_KEY(MOUSEDRAG10),
	KEYC_MOUSE_KEY(MOUSEDRAG11),
	KEYC_MOUSE_KEY(MOUSEDRAGEND1),
	KEYC_MOUSE_KEY(MOUSEDRAGEND2),
	KEYC_MOUSE_KEY(MOUSEDRAGEND3),
	KEYC_MOUSE_KEY(MOUSEDRAGEND6),
	KEYC_MOUSE_KEY(MOUSEDRAGEND7),
	KEYC_MOUSE_KEY(MOUSEDRAGEND8),
	KEYC_MOUSE_KEY(MOUSEDRAGEND9),
	KEYC_MOUSE_KEY(MOUSEDRAGEND10),
	KEYC_MOUSE_KEY(MOUSEDRAGEND11),
	KEYC_MOUSE_KEY(WHEELUP),
	KEYC_MOUSE_KEY(WHEELDOWN),
	KEYC_MOUSE_KEY(SECONDCLICK1),
	KEYC_MOUSE_KEY(SECONDCLICK2),
	KEYC_MOUSE_KEY(SECONDCLICK3),
	KEYC_MOUSE_KEY(SECONDCLICK6),
	KEYC_MOUSE_KEY(SECONDCLICK7),
	KEYC_MOUSE_KEY(SECONDCLICK8),
	KEYC_MOUSE_KEY(SECONDCLICK9),
	KEYC_MOUSE_KEY(SECONDCLICK10),
	KEYC_MOUSE_KEY(SECONDCLICK11),
	KEYC_MOUSE_KEY(DOUBLECLICK1),
	KEYC_MOUSE_KEY(DOUBLECLICK2),
	KEYC_MOUSE_KEY(DOUBLECLICK3),
	KEYC_MOUSE_KEY(DOUBLECLICK6),
	KEYC_MOUSE_KEY(DOUBLECLICK7),
	KEYC_MOUSE_KEY(DOUBLECLICK8),
	KEYC_MOUSE_KEY(DOUBLECLICK9),
	KEYC_MOUSE_KEY(DOUBLECLICK10),
	KEYC_MOUSE_KEY(DOUBLECLICK11),
	KEYC_MOUSE_KEY(TRIPLECLICK1),
	KEYC_MOUSE_KEY(TRIPLECLICK2),
	KEYC_MOUSE_KEY(TRIPLECLICK3),
	KEYC_MOUSE_KEY(TRIPLECLICK6),
	KEYC_MOUSE_KEY(TRIPLECLICK7),
	KEYC_MOUSE_KEY(TRIPLECLICK8),
	KEYC_MOUSE_KEY(TRIPLECLICK9),
	KEYC_MOUSE_KEY(TRIPLECLICK10),
	KEYC_MOUSE_KEY(TRIPLECLICK11),

	/* Backspace key. */
	KEYC_BSPACE,

	/* Function keys. */
	KEYC_F1,
	KEYC_F2,
	KEYC_F3,
	KEYC_F4,
	KEYC_F5,
	KEYC_F6,
	KEYC_F7,
	KEYC_F8,
	KEYC_F9,
	KEYC_F10,
	KEYC_F11,
	KEYC_F12,
	KEYC_IC,
	KEYC_DC,
	KEYC_HOME,
	KEYC_END,
	KEYC_NPAGE,
	KEYC_PPAGE,
	KEYC_BTAB,

	/* Arrow keys. */
	KEYC_UP,
	KEYC_DOWN,
	KEYC_LEFT,
	KEYC_RIGHT,

	/* Numeric keypad. */
	KEYC_KP_SLASH,
	KEYC_KP_STAR,
	KEYC_KP_MINUS,
	KEYC_KP_SEVEN,
	KEYC_KP_EIGHT,
	KEYC_KP_NINE,
	KEYC_KP_PLUS,
	KEYC_KP_FOUR,
	KEYC_KP_FIVE,
	KEYC_KP_SIX,
	KEYC_KP_ONE,
	KEYC_KP_TWO,
	KEYC_KP_THREE,
	KEYC_KP_ENTER,
	KEYC_KP_ZERO,
	KEYC_KP_PERIOD,

	/* End of special keys. */
	KEYC_BASE_END
};

/* Termcap codes. */
enum tty_code_code {
	TTYC_ACSC,
	TTYC_AM,
	TTYC_AX,
	TTYC_BCE,
	TTYC_BEL,
	TTYC_BIDI,
	TTYC_BLINK,
	TTYC_BOLD,
	TTYC_CIVIS,
	TTYC_CLEAR,
	TTYC_CLMG,
	TTYC_CMG,
	TTYC_CNORM,
	TTYC_COLORS,
	TTYC_CR,
	TTYC_CS,
	TTYC_CSR,
	TTYC_CUB,
	TTYC_CUB1,
	TTYC_CUD,
	TTYC_CUD1,
	TTYC_CUF,
	TTYC_CUF1,
	TTYC_CUP,
	TTYC_CUU,
	TTYC_CUU1,
	TTYC_CVVIS,
	TTYC_DCH,
	TTYC_DCH1,
	TTYC_DIM,
	TTYC_DL,
	TTYC_DL1,
	TTYC_DSBP,
	TTYC_DSEKS,
	TTYC_DSFCS,
	TTYC_DSMG,
	TTYC_E3,
	TTYC_ECH,
	TTYC_ED,
	TTYC_EL,
	TTYC_EL1,
	TTYC_ENACS,
	TTYC_ENBP,
	TTYC_ENEKS,
	TTYC_ENFCS,
	TTYC_ENMG,
	TTYC_FSL,
	TTYC_HLS,
	TTYC_HOME,
	TTYC_HPA,
	TTYC_ICH,
	TTYC_ICH1,
	TTYC_IL,
	TTYC_IL1,
	TTYC_INDN,
	TTYC_INVIS,
	TTYC_KCBT,
	TTYC_KCUB1,
	TTYC_KCUD1,
	TTYC_KCUF1,
	TTYC_KCUU1,
	TTYC_KDC2,
	TTYC_KDC3,
	TTYC_KDC4,
	TTYC_KDC5,
	TTYC_KDC6,
	TTYC_KDC7,
	TTYC_KDCH1,
	TTYC_KDN2,
	TTYC_KDN3,
	TTYC_KDN4,
	TTYC_KDN5,
	TTYC_KDN6,
	TTYC_KDN7,
	TTYC_KEND,
	TTYC_KEND2,
	TTYC_KEND3,
	TTYC_KEND4,
	TTYC_KEND5,
	TTYC_KEND6,
	TTYC_KEND7,
	TTYC_KF1,
	TTYC_KF10,
	TTYC_KF11,
	TTYC_KF12,
	TTYC_KF13,
	TTYC_KF14,
	TTYC_KF15,
	TTYC_KF16,
	TTYC_KF17,
	TTYC_KF18,
	TTYC_KF19,
	TTYC_KF2,
	TTYC_KF20,
	TTYC_KF21,
	TTYC_KF22,
	TTYC_KF23,
	TTYC_KF24,
	TTYC_KF25,
	TTYC_KF26,
	TTYC_KF27,
	TTYC_KF28,
	TTYC_KF29,
	TTYC_KF3,
	TTYC_KF30,
	TTYC_KF31,
	TTYC_KF32,
	TTYC_KF33,
	TTYC_KF34,
	TTYC_KF35,
	TTYC_KF36,
	TTYC_KF37,
	TTYC_KF38,
	TTYC_KF39,
	TTYC_KF4,
	TTYC_KF40,
	TTYC_KF41,
	TTYC_KF42,
	TTYC_KF43,
	TTYC_KF44,
	TTYC_KF45,
	TTYC_KF46,
	TTYC_KF47,
	TTYC_KF48,
	TTYC_KF49,
	TTYC_KF5,
	TTYC_KF50,
	TTYC_KF51,
	TTYC_KF52,
	TTYC_KF53,
	TTYC_KF54,
	TTYC_KF55,
	TTYC_KF56,
	TTYC_KF57,
	TTYC_KF58,
	TTYC_KF59,
	TTYC_KF6,
	TTYC_KF60,
	TTYC_KF61,
	TTYC_KF62,
	TTYC_KF63,
	TTYC_KF7,
	TTYC_KF8,
	TTYC_KF9,
	TTYC_KHOM2,
	TTYC_KHOM3,
	TTYC_KHOM4,
	TTYC_KHOM5,
	TTYC_KHOM6,
	TTYC_KHOM7,
	TTYC_KHOME,
	TTYC_KIC2,
	TTYC_KIC3,
	TTYC_KIC4,
	TTYC_KIC5,
	TTYC_KIC6,
	TTYC_KIC7,
	TTYC_KICH1,
	TTYC_KIND,
	TTYC_KLFT2,
	TTYC_KLFT3,
	TTYC_KLFT4,
	TTYC_KLFT5,
	TTYC_KLFT6,
	TTYC_KLFT7,
	TTYC_KMOUS,
	TTYC_KNP,
	TTYC_KNXT2,
	TTYC_KNXT3,
	TTYC_KNXT4,
	TTYC_KNXT5,
	TTYC_KNXT6,
	TTYC_KNXT7,
	TTYC_KPP,
	TTYC_KPRV2,
	TTYC_KPRV3,
	TTYC_KPRV4,
	TTYC_KPRV5,
	TTYC_KPRV6,
	TTYC_KPRV7,
	TTYC_KRI,
	TTYC_KRIT2,
	TTYC_KRIT3,
	TTYC_KRIT4,
	TTYC_KRIT5,
	TTYC_KRIT6,
	TTYC_KRIT7,
	TTYC_KUP2,
	TTYC_KUP3,
	TTYC_KUP4,
	TTYC_KUP5,
	TTYC_KUP6,
	TTYC_KUP7,
	TTYC_MS,
	TTYC_NOBR,
	TTYC_OL,
	TTYC_OP,
	TTYC_RECT,
	TTYC_REV,
	TTYC_RGB,
	TTYC_RI,
	TTYC_RIN,
	TTYC_RMACS,
	TTYC_RMCUP,
	TTYC_RMKX,
	TTYC_SE,
	TTYC_SETAB,
	TTYC_SETAF,
	TTYC_SETAL,
	TTYC_SETRGBB,
	TTYC_SETRGBF,
	TTYC_SETULC,
	TTYC_SGR0,
	TTYC_SITM,
	TTYC_SMACS,
	TTYC_SMCUP,
	TTYC_SMKX,
	TTYC_SMOL,
	TTYC_SMSO,
	TTYC_SMUL,
	TTYC_SMULX,
	TTYC_SMXX,
	TTYC_SXL,
	TTYC_SS,
	TTYC_SWD,
	TTYC_SYNC,
	TTYC_TC,
	TTYC_TSL,
	TTYC_U8,
	TTYC_VPA,
	TTYC_XT
};

/* Character classes. */
#define WHITESPACE " "

/* Mode keys. */
#define MODEKEY_EMACS 0
#define MODEKEY_VI 1

/* Modes. */
#define MODE_CURSOR 0x1
#define MODE_INSERT 0x2
#define MODE_KCURSOR 0x4
#define MODE_KKEYPAD 0x8
#define MODE_WRAP 0x10
#define MODE_MOUSE_STANDARD 0x20
#define MODE_MOUSE_BUTTON 0x40
#define MODE_CURSOR_BLINKING 0x80
#define MODE_MOUSE_UTF8 0x100
#define MODE_MOUSE_SGR 0x200
#define MODE_BRACKETPASTE 0x400
#define MODE_FOCUSON 0x800
#define MODE_MOUSE_ALL 0x1000
#define MODE_ORIGIN 0x2000
#define MODE_CRLF 0x4000
#define MODE_KEXTENDED 0x8000
#define MODE_CURSOR_VERY_VISIBLE 0x10000
#define MODE_CURSOR_BLINKING_SET 0x20000

#define ALL_MODES 0xffffff
#define ALL_MOUSE_MODES (MODE_MOUSE_STANDARD|MODE_MOUSE_BUTTON|MODE_MOUSE_ALL)
#define MOTION_MOUSE_MODES (MODE_MOUSE_BUTTON|MODE_MOUSE_ALL)
#define CURSOR_MODES (MODE_CURSOR|MODE_CURSOR_BLINKING|MODE_CURSOR_VERY_VISIBLE)

/* Mouse protocol constants. */
#define MOUSE_PARAM_MAX 0xff
#define MOUSE_PARAM_UTF8_MAX 0x7ff
#define MOUSE_PARAM_BTN_OFF 0x20
#define MOUSE_PARAM_POS_OFF 0x21

/* A single UTF-8 character. */
typedef u_int utf8_char;

/*
 * An expanded UTF-8 character. UTF8_SIZE must be big enough to hold combining
 * characters as well. It can't be more than 32 bytes without changes to how
 * characters are stored.
 */
#define UTF8_SIZE 21
struct utf8_data {
	u_char	data[UTF8_SIZE];

	u_char	have;
	u_char	size;

	u_char	width;	/* 0xff if invalid */
};
enum utf8_state {
	UTF8_MORE,
	UTF8_DONE,
	UTF8_ERROR
};

/* UTF-8 combine state. */
enum utf8_combine_state {
	UTF8_DISCARD_NOW,	   /* discard immediately */
	UTF8_WRITE_NOW,            /* do not combine, write immediately */
	UTF8_COMBINE_NOW,          /* combine immediately */
	UTF8_WRITE_MAYBE_COMBINE,  /* write but try to combine the next */
	UTF8_DISCARD_MAYBE_COMBINE /* discard but try to combine the next */
};

/* Colour flags. */
#define COLOUR_FLAG_256 0x01000000
#define COLOUR_FLAG_RGB 0x02000000

/* Special colours. */
#define COLOUR_DEFAULT(c) ((c) == 8 || (c) == 9)

/* Replacement palette. */
struct colour_palette {
	int	 fg;
	int	 bg;

	int	*palette;
	int	*default_palette;
};

/* Grid attributes. Anything above 0xff is stored in an extended cell. */
#define GRID_ATTR_BRIGHT 0x1
#define GRID_ATTR_DIM 0x2
#define GRID_ATTR_UNDERSCORE 0x4
#define GRID_ATTR_BLINK 0x8
#define GRID_ATTR_REVERSE 0x10
#define GRID_ATTR_HIDDEN 0x20
#define GRID_ATTR_ITALICS 0x40
#define GRID_ATTR_CHARSET 0x80	/* alternative character set */
#define GRID_ATTR_STRIKETHROUGH 0x100
#define GRID_ATTR_UNDERSCORE_2 0x200
#define GRID_ATTR_UNDERSCORE_3 0x400
#define GRID_ATTR_UNDERSCORE_4 0x800
#define GRID_ATTR_UNDERSCORE_5 0x1000
#define GRID_ATTR_OVERLINE 0x2000

/* All underscore attributes. */
#define GRID_ATTR_ALL_UNDERSCORE \
	(GRID_ATTR_UNDERSCORE|	 \
	 GRID_ATTR_UNDERSCORE_2| \
	 GRID_ATTR_UNDERSCORE_3| \
	 GRID_ATTR_UNDERSCORE_4| \
	 GRID_ATTR_UNDERSCORE_5)

/* Grid flags. */
#define GRID_FLAG_FG256 0x1
#define GRID_FLAG_BG256 0x2
#define GRID_FLAG_PADDING 0x4
#define GRID_FLAG_EXTENDED 0x8
#define GRID_FLAG_SELECTED 0x10
#define GRID_FLAG_NOPALETTE 0x20
#define GRID_FLAG_CLEARED 0x40

/* Grid line flags. */
#define GRID_LINE_WRAPPED 0x1
#define GRID_LINE_EXTENDED 0x2
#define GRID_LINE_DEAD 0x4
#define GRID_LINE_START_PROMPT 0x8
#define GRID_LINE_START_OUTPUT 0x10

/* Grid string flags. */
#define GRID_STRING_WITH_SEQUENCES 0x1
#define GRID_STRING_ESCAPE_SEQUENCES 0x2
#define GRID_STRING_TRIM_SPACES 0x4
#define GRID_STRING_USED_ONLY 0x8
#define GRID_STRING_EMPTY_CELLS 0x10

/* Cell positions. */
#define CELL_INSIDE 0
#define CELL_TOPBOTTOM 1
#define CELL_LEFTRIGHT 2
#define CELL_TOPLEFT 3
#define CELL_TOPRIGHT 4
#define CELL_BOTTOMLEFT 5
#define CELL_BOTTOMRIGHT 6
#define CELL_TOPJOIN 7
#define CELL_BOTTOMJOIN 8
#define CELL_LEFTJOIN 9
#define CELL_RIGHTJOIN 10
#define CELL_JOIN 11
#define CELL_OUTSIDE 12

/* Cell borders. */
#define CELL_BORDERS " xqlkmjwvtun~"
#define SIMPLE_BORDERS " |-+++++++++."
#define PADDED_BORDERS "             "

/* Grid cell data. */
struct grid_cell {
	struct utf8_data	data;
	u_short			attr;
	u_char			flags;
	int			fg;
	int			bg;
	int			us;
	u_int			link;
};

/* Grid extended cell entry. */
struct grid_extd_entry {
	utf8_char		data;
	u_short			attr;
	u_char			flags;
	int			fg;
	int			bg;
	int			us;
	u_int			link;
} __packed;

/* Grid cell entry. */
struct grid_cell_entry {
	union {
		u_int		offset;
		struct {
			u_char	attr;
			u_char	fg;
			u_char	bg;
			u_char	data;
		} data;
	};
	u_char			flags;
} __packed;

/* Grid line. */
struct grid_line {
	struct grid_cell_entry	*celldata;
	u_int			 cellused;
	u_int			 cellsize;

	struct grid_extd_entry	*extddata;
	u_int			 extdsize;

	int			 flags;
	time_t			 time;
};

/* Entire grid of cells. */
struct grid {
	int			 flags;
#define GRID_HISTORY 0x1 /* scroll lines into history */

	u_int			 sx;
	u_int			 sy;

	u_int			 hscrolled;
	u_int			 hsize;
	u_int			 hlimit;

	struct grid_line	*linedata;
};

/* Virtual cursor in a grid. */
struct grid_reader {
	struct grid	*gd;
	u_int		 cx;
	u_int		 cy;
};

/* Style alignment. */
enum style_align {
	STYLE_ALIGN_DEFAULT,
	STYLE_ALIGN_LEFT,
	STYLE_ALIGN_CENTRE,
	STYLE_ALIGN_RIGHT,
	STYLE_ALIGN_ABSOLUTE_CENTRE
};

/* Style list. */
enum style_list {
	STYLE_LIST_OFF,
	STYLE_LIST_ON,
	STYLE_LIST_FOCUS,
	STYLE_LIST_LEFT_MARKER,
	STYLE_LIST_RIGHT_MARKER,
};

/* Style range. */
enum style_range_type {
	STYLE_RANGE_NONE,
	STYLE_RANGE_LEFT,
	STYLE_RANGE_RIGHT,
	STYLE_RANGE_PANE,
	STYLE_RANGE_WINDOW,
	STYLE_RANGE_SESSION,
	STYLE_RANGE_USER
};
struct style_range {
	enum style_range_type	 type;
	u_int			 argument;
	char			 string[16];

	u_int			 start;
	u_int			 end; /* not included */

	TAILQ_ENTRY(style_range) entry;
};
TAILQ_HEAD(style_ranges, style_range);

/* Style default. */
enum style_default_type {
	STYLE_DEFAULT_BASE,
	STYLE_DEFAULT_PUSH,
	STYLE_DEFAULT_POP
};

/* Style option. */
struct style {
	struct grid_cell	gc;
	int			ignore;

	int			fill;
	enum style_align	align;
	enum style_list		list;

	enum style_range_type	range_type;
	u_int			range_argument;
	char			range_string[16];

	enum style_default_type	default_type;
};

#ifdef ENABLE_SIXEL
/* Image. */
struct image {
	struct screen		*s;
	struct sixel_image	*data;
	char			*fallback;

	u_int			 px;
	u_int			 py;
	u_int			 sx;
	u_int			 sy;

	TAILQ_ENTRY (image)	 all_entry;
	TAILQ_ENTRY (image)	 entry;
};
TAILQ_HEAD(images, image);
#endif

/* Cursor style. */
enum screen_cursor_style {
	SCREEN_CURSOR_DEFAULT,
	SCREEN_CURSOR_BLOCK,
	SCREEN_CURSOR_UNDERLINE,
	SCREEN_CURSOR_BAR
};

/* Virtual screen. */
struct screen_sel;
struct screen_titles;
struct screen {
	char				*title;
	char				*path;
	struct screen_titles		*titles;

	struct grid			*grid;	  /* grid data */

	u_int				 cx;	  /* cursor x */
	u_int				 cy;	  /* cursor y */

	enum screen_cursor_style	 cstyle;  /* cursor style */
	enum screen_cursor_style	 default_cstyle;
	int				 ccolour; /* cursor colour */
	int				 default_ccolour;

	u_int				 rupper;  /* scroll region top */
	u_int				 rlower;  /* scroll region bottom */

	int				 mode;
	int				 default_mode;

	u_int				 saved_cx;
	u_int				 saved_cy;
	struct grid			*saved_grid;
	struct grid_cell		 saved_cell;
	int				 saved_flags;

	bitstr_t			*tabs;
	struct screen_sel		*sel;

#ifdef ENABLE_SIXEL
	struct images			 images;
#endif

	struct screen_write_cline	*write_list;

	struct hyperlinks		*hyperlinks;
};

/* Screen write context. */
typedef void (*screen_write_init_ctx_cb)(struct screen_write_ctx *,
    struct tty_ctx *);
struct screen_write_ctx {
	struct window_pane		*wp;
	struct screen			*s;

	int				 flags;
#define SCREEN_WRITE_SYNC 0x1
#define SCREEN_WRITE_COMBINE 0x2

	screen_write_init_ctx_cb	 init_ctx_cb;
	void				*arg;

	struct screen_write_citem	*item;
	u_int				 scrolled;
	u_int				 bg;
	struct utf8_data		 previous;
};

/* Box border lines option. */
enum box_lines {
	BOX_LINES_DEFAULT = -1,
	BOX_LINES_SINGLE,
	BOX_LINES_DOUBLE,
	BOX_LINES_HEAVY,
	BOX_LINES_SIMPLE,
	BOX_LINES_ROUNDED,
	BOX_LINES_PADDED,
	BOX_LINES_NONE
};

/* Pane border lines option. */
enum pane_lines {
	PANE_LINES_SINGLE,
	PANE_LINES_DOUBLE,
	PANE_LINES_HEAVY,
	PANE_LINES_SIMPLE,
	PANE_LINES_NUMBER
};

/* Pane border indicator option. */
#define PANE_BORDER_OFF 0
#define PANE_BORDER_COLOUR 1
#define PANE_BORDER_ARROWS 2
#define PANE_BORDER_BOTH 3

/* Screen redraw context. */
struct screen_redraw_ctx {
	struct client	*c;

	u_int		 statuslines;
	int		 statustop;

	int		 pane_status;
	enum pane_lines	 pane_lines;

	struct grid_cell no_pane_gc;
	int		 no_pane_gc_set;

	u_int		 sx;
	u_int		 sy;
	u_int		 ox;
	u_int		 oy;
};

/* Screen size. */
#define screen_size_x(s) ((s)->grid->sx)
#define screen_size_y(s) ((s)->grid->sy)
#define screen_hsize(s) ((s)->grid->hsize)
#define screen_hlimit(s) ((s)->grid->hlimit)

/* Menu. */
struct menu_item {
	const char	*name;
	key_code	 key;
	const char	*command;
};
struct menu {
	const char		*title;
	struct menu_item	*items;
	u_int			 count;
	u_int			 width;
};
typedef void (*menu_choice_cb)(struct menu *, u_int, key_code, void *);

/*
 * Window mode. Windows can be in several modes and this is used to call the
 * right function to handle input and output.
 */
struct window_mode_entry;
struct window_mode {
	const char	*name;
	const char	*default_format;

	struct screen	*(*init)(struct window_mode_entry *,
			     struct cmd_find_state *, struct args *);
	void		 (*free)(struct window_mode_entry *);
	void		 (*resize)(struct window_mode_entry *, u_int, u_int);
	void		 (*update)(struct window_mode_entry *);
	void		 (*key)(struct window_mode_entry *, struct client *,
			     struct session *, struct winlink *, key_code,
			     struct mouse_event *);

	const char	*(*key_table)(struct window_mode_entry *);
	void		 (*command)(struct window_mode_entry *, struct client *,
			     struct session *, struct winlink *, struct args *,
			     struct mouse_event *);
	void		 (*formats)(struct window_mode_entry *,
			     struct format_tree *);
};

/* Active window mode. */
struct window_mode_entry {
	struct window_pane		*wp;
	struct window_pane		*swp;

	const struct window_mode	*mode;
	void				*data;

	struct screen			*screen;
	u_int				 prefix;

	TAILQ_ENTRY(window_mode_entry)	 entry;
};

/* Offsets into pane buffer. */
struct window_pane_offset {
	size_t	used;
};

/* Queued pane resize. */
struct window_pane_resize {
	u_int				sx;
	u_int				sy;

	u_int				osx;
	u_int				osy;

	TAILQ_ENTRY(window_pane_resize)	entry;
};
TAILQ_HEAD(window_pane_resizes, window_pane_resize);

/* Child window structure. */
struct window_pane {
	u_int		 id;
	u_int		 active_point;

	struct window	*window;
	struct options	*options;

	struct layout_cell *layout_cell;
	struct layout_cell *saved_layout_cell;

	u_int		 sx;
	u_int		 sy;

	u_int		 xoff;
	u_int		 yoff;

	int		 flags;
#define PANE_REDRAW 0x1
#define PANE_DROP 0x2
#define PANE_FOCUSED 0x4
#define PANE_VISITED 0x8
/* 0x10 unused */
/* 0x20 unused */
#define PANE_INPUTOFF 0x40
#define PANE_CHANGED 0x80
#define PANE_EXITED 0x100
#define PANE_STATUSREADY 0x200
#define PANE_STATUSDRAWN 0x400
#define PANE_EMPTY 0x800
#define PANE_STYLECHANGED 0x1000
#define PANE_UNSEENCHANGES 0x2000

	int		 argc;
	char	       **argv;
	char		*shell;
	char		*cwd;

	pid_t		 pid;
	char		 tty[TTY_NAME_MAX];
	int		 status;
	struct timeval	 dead_time;

	int		 fd;
	struct bufferevent *event;

	struct window_pane_offset offset;
	size_t		 base_offset;

	struct window_pane_resizes resize_queue;
	struct event	 resize_timer;

	struct input_ctx *ictx;

	struct grid_cell cached_gc;
	struct grid_cell cached_active_gc;
	struct colour_palette palette;

	int		 pipe_fd;
	struct bufferevent *pipe_event;
	struct window_pane_offset pipe_offset;

	struct screen	*screen;
	struct screen	 base;

	struct screen	 status_screen;
	size_t		 status_size;

	TAILQ_HEAD(, window_mode_entry) modes;

	char		*searchstr;
	int		 searchregex;

	int		 border_gc_set;
	struct grid_cell border_gc;

	TAILQ_ENTRY(window_pane) entry;  /* link in list of all panes */
	TAILQ_ENTRY(window_pane) sentry; /* link in list of last visited */
	RB_ENTRY(window_pane) tree_entry;
};
TAILQ_HEAD(window_panes, window_pane);
RB_HEAD(window_pane_tree, window_pane);

/* Window structure. */
struct window {
	u_int			 id;
	void			*latest;

	char			*name;
	struct event		 name_event;
	struct timeval		 name_time;

	struct event		 alerts_timer;
	struct event		 offset_timer;

	struct timeval		 activity_time;

	struct window_pane	*active;
	struct window_panes 	 last_panes;
	struct window_panes	 panes;

	int			 lastlayout;
	struct layout_cell	*layout_root;
	struct layout_cell	*saved_layout_root;
	char			*old_layout;

	u_int			 sx;
	u_int			 sy;
	u_int			 manual_sx;
	u_int			 manual_sy;
	u_int			 xpixel;
	u_int			 ypixel;

	u_int			 new_sx;
	u_int			 new_sy;
	u_int			 new_xpixel;
	u_int			 new_ypixel;

	struct utf8_data	*fill_character;
	int			 flags;
#define WINDOW_BELL 0x1
#define WINDOW_ACTIVITY 0x2
#define WINDOW_SILENCE 0x4
#define WINDOW_ZOOMED 0x8
#define WINDOW_WASZOOMED 0x10
#define WINDOW_RESIZE 0x20
#define WINDOW_ALERTFLAGS (WINDOW_BELL|WINDOW_ACTIVITY|WINDOW_SILENCE)

	int			 alerts_queued;
	TAILQ_ENTRY(window)	 alerts_entry;

	struct options		*options;

	u_int			 references;
	TAILQ_HEAD(, winlink)	 winlinks;

	RB_ENTRY(window)	 entry;
};
RB_HEAD(windows, window);

/* Entry on local window list. */
struct winlink {
	int		 idx;
	struct session	*session;
	struct window	*window;

	int		 flags;
#define WINLINK_BELL 0x1
#define WINLINK_ACTIVITY 0x2
#define WINLINK_SILENCE 0x4
#define WINLINK_ALERTFLAGS (WINLINK_BELL|WINLINK_ACTIVITY|WINLINK_SILENCE)
#define WINLINK_VISITED 0x8

	RB_ENTRY(winlink) entry;
	TAILQ_ENTRY(winlink) wentry;
	TAILQ_ENTRY(winlink) sentry;
};
RB_HEAD(winlinks, winlink);
TAILQ_HEAD(winlink_stack, winlink);

/* Window size option. */
#define WINDOW_SIZE_LARGEST 0
#define WINDOW_SIZE_SMALLEST 1
#define WINDOW_SIZE_MANUAL 2
#define WINDOW_SIZE_LATEST 3

/* Pane border status option. */
#define PANE_STATUS_OFF 0
#define PANE_STATUS_TOP 1
#define PANE_STATUS_BOTTOM 2

/* Layout direction. */
enum layout_type {
	LAYOUT_LEFTRIGHT,
	LAYOUT_TOPBOTTOM,
	LAYOUT_WINDOWPANE
};

/* Layout cells queue. */
TAILQ_HEAD(layout_cells, layout_cell);

/* Layout cell. */
struct layout_cell {
	enum layout_type type;

	struct layout_cell *parent;

	u_int		 sx;
	u_int		 sy;

	u_int		 xoff;
	u_int		 yoff;

	struct window_pane *wp;
	struct layout_cells cells;

	TAILQ_ENTRY(layout_cell) entry;
};

/* Environment variable. */
struct environ_entry {
	char		*name;
	char		*value;

	int		 flags;
#define ENVIRON_HIDDEN 0x1

	RB_ENTRY(environ_entry) entry;
};

/* Client session. */
struct session_group {
	const char		*name;
	TAILQ_HEAD(, session)	 sessions;

	RB_ENTRY(session_group)	 entry;
};
RB_HEAD(session_groups, session_group);

struct session {
	u_int		 id;

	char		*name;
	const char	*cwd;

	struct timeval	 creation_time;
	struct timeval	 last_attached_time;
	struct timeval	 activity_time;
	struct timeval	 last_activity_time;

	struct event	 lock_timer;

	struct winlink	*curw;
	struct winlink_stack lastw;
	struct winlinks	 windows;

	int		 statusat;
	u_int		 statuslines;

	struct options	*options;

#define SESSION_PASTING 0x1
#define SESSION_ALERTED 0x2
	int		 flags;

	u_int		 attached;

	struct termios	*tio;

	struct environ	*environ;

	int		 references;

	TAILQ_ENTRY(session) gentry;
	RB_ENTRY(session)    entry;
};
RB_HEAD(sessions, session);

/* Mouse button masks. */
#define MOUSE_MASK_BUTTONS 195
#define MOUSE_MASK_SHIFT 4
#define MOUSE_MASK_META 8
#define MOUSE_MASK_CTRL 16
#define MOUSE_MASK_DRAG 32
#define MOUSE_MASK_MODIFIERS (MOUSE_MASK_SHIFT|MOUSE_MASK_META|MOUSE_MASK_CTRL)

/* Mouse wheel type. */
#define MOUSE_WHEEL_UP 64
#define MOUSE_WHEEL_DOWN 65

/* Mouse button type. */
#define MOUSE_BUTTON_1 0
#define MOUSE_BUTTON_2 1
#define MOUSE_BUTTON_3 2
#define MOUSE_BUTTON_6 66
#define MOUSE_BUTTON_7 67
#define MOUSE_BUTTON_8 128
#define MOUSE_BUTTON_9 129
#define MOUSE_BUTTON_10 130
#define MOUSE_BUTTON_11 131

/* Mouse helpers. */
#define MOUSE_BUTTONS(b) ((b) & MOUSE_MASK_BUTTONS)
#define MOUSE_WHEEL(b) \
	(((b) & MOUSE_MASK_BUTTONS) == MOUSE_WHEEL_UP || \
	 ((b) & MOUSE_MASK_BUTTONS) == MOUSE_WHEEL_DOWN)
#define MOUSE_DRAG(b) ((b) & MOUSE_MASK_DRAG)
#define MOUSE_RELEASE(b) (((b) & MOUSE_MASK_BUTTONS) == 3)

/* Mouse input. */
struct mouse_event {
	int		valid;
	int		ignore;

	key_code	key;

	int		statusat;
	u_int		statuslines;

	u_int		x;
	u_int		y;
	u_int		b;

	u_int		lx;
	u_int		ly;
	u_int		lb;

	u_int		ox;
	u_int		oy;

	int		s;
	int		w;
	int		wp;

	u_int		sgr_type;
	u_int		sgr_b;
};

/* Key event. */
struct key_event {
	key_code		key;
	struct mouse_event	m;
};

/* Terminal definition. */
struct tty_term {
	char		*name;
	struct tty	*tty;
	int		 features;

	char		 acs[UCHAR_MAX + 1][2];

	struct tty_code	*codes;

#define TERM_256COLOURS 0x1
#define TERM_NOAM 0x2
#define TERM_DECSLRM 0x4
#define TERM_DECFRA 0x8
#define TERM_RGBCOLOURS 0x10
#define TERM_VT100LIKE 0x20
#define TERM_SIXEL 0x40
	int		 flags;

	LIST_ENTRY(tty_term) entry;
};
LIST_HEAD(tty_terms, tty_term);

/* Client terminal. */
struct tty {
	struct client	*client;
	struct event	 start_timer;
	struct event	 clipboard_timer;

	u_int		 sx;
	u_int		 sy;
        /* Cell size in pixels. */
	u_int		 xpixel;
	u_int		 ypixel;

	u_int		 cx;
	u_int		 cy;
	enum screen_cursor_style cstyle;
	int		 ccolour;

        /* Properties of the area being drawn on. */
        /* When true, the drawing area is bigger than the terminal. */
	int		 oflag;
	u_int		 oox;
	u_int		 ooy;
	u_int		 osx;
	u_int		 osy;

	int		 mode;
	int              fg;
	int              bg;

	u_int		 rlower;
	u_int		 rupper;

	u_int		 rleft;
	u_int		 rright;

	struct event	 event_in;
	struct evbuffer	*in;
	struct event	 event_out;
	struct evbuffer	*out;
	struct event	 timer;
	size_t		 discarded;

	struct termios	 tio;

	struct grid_cell cell;
	struct grid_cell last_cell;

#define TTY_NOCURSOR 0x1
#define TTY_FREEZE 0x2
#define TTY_TIMER 0x4
#define TTY_NOBLOCK 0x8
#define TTY_STARTED 0x10
#define TTY_OPENED 0x20
#define TTY_OSC52QUERY 0x40
#define TTY_BLOCK 0x80
#define TTY_HAVEDA 0x100 /* Primary DA. */
#define TTY_HAVEXDA 0x200
#define TTY_SYNCING 0x400
#define TTY_HAVEDA2 0x800 /* Secondary DA. */
#define TTY_HAVEFG 0x1000
#define TTY_HAVEBG 0x2000
#define TTY_ALL_REQUEST_FLAGS \
	(TTY_HAVEDA|TTY_HAVEDA2|TTY_HAVEXDA|TTY_HAVEFG|TTY_HAVEBG)
	int		 flags;

	struct tty_term	*term;

	u_int		 mouse_last_x;
	u_int		 mouse_last_y;
	u_int		 mouse_last_b;
	int		 mouse_drag_flag;
	void		(*mouse_drag_update)(struct client *,
			    struct mouse_event *);
	void		(*mouse_drag_release)(struct client *,
			    struct mouse_event *);

	struct event	 key_timer;
	struct tty_key	*key_tree;
};

/* Terminal command context. */
typedef void (*tty_ctx_redraw_cb)(const struct tty_ctx *);
typedef int (*tty_ctx_set_client_cb)(struct tty_ctx *, struct client *);
struct tty_ctx {
	struct screen		*s;

	tty_ctx_redraw_cb	 redraw_cb;
	tty_ctx_set_client_cb	 set_client_cb;
	void			*arg;

	const struct grid_cell	*cell;
	int			 wrapped;

	u_int			 num;
	void			*ptr;
	void			*ptr2;

	/*
	 * Whether this command should be sent even when the pane is not
	 * visible (used for a passthrough sequence when allow-passthrough is
	 * "all").
	 */
	int			 allow_invisible_panes;

	/*
	 * Cursor and region position before the screen was updated - this is
	 * where the command should be applied; the values in the screen have
	 * already been updated.
	 */
	u_int			 ocx;
	u_int			 ocy;

	u_int			 orupper;
	u_int			 orlower;

	/* Target region (usually pane) offset and size. */
	u_int			 xoff;
	u_int			 yoff;
	u_int			 rxoff;
	u_int			 ryoff;
	u_int			 sx;
	u_int			 sy;

	/* The background colour used for clearing (erasing). */
	u_int			 bg;

	/* The default colours and palette. */
	struct grid_cell	 defaults;
	struct colour_palette	*palette;

	/* Containing region (usually window) offset and size. */
	int			 bigger;
	u_int			 wox;
	u_int			 woy;
	u_int			 wsx;
	u_int			 wsy;
};

/* Saved message entry. */
struct message_entry {
	char				*msg;
	u_int				 msg_num;
	struct timeval			 msg_time;

	TAILQ_ENTRY(message_entry)	 entry;
};
TAILQ_HEAD(message_list, message_entry);

/* Argument type. */
enum args_type {
	ARGS_NONE,
	ARGS_STRING,
	ARGS_COMMANDS
};

/* Argument value. */
struct args_value {
	enum args_type		 type;
	union {
		char		*string;
		struct cmd_list	*cmdlist;
	};
	char			*cached;
	TAILQ_ENTRY(args_value)	 entry;
};

/* Arguments set. */
struct args_entry;
RB_HEAD(args_tree, args_entry);

/* Arguments parsing type. */
enum args_parse_type {
	ARGS_PARSE_INVALID,
	ARGS_PARSE_STRING,
	ARGS_PARSE_COMMANDS_OR_STRING,
	ARGS_PARSE_COMMANDS
};

/* Arguments parsing state. */
typedef enum args_parse_type (*args_parse_cb)(struct args *, u_int, char **);
struct args_parse {
	const char	*template;
	int		 lower;
	int		 upper;
	args_parse_cb	 cb;
};

/* Command find structures. */
enum cmd_find_type {
	CMD_FIND_PANE,
	CMD_FIND_WINDOW,
	CMD_FIND_SESSION,
};
struct cmd_find_state {
	int			 flags;
	struct cmd_find_state	*current;

	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	int			 idx;
};

/* Command find flags. */
#define CMD_FIND_PREFER_UNATTACHED 0x1
#define CMD_FIND_QUIET 0x2
#define CMD_FIND_WINDOW_INDEX 0x4
#define CMD_FIND_DEFAULT_MARKED 0x8
#define CMD_FIND_EXACT_SESSION 0x10
#define CMD_FIND_EXACT_WINDOW 0x20
#define CMD_FIND_CANFAIL 0x40

/* List of commands. */
struct cmd_list {
	int		 references;
	u_int		 group;
	struct cmds	*list;
};

/* Command return values. */
enum cmd_retval {
	CMD_RETURN_ERROR = -1,
	CMD_RETURN_NORMAL = 0,
	CMD_RETURN_WAIT,
	CMD_RETURN_STOP
};

/* Command parse result. */
enum cmd_parse_status {
	CMD_PARSE_ERROR,
	CMD_PARSE_SUCCESS
};
struct cmd_parse_result {
	enum cmd_parse_status	 status;
	struct cmd_list		*cmdlist;
	char			*error;
};
struct cmd_parse_input {
	int			 flags;
#define CMD_PARSE_QUIET 0x1
#define CMD_PARSE_PARSEONLY 0x2
#define CMD_PARSE_NOALIAS 0x4
#define CMD_PARSE_VERBOSE 0x8
#define CMD_PARSE_ONEGROUP 0x10

	const char		*file;
	u_int			 line;

	struct cmdq_item	*item;
	struct client		*c;
	struct cmd_find_state	 fs;
};

/* Command queue flags. */
#define CMDQ_STATE_REPEAT 0x1
#define CMDQ_STATE_CONTROL 0x2
#define CMDQ_STATE_NOHOOKS 0x4

/* Command queue callback. */
typedef enum cmd_retval (*cmdq_cb) (struct cmdq_item *, void *);

/* Command definition flag. */
struct cmd_entry_flag {
	char			 flag;
	enum cmd_find_type	 type;
	int			 flags;
};

/* Command definition. */
struct cmd_entry {
	const char		*name;
	const char		*alias;

	struct args_parse	 args;
	const char		*usage;

	struct cmd_entry_flag	 source;
	struct cmd_entry_flag	 target;

#define CMD_STARTSERVER 0x1
#define CMD_READONLY 0x2
#define CMD_AFTERHOOK 0x4
#define CMD_CLIENT_CFLAG 0x8
#define CMD_CLIENT_TFLAG 0x10
#define CMD_CLIENT_CANFAIL 0x20
	int		 flags;

	enum cmd_retval	 (*exec)(struct cmd *, struct cmdq_item *);
};

/* Status line. */
#define STATUS_LINES_LIMIT 5
struct status_line_entry {
	char			*expanded;
	struct style_ranges	 ranges;
};
struct status_line {
	struct event		 timer;

	struct screen		 screen;
	struct screen		*active;
	int			 references;

	struct grid_cell	 style;
	struct status_line_entry entries[STATUS_LINES_LIMIT];
};

/* Prompt type. */
#define PROMPT_NTYPES 4
enum prompt_type {
	PROMPT_TYPE_COMMAND,
	PROMPT_TYPE_SEARCH,
	PROMPT_TYPE_TARGET,
	PROMPT_TYPE_WINDOW_TARGET,
	PROMPT_TYPE_INVALID = 0xff
};

/* File in client. */
typedef void (*client_file_cb) (struct client *, const char *, int, int,
    struct evbuffer *, void *);
struct client_file {
	struct client			*c;
	struct tmuxpeer			*peer;
	struct client_files		*tree;
	int				 references;
	int				 stream;

	char				*path;
	struct evbuffer			*buffer;
	struct bufferevent		*event;

	int				 fd;
	int				 error;
	int				 closed;

	client_file_cb			 cb;
	void				*data;

	RB_ENTRY(client_file)		 entry;
};
RB_HEAD(client_files, client_file);

/* Client window. */
struct client_window {
	u_int			 window;
	struct window_pane	*pane;

	u_int			 sx;
	u_int			 sy;

	RB_ENTRY(client_window)	 entry;
};
RB_HEAD(client_windows, client_window);

/* Visible areas not obstructed by overlays. */
#define OVERLAY_MAX_RANGES 3
struct overlay_ranges {
	u_int	px[OVERLAY_MAX_RANGES];
	u_int	nx[OVERLAY_MAX_RANGES];
};

/* Client connection. */
typedef int (*prompt_input_cb)(struct client *, void *, const char *, int);
typedef void (*prompt_free_cb)(void *);
typedef void (*overlay_check_cb)(struct client*, void *, u_int, u_int, u_int,
	    struct overlay_ranges *);
typedef struct screen *(*overlay_mode_cb)(struct client *, void *, u_int *,
	    u_int *);
typedef void (*overlay_draw_cb)(struct client *, void *,
	    struct screen_redraw_ctx *);
typedef int (*overlay_key_cb)(struct client *, void *, struct key_event *);
typedef void (*overlay_free_cb)(struct client *, void *);
typedef void (*overlay_resize_cb)(struct client *, void *);
struct client {
	const char		*name;
	struct tmuxpeer		*peer;
	struct cmdq_list	*queue;

	struct client_windows	 windows;

	struct control_state	*control_state;
	u_int			 pause_age;

	pid_t			 pid;
	int			 fd;
	int			 out_fd;
	struct event		 event;
	int			 retval;

	struct timeval		 creation_time;
	struct timeval		 activity_time;

	struct environ		*environ;
	struct format_job_tree	*jobs;

	char			*title;
	char			*path;
	const char		*cwd;

	char			*term_name;
	int			 term_features;
	char			*term_type;
	char		       **term_caps;
	u_int			 term_ncaps;

	char			*ttyname;
	struct tty		 tty;

	size_t			 written;
	size_t			 discarded;
	size_t			 redraw;

	struct event		 repeat_timer;

	struct event		 click_timer;
	u_int			 click_button;
	struct mouse_event	 click_event;

	struct status_line	 status;

#define CLIENT_TERMINAL 0x1
#define CLIENT_LOGIN 0x2
#define CLIENT_EXIT 0x4
#define CLIENT_REDRAWWINDOW 0x8
#define CLIENT_REDRAWSTATUS 0x10
#define CLIENT_REPEAT 0x20
#define CLIENT_SUSPENDED 0x40
#define CLIENT_ATTACHED 0x80
#define CLIENT_EXITED 0x100
#define CLIENT_DEAD 0x200
#define CLIENT_REDRAWBORDERS 0x400
#define CLIENT_READONLY 0x800
#define CLIENT_NOSTARTSERVER 0x1000
#define CLIENT_CONTROL 0x2000
#define CLIENT_CONTROLCONTROL 0x4000
#define CLIENT_FOCUSED 0x8000
#define CLIENT_UTF8 0x10000
#define CLIENT_IGNORESIZE 0x20000
#define CLIENT_IDENTIFIED 0x40000
#define CLIENT_STATUSFORCE 0x80000
#define CLIENT_DOUBLECLICK 0x100000
#define CLIENT_TRIPLECLICK 0x200000
#define CLIENT_SIZECHANGED 0x400000
#define CLIENT_STATUSOFF 0x800000
#define CLIENT_REDRAWSTATUSALWAYS 0x1000000
#define CLIENT_REDRAWOVERLAY 0x2000000
#define CLIENT_CONTROL_NOOUTPUT 0x4000000
#define CLIENT_DEFAULTSOCKET 0x8000000
#define CLIENT_STARTSERVER 0x10000000
#define CLIENT_REDRAWPANES 0x20000000
#define CLIENT_NOFORK 0x40000000
#define CLIENT_ACTIVEPANE 0x80000000ULL
#define CLIENT_CONTROL_PAUSEAFTER 0x100000000ULL
#define CLIENT_CONTROL_WAITEXIT 0x200000000ULL
#define CLIENT_WINDOWSIZECHANGED 0x400000000ULL
#define CLIENT_CLIPBOARDBUFFER 0x800000000ULL
#define CLIENT_BRACKETPASTING 0x1000000000ULL
#define CLIENT_ALLREDRAWFLAGS		\
	(CLIENT_REDRAWWINDOW|		\
	 CLIENT_REDRAWSTATUS|		\
	 CLIENT_REDRAWSTATUSALWAYS|	\
	 CLIENT_REDRAWBORDERS|		\
	 CLIENT_REDRAWOVERLAY|		\
	 CLIENT_REDRAWPANES)
#define CLIENT_UNATTACHEDFLAGS	\
	(CLIENT_DEAD|		\
	 CLIENT_SUSPENDED|	\
	 CLIENT_EXIT)
#define CLIENT_NODETACHFLAGS	\
	(CLIENT_DEAD|		\
	 CLIENT_EXIT)
#define CLIENT_NOSIZEFLAGS	\
	(CLIENT_DEAD|		\
	 CLIENT_SUSPENDED|	\
	 CLIENT_EXIT)
	uint64_t		 flags;

	enum {
		CLIENT_EXIT_RETURN,
		CLIENT_EXIT_SHUTDOWN,
		CLIENT_EXIT_DETACH
	}			 exit_type;
	enum msgtype		 exit_msgtype;
	char			*exit_session;
	char			*exit_message;

	struct key_table	*keytable;

	uint64_t		 redraw_panes;

	int			 message_ignore_keys;
	int			 message_ignore_styles;
	char			*message_string;
	struct event		 message_timer;

	char			*prompt_string;
	struct utf8_data	*prompt_buffer;
	char			*prompt_last;
	size_t			 prompt_index;
	prompt_input_cb		 prompt_inputcb;
	prompt_free_cb		 prompt_freecb;
	void			*prompt_data;
	u_int			 prompt_hindex[PROMPT_NTYPES];
	enum {
		PROMPT_ENTRY,
		PROMPT_COMMAND
	}			 prompt_mode;
	struct utf8_data	*prompt_saved;
#define PROMPT_SINGLE 0x1
#define PROMPT_NUMERIC 0x2
#define PROMPT_INCREMENTAL 0x4
#define PROMPT_NOFORMAT 0x8
#define PROMPT_KEY 0x10
	int			 prompt_flags;
	enum prompt_type	 prompt_type;
	int			 prompt_cursor;

	struct session		*session;
	struct session		*last_session;

	int			 references;

	void			*pan_window;
	u_int			 pan_ox;
	u_int			 pan_oy;

	overlay_check_cb	 overlay_check;
	overlay_mode_cb		 overlay_mode;
	overlay_draw_cb		 overlay_draw;
	overlay_key_cb		 overlay_key;
	overlay_free_cb		 overlay_free;
	overlay_resize_cb	 overlay_resize;
	void			*overlay_data;
	struct event		 overlay_timer;

	struct client_files	 files;

	u_int			*clipboard_panes;
	u_int			 clipboard_npanes;

	TAILQ_ENTRY(client)	 entry;
};
TAILQ_HEAD(clients, client);

/* Control mode subscription type. */
enum control_sub_type {
	CONTROL_SUB_SESSION,
	CONTROL_SUB_PANE,
	CONTROL_SUB_ALL_PANES,
	CONTROL_SUB_WINDOW,
	CONTROL_SUB_ALL_WINDOWS
};

/* Key binding and key table. */
struct key_binding {
	key_code		 key;
	struct cmd_list		*cmdlist;
	const char		*note;

	int			 flags;
#define KEY_BINDING_REPEAT 0x1

	RB_ENTRY(key_binding)	 entry;
};
RB_HEAD(key_bindings, key_binding);

struct key_table {
	const char		*name;
	struct key_bindings	 key_bindings;
	struct key_bindings	 default_key_bindings;

	u_int			 references;

	RB_ENTRY(key_table)	 entry;
};
RB_HEAD(key_tables, key_table);

/* Option data. */
RB_HEAD(options_array, options_array_item);
union options_value {
	char			*string;
	long long		 number;
	struct style		 style;
	struct options_array	 array;
	struct cmd_list		*cmdlist;
};

/* Option table entries. */
enum options_table_type {
	OPTIONS_TABLE_STRING,
	OPTIONS_TABLE_NUMBER,
	OPTIONS_TABLE_KEY,
	OPTIONS_TABLE_COLOUR,
	OPTIONS_TABLE_FLAG,
	OPTIONS_TABLE_CHOICE,
	OPTIONS_TABLE_COMMAND
};

#define OPTIONS_TABLE_NONE 0
#define OPTIONS_TABLE_SERVER 0x1
#define OPTIONS_TABLE_SESSION 0x2
#define OPTIONS_TABLE_WINDOW 0x4
#define OPTIONS_TABLE_PANE 0x8

#define OPTIONS_TABLE_IS_ARRAY 0x1
#define OPTIONS_TABLE_IS_HOOK 0x2
#define OPTIONS_TABLE_IS_STYLE 0x4

struct options_table_entry {
	const char		 *name;
	const char		 *alternative_name;
	enum options_table_type	  type;
	int			  scope;
	int			  flags;

	u_int			  minimum;
	u_int			  maximum;
	const char		**choices;

	const char		 *default_str;
	long long		  default_num;
	const char		**default_arr;

	const char		 *separator;
	const char		 *pattern;

	const char		 *text;
	const char		 *unit;
};

struct options_name_map {
	const char		*from;
	const char		*to;
};

/* Common command usages. */
#define CMD_TARGET_PANE_USAGE "[-t target-pane]"
#define CMD_TARGET_WINDOW_USAGE "[-t target-window]"
#define CMD_TARGET_SESSION_USAGE "[-t target-session]"
#define CMD_TARGET_CLIENT_USAGE "[-t target-client]"
#define CMD_SRCDST_PANE_USAGE "[-s src-pane] [-t dst-pane]"
#define CMD_SRCDST_WINDOW_USAGE "[-s src-window] [-t dst-window]"
#define CMD_SRCDST_SESSION_USAGE "[-s src-session] [-t dst-session]"
#define CMD_SRCDST_CLIENT_USAGE "[-s src-client] [-t dst-client]"
#define CMD_BUFFER_USAGE "[-b buffer-name]"

/* Spawn common context. */
struct spawn_context {
	struct cmdq_item	 *item;

	struct session		 *s;
	struct winlink		 *wl;
	struct client		 *tc;

	struct window_pane	 *wp0;
	struct layout_cell	 *lc;

	const char		 *name;
	char			**argv;
	int			  argc;
	struct environ		 *environ;

	int			  idx;
	const char		 *cwd;

	int			  flags;
#define SPAWN_KILL 0x1
#define SPAWN_DETACHED 0x2
#define SPAWN_RESPAWN 0x4
#define SPAWN_BEFORE 0x8
#define SPAWN_NONOTIFY 0x10
#define SPAWN_FULLSIZE 0x20
#define SPAWN_EMPTY 0x40
#define SPAWN_ZOOM 0x80
};

/* Mode tree sort order. */
struct mode_tree_sort_criteria {
	u_int	field;
	int	reversed;
};

/* tmux.c */
extern struct options	*global_options;
extern struct options	*global_s_options;
extern struct options	*global_w_options;
extern struct environ	*global_environ;
extern struct timeval	 start_time;
extern const char	*socket_path;
extern const char	*shell_command;
extern int		 ptm_fd;
extern const char	*shell_command;
int		 checkshell(const char *);
void		 setblocking(int, int);
uint64_t	 get_timer(void);
const char	*sig2name(int);
const char	*find_cwd(void);
const char	*find_home(void);
const char	*getversion(void);

/* proc.c */
struct imsg;
int	proc_send(struct tmuxpeer *, enum msgtype, int, const void *, size_t);
struct tmuxproc *proc_start(const char *);
void	proc_loop(struct tmuxproc *, int (*)(void));
void	proc_exit(struct tmuxproc *);
void	proc_set_signals(struct tmuxproc *, void(*)(int));
void	proc_clear_signals(struct tmuxproc *, int);
struct tmuxpeer *proc_add_peer(struct tmuxproc *, int,
	    void (*)(struct imsg *, void *), void *);
void	proc_remove_peer(struct tmuxpeer *);
void	proc_kill_peer(struct tmuxpeer *);
void	proc_flush_peer(struct tmuxpeer *);
void	proc_toggle_log(struct tmuxproc *);
pid_t	proc_fork_and_daemon(int *);
uid_t	proc_get_peer_uid(struct tmuxpeer *);

/* cfg.c */
extern int cfg_finished;
extern struct client *cfg_client;
extern char **cfg_files;
extern u_int cfg_nfiles;
extern int cfg_quiet;
void	start_cfg(void);
int	load_cfg(const char *, struct client *, struct cmdq_item *, int,
	    struct cmdq_item **);
int	load_cfg_from_buffer(const void *, size_t, const char *,
	    struct client *, struct cmdq_item *, int, struct cmdq_item **);
void printflike(1, 2) cfg_add_cause(const char *, ...);
void	cfg_print_causes(struct cmdq_item *);
void	cfg_show_causes(struct session *);

/* paste.c */
struct paste_buffer;
const char	*paste_buffer_name(struct paste_buffer *);
u_int		 paste_buffer_order(struct paste_buffer *);
time_t		 paste_buffer_created(struct paste_buffer *);
const char	*paste_buffer_data(struct paste_buffer *, size_t *);
struct paste_buffer *paste_walk(struct paste_buffer *);
int		 paste_is_empty(void);
struct paste_buffer *paste_get_top(const char **);
struct paste_buffer *paste_get_name(const char *);
void		 paste_free(struct paste_buffer *);
void		 paste_add(const char *, char *, size_t);
int		 paste_rename(const char *, const char *, char **);
int		 paste_set(char *, size_t, const char *, char **);
void		 paste_replace(struct paste_buffer *, char *, size_t);
char		*paste_make_sample(struct paste_buffer *);

/* format.c */
#define FORMAT_STATUS 0x1
#define FORMAT_FORCE 0x2
#define FORMAT_NOJOBS 0x4
#define FORMAT_VERBOSE 0x8
#define FORMAT_NONE 0
#define FORMAT_PANE 0x80000000U
#define FORMAT_WINDOW 0x40000000U
struct format_tree;
struct format_modifier;
typedef void *(*format_cb)(struct format_tree *);
void		 format_tidy_jobs(void);
const char	*format_skip(const char *, const char *);
int		 format_true(const char *);
struct format_tree *format_create(struct client *, struct cmdq_item *, int,
		     int);
void		 format_free(struct format_tree *);
void		 format_merge(struct format_tree *, struct format_tree *);
struct window_pane *format_get_pane(struct format_tree *);
void printflike(3, 4) format_add(struct format_tree *, const char *,
		     const char *, ...);
void		 format_add_tv(struct format_tree *, const char *,
		     struct timeval *);
void		 format_add_cb(struct format_tree *, const char *, format_cb);
void		 format_log_debug(struct format_tree *, const char *);
void		 format_each(struct format_tree *, void (*)(const char *,
		     const char *, void *), void *);
char		*format_pretty_time(time_t, int);
char		*format_expand_time(struct format_tree *, const char *);
char		*format_expand(struct format_tree *, const char *);
char		*format_single(struct cmdq_item *, const char *,
		     struct client *, struct session *, struct winlink *,
		     struct window_pane *);
char		*format_single_from_state(struct cmdq_item *, const char *,
		    struct client *, struct cmd_find_state *);
char		*format_single_from_target(struct cmdq_item *, const char *);
struct format_tree *format_create_defaults(struct cmdq_item *, struct client *,
		     struct session *, struct winlink *, struct window_pane *);
struct format_tree *format_create_from_state(struct cmdq_item *,
		     struct client *, struct cmd_find_state *);
struct format_tree *format_create_from_target(struct cmdq_item *);
void		 format_defaults(struct format_tree *, struct client *,
		     struct session *, struct winlink *, struct window_pane *);
void		 format_defaults_window(struct format_tree *, struct window *);
void		 format_defaults_pane(struct format_tree *,
		     struct window_pane *);
void		 format_defaults_paste_buffer(struct format_tree *,
		     struct paste_buffer *);
void		 format_lost_client(struct client *);
char		*format_grid_word(struct grid *, u_int, u_int);
char		*format_grid_hyperlink(struct grid *, u_int, u_int,
		     struct screen *);
char		*format_grid_line(struct grid *, u_int);

/* format-draw.c */
void		 format_draw(struct screen_write_ctx *,
		     const struct grid_cell *, u_int, const char *,
		     struct style_ranges *, int);
u_int		 format_width(const char *);
char		*format_trim_left(const char *, u_int);
char		*format_trim_right(const char *, u_int);

/* notify.c */
void	notify_hook(struct cmdq_item *, const char *);
void	notify_client(const char *, struct client *);
void	notify_session(const char *, struct session *);
void	notify_winlink(const char *, struct winlink *);
void	notify_session_window(const char *, struct session *, struct window *);
void	notify_window(const char *, struct window *);
void	notify_pane(const char *, struct window_pane *);
void	notify_paste_buffer(const char *, int);

/* options.c */
struct options	*options_create(struct options *);
void		 options_free(struct options *);
struct options	*options_get_parent(struct options *);
void		 options_set_parent(struct options *, struct options *);
struct options_entry *options_first(struct options *);
struct options_entry *options_next(struct options_entry *);
struct options_entry *options_empty(struct options *,
		     const struct options_table_entry *);
struct options_entry *options_default(struct options *,
		     const struct options_table_entry *);
char		*options_default_to_string(const struct options_table_entry *);
const char	*options_name(struct options_entry *);
struct options	*options_owner(struct options_entry *);
const struct options_table_entry *options_table_entry(struct options_entry *);
struct options_entry *options_get_only(struct options *, const char *);
struct options_entry *options_get(struct options *, const char *);
void		 options_array_clear(struct options_entry *);
union options_value *options_array_get(struct options_entry *, u_int);
int		 options_array_set(struct options_entry *, u_int, const char *,
		     int, char **);
int		 options_array_assign(struct options_entry *, const char *,
		     char **);
struct options_array_item *options_array_first(struct options_entry *);
struct options_array_item *options_array_next(struct options_array_item *);
u_int		 options_array_item_index(struct options_array_item *);
union options_value *options_array_item_value(struct options_array_item *);
int		 options_is_array(struct options_entry *);
int		 options_is_string(struct options_entry *);
char		*options_to_string(struct options_entry *, int, int);
char		*options_parse(const char *, int *);
struct options_entry *options_parse_get(struct options *, const char *, int *,
		     int);
char		*options_match(const char *, int *, int *);
struct options_entry *options_match_get(struct options *, const char *, int *,
		     int, int *);
const char	*options_get_string(struct options *, const char *);
long long	 options_get_number(struct options *, const char *);
struct options_entry * printflike(4, 5) options_set_string(struct options *,
		     const char *, int, const char *, ...);
struct options_entry *options_set_number(struct options *, const char *,
		     long long);
int		 options_scope_from_name(struct args *, int,
		     const char *, struct cmd_find_state *, struct options **,
		     char **);
int		 options_scope_from_flags(struct args *, int,
		     struct cmd_find_state *, struct options **, char **);
struct style	*options_string_to_style(struct options *, const char *,
		     struct format_tree *);
int		 options_from_string(struct options *,
		     const struct options_table_entry *, const char *,
		     const char *, int, char **);
int	 	 options_find_choice(const struct options_table_entry *,
		     const char *, char **);
void		 options_push_changes(const char *);
int		 options_remove_or_default(struct options_entry *, int,
		     char **);

/* options-table.c */
extern const struct options_table_entry	options_table[];
extern const struct options_name_map	options_other_names[];

/* job.c */
typedef void (*job_update_cb) (struct job *);
typedef void (*job_complete_cb) (struct job *);
typedef void (*job_free_cb) (void *);
#define JOB_NOWAIT 0x1
#define JOB_KEEPWRITE 0x2
#define JOB_PTY 0x4
struct job	*job_run(const char *, int, char **, struct environ *,
		     struct session *, const char *, job_update_cb,
		     job_complete_cb, job_free_cb, void *, int, int, int);
void		 job_free(struct job *);
int		 job_transfer(struct job *, pid_t *, char *, size_t);
void		 job_resize(struct job *, u_int, u_int);
void		 job_check_died(pid_t, int);
int		 job_get_status(struct job *);
void		*job_get_data(struct job *);
struct bufferevent *job_get_event(struct job *);
void		 job_kill_all(void);
int		 job_still_running(void);
void		 job_print_summary(struct cmdq_item *, int);

/* environ.c */
struct environ *environ_create(void);
void	environ_free(struct environ *);
struct environ_entry *environ_first(struct environ *);
struct environ_entry *environ_next(struct environ_entry *);
void	environ_copy(struct environ *, struct environ *);
struct environ_entry *environ_find(struct environ *, const char *);
void printflike(4, 5) environ_set(struct environ *, const char *, int,
	    const char *, ...);
void	environ_clear(struct environ *, const char *);
void	environ_put(struct environ *, const char *, int);
void	environ_unset(struct environ *, const char *);
void	environ_update(struct options *, struct environ *, struct environ *);
void	environ_push(struct environ *);
void printflike(2, 3) environ_log(struct environ *, const char *, ...);
struct environ *environ_for_session(struct session *, int);

/* tty.c */
void	tty_create_log(void);
int	tty_window_bigger(struct tty *);
int	tty_window_offset(struct tty *, u_int *, u_int *, u_int *, u_int *);
void	tty_update_window_offset(struct window *);
void	tty_update_client_offset(struct client *);
void	tty_raw(struct tty *, const char *);
void	tty_attributes(struct tty *, const struct grid_cell *,
	    const struct grid_cell *, struct colour_palette *,
	    struct hyperlinks *);
void	tty_reset(struct tty *);
void	tty_region_off(struct tty *);
void	tty_margin_off(struct tty *);
void	tty_cursor(struct tty *, u_int, u_int);
void	tty_clipboard_query(struct tty *);
void	tty_putcode(struct tty *, enum tty_code_code);
void	tty_putcode_i(struct tty *, enum tty_code_code, int);
void	tty_putcode_ii(struct tty *, enum tty_code_code, int, int);
void	tty_putcode_iii(struct tty *, enum tty_code_code, int, int, int);
void	tty_putcode_s(struct tty *, enum tty_code_code, const char *);
void	tty_putcode_ss(struct tty *, enum tty_code_code, const char *,
	    const char *);
void	tty_puts(struct tty *, const char *);
void	tty_putc(struct tty *, u_char);
void	tty_putn(struct tty *, const void *, size_t, u_int);
void	tty_cell(struct tty *, const struct grid_cell *,
	    const struct grid_cell *, struct colour_palette *,
	    struct hyperlinks *);
int	tty_init(struct tty *, struct client *);
void	tty_resize(struct tty *);
void	tty_set_size(struct tty *, u_int, u_int, u_int, u_int);
void	tty_start_tty(struct tty *);
void	tty_send_requests(struct tty *);
void	tty_stop_tty(struct tty *);
void	tty_set_title(struct tty *, const char *);
void	tty_set_path(struct tty *, const char *);
void	tty_update_mode(struct tty *, int, struct screen *);
void	tty_draw_line(struct tty *, struct screen *, u_int, u_int, u_int,
	    u_int, u_int, const struct grid_cell *, struct colour_palette *);

#ifdef ENABLE_SIXEL
void	tty_draw_images(struct client *, struct window_pane *, struct screen *);
#endif

void	tty_sync_start(struct tty *);
void	tty_sync_end(struct tty *);
int	tty_open(struct tty *, char **);
void	tty_close(struct tty *);
void	tty_free(struct tty *);
void	tty_update_features(struct tty *);
void	tty_set_selection(struct tty *, const char *, const char *, size_t);
void	tty_write(void (*)(struct tty *, const struct tty_ctx *),
	    struct tty_ctx *);
void	tty_cmd_alignmenttest(struct tty *, const struct tty_ctx *);
void	tty_cmd_cell(struct tty *, const struct tty_ctx *);
void	tty_cmd_cells(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearendofline(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearendofscreen(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearline(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearscreen(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearstartofline(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearstartofscreen(struct tty *, const struct tty_ctx *);
void	tty_cmd_deletecharacter(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearcharacter(struct tty *, const struct tty_ctx *);
void	tty_cmd_deleteline(struct tty *, const struct tty_ctx *);
void	tty_cmd_erasecharacter(struct tty *, const struct tty_ctx *);
void	tty_cmd_insertcharacter(struct tty *, const struct tty_ctx *);
void	tty_cmd_insertline(struct tty *, const struct tty_ctx *);
void	tty_cmd_linefeed(struct tty *, const struct tty_ctx *);
void	tty_cmd_scrollup(struct tty *, const struct tty_ctx *);
void	tty_cmd_scrolldown(struct tty *, const struct tty_ctx *);
void	tty_cmd_reverseindex(struct tty *, const struct tty_ctx *);
void	tty_cmd_setselection(struct tty *, const struct tty_ctx *);
void	tty_cmd_rawstring(struct tty *, const struct tty_ctx *);

#ifdef ENABLE_SIXEL
void	tty_cmd_sixelimage(struct tty *, const struct tty_ctx *);
#endif

void	tty_cmd_syncstart(struct tty *, const struct tty_ctx *);
void	tty_default_colours(struct grid_cell *, struct window_pane *);

/* tty-term.c */
extern struct tty_terms tty_terms;
u_int		 tty_term_ncodes(void);
void		 tty_term_apply(struct tty_term *, const char *, int);
void		 tty_term_apply_overrides(struct tty_term *);
struct tty_term *tty_term_create(struct tty *, char *, char **, u_int, int *,
		     char **);
void		 tty_term_free(struct tty_term *);
int		 tty_term_read_list(const char *, int, char ***, u_int *,
		     char **);
void		 tty_term_free_list(char **, u_int);
int		 tty_term_has(struct tty_term *, enum tty_code_code);
const char	*tty_term_string(struct tty_term *, enum tty_code_code);
const char	*tty_term_string_i(struct tty_term *, enum tty_code_code, int);
const char	*tty_term_string_ii(struct tty_term *, enum tty_code_code, int,
		     int);
const char	*tty_term_string_iii(struct tty_term *, enum tty_code_code, int,
		     int, int);
const char	*tty_term_string_s(struct tty_term *, enum tty_code_code,
		     const char *);
const char	*tty_term_string_ss(struct tty_term *, enum tty_code_code,
		     const char *, const char *);
int		 tty_term_number(struct tty_term *, enum tty_code_code);
int		 tty_term_flag(struct tty_term *, enum tty_code_code);
const char	*tty_term_describe(struct tty_term *, enum tty_code_code);

/* tty-features.c */
void		 tty_add_features(int *, const char *, const char *);
const char	*tty_get_features(int);
int		 tty_apply_features(struct tty_term *, int);
void		 tty_default_features(int *, const char *, u_int);

/* tty-acs.c */
int		 tty_acs_needed(struct tty *);
const char	*tty_acs_get(struct tty *, u_char);
int		 tty_acs_reverse_get(struct tty *, const char *, size_t);
const struct utf8_data *tty_acs_double_borders(int);
const struct utf8_data *tty_acs_heavy_borders(int);
const struct utf8_data *tty_acs_rounded_borders(int);

/* tty-keys.c */
void		tty_keys_build(struct tty *);
void		tty_keys_free(struct tty *);
int		tty_keys_next(struct tty *);

/* arguments.c */
void		 args_set(struct args *, u_char, struct args_value *, int);
struct args 	*args_create(void);
struct args	*args_parse(const struct args_parse *, struct args_value *,
		     u_int, char **);
struct args	*args_copy(struct args *, int, char **);
void		 args_to_vector(struct args *, int *, char ***);
struct args_value *args_from_vector(int, char **);
void		 args_free_value(struct args_value *);
void		 args_free_values(struct args_value *, u_int);
void		 args_free(struct args *);
char		*args_print(struct args *);
char		*args_escape(const char *);
int		 args_has(struct args *, u_char);
const char	*args_get(struct args *, u_char);
u_char		 args_first(struct args *, struct args_entry **);
u_char		 args_next(struct args_entry **);
u_int		 args_count(struct args *);
struct args_value *args_values(struct args *);
struct args_value *args_value(struct args *, u_int);
const char	*args_string(struct args *, u_int);
struct cmd_list	*args_make_commands_now(struct cmd *, struct cmdq_item *,
		     u_int, int);
struct args_command_state *args_make_commands_prepare(struct cmd *,
		     struct cmdq_item *, u_int, const char *, int, int);
struct cmd_list *args_make_commands(struct args_command_state *, int, char **,
		     char **);
void		 args_make_commands_free(struct args_command_state *);
char		*args_make_commands_get_command(struct args_command_state *);
struct args_value *args_first_value(struct args *, u_char);
struct args_value *args_next_value(struct args_value *);
long long	 args_strtonum(struct args *, u_char, long long, long long,
		     char **);
long long	 args_strtonum_and_expand(struct args *, u_char, long long,
		     long long, struct cmdq_item *, char **);
long long	 args_percentage(struct args *, u_char, long long,
		     long long, long long, char **);
long long	 args_string_percentage(const char *, long long, long long,
		     long long, char **);
long long	 args_percentage_and_expand(struct args *, u_char, long long,
		     long long, long long, struct cmdq_item *, char **);
long long	 args_string_percentage_and_expand(const char *, long long,
		     long long, long long, struct cmdq_item *, char **);

/* cmd-find.c */
int		 cmd_find_target(struct cmd_find_state *, struct cmdq_item *,
		     const char *, enum cmd_find_type, int);
struct client	*cmd_find_best_client(struct session *);
struct client	*cmd_find_client(struct cmdq_item *, const char *, int);
void		 cmd_find_clear_state(struct cmd_find_state *, int);
int		 cmd_find_empty_state(struct cmd_find_state *);
int		 cmd_find_valid_state(struct cmd_find_state *);
void		 cmd_find_copy_state(struct cmd_find_state *,
		     struct cmd_find_state *);
void		 cmd_find_from_session(struct cmd_find_state *,
		     struct session *, int);
void		 cmd_find_from_winlink(struct cmd_find_state *,
		     struct winlink *, int);
int		 cmd_find_from_session_window(struct cmd_find_state *,
		     struct session *, struct window *, int);
int		 cmd_find_from_window(struct cmd_find_state *, struct window *,
		     int);
void		 cmd_find_from_winlink_pane(struct cmd_find_state *,
		     struct winlink *, struct window_pane *, int);
int		 cmd_find_from_pane(struct cmd_find_state *,
		     struct window_pane *, int);
int		 cmd_find_from_client(struct cmd_find_state *, struct client *,
		     int);
int		 cmd_find_from_mouse(struct cmd_find_state *,
		     struct mouse_event *, int);
int		 cmd_find_from_nothing(struct cmd_find_state *, int);

/* cmd.c */
extern const struct cmd_entry *cmd_table[];
void printflike(3, 4) cmd_log_argv(int, char **, const char *, ...);
void		 cmd_prepend_argv(int *, char ***, const char *);
void		 cmd_append_argv(int *, char ***, const char *);
int		 cmd_pack_argv(int, char **, char *, size_t);
int		 cmd_unpack_argv(char *, size_t, int, char ***);
char	       **cmd_copy_argv(int, char **);
void		 cmd_free_argv(int, char **);
char		*cmd_stringify_argv(int, char **);
char		*cmd_get_alias(const char *);
const struct cmd_entry *cmd_get_entry(struct cmd *);
struct args	*cmd_get_args(struct cmd *);
u_int		 cmd_get_group(struct cmd *);
void		 cmd_get_source(struct cmd *, const char **, u_int *);
struct cmd	*cmd_parse(struct args_value *, u_int, const char *, u_int,
		     char **);
struct cmd	*cmd_copy(struct cmd *, int, char **);
void		 cmd_free(struct cmd *);
char		*cmd_print(struct cmd *);
struct cmd_list	*cmd_list_new(void);
struct cmd_list	*cmd_list_copy(struct cmd_list *, int, char **);
void		 cmd_list_append(struct cmd_list *, struct cmd *);
void		 cmd_list_append_all(struct cmd_list *, struct cmd_list *);
void		 cmd_list_move(struct cmd_list *, struct cmd_list *);
void		 cmd_list_free(struct cmd_list *);
char		*cmd_list_print(struct cmd_list *, int);
struct cmd	*cmd_list_first(struct cmd_list *);
struct cmd	*cmd_list_next(struct cmd *);
int		 cmd_list_all_have(struct cmd_list *, int);
int		 cmd_list_any_have(struct cmd_list *, int);
int		 cmd_mouse_at(struct window_pane *, struct mouse_event *,
		     u_int *, u_int *, int);
struct winlink	*cmd_mouse_window(struct mouse_event *, struct session **);
struct window_pane *cmd_mouse_pane(struct mouse_event *, struct session **,
		     struct winlink **);
char		*cmd_template_replace(const char *, const char *, int);

/* cmd-attach-session.c */
enum cmd_retval	 cmd_attach_session(struct cmdq_item *, const char *, int, int,
		     int, const char *, int, const char *);

/* cmd-parse.c */
void		 cmd_parse_empty(struct cmd_parse_input *);
struct cmd_parse_result *cmd_parse_from_file(FILE *, struct cmd_parse_input *);
struct cmd_parse_result *cmd_parse_from_string(const char *,
		     struct cmd_parse_input *);
enum cmd_parse_status cmd_parse_and_insert(const char *,
		     struct cmd_parse_input *, struct cmdq_item *,
		     struct cmdq_state *, char **);
enum cmd_parse_status cmd_parse_and_append(const char *,
		     struct cmd_parse_input *, struct client *,
		     struct cmdq_state *, char **);
struct cmd_parse_result *cmd_parse_from_buffer(const void *, size_t,
		     struct cmd_parse_input *);
struct cmd_parse_result *cmd_parse_from_arguments(struct args_value *, u_int,
		     struct cmd_parse_input *);

/* cmd-queue.c */
struct cmdq_state *cmdq_new_state(struct cmd_find_state *, struct key_event *,
		     int);
struct cmdq_state *cmdq_link_state(struct cmdq_state *);
struct cmdq_state *cmdq_copy_state(struct cmdq_state *);
void		  cmdq_free_state(struct cmdq_state *);
void printflike(3, 4) cmdq_add_format(struct cmdq_state *, const char *,
		     const char *, ...);
void		  cmdq_add_formats(struct cmdq_state *, struct format_tree *);
void		  cmdq_merge_formats(struct cmdq_item *, struct format_tree *);
struct cmdq_list *cmdq_new(void);
void cmdq_free(struct cmdq_list *);
const char	 *cmdq_get_name(struct cmdq_item *);
struct client	 *cmdq_get_client(struct cmdq_item *);
struct client	 *cmdq_get_target_client(struct cmdq_item *);
struct cmdq_state *cmdq_get_state(struct cmdq_item *);
struct cmd_find_state *cmdq_get_target(struct cmdq_item *);
struct cmd_find_state *cmdq_get_source(struct cmdq_item *);
struct key_event *cmdq_get_event(struct cmdq_item *);
struct cmd_find_state *cmdq_get_current(struct cmdq_item *);
int		  cmdq_get_flags(struct cmdq_item *);
struct cmdq_item *cmdq_get_command(struct cmd_list *, struct cmdq_state *);
#define cmdq_get_callback(cb, data) cmdq_get_callback1(#cb, cb, data)
struct cmdq_item *cmdq_get_callback1(const char *, cmdq_cb, void *);
struct cmdq_item *cmdq_get_error(const char *);
struct cmdq_item *cmdq_insert_after(struct cmdq_item *, struct cmdq_item *);
struct cmdq_item *cmdq_append(struct client *, struct cmdq_item *);
void printflike(4, 5) cmdq_insert_hook(struct session *, struct cmdq_item *,
		     struct cmd_find_state *, const char *, ...);
void		 cmdq_continue(struct cmdq_item *);
u_int		 cmdq_next(struct client *);
struct cmdq_item *cmdq_running(struct client *);
void		 cmdq_guard(struct cmdq_item *, const char *, int);
void printflike(2, 3) cmdq_print(struct cmdq_item *, const char *, ...);
void 		 cmdq_print_data(struct cmdq_item *, int, struct evbuffer *);
void printflike(2, 3) cmdq_error(struct cmdq_item *, const char *, ...);

/* cmd-wait-for.c */
void	cmd_wait_for_flush(void);

/* client.c */
int	client_main(struct event_base *, int, char **, uint64_t, int);

/* key-bindings.c */
struct key_table *key_bindings_get_table(const char *, int);
struct key_table *key_bindings_first_table(void);
struct key_table *key_bindings_next_table(struct key_table *);
void	 key_bindings_unref_table(struct key_table *);
struct key_binding *key_bindings_get(struct key_table *, key_code);
struct key_binding *key_bindings_get_default(struct key_table *, key_code);
struct key_binding *key_bindings_first(struct key_table *);
struct key_binding *key_bindings_next(struct key_table *, struct key_binding *);
void	 key_bindings_add(const char *, key_code, const char *, int,
	     struct cmd_list *);
void	 key_bindings_remove(const char *, key_code);
void	 key_bindings_reset(const char *, key_code);
void	 key_bindings_remove_table(const char *);
void	 key_bindings_reset_table(const char *);
void	 key_bindings_init(void);
struct cmdq_item *key_bindings_dispatch(struct key_binding *,
	     struct cmdq_item *, struct client *, struct key_event *,
	     struct cmd_find_state *);

/* key-string.c */
key_code	 key_string_lookup_string(const char *);
const char	*key_string_lookup_key(key_code, int);

/* alerts.c */
void	alerts_reset_all(void);
void	alerts_queue(struct window *, int);
void	alerts_check_session(struct session *);

/* file.c */
int	 file_cmp(struct client_file *, struct client_file *);
RB_PROTOTYPE(client_files, client_file, entry, file_cmp);
struct client_file *file_create_with_peer(struct tmuxpeer *,
	    struct client_files *, int, client_file_cb, void *);
struct client_file *file_create_with_client(struct client *, int,
	    client_file_cb, void *);
void	 file_free(struct client_file *);
void	 file_fire_done(struct client_file *);
void	 file_fire_read(struct client_file *);
int	 file_can_print(struct client *);
void printflike(2, 3) file_print(struct client *, const char *, ...);
void printflike(2, 0) file_vprint(struct client *, const char *, va_list);
void	 file_print_buffer(struct client *, void *, size_t);
void printflike(2, 3) file_error(struct client *, const char *, ...);
void	 file_write(struct client *, const char *, int, const void *, size_t,
	     client_file_cb, void *);
struct client_file *file_read(struct client *, const char *, client_file_cb,
	     void *);
void	 file_cancel(struct client_file *);
void	 file_push(struct client_file *);
int	 file_write_left(struct client_files *);
void	 file_write_open(struct client_files *, struct tmuxpeer *,
	     struct imsg *, int, int, client_file_cb, void *);
void	 file_write_data(struct client_files *, struct imsg *);
void	 file_write_close(struct client_files *, struct imsg *);
void	 file_read_open(struct client_files *, struct tmuxpeer *, struct imsg *,
	     int, int, client_file_cb, void *);
void	 file_write_ready(struct client_files *, struct imsg *);
void	 file_read_data(struct client_files *, struct imsg *);
void	 file_read_done(struct client_files *, struct imsg *);
void	 file_read_cancel(struct client_files *, struct imsg *);

/* server.c */
extern struct tmuxproc *server_proc;
extern struct clients clients;
extern struct cmd_find_state marked_pane;
extern struct message_list message_log;
extern time_t current_time;
void	 server_set_marked(struct session *, struct winlink *,
	     struct window_pane *);
void	 server_clear_marked(void);
int	 server_is_marked(struct session *, struct winlink *,
	     struct window_pane *);
int	 server_check_marked(void);
int	 server_start(struct tmuxproc *, int, struct event_base *, int, char *);
void	 server_update_socket(void);
void	 server_add_accept(int);
void printflike(1, 2) server_add_message(const char *, ...);
int	 server_create_socket(int, char **);

/* server-client.c */
RB_PROTOTYPE(client_windows, client_window, entry, server_client_window_cmp);
u_int	 server_client_how_many(void);
void	 server_client_set_overlay(struct client *, u_int, overlay_check_cb,
	     overlay_mode_cb, overlay_draw_cb, overlay_key_cb,
	     overlay_free_cb, overlay_resize_cb, void *);
void	 server_client_clear_overlay(struct client *);
void	 server_client_overlay_range(u_int, u_int, u_int, u_int, u_int, u_int,
	     u_int, struct overlay_ranges *);
void	 server_client_set_key_table(struct client *, const char *);
const char *server_client_get_key_table(struct client *);
int	 server_client_check_nested(struct client *);
int	 server_client_handle_key(struct client *, struct key_event *);
struct client *server_client_create(int);
int	 server_client_open(struct client *, char **);
void	 server_client_unref(struct client *);
void	 server_client_set_session(struct client *, struct session *);
void	 server_client_lost(struct client *);
void	 server_client_suspend(struct client *);
void	 server_client_detach(struct client *, enum msgtype);
void	 server_client_exec(struct client *, const char *);
void	 server_client_loop(void);
void	 server_client_push_stdout(struct client *);
void	 server_client_push_stderr(struct client *);
const char *server_client_get_cwd(struct client *, struct session *);
void	 server_client_set_flags(struct client *, const char *);
const char *server_client_get_flags(struct client *);
struct client_window *server_client_get_client_window(struct client *, u_int);
struct client_window *server_client_add_client_window(struct client *, u_int);
struct window_pane *server_client_get_pane(struct client *);
void	 server_client_set_pane(struct client *, struct window_pane *);
void	 server_client_remove_pane(struct window_pane *);
void	 server_client_print(struct client *, int, struct evbuffer *);

/* server-fn.c */
void	 server_redraw_client(struct client *);
void	 server_status_client(struct client *);
void	 server_redraw_session(struct session *);
void	 server_redraw_session_group(struct session *);
void	 server_status_session(struct session *);
void	 server_status_session_group(struct session *);
void	 server_redraw_window(struct window *);
void	 server_redraw_window_borders(struct window *);
void	 server_status_window(struct window *);
void	 server_lock(void);
void	 server_lock_session(struct session *);
void	 server_lock_client(struct client *);
void	 server_kill_pane(struct window_pane *);
void	 server_kill_window(struct window *, int);
void	 server_renumber_session(struct session *);
void	 server_renumber_all(void);
int	 server_link_window(struct session *,
	     struct winlink *, struct session *, int, int, int, char **);
void	 server_unlink_window(struct session *, struct winlink *);
void	 server_destroy_pane(struct window_pane *, int);
void	 server_destroy_session(struct session *);
void	 server_check_unattached(void);
void	 server_unzoom_window(struct window *);

/* status.c */
extern char	**status_prompt_hlist[];
extern u_int	  status_prompt_hsize[];
void	 status_timer_start(struct client *);
void	 status_timer_start_all(void);
void	 status_update_cache(struct session *);
int	 status_at_line(struct client *);
u_int	 status_line_size(struct client *);
struct style_range *status_get_range(struct client *, u_int, u_int);
void	 status_init(struct client *);
void	 status_free(struct client *);
int	 status_redraw(struct client *);
void printflike(5, 6) status_message_set(struct client *, int, int, int,
	     const char *, ...);
void	 status_message_clear(struct client *);
int	 status_message_redraw(struct client *);
void	 status_prompt_set(struct client *, struct cmd_find_state *,
	     const char *, const char *, prompt_input_cb, prompt_free_cb,
	     void *, int, enum prompt_type);
void	 status_prompt_clear(struct client *);
int	 status_prompt_redraw(struct client *);
int	 status_prompt_key(struct client *, key_code);
void	 status_prompt_update(struct client *, const char *, const char *);
void	 status_prompt_load_history(void);
void	 status_prompt_save_history(void);
const char *status_prompt_type_string(u_int);
enum prompt_type status_prompt_type(const char *type);

/* resize.c */
void	 resize_window(struct window *, u_int, u_int, int, int);
void	 default_window_size(struct client *, struct session *, struct window *,
	     u_int *, u_int *, u_int *, u_int *, int);
void	 recalculate_size(struct window *, int);
void	 recalculate_sizes(void);
void	 recalculate_sizes_now(int);

/* input.c */
struct input_ctx *input_init(struct window_pane *, struct bufferevent *,
	     struct colour_palette *);
void	 input_free(struct input_ctx *);
void	 input_reset(struct input_ctx *, int);
struct evbuffer *input_pending(struct input_ctx *);
void	 input_parse_pane(struct window_pane *);
void	 input_parse_buffer(struct window_pane *, u_char *, size_t);
void	 input_parse_screen(struct input_ctx *, struct screen *,
	     screen_write_init_ctx_cb, void *, u_char *, size_t);
void	 input_reply_clipboard(struct bufferevent *, const char *, size_t,
	     const char *);

/* input-key.c */
void	 input_key_build(void);
int	 input_key_pane(struct window_pane *, key_code, struct mouse_event *);
int	 input_key(struct screen *, struct bufferevent *, key_code);
int	 input_key_get_mouse(struct screen *, struct mouse_event *, u_int,
	     u_int, const char **, size_t *);

/* colour.c */
int	 colour_find_rgb(u_char, u_char, u_char);
int	 colour_join_rgb(u_char, u_char, u_char);
void	 colour_split_rgb(int, u_char *, u_char *, u_char *);
int	 colour_force_rgb(int);
const char *colour_tostring(int);
int	 colour_fromstring(const char *s);
int	 colour_256toRGB(int);
int	 colour_256to16(int);
int	 colour_byname(const char *);
int	 colour_parseX11(const char *);
void	 colour_palette_init(struct colour_palette *);
void	 colour_palette_clear(struct colour_palette *);
void	 colour_palette_free(struct colour_palette *);
int	 colour_palette_get(struct colour_palette *, int);
int	 colour_palette_set(struct colour_palette *, int, int);
void	 colour_palette_from_option(struct colour_palette *, struct options *);

/* attributes.c */
const char *attributes_tostring(int);
int	 attributes_fromstring(const char *);

/* grid.c */
extern const struct grid_cell grid_default_cell;
void	 grid_empty_line(struct grid *, u_int, u_int);
int	 grid_cells_equal(const struct grid_cell *, const struct grid_cell *);
int	 grid_cells_look_equal(const struct grid_cell *,
	     const struct grid_cell *);
struct grid *grid_create(u_int, u_int, u_int);
void	 grid_destroy(struct grid *);
int	 grid_compare(struct grid *, struct grid *);
void	 grid_collect_history(struct grid *);
void	 grid_remove_history(struct grid *, u_int );
void	 grid_scroll_history(struct grid *, u_int);
void	 grid_scroll_history_region(struct grid *, u_int, u_int, u_int);
void	 grid_clear_history(struct grid *);
const struct grid_line *grid_peek_line(struct grid *, u_int);
void	 grid_get_cell(struct grid *, u_int, u_int, struct grid_cell *);
void	 grid_set_cell(struct grid *, u_int, u_int, const struct grid_cell *);
void	 grid_set_padding(struct grid *, u_int, u_int);
void	 grid_set_cells(struct grid *, u_int, u_int, const struct grid_cell *,
	     const char *, size_t);
struct grid_line *grid_get_line(struct grid *, u_int);
void	 grid_adjust_lines(struct grid *, u_int);
void	 grid_clear(struct grid *, u_int, u_int, u_int, u_int, u_int);
void	 grid_clear_lines(struct grid *, u_int, u_int, u_int);
void	 grid_move_lines(struct grid *, u_int, u_int, u_int, u_int);
void	 grid_move_cells(struct grid *, u_int, u_int, u_int, u_int, u_int);
char	*grid_string_cells(struct grid *, u_int, u_int, u_int,
	     struct grid_cell **, int, struct screen *);
void	 grid_duplicate_lines(struct grid *, u_int, struct grid *, u_int,
	     u_int);
void	 grid_reflow(struct grid *, u_int);
void	 grid_wrap_position(struct grid *, u_int, u_int, u_int *, u_int *);
void	 grid_unwrap_position(struct grid *, u_int *, u_int *, u_int, u_int);
u_int	 grid_line_length(struct grid *, u_int);

/* grid-reader.c */
void	 grid_reader_start(struct grid_reader *, struct grid *, u_int, u_int);
void	 grid_reader_get_cursor(struct grid_reader *, u_int *, u_int *);
u_int	 grid_reader_line_length(struct grid_reader *);
int	 grid_reader_in_set(struct grid_reader *, const char *);
void	 grid_reader_cursor_right(struct grid_reader *, int, int);
void	 grid_reader_cursor_left(struct grid_reader *, int);
void	 grid_reader_cursor_down(struct grid_reader *);
void	 grid_reader_cursor_up(struct grid_reader *);
void	 grid_reader_cursor_start_of_line(struct grid_reader *, int);
void	 grid_reader_cursor_end_of_line(struct grid_reader *, int, int);
void	 grid_reader_cursor_next_word(struct grid_reader *, const char *);
void	 grid_reader_cursor_next_word_end(struct grid_reader *, const char *);
void	 grid_reader_cursor_previous_word(struct grid_reader *, const char *,
	     int, int);
int	 grid_reader_cursor_jump(struct grid_reader *,
	     const struct utf8_data *);
int	 grid_reader_cursor_jump_back(struct grid_reader *,
	     const struct utf8_data *);
void	 grid_reader_cursor_back_to_indentation(struct grid_reader *);

/* grid-view.c */
void	 grid_view_get_cell(struct grid *, u_int, u_int, struct grid_cell *);
void	 grid_view_set_cell(struct grid *, u_int, u_int,
	     const struct grid_cell *);
void	 grid_view_set_padding(struct grid *, u_int, u_int);
void	 grid_view_set_cells(struct grid *, u_int, u_int,
	     const struct grid_cell *, const char *, size_t);
void	 grid_view_clear_history(struct grid *, u_int);
void	 grid_view_clear(struct grid *, u_int, u_int, u_int, u_int, u_int);
void	 grid_view_scroll_region_up(struct grid *, u_int, u_int, u_int);
void	 grid_view_scroll_region_down(struct grid *, u_int, u_int, u_int);
void	 grid_view_insert_lines(struct grid *, u_int, u_int, u_int);
void	 grid_view_insert_lines_region(struct grid *, u_int, u_int, u_int,
	     u_int);
void	 grid_view_delete_lines(struct grid *, u_int, u_int, u_int);
void	 grid_view_delete_lines_region(struct grid *, u_int, u_int, u_int,
	     u_int);
void	 grid_view_insert_cells(struct grid *, u_int, u_int, u_int, u_int);
void	 grid_view_delete_cells(struct grid *, u_int, u_int, u_int, u_int);
char	*grid_view_string_cells(struct grid *, u_int, u_int, u_int);

/* screen-write.c */
void	 screen_write_make_list(struct screen *);
void	 screen_write_free_list(struct screen *);
void	 screen_write_start_pane(struct screen_write_ctx *,
	     struct window_pane *, struct screen *);
void	 screen_write_start(struct screen_write_ctx *, struct screen *);
void	 screen_write_start_callback(struct screen_write_ctx *, struct screen *,
	     screen_write_init_ctx_cb, void *);
void	 screen_write_stop(struct screen_write_ctx *);
void	 screen_write_reset(struct screen_write_ctx *);
size_t printflike(1, 2) screen_write_strlen(const char *, ...);
int printflike(7, 8) screen_write_text(struct screen_write_ctx *, u_int, u_int,
	     u_int, int, const struct grid_cell *, const char *, ...);
void printflike(3, 4) screen_write_puts(struct screen_write_ctx *,
	     const struct grid_cell *, const char *, ...);
void printflike(4, 5) screen_write_nputs(struct screen_write_ctx *,
	     ssize_t, const struct grid_cell *, const char *, ...);
void printflike(4, 0) screen_write_vnputs(struct screen_write_ctx *, ssize_t,
	     const struct grid_cell *, const char *, va_list);
void	 screen_write_putc(struct screen_write_ctx *, const struct grid_cell *,
	     u_char);
void	 screen_write_fast_copy(struct screen_write_ctx *, struct screen *,
	     u_int, u_int, u_int, u_int);
void	 screen_write_hline(struct screen_write_ctx *, u_int, int, int,
	     enum box_lines, const struct grid_cell *);
void	 screen_write_vline(struct screen_write_ctx *, u_int, int, int);
void	 screen_write_menu(struct screen_write_ctx *, struct menu *, int,
	     enum box_lines, const struct grid_cell *, const struct grid_cell *,
	     const struct grid_cell *);
void	 screen_write_box(struct screen_write_ctx *, u_int, u_int,
             enum box_lines, const struct grid_cell *, const char *);
void	 screen_write_preview(struct screen_write_ctx *, struct screen *, u_int,
	     u_int);
void	 screen_write_backspace(struct screen_write_ctx *);
void	 screen_write_mode_set(struct screen_write_ctx *, int);
void	 screen_write_mode_clear(struct screen_write_ctx *, int);
void	 screen_write_cursorup(struct screen_write_ctx *, u_int);
void	 screen_write_cursordown(struct screen_write_ctx *, u_int);
void	 screen_write_cursorright(struct screen_write_ctx *, u_int);
void	 screen_write_cursorleft(struct screen_write_ctx *, u_int);
void	 screen_write_alignmenttest(struct screen_write_ctx *);
void	 screen_write_insertcharacter(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_deletecharacter(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_clearcharacter(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_insertline(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_deleteline(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_clearline(struct screen_write_ctx *, u_int);
void	 screen_write_clearendofline(struct screen_write_ctx *, u_int);
void	 screen_write_clearstartofline(struct screen_write_ctx *, u_int);
void	 screen_write_cursormove(struct screen_write_ctx *, int, int, int);
void	 screen_write_reverseindex(struct screen_write_ctx *, u_int);
void	 screen_write_scrollregion(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_linefeed(struct screen_write_ctx *, int, u_int);
void	 screen_write_scrollup(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_scrolldown(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_carriagereturn(struct screen_write_ctx *);
void	 screen_write_clearendofscreen(struct screen_write_ctx *, u_int);
void	 screen_write_clearstartofscreen(struct screen_write_ctx *, u_int);
void	 screen_write_clearscreen(struct screen_write_ctx *, u_int);
void	 screen_write_clearhistory(struct screen_write_ctx *);
void	 screen_write_fullredraw(struct screen_write_ctx *);
void	 screen_write_collect_end(struct screen_write_ctx *);
void	 screen_write_collect_add(struct screen_write_ctx *,
	     const struct grid_cell *);
void	 screen_write_cell(struct screen_write_ctx *, const struct grid_cell *);
void	 screen_write_setselection(struct screen_write_ctx *, const char *,
	     u_char *, u_int);
void	 screen_write_rawstring(struct screen_write_ctx *, u_char *, u_int,
	     int);
#ifdef ENABLE_SIXEL
void	 screen_write_sixelimage(struct screen_write_ctx *,
	     struct sixel_image *, u_int);
#endif
void	 screen_write_alternateon(struct screen_write_ctx *,
	     struct grid_cell *, int);
void	 screen_write_alternateoff(struct screen_write_ctx *,
	     struct grid_cell *, int);

/* screen-redraw.c */
void	 screen_redraw_screen(struct client *);
void	 screen_redraw_pane(struct client *, struct window_pane *);

/* screen.c */
void	 screen_init(struct screen *, u_int, u_int, u_int);
void	 screen_reinit(struct screen *);
void	 screen_free(struct screen *);
void	 screen_reset_tabs(struct screen *);
void	 screen_reset_hyperlinks(struct screen *);
void	 screen_set_cursor_style(u_int, enum screen_cursor_style *, int *);
void	 screen_set_cursor_colour(struct screen *, int);
int	 screen_set_title(struct screen *, const char *);
void	 screen_set_path(struct screen *, const char *);
void	 screen_push_title(struct screen *);
void	 screen_pop_title(struct screen *);
void	 screen_resize(struct screen *, u_int, u_int, int);
void	 screen_resize_cursor(struct screen *, u_int, u_int, int, int, int);
void	 screen_set_selection(struct screen *, u_int, u_int, u_int, u_int,
	     u_int, int, struct grid_cell *);
void	 screen_clear_selection(struct screen *);
void	 screen_hide_selection(struct screen *);
int	 screen_check_selection(struct screen *, u_int, u_int);
void	 screen_select_cell(struct screen *, struct grid_cell *,
	     const struct grid_cell *);
void	 screen_alternate_on(struct screen *, struct grid_cell *, int);
void	 screen_alternate_off(struct screen *, struct grid_cell *, int);
const char *screen_mode_to_string(int);

/* window.c */
extern struct windows windows;
extern struct window_pane_tree all_window_panes;
int		 window_cmp(struct window *, struct window *);
RB_PROTOTYPE(windows, window, entry, window_cmp);
int		 winlink_cmp(struct winlink *, struct winlink *);
RB_PROTOTYPE(winlinks, winlink, entry, winlink_cmp);
int		 window_pane_cmp(struct window_pane *, struct window_pane *);
RB_PROTOTYPE(window_pane_tree, window_pane, tree_entry, window_pane_cmp);
struct winlink	*winlink_find_by_index(struct winlinks *, int);
struct winlink	*winlink_find_by_window(struct winlinks *, struct window *);
struct winlink	*winlink_find_by_window_id(struct winlinks *, u_int);
u_int		 winlink_count(struct winlinks *);
struct winlink	*winlink_add(struct winlinks *, int);
void		 winlink_set_window(struct winlink *, struct window *);
void		 winlink_remove(struct winlinks *, struct winlink *);
struct winlink	*winlink_next(struct winlink *);
struct winlink	*winlink_previous(struct winlink *);
struct winlink	*winlink_next_by_number(struct winlink *, struct session *,
		     int);
struct winlink	*winlink_previous_by_number(struct winlink *, struct session *,
		     int);
void		 winlink_stack_push(struct winlink_stack *, struct winlink *);
void		 winlink_stack_remove(struct winlink_stack *, struct winlink *);
struct window	*window_find_by_id_str(const char *);
struct window	*window_find_by_id(u_int);
void		 window_update_activity(struct window *);
struct window	*window_create(u_int, u_int, u_int, u_int);
void		 window_pane_set_event(struct window_pane *);
struct window_pane *window_get_active_at(struct window *, u_int, u_int);
struct window_pane *window_find_string(struct window *, const char *);
int		 window_has_pane(struct window *, struct window_pane *);
int		 window_set_active_pane(struct window *, struct window_pane *,
		     int);
void		 window_update_focus(struct window *);
void		 window_pane_update_focus(struct window_pane *);
void		 window_redraw_active_switch(struct window *,
		     struct window_pane *);
struct window_pane *window_add_pane(struct window *, struct window_pane *,
		     u_int, int);
void		 window_resize(struct window *, u_int, u_int, int, int);
void		 window_pane_send_resize(struct window_pane *, u_int, u_int);
int		 window_zoom(struct window_pane *);
int		 window_unzoom(struct window *);
int		 window_push_zoom(struct window *, int, int);
int		 window_pop_zoom(struct window *);
void		 window_lost_pane(struct window *, struct window_pane *);
void		 window_remove_pane(struct window *, struct window_pane *);
struct window_pane *window_pane_at_index(struct window *, u_int);
struct window_pane *window_pane_next_by_number(struct window *,
			struct window_pane *, u_int);
struct window_pane *window_pane_previous_by_number(struct window *,
			struct window_pane *, u_int);
int		 window_pane_index(struct window_pane *, u_int *);
u_int		 window_count_panes(struct window *);
void		 window_destroy_panes(struct window *);
struct window_pane *window_pane_find_by_id_str(const char *);
struct window_pane *window_pane_find_by_id(u_int);
int		 window_pane_destroy_ready(struct window_pane *);
void		 window_pane_resize(struct window_pane *, u_int, u_int);
int		 window_pane_set_mode(struct window_pane *,
		     struct window_pane *, const struct window_mode *,
		     struct cmd_find_state *, struct args *);
void		 window_pane_reset_mode(struct window_pane *);
void		 window_pane_reset_mode_all(struct window_pane *);
int		 window_pane_key(struct window_pane *, struct client *,
		     struct session *, struct winlink *, key_code,
		     struct mouse_event *);
int		 window_pane_visible(struct window_pane *);
u_int		 window_pane_search(struct window_pane *, const char *, int,
		     int);
const char	*window_printable_flags(struct winlink *, int);
struct window_pane *window_pane_find_up(struct window_pane *);
struct window_pane *window_pane_find_down(struct window_pane *);
struct window_pane *window_pane_find_left(struct window_pane *);
struct window_pane *window_pane_find_right(struct window_pane *);
void		 window_pane_stack_push(struct window_panes *,
		     struct window_pane *);
void		 window_pane_stack_remove(struct window_panes *,
		     struct window_pane *);
void		 window_set_name(struct window *, const char *);
void		 window_add_ref(struct window *, const char *);
void		 window_remove_ref(struct window *, const char *);
void		 winlink_clear_flags(struct winlink *);
int		 winlink_shuffle_up(struct session *, struct winlink *, int);
int		 window_pane_start_input(struct window_pane *,
		     struct cmdq_item *, char **);
void		*window_pane_get_new_data(struct window_pane *,
		     struct window_pane_offset *, size_t *);
void		 window_pane_update_used_data(struct window_pane *,
		     struct window_pane_offset *, size_t);
void		 window_set_fill_character(struct window *);
void		 window_pane_default_cursor(struct window_pane *);

/* layout.c */
u_int		 layout_count_cells(struct layout_cell *);
struct layout_cell *layout_create_cell(struct layout_cell *);
void		 layout_free_cell(struct layout_cell *);
void		 layout_print_cell(struct layout_cell *, const char *, u_int);
void		 layout_destroy_cell(struct window *, struct layout_cell *,
		     struct layout_cell **);
void		 layout_resize_layout(struct window *, struct layout_cell *,
		     enum layout_type, int, int);
struct layout_cell *layout_search_by_border(struct layout_cell *, u_int, u_int);
void		 layout_set_size(struct layout_cell *, u_int, u_int, u_int,
		     u_int);
void		 layout_make_leaf(struct layout_cell *, struct window_pane *);
void		 layout_make_node(struct layout_cell *, enum layout_type);
void		 layout_fix_offsets(struct window *);
void		 layout_fix_panes(struct window *, struct window_pane *);
void		 layout_resize_adjust(struct window *, struct layout_cell *,
		     enum layout_type, int);
void		 layout_init(struct window *, struct window_pane *);
void		 layout_free(struct window *);
void		 layout_resize(struct window *, u_int, u_int);
void		 layout_resize_pane(struct window_pane *, enum layout_type,
		     int, int);
void		 layout_resize_pane_to(struct window_pane *, enum layout_type,
		     u_int);
void		 layout_assign_pane(struct layout_cell *, struct window_pane *,
		     int);
struct layout_cell *layout_split_pane(struct window_pane *, enum layout_type,
		     int, int);
void		 layout_close_pane(struct window_pane *);
int		 layout_spread_cell(struct window *, struct layout_cell *);
void		 layout_spread_out(struct window_pane *);

/* layout-custom.c */
char		*layout_dump(struct layout_cell *);
int		 layout_parse(struct window *, const char *, char **);

/* layout-set.c */
int		 layout_set_lookup(const char *);
u_int		 layout_set_select(struct window *, u_int);
u_int		 layout_set_next(struct window *);
u_int		 layout_set_previous(struct window *);

/* mode-tree.c */
typedef void (*mode_tree_build_cb)(void *, struct mode_tree_sort_criteria *,
				   uint64_t *, const char *);
typedef void (*mode_tree_draw_cb)(void *, void *, struct screen_write_ctx *,
	     u_int, u_int);
typedef int (*mode_tree_search_cb)(void *, void *, const char *);
typedef void (*mode_tree_menu_cb)(void *, struct client *, key_code);
typedef u_int (*mode_tree_height_cb)(void *, u_int);
typedef key_code (*mode_tree_key_cb)(void *, void *, u_int);
typedef void (*mode_tree_each_cb)(void *, void *, struct client *, key_code);
u_int	 mode_tree_count_tagged(struct mode_tree_data *);
void	*mode_tree_get_current(struct mode_tree_data *);
const char *mode_tree_get_current_name(struct mode_tree_data *);
void	 mode_tree_expand_current(struct mode_tree_data *);
void	 mode_tree_collapse_current(struct mode_tree_data *);
void	 mode_tree_expand(struct mode_tree_data *, uint64_t);
int	 mode_tree_set_current(struct mode_tree_data *, uint64_t);
void	 mode_tree_each_tagged(struct mode_tree_data *, mode_tree_each_cb,
	     struct client *, key_code, int);
void	 mode_tree_up(struct mode_tree_data *, int);
void	 mode_tree_down(struct mode_tree_data *, int);
struct mode_tree_data *mode_tree_start(struct window_pane *, struct args *,
	     mode_tree_build_cb, mode_tree_draw_cb, mode_tree_search_cb,
	     mode_tree_menu_cb, mode_tree_height_cb, mode_tree_key_cb, void *,
	     const struct menu_item *, const char **, u_int, struct screen **);
void	 mode_tree_zoom(struct mode_tree_data *, struct args *);
void	 mode_tree_build(struct mode_tree_data *);
void	 mode_tree_free(struct mode_tree_data *);
void	 mode_tree_resize(struct mode_tree_data *, u_int, u_int);
struct mode_tree_item *mode_tree_add(struct mode_tree_data *,
	     struct mode_tree_item *, void *, uint64_t, const char *,
	     const char *, int);
void	 mode_tree_draw_as_parent(struct mode_tree_item *);
void	 mode_tree_no_tag(struct mode_tree_item *);
void	 mode_tree_remove(struct mode_tree_data *, struct mode_tree_item *);
void	 mode_tree_draw(struct mode_tree_data *);
int	 mode_tree_key(struct mode_tree_data *, struct client *, key_code *,
	     struct mouse_event *, u_int *, u_int *);
void	 mode_tree_run_command(struct client *, struct cmd_find_state *,
	     const char *, const char *);

/* window-buffer.c */
extern const struct window_mode window_buffer_mode;

/* window-tree.c */
extern const struct window_mode window_tree_mode;

/* window-clock.c */
extern const struct window_mode window_clock_mode;
extern const char window_clock_table[14][5][5];

/* window-client.c */
extern const struct window_mode window_client_mode;

/* window-copy.c */
extern const struct window_mode window_copy_mode;
extern const struct window_mode window_view_mode;
void printflike(3, 4) window_copy_add(struct window_pane *, int, const char *,
		     ...);
void printflike(3, 0) window_copy_vadd(struct window_pane *, int, const char *,
		     va_list);
void		 window_copy_pageup(struct window_pane *, int);
void		 window_copy_start_drag(struct client *, struct mouse_event *);
char		*window_copy_get_word(struct window_pane *, u_int, u_int);
char		*window_copy_get_line(struct window_pane *, u_int);

/* window-option.c */
extern const struct window_mode window_customize_mode;

/* names.c */
void	 check_window_name(struct window *);
char	*default_window_name(struct window *);
char	*parse_window_name(const char *);

/* control.c */
void	control_discard(struct client *);
void	control_start(struct client *);
void	control_ready(struct client *);
void	control_stop(struct client *);
void	control_set_pane_on(struct client *, struct window_pane *);
void	control_set_pane_off(struct client *, struct window_pane *);
void	control_continue_pane(struct client *, struct window_pane *);
void	control_pause_pane(struct client *, struct window_pane *);
struct window_pane_offset *control_pane_offset(struct client *,
	   struct window_pane *, int *);
void	control_reset_offsets(struct client *);
void printflike(2, 3) control_write(struct client *, const char *, ...);
void	control_write_output(struct client *, struct window_pane *);
int	control_all_done(struct client *);
void	control_add_sub(struct client *, const char *, enum control_sub_type,
    	   int, const char *);
void	control_remove_sub(struct client *, const char *);

/* control-notify.c */
void	control_notify_input(struct client *, struct window_pane *,
	    const u_char *, size_t);
void	control_notify_pane_mode_changed(int);
void	control_notify_window_layout_changed(struct window *);
void	control_notify_window_pane_changed(struct window *);
void	control_notify_window_unlinked(struct session *, struct window *);
void	control_notify_window_linked(struct session *, struct window *);
void	control_notify_window_renamed(struct window *);
void	control_notify_client_session_changed(struct client *);
void	control_notify_client_detached(struct client *);
void	control_notify_session_renamed(struct session *);
void	control_notify_session_created(struct session *);
void	control_notify_session_closed(struct session *);
void	control_notify_session_window_changed(struct session *);
void	control_notify_paste_buffer_changed(const char *);
void	control_notify_paste_buffer_deleted(const char *);

/* session.c */
extern struct sessions sessions;
extern u_int next_session_id;
int	session_cmp(struct session *, struct session *);
RB_PROTOTYPE(sessions, session, entry, session_cmp);
int		 session_alive(struct session *);
struct session	*session_find(const char *);
struct session	*session_find_by_id_str(const char *);
struct session	*session_find_by_id(u_int);
struct session	*session_create(const char *, const char *, const char *,
		     struct environ *, struct options *, struct termios *);
void		 session_destroy(struct session *, int,	 const char *);
void		 session_add_ref(struct session *, const char *);
void		 session_remove_ref(struct session *, const char *);
char		*session_check_name(const char *);
void		 session_update_activity(struct session *, struct timeval *);
struct session	*session_next_session(struct session *);
struct session	*session_previous_session(struct session *);
struct winlink	*session_new(struct session *, const char *, int, char **,
		     const char *, const char *, int, char **);
struct winlink	*session_attach(struct session *, struct window *, int,
		     char **);
int		 session_detach(struct session *, struct winlink *);
int		 session_has(struct session *, struct window *);
int		 session_is_linked(struct session *, struct window *);
int		 session_next(struct session *, int);
int		 session_previous(struct session *, int);
int		 session_select(struct session *, int);
int		 session_last(struct session *);
int		 session_set_current(struct session *, struct winlink *);
struct session_group *session_group_contains(struct session *);
struct session_group *session_group_find(const char *);
struct session_group *session_group_new(const char *);
void		 session_group_add(struct session_group *, struct session *);
void		 session_group_synchronize_to(struct session *);
void		 session_group_synchronize_from(struct session *);
u_int		 session_group_count(struct session_group *);
u_int		 session_group_attached_count(struct session_group *);
void		 session_renumber_windows(struct session *);

/* utf8.c */
utf8_char	 utf8_build_one(u_char);
enum utf8_state	 utf8_from_data(const struct utf8_data *, utf8_char *);
void		 utf8_to_data(utf8_char, struct utf8_data *);
void		 utf8_set(struct utf8_data *, u_char);
void		 utf8_copy(struct utf8_data *, const struct utf8_data *);
enum utf8_state	 utf8_open(struct utf8_data *, u_char);
enum utf8_state	 utf8_append(struct utf8_data *, u_char);
int		 utf8_isvalid(const char *);
int		 utf8_strvis(char *, const char *, size_t, int);
int		 utf8_stravis(char **, const char *, int);
int		 utf8_stravisx(char **, const char *, size_t, int);
char		*utf8_sanitize(const char *);
size_t		 utf8_strlen(const struct utf8_data *);
u_int		 utf8_strwidth(const struct utf8_data *, ssize_t);
struct utf8_data *utf8_fromcstr(const char *);
char		*utf8_tocstr(struct utf8_data *);
u_int		 utf8_cstrwidth(const char *);
char		*utf8_padcstr(const char *, u_int);
char		*utf8_rpadcstr(const char *, u_int);
int		 utf8_cstrhas(const char *, const struct utf8_data *);

/* osdep-*.c */
char		*osdep_get_name(int, char *);
char		*osdep_get_cwd(int);
struct event_base *osdep_event_init(void);
/* utf8-combined.c */
void		 utf8_build_combined(void);
int		 utf8_try_combined(const struct utf8_data *,
		     const struct utf8_data *, const struct utf8_data **,
		     u_int *width);

/* procname.c */
char   *get_proc_name(int, char *);
char   *get_proc_cwd(int);

/* log.c */
void	log_add_level(void);
int	log_get_level(void);
void	log_open(const char *);
void	log_toggle(const char *);
void	log_close(void);
void printflike(1, 2) log_debug(const char *, ...);
__dead void printflike(1, 2) fatal(const char *, ...);
__dead void printflike(1, 2) fatalx(const char *, ...);

/* menu.c */
#define MENU_NOMOUSE 0x1
#define MENU_TAB 0x2
#define MENU_STAYOPEN 0x4
struct menu	*menu_create(const char *);
void		 menu_add_items(struct menu *, const struct menu_item *,
		    struct cmdq_item *, struct client *,
		    struct cmd_find_state *);
void		 menu_add_item(struct menu *, const struct menu_item *,
		    struct cmdq_item *, struct client *,
		    struct cmd_find_state *);
void		 menu_free(struct menu *);
struct menu_data *menu_prepare(struct menu *, int, int, struct cmdq_item *,
		    u_int, u_int, struct client *, enum box_lines, const char *,
		    const char *, const char *, struct cmd_find_state *,
		    menu_choice_cb, void *);
int		 menu_display(struct menu *, int, int, struct cmdq_item *,
		    u_int, u_int, struct client *, enum box_lines, const char *,
		    const char *, const char *, struct cmd_find_state *,
		    menu_choice_cb, void *);
struct screen	*menu_mode_cb(struct client *, void *, u_int *, u_int *);
void		 menu_check_cb(struct client *, void *, u_int, u_int, u_int,
		    struct overlay_ranges *);
void		 menu_draw_cb(struct client *, void *,
		    struct screen_redraw_ctx *);
void		 menu_free_cb(struct client *, void *);
int		 menu_key_cb(struct client *, void *, struct key_event *);

/* popup.c */
#define POPUP_CLOSEEXIT 0x1
#define POPUP_CLOSEEXITZERO 0x2
#define POPUP_INTERNAL 0x4
typedef void (*popup_close_cb)(int, void *);
typedef void (*popup_finish_edit_cb)(char *, size_t, void *);
int		 popup_display(int, enum box_lines, struct cmdq_item *, u_int,
                    u_int, u_int, u_int, struct environ *, const char *, int,
                    char **, const char *, const char *, struct client *,
                    struct session *, const char *, const char *,
                    popup_close_cb, void *);
int		 popup_editor(struct client *, const char *, size_t,
		    popup_finish_edit_cb, void *);

/* style.c */
int		 style_parse(struct style *,const struct grid_cell *,
		     const char *);
const char	*style_tostring(struct style *);
void		 style_add(struct grid_cell *, struct options *,
		     const char *, struct format_tree *);
void		 style_apply(struct grid_cell *, struct options *,
		     const char *, struct format_tree *);
void		 style_set(struct style *, const struct grid_cell *);
void		 style_copy(struct style *, struct style *);

/* spawn.c */
struct winlink	*spawn_window(struct spawn_context *, char **);
struct window_pane *spawn_pane(struct spawn_context *, char **);

/* regsub.c */
char		*regsub(const char *, const char *, const char *, int);

#ifdef ENABLE_SIXEL
/* image.c */
int		 image_free_all(struct screen *);
struct image	*image_store(struct screen *, struct sixel_image *);
int		 image_check_line(struct screen *, u_int, u_int);
int		 image_check_area(struct screen *, u_int, u_int, u_int, u_int);
int		 image_scroll_up(struct screen *, u_int);

/* image-sixel.c */
struct sixel_image *sixel_parse(const char *, size_t, u_int, u_int);
void		 sixel_free(struct sixel_image *);
void		 sixel_log(struct sixel_image *);
void		 sixel_size_in_cells(struct sixel_image *, u_int *, u_int *);
struct sixel_image *sixel_scale(struct sixel_image *, u_int, u_int, u_int,
		     u_int, u_int, u_int, int);
char		*sixel_print(struct sixel_image *, struct sixel_image *,
		     size_t *);
struct screen	*sixel_to_screen(struct sixel_image *);
#endif

/* server-acl.c */
void			 server_acl_init(void);
struct server_acl_user	*server_acl_user_find(uid_t);
void 			 server_acl_display(struct cmdq_item *);
void			 server_acl_user_allow(uid_t);
void			 server_acl_user_deny(uid_t);
void			 server_acl_user_allow_write(uid_t);
void			 server_acl_user_deny_write(uid_t);
int			 server_acl_join(struct client *);
uid_t			 server_acl_get_uid(struct server_acl_user *);

/* hyperlink.c */
u_int	 		 hyperlinks_put(struct hyperlinks *, const char *,
			     const char *);
int			 hyperlinks_get(struct hyperlinks *, u_int,
			     const char **, const char **, const char **);
struct hyperlinks	*hyperlinks_init(void);
void			 hyperlinks_reset(struct hyperlinks *);
void			 hyperlinks_free(struct hyperlinks *);

#endif /* TMUX_H */
