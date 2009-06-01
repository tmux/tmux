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

#ifndef TMUX_H
#define TMUX_H

#define PROTOCOL_VERSION -13

#include <sys/param.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>

#include "array.h"

extern const char    *__progname;

/* Default configuration file. */
#define DEFAULT_CFG ".tmux.conf"

/* Default prompt history length. */
#define PROMPT_HISTORY 100

/* Minimum pane size. */
#define PANE_MINIMUM 4	/* includes separator line */

/* Automatic name refresh interval, in milliseconds. */
#define NAME_INTERVAL 500

/* Escape timer period, in milliseconds. */
#define ESCAPE_PERIOD 250

/* Maximum poll timeout (when attached). */
#define POLL_TIMEOUT 50

/* Fatal errors. */
#define fatal(msg) log_fatal("%s: %s", __func__, msg);
#define fatalx(msg) log_fatalx("%s: %s", __func__, msg);

/* Definition to shut gcc up about unused arguments. */
#define unused __attribute__ ((unused))

/* Attribute to make gcc check printf-like arguments. */
#define printflike1 __attribute__ ((format (printf, 1, 2)))
#define printflike2 __attribute__ ((format (printf, 2, 3)))
#define printflike3 __attribute__ ((format (printf, 3, 4)))
#define printflike4 __attribute__ ((format (printf, 4, 5)))

/* Number of items in array. */
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))

/* Buffer macros. */
#define BUFFER_USED(b) ((b)->size)
#define BUFFER_FREE(b) ((b)->space - (b)->off - (b)->size)
#define BUFFER_IN(b) ((b)->base + (b)->off + (b)->size)
#define BUFFER_OUT(b) ((b)->base + (b)->off)

/* Buffer structure. */
struct buffer {
	u_char	*base;		/* buffer start */
	size_t	 space;		/* total size of buffer */

	size_t	 size;		/* size of data in buffer */
	size_t	 off;		/* offset of data in buffer */
};

/* Bell option values. */
#define BELL_NONE 0
#define BELL_ANY 1
#define BELL_CURRENT 2

/* Key codes. ncurses defines KEY_*. Grrr. */
#define KEYC_NONE    0x00ffff
#define KEYC_OFFSET  0x010000
#define KEYC_ESCAPE  0x020000
#define KEYC_CONTROL 0x080000
#define KEYC_SHIFT   0x100000

#define KEYC_ADDESC(k) ((k) | KEYC_ESCAPE)
#define KEYC_REMOVEESC(k) ((k) & ~KEYC_ESCAPE)
#define KEYC_ISESC(k) ((k) != KEYC_NONE && ((k) & KEYC_ESCAPE))

#define KEYC_ADDCTL(k) ((k) | KEYC_CONTROL)
#define KEYC_REMOVECTL(k) ((k) & ~KEYC_CONTROL)
#define KEYC_ISCTL(k) ((k) != KEYC_NONE && ((k) & KEYC_CONTROL))

#define KEYC_ADDSFT(k) ((k) | KEYC_SHIFT)
#define KEYC_REMOVESFT(k) ((k) & ~KEYC_SHIFT)
#define KEYC_ISSFT(k) ((k) != KEYC_NONE && ((k) & KEYC_SHIFT))

/* Mouse key. */
#define KEYC_MOUSE (KEYC_OFFSET + 0x00)

/* Function keys. */
#define KEYC_F1 (KEYC_OFFSET + 0x01)
#define KEYC_F2 (KEYC_OFFSET + 0x02)
#define KEYC_F3 (KEYC_OFFSET + 0x03)
#define KEYC_F4 (KEYC_OFFSET + 0x04)
#define KEYC_F5 (KEYC_OFFSET + 0x05)
#define KEYC_F6 (KEYC_OFFSET + 0x06)
#define KEYC_F7 (KEYC_OFFSET + 0x07)
#define KEYC_F8 (KEYC_OFFSET + 0x08)
#define KEYC_F9 (KEYC_OFFSET + 0x09)
#define KEYC_F10 (KEYC_OFFSET + 0x10)
#define KEYC_F11 (KEYC_OFFSET + 0x11)
#define KEYC_F12 (KEYC_OFFSET + 0x12)
#define KEYC_F13 (KEYC_OFFSET + 0x13)
#define KEYC_F14 (KEYC_OFFSET + 0x14)
#define KEYC_F15 (KEYC_OFFSET + 0x15)
#define KEYC_F16 (KEYC_OFFSET + 0x16)
#define KEYC_F17 (KEYC_OFFSET + 0x17)
#define KEYC_F18 (KEYC_OFFSET + 0x18)
#define KEYC_F19 (KEYC_OFFSET + 0x19)
#define KEYC_F20 (KEYC_OFFSET + 0x1a)
#define KEYC_IC (KEYC_OFFSET + 0x1b)
#define KEYC_DC (KEYC_OFFSET + 0x1c)
#define KEYC_HOME (KEYC_OFFSET + 0x1d)
#define KEYC_END (KEYC_OFFSET + 0x1e)
#define KEYC_NPAGE (KEYC_OFFSET + 0x1f)
#define KEYC_PPAGE (KEYC_OFFSET + 0x20)
#define KEYC_BTAB (KEYC_OFFSET + 0x21)

/* Arrow keys. */
#define KEYC_UP (KEYC_OFFSET + 0x50)
#define KEYC_DOWN (KEYC_OFFSET + 0x51)
#define KEYC_LEFT (KEYC_OFFSET + 0x52)
#define KEYC_RIGHT (KEYC_OFFSET + 0x53)

/* Numeric keypad. Numbered from top-left, KPY_X. */
#define KEYC_KP0_1 (KEYC_OFFSET + 0x100)
#define KEYC_KP0_2 (KEYC_OFFSET + 0x101)
#define KEYC_KP0_3 (KEYC_OFFSET + 0x102)
#define KEYC_KP1_0 (KEYC_OFFSET + 0x103)
#define KEYC_KP1_1 (KEYC_OFFSET + 0x104)
#define KEYC_KP1_2 (KEYC_OFFSET + 0x105)
#define KEYC_KP1_3 (KEYC_OFFSET + 0x106)
#define KEYC_KP2_0 (KEYC_OFFSET + 0x107)
#define KEYC_KP2_1 (KEYC_OFFSET + 0x108)
#define KEYC_KP2_2 (KEYC_OFFSET + 0x109)
#define KEYC_KP3_0 (KEYC_OFFSET + 0x10a)
#define KEYC_KP3_1 (KEYC_OFFSET + 0x10b)
#define KEYC_KP3_2 (KEYC_OFFSET + 0x10c)
#define KEYC_KP3_3 (KEYC_OFFSET + 0x10d)
#define KEYC_KP4_0 (KEYC_OFFSET + 0x10e)
#define KEYC_KP4_2 (KEYC_OFFSET + 0x10f)

/* Termcap codes. */
enum tty_code_code {
	TTYC_AX = 0,
	TTYC_ACSC,	/* acs_chars, ac */
	TTYC_BEL,	/* bell, bl */
	TTYC_BLINK,	/* enter_blink_mode, mb */
	TTYC_BOLD,	/* enter_bold_mode, md */
	TTYC_CIVIS,	/* cursor_invisible, vi */
	TTYC_CLEAR,	/* clear_screen, cl */
	TTYC_CNORM,	/* cursor_normal, ve */
	TTYC_COLORS,	/* max_colors, Co */
	TTYC_CSR,	/* change_scroll_region, cs */
	TTYC_CUD,	/* parm_down_cursor, DO */
	TTYC_CUD1,	/* cursor_down, do */
	TTYC_CUP,	/* cursor_address, cm */
	TTYC_DCH,	/* parm_dch, DC */
	TTYC_DCH1,	/* delete_character, dc */
	TTYC_DIM,	/* enter_dim_mode, mh */
	TTYC_DL,	/* parm_delete_line, DL */
	TTYC_DL1,	/* delete_line, dl */
	TTYC_EL,	/* clr_eol, ce */
	TTYC_EL1,	/* clr_bol, cb */
	TTYC_ENACS,	/* ena_acs, eA */
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
	TTYC_KDCH1,	/* key_dc, kD */
	TTYC_KEND,	/* key_end, ke */
	TTYC_KF1,	/* key_f1, k1 */
	TTYC_KF10,	/* key_f10, k; */
	TTYC_KF11,	/* key_f11, F1 */
	TTYC_KF12,	/* key_f12, F2 */
	TTYC_KF13,	/* key_f13, F3 */
	TTYC_KF14,	/* key_f14, F4 */
	TTYC_KF15,	/* key_f15, F5 */
	TTYC_KF16,	/* key_f16, F6 */
	TTYC_KF17,	/* key_f17, F7 */
	TTYC_KF18,	/* key_f18, F8 */
	TTYC_KF19,	/* key_f19, F9 */
	TTYC_KF20,	/* key_f20, F10 */
	TTYC_KF2,	/* key_f2, k2 */
	TTYC_KF3,	/* key_f3, k3 */
	TTYC_KF4,	/* key_f4, k4 */
	TTYC_KF5,	/* key_f5, k5 */
	TTYC_KF6,	/* key_f6, k6 */
	TTYC_KF7,	/* key_f7, k7 */
	TTYC_KF8,	/* key_f8, k8 */
	TTYC_KF9,	/* key_f9, k9 */
	TTYC_KHOME,	/* key_home, kh */
	TTYC_KICH1,	/* key_ic, kI */
	TTYC_KMOUS,	/* key_mouse, Km */
	TTYC_KNP,	/* key_npage, kN */
	TTYC_KPP,	/* key_ppage, kP */
	TTYC_OP,	/* orig_pair, op */
	TTYC_REV,	/* enter_reverse_mode, mr */
	TTYC_RI,	/* scroll_reverse, sr */
	TTYC_RMACS,	/* exit_alt_charset_mode */
	TTYC_RMCUP,	/* exit_ca_mode, te */
	TTYC_RMIR,	/* exit_insert_mode, ei */
	TTYC_RMKX,	/* keypad_local, ke */
	TTYC_SETAB,	/* set_a_background, AB */
	TTYC_SETAF,	/* set_a_foreground, AF */
	TTYC_SGR0,	/* exit_attribute_mode, me */
	TTYC_SMACS,	/* enter_alt_charset_mode, as */
	TTYC_SMCUP,	/* enter_ca_mode, ti */
	TTYC_SMIR,	/* enter_insert_mode, im */
	TTYC_SMKX,	/* keypad_xmit, ks */
	TTYC_SMSO,	/* enter_standout_mode, so */
	TTYC_SMUL,	/* enter_underline_mode, us */
	TTYC_XENL,	/* eat_newline_glitch, xn */
};
#define NTTYCODE (TTYC_XENL + 1)

/* Termcap types. */
enum tty_code_type {
	TTYCODE_NONE = 0,
	TTYCODE_STRING,
	TTYCODE_NUMBER,
	TTYCODE_FLAG,
};

/* Termcap code. */
struct tty_code {
	enum tty_code_type	type;
	union {
		char 	       *string;
		int	 	number;
		int	 	flag;
	} value;
};

/* Entry in terminal code table. */
struct tty_term_code_entry {
	enum tty_code_code	code;
	enum tty_code_type	type;
	const char	       *name;
};

/* Output commands. */
enum tty_cmd {
	TTY_CELL,
	TTY_CLEARENDOFLINE,
	TTY_CLEARENDOFSCREEN,
	TTY_CLEARLINE,
	TTY_CLEARSCREEN,
	TTY_CLEARSTARTOFLINE,
	TTY_CLEARSTARTOFSCREEN,
	TTY_DELETECHARACTER,
	TTY_DELETELINE,
	TTY_INSERTCHARACTER,
	TTY_INSERTLINE,
	TTY_LINEFEED,
	TTY_RAW,
	TTY_REVERSEINDEX,
};

/* Message codes. */
enum hdrtype {
	MSG_COMMAND,
	MSG_DETACH,
	MSG_ERROR,
	MSG_EXIT,
	MSG_EXITED,
	MSG_EXITING,
	MSG_IDENTIFY,
	MSG_PRINT,
	MSG_READY,
	MSG_RESIZE,
	MSG_SHUTDOWN,
	MSG_SUSPEND,
	MSG_UNLOCK,
	MSG_WAKEUP,
};

/* Message header structure. */
struct hdr {
	enum hdrtype	type;
	size_t		size;
};

struct msg_command_data {
	pid_t		pid;			/* pid from $TMUX or -1 */
	u_int		idx;			/* index from $TMUX */

	size_t		namelen;
};

struct msg_identify_data {
	char		tty[TTY_NAME_MAX];
	int	        version;

	char		cwd[MAXPATHLEN];

#define IDENTIFY_UTF8 0x1
#define IDENTIFY_256COLOURS 0x2
#define IDENTIFY_88COLOURS 0x4
#define IDENTIFY_HASDEFAULTS 0x8
	int		flags;

	u_int		sx;
	u_int		sy;

	size_t		termlen;
};

struct msg_resize_data {
	u_int		sx;
	u_int		sy;
};

/* Editing keys. */
enum mode_key_cmd {
	MODEKEYCMD_BACKSPACE = 0x1000,
	MODEKEYCMD_CHOOSE,
	MODEKEYCMD_CLEARSELECTION,
	MODEKEYCMD_COMPLETE,
	MODEKEYCMD_COPYSELECTION,
	MODEKEYCMD_DELETE,
	MODEKEYCMD_DOWN,
	MODEKEYCMD_ENDOFLINE,
	MODEKEYCMD_LEFT,
	MODEKEYCMD_NEXTPAGE,
	MODEKEYCMD_NEXTWORD,
	MODEKEYCMD_NONE,
	MODEKEYCMD_OTHERKEY,
	MODEKEYCMD_PASTE,
	MODEKEYCMD_PREVIOUSPAGE,
	MODEKEYCMD_PREVIOUSWORD,
	MODEKEYCMD_QUIT,
	MODEKEYCMD_RIGHT,
	MODEKEYCMD_STARTOFLINE,
	MODEKEYCMD_STARTSELECTION,
	MODEKEYCMD_UP,
};

struct mode_key_data {
	int			 type;

	int			 flags;
#define MODEKEY_EDITMODE 0x1
#define MODEKEY_CANEDIT 0x2
#define MODEKEY_CHOOSEMODE 0x4
};

#define MODEKEY_EMACS 0
#define MODEKEY_VI 1

/* Modes. */
#define MODE_CURSOR 0x1
#define MODE_INSERT 0x2
#define MODE_KCURSOR 0x4
#define MODE_KKEYPAD 0x8
#define MODE_MOUSE 0x10

/* Grid output. */
#if defined(DEBUG) && \
    ((defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || \
     (defined(__GNUC__) && __GNUC__ >= 3))
#define GRID_DEBUG(gd, fmt, ...) log_debug3("%s: (sx=%u, sy=%u, hsize=%u) " \
    fmt, __func__, (gd)->sx, (gd)->sy, (gd)->hsize, ## __VA_ARGS__)
#else
#define GRID_DEBUG(...)
#endif

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
#define GRID_FLAG_UTF8 0x8

/* Grid cell data. */
struct grid_cell {
	u_char	attr;
	u_char	flags;
	u_char	fg;
	u_char	bg;
	u_char	data;
} __packed;

/* Grid cell UTF-8 data. Used instead of data in grid_cell for UTF-8 cells. */
#define UTF8_SIZE 8
struct grid_utf8 {
	u_char	width;
	u_char	data[UTF8_SIZE];
} __packed;

/* Entire grid of cells. */
struct grid {
	u_int	sx;
	u_int	sy;

	u_int	hsize;
	u_int	hlimit;

	u_int  *size;
	struct grid_cell **data;

	u_int  *usize;
 	struct grid_utf8 **udata;
};

/* Option data structures. */
struct options_entry {
	char		*name;

	enum {
		OPTIONS_STRING,
		OPTIONS_NUMBER,
		OPTIONS_KEY,
	} type;
	union {
		char	*string;
		long long number;
		int	 key;
	} value;

	SPLAY_ENTRY(options_entry) entry;
};

struct options {
	SPLAY_HEAD(options_tree, options_entry) tree;
	struct options	*parent;
};

/* Screen selection. */
struct screen_sel {
	int		 flag;

	u_int		 sx;
	u_int		 sy;

	u_int		 ex;
	u_int		 ey;

	struct grid_cell cell;
};

/* Virtual screen. */
struct screen {
	char		*title;

	struct grid     *grid;		/* grid data */

	u_int		 cx;		/* cursor x */
	u_int		 cy;		/* cursor y */

	u_int		 old_cx;
	u_int		 old_cy;

	u_int		 rupper;	/* scroll region top */
	u_int		 rlower;	/* scroll region bottom */

	u_int		 old_rupper;
	u_int		 old_rlower;

	int		 mode;

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

/* Input parser sequence argument. */
struct input_arg {
	u_char		 data[64];
	size_t		 used;
};

/* Input parser context. */
struct input_ctx {
	struct window_pane *wp;
	struct screen_write_ctx ctx;

	u_char		*buf;
	size_t		 len;
	size_t		 off;

	struct grid_cell cell;


	struct grid_cell saved_cell;
	u_int		 saved_cx;
	u_int		 saved_cy;

#define MAXSTRINGLEN	1024
	u_char		*string_buf;
	size_t		 string_len;
	int		 string_type;
#define STRING_SYSTEM 0
#define STRING_APPLICATION 1
#define STRING_NAME 2

	u_char		 utf8_buf[4];
	u_int		 utf8_len;
	u_int		 utf8_off;

	u_char		 intermediate;
	void 		*(*state)(u_char, struct input_ctx *);

	u_char		 private;
	ARRAY_DECL(, struct input_arg) args;
};

/*
 * Window mode. Windows can be in several modes and this is used to call the
 * right function to handle input and output.
 */
struct client;
struct window;
struct window_mode {
	struct screen *(*init)(struct window_pane *);
	void	(*free)(struct window_pane *);
	void	(*resize)(struct window_pane *, u_int, u_int);
	void	(*key)(struct window_pane *, struct client *, int);
	void	(*mouse)(struct window_pane *,
	    	    struct client *, u_char, u_char, u_char);
	void	(*timer)(struct window_pane *);
};

/* Child window structure. */
struct window_pane {
	struct window	*window;

	u_int		 sx;
	u_int		 sy;

	u_int		 xoff;
	u_int		 yoff;

	int		 flags;
#define PANE_HIDDEN 0x1
#define PANE_RESTART 0x2
#define PANE_REDRAW 0x4

	char		*cmd;
	char		*cwd;

	pid_t		 pid;
	int		 fd;
	char		 tty[TTY_NAME_MAX];
	struct buffer	*in;
	struct buffer	*out;

	struct input_ctx ictx;

	struct screen	*screen;
	struct screen	 base;

	const struct window_mode *mode;
	void		*modedata;

	TAILQ_ENTRY(window_pane) entry;
};
TAILQ_HEAD(window_panes, window_pane);

/* Window structure. */
struct window {
	char		*name;
	struct timeval	 name_timer;

	struct window_pane *active;
	struct window_panes panes;
	u_int		 layout;

	u_int		 sx;
	u_int		 sy;

	int		 flags;
#define WINDOW_BELL 0x1
#define WINDOW_HIDDEN 0x2
#define WINDOW_ACTIVITY 0x4
#define WINDOW_CONTENT 0x6
#define WINDOW_REDRAW 0x8

	struct options	 options;

	u_int		 references;
};
ARRAY_DECL(windows, struct window *);

/* Entry on local window list. */
struct winlink {
	int		 idx;
	struct window	*window;

	RB_ENTRY(winlink) entry;
	SLIST_ENTRY(winlink) sentry;
};
RB_HEAD(winlinks, winlink);
SLIST_HEAD(winlink_stack, winlink);

/* Paste buffer. */
struct paste_buffer {
     	char		*data;
	struct timeval	 tv;
};
ARRAY_DECL(paste_stack, struct paste_buffer *);

/* Client session. */
struct session_alert {
	struct winlink	*wl;
	int		 type;

	SLIST_ENTRY(session_alert) entry;
};

struct session {
	char		*name;
	struct timeval	 tv;

	u_int		 sx;
	u_int		 sy;

	struct winlink	*curw;
	struct winlink_stack lastw;
	struct winlinks	 windows;

	struct options	 options;

	struct paste_stack buffers;

	SLIST_HEAD(, session_alert) alerts;

#define SESSION_UNATTACHED 0x1	/* not attached to any clients */
	int		 flags;
};
ARRAY_DECL(sessions, struct session *);

/* TTY information. */
struct tty_key {
	int	 	 key;
	char		*string;

	int		 flags;
#define TTYKEY_CTRL 0x1
#define TTYKEY_RAW 0x2

	RB_ENTRY(tty_key) entry;
};

struct tty_term {
	char		*name;
	u_int		 references;

	struct tty_code	 codes[NTTYCODE];

#define TERM_HASDEFAULTS 0x1
#define TERM_256COLOURS 0x2
#define TERM_88COLOURS 0x4
#define TERM_EARLYWRAP 0x8
	int		 flags;

	SLIST_ENTRY(tty_term) entry;
};
SLIST_HEAD(tty_terms, tty_term);

struct tty {
	char		*path;

        u_int            sx;
        u_int            sy;

	u_int		 cx;
	u_int		 cy;

	int		 mode;

	u_int		 rlower;
	u_int		 rupper;

	char		*termname;
	struct tty_term	*term;

	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	int		 log_fd;

	struct termios   tio;

	struct grid_cell cell;

	u_char		 acs[UCHAR_MAX + 1];

#define TTY_NOCURSOR 0x1
#define TTY_FREEZE 0x2
#define TTY_ESCAPE 0x4
#define TTY_UTF8 0x8
	int    		 flags;

	int		 term_flags;

	struct timeval	 key_timer;

	size_t		 ksize;	/* maximum key size */
	RB_HEAD(tty_keys, tty_key) ktree;
};

/* Client connection. */
struct client {
	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	char		*title;
	char		*cwd;

	struct tty 	 tty;
	struct timeval	 status_timer;
	struct timeval	 repeat_timer;

	struct screen	 status;

#define CLIENT_TERMINAL 0x1
#define CLIENT_PREFIX 0x2
#define CLIENT_MOUSE 0x4
#define CLIENT_REDRAW 0x8
#define CLIENT_STATUS 0x10
#define CLIENT_REPEAT 0x20	/* allow command to repeat within repeat time */
#define CLIENT_SUSPENDED 0x40
	int		 flags;

	char		*message_string;
	struct timeval	 message_timer;

	char		*prompt_string;
	char		*prompt_buffer;
	size_t		 prompt_index;
	int		 (*prompt_callback)(void *, const char *);
	void		*prompt_data;

#define PROMPT_HIDDEN 0x1
#define PROMPT_SINGLE 0x2
	int		 prompt_flags;

	u_int		 prompt_hindex;
	ARRAY_DECL(, char *) prompt_hdata;

	struct mode_key_data prompt_mdata;

	struct session	*session;
};
ARRAY_DECL(clients, struct client *);

/* Client context. */
struct client_ctx {
	int		 srv_fd;
	struct buffer	*srv_in;
	struct buffer	*srv_out;

#define CCTX_DETACH 0x1
#define CCTX_EXIT 0x2
#define CCTX_SHUTDOWN 0x4
	int 		 flags;
};

/* Key/command line command. */
struct cmd_ctx {
	struct client  *cmdclient;

	struct client  *curclient;
	struct session *cursession;
	struct msg_command_data	*msgdata;

	void		(*print)(struct cmd_ctx *, const char *, ...);
	void		(*info)(struct cmd_ctx *, const char *, ...);
	void		(*error)(struct cmd_ctx *, const char *, ...);
};

struct cmd {
	const struct cmd_entry *entry;
	void	       	*data;

	TAILQ_ENTRY(cmd) qentry;
};
TAILQ_HEAD(cmd_list, cmd);

struct cmd_entry {
	const char	*name;
	const char	*alias;
	const char	*usage;

#define CMD_STARTSERVER 0x1
#define CMD_CANTNEST 0x2
#define CMD_ARG1 0x4
#define CMD_ARG01 0x8
#define CMD_AFLAG 0x10
#define CMD_DFLAG 0x20
#define CMD_GFLAG 0x40
#define CMD_KFLAG 0x80
#define CMD_UFLAG 0x100
#define CMD_BIGDFLAG 0x200
#define CMD_BIGUFLAG 0x400

	int		 flags;

	void		 (*init)(struct cmd *, int);
	int		 (*parse)(struct cmd *, int, char **, char **);
	int		 (*exec)(struct cmd *, struct cmd_ctx *);
	void		 (*send)(struct cmd *, struct buffer *);
	void	         (*recv)(struct cmd *, struct buffer *);
	void		 (*free)(struct cmd *);
	size_t 		 (*print)(struct cmd *, char *, size_t);
};

/* Generic command data. */
struct cmd_target_data {
	int	 flags;
	char	*target;
	char	*arg;
};

struct cmd_srcdst_data {
	int	 flags;
	char	*src;
	char	*dst;
	char	*arg;
};

struct cmd_buffer_data {
	int	 flags;
	char	*target;
	int	 buffer;
	char	*arg;
};

struct cmd_option_data {
	int	 flags;
	char	*target;
	char	*option;
	char	*value;
};

struct cmd_pane_data {
	int	 flags;
	char	*target;
	char	*arg;
	int	 pane;
};

/* Key binding. */
struct key_binding {
	int		 key;
	struct cmd_list	*cmdlist;
	int		 can_repeat;

	SPLAY_ENTRY(key_binding) entry;
};
SPLAY_HEAD(key_bindings, key_binding);

/* Set/display option data. */
struct set_option_entry {
	const char	*name;
	enum {
		SET_OPTION_STRING,
		SET_OPTION_NUMBER,
		SET_OPTION_KEY,
		SET_OPTION_COLOUR,
		SET_OPTION_ATTRIBUTES,
		SET_OPTION_FLAG,
		SET_OPTION_CHOICE
	} type;

	u_int		 minimum;
	u_int		 maximum;

	const char     **choices;
};
extern const struct set_option_entry set_option_table[];
extern const struct set_option_entry set_window_option_table[];
#define NSETOPTION 24
#define NSETWINDOWOPTION 19

/* tmux.c */
extern volatile sig_atomic_t sigwinch;
extern volatile sig_atomic_t sigterm;
extern volatile sig_atomic_t sigcont;
extern volatile sig_atomic_t sigchld;
extern volatile sig_atomic_t sigusr1;
extern volatile sig_atomic_t sigusr2;
extern struct options global_options;
extern struct options global_window_options;
extern char	*cfg_file;
extern int	 server_locked;
extern char	*server_password;
extern time_t	 server_activity;
extern int	 debug_level;
extern int	 be_quiet;
extern time_t	 start_time;
extern char 	*socket_path;
void		 logfile(const char *);
void		 siginit(void);
void		 sigreset(void);
void		 sighandler(int);

/* cfg.c */
int		 load_cfg(const char *, char **x);

/* mode-key.c */
void		 mode_key_init(struct mode_key_data *, int, int);
void		 mode_key_free(struct mode_key_data *);
enum mode_key_cmd mode_key_lookup(struct mode_key_data *, int);

/* options.c */
int	options_cmp(struct options_entry *, struct options_entry *);
SPLAY_PROTOTYPE(options_tree, options_entry, entry, options_cmp);
void	options_init(struct options *, struct options *);
void	options_free(struct options *);
struct options_entry *options_find1(struct options *, const char *);
struct options_entry *options_find(struct options *, const char *);
int	options_remove(struct options *, const char *);
void printflike3 options_set_string(
    	    struct options *, const char *, const char *, ...);
char   *options_get_string(struct options *, const char *);
void	options_set_number(struct options *, const char *, long long);
long long options_get_number(struct options *, const char *);

/* tty.c */
u_char		 tty_get_acs(struct tty *, u_char);
void		 tty_emulate_repeat(struct tty *,
    		     enum tty_code_code, enum tty_code_code, u_int);
void		 tty_reset(struct tty *);
void		 tty_region(struct tty *, u_int, u_int, u_int);
void		 tty_cursor(struct tty *, u_int, u_int, u_int, u_int);
void		 tty_cell(struct tty *,
    		     const struct grid_cell *, const struct grid_utf8 *);
void		 tty_putcode(struct tty *, enum tty_code_code);
void		 tty_putcode1(struct tty *, enum tty_code_code, int);
void		 tty_putcode2(struct tty *, enum tty_code_code, int, int);
void		 tty_puts(struct tty *, const char *);
void		 tty_putc(struct tty *, u_char);
void		 tty_init(struct tty *, char *, char *);
void		 tty_start_tty(struct tty *);
void		 tty_stop_tty(struct tty *);
void		 tty_detect_utf8(struct tty *);
void		 tty_set_title(struct tty *, const char *);
void		 tty_update_mode(struct tty *, int);
void		 tty_draw_line(
    		     struct tty *, struct screen *, u_int, u_int, u_int);
void		 tty_redraw_region(struct tty *, struct window_pane *);
int		 tty_open(struct tty *, char **);
void		 tty_close(struct tty *, int);
void		 tty_free(struct tty *, int);
void		 tty_write(
		     struct tty *, struct window_pane *, enum tty_cmd, ...);
void		 tty_vwrite(
    		     struct tty *, struct window_pane *, enum tty_cmd, va_list);

/* tty-term.c */
extern struct tty_terms tty_terms;
extern struct tty_term_code_entry tty_term_codes[NTTYCODE];
struct tty_term *tty_term_find(char *, int, char **);
void 		 tty_term_free(struct tty_term *);
int		 tty_term_has(struct tty_term *, enum tty_code_code);
const char	*tty_term_string(struct tty_term *, enum tty_code_code);
const char	*tty_term_string1(struct tty_term *, enum tty_code_code, int);
const char	*tty_term_string2(
    		     struct tty_term *, enum tty_code_code, int, int);
int		 tty_term_number(struct tty_term *, enum tty_code_code);
int		 tty_term_flag(struct tty_term *, enum tty_code_code);

/* tty-keys.c */
int		 tty_keys_cmp(struct tty_key *, struct tty_key *);
RB_PROTOTYPE(tty_keys, tty_key, entry, tty_keys_cmp);
void		 tty_keys_init(struct tty *);
void		 tty_keys_free(struct tty *);
int		 tty_keys_next(struct tty *, int *, u_char *);

/* tty-write.c */
void		 tty_write_cmd(struct window_pane *, enum tty_cmd, ...);
void		 tty_write_mode(struct window_pane *, int);

/* options-cmd.c */
void	set_option_string(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);
void	set_option_number(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_key(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_colour(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_attributes(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_flag(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_choice(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);

/* paste.c */
void		 paste_init_stack(struct paste_stack *);
void		 paste_free_stack(struct paste_stack *);
struct paste_buffer *paste_walk_stack(struct paste_stack *, uint *);
struct paste_buffer *paste_get_top(struct paste_stack *);
struct paste_buffer *paste_get_index(struct paste_stack *, u_int);
int	     	 paste_free_top(struct paste_stack *);
int		 paste_free_index(struct paste_stack *, u_int);
void		 paste_add(struct paste_stack *, char *, u_int);
int		 paste_replace(struct paste_stack *, u_int, char *);

/* clock.c */
void		 clock_draw(struct screen_write_ctx *, u_int, int);

/* arg.c */
struct client 	*arg_parse_client(const char *);
struct session 	*arg_parse_session(const char *);
int		 arg_parse_window(const char *, struct session **, int *);

/* cmd.c */
struct cmd	*cmd_parse(int, char **, char **);
int		 cmd_exec(struct cmd *, struct cmd_ctx *);
void		 cmd_send(struct cmd *, struct buffer *);
struct cmd	*cmd_recv(struct buffer *);
void		 cmd_free(struct cmd *);
size_t		 cmd_print(struct cmd *, char *, size_t);
void		 cmd_send_string(struct buffer *, const char *);
char		*cmd_recv_string(struct buffer *);
struct session	*cmd_current_session(struct cmd_ctx *);
struct client	*cmd_find_client(struct cmd_ctx *, const char *);
struct session	*cmd_find_session(struct cmd_ctx *, const char *);
struct winlink	*cmd_find_window(
    		     struct cmd_ctx *, const char *, struct session **);
extern const struct cmd_entry *cmd_table[];
extern const struct cmd_entry cmd_attach_session_entry;
extern const struct cmd_entry cmd_bind_key_entry;
extern const struct cmd_entry cmd_break_pane_entry;
extern const struct cmd_entry cmd_choose_session_entry;
extern const struct cmd_entry cmd_choose_window_entry;
extern const struct cmd_entry cmd_clear_history_entry;
extern const struct cmd_entry cmd_clock_mode_entry;
extern const struct cmd_entry cmd_command_prompt_entry;
extern const struct cmd_entry cmd_confirm_before_entry;
extern const struct cmd_entry cmd_copy_buffer_entry;
extern const struct cmd_entry cmd_copy_mode_entry;
extern const struct cmd_entry cmd_delete_buffer_entry;
extern const struct cmd_entry cmd_detach_client_entry;
extern const struct cmd_entry cmd_down_pane_entry;
extern const struct cmd_entry cmd_find_window_entry;
extern const struct cmd_entry cmd_has_session_entry;
extern const struct cmd_entry cmd_kill_pane_entry;
extern const struct cmd_entry cmd_kill_server_entry;
extern const struct cmd_entry cmd_kill_session_entry;
extern const struct cmd_entry cmd_kill_window_entry;
extern const struct cmd_entry cmd_last_window_entry;
extern const struct cmd_entry cmd_link_window_entry;
extern const struct cmd_entry cmd_list_buffers_entry;
extern const struct cmd_entry cmd_list_clients_entry;
extern const struct cmd_entry cmd_list_commands_entry;
extern const struct cmd_entry cmd_list_keys_entry;
extern const struct cmd_entry cmd_list_sessions_entry;
extern const struct cmd_entry cmd_list_windows_entry;
extern const struct cmd_entry cmd_load_buffer_entry;
extern const struct cmd_entry cmd_lock_server_entry;
extern const struct cmd_entry cmd_move_window_entry;
extern const struct cmd_entry cmd_new_session_entry;
extern const struct cmd_entry cmd_new_window_entry;
extern const struct cmd_entry cmd_next_layout_entry;
extern const struct cmd_entry cmd_next_window_entry;
extern const struct cmd_entry cmd_paste_buffer_entry;
extern const struct cmd_entry cmd_previous_layout_entry;
extern const struct cmd_entry cmd_previous_window_entry;
extern const struct cmd_entry cmd_refresh_client_entry;
extern const struct cmd_entry cmd_rename_session_entry;
extern const struct cmd_entry cmd_rename_window_entry;
extern const struct cmd_entry cmd_resize_pane_entry;
extern const struct cmd_entry cmd_respawn_window_entry;
extern const struct cmd_entry cmd_rotate_window_entry;
extern const struct cmd_entry cmd_save_buffer_entry;
extern const struct cmd_entry cmd_scroll_mode_entry;
extern const struct cmd_entry cmd_select_layout_entry;
extern const struct cmd_entry cmd_select_pane_entry;
extern const struct cmd_entry cmd_select_prompt_entry;
extern const struct cmd_entry cmd_select_window_entry;
extern const struct cmd_entry cmd_send_keys_entry;
extern const struct cmd_entry cmd_send_prefix_entry;
extern const struct cmd_entry cmd_server_info_entry;
extern const struct cmd_entry cmd_set_buffer_entry;
extern const struct cmd_entry cmd_set_option_entry;
extern const struct cmd_entry cmd_set_password_entry;
extern const struct cmd_entry cmd_set_window_option_entry;
extern const struct cmd_entry cmd_show_buffer_entry;
extern const struct cmd_entry cmd_show_options_entry;
extern const struct cmd_entry cmd_show_window_options_entry;
extern const struct cmd_entry cmd_source_file_entry;
extern const struct cmd_entry cmd_split_window_entry;
extern const struct cmd_entry cmd_start_server_entry;
extern const struct cmd_entry cmd_suspend_client_entry;
extern const struct cmd_entry cmd_swap_pane_entry;
extern const struct cmd_entry cmd_swap_window_entry;
extern const struct cmd_entry cmd_switch_client_entry;
extern const struct cmd_entry cmd_unbind_key_entry;
extern const struct cmd_entry cmd_unlink_window_entry;
extern const struct cmd_entry cmd_up_pane_entry;

/* cmd-list.c */
struct cmd_list	*cmd_list_parse(int, char **, char **);
int		 cmd_list_exec(struct cmd_list *, struct cmd_ctx *);
void		 cmd_list_send(struct cmd_list *, struct buffer *);
struct cmd_list	*cmd_list_recv(struct buffer *);
void		 cmd_list_free(struct cmd_list *);
size_t		 cmd_list_print(struct cmd_list *, char *, size_t);

/* cmd-string.c */
int	cmd_string_parse(const char *, struct cmd_list **, char **);

/* cmd-generic.c */
size_t  cmd_prarg(char *, size_t, const char *, char *);
#define CMD_TARGET_WINDOW_USAGE "[-t target-window]"
#define CMD_TARGET_SESSION_USAGE "[-t target-session]"
#define CMD_TARGET_CLIENT_USAGE "[-t target-client]"
void	cmd_target_init(struct cmd *, int);
int	cmd_target_parse(struct cmd *, int, char **, char **);
void	cmd_target_send(struct cmd *, struct buffer *);
void	cmd_target_recv(struct cmd *, struct buffer *);
void	cmd_target_free(struct cmd *);
size_t	cmd_target_print(struct cmd *, char *, size_t);
#define CMD_SRCDST_WINDOW_USAGE "[-s src-window] [-t dst-window]"
#define CMD_SRCDST_SESSION_USAGE "[-s src-session] [-t dst-session]"
#define CMD_SRCDST_CLIENT_USAGE "[-s src-client] [-t dst-client]"
void	cmd_srcdst_init(struct cmd *, int);
int	cmd_srcdst_parse(struct cmd *, int, char **, char **);
void	cmd_srcdst_send(struct cmd *, struct buffer *);
void	cmd_srcdst_recv(struct cmd *, struct buffer *);
void	cmd_srcdst_free(struct cmd *);
size_t	cmd_srcdst_print(struct cmd *, char *, size_t);
#define CMD_BUFFER_WINDOW_USAGE "[-b buffer-index] [-t target-window]"
#define CMD_BUFFER_SESSION_USAGE "[-b buffer-index] [-t target-session]"
#define CMD_BUFFER_CLIENT_USAGE "[-b buffer-index] [-t target-client]"
void	cmd_buffer_init(struct cmd *, int);
int	cmd_buffer_parse(struct cmd *, int, char **, char **);
void	cmd_buffer_send(struct cmd *, struct buffer *);
void	cmd_buffer_recv(struct cmd *, struct buffer *);
void	cmd_buffer_free(struct cmd *);
size_t	cmd_buffer_print(struct cmd *, char *, size_t);
#define CMD_OPTION_WINDOW_USAGE "[-gu] [-t target-window] option [value]"
#define CMD_OPTION_SESSION_USAGE "[-gu] [-t target-session] option [value]"
#define CMD_OPTION_CLIENT_USAGE "[-gu] [-t target-client] option [value]"
void	cmd_option_init(struct cmd *, int);
int	cmd_option_parse(struct cmd *, int, char **, char **);
void	cmd_option_send(struct cmd *, struct buffer *);
void	cmd_option_recv(struct cmd *, struct buffer *);
void	cmd_option_free(struct cmd *);
size_t	cmd_option_print(struct cmd *, char *, size_t);
#define CMD_PANE_WINDOW_USAGE "[-t target-window] [-p pane-index]"
#define CMD_PANE_SESSION_USAGE "[-t target-session] [-p pane-index]"
#define CMD_PANE_CLIENT_USAGE "[-t target-client] [-p pane-index]"
void	cmd_pane_init(struct cmd *, int);
int	cmd_pane_parse(struct cmd *, int, char **, char **);
void	cmd_pane_send(struct cmd *, struct buffer *);
void	cmd_pane_recv(struct cmd *, struct buffer *);
void	cmd_pane_free(struct cmd *);
size_t	cmd_pane_print(struct cmd *, char *, size_t);

/* client.c */
int	 client_init(char *, struct client_ctx *, int, int);
int	 client_flush(struct client_ctx *);
int	 client_main(struct client_ctx *);

/* client-msg.c */
int	 client_msg_dispatch(struct client_ctx *, char **);

/* client-fn.c */
void	 client_write_server(struct client_ctx *, enum hdrtype, void *, size_t);
void	 client_write_server2(
    	     struct client_ctx *, enum hdrtype, void *, size_t, void *, size_t);
void	 client_fill_session(struct msg_command_data *);

/* key-bindings.c */
extern struct key_bindings key_bindings;
int	 key_bindings_cmp(struct key_binding *, struct key_binding *);
SPLAY_PROTOTYPE(key_bindings, key_binding, entry, key_bindings_cmp);
struct key_binding *key_bindings_lookup(int);
void	 key_bindings_add(int, int, struct cmd_list *);
void	 key_bindings_remove(int);
void	 key_bindings_init(void);
void	 key_bindings_free(void);
void	 key_bindings_dispatch(struct key_binding *, struct client *);
void printflike2 key_bindings_error(struct cmd_ctx *, const char *, ...);
void printflike2 key_bindings_print(struct cmd_ctx *, const char *, ...);
void printflike2 key_bindings_info(struct cmd_ctx *, const char *, ...);

/* key-string.c */
int	 key_string_lookup_string(const char *);
const char *key_string_lookup_key(int);

/* server.c */
extern struct clients clients;
struct client *server_create_client(int);
int	 server_client_index(struct client *);
int	 server_start(char *);

/* server-msg.c */
int	 server_msg_dispatch(struct client *);

/* server-fn.c */
const char **server_fill_environ(struct session *);
void	 server_write_client(
             struct client *, enum hdrtype, const void *, size_t);
void	 server_write_session(
             struct session *, enum hdrtype, const void *, size_t);
void	 server_write_window(
	     struct window *, enum hdrtype, const void *, size_t);
void	 server_redraw_client(struct client *);
void	 server_status_client(struct client *);
void	 server_redraw_session(struct session *);
void	 server_status_session(struct session *);
void	 server_redraw_window(struct window *);
void	 server_status_window(struct window *);
void	 server_lock(void);
int	 server_unlock(const char *);

/* status.c */
int	 status_redraw(struct client *);
void	 status_message_set(struct client *, const char *);
void	 status_message_clear(struct client *);
int	 status_message_redraw(struct client *);
void	 status_prompt_set(struct client *,
	     const char *, int (*)(void *, const char *), void *, int);
void	 status_prompt_clear(struct client *);
int	 status_prompt_redraw(struct client *);
void	 status_prompt_key(struct client *, int);

/* resize.c */
void	 recalculate_sizes(void);

/* input.c */
void	 input_init(struct window_pane *);
void	 input_free(struct window_pane *);
void	 input_parse(struct window_pane *);

/* input-key.c */
void	 input_key(struct window_pane *, int);
void	 input_mouse(struct window_pane *, u_char, u_char, u_char);

/* colour.c */
const char *colour_tostring(u_char);
int	 colour_fromstring(const char *);
u_char	 colour_256to16(u_char);
u_char	 colour_256to88(u_char);

/* attributes.c */
const char *attributes_tostring(u_char);
int	 attributes_fromstring(const char *);

/* grid.c */
extern const struct grid_cell grid_default_cell;
struct grid *grid_create(u_int, u_int, u_int);
void	 grid_destroy(struct grid *);
int	 grid_compare(struct grid *, struct grid *);
void	 grid_reduce_line(struct grid *, u_int, u_int);
void	 grid_expand_line(struct grid *, u_int, u_int);
void	 grid_expand_line_utf8(struct grid *, u_int, u_int);
void	 grid_scroll_line(struct grid *);
const struct grid_cell *grid_peek_cell(struct grid *, u_int, u_int);
struct grid_cell *grid_get_cell(struct grid *, u_int, u_int);
void	 grid_set_cell(struct grid *, u_int, u_int, const struct grid_cell *);
const struct grid_utf8 *grid_peek_utf8(struct grid *, u_int, u_int);
struct grid_utf8 *grid_get_utf8(struct grid *, u_int, u_int);
void	 grid_set_utf8(struct grid *, u_int, u_int, const struct grid_utf8 *);
void	 grid_clear(struct grid *, u_int, u_int, u_int, u_int);
void	 grid_clear_lines(struct grid *, u_int, u_int);
void	 grid_move_lines(struct grid *, u_int, u_int, u_int);
void	 grid_clear_cells(struct grid *, u_int, u_int, u_int);
void	 grid_move_cells(struct grid *, u_int, u_int, u_int, u_int);

/* grid-view.c */
const struct grid_cell *grid_view_peek_cell(struct grid *, u_int, u_int);
struct grid_cell *grid_view_get_cell(struct grid *, u_int, u_int);
void	 grid_view_set_cell(
    	     struct grid *, u_int, u_int, const struct grid_cell *);
const struct grid_utf8 *grid_view_peek_utf8(struct grid *, u_int, u_int);
struct grid_utf8 *grid_view_get_utf8(struct grid *, u_int, u_int);
void	 grid_view_set_utf8(
    	     struct grid *, u_int, u_int, const struct grid_utf8 *);
void	 grid_view_clear(struct grid *, u_int, u_int, u_int, u_int);
void	 grid_view_scroll_region_up(struct grid *, u_int, u_int);
void	 grid_view_scroll_region_down(struct grid *, u_int, u_int);
void	 grid_view_insert_lines(struct grid *, u_int, u_int);
void	 grid_view_insert_lines_region(
    	     struct grid *, u_int, u_int, u_int, u_int);
void	 grid_view_delete_lines(struct grid *, u_int, u_int);
void	 grid_view_delete_lines_region(
    	     struct grid *, u_int, u_int, u_int, u_int);
void	 grid_view_insert_cells(struct grid *, u_int, u_int, u_int);
void	 grid_view_delete_cells(struct grid *, u_int, u_int, u_int);

/* screen-write.c */
void	 screen_write_start(
    	     struct screen_write_ctx *, struct window_pane *, struct screen *);
void	 screen_write_stop(struct screen_write_ctx *);
void printflike3 screen_write_puts(
	     struct screen_write_ctx *, struct grid_cell *, const char *, ...);
void	 screen_write_putc(
    	     struct screen_write_ctx *, struct grid_cell *, u_char);
void	 screen_write_copy(struct screen_write_ctx *,
	     struct screen *, u_int, u_int, u_int, u_int);
void	 screen_write_cursorup(struct screen_write_ctx *, u_int);
void	 screen_write_cursordown(struct screen_write_ctx *, u_int);
void	 screen_write_cursorright(struct screen_write_ctx *, u_int);
void	 screen_write_cursorleft(struct screen_write_ctx *, u_int);
void	 screen_write_insertcharacter(struct screen_write_ctx *, u_int);
void	 screen_write_deletecharacter(struct screen_write_ctx *, u_int);
void	 screen_write_insertline(struct screen_write_ctx *, u_int);
void	 screen_write_deleteline(struct screen_write_ctx *, u_int);
void	 screen_write_clearline(struct screen_write_ctx *);
void	 screen_write_clearendofline(struct screen_write_ctx *);
void	 screen_write_clearstartofline(struct screen_write_ctx *);
void	 screen_write_cursormove(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_cursormode(struct screen_write_ctx *, int);
void	 screen_write_reverseindex(struct screen_write_ctx *);
void	 screen_write_scrollregion(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_insertmode(struct screen_write_ctx *, int);
void	 screen_write_mousemode(struct screen_write_ctx *, int);
void	 screen_write_linefeed(struct screen_write_ctx *);
void	 screen_write_carriagereturn(struct screen_write_ctx *);
void	 screen_write_kcursormode(struct screen_write_ctx *, int);
void	 screen_write_kkeypadmode(struct screen_write_ctx *, int);
void	 screen_write_clearendofscreen(struct screen_write_ctx *);
void	 screen_write_clearstartofscreen(struct screen_write_ctx *);
void	 screen_write_clearscreen(struct screen_write_ctx *);
void	 screen_write_cell(
    	     struct screen_write_ctx *, const struct grid_cell *, u_char *);

/* screen-redraw.c */
void	 screen_redraw_screen(struct client *);
void	 screen_redraw_pane(struct client *, struct window_pane *);
void	 screen_redraw_status(struct client *);

/* screen.c */
void	 screen_init(struct screen *, u_int, u_int, u_int);
void	 screen_reinit(struct screen *);
void	 screen_free(struct screen *);
void	 screen_set_title(struct screen *, const char *);
void	 screen_resize(struct screen *, u_int, u_int);
void	 screen_set_selection(
	     struct screen *, u_int, u_int, u_int, u_int, struct grid_cell *);
void	 screen_clear_selection(struct screen *);
int	 screen_check_selection(struct screen *, u_int, u_int);
void	 screen_display_copy_area(struct screen *, struct screen *,
    	     u_int, u_int, u_int, u_int, u_int, u_int);

/* window.c */
extern struct windows windows;
int		 window_cmp(struct window *, struct window *);
int		 winlink_cmp(struct winlink *, struct winlink *);
RB_PROTOTYPE(windows, window, entry, window_cmp);
RB_PROTOTYPE(winlinks, winlink, entry, winlink_cmp);
struct winlink	*winlink_find_by_index(struct winlinks *, int);
struct winlink 	*winlink_find_by_window(struct winlinks *, struct window *);
int		 winlink_next_index(struct winlinks *);
u_int		 winlink_count(struct winlinks *);
struct winlink	*winlink_add(struct winlinks *, struct window *, int);
void		 winlink_remove(struct winlinks *, struct winlink *);
struct winlink	*winlink_next(struct winlinks *, struct winlink *);
struct winlink	*winlink_previous(struct winlinks *, struct winlink *);
void		 winlink_stack_push(struct winlink_stack *, struct winlink *);
void		 winlink_stack_remove(struct winlink_stack *, struct winlink *);
int	 	 window_index(struct window *, u_int *);
struct window	*window_create1(u_int, u_int);
struct window	*window_create(const char *, const char *,
		     const char *, const char **, u_int, u_int, u_int, char **);
void		 window_destroy(struct window *);
int		 window_resize(struct window *, u_int, u_int);
void		 window_set_active_pane(struct window *, struct window_pane *);
struct window_pane *window_add_pane(struct window *, int,
		     const char *, const char *, const char **, u_int, char **);
void		 window_remove_pane(struct window *, struct window_pane *);
u_int		 window_index_of_pane(struct window *, struct window_pane *);
struct window_pane *window_pane_at_index(struct window *, u_int);
u_int		 window_count_panes(struct window *);
void		 window_destroy_panes(struct window *);
struct window_pane *window_pane_create(struct window *, u_int, u_int, u_int);
void		 window_pane_destroy(struct window_pane *);
int		 window_pane_spawn(struct window_pane *,
		     const char *, const char *, const char **, char **);
int		 window_pane_resize(struct window_pane *, u_int, u_int);
void		 window_calculate_sizes(struct window *);
int		 window_pane_set_mode(
		     struct window_pane *, const struct window_mode *);
void		 window_pane_reset_mode(struct window_pane *);
void		 window_pane_parse(struct window_pane *);
void		 window_pane_key(struct window_pane *, struct client *, int);
void		 window_pane_mouse(struct window_pane *,
    		     struct client *, u_char, u_char, u_char);
char		*window_pane_search(struct window_pane *, const char *);

/* layout.c */
const char * 	 layout_name(struct window *);
int		 layout_lookup(const char *);
void		 layout_refresh(struct window *, int);
int		 layout_resize(struct window_pane *, int);
int		 layout_select(struct window *, u_int);
void		 layout_next(struct window *);
void		 layout_previous(struct window *);

/* layout-manual.c */
void		 layout_manual_v_refresh(struct window *, int);
void		 layout_manual_v_resize(struct window_pane *, int);

/* window-clock.c */
extern const struct window_mode window_clock_mode;

/* window-copy.c */
extern const struct window_mode window_copy_mode;
void 		 window_copy_pageup(struct window_pane *);

/* window-scroll.c */
extern const struct window_mode window_scroll_mode;
void 		 window_scroll_pageup(struct window_pane *);

/* window-more.c */
extern const struct window_mode window_more_mode;
void 		 window_more_vadd(struct window_pane *, const char *, va_list);
void printflike2 window_more_add(struct window_pane *, const char *, ...);

/* window-choose.c */
extern const struct window_mode window_choose_mode;
void 		 window_choose_vadd(
    		     struct window_pane *, int, const char *, va_list);
void printflike3 window_choose_add(
    		     struct window_pane *, int, const char *, ...);
void		 window_choose_ready(struct window_pane *,
		     u_int, void (*)(void *, int), void *);

/* names.c */
void		 set_window_names(void);
char 		*default_window_name(struct window *);

/* session.c */
extern struct sessions sessions;
void	 session_alert_add(struct session *, struct window *, int);
void	 session_alert_cancel(struct session *, struct winlink *);
int	 session_alert_has(struct session *, struct winlink *, int);
int	 session_alert_has_window(struct session *, struct window *, int);
struct session	*session_find(const char *);
struct session	*session_create(const char *, const char *,
    		     const char *, u_int, u_int, char **);
void	 	 session_destroy(struct session *);
int	 	 session_index(struct session *, u_int *);
struct winlink	*session_new(struct session *,
	    	     const char *, const char *, const char *, int, char **);
struct winlink	*session_attach(
    		     struct session *, struct window *, int, char **);
int		 session_detach(struct session *, struct winlink *);
int		 session_has(struct session *, struct window *);
int		 session_next(struct session *, int);
int		 session_previous(struct session *, int);
int		 session_select(struct session *, int);
int		 session_last(struct session *);

/* utf8.c */
void	utf8_build(void);
int	utf8_width(u_char *);

/* util.c */
char   *section_string(char *, size_t, size_t, size_t);
void	clean_string(const char *, char *, size_t);

/* procname.c */
char   *get_proc_name(int, char *);

/* buffer.c */
struct buffer 	*buffer_create(size_t);
void		 buffer_destroy(struct buffer *);
void		 buffer_clear(struct buffer *);
void		 buffer_ensure(struct buffer *, size_t);
void		 buffer_add(struct buffer *, size_t);
void		 buffer_reverse_add(struct buffer *, size_t);
void		 buffer_remove(struct buffer *, size_t);
void		 buffer_reverse_remove(struct buffer *, size_t);
void		 buffer_insert_range(struct buffer *, size_t, size_t);
void		 buffer_delete_range(struct buffer *, size_t, size_t);
void		 buffer_write(struct buffer *, const void *, size_t);
void		 buffer_read(struct buffer *, void *, size_t);
void	 	 buffer_write8(struct buffer *, uint8_t);
void	 	 buffer_write16(struct buffer *, uint16_t);
uint8_t		 buffer_read8(struct buffer *);
uint16_t 	 buffer_read16(struct buffer *);

/* buffer-poll.c */
void		 buffer_set(
		     struct pollfd *, int, struct buffer *, struct buffer *);
int		 buffer_poll(struct pollfd *, struct buffer *, struct buffer *);
void		 buffer_flush(int, struct buffer *n, struct buffer *);

/* log.c */
#define LOG_FACILITY LOG_DAEMON
void		 log_open_syslog(int);
void		 log_open_tty(int);
void		 log_open_file(int, const char *);
void		 log_close(void);
void		 log_vwrite(int, const char *, va_list);
void		 log_write(int, const char *, ...);
void printflike1 log_warn(const char *, ...);
void printflike1 log_warnx(const char *, ...);
void printflike1 log_info(const char *, ...);
void printflike1 log_debug(const char *, ...);
void printflike1 log_debug2(const char *, ...);
void printflike1 log_debug3(const char *, ...);
__dead void	 log_vfatal(const char *, va_list);
__dead void	 log_fatal(const char *, ...);
__dead void	 log_fatalx(const char *, ...);

/* xmalloc.c */
char		*xstrdup(const char *);
void		*xcalloc(size_t, size_t);
void		*xmalloc(size_t);
void		*xrealloc(void *, size_t, size_t);
void		 xfree(void *);
int printflike2	 xasprintf(char **, const char *, ...);
int		 xvasprintf(char **, const char *, va_list);
int printflike3	 xsnprintf(char *, size_t, const char *, ...);
int		 xvsnprintf(char *, size_t, const char *, va_list);
int printflike3	 printpath(char *, size_t, const char *, ...);

#endif /* TMUX_H */
