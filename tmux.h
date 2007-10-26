/* $Id: tmux.h,v 1.73 2007-10-26 16:57:32 nicm Exp $ */

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
#include <sys/tree.h>
#include <sys/queue.h>

#include <poll.h>
#include <stdarg.h>
#include <stdio.h>

#include "array.h"

extern char    *__progname;

#ifndef __dead
#define __dead __attribute__ ((__noreturn__))
#endif
#ifndef __packed
#define __packed __attribute__ ((__packed__))
#endif

#ifndef TTY_NAME_MAX
#define TTY_NAME_MAX 32
#endif

#define MAXTITLELEN	192

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
/* 9 unused */
#define CODE_CLEARENDOFLINE 10
/* 11 unused */
#define CODE_CLEARSTARTOFLINE 12
#define CODE_CURSORMOVE 13
#define CODE_ATTRIBUTES 14
#define CODE_CURSOROFF 15
#define CODE_CURSORON 16
#define CODE_REVERSEINDEX 17
/* 18 unused */
#define CODE_SCROLLREGION 19
#define CODE_INSERTON 20
#define CODE_INSERTOFF 21
#define CODE_KCURSOROFF 22
#define CODE_KCURSORON 23
#define CODE_KKEYPADOFF 24
#define CODE_KKEYPADON 25
#define CODE_TITLE 26

/* Message codes. */
enum hdrtype {
	MSG_COMMAND,
	MSG_ERROR,
	MSG_PRINT,
	MSG_EXIT,
	MSG_IDENTIFY,
	MSG_READY,
	MSG_DETACH,
	MSG_RESIZE,
	MSG_DATA,
	MSG_KEYS,
	MSG_PAUSE,
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

/* Modes. */
#define MODE_CURSOR 0x1
#define MODE_INSERT 0x2
#define MODE_KCURSOR 0x4
#define MODE_KKEYPAD 0x8
#define MODE_SAVED 0x10

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

	u_int		 saved_cx;
	u_int		 saved_cy;
	u_int		 saved_ry_upper;
	u_int		 saved_ry_lower;
	u_int		 saved_attr;
	u_int		 saved_colr;
	
	int		 mode;
};

/* Screen default contents. */
#define SCREEN_DEFDATA ' '
#define SCREEN_DEFATTR 0
#define SCREEN_DEFCOLR 0x88
 
/* Input parser sequence argument. */
struct input_arg {
	u_char		 data[64];
	size_t		 used;
};

/* Input parser context. */
struct input_ctx {
	struct window	*w;
	struct buffer	*b;

	u_char		*buf;
	size_t		 len;
	size_t		 off;

	u_char		 title_buf[MAXTITLELEN];
	size_t		 title_len;
	u_int		 title_type;

	void 		*(*state)(u_char, struct input_ctx *);

	u_char		 private;
	ARRAY_DECL(, struct input_arg) args;
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

	struct screen	 screen;

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

	struct winlink *curw;
	struct winlink *lastw;
	struct winlinks windows;

	ARRAY_DECL(, struct winlink *) bells;	/* windows with bells */

#define SESSION_UNATTACHED 0x1	/* not attached to any clients */
	int		 flags;
};
ARRAY_DECL(sessions, struct session *);

/* Client connection. */
struct client {
	char		*tty;

	int		 fd;
	struct buffer	*in;
	struct buffer	*out;

	u_int		 sx;
	u_int		 sy;

#define CLIENT_TERMINAL 0x1
#define CLIENT_PREFIX 0x2
#define CLIENT_HOLD 0x4
	int		 flags;

	struct session	*session;
};
ARRAY_DECL(clients, struct client *);

/* Client context. */
struct client_ctx {
	int		 srv_fd;
	struct buffer	*srv_in;
	struct buffer	*srv_out;

	int		 loc_fd;
	struct buffer	*loc_in;
	struct buffer	*loc_out;

#define CCTX_PAUSE 0x1
#define CCTX_DETACH 0x2
#define CCTX_EXIT 0x4
	int 		 flags;
};

/* Key/command line command. */
struct cmd_ctx {
	struct client  *client;
	struct session *session;

	void		(*print)(struct cmd_ctx *, const char *, ...);
	void		(*error)(struct cmd_ctx *, const char *, ...);

#define CMD_KEY 0x1
	int		flags;
};

struct cmd_entry {
	const char	*name;
	const char	*alias;
	const char	*usage;

#define CMD_STARTSERVER 0x1
#define CMD_NOSESSION 0x2
#define CMD_CANTNEST 0x4
	int		 flags;

	int		 (*parse)(void **, int, char **, char **);
	void		 (*exec)(void *, struct cmd_ctx *);
	void		 (*send)(void *, struct buffer *);
	void	         (*recv)(void **, struct buffer *);
	void		 (*free)(void *);
};

struct cmd {
	const struct cmd_entry *entry;
	void	       	*data;
};

/* Key binding. */
struct binding {
	int		 key;
	struct cmd	*cmd;
};
ARRAY_DECL(bindings, struct binding *);

/* tmux.c */
extern volatile sig_atomic_t sigwinch;
extern volatile sig_atomic_t sigterm;
#define BELL_NONE 0
#define BELL_ANY 1
#define BELL_CURRENT 2
extern int	 bell_action;
extern int	 prefix_key;
extern int	 debug_level;
extern u_int	 status_lines;
extern u_char	 status_colour;
extern char     *default_command;
void		 usage(char **, const char *, ...);
void		 logfile(const char *);
void		 siginit(void);
void		 sigreset(void);

/* cmd.c */
struct cmd	*cmd_parse(int, char **, char **);
void		 cmd_exec(struct cmd *, struct cmd_ctx *);
void		 cmd_send(struct cmd *, struct buffer *);
struct cmd	*cmd_recv(struct buffer *);
void		 cmd_free(struct cmd *);
void		 cmd_send_string(struct buffer *, const char *);
char		*cmd_recv_string(struct buffer *);
extern const struct cmd_entry cmd_attach_session_entry;
extern const struct cmd_entry cmd_bind_key_entry;
extern const struct cmd_entry cmd_detach_session_entry;
extern const struct cmd_entry cmd_has_session_entry;
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
extern const struct cmd_entry cmd_previous_window_entry;
extern const struct cmd_entry cmd_refresh_session_entry;
extern const struct cmd_entry cmd_rename_window_entry;
extern const struct cmd_entry cmd_select_window_entry;
extern const struct cmd_entry cmd_send_prefix_entry;
extern const struct cmd_entry cmd_set_option_entry;
extern const struct cmd_entry cmd_unbind_key_entry;
extern const struct cmd_entry cmd_unlink_window_entry;
void	cmd_select_window_default(void **, int);

/* client.c */
int	 client_init(char *, struct client_ctx *, int);
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
int	 server_start(char *);

/* server-msg.c */
int	 server_msg_dispatch(struct client *);

/* server-fn.c */
struct session *server_extract_session(
    	     struct msg_command_data *, char *, char **);
void	 server_write_client(
             struct client *, enum hdrtype, const void *, size_t);
void	 server_write_session(
             struct session *, enum hdrtype, const void *, size_t);
void	 server_write_window_cur(
             struct window *, enum hdrtype, const void *, size_t);
void	 server_write_window_all(
             struct window *, enum hdrtype, const void *, size_t);
void	 server_status_client(struct client *);
void	 server_clear_client(struct client *);
void	 server_redraw_client(struct client *);
void	 server_status_session(struct session *);
void	 server_redraw_session(struct session *);
void	 server_status_window_cur(struct window *);
void	 server_status_window_all(struct window *);
void	 server_clear_window_cur(struct window *);
void	 server_clear_window_all(struct window *);
void	 server_redraw_window_cur(struct window *);
void	 server_redraw_window_all(struct window *);
void	 server_write_message(struct client *, const char *, ...);

/* status.c */
void	 status_write(struct client *c);

/* resize.c */
void	 recalculate_sizes(void);

/* input.c */
void	 input_init(struct window *);
void	 input_free(struct window *);
void	 input_parse(struct window *, struct buffer *);
uint8_t  input_extract8(struct buffer *);
uint16_t input_extract16(struct buffer *);
void	 input_store8(struct buffer *, uint8_t);
void	 input_store16(struct buffer *, uint16_t);
void	 input_store_zero(struct buffer *, u_char);
void	 input_store_one(struct buffer *, u_char, uint16_t);
void	 input_store_two(struct buffer *, u_char, uint16_t, uint16_t);

/* input-key.c */
void	 input_translate_key(struct buffer *, int);

/* screen.c */
const char *screen_colourstring(u_char);
u_char	 screen_stringcolour(const char *);
void	 screen_create(struct screen *, u_int, u_int);
void	 screen_destroy(struct screen *);
void	 screen_resize(struct screen *, u_int, u_int);
void	 screen_draw(struct screen *, struct buffer *, u_int, u_int);
void	 screen_write_character(struct screen *, u_char);
void	 screen_insert_lines(struct screen *, u_int, u_int);
void	 screen_insert_lines_region(struct screen *, u_int, u_int);
void	 screen_delete_lines(struct screen *, u_int, u_int);
void	 screen_delete_lines_region(struct screen *, u_int, u_int);
void	 screen_insert_characters(struct screen *, u_int, u_int, u_int);
void	 screen_delete_characters(struct screen *, u_int, u_int, u_int);
void	 screen_cursor_up_scroll(struct screen *);
void	 screen_cursor_down_scroll(struct screen *);
void	 screen_scroll_region_up(struct screen *);
void	 screen_scroll_region_down(struct screen *);
void	 screen_scroll_up(struct screen *, u_int);
void	 screen_scroll_down(struct screen *, u_int);
void	 screen_fill_screen(struct screen *, u_char, u_char, u_char);
void	 screen_fill_line(struct screen *, u_int, u_char, u_char, u_char);
void	 screen_fill_end_of_screen(
    	     struct screen *, u_int, u_int, u_char, u_char, u_char);
void	 screen_fill_end_of_line(
    	     struct screen *, u_int, u_int, u_char, u_char, u_char);
void	 screen_fill_start_of_line(
    	     struct screen *, u_int, u_int, u_char, u_char, u_char);

/* local.c */
int	 local_init(struct buffer **, struct buffer **);
void	 local_done(void);
int	 local_key(void);
void	 local_output(struct buffer *, size_t);

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
