/* $Id: tmux.h,v 1.182 2008-08-28 17:45:27 nicm Exp $ */

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

#define PROTOCOL_VERSION -1

/* Shut up gcc warnings about empty if bodies. */
#define RB_AUGMENT(x) do {} while (0)

#include <sys/param.h>
#include <sys/time.h>

#ifndef NO_QUEUE_H
#include <sys/queue.h>
#else
#include "compat/queue.h"
#endif

#ifndef NO_TREE_H
#include <sys/tree.h>
#else
#include "compat/tree.h"
#endif

#ifndef BROKEN_POLL
#include <poll.h>
#else
#undef HAVE_POLL
#include "compat/bsd-poll.h"
#endif

#include <ncurses.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <term.h>

#include "array.h"

extern const char    *__progname;

#ifndef INFTIM
#define INFTIM -1
#endif

#ifndef WAIT_ANY
#define WAIT_ANY -1
#endif

#ifndef SUN_LEN
#define SUN_LEN(sun) (sizeof (sun)->sun_path)
#endif

#ifndef __dead
#define __dead __attribute__ ((__noreturn__))
#endif
#ifndef __packed
#define __packed __attribute__ ((__packed__))
#endif

#ifndef timercmp
#define	timercmp(tvp, uvp, cmp)						\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#endif

#ifndef timeradd
#define	timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#endif

#ifndef TTY_NAME_MAX
#define TTY_NAME_MAX 32
#endif

#ifdef NO_PATHS_H
#define	_PATH_BSHELL	"/bin/sh"
#define	_PATH_TMP	"/tmp/"
#define _PATH_DEVNULL	"/dev/null"
#endif

/* Default configuration file. */
#define DEFAULT_CFG ".tmux.conf"

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
#define KEYC_NONE   0x0ffff
#define KEYC_OFFSET 0x10000
#define KEYC_ESCAPE 0x20000

#define KEYC_ADDESCAPE(k) ((k) | KEYC_ESCAPE)
#define KEYC_REMOVEESCAPE(k) ((k) & ~KEYC_ESCAPE)
#define KEYC_ISESCAPE(k) ((k) != KEYC_NONE && ((k) & KEYC_ESCAPE))

/* #define KEYC_A1 (KEYC_OFFSET + 0x1) */
/* #define KEYC_A3 (KEYC_OFFSET + 0x2) */
/* #define KEYC_B2 (KEYC_OFFSET + 0x3) */
/* #define KEYC_BACKSPACE (KEYC_OFFSET + 0x4) */
#define KEYC_BEG (KEYC_OFFSET + 0x5)
#define KEYC_BTAB (KEYC_OFFSET + 0x6)
/* #define KEYC_C1 (KEYC_OFFSET + 0x7) */
/* #define KEYC_C3 (KEYC_OFFSET + 0x8) */
#define KEYC_CANCEL (KEYC_OFFSET + 0x9)
#define KEYC_CATAB (KEYC_OFFSET + 0xa)
#define KEYC_CLEAR (KEYC_OFFSET + 0xb)
#define KEYC_CLOSE (KEYC_OFFSET + 0xc)
#define KEYC_COMMAND (KEYC_OFFSET + 0xd)
#define KEYC_COPY (KEYC_OFFSET + 0xe)
#define KEYC_CREATE (KEYC_OFFSET + 0xf)
#define KEYC_CTAB (KEYC_OFFSET + 0x10)
#define KEYC_DC (KEYC_OFFSET + 0x11)
#define KEYC_DL (KEYC_OFFSET + 0x12)
#define KEYC_DOWN (KEYC_OFFSET + 0x13)
#define KEYC_EIC (KEYC_OFFSET + 0x14)
#define KEYC_END (KEYC_OFFSET + 0x15)
/* #define KEYC_ENTER (KEYC_OFFSET + 0x16) */
#define KEYC_EOL (KEYC_OFFSET + 0x17)
#define KEYC_EOS (KEYC_OFFSET + 0x18)
#define KEYC_EXIT (KEYC_OFFSET + 0x19)
#define KEYC_F0 (KEYC_OFFSET + 0x1a)
#define KEYC_F1 (KEYC_OFFSET + 0x1b)
#define KEYC_F10 (KEYC_OFFSET + 0x1c)
#define KEYC_F11 (KEYC_OFFSET + 0x1d)
#define KEYC_F12 (KEYC_OFFSET + 0x1e)
#define KEYC_F13 (KEYC_OFFSET + 0x1f)
#define KEYC_F14 (KEYC_OFFSET + 0x20)
#define KEYC_F15 (KEYC_OFFSET + 0x21)
#define KEYC_F16 (KEYC_OFFSET + 0x22)
#define KEYC_F17 (KEYC_OFFSET + 0x23)
#define KEYC_F18 (KEYC_OFFSET + 0x24)
#define KEYC_F19 (KEYC_OFFSET + 0x25)
#define KEYC_F2 (KEYC_OFFSET + 0x26)
#define KEYC_F20 (KEYC_OFFSET + 0x27)
#define KEYC_F21 (KEYC_OFFSET + 0x28)
#define KEYC_F22 (KEYC_OFFSET + 0x29)
#define KEYC_F23 (KEYC_OFFSET + 0x2a)
#define KEYC_F24 (KEYC_OFFSET + 0x2b)
#define KEYC_F25 (KEYC_OFFSET + 0x2c)
#define KEYC_F26 (KEYC_OFFSET + 0x2d)
#define KEYC_F27 (KEYC_OFFSET + 0x2e)
#define KEYC_F28 (KEYC_OFFSET + 0x2f)
#define KEYC_F29 (KEYC_OFFSET + 0x30)
#define KEYC_F3 (KEYC_OFFSET + 0x31)
#define KEYC_F30 (KEYC_OFFSET + 0x32)
#define KEYC_F31 (KEYC_OFFSET + 0x33)
#define KEYC_F32 (KEYC_OFFSET + 0x34)
#define KEYC_F33 (KEYC_OFFSET + 0x35)
#define KEYC_F34 (KEYC_OFFSET + 0x36)
#define KEYC_F35 (KEYC_OFFSET + 0x37)
#define KEYC_F36 (KEYC_OFFSET + 0x38)
#define KEYC_F37 (KEYC_OFFSET + 0x39)
#define KEYC_F38 (KEYC_OFFSET + 0x3a)
#define KEYC_F39 (KEYC_OFFSET + 0x3b)
#define KEYC_F4 (KEYC_OFFSET + 0x3c)
#define KEYC_F40 (KEYC_OFFSET + 0x3d)
#define KEYC_F41 (KEYC_OFFSET + 0x3e)
#define KEYC_F42 (KEYC_OFFSET + 0x3f)
#define KEYC_F43 (KEYC_OFFSET + 0x40)
#define KEYC_F44 (KEYC_OFFSET + 0x41)
#define KEYC_F45 (KEYC_OFFSET + 0x42)
#define KEYC_F46 (KEYC_OFFSET + 0x43)
#define KEYC_F47 (KEYC_OFFSET + 0x44)
#define KEYC_F48 (KEYC_OFFSET + 0x45)
#define KEYC_F49 (KEYC_OFFSET + 0x46)
#define KEYC_F5 (KEYC_OFFSET + 0x47)
#define KEYC_F50 (KEYC_OFFSET + 0x48)
#define KEYC_F51 (KEYC_OFFSET + 0x49)
#define KEYC_F52 (KEYC_OFFSET + 0x4a)
#define KEYC_F53 (KEYC_OFFSET + 0x4b)
#define KEYC_F54 (KEYC_OFFSET + 0x4c)
#define KEYC_F55 (KEYC_OFFSET + 0x4d)
#define KEYC_F56 (KEYC_OFFSET + 0x4e)
#define KEYC_F57 (KEYC_OFFSET + 0x4f)
#define KEYC_F58 (KEYC_OFFSET + 0x50)
#define KEYC_F59 (KEYC_OFFSET + 0x51)
#define KEYC_F6 (KEYC_OFFSET + 0x52)
#define KEYC_F60 (KEYC_OFFSET + 0x53)
#define KEYC_F61 (KEYC_OFFSET + 0x54)
#define KEYC_F62 (KEYC_OFFSET + 0x55)
#define KEYC_F63 (KEYC_OFFSET + 0x56)
#define KEYC_F7 (KEYC_OFFSET + 0x57)
#define KEYC_F8 (KEYC_OFFSET + 0x58)
#define KEYC_F9 (KEYC_OFFSET + 0x59)
#define KEYC_FIND (KEYC_OFFSET + 0x5a)
#define KEYC_HELP (KEYC_OFFSET + 0x5b)
#define KEYC_HOME (KEYC_OFFSET + 0x5c)
#define KEYC_IC (KEYC_OFFSET + 0x5d)
#define KEYC_IL (KEYC_OFFSET + 0x5e)
#define KEYC_LEFT (KEYC_OFFSET + 0x5f)
#define KEYC_LL (KEYC_OFFSET + 0x60)
#define KEYC_MARK (KEYC_OFFSET + 0x61)
#define KEYC_MESSAGE (KEYC_OFFSET + 0x62)
#define KEYC_MOVE (KEYC_OFFSET + 0x63)
#define KEYC_NEXT (KEYC_OFFSET + 0x64)
#define KEYC_NPAGE (KEYC_OFFSET + 0x65)
#define KEYC_OPEN (KEYC_OFFSET + 0x66)
#define KEYC_OPTIONS (KEYC_OFFSET + 0x67)
#define KEYC_PPAGE (KEYC_OFFSET + 0x68)
#define KEYC_PREVIOUS (KEYC_OFFSET + 0x69)
#define KEYC_PRINT (KEYC_OFFSET + 0x6a)
#define KEYC_REDO (KEYC_OFFSET + 0x6b)
#define KEYC_REFERENCE (KEYC_OFFSET + 0x6c)
#define KEYC_REFRESH (KEYC_OFFSET + 0x6d)
#define KEYC_REPLACE (KEYC_OFFSET + 0x6e)
#define KEYC_RESTART (KEYC_OFFSET + 0x6f)
#define KEYC_RESUME (KEYC_OFFSET + 0x70)
#define KEYC_RIGHT (KEYC_OFFSET + 0x71)
#define KEYC_SAVE (KEYC_OFFSET + 0x72)
#define KEYC_SBEG (KEYC_OFFSET + 0x73)
#define KEYC_SCANCEL (KEYC_OFFSET + 0x74)
#define KEYC_SCOMMAND (KEYC_OFFSET + 0x75)
#define KEYC_SCOPY (KEYC_OFFSET + 0x76)
#define KEYC_SCREATE (KEYC_OFFSET + 0x77)
#define KEYC_SDC (KEYC_OFFSET + 0x78)
#define KEYC_SDL (KEYC_OFFSET + 0x79)
#define KEYC_SELECT (KEYC_OFFSET + 0x7a)
#define KEYC_SEND (KEYC_OFFSET + 0x7b)
#define KEYC_SEOL (KEYC_OFFSET + 0x7c)
#define KEYC_SEXIT (KEYC_OFFSET + 0x7d)
#define KEYC_SF (KEYC_OFFSET + 0x7e)
#define KEYC_SFIND (KEYC_OFFSET + 0x7f)
#define KEYC_SHELP (KEYC_OFFSET + 0x80)
#define KEYC_SHOME (KEYC_OFFSET + 0x81)
#define KEYC_SIC (KEYC_OFFSET + 0x82)
#define KEYC_SLEFT (KEYC_OFFSET + 0x83)
#define KEYC_SMESSAGE (KEYC_OFFSET + 0x84)
#define KEYC_SMOVE (KEYC_OFFSET + 0x85)
#define KEYC_SNEXT (KEYC_OFFSET + 0x86)
#define KEYC_SOPTIONS (KEYC_OFFSET + 0x87)
#define KEYC_SPREVIOUS (KEYC_OFFSET + 0x88)
#define KEYC_SPRINT (KEYC_OFFSET + 0x89)
#define KEYC_SR (KEYC_OFFSET + 0x8a)
#define KEYC_SREDO (KEYC_OFFSET + 0x8b)
#define KEYC_SREPLACE (KEYC_OFFSET + 0x8c)
#define KEYC_SRIGHT (KEYC_OFFSET + 0x8d)
#define KEYC_SRSUME (KEYC_OFFSET + 0x8e)
#define KEYC_SSAVE (KEYC_OFFSET + 0x8f)
#define KEYC_SSUSPEND (KEYC_OFFSET + 0x90)
#define KEYC_STAB (KEYC_OFFSET + 0x91)
#define KEYC_SUNDO (KEYC_OFFSET + 0x92)
#define KEYC_SUSPEND (KEYC_OFFSET + 0x93)
#define KEYC_UNDO (KEYC_OFFSET + 0x94)
#define KEYC_UP (KEYC_OFFSET + 0x95)
#define KEYC_MOUSE (KEYC_OFFSET + 0x96)

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

/* Output codes. */
#define TTY_CHARACTER 0
#define TTY_CURSORUP 1
#define TTY_CURSORDOWN 2
#define TTY_CURSORRIGHT 3
#define TTY_CURSORLEFT 4
#define TTY_INSERTCHARACTER 5
#define TTY_DELETECHARACTER 6
#define TTY_INSERTLINE 7
#define TTY_DELETELINE 8
#define TTY_CLEARLINE 9
#define TTY_CLEARENDOFLINE 10
#define TTY_CLEARSTARTOFLINE 11
#define TTY_CURSORMOVE 12
#define TTY_ATTRIBUTES 13
#define TTY_CURSOROFF 14
#define TTY_CURSORON 15
#define TTY_REVERSEINDEX 16
#define TTY_SCROLLREGION 17
#define TTY_INSERTON 18
#define TTY_INSERTOFF 19
#define TTY_MOUSEON 20
#define TTY_MOUSEOFF 21 /* XXX merge all on/off into 1 arg? */

/* Message codes. */
enum hdrtype {
	MSG_COMMAND,
	MSG_ERROR,
	MSG_PRINT,
	MSG_EXIT,
	MSG_EXITING,
	MSG_EXITED,
	MSG_DETACH,
	MSG_IDENTIFY,
	MSG_READY,
	MSG_RESIZE,
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

	u_int		sx;
	u_int		sy;

	size_t		termlen;
};

struct msg_resize_data {
	u_int		sx;
	u_int		sy;
};

/* Attributes. */
#define ATTR_BRIGHT 0x1
#define ATTR_DIM 0x2
#define ATTR_UNDERSCORE 0x4
#define ATTR_BLINK 0x8
#define ATTR_REVERSE 0x10
#define ATTR_HIDDEN 0x20
#define ATTR_ITALICS 0x40
#define ATTR_CHARSET 0x80	/* alternative character set */

/* Modes. */
#define MODE_CURSOR 0x01
#define MODE_INSERT 0x02
#define MODE_KCURSOR 0x04
#define MODE_KKEYPAD 0x08
#define MODE_SAVED 0x10
#define MODE_MOUSE 0x20

/* Screen selection. */
struct screen_sel {
	int		 flag;

	u_int		 sx;
	u_int		 sy;

	u_int		 ex;
	u_int		 ey;
};

/* Virtual screen. */
struct screen {
	char		*title;

	u_char	       **grid_data;
	u_char	       **grid_attr;
	u_char	       **grid_colr;
	u_int		*grid_size;

 	u_int		 dx;		/* display x size */
	u_int		 dy;		/* display y size */

	u_int		 hsize;		/* history y size */
	u_int		 hlimit;	/* history y limit */

	u_int		 rupper;	/* scroll region top */
	u_int		 rlower;	/* scroll region bottom */

	u_int		 cx;		/* cursor x */
	u_int		 cy;		/* cursor y */
	u_char		 attr;
	u_char		 colr;		/* fg:bg */

	u_int		 saved_cx;
	u_int		 saved_cy;
	u_char		 saved_attr;
	u_char		 saved_colr;

	int		 mode;

	struct screen_sel sel;
};

/* Screen redraw context. */
struct screen_redraw_ctx {
	void		*data;
	void		 (*write)(void *, int, ...);

	u_int		 saved_cx;
	u_int		 saved_cy;

	struct screen	*s;
};

/* Screen write context. */
struct screen_write_ctx {
	void		*data;
	void		 (*write)(void *, int, ...);

	struct screen	*s;
};

/* Screen display access macros. */
#define screen_x(s, x) (x)
#define screen_y(s, y) ((s)->hsize + y)

#define screen_last_x(s) ((s)->dx - 1)
#define screen_last_y(s) ((s)->dy - 1)

#define screen_size_x(s) ((s)->dx)
#define screen_size_y(s) ((s)->dy)

#define screen_in_x(s, x) ((x) < screen_size_x(s))
#define screen_in_y(s, y) ((y) < screen_size_y(s))
#define screen_in_region(s, y) ((y) >= (s)->rupper && (y) <= (s)->rlower)

/* These are inclusive... */
#define screen_left_x(s, x) ((x) + 1)
#define screen_right_x(s, x) \
	((x) < screen_size_x(s) ? screen_size_x(s) - (x) : 0)

#define screen_above_y(s, y) ((y) + 1)
#define screen_below_y(s, y) \
	((y) < screen_size_y(s) ? screen_size_y(s) - (y) : 0)

#define SCREEN_DEBUG(s) do {						\
	log_warnx("%s: cx=%u,cy=%u sx=%u,sy=%u", __func__,		\
	    s->cx, s->cy, screen_size_x(s), screen_size_y(s));		\
} while (0)
#define SCREEN_DEBUG1(s, n) do {					\
	log_warnx("%s: cx=%u,cy=%u sx=%u,sy=%u n=%u m=%u", __func__,	\
	    s->cx, s->cy, screen_size_x(s), screen_size_y(s), n);	\
} while (0)
#define SCREEN_DEBUG2(s, n, m) do {					\
	log_warnx("%s: cx=%u,cy=%u sx=%u,sy=%u n=%u m=%u", __func__,	\
	    s->cx, s->cy, screen_size_x(s), screen_size_y(s), n, m);	\
} while (0)
#define SCREEN_DEBUG3(s, n, m, o) do {					\
	log_warnx("%s: cx=%u,cy=%u sx=%u,sy=%u n=%u m=%u o=%u",		\
	    __func__, s->cx, s->cy, screen_size_x(s), screen_size_y(s), \
	    n, m, o);							\
} while (0)
#define SCREEN_DEBUG4(s, n, m, o, p) do {				\
	log_warnx("%s: cx=%u,cy=%u sx=%u,sy=%u n=%u m=%u o=%u p=%u",	\
	    __func__, s->cx, s->cy, screen_size_x(s), screen_size_y(s), \
	    n, m, o, p);					       	\
} while (0)

/* Input parser sequence argument. */
struct input_arg {
	u_char		 data[64];
	size_t		 used;
};

/* Input parser context. */
struct input_ctx {
	struct window	*w;
	struct screen_write_ctx ctx;

	u_char		*buf;
	size_t		 len;
	size_t		 off;

#define MAXSTRINGLEN	1024
	u_char		*string_buf;
	size_t		 string_len;
	int		 string_type;
#define STRING_TITLE 0
#define STRING_NAME 1
#define STRING_IGNORE 2

	void 		*(*state)(u_char, struct input_ctx *);

	u_char		 private;
	ARRAY_DECL(, struct input_arg) args;
};

/*
 * Window mode. Windows can be in several modes and this is used to call the
 * right function to handle input and output.
 */
struct client;
struct window_mode {
	struct screen *(*init)(struct window *);
	void	(*free)(struct window *);
	void	(*resize)(struct window *, u_int, u_int);
	void	(*key)(struct window *, struct client *, int);
};

/* Window structure. */
struct window {
	char		*name;
	char		*cmd;

	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	struct input_ctx ictx;

	int		 flags;
#define WINDOW_BELL 0x1
#define WINDOW_HIDDEN 0x2
#define WINDOW_ACTIVITY 0x4
#define WINDOW_MONITOR 0x8
#define WINDOW_AGGRESSIVE 0x10
#define WINDOW_ZOMBIFY 0x20

	u_int		 limitx;
	u_int		 limity;

	struct screen	*screen;
	struct screen	 base;

	const struct window_mode *mode;
	void		*modedata;

	u_int		 references;
};
ARRAY_DECL(windows, struct window *);

/* Entry on local window list. */
struct winlink {
	int		 idx;
	struct window	*window;

	RB_ENTRY(winlink) entry;
};
RB_HEAD(winlinks, winlink);

/* Option data structures. */
struct options_entry {
	char		*name;

	enum {
		OPTIONS_STRING,
		OPTIONS_NUMBER,
		OPTIONS_KEY,
		OPTIONS_COLOURS
	} type;
	union {
		char	*string;
		long long number;
		int	 key;
		u_char	 colours;
	} value;

	SPLAY_ENTRY(options_entry) entry;
};

struct options {
	SPLAY_HEAD(options_tree, options_entry) tree;
	struct options	*parent;
};

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

	TAILQ_ENTRY(session_alert) entry;
};

struct session {
	char		*name;
	struct timeval	 tv;

	u_int		 sx;
	u_int		 sy;

	struct winlink	*curw;
	struct winlink	*lastw;
	struct winlinks	 windows;

	struct options	 options;

	struct paste_stack buffers;

	TAILQ_HEAD(, session_alert) alerts;

#define SESSION_UNATTACHED 0x1	/* not attached to any clients */
	int		 flags;
};
ARRAY_DECL(sessions, struct session *);

/* TTY information. */
struct tty_key {
	int	 	 code;
	char		*string;

	RB_ENTRY(tty_key) entry;
};

struct tty_term {
	char		*name;
	TERMINAL	*term;
	u_int		 references;

	TAILQ_ENTRY(tty_term) entry;
};

struct tty {
	char		*path;

	char		*termname;
	struct tty_term	*term;

	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	struct termios   tio;

	u_int		 attr;
	u_int		 colr;

	u_char		 acs[UCHAR_MAX + 1];

#define TTY_NOCURSOR 0x1
#define TTY_FREEZE 0x2
#define TTY_ESCAPE 0x4
	int		 flags;

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

	struct tty 	 tty;
	struct timeval	 status_timer;

	u_int		 sx;
	u_int		 sy;

#define CLIENT_TERMINAL 0x1
#define CLIENT_PREFIX 0x2
#define CLIENT_MOUSE 0x4
#define CLIENT_REDRAW 0x8
#define CLIENT_STATUS 0x10
	int		 flags;

	char		*message_string;
	struct timeval	 message_timer;

	char		*prompt_string;
	char		*prompt_buffer;
	size_t		 prompt_index;
	void		 (*prompt_callback)(void *, char *);
	void		*prompt_data;

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
};

struct cmd_entry {
	const char	*name;
	const char	*alias;
	const char	*usage;

#define CMD_STARTSERVER 0x1
#define CMD_CANTNEST 0x2
#define CMD_KFLAG 0x4
#define CMD_DFLAG 0x8
#define CMD_ONEARG 0x10
#define CMD_ZEROONEARG 0x20
	int		 flags;

	void		 (*init)(struct cmd *, int);
	int		 (*parse)(struct cmd *, int, char **, char **);
	void		 (*exec)(struct cmd *, struct cmd_ctx *);
	void		 (*send)(struct cmd *, struct buffer *);
	void	         (*recv)(struct cmd *, struct buffer *);
	void		 (*free)(struct cmd *);
	void 		 (*print)(struct cmd *, char *, size_t);
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

/* Key binding. */
struct binding {
	int		 key;
	struct cmd	*cmd;
};
ARRAY_DECL(bindings, struct binding *);

/* Set/display option data. */
struct set_option_entry {
	const char	*name;
	enum {
		SET_OPTION_STRING,
		SET_OPTION_NUMBER,
		SET_OPTION_KEY,		
		SET_OPTION_COLOUR,
		SET_OPTION_FLAG,
		SET_OPTION_CHOICE
	} type;

	u_int		 minimum;
	u_int		 maximum;
	
	const char     **choices;
};
extern const struct set_option_entry set_option_table[];
#define NSETOPTION 15

/* Edit keys. */
enum mode_key {
	MODEKEY_BOL,
	MODEKEY_CLEARSEL,
	MODEKEY_COPYSEL,
	MODEKEY_DOWN,
	MODEKEY_EOL,
	MODEKEY_LEFT,
	MODEKEY_NONE,
	MODEKEY_NPAGE,
	MODEKEY_NWORD,
	MODEKEY_PPAGE,
	MODEKEY_PWORD,
	MODEKEY_QUIT,
	MODEKEY_RIGHT,
	MODEKEY_STARTSEL,
	MODEKEY_UP,
};
#define MODEKEY_EMACS 0
#define MODEKEY_VI 1

#ifdef NO_STRTONUM
/* strtonum.c */
long long	 strtonum(const char *, long long, long long, const char **);
#endif

#ifdef NO_STRLCPY
/* strlcpy.c */
size_t	 	 strlcpy(char *, const char *, size_t);
#endif

#ifdef NO_STRLCAT
/* strlcat.c */
size_t	 	 strlcat(char *, const char *, size_t);
#endif

#ifdef NO_DAEMON
/* daemon.c */
int	 	 daemon(int, int);
#endif

#ifdef NO_FORKPTY
/* forkpty.c */
pid_t		 forkpty(int *, char *, struct termios *, struct winsize *);
#endif

#ifdef NO_ASPRINTF
/* asprintf.c */
int	asprintf(char **, const char *, ...);
int	vasprintf(char **, const char *, va_list);
#endif

#ifdef NO_FGETLN
/* fgetln.c */
char   *fgetln(FILE *, size_t *);
#endif

/* tmux.c */
extern volatile sig_atomic_t sigwinch;
extern volatile sig_atomic_t sigterm;
extern struct options global_options;
extern char	*cfg_file;
extern int	 debug_level;
extern int	 be_quiet;
void		 logfile(const char *);
void		 siginit(void);
void		 sigreset(void);

/* cfg.c */
int		 load_cfg(const char *, char **x);

/* mode-key.c */
enum mode_key	 mode_key_lookup(int, int);

/* options.c */
int	options_cmp(struct options_entry *, struct options_entry *);
SPLAY_PROTOTYPE(options_tree, options_entry, entry, options_cmp);
void	options_init(struct options *, struct options *);
void	options_free(struct options *);
struct options_entry *options_find1(struct options *, const char *);
struct options_entry *options_find(struct options *, const char *);
void printflike3 options_set_string(
    	    struct options *, const char *, const char *, ...);
char   *options_get_string(struct options *, const char *);
void	options_set_number(struct options *, const char *, long long);
long long options_get_number(struct options *, const char *);

/* tty.c */
void		 tty_init(struct tty *, char *, char *);
void		 tty_set_title(struct tty *, const char *);
int		 tty_open(struct tty *, char **);
void		 tty_close(struct tty *);
void		 tty_free(struct tty *);
void		 tty_vwrite(struct tty *, struct screen *s, int, va_list);

/* tty-keys.c */
int		 tty_keys_cmp(struct tty_key *, struct tty_key *);
RB_PROTOTYPE(tty_keys, tty_key, entry, tty_keys_cmp);
void		 tty_keys_init(struct tty *);
void		 tty_keys_free(struct tty *);
int		 tty_keys_next(struct tty *, int *);

/* tty-write.c */
void		 tty_write_client(void *, int, ...);
void		 tty_vwrite_client(void *, int, va_list);
void		 tty_write_window(void *, int, ...);
void		 tty_vwrite_window(void *, int, va_list);
void		 tty_write_session(void *, int, ...);
void		 tty_vwrite_session(void *, int, va_list);

/* paste.c */
void		 paste_init_stack(struct paste_stack *);
void		 paste_free_stack(struct paste_stack *);
struct paste_buffer *paste_walk_stack(struct paste_stack *, uint *);
struct paste_buffer *paste_get_top(struct paste_stack *);
struct paste_buffer *paste_get_index(struct paste_stack *, u_int);
int	     	 paste_free_top(struct paste_stack *);
int		 paste_free_index(struct paste_stack *, u_int);
void		 paste_add(struct paste_stack *, const char *, u_int);
int		 paste_replace(struct paste_stack *, u_int, const char *);

/* arg.c */
struct client 	*arg_parse_client(const char *);
struct session 	*arg_parse_session(const char *);
int		 arg_parse_window(const char *, struct session **, int *);

/* cmd.c */
char		*cmd_complete(const char *);
struct cmd	*cmd_parse(int, char **, char **);
void		 cmd_exec(struct cmd *, struct cmd_ctx *);
void		 cmd_send(struct cmd *, struct buffer *);
struct cmd	*cmd_recv(struct buffer *);
void		 cmd_free(struct cmd *);
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
extern const struct cmd_entry cmd_command_prompt_entry;
extern const struct cmd_entry cmd_copy_mode_entry;
extern const struct cmd_entry cmd_delete_buffer_entry;
extern const struct cmd_entry cmd_detach_client_entry;
extern const struct cmd_entry cmd_has_session_entry;
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
extern const struct cmd_entry cmd_move_window_entry;
extern const struct cmd_entry cmd_new_session_entry;
extern const struct cmd_entry cmd_new_window_entry;
extern const struct cmd_entry cmd_next_window_entry;
extern const struct cmd_entry cmd_paste_buffer_entry;
extern const struct cmd_entry cmd_previous_window_entry;
extern const struct cmd_entry cmd_refresh_client_entry;
extern const struct cmd_entry cmd_rename_session_entry;
extern const struct cmd_entry cmd_rename_window_entry;
extern const struct cmd_entry cmd_respawn_window_entry;
extern const struct cmd_entry cmd_scroll_mode_entry;
extern const struct cmd_entry cmd_select_window_entry;
extern const struct cmd_entry cmd_send_keys_entry;
extern const struct cmd_entry cmd_send_prefix_entry;
extern const struct cmd_entry cmd_select_prompt_entry;
extern const struct cmd_entry cmd_set_buffer_entry;
extern const struct cmd_entry cmd_set_option_entry;
extern const struct cmd_entry cmd_set_window_option_entry;
extern const struct cmd_entry cmd_show_buffer_entry;
extern const struct cmd_entry cmd_show_options_entry;
extern const struct cmd_entry cmd_show_window_options_entry;
extern const struct cmd_entry cmd_start_server_entry;
extern const struct cmd_entry cmd_swap_window_entry;
extern const struct cmd_entry cmd_switch_client_entry;
extern const struct cmd_entry cmd_unbind_key_entry;
extern const struct cmd_entry cmd_unlink_window_entry;

/* cmd-string.c */
int	cmd_string_parse(const char *, struct cmd **, char **);

/* cmd-generic.c */
#define CMD_TARGET_WINDOW_USAGE "[-t target-window]"
#define CMD_TARGET_SESSION_USAGE "[-t target-session]"
#define CMD_TARGET_CLIENT_USAGE "[-t target-client]"
void	cmd_target_init(struct cmd *, int);
int	cmd_target_parse(struct cmd *, int, char **, char **);
void	cmd_target_exec(struct cmd *, struct cmd_ctx *);
void	cmd_target_send(struct cmd *, struct buffer *);
void	cmd_target_recv(struct cmd *, struct buffer *);
void	cmd_target_free(struct cmd *);
void	cmd_target_print(struct cmd *, char *, size_t);
#define CMD_SRCDST_WINDOW_USAGE "[-s src-window] [-t dst-window]"
#define CMD_SRCDST_SESSION_USAGE "[-s src-session] [-t dst-session]"
#define CMD_SRCDST_CLIENT_USAGE "[-s src-client] [-t dst-client]"
void	cmd_srcdst_init(struct cmd *, int);
int	cmd_srcdst_parse(struct cmd *, int, char **, char **);
void	cmd_srcdst_exec(struct cmd *, struct cmd_ctx *);
void	cmd_srcdst_send(struct cmd *, struct buffer *);
void	cmd_srcdst_recv(struct cmd *, struct buffer *);
void	cmd_srcdst_free(struct cmd *);
void	cmd_srcdst_print(struct cmd *, char *, size_t);
#define CMD_BUFFER_WINDOW_USAGE "[-b buffer-index] [-t target-window]"
#define CMD_BUFFER_SESSION_USAGE "[-b buffer-index] [-t target-session]"
#define CMD_BUFFER_CLIENT_USAGE "[-b buffer-index] [-t target-client]"
void	cmd_buffer_init(struct cmd *, int);
int	cmd_buffer_parse(struct cmd *, int, char **, char **);
void	cmd_buffer_exec(struct cmd *, struct cmd_ctx *);
void	cmd_buffer_send(struct cmd *, struct buffer *);
void	cmd_buffer_recv(struct cmd *, struct buffer *);
void	cmd_buffer_free(struct cmd *);
void	cmd_buffer_print(struct cmd *, char *, size_t);

/* client.c */
int	 client_init(const char *, struct client_ctx *, int);
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
extern struct bindings key_bindings;
void	 key_bindings_add(int, struct cmd *);
void	 key_bindings_remove(int);
void	 key_bindings_init(void);
void	 key_bindings_free(void);
void	 key_bindings_dispatch(int, struct client *);
void printflike2 key_bindings_error(struct cmd_ctx *, const char *, ...);
void printflike2 key_bindings_print(struct cmd_ctx *, const char *, ...);
void printflike2 key_bindings_info(struct cmd_ctx *, const char *, ...);

/* key-string.c */
int	 key_string_lookup_string(const char *);
const char *key_string_lookup_key(int);

/* server.c */
extern struct clients clients;
int	 server_start(const char *);

/* server-msg.c */
int	 server_msg_dispatch(struct client *);

/* server-fn.c */
void	 server_set_client_message(struct client *, const char *);
void	 server_clear_client_message(struct client *);
void	 server_set_client_prompt(
	     struct client *, const char *, void (*)(void *, char *), void *);
void	 server_clear_client_prompt(struct client *);
struct session *server_extract_session(
    	     struct msg_command_data *, char *, char **);
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

/* status.c */
void	 status_redraw(struct client *);
void	 status_message_redraw(struct client *);
void	 status_prompt_redraw(struct client *);
void	 status_prompt_key(struct client *, int);

/* resize.c */
void	 recalculate_sizes(void);

/* input.c */
void	 input_init(struct window *);
void	 input_free(struct window *);
void	 input_parse(struct window *);

/* input-key.c */
void	 input_key(struct window *, int);

/* screen-display.c */
void	 screen_display_set_cell(
    	     struct screen *, u_int, u_int, u_char, u_char, u_char);
void	 screen_display_make_lines(struct screen *, u_int, u_int);
void	 screen_display_free_lines(struct screen *, u_int, u_int);
void	 screen_display_move_lines(struct screen *, u_int, u_int, u_int);
void	 screen_display_fill_area(struct screen *,
    	     u_int, u_int, u_int, u_int, u_char, u_char, u_char);
void	 screen_display_scroll_region_up(struct screen *);
void	 screen_display_scroll_region_down(struct screen *);
void	 screen_display_insert_lines(struct screen *, u_int, u_int);
void	 screen_display_insert_lines_region(struct screen *, u_int, u_int);
void	 screen_display_delete_lines(struct screen *, u_int, u_int);
void	 screen_display_delete_lines_region(struct screen *, u_int, u_int);
void	 screen_display_insert_characters(struct screen *, u_int, u_int, u_int);
void	 screen_display_delete_characters(struct screen *, u_int, u_int, u_int);
void	 screen_display_copy_area(struct screen *, struct screen *,
    	     u_int, u_int, u_int, u_int, u_int, u_int);

/* screen-write.c */
void	 screen_write_start_window(struct screen_write_ctx *, struct window *);
void	 screen_write_start_client(struct screen_write_ctx *, struct client *);
void	 screen_write_start_session(
    	    struct screen_write_ctx *, struct session *);
void	 screen_write_start(struct screen_write_ctx *,
    	    struct screen *, void (*)(void *, int, ...), void *);
void	 screen_write_stop(struct screen_write_ctx *);
void	 screen_write_set_title(struct screen_write_ctx *, char *);
void	 screen_write_put_character(struct screen_write_ctx *, u_char);
size_t printflike2 screen_write_put_string_rjust(
	     struct screen_write_ctx *, const char *, ...);
void printflike2 screen_write_put_string(
	     struct screen_write_ctx *, const char *, ...);
void	 screen_write_set_attributes(struct screen_write_ctx *, u_char, u_char);
void	 screen_write_set_region(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_cursor_up_scroll(struct screen_write_ctx *);
void	 screen_write_cursor_down_scroll(struct screen_write_ctx *);
void	 screen_write_cursor_up(struct screen_write_ctx *, u_int);
void	 screen_write_cursor_down(struct screen_write_ctx *, u_int);
void	 screen_write_cursor_left(struct screen_write_ctx *, u_int);
void	 screen_write_cursor_right(struct screen_write_ctx *, u_int);
void	 screen_write_delete_lines(struct screen_write_ctx *, u_int);
void	 screen_write_delete_characters(struct screen_write_ctx *, u_int);
void	 screen_write_insert_lines(struct screen_write_ctx *, u_int);
void	 screen_write_insert_characters(struct screen_write_ctx *, u_int);
void	 screen_write_move_cursor(struct screen_write_ctx *, u_int, u_int);
void	 screen_write_fill_end_of_screen(struct screen_write_ctx *);
void	 screen_write_fill_screen(struct screen_write_ctx *);
void	 screen_write_fill_end_of_line(struct screen_write_ctx *);
void	 screen_write_fill_start_of_line(struct screen_write_ctx *);
void	 screen_write_fill_line(struct screen_write_ctx *);
void	 screen_write_set_mode(struct screen_write_ctx *, int);
void	 screen_write_clear_mode(struct screen_write_ctx *, int);
void	 screen_write_copy_area(struct screen_write_ctx *,
    	     struct screen *, u_int, u_int, u_int, u_int);

/* screen-redraw.c */
void	screen_redraw_start_window(struct screen_redraw_ctx *, struct window *);
void	screen_redraw_start_client(struct screen_redraw_ctx *, struct client *);
void	screen_redraw_start_session(
    	    struct screen_redraw_ctx *, struct session *);
void	screen_redraw_start(struct screen_redraw_ctx *,
    	    struct screen *, void (*)(void *, int, ...), void *);
void	screen_redraw_stop(struct screen_redraw_ctx *);
void	screen_redraw_move_cursor(struct screen_redraw_ctx *, u_int, u_int);
void	screen_redraw_set_attributes(struct screen_redraw_ctx *, u_int, u_int);
void printflike2 screen_redraw_write_string(
    	     struct screen_redraw_ctx *, const char *, ...);
void	screen_redraw_cell(struct screen_redraw_ctx *, u_int, u_int);
void	screen_redraw_area(
    	    struct screen_redraw_ctx *, u_int, u_int, u_int, u_int);
void	screen_redraw_lines(struct screen_redraw_ctx *, u_int, u_int);
void	screen_redraw_columns(struct screen_redraw_ctx *, u_int, u_int);

/* screen.c */
const char *screen_colourstring(u_char);
u_char	 screen_stringcolour(const char *);
void	 screen_create(struct screen *, u_int, u_int, u_int);
void	 screen_reset(struct screen *);
void	 screen_destroy(struct screen *);
void	 screen_resize(struct screen *, u_int, u_int);
void	 screen_expand_line(struct screen *, u_int, u_int);
void	 screen_reduce_line(struct screen *, u_int, u_int);
void	 screen_get_cell(
    	     struct screen *, u_int, u_int, u_char *, u_char *, u_char *);
void	 screen_set_cell(struct screen *, u_int, u_int, u_char, u_char, u_char);
void	 screen_make_lines(struct screen *, u_int, u_int);
void	 screen_free_lines(struct screen *, u_int, u_int);
void	 screen_move_lines(struct screen *, u_int, u_int, u_int);
void	 screen_fill_area(struct screen *,
    	     u_int, u_int, u_int, u_int, u_char, u_char, u_char);
void	 screen_set_selection(struct screen *, u_int, u_int, u_int, u_int);
void	 screen_clear_selection(struct screen *);
int	 screen_check_selection(struct screen *, u_int, u_int);

/* window.c */
extern struct windows windows;
int		 window_cmp(struct window *, struct window *);
int		 winlink_cmp(struct winlink *, struct winlink *);
RB_PROTOTYPE(windows, window, entry, window_cmp);
RB_PROTOTYPE(winlinks, winlink, entry, winlink_cmp);
struct winlink	*winlink_find_by_index(struct winlinks *, int);
struct winlink 	*winlink_find_by_window(struct winlinks *, struct window *);
int		 winlink_next_index(struct winlinks *);
struct winlink	*winlink_add(struct winlinks *, struct window *, int);
void		 winlink_remove(struct winlinks *, struct winlink *);
struct winlink	*winlink_next(struct winlinks *, struct winlink *);
struct winlink	*winlink_previous(struct winlinks *, struct winlink *);
struct window	*window_create(const char *,
		     const char *, const char **, u_int, u_int, u_int);
int		 window_spawn(struct window *, const char *, const char **);
void		 window_destroy(struct window *);
int		 window_resize(struct window *, u_int, u_int);
int		 window_set_mode(struct window *, const struct window_mode *);
void		 window_reset_mode(struct window *);
void		 window_parse(struct window *);
void		 window_key(struct window *, struct client *, int);

/* window-copy.c */
extern const struct window_mode window_copy_mode;

/* window-scroll.c */
extern const struct window_mode window_scroll_mode;

/* window-more.c */
extern const struct window_mode window_more_mode;
void 		 window_more_vadd(struct window *, const char *, va_list);
void printflike2 window_more_add(struct window *, const char *, ...);

/* session.c */
extern struct sessions sessions;
void	 session_alert_add(struct session *, struct window *, int);
void	 session_alert_cancel(struct session *, struct winlink *);
int	 session_alert_has(struct session *, struct winlink *, int);
int	 session_alert_has_window(struct session *, struct window *, int);
struct session	*session_find(const char *);
struct session	*session_create(const char *, const char *, u_int, u_int);
void	 	 session_destroy(struct session *);
int	 	 session_index(struct session *, u_int *);
struct winlink	*session_new(struct session *, const char *, const char *, int);
struct winlink	*session_attach(struct session *, struct window *, int);
int		 session_detach(struct session *, struct winlink *);
int		 session_has(struct session *, struct window *);
int		 session_next(struct session *);
int		 session_previous(struct session *);
int		 session_select(struct session *, int);
int		 session_last(struct session *);

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
char 		*xdirname(const char *);
char 		*xbasename(const char *);

/* xmalloc-debug.c */
#ifdef DEBUG
#define xmalloc_caller() __builtin_return_address(0)

void		 xmalloc_clear(void);
void		 xmalloc_report(pid_t, const char *);

void		 xmalloc_new(void *, void *, size_t);
void		 xmalloc_change(void *, void *, void *, size_t);
void		 xmalloc_free(void *);
#endif

#endif
