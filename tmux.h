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

#define PROTOCOL_VERSION 8

#include <sys/time.h>
#include <sys/uio.h>

#include <event.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <termios.h>
#include <wchar.h>

#ifdef HAVE_UTEMPTER
#include <utempter.h>
#endif

#include "compat.h"
#include "xmalloc.h"

extern char    *__progname;
extern char   **environ;

struct client;
struct environ;
struct input_ctx;
struct mouse_event;
struct options;
struct session;
struct tmuxpeer;
struct tmuxproc;

/* Default global configuration file. */
#define TMUX_CONF "/etc/tmux.conf"

/*
 * Minimum layout cell size, NOT including separator line. The scroll region
 * cannot be one line in height so this must be at least two.
 */
#define PANE_MINIMUM 2

/* Automatic name refresh interval, in microseconds. Must be < 1 second. */
#define NAME_INTERVAL 500000

/*
 * READ_SIZE is the maximum size of data to hold from a pty (the event high
 * watermark). READ_BACKOFF is the amount of data waiting to be output to a tty
 * before pty reads will be backed off. READ_TIME is how long to back off
 * before the next read (in microseconds) if a tty is above READ_BACKOFF.
 */
#define READ_SIZE 1024
#define READ_BACKOFF 512
#define READ_TIME 100

/* Attribute to make gcc check printf-like arguments. */
#define printflike(a, b) __attribute__ ((format (printf, a, b)))

/* Number of items in array. */
#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

/* Bell option values. */
#define BELL_NONE 0
#define BELL_ANY 1
#define BELL_CURRENT 2
#define BELL_OTHER 3

/* Special key codes. */
#define KEYC_NONE 0xffff00000000ULL
#define KEYC_UNKNOWN 0xfffe00000000ULL
#define KEYC_BASE 0x100000000000ULL

/* Key modifier bits. */
#define KEYC_ESCAPE 0x200000000000ULL
#define KEYC_CTRL   0x400000000000ULL
#define KEYC_SHIFT  0x800000000000ULL

/* Mask to obtain key w/o modifiers. */
#define KEYC_MASK_MOD (KEYC_ESCAPE|KEYC_CTRL|KEYC_SHIFT)
#define KEYC_MASK_KEY (~KEYC_MASK_MOD)

/* Is this a mouse key? */
#define KEYC_IS_MOUSE(key) (((key) & KEYC_MASK_KEY) >= KEYC_MOUSE &&	\
    ((key) & KEYC_MASK_KEY) < KEYC_BSPACE)

/* Mouse key codes. */
#define KEYC_MOUSE_KEY(name)				\
	KEYC_ ## name ## _PANE,				\
	KEYC_ ## name ## _STATUS,			\
	KEYC_ ## name ## _BORDER
#define KEYC_MOUSE_STRING(name, s)			\
	{ #s "Pane", KEYC_ ## name ## _PANE },		\
	{ #s "Status", KEYC_ ## name ## _STATUS },	\
	{ #s "Border", KEYC_ ## name ## _BORDER }

/*
 * A single key. This can be ASCII or Unicode or one of the keys starting at
 * KEYC_BASE.
 */
typedef unsigned long long key_code;

/* Special key codes. */
enum {
	/* Focus events. */
	KEYC_FOCUS_IN = KEYC_BASE,
	KEYC_FOCUS_OUT,

	/* Mouse keys. */
	KEYC_MOUSE, /* unclassified mouse event */
	KEYC_MOUSE_KEY(MOUSEDOWN1),
	KEYC_MOUSE_KEY(MOUSEDOWN2),
	KEYC_MOUSE_KEY(MOUSEDOWN3),
	KEYC_MOUSE_KEY(MOUSEUP1),
	KEYC_MOUSE_KEY(MOUSEUP2),
	KEYC_MOUSE_KEY(MOUSEUP3),
	KEYC_MOUSE_KEY(MOUSEDRAG1),
	KEYC_MOUSE_KEY(MOUSEDRAG2),
	KEYC_MOUSE_KEY(MOUSEDRAG3),
	KEYC_MOUSE_KEY(WHEELUP),
	KEYC_MOUSE_KEY(WHEELDOWN),

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
};

/* Termcap codes. */
enum tty_code_code {
	TTYC_AX = 0,
	TTYC_ACSC,	/* acs_chars, ac */
	TTYC_BCE,	/* back_color_erase, ut */
	TTYC_BEL,	/* bell, bl */
	TTYC_BLINK,	/* enter_blink_mode, mb */
	TTYC_BOLD,	/* enter_bold_mode, md */
	TTYC_CIVIS,	/* cursor_invisible, vi */
	TTYC_CLEAR,	/* clear_screen, cl */
	TTYC_CNORM,	/* cursor_normal, ve */
	TTYC_COLORS,	/* max_colors, Co */
	TTYC_CR,	/* restore cursor colour, Cr */
	TTYC_CS,	/* set cursor colour, Cs */
	TTYC_CSR,	/* change_scroll_region, cs */
	TTYC_CUB,	/* parm_left_cursor, LE */
	TTYC_CUB1,	/* cursor_left, le */
	TTYC_CUD,	/* parm_down_cursor, DO */
	TTYC_CUD1,	/* cursor_down, do */
	TTYC_CUF,	/* parm_right_cursor, RI */
	TTYC_CUF1,	/* cursor_right, nd */
	TTYC_CUP,	/* cursor_address, cm */
	TTYC_CUU,	/* parm_up_cursor, UP */
	TTYC_CUU1,	/* cursor_up, up */
	TTYC_CVVIS,	/* cursor_visible, vs */
	TTYC_DCH,	/* parm_dch, DC */
	TTYC_DCH1,	/* delete_character, dc */
	TTYC_DIM,	/* enter_dim_mode, mh */
	TTYC_DL,	/* parm_delete_line, DL */
	TTYC_DL1,	/* delete_line, dl */
	TTYC_E3,
	TTYC_ECH,	/* erase_chars, ec */
	TTYC_EL,	/* clr_eol, ce */
	TTYC_EL1,	/* clr_bol, cb */
	TTYC_ENACS,	/* ena_acs, eA */
	TTYC_FSL,	/* from_status_line, fsl */
	TTYC_HOME,	/* cursor_home, ho */
	TTYC_HPA,	/* column_address, ch */
	TTYC_ICH,	/* parm_ich, IC */
	TTYC_ICH1,	/* insert_character, ic */
	TTYC_IL,	/* parm_insert_line, IL */
	TTYC_IL1,	/* insert_line, il */
	TTYC_INVIS,	/* enter_secure_mode, mk */
	TTYC_IS1,	/* init_1string, i1 */
	TTYC_IS2,	/* init_2string, i2 */
	TTYC_IS3,	/* init_3string, i3 */
	TTYC_KCBT,	/* key_btab, kB */
	TTYC_KCUB1,	/* key_left, kl */
	TTYC_KCUD1,	/* key_down, kd */
	TTYC_KCUF1,	/* key_right, kr */
	TTYC_KCUU1,	/* key_up, ku */
	TTYC_KDC2,
	TTYC_KDC3,
	TTYC_KDC4,
	TTYC_KDC5,
	TTYC_KDC6,
	TTYC_KDC7,
	TTYC_KDCH1,	/* key_dc, kD */
	TTYC_KDN2,
	TTYC_KDN3,
	TTYC_KDN4,
	TTYC_KDN5,
	TTYC_KDN6,
	TTYC_KDN7,
	TTYC_KEND,	/* key_end, ke */
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
	TTYC_KHOME,	/* key_home, kh */
	TTYC_KIC2,
	TTYC_KIC3,
	TTYC_KIC4,
	TTYC_KIC5,
	TTYC_KIC6,
	TTYC_KIC7,
	TTYC_KICH1,	/* key_ic, kI */
	TTYC_KLFT2,
	TTYC_KLFT3,
	TTYC_KLFT4,
	TTYC_KLFT5,
	TTYC_KLFT6,
	TTYC_KLFT7,
	TTYC_KMOUS,	/* key_mouse, Km */
	TTYC_KNP,	/* key_npage, kN */
	TTYC_KNXT2,
	TTYC_KNXT3,
	TTYC_KNXT4,
	TTYC_KNXT5,
	TTYC_KNXT6,
	TTYC_KNXT7,
	TTYC_KPP,	/* key_ppage, kP */
	TTYC_KPRV2,
	TTYC_KPRV3,
	TTYC_KPRV4,
	TTYC_KPRV5,
	TTYC_KPRV6,
	TTYC_KPRV7,
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
	TTYC_MS,	/* modify xterm(1) selection */
	TTYC_OP,	/* orig_pair, op */
	TTYC_REV,	/* enter_reverse_mode, mr */
	TTYC_RI,	/* scroll_reverse, sr */
	TTYC_RMACS,	/* exit_alt_charset_mode */
	TTYC_RMCUP,	/* exit_ca_mode, te */
	TTYC_RMKX,	/* keypad_local, ke */
	TTYC_SE,	/* reset cursor style, Se */
	TTYC_SETAB,	/* set_a_background, AB */
	TTYC_SETAF,	/* set_a_foreground, AF */
	TTYC_SGR0,	/* exit_attribute_mode, me */
	TTYC_SITM,	/* enter_italics_mode, it */
	TTYC_SMACS,	/* enter_alt_charset_mode, as */
	TTYC_SMCUP,	/* enter_ca_mode, ti */
	TTYC_SMKX,	/* keypad_xmit, ks */
	TTYC_SMSO,	/* enter_standout_mode, so */
	TTYC_SMUL,	/* enter_underline_mode, us */
	TTYC_SS,	/* set cursor style, Ss */
	TTYC_TC,	/* 24-bit "true" colour, Tc */
	TTYC_TSL,	/* to_status_line, tsl */
	TTYC_VPA,	/* row_address, cv */
	TTYC_XENL,	/* eat_newline_glitch, xn */
	TTYC_XT,	/* xterm(1)-compatible title, XT */
};

/* Message codes. */
enum msgtype {
	MSG_VERSION = 12,

	MSG_IDENTIFY_FLAGS = 100,
	MSG_IDENTIFY_TERM,
	MSG_IDENTIFY_TTYNAME,
	MSG_IDENTIFY_OLDCWD, /* unused */
	MSG_IDENTIFY_STDIN,
	MSG_IDENTIFY_ENVIRON,
	MSG_IDENTIFY_DONE,
	MSG_IDENTIFY_CLIENTPID,
	MSG_IDENTIFY_CWD,

	MSG_COMMAND = 200,
	MSG_DETACH,
	MSG_DETACHKILL,
	MSG_EXIT,
	MSG_EXITED,
	MSG_EXITING,
	MSG_LOCK,
	MSG_READY,
	MSG_RESIZE,
	MSG_SHELL,
	MSG_SHUTDOWN,
	MSG_STDERR,
	MSG_STDIN,
	MSG_STDOUT,
	MSG_SUSPEND,
	MSG_UNLOCK,
	MSG_WAKEUP,
};

/*
 * Message data.
 *
 * Don't forget to bump PROTOCOL_VERSION if any of these change!
 */
struct msg_command_data {
	int	argc;
}; /* followed by packed argv */

struct msg_stdin_data {
	ssize_t	size;
	char	data[BUFSIZ];
};

struct msg_stdout_data {
	ssize_t	size;
	char	data[BUFSIZ];
};

struct msg_stderr_data {
	ssize_t	size;
	char	data[BUFSIZ];
};

/* Mode key commands. */
enum mode_key_cmd {
	MODEKEY_NONE,
	MODEKEY_OTHER,

	/* Editing keys. */
	MODEKEYEDIT_BACKSPACE,
	MODEKEYEDIT_CANCEL,
	MODEKEYEDIT_COMPLETE,
	MODEKEYEDIT_CURSORLEFT,
	MODEKEYEDIT_CURSORRIGHT,
	MODEKEYEDIT_DELETE,
	MODEKEYEDIT_DELETELINE,
	MODEKEYEDIT_DELETETOENDOFLINE,
	MODEKEYEDIT_DELETEWORD,
	MODEKEYEDIT_ENDOFLINE,
	MODEKEYEDIT_ENTER,
	MODEKEYEDIT_HISTORYDOWN,
	MODEKEYEDIT_HISTORYUP,
	MODEKEYEDIT_NEXTSPACE,
	MODEKEYEDIT_NEXTSPACEEND,
	MODEKEYEDIT_NEXTWORD,
	MODEKEYEDIT_NEXTWORDEND,
	MODEKEYEDIT_PASTE,
	MODEKEYEDIT_PREVIOUSSPACE,
	MODEKEYEDIT_PREVIOUSWORD,
	MODEKEYEDIT_STARTOFLINE,
	MODEKEYEDIT_SWITCHMODE,
	MODEKEYEDIT_SWITCHMODEAPPEND,
	MODEKEYEDIT_SWITCHMODEAPPENDLINE,
	MODEKEYEDIT_SWITCHMODEBEGINLINE,
	MODEKEYEDIT_SWITCHMODECHANGELINE,
	MODEKEYEDIT_SWITCHMODESUBSTITUTE,
	MODEKEYEDIT_SWITCHMODESUBSTITUTELINE,
	MODEKEYEDIT_TRANSPOSECHARS,

	/* Menu (choice) keys. */
	MODEKEYCHOICE_BACKSPACE,
	MODEKEYCHOICE_BOTTOMLINE,
	MODEKEYCHOICE_CANCEL,
	MODEKEYCHOICE_CHOOSE,
	MODEKEYCHOICE_DOWN,
	MODEKEYCHOICE_ENDOFLIST,
	MODEKEYCHOICE_PAGEDOWN,
	MODEKEYCHOICE_PAGEUP,
	MODEKEYCHOICE_SCROLLDOWN,
	MODEKEYCHOICE_SCROLLUP,
	MODEKEYCHOICE_STARTNUMBERPREFIX,
	MODEKEYCHOICE_STARTOFLIST,
	MODEKEYCHOICE_TOPLINE,
	MODEKEYCHOICE_TREE_COLLAPSE,
	MODEKEYCHOICE_TREE_COLLAPSE_ALL,
	MODEKEYCHOICE_TREE_EXPAND,
	MODEKEYCHOICE_TREE_EXPAND_ALL,
	MODEKEYCHOICE_TREE_TOGGLE,
	MODEKEYCHOICE_UP,

	/* Copy keys. */
	MODEKEYCOPY_APPENDSELECTION,
	MODEKEYCOPY_BACKTOINDENTATION,
	MODEKEYCOPY_BOTTOMLINE,
	MODEKEYCOPY_CANCEL,
	MODEKEYCOPY_CLEARSELECTION,
	MODEKEYCOPY_COPYPIPE,
	MODEKEYCOPY_COPYLINE,
	MODEKEYCOPY_COPYENDOFLINE,
	MODEKEYCOPY_COPYSELECTION,
	MODEKEYCOPY_DOWN,
	MODEKEYCOPY_ENDOFLINE,
	MODEKEYCOPY_GOTOLINE,
	MODEKEYCOPY_HALFPAGEDOWN,
	MODEKEYCOPY_HALFPAGEUP,
	MODEKEYCOPY_HISTORYBOTTOM,
	MODEKEYCOPY_HISTORYTOP,
	MODEKEYCOPY_JUMP,
	MODEKEYCOPY_JUMPAGAIN,
	MODEKEYCOPY_JUMPREVERSE,
	MODEKEYCOPY_JUMPBACK,
	MODEKEYCOPY_JUMPTO,
	MODEKEYCOPY_JUMPTOBACK,
	MODEKEYCOPY_LEFT,
	MODEKEYCOPY_MIDDLELINE,
	MODEKEYCOPY_NEXTPAGE,
	MODEKEYCOPY_NEXTSPACE,
	MODEKEYCOPY_NEXTSPACEEND,
	MODEKEYCOPY_NEXTWORD,
	MODEKEYCOPY_NEXTWORDEND,
	MODEKEYCOPY_OTHEREND,
	MODEKEYCOPY_PREVIOUSPAGE,
	MODEKEYCOPY_PREVIOUSSPACE,
	MODEKEYCOPY_PREVIOUSWORD,
	MODEKEYCOPY_RECTANGLETOGGLE,
	MODEKEYCOPY_RIGHT,
	MODEKEYCOPY_SCROLLDOWN,
	MODEKEYCOPY_SCROLLUP,
	MODEKEYCOPY_SEARCHAGAIN,
	MODEKEYCOPY_SEARCHDOWN,
	MODEKEYCOPY_SEARCHREVERSE,
	MODEKEYCOPY_SEARCHUP,
	MODEKEYCOPY_SELECTLINE,
	MODEKEYCOPY_STARTNAMEDBUFFER,
	MODEKEYCOPY_STARTNUMBERPREFIX,
	MODEKEYCOPY_STARTOFLINE,
	MODEKEYCOPY_STARTSELECTION,
	MODEKEYCOPY_TOPLINE,
	MODEKEYCOPY_UP,
};

/* Data required while mode keys are in use. */
struct mode_key_data {
	struct mode_key_tree   *tree;
	int			mode;
};
#define MODEKEY_EMACS 0
#define MODEKEY_VI 1

/* Binding between a key and a command. */
struct mode_key_binding {
	key_code			 key;

	int				 mode;
	enum mode_key_cmd		 cmd;
	const char			*arg;

	RB_ENTRY(mode_key_binding)	 entry;
};
RB_HEAD(mode_key_tree, mode_key_binding);

/* Command to string mapping. */
struct mode_key_cmdstr {
	enum mode_key_cmd	 cmd;
	const char		*name;
};

/* Named mode key table description. */
struct mode_key_entry;
struct mode_key_table {
	const char			*name;
	const struct mode_key_cmdstr	*cmdstr;
	struct mode_key_tree		*tree;
	const struct mode_key_entry	*table;	/* default entries */
};

/* Modes. */
#define MODE_CURSOR 0x1
#define MODE_INSERT 0x2
#define MODE_KCURSOR 0x4
#define MODE_KKEYPAD 0x8	/* set = application, clear = number */
#define MODE_WRAP 0x10		/* whether lines wrap */
#define MODE_MOUSE_STANDARD 0x20
#define MODE_MOUSE_BUTTON 0x40
#define MODE_BLINKING 0x80
#define MODE_MOUSE_UTF8 0x100
#define MODE_MOUSE_SGR 0x200
#define MODE_BRACKETPASTE 0x400
#define MODE_FOCUSON 0x800

#define ALL_MOUSE_MODES (MODE_MOUSE_STANDARD|MODE_MOUSE_BUTTON)

/*
 * A single UTF-8 character. UTF8_SIZE must be big enough to hold at least one
 * combining character as well.
*/
#define UTF8_SIZE 9
struct utf8_data {
	u_char	data[UTF8_SIZE];

	u_char	have;
	u_char	size;

	u_char	width;	/* 0xff if invalid */
} __packed;
enum utf8_state {
	UTF8_MORE,
	UTF8_DONE,
	UTF8_ERROR
};

/* Grid attributes. */
#define GRID_ATTR_BRIGHT 0x1
#define GRID_ATTR_DIM 0x2
#define GRID_ATTR_UNDERSCORE 0x4
#define GRID_ATTR_BLINK 0x8
#define GRID_ATTR_REVERSE 0x10
#define GRID_ATTR_HIDDEN 0x20
#define GRID_ATTR_ITALICS 0x40
#define GRID_ATTR_CHARSET 0x80	/* alternative character set */

/* Grid flags. */
#define GRID_FLAG_FG256 0x1
#define GRID_FLAG_BG256 0x2
#define GRID_FLAG_PADDING 0x4
#define GRID_FLAG_EXTENDED 0x8
#define GRID_FLAG_FGRGB 0x10
#define GRID_FLAG_BGRGB 0x20

/* Grid line flags. */
#define GRID_LINE_WRAPPED 0x1

/* Grid cell RGB colours. */
struct grid_cell_rgb {
	u_char	r;
	u_char	g;
	u_char	b;
};

/* Grid cell data. */
struct grid_cell {
	u_char			flags;
	u_char			attr;
	union {
		u_char		fg;
		struct grid_cell_rgb	fg_rgb;
	};
	union {
		u_char		bg;
		struct grid_cell_rgb	bg_rgb;
	};
	struct utf8_data	data;

};
struct grid_cell_entry {
	u_char			flags;
	union {
		u_int		offset;
		struct {
			u_char	attr;
			u_char	fg;
			u_char	bg;
			u_char	data;
		} data;
	};
} __packed;

/* Grid line. */
struct grid_line {
	u_int			 cellsize;
	struct grid_cell_entry	*celldata;

	u_int			 extdsize;
	struct grid_cell	*extddata;

	int			 flags;
} __packed;

/* Entire grid of cells. */
struct grid {
	int			 flags;
#define GRID_HISTORY 0x1 /* scroll lines into history */

	u_int			 sx;
	u_int			 sy;

	u_int			 hsize;
	u_int			 hlimit;

	struct grid_line	*linedata;
};

/* Hook data structures. */
struct hook {
	const char	*name;

	struct cmd_q	*cmdq;
	struct cmd_list	*cmdlist;

	RB_ENTRY(hook)	 entry;
};

/* Option data structures. */
struct options_entry {
	const char		*name;

	enum {
		OPTIONS_STRING,
		OPTIONS_NUMBER,
		OPTIONS_STYLE
	} type;

	char			*str;
	long long		 num;
	struct grid_cell	 style;

	RB_ENTRY(options_entry) entry;
};

/* Scheduled job. */
struct job {
	enum {
		JOB_RUNNING,
		JOB_DEAD,
		JOB_CLOSED
	} state;

	char		*cmd;
	pid_t		 pid;
	int		 status;

	int		 fd;
	struct bufferevent *event;

	void		(*callbackfn)(struct job *);
	void		(*freefn)(void *);
	void		*data;

	LIST_ENTRY(job)	 lentry;
};
LIST_HEAD(joblist, job);

/* Screen selection. */
struct screen_sel {
	int		 flag;
	int		 rectflag;
	enum {
		LINE_SEL_NONE,
		LINE_SEL_LEFT_RIGHT,
		LINE_SEL_RIGHT_LEFT,
	} lineflag;

	int		 modekeys;

	u_int		 sx;
	u_int		 sy;

	u_int		 ex;
	u_int		 ey;

	struct grid_cell cell;
};

/* Virtual screen. */
struct screen {
	char		*title;

	struct grid	*grid;		/* grid data */

	u_int		 cx;		/* cursor x */
	u_int		 cy;		/* cursor y */

	u_int		 cstyle;	/* cursor style */
	char		*ccolour;	/* cursor colour string */

	u_int		 rupper;	/* scroll region top */
	u_int		 rlower;	/* scroll region bottom */

	int		 mode;

	bitstr_t	*tabs;

	struct screen_sel sel;
};

/* Screen write context. */
struct screen_write_ctx {
	struct window_pane *wp;
	struct screen	*s;
};

/* Screen size. */
#define screen_size_x(s) ((s)->grid->sx)
#define screen_size_y(s) ((s)->grid->sy)
#define screen_hsize(s) ((s)->grid->hsize)
#define screen_hlimit(s) ((s)->grid->hlimit)

/*
 * Window mode. Windows can be in several modes and this is used to call the
 * right function to handle input and output.
 */
struct window_mode {
	struct screen *(*init)(struct window_pane *);
	void	(*free)(struct window_pane *);
	void	(*resize)(struct window_pane *, u_int, u_int);
	void	(*key)(struct window_pane *, struct client *, struct session *,
		    key_code, struct mouse_event *);
};

/* Structures for choose mode. */
struct window_choose_data {
	struct client		*start_client;
	struct session		*start_session;

	u_int			 idx;
	int			 type;
#define TREE_OTHER 0x0
#define TREE_WINDOW 0x1
#define TREE_SESSION 0x2

	struct session		*tree_session; /* session of items in tree */

	struct winlink		*wl;
	int			 pane_id;

	char			*ft_template;
	struct format_tree	*ft;

	char			*command;
};

/* Child window structure. */
struct window_pane {
	u_int		 id;
	u_int		 active_point;

	struct window	*window;

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
#define PANE_RESIZE 0x8
#define PANE_FOCUSPUSH 0x10
#define PANE_INPUTOFF 0x20
#define PANE_CHANGED 0x40

	int		 argc;
	char	       **argv;
	char		*shell;
	const char	*cwd;

	pid_t		 pid;
	char		 tty[TTY_NAME_MAX];
	int		 status;

	int		 fd;
	struct bufferevent *event;
	struct event	 timer;

	struct input_ctx *ictx;

	struct grid_cell colgc;

	int		 pipe_fd;
	struct bufferevent *pipe_event;
	size_t		 pipe_off;

	struct screen	*screen;
	struct screen	 base;

	/* Saved in alternative screen mode. */
	u_int		 saved_cx;
	u_int		 saved_cy;
	struct grid	*saved_grid;
	struct grid_cell saved_cell;

	const struct window_mode *mode;
	void		*modedata;

	TAILQ_ENTRY(window_pane) entry;
	RB_ENTRY(window_pane) tree_entry;
};
TAILQ_HEAD(window_panes, window_pane);
RB_HEAD(window_pane_tree, window_pane);

/* Window structure. */
struct window {
	u_int		 id;

	char		*name;
	struct event	 name_event;
	struct timeval	 name_time;

	struct event	 alerts_timer;

	struct timeval	 activity_time;

	struct window_pane *active;
	struct window_pane *last;
	struct window_panes panes;

	int		 lastlayout;
	struct layout_cell *layout_root;
	struct layout_cell *saved_layout_root;
	char		*old_layout;

	u_int		 sx;
	u_int		 sy;

	int		 flags;
#define WINDOW_BELL 0x1
#define WINDOW_ACTIVITY 0x2
#define WINDOW_REDRAW 0x4
#define WINDOW_SILENCE 0x8
#define WINDOW_ZOOMED 0x1000
#define WINDOW_FORCEWIDTH 0x2000
#define WINDOW_FORCEHEIGHT 0x4000
#define WINDOW_ALERTFLAGS (WINDOW_BELL|WINDOW_ACTIVITY|WINDOW_SILENCE)

	struct options	*options;

	u_int		 references;

	RB_ENTRY(window) entry;
};
RB_HEAD(windows, window);

/* Entry on local window list. */
struct winlink {
	int		 idx;
	struct window	*window;

	size_t		 status_width;
	struct grid_cell status_cell;
	char		*status_text;

	int		 flags;
#define WINLINK_BELL 0x1
#define WINLINK_ACTIVITY 0x2
#define WINLINK_SILENCE 0x4
#define WINLINK_ALERTFLAGS (WINLINK_BELL|WINLINK_ACTIVITY|WINLINK_SILENCE)

	RB_ENTRY(winlink) entry;
	TAILQ_ENTRY(winlink) sentry;
};
RB_HEAD(winlinks, winlink);
TAILQ_HEAD(winlink_stack, winlink);

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

	RB_ENTRY(environ_entry) entry;
};

/* Client session. */
struct session_group {
	TAILQ_HEAD(, session) sessions;

	TAILQ_ENTRY(session_group) entry;
};
TAILQ_HEAD(session_groups, session_group);

struct session {
	u_int		 id;

	char		*name;
	const char	*cwd;

	struct timeval	 creation_time;
	struct timeval	 last_attached_time;
	struct timeval	 activity_time;
	struct timeval	 last_activity_time;

	struct event	 lock_timer;

	u_int		 sx;
	u_int		 sy;

	struct winlink	*curw;
	struct winlink_stack lastw;
	struct winlinks	 windows;

	struct hooks	*hooks;
	struct options	*options;

#define SESSION_UNATTACHED 0x1	/* not attached to any clients */
#define SESSION_PASTING 0x2
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
#define MOUSE_MASK_BUTTONS 3
#define MOUSE_MASK_SHIFT 4
#define MOUSE_MASK_META 8
#define MOUSE_MASK_CTRL 16
#define MOUSE_MASK_DRAG 32
#define MOUSE_MASK_WHEEL 64

/* Mouse wheel states. */
#define MOUSE_WHEEL_UP 0
#define MOUSE_WHEEL_DOWN 64

/* Mouse helpers. */
#define MOUSE_BUTTONS(b) ((b) & MOUSE_MASK_BUTTONS)
#define MOUSE_WHEEL(b) ((b) & MOUSE_MASK_WHEEL)
#define MOUSE_DRAG(b) ((b) & MOUSE_MASK_DRAG)
#define MOUSE_RELEASE(b) (((b) & MOUSE_MASK_BUTTONS) == 3)

/* Mouse input. */
struct mouse_event {
	int		valid;

	key_code	key;
	int		statusat;

	u_int		x;
	u_int		y;
	u_int		b;

	u_int		lx;
	u_int		ly;
	u_int		lb;

	int		s;
	int		w;
	int		wp;

	u_int		sgr_type;
	u_int		sgr_b;
};

/* TTY information. */
struct tty_key {
	char		 ch;
	key_code	 key;

	struct tty_key	*left;
	struct tty_key	*right;

	struct tty_key	*next;
};

struct tty_code;
struct tty_term {
	char		*name;
	u_int		 references;

	char		 acs[UCHAR_MAX + 1][2];

	struct tty_code	*codes;

#define TERM_256COLOURS 0x1
#define TERM_EARLYWRAP 0x2
	int		 flags;

	LIST_ENTRY(tty_term) entry;
};
LIST_HEAD(tty_terms, tty_term);

struct tty {
	struct client	*client;
	char		*path;

	u_int		 sx;
	u_int		 sy;

	u_int		 cx;
	u_int		 cy;
	u_int		 cstyle;
	char		*ccolour;

	int		 mode;

	u_int		 rlower;
	u_int		 rupper;

	char		*termname;
	struct tty_term	*term;

	int		 fd;
	struct bufferevent *event;

	struct termios	 tio;

	struct grid_cell cell;

#define TTY_NOCURSOR 0x1
#define TTY_FREEZE 0x2
#define TTY_TIMER 0x4
#define TTY_UTF8 0x8
#define TTY_STARTED 0x10
#define TTY_OPENED 0x20
#define TTY_FOCUS 0x40
	int		 flags;

	int		 term_flags;

	struct mouse_event mouse;
	int		 mouse_drag_flag;
	void		(*mouse_drag_update)(struct client *,
			    struct mouse_event *);
	void		(*mouse_drag_release)(struct client *,
			    struct mouse_event *);

	struct event	 key_timer;
	struct tty_key	*key_tree;
};

/* TTY command context. */
struct tty_ctx {
	struct window_pane *wp;

	const struct grid_cell *cell;

	u_int		 num;
	void		*ptr;

	/*
	 * Cursor and region position before the screen was updated - this is
	 * where the command should be applied; the values in the screen have
	 * already been updated.
	 */
	u_int		 ocx;
	u_int		 ocy;

	u_int		 orupper;
	u_int		 orlower;

	u_int		 xoff;
	u_int		 yoff;

	/* Saved last cell on line. */
	struct grid_cell last_cell;
	u_int		 last_width;
};

/* Saved message entry. */
struct message_entry {
	char	*msg;
	u_int	 msg_num;
	time_t	 msg_time;
	TAILQ_ENTRY(message_entry) entry;
};

/* Client connection. */
struct client {
	struct tmuxpeer	*peer;

	pid_t		 pid;
	int		 fd;
	struct event	 event;
	int		 retval;

	struct timeval	 creation_time;
	struct timeval	 activity_time;

	struct environ	*environ;

	char		*title;
	const char	*cwd;

	char		*term;
	char		*ttyname;
	struct tty	 tty;

	void		(*stdin_callback)(struct client *, int, void *);
	void		*stdin_callback_data;
	struct evbuffer	*stdin_data;
	int		 stdin_closed;
	struct evbuffer	*stdout_data;
	struct evbuffer	*stderr_data;

	struct event	 repeat_timer;

	struct event	 status_timer;
	struct screen	 status;

#define CLIENT_TERMINAL 0x1
#define CLIENT_LOGIN 0x2
#define CLIENT_EXIT 0x4
#define CLIENT_REDRAW 0x8
#define CLIENT_STATUS 0x10
#define CLIENT_REPEAT 0x20
#define CLIENT_SUSPENDED 0x40
/* 0x80 unused */
#define CLIENT_IDENTIFY 0x100
#define CLIENT_DEAD 0x200
#define CLIENT_BORDERS 0x400
#define CLIENT_READONLY 0x800
#define CLIENT_REDRAWWINDOW 0x1000
#define CLIENT_CONTROL 0x2000
#define CLIENT_CONTROLCONTROL 0x4000
#define CLIENT_FOCUSED 0x8000
#define CLIENT_UTF8 0x10000
#define CLIENT_256COLOURS 0x20000
#define CLIENT_IDENTIFIED 0x40000
#define CLIENT_STATUSFORCE 0x80000
	int		 flags;
	struct key_table *keytable;

	struct event	 identify_timer;

	char		*message_string;
	struct event	 message_timer;
	u_int		 message_next;
	TAILQ_HEAD(, message_entry) message_log;

	char		*prompt_string;
	char		*prompt_buffer;
	size_t		 prompt_index;
	int		 (*prompt_callbackfn)(void *, const char *);
	void		 (*prompt_freefn)(void *);
	void		*prompt_data;
	u_int		 prompt_hindex;

#define PROMPT_SINGLE 0x1
	int		 prompt_flags;

	struct mode_key_data prompt_mdata;

	struct session	*session;
	struct session	*last_session;

	int		 wlmouse;

	struct cmd_q	*cmdq;
	int		 references;

	TAILQ_ENTRY(client) entry;
};
TAILQ_HEAD(clients, client);

/* Parsed arguments structures. */
struct args_entry;
RB_HEAD(args_tree, args_entry);
struct args {
	struct args_tree	  tree;
	int			  argc;
	char			**argv;
};

/* Command find structures. */
enum cmd_find_type {
	CMD_FIND_PANE,
	CMD_FIND_WINDOW,
	CMD_FIND_SESSION,
};
struct cmd_find_state {
	struct cmd_q		*cmdq;
	int			 flags;
	struct cmd_find_state	*current;

	struct session          *s;
	struct winlink          *wl;
	struct window		*w;
	struct window_pane      *wp;
	int			 idx;
};

/* Command find flags. */
#define CMD_FIND_PREFER_UNATTACHED 0x1
#define CMD_FIND_QUIET 0x2
#define CMD_FIND_WINDOW_INDEX 0x4
#define CMD_FIND_DEFAULT_MARKED 0x8
#define CMD_FIND_EXACT_SESSION 0x10
#define CMD_FIND_EXACT_WINDOW 0x20

/* Context for command being executed. */
struct cmd_state {
	struct client		*c;
	struct cmd_find_state	 tflag;
	struct cmd_find_state	 sflag;
};

/* Command and list of commands. */
struct cmd {
	const struct cmd_entry	*entry;
	struct args		*args;

	char			*file;
	u_int			 line;

#define CMD_CONTROL 0x1
	int			 flags;

	TAILQ_ENTRY(cmd)	 qentry;
};

struct cmd_list {
	int			 references;
	TAILQ_HEAD(, cmd)	 list;
};

/* Command return values. */
enum cmd_retval {
	CMD_RETURN_ERROR = -1,
	CMD_RETURN_NORMAL = 0,
	CMD_RETURN_WAIT,
	CMD_RETURN_STOP
};

/* Command queue entry. */
struct cmd_q_item {
	struct cmd_list		*cmdlist;

	struct mouse_event	 mouse;

	TAILQ_ENTRY(cmd_q_item)	 qentry;
};
TAILQ_HEAD(cmd_q_items, cmd_q_item);

/* Command queue. */
struct cmd_q {
	int			 references;
	int			 flags;
#define CMD_Q_DEAD 0x1
#define CMD_Q_REENTRY 0x2
#define CMD_Q_NOHOOKS 0x4

	struct client		*client;
	int			 client_exit;

	struct cmd_q_items	 queue;
	struct cmd_q_item	*item;
	struct cmd		*cmd;
	struct cmd_q		*parent;

	struct cmd_find_state	 current;
	struct cmd_state	 state;

	time_t			 time;
	u_int			 number;

	void			 (*emptyfn)(struct cmd_q *);
	void			*data;

	TAILQ_ENTRY(cmd_q)	 waitentry;
};

/* Command -c, -t or -s flags. */
enum cmd_entry_flag {
	CMD_NONE,

	CMD_CLIENT,
	CMD_CLIENT_CANFAIL,

	CMD_SESSION,
	CMD_SESSION_CANFAIL,
	CMD_SESSION_PREFERUNATTACHED,
	CMD_SESSION_WITHPANE,

	CMD_WINDOW,
	CMD_WINDOW_CANFAIL,
	CMD_WINDOW_MARKED,
	CMD_WINDOW_INDEX,

	CMD_PANE,
	CMD_PANE_CANFAIL,
	CMD_PANE_MARKED,

	CMD_MOVEW_R,
};

/* Command definition. */
struct cmd_entry {
	const char		*name;
	const char		*alias;

	struct {
		const char	*template;
		int		 lower;
		int		 upper;
	} args;
	const char		*usage;

	enum cmd_entry_flag	 tflag;
	enum cmd_entry_flag	 sflag;
	enum cmd_entry_flag	 cflag;

#define CMD_STARTSERVER 0x1
#define CMD_READONLY 0x2
	int		 flags;

	enum cmd_retval		 (*exec)(struct cmd *, struct cmd_q *);
};

/* Key binding and key table. */
struct key_binding {
	key_code		 key;
	struct cmd_list		*cmdlist;
	int			 can_repeat;

	RB_ENTRY(key_binding)	 entry;
};
RB_HEAD(key_bindings, key_binding);

struct key_table {
	const char		 *name;
	struct key_bindings	 key_bindings;

	u_int			 references;

	RB_ENTRY(key_table)	 entry;
};
RB_HEAD(key_tables, key_table);

/*
 * Option table entries. The option table is the user-visible part of the
 * option, as opposed to the internal options (struct option) which are just
 * number or string.
 */
enum options_table_type {
	OPTIONS_TABLE_STRING,
	OPTIONS_TABLE_NUMBER,
	OPTIONS_TABLE_KEY,
	OPTIONS_TABLE_COLOUR,
	OPTIONS_TABLE_ATTRIBUTES,
	OPTIONS_TABLE_FLAG,
	OPTIONS_TABLE_CHOICE,
	OPTIONS_TABLE_STYLE
};
enum options_table_scope {
	OPTIONS_TABLE_NONE,
	OPTIONS_TABLE_SERVER,
	OPTIONS_TABLE_SESSION,
	OPTIONS_TABLE_WINDOW,
};

struct options_table_entry {
	const char		 *name;
	enum options_table_type	  type;
	enum options_table_scope  scope;

	u_int			  minimum;
	u_int			  maximum;
	const char		**choices;

	const char		 *default_str;
	long long		  default_num;

	const char		 *style;
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

/* tmux.c */
extern struct hooks	*global_hooks;
extern struct options	*global_options;
extern struct options	*global_s_options;
extern struct options	*global_w_options;
extern struct environ	*global_environ;
extern struct timeval	 start_time;
extern const char	*socket_path;
const char	*getshell(void);
int		 checkshell(const char *);
int		 areshell(const char *);
void		 setblocking(int, int);
const char	*find_home(void);

/* proc.c */
struct imsg;
int	proc_send(struct tmuxpeer *, enum msgtype, int, const void *, size_t);
int	proc_send_s(struct tmuxpeer *, enum msgtype, const char *);
struct tmuxproc *proc_start(const char *, struct event_base *, int,
	    void (*)(int));
void	proc_loop(struct tmuxproc *, int (*)(void));
void	proc_exit(struct tmuxproc *);
struct tmuxpeer *proc_add_peer(struct tmuxproc *, int,
	    void (*)(struct imsg *, void *), void *);
void	proc_remove_peer(struct tmuxpeer *);
void	proc_kill_peer(struct tmuxpeer *);

/* cfg.c */
extern int cfg_finished;
extern int cfg_references;
extern struct client *cfg_client;
void		 start_cfg(void);
int		 load_cfg(const char *, struct cmd_q *, char **);
void		 set_cfg_file(const char *);
void printflike(1, 2) cfg_add_cause(const char *, ...);
void		 cfg_print_causes(struct cmd_q *);
void		 cfg_show_causes(struct session *);

/* paste.c */
struct paste_buffer;
const char	*paste_buffer_name(struct paste_buffer *);
const char	*paste_buffer_data(struct paste_buffer *, size_t *);
struct paste_buffer *paste_walk(struct paste_buffer *);
struct paste_buffer *paste_get_top(const char **);
struct paste_buffer *paste_get_name(const char *);
void		 paste_free(struct paste_buffer *);
void		 paste_add(char *, size_t);
int		 paste_rename(const char *, const char *, char **);
int		 paste_set(char *, size_t, const char *, char **);
char		*paste_make_sample(struct paste_buffer *);

/* format.c */
#define FORMAT_STATUS 0x1
#define FORMAT_FORCE 0x2
struct format_tree;
struct format_tree *format_create(struct cmd_q *, int);
void		 format_free(struct format_tree *);
void printflike(3, 4) format_add(struct format_tree *, const char *,
		     const char *, ...);
char		*format_expand_time(struct format_tree *, const char *, time_t);
char		*format_expand(struct format_tree *, const char *);
void		 format_defaults(struct format_tree *, struct client *,
		     struct session *, struct winlink *, struct window_pane *);
void		 format_defaults_window(struct format_tree *, struct window *);
void		 format_defaults_pane(struct format_tree *,
		     struct window_pane *);
void		 format_defaults_paste_buffer(struct format_tree *,
		     struct paste_buffer *);

/* hooks.c */
struct hook;
struct hooks 	*hooks_get(struct session *);
struct hooks	*hooks_create(struct hooks *);
void		 hooks_free(struct hooks *);
struct hook	*hooks_first(struct hooks *);
struct hook	*hooks_next(struct hook *);
void		 hooks_add(struct hooks *, const char *, struct cmd_list *);
void		 hooks_copy(struct hooks *, struct hooks *);
void		 hooks_remove(struct hooks *, const char *);
struct hook	*hooks_find(struct hooks *, const char *);
int printflike(4, 5) hooks_run(struct hooks *, struct client *,
		    struct cmd_find_state *, const char *, ...);
int printflike(4, 5) hooks_wait(struct hooks *, struct cmd_q *,
		    struct cmd_find_state *, const char *, ...);

/* mode-key.c */
extern const struct mode_key_table mode_key_tables[];
extern struct mode_key_tree mode_key_tree_vi_edit;
extern struct mode_key_tree mode_key_tree_vi_choice;
extern struct mode_key_tree mode_key_tree_vi_copy;
extern struct mode_key_tree mode_key_tree_emacs_edit;
extern struct mode_key_tree mode_key_tree_emacs_choice;
extern struct mode_key_tree mode_key_tree_emacs_copy;
int	mode_key_cmp(struct mode_key_binding *, struct mode_key_binding *);
RB_PROTOTYPE(mode_key_tree, mode_key_binding, entry, mode_key_cmp);
const char *mode_key_tostring(const struct mode_key_cmdstr *,
	    enum mode_key_cmd);
enum mode_key_cmd mode_key_fromstring(const struct mode_key_cmdstr *,
	    const char *);
const struct mode_key_table *mode_key_findtable(const char *);
void	mode_key_init_trees(void);
void	mode_key_init(struct mode_key_data *, struct mode_key_tree *);
enum mode_key_cmd mode_key_lookup(struct mode_key_data *, key_code,
	    const char **);

/* notify.c */
void	notify_enable(void);
void	notify_disable(void);
void	notify_input(struct window_pane *, struct evbuffer *);
void	notify_window_layout_changed(struct window *);
void	notify_window_unlinked(struct session *, struct window *);
void	notify_window_linked(struct session *, struct window *);
void	notify_window_renamed(struct window *);
void	notify_attached_session_changed(struct client *);
void	notify_session_renamed(struct session *);
void	notify_session_created(struct session *);
void	notify_session_closed(struct session *);

/* options.c */
struct options *options_create(struct options *);
void	options_free(struct options *);
struct options_entry *options_first(struct options *);
struct options_entry *options_next(struct options_entry *);
struct options_entry *options_find1(struct options *, const char *);
struct options_entry *options_find(struct options *, const char *);
void	options_remove(struct options *, const char *);
struct options_entry *printflike(3, 4) options_set_string(struct options *,
	    const char *, const char *, ...);
char   *options_get_string(struct options *, const char *);
struct options_entry *options_set_number(struct options *, const char *,
	    long long);
long long options_get_number(struct options *, const char *);
struct options_entry *options_set_style(struct options *, const char *,
	    const char *, int);
struct grid_cell *options_get_style(struct options *, const char *);

/* options-table.c */
extern const struct options_table_entry options_table[];
void	options_table_populate_tree(enum options_table_scope, struct options *);
const char *options_table_print_entry(const struct options_table_entry *,
	    struct options_entry *, int);
int	options_table_find(const char *, const struct options_table_entry **);

/* job.c */
extern struct joblist all_jobs;
struct job *job_run(const char *, struct session *, const char *,
	    void (*)(struct job *), void (*)(void *), void *);
void	job_free(struct job *);
void	job_died(struct job *, int);

/* environ.c */
struct environ *environ_create(void);
void	environ_free(struct environ *);
struct environ_entry *environ_first(struct environ *);
struct environ_entry *environ_next(struct environ_entry *);
void	environ_copy(struct environ *, struct environ *);
struct environ_entry *environ_find(struct environ *, const char *);
void printflike(3, 4) environ_set(struct environ *, const char *, const char *,
	    ...);
void	environ_clear(struct environ *, const char *);
void	environ_put(struct environ *, const char *);
void	environ_unset(struct environ *, const char *);
void	environ_update(const char *, struct environ *, struct environ *);
void	environ_push(struct environ *);

/* tty.c */
void	tty_create_log(void);
void	tty_init_termios(int, struct termios *, struct bufferevent *);
void	tty_raw(struct tty *, const char *);
void	tty_attributes(struct tty *, const struct grid_cell *,
	    const struct window_pane *);
void	tty_reset(struct tty *);
void	tty_region_pane(struct tty *, const struct tty_ctx *, u_int, u_int);
void	tty_region(struct tty *, u_int, u_int);
void	tty_cursor_pane(struct tty *, const struct tty_ctx *, u_int, u_int);
void	tty_cursor(struct tty *, u_int, u_int);
void	tty_putcode(struct tty *, enum tty_code_code);
void	tty_putcode1(struct tty *, enum tty_code_code, int);
void	tty_putcode2(struct tty *, enum tty_code_code, int, int);
void	tty_putcode_ptr1(struct tty *, enum tty_code_code, const void *);
void	tty_putcode_ptr2(struct tty *, enum tty_code_code, const void *,
	    const void *);
void	tty_puts(struct tty *, const char *);
void	tty_putc(struct tty *, u_char);
void	tty_putn(struct tty *, const void *, size_t, u_int);
int	tty_init(struct tty *, struct client *, int, char *);
int	tty_resize(struct tty *);
int	tty_set_size(struct tty *, u_int, u_int);
void	tty_start_tty(struct tty *);
void	tty_stop_tty(struct tty *);
void	tty_set_title(struct tty *, const char *);
void	tty_update_mode(struct tty *, int, struct screen *);
void	tty_force_cursor_colour(struct tty *, const char *);
void	tty_draw_pane(struct tty *, const struct window_pane *, u_int, u_int,
	    u_int);
void	tty_draw_line(struct tty *, const struct window_pane *, struct screen *,
	    u_int, u_int, u_int);
int	tty_open(struct tty *, char **);
void	tty_close(struct tty *);
void	tty_free(struct tty *);
void	tty_write(void (*)(struct tty *, const struct tty_ctx *),
	    struct tty_ctx *);
int	tty_client_ready(struct client *, struct window_pane *wp);
void	tty_cmd_alignmenttest(struct tty *, const struct tty_ctx *);
void	tty_cmd_cell(struct tty *, const struct tty_ctx *);
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
void	tty_cmd_utf8character(struct tty *, const struct tty_ctx *);
void	tty_cmd_reverseindex(struct tty *, const struct tty_ctx *);
void	tty_cmd_setselection(struct tty *, const struct tty_ctx *);
void	tty_cmd_rawstring(struct tty *, const struct tty_ctx *);

/* tty-term.c */
extern struct tty_terms tty_terms;
u_int		 tty_term_ncodes(void);
struct tty_term *tty_term_find(char *, int, char **);
void		 tty_term_free(struct tty_term *);
int		 tty_term_has(struct tty_term *, enum tty_code_code);
const char	*tty_term_string(struct tty_term *, enum tty_code_code);
const char	*tty_term_string1(struct tty_term *, enum tty_code_code, int);
const char	*tty_term_string2(struct tty_term *, enum tty_code_code, int,
		     int);
const char	*tty_term_ptr1(struct tty_term *, enum tty_code_code,
		     const void *);
const char	*tty_term_ptr2(struct tty_term *, enum tty_code_code,
		     const void *, const void *);
int		 tty_term_number(struct tty_term *, enum tty_code_code);
int		 tty_term_flag(struct tty_term *, enum tty_code_code);
const char	*tty_term_describe(struct tty_term *, enum tty_code_code);

/* tty-acs.c */
const char	*tty_acs_get(struct tty *, u_char);

/* tty-keys.c */
void		tty_keys_build(struct tty *);
void		tty_keys_free(struct tty *);
key_code	tty_keys_next(struct tty *);

/* arguments.c */
int		 args_cmp(struct args_entry *, struct args_entry *);
RB_PROTOTYPE(args_tree, args_entry, entry, args_cmp);
struct args	*args_create(int, ...);
struct args	*args_parse(const char *, int, char **);
void		 args_free(struct args *);
char		*args_print(struct args *);
int		 args_has(struct args *, u_char);
void		 args_set(struct args *, u_char, const char *);
const char	*args_get(struct args *, u_char);
long long	 args_strtonum(struct args *, u_char, long long, long long,
		     char **);

/* cmd-find.c */
int		 cmd_find_current(struct cmd_find_state *, struct cmd_q *,
		     int);
int		 cmd_find_target(struct cmd_find_state *,
		     struct cmd_find_state *, struct cmd_q *, const char *,
		     enum cmd_find_type, int);
struct client	*cmd_find_client(struct cmd_q *, const char *, int);
void		 cmd_find_clear_state(struct cmd_find_state *, struct cmd_q *,
		     int);
int		 cmd_find_valid_state(struct cmd_find_state *);
void		 cmd_find_copy_state(struct cmd_find_state *,
		     struct cmd_find_state *);
void		 cmd_find_log_state(const char *, struct cmd_find_state *);
int		 cmd_find_from_session(struct cmd_find_state *,
		     struct session *);
int		 cmd_find_from_winlink(struct cmd_find_state *,
		     struct session *, struct winlink *);
int		 cmd_find_from_window(struct cmd_find_state *, struct window *);
int		 cmd_find_from_pane(struct cmd_find_state *,
		     struct window_pane *);

/* cmd.c */
int		 cmd_pack_argv(int, char **, char *, size_t);
int		 cmd_unpack_argv(char *, size_t, int, char ***);
char	       **cmd_copy_argv(int, char **);
void		 cmd_free_argv(int, char **);
char		*cmd_stringify_argv(int, char **);
struct cmd	*cmd_parse(int, char **, const char *, u_int, char **);
int		 cmd_prepare_state(struct cmd *, struct cmd_q *,
		     struct cmd_q *);
char		*cmd_print(struct cmd *);
int		 cmd_mouse_at(struct window_pane *, struct mouse_event *,
		     u_int *, u_int *, int);
struct winlink	*cmd_mouse_window(struct mouse_event *, struct session **);
struct window_pane *cmd_mouse_pane(struct mouse_event *, struct session **,
		     struct winlink **);
char		*cmd_template_replace(const char *, const char *, int);
extern const struct cmd_entry *cmd_table[];

/* cmd-attach-session.c */
enum cmd_retval	 cmd_attach_session(struct cmd_q *, int, int, const char *,
    int);

/* cmd-list.c */
struct cmd_list	*cmd_list_parse(int, char **, const char *, u_int, char **);
void		 cmd_list_free(struct cmd_list *);
char		*cmd_list_print(struct cmd_list *);

/* cmd-queue.c */
struct cmd_q	*cmdq_new(struct client *);
int		 cmdq_free(struct cmd_q *);
void printflike(2, 3) cmdq_print(struct cmd_q *, const char *, ...);
void printflike(2, 3) cmdq_error(struct cmd_q *, const char *, ...);
void		 cmdq_guard(struct cmd_q *, const char *, int);
void		 cmdq_run(struct cmd_q *, struct cmd_list *,
		     struct mouse_event *);
void		 cmdq_append(struct cmd_q *, struct cmd_list *,
		     struct mouse_event *);
int		 cmdq_continue(struct cmd_q *);
void		 cmdq_flush(struct cmd_q *);

/* cmd-string.c */
int	cmd_string_parse(const char *, struct cmd_list **, const char *,
	    u_int, char **);

/* cmd-wait-for.c */
void	cmd_wait_for_flush(void);

/* client.c */
int	client_main(struct event_base *, int, char **, int, const char *);

/* key-bindings.c */
RB_PROTOTYPE(key_bindings, key_binding, entry, key_bindings_cmp);
RB_PROTOTYPE(key_tables, key_table, entry, key_table_cmp);
extern struct key_tables key_tables;
int	 key_table_cmp(struct key_table *, struct key_table *);
int	 key_bindings_cmp(struct key_binding *, struct key_binding *);
struct key_table *key_bindings_get_table(const char *, int);
void	 key_bindings_unref_table(struct key_table *);
void	 key_bindings_add(const char *, key_code, int, struct cmd_list *);
void	 key_bindings_remove(const char *, key_code);
void	 key_bindings_remove_table(const char *);
void	 key_bindings_init(void);
void	 key_bindings_dispatch(struct key_binding *, struct client *,
	     struct mouse_event *);

/* key-string.c */
key_code	 key_string_lookup_string(const char *);
const char	*key_string_lookup_key(key_code);

/* alerts.c */
void	alerts_reset_all(void);
void	alerts_queue(struct window *, int);
void	alerts_check_session(struct session *);

/* server.c */
extern struct tmuxproc *server_proc;
extern struct clients clients;
extern struct cmd_find_state marked_pane;
void	 server_set_marked(struct session *, struct winlink *,
	     struct window_pane *);
void	 server_clear_marked(void);
int	 server_is_marked(struct session *, struct winlink *,
	     struct window_pane *);
int	 server_check_marked(void);
int	 server_start(struct event_base *, int, char *);
void	 server_update_socket(void);
void	 server_add_accept(int);

/* server-client.c */
void	 server_client_set_key_table(struct client *, const char *);
const char *server_client_get_key_table(struct client *);
int	 server_client_check_nested(struct client *);
void	 server_client_handle_key(struct client *, key_code);
void	 server_client_create(int);
int	 server_client_open(struct client *, char **);
void	 server_client_unref(struct client *);
void	 server_client_lost(struct client *);
void	 server_client_detach(struct client *, enum msgtype);
void	 server_client_loop(void);
void	 server_client_push_stdout(struct client *);
void	 server_client_push_stderr(struct client *);

/* server-fn.c */
void	 server_fill_environ(struct session *, struct environ *);
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
void	 server_kill_window(struct window *);
int	 server_link_window(struct session *,
	     struct winlink *, struct session *, int, int, int, char **);
void	 server_unlink_window(struct session *, struct winlink *);
void	 server_destroy_pane(struct window_pane *, int);
void	 server_destroy_session_group(struct session *);
void	 server_destroy_session(struct session *);
void	 server_check_unattached(void);
void	 server_set_identify(struct client *);
void	 server_clear_identify(struct client *);
int	 server_set_stdin_callback(struct client *, void (*)(struct client *,
	     int, void *), void *, char **);
void	 server_unzoom_window(struct window *);

/* status.c */
void	 status_timer_start(struct client *);
void	 status_timer_start_all(void);
int	 status_at_line(struct client *);
struct window *status_get_window_at(struct client *, u_int);
int	 status_redraw(struct client *);
void printflike(2, 3) status_message_set(struct client *, const char *, ...);
void	 status_message_clear(struct client *);
int	 status_message_redraw(struct client *);
void	 status_prompt_set(struct client *, const char *, const char *,
	     int (*)(void *, const char *), void (*)(void *), void *, int);
void	 status_prompt_clear(struct client *);
int	 status_prompt_redraw(struct client *);
void	 status_prompt_key(struct client *, key_code);
void	 status_prompt_update(struct client *, const char *, const char *);
void	 status_prompt_load_history(void);
void	 status_prompt_save_history(void);

/* resize.c */
void	 recalculate_sizes(void);

/* input.c */
void	 input_init(struct window_pane *);
void	 input_free(struct window_pane *);
void	 input_reset(struct window_pane *, int);
struct evbuffer *input_pending(struct window_pane *);
void	 input_parse(struct window_pane *);

/* input-key.c */
void	 input_key(struct window_pane *, key_code, struct mouse_event *);

/* xterm-keys.c */
char	*xterm_keys_lookup(key_code);
int	 xterm_keys_find(const char *, size_t, size_t *, key_code *);

/* colour.c */
int	 colour_find_rgb(u_char, u_char, u_char);
void	 colour_set_fg(struct grid_cell *, int);
void	 colour_set_bg(struct grid_cell *, int);
const char *colour_tostring(int);
int	 colour_fromstring(const char *);
u_char	 colour_256to16(u_char);

/* attributes.c */
const char *attributes_tostring(u_char);
int	 attributes_fromstring(const char *);

/* grid.c */
extern const struct grid_cell grid_default_cell;
struct grid *grid_create(u_int, u_int, u_int);
void	 grid_destroy(struct grid *);
int	 grid_compare(struct grid *, struct grid *);
void	 grid_collect_history(struct grid *);
void	 grid_scroll_history(struct grid *);
void	 grid_scroll_history_region(struct grid *, u_int, u_int);
void	 grid_clear_history(struct grid *);
void	 grid_expand_line(struct grid *, u_int, u_int);
const struct grid_line *grid_peek_line(struct grid *, u_int);
void	 grid_get_cell(struct grid *, u_int, u_int, struct grid_cell *);
void	 grid_set_cell(struct grid *, u_int, u_int, const struct grid_cell *);
void	 grid_clear(struct grid *, u_int, u_int, u_int, u_int);
void	 grid_clear_lines(struct grid *, u_int, u_int);
void	 grid_move_lines(struct grid *, u_int, u_int, u_int);
void	 grid_move_cells(struct grid *, u_int, u_int, u_int, u_int);
char	*grid_string_cells(struct grid *, u_int, u_int, u_int,
	     struct grid_cell **, int, int, int);
void	 grid_duplicate_lines(struct grid *, u_int, struct grid *, u_int,
	     u_int);
u_int	 grid_reflow(struct grid *, struct grid *, u_int);

/* grid-view.c */
void	 grid_view_get_cell(struct grid *, u_int, u_int, struct grid_cell *);
void	 grid_view_set_cell(struct grid *, u_int, u_int,
	     const struct grid_cell *);
void	 grid_view_clear_history(struct grid *);
void	 grid_view_clear(struct grid *, u_int, u_int, u_int, u_int);
void	 grid_view_scroll_region_up(struct grid *, u_int, u_int);
void	 grid_view_scroll_region_down(struct grid *, u_int, u_int);
void	 grid_view_insert_lines(struct grid *, u_int, u_int);
void	 grid_view_insert_lines_region(struct grid *, u_int, u_int, u_int);
void	 grid_view_delete_lines(struct grid *, u_int, u_int);
void	 grid_view_delete_lines_region(struct grid *, u_int, u_int, u_int);
void	 grid_view_insert_cells(struct grid *, u_int, u_int, u_int);
void	 grid_view_delete_cells(struct grid *, u_int, u_int, u_int);
char	*grid_view_string_cells(struct grid *, u_int, u_int, u_int);

/* screen-write.c */
void	 screen_write_start(struct screen_write_ctx *, struct window_pane *,
	     struct screen *);
void	 screen_write_stop(struct screen_write_ctx *);
void	 screen_write_reset(struct screen_write_ctx *);
size_t printflike(1, 2) screen_write_cstrlen(const char *, ...);
void printflike(4, 5) screen_write_cnputs(struct screen_write_ctx *,
	     ssize_t, struct grid_cell *, const char *, ...);
size_t printflike(1, 2) screen_write_strlen(const char *, ...);
void printflike(3, 4) screen_write_puts(struct screen_write_ctx *,
	     struct grid_cell *, const char *, ...);
void printflike(4, 5) screen_write_nputs(struct screen_write_ctx *,
	     ssize_t, struct grid_cell *, const char *, ...);
void	 screen_write_vnputs(struct screen_write_ctx *, ssize_t,
	     struct grid_cell *, const char *, va_list);
void	 screen_write_putc(struct screen_write_ctx *, struct grid_cell *,
	     u_char);
void	 screen_write_copy(struct screen_write_ctx *, struct screen *, u_int,
	     u_int, u_int, u_int);
void	 screen_write_backspace(struct screen_write_ctx *);
void	 screen_write_mode_set(struct screen_write_ctx *, int);
void	 screen_write_mode_clear(struct screen_write_ctx *, int);
void	 screen_write_cursorup(struct screen_write_ctx *, u_int);
void	 screen_write_cursordown(struct screen_write_ctx *, u_int);
void	 screen_write_cursorright(struct screen_write_ctx *, u_int);
void	 screen_write_cursorleft(struct screen_write_ctx *, u_int);
void	 screen_write_alignmenttest(struct screen_write_ctx *);
void	 screen_write_insertcharacter(struct screen_write_ctx *, u_int);
void	 screen_write_deletecharacter(struct screen_write_ctx *, u_int);
void	 screen_write_clearcharacter(struct screen_write_ctx *, u_int);
void	 screen_write_insertline(struct screen_write_ctx *, u_int);
void	 screen_write_deleteline(struct screen_write_ctx *, u_int);
void	 screen_write_clearline(struct screen_write_ctx *);
void	 screen_write_clearendofline(struct screen_write_ctx *);
void	 screen_write_clearstartofline(struct screen_write_ctx *);
void	 screen_write_cursormove(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_reverseindex(struct screen_write_ctx *);
void	 screen_write_scrollregion(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_linefeed(struct screen_write_ctx *, int);
void	 screen_write_carriagereturn(struct screen_write_ctx *);
void	 screen_write_clearendofscreen(struct screen_write_ctx *);
void	 screen_write_clearstartofscreen(struct screen_write_ctx *);
void	 screen_write_clearscreen(struct screen_write_ctx *);
void	 screen_write_clearhistory(struct screen_write_ctx *);
void	 screen_write_cell(struct screen_write_ctx *, const struct grid_cell *);
void	 screen_write_setselection(struct screen_write_ctx *, u_char *, u_int);
void	 screen_write_rawstring(struct screen_write_ctx *, u_char *, u_int);

/* screen-redraw.c */
void	 screen_redraw_screen(struct client *, int, int, int);
void	 screen_redraw_pane(struct client *, struct window_pane *);

/* screen.c */
void	 screen_init(struct screen *, u_int, u_int, u_int);
void	 screen_reinit(struct screen *);
void	 screen_free(struct screen *);
void	 screen_reset_tabs(struct screen *);
void	 screen_set_cursor_style(struct screen *, u_int);
void	 screen_set_cursor_colour(struct screen *, const char *);
void	 screen_set_title(struct screen *, const char *);
void	 screen_resize(struct screen *, u_int, u_int, int);
void	 screen_set_selection(struct screen *,
	     u_int, u_int, u_int, u_int, u_int, struct grid_cell *);
void	 screen_clear_selection(struct screen *);
int	 screen_check_selection(struct screen *, u_int, u_int);
void	 screen_reflow(struct screen *, u_int);

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
int		 winlink_next_index(struct winlinks *, int);
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
struct window	*window_create1(u_int, u_int);
struct window	*window_create(const char *, int, char **, const char *,
		     const char *, const char *, struct environ *,
		     struct termios *, u_int, u_int, u_int, char **);
void		 window_destroy(struct window *);
struct window_pane *window_get_active_at(struct window *, u_int, u_int);
struct window_pane *window_find_string(struct window *, const char *);
int		 window_has_pane(struct window *, struct window_pane *);
int		 window_set_active_pane(struct window *, struct window_pane *);
void		 window_redraw_active_switch(struct window *,
		     struct window_pane *);
struct window_pane *window_add_pane(struct window *, u_int);
void		 window_resize(struct window *, u_int, u_int);
int		 window_zoom(struct window_pane *);
int		 window_unzoom(struct window *);
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
struct window_pane *window_pane_create(struct window *, u_int, u_int, u_int);
void		 window_pane_destroy(struct window_pane *);
int		 window_pane_spawn(struct window_pane *, int, char **,
		     const char *, const char *, const char *, struct environ *,
		     struct termios *, char **);
void		 window_pane_resize(struct window_pane *, u_int, u_int);
void		 window_pane_alternate_on(struct window_pane *,
		     struct grid_cell *, int);
void		 window_pane_alternate_off(struct window_pane *,
		     struct grid_cell *, int);
int		 window_pane_set_mode(struct window_pane *,
		     const struct window_mode *);
void		 window_pane_reset_mode(struct window_pane *);
void		 window_pane_key(struct window_pane *, struct client *,
		     struct session *, key_code, struct mouse_event *);
int		 window_pane_visible(struct window_pane *);
char		*window_pane_search(struct window_pane *, const char *,
		     u_int *);
char		*window_printable_flags(struct session *, struct winlink *);
struct window_pane *window_pane_find_up(struct window_pane *);
struct window_pane *window_pane_find_down(struct window_pane *);
struct window_pane *window_pane_find_left(struct window_pane *);
struct window_pane *window_pane_find_right(struct window_pane *);
void		 window_set_name(struct window *, const char *);
void		 window_remove_ref(struct window *);
void		 winlink_clear_flags(struct winlink *);
int		 winlink_shuffle_up(struct session *, struct winlink *);

/* layout.c */
u_int		 layout_count_cells(struct layout_cell *);
struct layout_cell *layout_create_cell(struct layout_cell *);
void		 layout_free_cell(struct layout_cell *);
void		 layout_print_cell(struct layout_cell *, const char *, u_int);
void		 layout_destroy_cell(struct layout_cell *,
		     struct layout_cell **);
void		 layout_set_size(struct layout_cell *, u_int, u_int, u_int,
		     u_int);
void		 layout_make_leaf(struct layout_cell *, struct window_pane *);
void		 layout_make_node(struct layout_cell *, enum layout_type);
void		 layout_fix_offsets(struct layout_cell *);
void		 layout_fix_panes(struct window *, u_int, u_int);
u_int		 layout_resize_check(struct layout_cell *, enum layout_type);
void		 layout_resize_adjust(struct layout_cell *, enum layout_type,
		     int);
void		 layout_init(struct window *, struct window_pane *);
void		 layout_free(struct window *);
void		 layout_resize(struct window *, u_int, u_int);
void		 layout_resize_pane(struct window_pane *, enum layout_type,
		     int);
void		 layout_resize_pane_to(struct window_pane *, enum layout_type,
		     u_int);
void		 layout_assign_pane(struct layout_cell *, struct window_pane *);
struct layout_cell *layout_split_pane(struct window_pane *, enum layout_type,
		     int, int);
void		 layout_close_pane(struct window_pane *);

/* layout-custom.c */
char		*layout_dump(struct layout_cell *);
int		 layout_parse(struct window *, const char *);

/* layout-set.c */
int		 layout_set_lookup(const char *);
u_int		 layout_set_select(struct window *, u_int);
u_int		 layout_set_next(struct window *);
u_int		 layout_set_previous(struct window *);

/* window-clock.c */
extern const struct window_mode window_clock_mode;
extern const char window_clock_table[14][5][5];

/* window-copy.c */
extern const struct window_mode window_copy_mode;
void		 window_copy_init_from_pane(struct window_pane *, int);
void		 window_copy_init_for_output(struct window_pane *);
void printflike(2, 3) window_copy_add(struct window_pane *, const char *, ...);
void		 window_copy_vadd(struct window_pane *, const char *, va_list);
void		 window_copy_pageup(struct window_pane *);
void		 window_copy_start_drag(struct client *, struct mouse_event *);
int		 window_copy_scroll_position(struct window_pane *);

/* window-choose.c */
extern const struct window_mode window_choose_mode;
void		 window_choose_add(struct window_pane *,
			 struct window_choose_data *);
void		 window_choose_ready(struct window_pane *,
		     u_int, void (*)(struct window_choose_data *));
struct window_choose_data	*window_choose_data_create (int,
		     struct client *, struct session *);
void	window_choose_data_free(struct window_choose_data *);
void	window_choose_data_run(struct window_choose_data *);
struct window_choose_data	*window_choose_add_window(struct window_pane *,
			struct client *, struct session *, struct winlink *,
			const char *, const char *, u_int);
struct window_choose_data	*window_choose_add_session(struct window_pane *,
			struct client *, struct session *, const char *,
			const char *, u_int);
void	window_choose_expand_all(struct window_pane *);
void	window_choose_collapse_all(struct window_pane *);
void	window_choose_set_current(struct window_pane *, u_int);

/* names.c */
void	 check_window_name(struct window *);
char	*default_window_name(struct window *);
char	*format_window_name(struct window *);
char	*parse_window_name(const char *);

/* signal.c */
void	set_signals(void(*)(int, short, void *), void *);
void	clear_signals(int);

/* control.c */
void	control_callback(struct client *, int, void *);
void printflike(2, 3) control_write(struct client *, const char *, ...);
void	control_write_buffer(struct client *, struct evbuffer *);

/* control-notify.c */
void	control_notify_input(struct client *, struct window_pane *,
	    struct evbuffer *);
void	control_notify_window_layout_changed(struct window *);
void	control_notify_window_unlinked(struct session *, struct window *);
void	control_notify_window_linked(struct session *, struct window *);
void	control_notify_window_renamed(struct window *);
void	control_notify_attached_session_changed(struct client *);
void	control_notify_session_renamed(struct session *);
void	control_notify_session_created(struct session *);
void	control_notify_session_close(struct session *);

/* session.c */
extern struct sessions sessions;
extern struct session_groups session_groups;
int	session_cmp(struct session *, struct session *);
RB_PROTOTYPE(sessions, session, entry, session_cmp);
int		 session_alive(struct session *);
struct session	*session_find(const char *);
struct session	*session_find_by_id_str(const char *);
struct session	*session_find_by_id(u_int);
struct session	*session_create(const char *, int, char **, const char *,
		     const char *, struct environ *, struct termios *, int,
		     u_int, u_int, char **);
void		 session_destroy(struct session *);
void		 session_unref(struct session *);
int		 session_check_name(const char *);
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
struct session_group *session_group_find(struct session *);
u_int		 session_group_index(struct session_group *);
void		 session_group_add(struct session *, struct session *);
void		 session_group_remove(struct session *);
u_int		 session_group_count(struct session_group *);
void		 session_group_synchronize_to(struct session *);
void		 session_group_synchronize_from(struct session *);
void		 session_group_synchronize1(struct session *, struct session *);
void		 session_renumber_windows(struct session *);

/* utf8.c */
void		 utf8_set(struct utf8_data *, u_char);
void		 utf8_copy(struct utf8_data *, const struct utf8_data *);
enum utf8_state	 utf8_open(struct utf8_data *, u_char);
enum utf8_state	 utf8_append(struct utf8_data *, u_char);
u_int		 utf8_width(wchar_t);
wchar_t		 utf8_combine(const struct utf8_data *);
enum utf8_state	 utf8_split(wchar_t, struct utf8_data *);
int		 utf8_strvis(char *, const char *, size_t, int);
char		*utf8_sanitize(const char *);
struct utf8_data *utf8_fromcstr(const char *);
char		*utf8_tocstr(struct utf8_data *);
u_int		 utf8_cstrwidth(const char *);
char		*utf8_rtrimcstr(const char *, u_int);
char		*utf8_trimcstr(const char *, u_int);
char		*utf8_padcstr(const char *, u_int);

/* osdep-*.c */
char		*osdep_get_name(int, char *);
char		*osdep_get_cwd(int);
struct event_base *osdep_event_init(void);

/* log.c */
void	log_add_level(void);
int	log_get_level(void);
void	log_open(const char *);
void	log_close(void);
void printflike(1, 2) log_debug(const char *, ...);
__dead void printflike(1, 2) fatal(const char *, ...);
__dead void printflike(1, 2) fatalx(const char *, ...);

/* style.c */
int		 style_parse(const struct grid_cell *,
		     struct grid_cell *, const char *);
const char	*style_tostring(struct grid_cell *);
void		 style_update_new(struct options *, const char *, const char *);
void		 style_update_old(struct options *, const char *,
		     struct grid_cell *);
void		 style_apply(struct grid_cell *, struct options *,
		     const char *);
void		 style_apply_update(struct grid_cell *, struct options *,
		     const char *);
int		 style_equal(const struct grid_cell *,
		     const struct grid_cell *);

#endif /* TMUX_H */
