/* $Id: tmux.h,v 1.118 2008-06-02 21:08:36 nicm Exp $ */

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

/* Shut up gcc warnings about empty if bodies. */
#define RB_AUGMENT(x) do {} while (0)

#include <sys/param.h>

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

#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <term.h>

#include "array.h"

extern char    *__progname;

#ifndef INFTIM
#define INFTIM -1
#endif

#ifndef __dead
#define __dead __attribute__ ((__noreturn__))
#endif
#ifndef __packed
#define __packed __attribute__ ((__packed__))
#endif

#ifndef TTY_NAME_MAX
#define TTY_NAME_MAX 32
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
/* XXX #define KEYC_BACKSPACE -4 */
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
#define TTY_KCURSOROFF 20
#define TTY_KCURSORON 21
#define TTY_KKEYPADOFF 22
#define TTY_KKEYPADON 23
#define TTY_TITLE 24
#define TTY_MOUSEON 25
#define TTY_MOUSEOFF 26 /* XXX merge allon/off into 1 arg? */

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
	long long	pid;			/* pid from $TMUX or -1 */
	u_int		idx;			/* index from $TMUX */

	size_t		namelen;
};

struct msg_identify_data {
	char		tty[TTY_NAME_MAX];

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

/* Screen default contents. */
#define SCREEN_DEFDATA ' '
#define SCREEN_DEFATTR 0
#define SCREEN_DEFCOLR 0x70	/* white on black */

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
struct window_mode {
	struct screen *(*init)(struct window *);
	void	(*free)(struct window *);
	void	(*resize)(struct window *, u_int, u_int);
	void	(*key)(struct window *, int);
};

/* Window structure. */
struct window {
	char		*name;

	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	struct input_ctx ictx;

	int		 flags;
#define WINDOW_BELL 0x1
#define WINDOW_HIDDEN 0x2

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

/* Client session. */
struct session {
	char		*name;
	time_t		 tim;

	u_int		 sx;
	u_int		 sy;

	struct winlink	*curw;
	struct winlink	*lastw;
	struct winlinks	 windows;

	ARRAY_DECL(, struct winlink *) bells;	/* windows with bells */

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

	size_t		 ksize;	/* maximum key size */
	RB_HEAD(tty_keys, tty_key) ktree;
};

/* Client connection. */
struct client {
	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	struct tty 	 tty;

	u_int		 sx;
	u_int		 sy;

#define CLIENT_TERMINAL 0x1
#define CLIENT_PREFIX 0x2
#define CLIENT_MOUSE 0x4
	int		 flags;

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
	void		(*error)(struct cmd_ctx *, const char *, ...);

#define CMD_KEY 0x1
	int		flags;
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
	int		 flags;

	int		 (*parse)(struct cmd *, void **, int, char **, char **);
	void		 (*exec)(void *, struct cmd_ctx *);
	void		 (*send)(void *, struct buffer *);
	void	         (*recv)(void **, struct buffer *);
	void		 (*free)(void *);
};

/* Key binding. */
struct binding {
	int		 key;
	struct cmd	*cmd;
};
ARRAY_DECL(bindings, struct binding *);

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

/* tmux.c */
extern volatile sig_atomic_t sigwinch;
extern volatile sig_atomic_t sigterm;
#define BELL_NONE 0
#define BELL_ANY 1
#define BELL_CURRENT 2
extern char	*default_command;
extern char	*cfg_file;
extern char	*paste_buffer;
extern int	 bell_action;
extern int	 debug_level;
extern int	 prefix_key;
extern u_char	 status_colour;
extern u_int	 history_limit;
extern u_int	 status_lines;
void		 logfile(const char *);
void		 siginit(void);
void		 sigreset(void);

/* cfg.c */
int		 load_cfg(const char *, char **x);

/* tty.c */
void		 tty_init(struct tty *, char *, char *);
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

/* cmd.c */
struct cmd	*cmd_parse(int, char **, char **);
void		 cmd_exec(struct cmd *, struct cmd_ctx *);
void		 cmd_send(struct cmd *, struct buffer *);
struct cmd	*cmd_recv(struct buffer *);
void		 cmd_free(struct cmd *);
void		 cmd_send_string(struct buffer *, const char *);
char		*cmd_recv_string(struct buffer *);
struct session	*cmd_find_session(struct cmd_ctx *, const char *);
struct client	*cmd_find_client(struct cmd_ctx *, const char *);
extern const struct cmd_entry cmd_attach_session_entry;
extern const struct cmd_entry cmd_bind_key_entry;
extern const struct cmd_entry cmd_copy_mode_entry;
extern const struct cmd_entry cmd_detach_client_entry;
extern const struct cmd_entry cmd_has_session_entry;
extern const struct cmd_entry cmd_kill_session_entry;
extern const struct cmd_entry cmd_kill_window_entry;
extern const struct cmd_entry cmd_last_window_entry;
extern const struct cmd_entry cmd_link_window_entry;
extern const struct cmd_entry cmd_list_clients_entry;
extern const struct cmd_entry cmd_list_keys_entry;
extern const struct cmd_entry cmd_list_sessions_entry;
extern const struct cmd_entry cmd_list_windows_entry;
extern const struct cmd_entry cmd_new_session_entry;
extern const struct cmd_entry cmd_new_window_entry;
extern const struct cmd_entry cmd_next_window_entry;
extern const struct cmd_entry cmd_paste_buffer_entry;
extern const struct cmd_entry cmd_previous_window_entry;
extern const struct cmd_entry cmd_refresh_client_entry;
extern const struct cmd_entry cmd_rename_session_entry;
extern const struct cmd_entry cmd_rename_window_entry;
extern const struct cmd_entry cmd_scroll_mode_entry;
extern const struct cmd_entry cmd_select_window_entry;
extern const struct cmd_entry cmd_send_keys_entry;
extern const struct cmd_entry cmd_send_prefix_entry;
extern const struct cmd_entry cmd_set_option_entry;
extern const struct cmd_entry cmd_start_server_entry;
extern const struct cmd_entry cmd_swap_window_entry;
extern const struct cmd_entry cmd_switch_client_entry;
extern const struct cmd_entry cmd_unbind_key_entry;
extern const struct cmd_entry cmd_unlink_window_entry;
void	cmd_select_window_default(void **, int);

/* cmd-generic.c */
#define CMD_CLIENTONLY_USAGE "[-c client-name]"
int	cmd_clientonly_parse(struct cmd *, void **, int, char **, char **);
void	cmd_clientonly_exec(void *, struct cmd_ctx *);
void	cmd_clientonly_send(void *, struct buffer *);
void	cmd_clientonly_recv(void **, struct buffer *);
void	cmd_clientonly_free(void *);
struct client *cmd_clientonly_get(void *, struct cmd_ctx *);
#define CMD_SESSIONONLY_USAGE "[-s session-name]"
int	cmd_sessiononly_parse(struct cmd *, void **, int, char **, char **);
void	cmd_sessiononly_exec(void *, struct cmd_ctx *);
void	cmd_sessiononly_send(void *, struct buffer *);
void	cmd_sessiononly_recv(void **, struct buffer *);
void	cmd_sessiononly_free(void *);
struct session *cmd_sessiononly_get(void *, struct cmd_ctx *);

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

/* key-string.c */
int	 key_string_lookup_string(const char *);
const char *key_string_lookup_key(int);

/* server.c */
extern struct clients clients;
pid_t	 server_start(const char *);

/* server-msg.c */
int	 server_msg_dispatch(struct client *);

/* server-fn.c */
struct session *server_extract_session(
    	     struct msg_command_data *, char *, char **);
void	 server_write_client(
             struct client *, enum hdrtype, const void *, size_t);
void	 server_write_session(
             struct session *, enum hdrtype, const void *, size_t);
void	 server_write_window(
             struct window *, enum hdrtype, const void *, size_t);
void	 server_clear_client(struct client *);
void	 server_redraw_client(struct client *);
void	 server_status_client(struct client *);
void	 server_clear_session(struct session *);
void	 server_redraw_session(struct session *);
void	 server_status_session(struct session *);
void	 server_clear_window(struct window *);
void	 server_redraw_window(struct window *);
void	 server_status_window(struct window *);
void printflike2 server_write_message(struct client *, const char *, ...);

/* status.c */
void	 status_write_client(struct client *);
void	 status_write_session(struct session *);
void	 status_write_window(struct window *);

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
void	screen_redraw_clear_screen(struct screen_redraw_ctx *);
void	screen_redraw_cell(struct screen_redraw_ctx *, u_int, u_int);
void	screen_redraw_area(
    	    struct screen_redraw_ctx *, u_int, u_int, u_int, u_int);
void	screen_redraw_lines(struct screen_redraw_ctx *, u_int, u_int);

/* screen.c */
const char *screen_colourstring(u_char);
u_char	 screen_stringcolour(const char *);
void	 screen_create(struct screen *, u_int, u_int);
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
struct window	*window_create(
    		     const char *, const char *, const char **, u_int, u_int);
void		 window_destroy(struct window *);
int		 window_resize(struct window *, u_int, u_int);
int		 window_set_mode(struct window *, const struct window_mode *);
void		 window_reset_mode(struct window *);
void		 window_parse(struct window *);
void		 window_key(struct window *, int);

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
void		 session_cancelbell(struct session *, struct winlink *);
void		 session_addbell(struct session *, struct window *);
int		 session_hasbell(struct session *, struct winlink *);
struct session	*session_find(const char *);
struct session	*session_create(const char *, const char *, u_int, u_int);
void		 session_destroy(struct session *);
int		 session_index(struct session *, u_int *);
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
char		*xmemstrdup(const char *, size_t);
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
