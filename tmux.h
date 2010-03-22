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

#define PROTOCOL_VERSION 5

#include <sys/param.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/uio.h>

#include <bitstring.h>
#include <event.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>

#include "array.h"
#include "imsg.h"

extern char    *__progname;
extern char   **environ;

/* Default configuration files. */
#define DEFAULT_CFG ".tmux.conf"
#define SYSTEM_CFG "/etc/tmux.conf"

/* Default prompt history length. */
#define PROMPT_HISTORY 100

/*
 * Minimum layout cell size, NOT including separator line. The scroll region
 * cannot be one line in height so this must be at least two.
 */
#define PANE_MINIMUM 2

/* Automatic name refresh interval, in milliseconds. */
#define NAME_INTERVAL 500

/* Maximum data to buffer for output before suspending reading from panes. */
#define BACKOFF_THRESHOLD 1024

/*
 * Maximum sizes of strings in message data. Don't forget to bump
 * PROTOCOL_VERSION if any of these change!
 */
#define COMMAND_LENGTH 2048	/* packed argv size */
#define TERMINAL_LENGTH 128	/* length of TERM environment variable */
#define PRINT_LENGTH 512	/* printed error/message size */
#define ENVIRON_LENGTH 1024	/* environment variable length */

/*
 * UTF-8 data size. This must be big enough to hold combined characters as well
 * as single.
 */
#define UTF8_SIZE 9

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
#define printflike5 __attribute__ ((format (printf, 5, 6)))

/* Number of items in array. */
#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

/* Bell option values. */
#define BELL_NONE 0
#define BELL_ANY 1
#define BELL_CURRENT 2

/* Special key codes. */
#define KEYC_NONE 0xfff
#define KEYC_BASE 0x1000

/* Key modifier bits. */
#define KEYC_ESCAPE 0x2000
#define KEYC_CTRL 0x4000
#define KEYC_SHIFT 0x8000
#define KEYC_PREFIX 0x10000

/* Other key codes. */
enum key_code {
	/* Mouse key. */
	KEYC_MOUSE = KEYC_BASE,

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
	KEYC_F13,
	KEYC_F14,
	KEYC_F15,
	KEYC_F16,
	KEYC_F17,
	KEYC_F18,
	KEYC_F19,
	KEYC_F20,
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
	TTYC_BEL,	/* bell, bl */
	TTYC_BLINK,	/* enter_blink_mode, mb */
	TTYC_BOLD,	/* enter_bold_mode, md */
	TTYC_CIVIS,	/* cursor_invisible, vi */
	TTYC_CLEAR,	/* clear_screen, cl */
	TTYC_CNORM,	/* cursor_normal, ve */
	TTYC_COLORS,	/* max_colors, Co */
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
	TTYC_DCH,	/* parm_dch, DC */
	TTYC_DCH1,	/* delete_character, dc */
	TTYC_DIM,	/* enter_dim_mode, mh */
	TTYC_DL,	/* parm_delete_line, DL */
	TTYC_DL1,	/* delete_line, dl */
	TTYC_EL,	/* clr_eol, ce */
	TTYC_EL1,	/* clr_bol, cb */
	TTYC_ENACS,	/* ena_acs, eA */
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
	TTYC_KF2,	/* key_f2, k2 */
	TTYC_KF20,	/* key_f20, F10 */
	TTYC_KF3,	/* key_f3, k3 */
	TTYC_KF4,	/* key_f4, k4 */
	TTYC_KF5,	/* key_f5, k5 */
	TTYC_KF6,	/* key_f6, k6 */
	TTYC_KF7,	/* key_f7, k7 */
	TTYC_KF8,	/* key_f8, k8 */
	TTYC_KF9,	/* key_f9, k9 */
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
	TTYC_VPA,	/* row_address, cv */
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
		char	       *string;
		int		number;
		int		flag;
	} value;
};

/* Entry in terminal code table. */
struct tty_term_code_entry {
	enum tty_code_code	code;
	enum tty_code_type	type;
	const char	       *name;
};

/* Message codes. */
enum msgtype {
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
	MSG_VERSION,
	MSG_WAKEUP,
	MSG_ENVIRON,
	MSG_UNLOCK,
	MSG_LOCK,
	MSG_SHELL
};

/*
 * Message data.
 *
 * Don't forget to bump PROTOCOL_VERSION if any of these change!
 */
struct msg_print_data {
	char		msg[PRINT_LENGTH];
};

struct msg_command_data {
	pid_t		pid;			/* pid from $TMUX or -1 */
	u_int		idx;			/* index from $TMUX */

	int		argc;
	char		argv[COMMAND_LENGTH];
};

struct msg_identify_data {
	char		cwd[MAXPATHLEN];

	char		term[TERMINAL_LENGTH];

#define IDENTIFY_UTF8 0x1
#define IDENTIFY_256COLOURS 0x2
#define IDENTIFY_88COLOURS 0x4
	int		flags;
};

struct msg_lock_data {
	char		cmd[COMMAND_LENGTH];
};

struct msg_environ_data {
	char		var[ENVIRON_LENGTH];
};

struct msg_shell_data {
	char		shell[MAXPATHLEN];
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
	MODEKEYEDIT_ENDOFLINE,
	MODEKEYEDIT_ENTER,
	MODEKEYEDIT_HISTORYDOWN,
	MODEKEYEDIT_HISTORYUP,
	MODEKEYEDIT_PASTE,
	MODEKEYEDIT_STARTOFLINE,
	MODEKEYEDIT_SWITCHMODE,
	MODEKEYEDIT_SWITCHMODEAPPEND,
	MODEKEYEDIT_TRANSPOSECHARS,

	/* Menu (choice) keys. */
	MODEKEYCHOICE_CANCEL,
	MODEKEYCHOICE_CHOOSE,
	MODEKEYCHOICE_DOWN,
	MODEKEYCHOICE_PAGEDOWN,
	MODEKEYCHOICE_PAGEUP,
	MODEKEYCHOICE_SCROLLDOWN,
	MODEKEYCHOICE_SCROLLUP,
	MODEKEYCHOICE_UP,

	/* Copy keys. */
	MODEKEYCOPY_BACKTOINDENTATION,
	MODEKEYCOPY_BOTTOMLINE,
	MODEKEYCOPY_CANCEL,
	MODEKEYCOPY_CLEARSELECTION,
	MODEKEYCOPY_COPYSELECTION,
	MODEKEYCOPY_DOWN,
	MODEKEYCOPY_ENDOFLINE,
	MODEKEYCOPY_GOTOLINE,
	MODEKEYCOPY_HALFPAGEDOWN,
	MODEKEYCOPY_HALFPAGEUP,
	MODEKEYCOPY_HISTORYBOTTOM,
	MODEKEYCOPY_HISTORYTOP,
	MODEKEYCOPY_LEFT,
	MODEKEYCOPY_MIDDLELINE,
	MODEKEYCOPY_NEXTPAGE,
	MODEKEYCOPY_NEXTSPACE,
	MODEKEYCOPY_NEXTSPACEEND,
	MODEKEYCOPY_NEXTWORD,
	MODEKEYCOPY_NEXTWORDEND,
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
	MODEKEYCOPY_STARTNUMBERPREFIX,
	MODEKEYCOPY_STARTOFLINE,
	MODEKEYCOPY_STARTSELECTION,
	MODEKEYCOPY_TOPLINE,
	MODEKEYCOPY_UP,
};

/* Entry in the default mode key tables. */
struct mode_key_entry {
	int			key;

	/*
	 * Editing mode for vi: 0 is edit mode, keys not in the table are
	 * returned as MODEKEY_OTHER; 1 is command mode, keys not in the table
	 * are returned as MODEKEY_NONE. This is also matched on, allowing some
	 * keys to be bound in edit mode.
	 */
	int			mode;
	enum mode_key_cmd	cmd;
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
	int			key;

	int			mode;
	enum mode_key_cmd	cmd;

	SPLAY_ENTRY(mode_key_binding) entry;
};
SPLAY_HEAD(mode_key_tree, mode_key_binding);

/* Command to string mapping. */
struct mode_key_cmdstr {
	enum mode_key_cmd	 cmd;
	const char		*name;
};

/* Named mode key table description. */
struct mode_key_table {
	const char		*name;
	struct mode_key_cmdstr	*cmdstr;
	struct mode_key_tree	*tree;
	const struct mode_key_entry *table;	/* default entries */
};

/* Modes. */
#define MODE_CURSOR 0x1
#define MODE_INSERT 0x2
#define MODE_KCURSOR 0x4
#define MODE_KKEYPAD 0x8	/* set = application, clear = number */
#define MODE_MOUSE 0x10

/*
 * A single UTF-8 character.
 *
 * The data member in this must be UTF8_SIZE to allow screen_write_copy to
 * reinject stored UTF-8 data back into screen_write_cell after combining (ugh
 * XXX XXX).
 */
struct utf8_data {
	u_char	data[UTF8_SIZE];

	size_t	have;
	size_t	size;

	u_int	width;
};

/* Grid output. */
#if defined(DEBUG) && \
    ((defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || \
     (defined(__GNUC__) && __GNUC__ >= 3))
#define GRID_DEBUG(gd, fmt, ...) log_debug2("%s: (sx=%u, sy=%u, hsize=%u) " \
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

/* Grid line flags. */
#define GRID_LINE_WRAPPED 0x1

/* Grid cell data. */
struct grid_cell {
	u_char	attr;
	u_char	flags;
	u_char	fg;
	u_char	bg;
	u_char	data;
} __packed;

/* Grid cell UTF-8 data. Used instead of data in grid_cell for UTF-8 cells. */
struct grid_utf8 {
	u_char	width;
	u_char	data[UTF8_SIZE];
} __packed;

/* Grid line. */
struct grid_line {
	u_int	cellsize;
	struct grid_cell *celldata;

	u_int	utf8size;
	struct grid_utf8 *utf8data;

	int	flags;
} __packed;

/* Entire grid of cells. */
struct grid {
	int	flags;
#define GRID_HISTORY 0x1	/* scroll lines into history */

	u_int	sx;
	u_int	sy;

	u_int	hsize;
	u_int	hlimit;

	struct grid_line *linedata;
};

/* Option data structures. */
struct options_entry {
	char		*name;

	enum {
		OPTIONS_STRING,
		OPTIONS_NUMBER,
		OPTIONS_DATA,
	} type;

	char		*str;
	long long	 num;
	void		*data;

	void		 (*freefn)(void *);

	SPLAY_ENTRY(options_entry) entry;
};

struct options {
	SPLAY_HEAD(options_tree, options_entry) tree;
	struct options	*parent;
};

/* Key list for prefix option. */
ARRAY_DECL(keylist, int);

/* Scheduled job. */
struct job {
	char		*cmd;
	pid_t		 pid;
	int		 status;

	struct client	*client;

	int		 fd;
	struct bufferevent *event;

	void		(*callbackfn)(struct job *);
	void		(*freefn)(void *);
	void		*data;

	int		 flags;
#define JOB_PERSIST 0x1	/* don't free after callback */

	RB_ENTRY(job)	 entry;
	SLIST_ENTRY(job) lentry;
};
RB_HEAD(jobs, job);
SLIST_HEAD(joblist, job);

/* Screen selection. */
struct screen_sel {
	int		 flag;
	int		 rectflag;

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

/* Input parser context. */
struct input_ctx {
	struct window_pane     *wp;
	struct screen_write_ctx ctx;

	struct grid_cell	cell;

	struct grid_cell	old_cell;
	u_int 			old_cx;
	u_int			old_cy;

	u_char			interm_buf[4];
	size_t			interm_len;

	u_char			param_buf[64];
	size_t			param_len;

	u_char			input_buf[256];
	size_t			input_len;

	int			param_list[24];	/* -1 not present */
	u_int			param_list_len;

	struct utf8_data	utf8data;

	int			ch;
	int			flags;
#define INPUT_DISCARD 0x1

	const struct input_state *state;
};

/*
 * Window mode. Windows can be in several modes and this is used to call the
 * right function to handle input and output.
 */
struct client;
struct window;
struct mouse_event;
struct window_mode {
	struct screen *(*init)(struct window_pane *);
	void	(*free)(struct window_pane *);
	void	(*resize)(struct window_pane *, u_int, u_int);
	void	(*key)(struct window_pane *, struct client *, int);
	void	(*mouse)(struct window_pane *,
		    struct client *, struct mouse_event *);
	void	(*timer)(struct window_pane *);
};

/* Child window structure. */
struct window_pane {
	struct window	*window;
	struct layout_cell *layout_cell;

	u_int		 sx;
	u_int		 sy;

	u_int		 xoff;
	u_int		 yoff;

	int		 flags;
#define PANE_REDRAW 0x1
#define PANE_FREEZE 0x2

	char		*cmd;
	char		*shell;
	char		*cwd;

	pid_t		 pid;
	char		 tty[TTY_NAME_MAX];

	int		 fd;
	struct bufferevent *event;

	struct input_ctx ictx;

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
};
TAILQ_HEAD(window_panes, window_pane);

/* Window structure. */
struct window {
	char		*name;
	struct event	 name_timer;

	struct window_pane *active;
	struct window_panes panes;

	int		 lastlayout;
	struct layout_cell *layout_root;

	u_int		 sx;
	u_int		 sy;

	int		 flags;
#define WINDOW_BELL 0x1
#define WINDOW_HIDDEN 0x2
#define WINDOW_ACTIVITY 0x4
#define WINDOW_CONTENT 0x8
#define WINDOW_REDRAW 0x10

	struct options	 options;

	u_int		 references;
};
ARRAY_DECL(windows, struct window *);

/* Entry on local window list. */
struct winlink {
	int		 idx;
	struct window	*window;

	size_t		 status_width;
	struct grid_cell status_cell;
	char		*status_text;

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

/* Paste buffer. */
struct paste_buffer {
	char		*data;
	size_t		 size;
};
ARRAY_DECL(paste_stack, struct paste_buffer *);

/* Environment variable. */
struct environ_entry {
	char		*name;
	char		*value;

	RB_ENTRY(environ_entry) entry;
};
RB_HEAD(environ, environ_entry);

/* Client session. */
struct session_alert {
	struct winlink	*wl;
	int		 type;

	SLIST_ENTRY(session_alert) entry;
};

struct session_group {
	TAILQ_HEAD(, session) sessions;

	TAILQ_ENTRY(session_group) entry;
};
TAILQ_HEAD(session_groups, session_group);

struct session {
	char		*name;

	struct timeval	 creation_time;
	struct timeval	 activity_time;

	u_int		 sx;
	u_int		 sy;

	struct winlink	*curw;
	struct winlink_stack lastw;
	struct winlinks	 windows;

	struct options	 options;

	struct paste_stack buffers;

	SLIST_HEAD(, session_alert) alerts;

#define SESSION_UNATTACHED 0x1	/* not attached to any clients */
#define SESSION_DEAD 0x2
	int		 flags;

	struct termios	*tio;

	struct environ	 environ;

	int		 references;

	TAILQ_ENTRY(session) gentry;
};
ARRAY_DECL(sessions, struct session *);

/* TTY information. */
struct tty_key {
	char		 ch;
	int		 key;

	struct tty_key	*left;
	struct tty_key	*right;

	struct tty_key	*next;
};

struct tty_term {
	char		*name;
	u_int		 references;

	struct tty_code	 codes[NTTYCODE];

#define TERM_256COLOURS 0x1
#define TERM_88COLOURS 0x2
#define TERM_EARLYWRAP 0x4
	int		 flags;

	SLIST_ENTRY(tty_term) entry;
};
SLIST_HEAD(tty_terms, tty_term);

struct tty {
	char		*path;

	u_int		 sx;
	u_int		 sy;

	u_int		 cx;
	u_int		 cy;

	int		 mode;

	u_int		 rlower;
	u_int		 rupper;

	char		*termname;
	struct tty_term	*term;

	int		 fd;
	struct bufferevent *event;

	int		 log_fd;

	struct termios	 tio;

	struct grid_cell cell;

	u_char		 acs[UCHAR_MAX + 1];

#define TTY_NOCURSOR 0x1
#define TTY_FREEZE 0x2
#define TTY_ESCAPE 0x4
#define TTY_UTF8 0x8
#define TTY_STARTED 0x10
#define TTY_OPENED 0x20
	int		 flags;

	int		 term_flags;

	void		 (*key_callback)(int, struct mouse_event *, void *);
	void		*key_data;
	struct event	 key_timer;
	struct tty_key	*key_tree;
};

/* TTY command context and function pointer. */
struct tty_ctx {
	struct window_pane *wp;

	const struct grid_cell *cell;
	const struct grid_utf8 *utf8;

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

	/* Saved last cell on line. */
	struct grid_cell last_cell;
	struct grid_utf8 last_utf8;
	u_int		 last_width;
};

/* Mouse input. */
struct mouse_event {
	u_char	b;
	u_char	x;
	u_char	y;
};

/* Saved message entry. */
struct message_entry {
	char   *msg;
	time_t	msg_time;
};

/* Client connection. */
struct client {
	struct imsgbuf	 ibuf;
	struct event	 event;

	struct timeval	 creation_time;
	struct timeval	 activity_time;

	struct environ	 environ;

	char		*title;
	char		*cwd;

	struct tty	 tty;
	struct event	 repeat_timer;

	struct timeval	 status_timer;
	struct jobs	 status_jobs;
	struct screen	 status;

#define CLIENT_TERMINAL 0x1
#define CLIENT_PREFIX 0x2
#define CLIENT_MOUSE 0x4
#define CLIENT_REDRAW 0x8
#define CLIENT_STATUS 0x10
#define CLIENT_REPEAT 0x20	/* allow command to repeat within repeat time */
#define CLIENT_SUSPENDED 0x40
#define CLIENT_BAD 0x80
#define CLIENT_IDENTIFY 0x100
#define CLIENT_DEAD 0x200
#define CLIENT_BORDERS 0x400
#define CLIENT_READONLY 0x800
	int		 flags;

	struct event	 identify_timer;

	char		*message_string;
	struct event	 message_timer;
	ARRAY_DECL(, struct message_entry) message_log;

	char		*prompt_string;
	char		*prompt_buffer;
	size_t		 prompt_index;
	int		 (*prompt_callbackfn)(void *, const char *);
	void		 (*prompt_freefn)(void *);
	void		*prompt_data;

#define PROMPT_SINGLE 0x1
	int		 prompt_flags;

	u_int		 prompt_hindex;
	ARRAY_DECL(, char *) prompt_hdata;

	struct mode_key_data prompt_mdata;

	struct session	*session;

	int		 references;
};
ARRAY_DECL(clients, struct client *);

/* Key/command line command. */
struct cmd_ctx {
	/*
	 * curclient is the client where this command was executed if inside
	 * tmux. This is NULL if the command came from the command-line.
	 *
	 * cmdclient is the client which sent the MSG_COMMAND to the server, if
	 * any. This is NULL unless the command came from the command-line.
	 *
	 * cmdclient and curclient may both be NULL if the command is in the
	 * configuration file.
	 */
	struct client  *curclient;
	struct client  *cmdclient;

	struct msg_command_data	*msgdata;

	/* gcc2 doesn't understand attributes on function pointers... */
#if defined(__GNUC__) && __GNUC__ >= 3
	void printflike2 (*print)(struct cmd_ctx *, const char *, ...);
	void printflike2 (*info)(struct cmd_ctx *, const char *, ...);
	void printflike2 (*error)(struct cmd_ctx *, const char *, ...);
#else
	void (*print)(struct cmd_ctx *, const char *, ...);
	void (*info)(struct cmd_ctx *, const char *, ...);
	void (*error)(struct cmd_ctx *, const char *, ...);
#endif
};

struct cmd {
	const struct cmd_entry *entry;
	void		*data;

	TAILQ_ENTRY(cmd) qentry;
};
TAILQ_HEAD(cmd_list, cmd);

struct cmd_entry {
	const char	*name;
	const char	*alias;
	const char	*usage;

#define CMD_STARTSERVER 0x1
#define CMD_CANTNEST 0x2
#define CMD_SENDENVIRON 0x4
#define CMD_ARG1 0x8
#define CMD_ARG01 0x10
#define CMD_ARG2 0x20
#define CMD_ARG12 0x40
#define CMD_READONLY 0x80
	int		 flags;

	const char	*chflags;

	void		 (*init)(struct cmd *, int);
	int		 (*parse)(struct cmd *, int, char **, char **);
	int		 (*exec)(struct cmd *, struct cmd_ctx *);
	void		 (*free)(struct cmd *);
	size_t		 (*print)(struct cmd *, char *, size_t);
};

/* Generic command data. */
struct cmd_target_data {
	uint64_t chflags;

	char	*target;

	char	*arg;
	char	*arg2;
};

struct cmd_srcdst_data {
	uint64_t chflags;

	char	*src;
	char	*dst;

	char	*arg;
	char	*arg2;
};

struct cmd_buffer_data {
	uint64_t chflags;

	char	*target;
	int	 buffer;

	char	*arg;
	char	*arg2;
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
		SET_OPTION_KEYS,
		SET_OPTION_COLOUR,
		SET_OPTION_ATTRIBUTES,
		SET_OPTION_FLAG,
		SET_OPTION_CHOICE
	} type;

	u_int		 minimum;
	u_int		 maximum;

	const char     **choices;
};

/* List of configuration causes. */
ARRAY_DECL(causelist, char *);

/* tmux.c */
extern struct options global_options;
extern struct options global_s_options;
extern struct options global_w_options;
extern struct environ global_environ;
extern char	*cfg_file;
extern int	 debug_level;
extern int	 be_quiet;
extern time_t	 start_time;
extern char	*socket_path;
extern int	 login_shell;
void		 logfile(const char *);
const char	*getshell(void);
int		 checkshell(const char *);
int		 areshell(const char *);

/* cfg.c */
extern int       cfg_finished;
struct causelist cfg_causes;
void printflike2 cfg_add_cause(struct causelist *, const char *, ...);
int		 load_cfg(const char *, struct cmd_ctx *, struct causelist *);

/* mode-key.c */
extern const struct mode_key_table mode_key_tables[];
extern struct mode_key_tree mode_key_tree_vi_edit;
extern struct mode_key_tree mode_key_tree_vi_choice;
extern struct mode_key_tree mode_key_tree_vi_copy;
extern struct mode_key_tree mode_key_tree_emacs_edit;
extern struct mode_key_tree mode_key_tree_emacs_choice;
extern struct mode_key_tree mode_key_tree_emacs_copy;
int	mode_key_cmp(struct mode_key_binding *, struct mode_key_binding *);
SPLAY_PROTOTYPE(mode_key_tree, mode_key_binding, entry, mode_key_cmp);
const char *mode_key_tostring(struct mode_key_cmdstr *r, enum mode_key_cmd);
enum mode_key_cmd mode_key_fromstring(struct mode_key_cmdstr *, const char *);
const struct mode_key_table *mode_key_findtable(const char *);
void	mode_key_init_trees(void);
void	mode_key_init(struct mode_key_data *, struct mode_key_tree *);
enum mode_key_cmd mode_key_lookup(struct mode_key_data *, int);

/* options.c */
int	options_cmp(struct options_entry *, struct options_entry *);
SPLAY_PROTOTYPE(options_tree, options_entry, entry, options_cmp);
void	options_init(struct options *, struct options *);
void	options_free(struct options *);
struct options_entry *options_find1(struct options *, const char *);
struct options_entry *options_find(struct options *, const char *);
void	options_remove(struct options *, const char *);
struct options_entry *printflike3 options_set_string(
	    struct options *, const char *, const char *, ...);
char   *options_get_string(struct options *, const char *);
struct options_entry *options_set_number(
	    struct options *, const char *, long long);
long long options_get_number(struct options *, const char *);
struct options_entry *options_set_data(
	    struct options *, const char *, void *, void (*)(void *));
void   *options_get_data(struct options *, const char *);

/* job.c */
extern struct joblist all_jobs;
int	job_cmp(struct job *, struct job *);
RB_PROTOTYPE(jobs, job, entry, job_cmp);
void	job_tree_init(struct jobs *);
void	job_tree_free(struct jobs *);
struct job *job_get(struct jobs *, const char *);
struct job *job_add(struct jobs *, int, struct client *,
	    const char *, void (*)(struct job *), void (*)(void *), void *);
void	job_remove(struct jobs *, struct job *);
void	job_free(struct job *);
int	job_run(struct job *);
void	job_died(struct job *, int);
void	job_kill(struct job *);

/* environ.c */
int	environ_cmp(struct environ_entry *, struct environ_entry *);
RB_PROTOTYPE(environ, environ_entry, entry, environ_cmp);
void	environ_init(struct environ *);
void	environ_free(struct environ *);
void	environ_copy(struct environ *, struct environ *);
struct environ_entry *environ_find(struct environ *, const char *);
void	environ_set(struct environ *, const char *, const char *);
void	environ_put(struct environ *, const char *);
void	environ_unset(struct environ *, const char *);
void	environ_update(const char *, struct environ *, struct environ *);

/* tty.c */
void	tty_raw(struct tty *, const char *);
u_char	tty_get_acs(struct tty *, u_char);
void	tty_attributes(struct tty *, const struct grid_cell *);
void	tty_reset(struct tty *);
void	tty_region_pane(struct tty *, const struct tty_ctx *, u_int, u_int);
void	tty_region(struct tty *, u_int, u_int);
void	tty_cursor_pane(struct tty *, const struct tty_ctx *, u_int, u_int);
void	tty_cursor(struct tty *, u_int, u_int);
void	tty_putcode(struct tty *, enum tty_code_code);
void	tty_putcode1(struct tty *, enum tty_code_code, int);
void	tty_putcode2(struct tty *, enum tty_code_code, int, int);
void	tty_puts(struct tty *, const char *);
void	tty_putc(struct tty *, u_char);
void	tty_pututf8(struct tty *, const struct grid_utf8 *);
void	tty_init(struct tty *, int, char *);
void	tty_resize(struct tty *);
void	tty_start_tty(struct tty *);
void	tty_stop_tty(struct tty *);
void	tty_set_title(struct tty *, const char *);
void	tty_update_mode(struct tty *, int);
void	tty_draw_line(struct tty *, struct screen *, u_int, u_int, u_int);
int	tty_open(struct tty *, const char *, char **);
void	tty_close(struct tty *);
void	tty_free(struct tty *);
void	tty_write(void (*)(
	    struct tty *, const struct tty_ctx *), const struct tty_ctx *);
void	tty_cmd_alignmenttest(struct tty *, const struct tty_ctx *);
void	tty_cmd_cell(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearendofline(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearendofscreen(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearline(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearscreen(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearstartofline(struct tty *, const struct tty_ctx *);
void	tty_cmd_clearstartofscreen(struct tty *, const struct tty_ctx *);
void	tty_cmd_deletecharacter(struct tty *, const struct tty_ctx *);
void	tty_cmd_deleteline(struct tty *, const struct tty_ctx *);
void	tty_cmd_insertcharacter(struct tty *, const struct tty_ctx *);
void	tty_cmd_insertline(struct tty *, const struct tty_ctx *);
void	tty_cmd_linefeed(struct tty *, const struct tty_ctx *);
void	tty_cmd_utf8character(struct tty *, const struct tty_ctx *);
void	tty_cmd_reverseindex(struct tty *, const struct tty_ctx *);

/* tty-term.c */
extern struct tty_terms tty_terms;
extern struct tty_term_code_entry tty_term_codes[NTTYCODE];
struct tty_term *tty_term_find(char *, int, const char *, char **);
void		 tty_term_free(struct tty_term *);
int		 tty_term_has(struct tty_term *, enum tty_code_code);
const char	*tty_term_string(struct tty_term *, enum tty_code_code);
const char	*tty_term_string1(struct tty_term *, enum tty_code_code, int);
const char	*tty_term_string2(
		     struct tty_term *, enum tty_code_code, int, int);
int		 tty_term_number(struct tty_term *, enum tty_code_code);
int		 tty_term_flag(struct tty_term *, enum tty_code_code);

/* tty-keys.c */
void	tty_keys_init(struct tty *);
void	tty_keys_free(struct tty *);
int	tty_keys_next(struct tty *);

/* paste.c */
void		 paste_init_stack(struct paste_stack *);
void		 paste_free_stack(struct paste_stack *);
struct paste_buffer *paste_walk_stack(struct paste_stack *, uint *);
struct paste_buffer *paste_get_top(struct paste_stack *);
struct paste_buffer *paste_get_index(struct paste_stack *, u_int);
int		 paste_free_top(struct paste_stack *);
int		 paste_free_index(struct paste_stack *, u_int);
void		 paste_add(struct paste_stack *, char *, size_t, u_int);
int		 paste_replace(struct paste_stack *, u_int, char *, size_t);

/* clock.c */
extern const char clock_table[14][5][5];
void		 clock_draw(struct screen_write_ctx *, int, int);

/* cmd-set-option.c */
extern const struct set_option_entry set_option_table[];
extern const struct set_option_entry set_session_option_table[];
extern const struct set_option_entry set_window_option_table[];
const char	*cmd_set_option_print(
		    const struct set_option_entry *, struct options_entry *);

/* cmd.c */
int		 cmd_pack_argv(int, char **, char *, size_t);
int		 cmd_unpack_argv(char *, size_t, int, char ***);
void		 cmd_free_argv(int, char **);
struct cmd	*cmd_parse(int, char **, char **);
int		 cmd_exec(struct cmd *, struct cmd_ctx *);
void		 cmd_free(struct cmd *);
size_t		 cmd_print(struct cmd *, char *, size_t);
struct session	*cmd_current_session(struct cmd_ctx *);
struct client	*cmd_current_client(struct cmd_ctx *);
struct client	*cmd_find_client(struct cmd_ctx *, const char *);
struct session	*cmd_find_session(struct cmd_ctx *, const char *);
struct winlink	*cmd_find_window(
		     struct cmd_ctx *, const char *, struct session **);
int		 cmd_find_index(
		     struct cmd_ctx *, const char *, struct session **);
struct winlink	*cmd_find_pane(struct cmd_ctx *,
		     const char *, struct session **, struct window_pane **);
char		*cmd_template_replace(char *, const char *, int);
extern const struct cmd_entry *cmd_table[];
extern const struct cmd_entry cmd_attach_session_entry;
extern const struct cmd_entry cmd_bind_key_entry;
extern const struct cmd_entry cmd_break_pane_entry;
extern const struct cmd_entry cmd_capture_pane_entry;
extern const struct cmd_entry cmd_choose_client_entry;
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
extern const struct cmd_entry cmd_display_message_entry;
extern const struct cmd_entry cmd_display_panes_entry;
extern const struct cmd_entry cmd_down_pane_entry;
extern const struct cmd_entry cmd_find_window_entry;
extern const struct cmd_entry cmd_has_session_entry;
extern const struct cmd_entry cmd_if_shell_entry;
extern const struct cmd_entry cmd_join_pane_entry;
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
extern const struct cmd_entry cmd_list_panes_entry;
extern const struct cmd_entry cmd_list_sessions_entry;
extern const struct cmd_entry cmd_list_windows_entry;
extern const struct cmd_entry cmd_load_buffer_entry;
extern const struct cmd_entry cmd_lock_client_entry;
extern const struct cmd_entry cmd_lock_server_entry;
extern const struct cmd_entry cmd_lock_session_entry;
extern const struct cmd_entry cmd_move_window_entry;
extern const struct cmd_entry cmd_new_session_entry;
extern const struct cmd_entry cmd_new_window_entry;
extern const struct cmd_entry cmd_next_layout_entry;
extern const struct cmd_entry cmd_next_window_entry;
extern const struct cmd_entry cmd_paste_buffer_entry;
extern const struct cmd_entry cmd_pipe_pane_entry;
extern const struct cmd_entry cmd_previous_layout_entry;
extern const struct cmd_entry cmd_previous_window_entry;
extern const struct cmd_entry cmd_refresh_client_entry;
extern const struct cmd_entry cmd_rename_session_entry;
extern const struct cmd_entry cmd_rename_window_entry;
extern const struct cmd_entry cmd_resize_pane_entry;
extern const struct cmd_entry cmd_respawn_window_entry;
extern const struct cmd_entry cmd_rotate_window_entry;
extern const struct cmd_entry cmd_run_shell_entry;
extern const struct cmd_entry cmd_save_buffer_entry;
extern const struct cmd_entry cmd_select_layout_entry;
extern const struct cmd_entry cmd_select_pane_entry;
extern const struct cmd_entry cmd_select_prompt_entry;
extern const struct cmd_entry cmd_select_window_entry;
extern const struct cmd_entry cmd_send_keys_entry;
extern const struct cmd_entry cmd_send_prefix_entry;
extern const struct cmd_entry cmd_server_info_entry;
extern const struct cmd_entry cmd_set_buffer_entry;
extern const struct cmd_entry cmd_set_environment_entry;
extern const struct cmd_entry cmd_set_option_entry;
extern const struct cmd_entry cmd_set_window_option_entry;
extern const struct cmd_entry cmd_show_buffer_entry;
extern const struct cmd_entry cmd_show_environment_entry;
extern const struct cmd_entry cmd_show_messages_entry;
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
void		 cmd_list_free(struct cmd_list *);
size_t		 cmd_list_print(struct cmd_list *, char *, size_t);

/* cmd-string.c */
int	cmd_string_parse(const char *, struct cmd_list **, char **);

/* cmd-generic.c */
size_t	cmd_prarg(char *, size_t, const char *, char *);
int	cmd_check_flag(uint64_t, int);
void	cmd_set_flag(uint64_t *, int);
#define CMD_TARGET_PANE_USAGE "[-t target-pane]"
#define CMD_TARGET_WINDOW_USAGE "[-t target-window]"
#define CMD_TARGET_SESSION_USAGE "[-t target-session]"
#define CMD_TARGET_CLIENT_USAGE "[-t target-client]"
void	cmd_target_init(struct cmd *, int);
int	cmd_target_parse(struct cmd *, int, char **, char **);
void	cmd_target_free(struct cmd *);
size_t	cmd_target_print(struct cmd *, char *, size_t);
#define CMD_SRCDST_PANE_USAGE "[-s src-pane] [-t dst-pane]"
#define CMD_SRCDST_WINDOW_USAGE "[-s src-window] [-t dst-window]"
#define CMD_SRCDST_SESSION_USAGE "[-s src-session] [-t dst-session]"
#define CMD_SRCDST_CLIENT_USAGE "[-s src-client] [-t dst-client]"
void	cmd_srcdst_init(struct cmd *, int);
int	cmd_srcdst_parse(struct cmd *, int, char **, char **);
void	cmd_srcdst_free(struct cmd *);
size_t	cmd_srcdst_print(struct cmd *, char *, size_t);
#define CMD_BUFFER_PANE_USAGE "[-b buffer-index] [-t target-pane]"
#define CMD_BUFFER_WINDOW_USAGE "[-b buffer-index] [-t target-window]"
#define CMD_BUFFER_SESSION_USAGE "[-b buffer-index] [-t target-session]"
#define CMD_BUFFER_CLIENT_USAGE "[-b buffer-index] [-t target-client]"
void	cmd_buffer_init(struct cmd *, int);
int	cmd_buffer_parse(struct cmd *, int, char **, char **);
void	cmd_buffer_free(struct cmd *);
size_t	cmd_buffer_print(struct cmd *, char *, size_t);

/* client.c */
struct imsgbuf *client_init(char *, int, int);
__dead void	client_main(void);

/* key-bindings.c */
extern struct key_bindings key_bindings;
int	 key_bindings_cmp(struct key_binding *, struct key_binding *);
SPLAY_PROTOTYPE(key_bindings, key_binding, entry, key_bindings_cmp);
struct key_binding *key_bindings_lookup(int);
void	 key_bindings_add(int, int, struct cmd_list *);
void	 key_bindings_remove(int);
void	 key_bindings_clean(void);
void	 key_bindings_init(void);
void	 key_bindings_dispatch(struct key_binding *, struct client *);
void printflike2 key_bindings_error(struct cmd_ctx *, const char *, ...);
void printflike2 key_bindings_print(struct cmd_ctx *, const char *, ...);
void printflike2 key_bindings_info(struct cmd_ctx *, const char *, ...);

/* key-string.c */
int	 key_string_lookup_string(const char *);
const char *key_string_lookup_key(int);

/* server.c */
extern struct clients clients;
extern struct clients dead_clients;
int	 server_start(char *);
void	 server_signal_set(void);
void	 server_signal_clear(void);
void	 server_update_socket(void);

/* server-client.c */
void	 server_client_create(int);
void	 server_client_lost(struct client *);
void	 server_client_callback(int, short, void *);
void	 server_client_status_timer(void);
void	 server_client_loop(void);

/* server-window.c */
void	 server_window_loop(void);

/* server-fn.c */
void	 server_fill_environ(struct session *, struct environ *);
void	 server_write_error(struct client *, const char *);
void	 server_write_client(
	     struct client *, enum msgtype, const void *, size_t);
void	 server_write_session(
	     struct session *, enum msgtype, const void *, size_t);
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
int	 server_unlock(const char *);
void	 server_kill_window(struct window *);
int	 server_link_window(struct session *,
	     struct winlink *, struct session *, int, int, int, char **);
void	 server_unlink_window(struct session *, struct winlink *);
void	 server_destroy_pane(struct window_pane *);
void	 server_destroy_session_group(struct session *);
void	 server_destroy_session(struct session *);
void	 server_set_identify(struct client *);
void	 server_clear_identify(struct client *);
void	 server_update_event(struct client *);

/* status.c */
int	 status_redraw(struct client *);
char	*status_replace(
	     struct client *, struct winlink *, const char *, time_t, int);
void printflike2 status_message_set(struct client *, const char *, ...);
void	 status_message_clear(struct client *);
int	 status_message_redraw(struct client *);
void	 status_prompt_set(struct client *, const char *,
	     int (*)(void *, const char *), void (*)(void *), void *, int);
void	 status_prompt_clear(struct client *);
int	 status_prompt_redraw(struct client *);
void	 status_prompt_key(struct client *, int);
void	 status_prompt_update(struct client *, const char *);

/* resize.c */
void	 recalculate_sizes(void);

/* input.c */
void	 input_init(struct window_pane *);
void	 input_free(struct window_pane *);
void	 input_parse(struct window_pane *);

/* input-key.c */
void	 input_key(struct window_pane *, int);
void	 input_mouse(struct window_pane *, struct mouse_event *);

/* xterm-keys.c */
char	*xterm_keys_lookup(int);
int	 xterm_keys_find(const char *, size_t, size_t *, int *);

/* colour.c */
void	 colour_set_fg(struct grid_cell *, int);
void	 colour_set_bg(struct grid_cell *, int);
const char *colour_tostring(int);
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
void	 grid_collect_history(struct grid *);
void	 grid_scroll_history(struct grid *);
void	 grid_scroll_history_region(struct grid *, u_int, u_int);
void	 grid_expand_line(struct grid *, u_int, u_int);
void	 grid_expand_line_utf8(struct grid *, u_int, u_int);
const struct grid_cell *grid_peek_cell(struct grid *, u_int, u_int);
struct grid_cell *grid_get_cell(struct grid *, u_int, u_int);
void	 grid_set_cell(struct grid *, u_int, u_int, const struct grid_cell *);
const struct grid_utf8 *grid_peek_utf8(struct grid *, u_int, u_int);
struct grid_utf8 *grid_get_utf8(struct grid *, u_int, u_int);
void	 grid_set_utf8(struct grid *, u_int, u_int, const struct grid_utf8 *);
void	 grid_clear(struct grid *, u_int, u_int, u_int, u_int);
void	 grid_clear_lines(struct grid *, u_int, u_int);
void	 grid_move_lines(struct grid *, u_int, u_int, u_int);
void	 grid_move_cells(struct grid *, u_int, u_int, u_int, u_int);
char	*grid_string_cells(struct grid *, u_int, u_int, u_int);
void	 grid_duplicate_lines(
	     struct grid *, u_int, struct grid *, u_int, u_int);

/* grid-utf8.c */
size_t	 grid_utf8_size(const struct grid_utf8 *);
size_t	 grid_utf8_copy(const struct grid_utf8 *, char *, size_t);
void	 grid_utf8_set(struct grid_utf8 *, const struct utf8_data *);
int	 grid_utf8_append(struct grid_utf8 *, const struct utf8_data *);
int	 grid_utf8_compare(const struct grid_utf8 *, const struct grid_utf8 *);

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
void	 grid_view_insert_lines_region(struct grid *, u_int, u_int, u_int);
void	 grid_view_delete_lines(struct grid *, u_int, u_int);
void	 grid_view_delete_lines_region(struct grid *, u_int, u_int, u_int);
void	 grid_view_insert_cells(struct grid *, u_int, u_int, u_int);
void	 grid_view_delete_cells(struct grid *, u_int, u_int, u_int);
char	*grid_view_string_cells(struct grid *, u_int, u_int, u_int);

/* screen-write.c */
void	 screen_write_start(
	     struct screen_write_ctx *, struct window_pane *, struct screen *);
void	 screen_write_stop(struct screen_write_ctx *);
size_t printflike2 screen_write_cstrlen(int, const char *, ...);
void printflike5 screen_write_cnputs(struct screen_write_ctx *,
	     ssize_t, struct grid_cell *, int, const char *, ...);
size_t printflike2 screen_write_strlen(int, const char *, ...);
void printflike3 screen_write_puts(struct screen_write_ctx *,
	     struct grid_cell *, const char *, ...);
void printflike5 screen_write_nputs(struct screen_write_ctx *,
	     ssize_t, struct grid_cell *, int, const char *, ...);
void	 screen_write_vnputs(struct screen_write_ctx *,
	     ssize_t, struct grid_cell *, int, const char *, va_list);
void	 screen_write_parsestyle(
	     struct grid_cell *, struct grid_cell *, const char *);
void	 screen_write_putc(
	     struct screen_write_ctx *, struct grid_cell *, u_char);
void	 screen_write_copy(struct screen_write_ctx *,
	     struct screen *, u_int, u_int, u_int, u_int);
void	 screen_write_backspace(struct screen_write_ctx *);
void	 screen_write_cursorup(struct screen_write_ctx *, u_int);
void	 screen_write_cursordown(struct screen_write_ctx *, u_int);
void	 screen_write_cursorright(struct screen_write_ctx *, u_int);
void	 screen_write_cursorleft(struct screen_write_ctx *, u_int);
void	 screen_write_alignmenttest(struct screen_write_ctx *);
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
void	 screen_write_linefeed(struct screen_write_ctx *, int);
void	 screen_write_linefeedscreen(struct screen_write_ctx *, int);
void	 screen_write_carriagereturn(struct screen_write_ctx *);
void	 screen_write_kcursormode(struct screen_write_ctx *, int);
void	 screen_write_kkeypadmode(struct screen_write_ctx *, int);
void	 screen_write_clearendofscreen(struct screen_write_ctx *);
void	 screen_write_clearstartofscreen(struct screen_write_ctx *);
void	 screen_write_clearscreen(struct screen_write_ctx *);
void	 screen_write_cell(struct screen_write_ctx *,
	     const struct grid_cell *, const struct utf8_data *);

/* screen-redraw.c */
void	 screen_redraw_screen(struct client *, int, int);
void	 screen_redraw_pane(struct client *, struct window_pane *);

/* screen.c */
void	 screen_init(struct screen *, u_int, u_int, u_int);
void	 screen_reinit(struct screen *);
void	 screen_free(struct screen *);
void	 screen_reset_tabs(struct screen *);
void	 screen_set_title(struct screen *, const char *);
void	 screen_resize(struct screen *, u_int, u_int);
void	 screen_set_selection(struct screen *,
	     u_int, u_int, u_int, u_int, u_int, struct grid_cell *);
void	 screen_clear_selection(struct screen *);
int	 screen_check_selection(struct screen *, u_int, u_int);

/* window.c */
extern struct windows windows;
int		 window_cmp(struct window *, struct window *);
int		 winlink_cmp(struct winlink *, struct winlink *);
RB_PROTOTYPE(windows, window, entry, window_cmp);
RB_PROTOTYPE(winlinks, winlink, entry, winlink_cmp);
struct winlink	*winlink_find_by_index(struct winlinks *, int);
struct winlink	*winlink_find_by_window(struct winlinks *, struct window *);
int		 winlink_next_index(struct winlinks *, int);
u_int		 winlink_count(struct winlinks *);
struct winlink	*winlink_add(struct winlinks *, struct window *, int);
void		 winlink_remove(struct winlinks *, struct winlink *);
struct winlink	*winlink_next(struct winlink *);
struct winlink	*winlink_previous(struct winlink *);
void		 winlink_stack_push(struct winlink_stack *, struct winlink *);
void		 winlink_stack_remove(struct winlink_stack *, struct winlink *);
int		 window_index(struct window *, u_int *);
struct window	*window_create1(u_int, u_int);
struct window	*window_create(const char *, const char *, const char *,
		     const char *, struct environ *, struct termios *,
		     u_int, u_int, u_int, char **);
void		 window_destroy(struct window *);
void		 window_set_active_at(struct window *, u_int, u_int);
void		 window_set_active_pane(struct window *, struct window_pane *);
struct window_pane *window_add_pane(struct window *, u_int);
void		 window_resize(struct window *, u_int, u_int);
void		 window_remove_pane(struct window *, struct window_pane *);
struct window_pane *window_pane_at_index(struct window *, u_int);
u_int		 window_pane_index(struct window *, struct window_pane *);
u_int		 window_count_panes(struct window *);
void		 window_destroy_panes(struct window *);
struct window_pane *window_pane_create(struct window *, u_int, u_int, u_int);
void		 window_pane_destroy(struct window_pane *);
int		 window_pane_spawn(struct window_pane *, const char *,
		     const char *, const char *, struct environ *,
		     struct termios *, char **);
void		 window_pane_resize(struct window_pane *, u_int, u_int);
void		 window_pane_alternate_on(
		     struct window_pane *, struct grid_cell *);
void		 window_pane_alternate_off(
		     struct window_pane *, struct grid_cell *);
int		 window_pane_set_mode(
		     struct window_pane *, const struct window_mode *);
void		 window_pane_reset_mode(struct window_pane *);
void		 window_pane_parse(struct window_pane *);
void		 window_pane_key(struct window_pane *, struct client *, int);
void		 window_pane_mouse(struct window_pane *,
		     struct client *, struct mouse_event *);
int		 window_pane_visible(struct window_pane *);
char		*window_pane_search(
		     struct window_pane *, const char *, u_int *);

/* layout.c */
struct layout_cell *layout_create_cell(struct layout_cell *);
void		 layout_free_cell(struct layout_cell *);
void		 layout_print_cell(struct layout_cell *, const char *, u_int);
void		 layout_set_size(
		     struct layout_cell *, u_int, u_int, u_int, u_int);
void		 layout_make_leaf(
		     struct layout_cell *, struct window_pane *);
void		 layout_make_node(struct layout_cell *, enum layout_type);
void		 layout_fix_offsets(struct layout_cell *);
void		 layout_fix_panes(struct window *, u_int, u_int);
u_int		 layout_resize_check(struct layout_cell *, enum layout_type);
void		 layout_resize_adjust(
		     struct layout_cell *, enum layout_type, int);
void		 layout_init(struct window *);
void		 layout_free(struct window *);
void		 layout_resize(struct window *, u_int, u_int);
void		 layout_resize_pane(
		     struct window_pane *, enum layout_type, int);
void		 layout_assign_pane(struct layout_cell *, struct window_pane *);
struct layout_cell *layout_split_pane(
		     struct window_pane *, enum layout_type, int);
void		 layout_close_pane(struct window_pane *);

/* layout-set.c */
const char	*layout_set_name(u_int);
int		 layout_set_lookup(const char *);
u_int		 layout_set_select(struct window *, u_int);
u_int		 layout_set_next(struct window *);
u_int		 layout_set_previous(struct window *);
void		 layout_set_active_changed(struct window *);

/* layout-string.c */
struct layout_cell *layout_find_string(struct window *, const char *);

/* window-clock.c */
extern const struct window_mode window_clock_mode;

/* window-copy.c */
extern const struct window_mode window_copy_mode;
void		 window_copy_pageup(struct window_pane *);

/* window-more.c */
extern const struct window_mode window_more_mode;
void		 window_more_add(struct window_pane *, const char *, ...);
void		 window_more_vadd(struct window_pane *, const char *, va_list);

/* window-choose.c */
extern const struct window_mode window_choose_mode;
void		 window_choose_vadd(
		     struct window_pane *, int, const char *, va_list);
void printflike3 window_choose_add(
		     struct window_pane *, int, const char *, ...);
void		 window_choose_ready(struct window_pane *,
		     u_int, void (*)(void *, int), void (*)(void *), void *);

/* names.c */
void		 queue_window_name(struct window *);
char		*default_window_name(struct window *);

/* session.c */
extern struct sessions sessions;
extern struct sessions dead_sessions;
extern struct session_groups session_groups;
void	 session_alert_add(struct session *, struct window *, int);
void	 session_alert_cancel(struct session *, struct winlink *);
int	 session_alert_has(struct session *, struct winlink *, int);
int	 session_alert_has_window(struct session *, struct window *, int);
struct session	*session_find(const char *);
struct session	*session_create(const char *, const char *, const char *,
		     struct environ *, struct termios *, int, u_int, u_int,
		     char **);
void		 session_destroy(struct session *);
int		 session_index(struct session *, u_int *);
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
struct session_group *session_group_find(struct session *);
u_int		 session_group_index(struct session_group *);
void		 session_group_add(struct session *, struct session *);
void		 session_group_remove(struct session *);
void		 session_group_synchronize_to(struct session *);
void		 session_group_synchronize_from(struct session *);
void		 session_group_synchronize1(struct session *, struct session *);

/* utf8.c */
void	utf8_build(void);
int	utf8_open(struct utf8_data *, u_char);
int	utf8_append(struct utf8_data *, u_char);

/* procname.c */
char   *get_proc_name(int, char *);

/* log.c */
void		 log_open_tty(int);
void		 log_open_file(int, const char *);
void		 log_close(void);
void printflike1 log_warn(const char *, ...);
void printflike1 log_warnx(const char *, ...);
void printflike1 log_info(const char *, ...);
void printflike1 log_debug(const char *, ...);
void printflike1 log_debug2(const char *, ...);
__dead void printflike1 log_fatal(const char *, ...);
__dead void printflike1 log_fatalx(const char *, ...);

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

#endif /* TMUX_H */
