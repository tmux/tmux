/* $Id: tmux.h,v 1.2 2007-07-25 23:13:18 nicm Exp $ */

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

#ifndef NSCR_H
#define NSCR_H

#include <sys/param.h>
#include <sys/tree.h>
#include <sys/queue.h>

#include <poll.h>
#include <stdarg.h>
#include <stdio.h>

#include "array.h"

extern char	*__progname;

#define MAXNAMELEN	32
#define MAXTITLELEN	192

/* Fatal errors. */
#define fatal(msg) log_fatal("%s: %s", __func__, msg);
#define fatalx(msg) log_fatal("%s: %s", __func__, msg);

/* Definition to shut gcc up about unused arguments. */
#define unused __attribute__ ((unused))

/* Attribute to make gcc check printf-like arguments. */
#define printflike1 __attribute__ ((format (printf, 1, 2)))
#define printflike2 __attribute__ ((format (printf, 2, 3)))
#define printflike3 __attribute__ ((format (printf, 3, 4)))
#define printflike4 __attribute__ ((format (printf, 4, 5)))

/* Ensure buffer size. */
#define ENSURE_SIZE(buf, len, size) do {				\
	(buf) = ensure_size(buf, &(len), 1, size);			\
} while (0)
#define ENSURE_SIZE2(buf, len, nmemb, size) do {			\
	(buf) = ensure_size(buf, &(len), nmemb, size);			\
} while (0)
#define ENSURE_FOR(buf, len, size, adj) do {				\
	(buf) = ensure_for(buf, &(len), size, adj);			\
} while (0)

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

/* Key codes. ncurses defines KEY_*. Grrr. */
#define KEYC_NONE 256
#define KEYC_A1 -1
#define KEYC_A3 -2
#define KEYC_B2 -3
#define KEYC_BACKSPACE -4
#define KEYC_BEG -5
#define KEYC_BTAB -6
#define KEYC_C1 -7
#define KEYC_C3 -8
#define KEYC_CANCEL -9
#define KEYC_CATAB -10
#define KEYC_CLEAR -11
#define KEYC_CLOSE -12
#define KEYC_COMMAND -13
#define KEYC_COPY -14
#define KEYC_CREATE -15
#define KEYC_CTAB -16
#define KEYC_DC -17
#define KEYC_DL -18
#define KEYC_DOWN -19
#define KEYC_EIC -20
#define KEYC_END -21
#define KEYC_ENTER -22
#define KEYC_EOL -23
#define KEYC_EOS -24
#define KEYC_EXIT -25
#define KEYC_F0 -26
#define KEYC_F1 -27
#define KEYC_F10 -28
#define KEYC_F11 -29
#define KEYC_F12 -30
#define KEYC_F13 -31
#define KEYC_F14 -32
#define KEYC_F15 -33
#define KEYC_F16 -34
#define KEYC_F17 -35
#define KEYC_F18 -36
#define KEYC_F19 -37
#define KEYC_F2 -38
#define KEYC_F20 -39
#define KEYC_F21 -40
#define KEYC_F22 -41
#define KEYC_F23 -42
#define KEYC_F24 -43
#define KEYC_F25 -44
#define KEYC_F26 -45
#define KEYC_F27 -46
#define KEYC_F28 -47
#define KEYC_F29 -48
#define KEYC_F3 -49
#define KEYC_F30 -50
#define KEYC_F31 -51
#define KEYC_F32 -52
#define KEYC_F33 -53
#define KEYC_F34 -54
#define KEYC_F35 -55
#define KEYC_F36 -56
#define KEYC_F37 -57
#define KEYC_F38 -58
#define KEYC_F39 -59
#define KEYC_F4 -60
#define KEYC_F40 -61
#define KEYC_F41 -62
#define KEYC_F42 -63
#define KEYC_F43 -64
#define KEYC_F44 -65
#define KEYC_F45 -66
#define KEYC_F46 -67
#define KEYC_F47 -68
#define KEYC_F48 -69
#define KEYC_F49 -70
#define KEYC_F5 -71
#define KEYC_F50 -72
#define KEYC_F51 -73
#define KEYC_F52 -74
#define KEYC_F53 -75
#define KEYC_F54 -76
#define KEYC_F55 -77
#define KEYC_F56 -78
#define KEYC_F57 -79
#define KEYC_F58 -80
#define KEYC_F59 -81
#define KEYC_F6 -82
#define KEYC_F60 -83
#define KEYC_F61 -84
#define KEYC_F62 -85
#define KEYC_F63 -86
#define KEYC_F7 -87
#define KEYC_F8 -88
#define KEYC_F9 -89
#define KEYC_FIND -90
#define KEYC_HELP -91
#define KEYC_HOME -92
#define KEYC_IC -93
#define KEYC_IL -94
#define KEYC_LEFT -95
#define KEYC_LL -96
#define KEYC_MARK -97
#define KEYC_MESSAGE -98
#define KEYC_MOVE -99
#define KEYC_NEXT -100
#define KEYC_NPAGE -101
#define KEYC_OPEN -102
#define KEYC_OPTIONS -103
#define KEYC_PPAGE -104
#define KEYC_PREVIOUS -105
#define KEYC_PRINT -106
#define KEYC_REDO -107
#define KEYC_REFERENCE -108
#define KEYC_REFRESH -109
#define KEYC_REPLACE -110
#define KEYC_RESTART -111
#define KEYC_RESUME -112
#define KEYC_RIGHT -113
#define KEYC_SAVE -114
#define KEYC_SBEG -115
#define KEYC_SCANCEL -116
#define KEYC_SCOMMAND -117
#define KEYC_SCOPY -118
#define KEYC_SCREATE -119
#define KEYC_SDC -120
#define KEYC_SDL -121
#define KEYC_SELECT -122
#define KEYC_SEND -123
#define KEYC_SEOL -124
#define KEYC_SEXIT -125
#define KEYC_SF -126
#define KEYC_SFIND -127
#define KEYC_SHELP -128
#define KEYC_SHOME -129
#define KEYC_SIC -130
#define KEYC_SLEFT -131
#define KEYC_SMESSAGE -132
#define KEYC_SMOVE -133
#define KEYC_SNEXT -134
#define KEYC_SOPTIONS -135
#define KEYC_SPREVIOUS -136
#define KEYC_SPRINT -137
#define KEYC_SR -138
#define KEYC_SREDO -139
#define KEYC_SREPLACE -140
#define KEYC_SRIGHT -141
#define KEYC_SRSUME -142
#define KEYC_SSAVE -143
#define KEYC_SSUSPEND -144
#define KEYC_STAB -145
#define KEYC_SUNDO -146
#define KEYC_SUSPEND -147
#define KEYC_UNDO -148
#define KEYC_UP -149
#define KEYC_MOUSE -150

/* Escape codes. */
/*
	AL=\E[%dL	parm_insert_line
	DC=\E[%dP	parm_dch
	DL=\E[%dM	parm_delete_line
	DO=\E[%dB	parm_down_cursor
	IC=\E[%d@	parm_ich
	Km=\E[M		key_mouse
	LE=\E[%dD	parm_left_cursor
	RI=\E[%dC	parm_right_cursor
	UP=\E[%dA	parm_up_cursor
	ac=++,,--..00``aaffgghhiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~
			acs_chars
	ae=^O		exit_alt_charset_mode
	al=\E[L		insert_line
	as=^N		enter_alt_charset_mode
	bl=^G		bell
	bt=\E[Z		back_tab
	cb=\E[1K	clear_bol
	cd=\E[J		clear_eos
	ce=\E[K		clear_eol
	cl=\E[H\E[J	clear_screen
	cm=\E[%i%d;%dH	cursor_address
	cr=^M		carriage_return
	cs=\E[%i%d;%dr	change_scroll_region
	ct=\E[3g	clear_all_tabs
	dc=\E[P		delete_character
	dl=\E[M		delete_line
	do=^J		cursor_down
	eA=\E(B\E)0	ena_acs
	ei=\E[4l	exit_insert_mode
	ho=\E[H		cursor_home
	im=\E[4h	enter_insert_mode
	is=\E)0		init_2string
	le=^H		cursor_left
	mb=\E[5m	enter_blink_mode
	md=\E[1m	enter_bold_mode
	me=\E[m		exit_attrbute_mode
	mr=\E[7m	enter_reverse_mode
	nd=\E[C		cursor_right
	nw=\EE		newline
	rc=\E8		restore_cursor
	rs=\Ec		reset_string
	sc=\E7		save_cursor
	se=\E[23m	exit_standout_mode
	sf=^J		scroll_forward
	so=\E[3m	enter_standout_mode
	sr=\EM		scroll_reverse
	st=\EH		set_tab
	ta=^I		tab
	ue=\E[24m	exit_underline_mode
	up=\EM		cursor_up
	s=\E[4m
	vb=\Eg		flash_screen
	ve=\E[34h\E[?25h
			cursor_normal
	vi=\E[?25l	cursor_invisible
	vs=\E[34l	cursor_visible
	E0=\E(B
	S0=\E(%p1%c
 */

/* Translated escape codes. */
#define CODE_CURSORUP 0
#define CODE_CURSORDOWN 1
#define CODE_CURSORRIGHT 2
#define CODE_CURSORLEFT 3
#define CODE_INSERTCHARACTER 4
#define CODE_DELETECHARACTER 5
#define CODE_INSERTLINE 6
#define CODE_DELETELINE 7
#define CODE_CLEARLINE 8
#define CODE_CLEARSCREEN 9
#define CODE_CLEARENDOFLINE 10
#define CODE_CLEARENDOFSCREEN 11
#define CODE_CLEARSTARTOFLINE 12
#define CODE_CURSORMOVE 13
#define CODE_ATTRIBUTES 14
#define CODE_CURSOROFF 15
#define CODE_CURSORON 16
#define CODE_CURSORUPSCROLL 17
#define CODE_CURSORDOWNSCROLL 18
#define CODE_SCROLLREGION 19
#define CODE_INSERTON 20
#define CODE_INSERTOFF 21
#define CODE_KCURSOROFF 22
#define CODE_KCURSORON 23
#define CODE_KKEYPADOFF 24
#define CODE_KKEYPADON 25
#define CODE_TITLE 26

/* Message codes. */
#define MSG_IDENTIFY 0
#define MSG_CREATE 1
#define MSG_EXIT 2
#define MSG_SIZE 3
#define MSG_NEXT 4
#define MSG_PREVIOUS 5
#define MSG_INPUT 6	/* input from client to server */
#define MSG_OUTPUT 7	/* output from server to client */
#define MSG_REFRESH 8
#define MSG_SELECT 9
#define MSG_SESSIONS 10
#define MSG_WINDOWS 11
#define MSG_PAUSE 12
#define MSG_RENAME 13

struct identify_data {
	char	name[MAXNAMELEN];

	u_int	sx;
	u_int	sy;
};

struct sessions_data {
	u_int	sessions;
};

struct sessions_entry {
	char	name[MAXNAMELEN];
	time_t	tim;
	u_int	windows;
};

struct windows_data {
	char	name[MAXNAMELEN];
	u_int	windows;
};

struct windows_entry {
	u_int	idx;
	char	tty[TTY_NAME_MAX];

	char	name[MAXNAMELEN];
	char	title[MAXTITLELEN];
};

struct size_data {
	u_int	sx;
	u_int	sy;
};

struct select_data {
	u_int	idx;
};

struct refresh_data {
	u_int	py_upper;
	u_int	py_lower;
};

/* Message header structure. */
struct hdr {
	u_int	code;
	size_t	size;
};

/* Attributes. */
#define ATTR_BRIGHT 0x1
#define ATTR_DIM 0x2
#define ATTR_UNDERSCORE 0x4
#define ATTR_BLINK 0x8
#define ATTR_REVERSE 0x10
#define ATTR_HIDDEN 0x20
#define ATTR_ITALICS 0x40

/* Modes. */
#define MODE_CURSOR 0x1
#define MODE_INSERT 0x2
#define MODE_KCURSOR 0x4
#define MODE_KKEYPAD 0x8

/*
 * Virtual screen. This is stored as three blocks of 8-bit values, one for
 * the actual characters, one for attributes and one for colours. Three
 * seperate blocks means memset and friends can be used.
 *
 * Each block is y by x in size, row then column order. Sizes are 0-based.
 */
struct screen {
	char		 title[MAXTITLELEN];

	u_char	       **grid_data;
	u_char	       **grid_attr;
	u_char	       **grid_colr;

	u_int		 sx;		/* size x */
	u_int		 sy;		/* size y */

	u_int		 cx;		/* cursor x */
	u_int		 cy;		/* cursor y */

	u_int		 ry_upper;	/* scroll region top */
	u_int		 ry_lower;	/* scroll region bottom */

	u_char		 attr;
	u_char		 colr;		/* fg:bg */

	int		 mode;
};

/* Window structure. */
struct window {
	char		 name[MAXNAMELEN];

	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	u_int		 references;

	struct screen	 screen;
};
ARRAY_DECL(windows, struct window *);

/* Client session. */
struct session {
	char		 name[MAXNAMELEN];
	time_t		 tim;

	struct window	*window;
	struct windows	 windows;
};
ARRAY_DECL(sessions, struct session *);

/* Client connection. */
struct client {
	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	u_int		 sx;
	u_int		 sy;

	struct session	*session;

	/* User input. */
	const char	*prompt;
	char		*buf;
	size_t		 len;
	size_t		 idx;
	void		 (*callback)(struct client *, const char *);

};
ARRAY_DECL(clients, struct client *);

/* tmux.c */
volatile sig_atomic_t sigterm;
extern int   debug_level;
extern char  socket_path[MAXPATHLEN];

/* server.c */
int	 server_start(void);

/* ansi.c */
void	 input_key(struct buffer *, int);
size_t	 input_parse(u_char *, size_t, struct buffer *, struct screen *);
uint8_t  input_extract8(struct buffer *);
uint16_t input_extract16(struct buffer *);
void	 input_store8(struct buffer *, uint8_t);
void	 input_store16(struct buffer *, uint16_t);
void	 input_store_zero(struct buffer *, u_char);
void	 input_store_one(struct buffer *, u_char, uint16_t);
void	 input_store_two(struct buffer *, u_char, uint16_t, uint16_t);

/* screen.c */
void	 screen_create(struct screen *, u_int, u_int);
void	 screen_resize(struct screen *, u_int, u_int);
void	 screen_draw(struct screen *, struct buffer *, u_int, u_int);
void	 screen_character(struct screen *, u_char);
void 	 screen_sequence(struct screen *, u_char *);

/* local.c */
int	 local_init(struct buffer **, struct buffer **);
void	 local_done(void);
int	 local_key(size_t *);
void	 local_output(struct buffer *, size_t);

/* command.c */
extern int cmd_prefix;
int	 cmd_execute(int, struct buffer *);

/* window.c */
extern struct windows windows;
struct window	*window_create(const char *, u_int, u_int);
int		 window_index(struct windows *, struct window *, u_int *);
void		 window_add(struct windows *, struct window *);
void		 window_remove(struct windows *, struct window *);
void		 window_destroy(struct window *);
struct window	*window_next(struct windows *, struct window *); 
struct window	*window_previous(struct windows *, struct window *); 
struct window	*window_at(struct windows *, u_int); 
int		 window_resize(struct window *, u_int, u_int);
int		 window_poll(struct window *, struct pollfd *);
void		 window_input(struct window *, struct buffer *, size_t);
void		 window_output(struct window *, struct buffer *);

/* session.c */
extern struct sessions sessions;
struct session	*session_find(const char *);
struct session	*session_create(const char *, const char *, u_int, u_int);
void		 session_destroy(struct session *);
int		 session_index(struct session *, u_int *);
int		 session_new(struct session *, const char *, u_int, u_int);
void		 session_attach(struct session *, struct window *);
int		 session_detach(struct session *, struct window *);
int		 session_has(struct session *, struct window *);
int		 session_next(struct session *);
int		 session_previous(struct session *);
int		 session_select(struct session *, u_int);

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

/* buffer-poll.c */
int		 buffer_poll(struct pollfd *, struct buffer *, struct buffer *);

/* log.c */
void		 log_open(FILE *, int, int);
void		 log_close(void);
void		 log_vwrite(FILE *, int, const char *, va_list);
void		 log_write(FILE *, int, const char *, ...);
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
void		*ensure_size(void *, size_t *, size_t, size_t);
void		*ensure_for(void *, size_t *, size_t, size_t);
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
