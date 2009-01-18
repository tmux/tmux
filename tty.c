/* $Id: tty.c,v 1.60 2009-01-18 21:35:09 nicm Exp $ */

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

#include <sys/types.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

void	tty_fill_acs(struct tty *);
u_char	tty_get_acs(struct tty *, u_char);

void	tty_emulate_repeat(
    	    struct tty *, enum tty_code_code, enum tty_code_code, u_int);

void	tty_raw(struct tty *, const char *);

void	tty_reset(struct tty *);
void	tty_region(struct tty *, struct screen *, u_int);
void	tty_attributes(struct tty *, const struct grid_cell *);
void	tty_attributes_fg(struct tty *, const struct grid_cell *);
void	tty_attributes_bg(struct tty *, const struct grid_cell *);

void	tty_cmd_bell(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_carriagereturn(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_cell(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_clearendofline(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_clearendofscreen(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_clearline(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_clearscreen(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_clearstartofline(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_clearstartofscreen(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_cursormode(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_deletecharacter(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_deleteline(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_insertcharacter(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_insertline(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_insertmode(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_kcursormode(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_kkeypadmode(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_linefeed(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_mousemode(struct tty *, struct screen *, u_int, va_list);
void	tty_cmd_reverseindex(struct tty *, struct screen *, u_int, va_list);

void (*tty_cmds[])(struct tty *, struct screen *, u_int, va_list) = {
	tty_cmd_cell,
	tty_cmd_clearendofline,
	tty_cmd_clearendofscreen,
	tty_cmd_clearline,
	tty_cmd_clearscreen,
	tty_cmd_clearstartofline,
	tty_cmd_clearstartofscreen,
	tty_cmd_cursormode,
	tty_cmd_deletecharacter,
	tty_cmd_deleteline,
	tty_cmd_insertcharacter,
	tty_cmd_insertline,
	tty_cmd_insertmode,
	tty_cmd_kcursormode,
	tty_cmd_kkeypadmode,
	tty_cmd_linefeed,
	tty_cmd_mousemode,
	tty_cmd_reverseindex,
};

void
tty_init(struct tty *tty, char *path, char *term)
{
	tty->path = xstrdup(path);
	if (term == NULL)
		tty->termname = xstrdup("unknown");
	else
		tty->termname = xstrdup(term);
	tty->flags = 0;
	tty->term_flags = 0;
}

int
tty_open(struct tty *tty, char **cause)
{
	int		 mode;

	tty->fd = open(tty->path, O_RDWR|O_NONBLOCK);
	if (tty->fd == -1) {
		xasprintf(cause, "%s: %s", tty->path, strerror(errno));
		return (-1);
	}

	if ((mode = fcntl(tty->fd, F_GETFL)) == -1)
		fatal("fcntl");
	if (fcntl(tty->fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl");

	if (debug_level > 3)
		tty->log_fd = open("tmux.out", O_WRONLY|O_CREAT|O_TRUNC, 0644);
	else
		tty->log_fd = -1;

	if ((tty->term = tty_term_find(tty->termname, tty->fd, cause)) == NULL)
		goto error;

	tty->in = buffer_create(BUFSIZ);
	tty->out = buffer_create(BUFSIZ);

	tty->flags &= TTY_UTF8;

	tty_start_tty(tty);

	tty_keys_init(tty);

	tty_fill_acs(tty);

	return (0);

error:
	close(tty->fd);
	tty->fd = -1;

	return (-1);
}

void
tty_start_tty(struct tty *tty)
{
	struct termios	 tio;
#ifdef TIOCFLUSH
	int		 what;
#endif

	if (tcgetattr(tty->fd, &tty->tio) != 0)
		fatal("tcgetattr failed");
	memcpy(&tio, &tty->tio, sizeof tio);
	tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR|IGNCR|IMAXBEL|ISTRIP);
	tio.c_iflag |= IGNBRK;
	tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET);
	tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHONL|ECHOCTL|
	    ECHOPRT|ECHOKE|ECHOCTL|ISIG);
	tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
	if (tcsetattr(tty->fd, TCSANOW, &tio) != 0)
		fatal("tcsetattr failed");

#ifdef TIOCFLUSH
	what = 0;
	if (ioctl(tty->fd, TIOCFLUSH, &what) != 0)
		fatal("ioctl(TIOCFLUSH)");
#endif

	tty_putcode(tty, TTYC_IS1);
	tty_putcode(tty, TTYC_IS2);
	tty_putcode(tty, TTYC_IS3);

	tty_putcode(tty, TTYC_SMCUP);
	tty_putcode(tty, TTYC_SMKX);
	tty_putcode(tty, TTYC_ENACS);
	tty_putcode(tty, TTYC_CLEAR);

	memcpy(&tty->cell, &grid_default_cell, sizeof tty->cell);

	tty->cx = UINT_MAX;
	tty->cy = UINT_MAX;

	tty->rlower = UINT_MAX;
	tty->rupper = UINT_MAX;
}

void
tty_stop_tty(struct tty *tty)
{
	struct winsize	ws;

	/*
	 * Be flexible about error handling and try not kill the server just
	 * because the fd is invalid. Things like ssh -t can easily leave us
	 * with a dead tty.
	 */
	if (ioctl(tty->fd, TIOCGWINSZ, &ws) == -1) {
		if (errno != EBADF && errno != ENXIO && errno != ENOTTY)
			fatal("ioctl(TIOCGWINSZ)");
	} else if (tcsetattr(tty->fd, TCSANOW, &tty->tio) == -1) {
		if (errno != EBADF && errno != ENXIO && errno != ENOTTY)
			fatal("tcsetattr failed");
	} else {
		tty_raw(tty,
		    tty_term_string2(tty->term, TTYC_CSR, 0, ws.ws_row - 1));
		tty_raw(tty, tty_term_string(tty->term, TTYC_RMACS));
		tty_raw(tty, tty_term_string(tty->term, TTYC_SGR0));
		tty_raw(tty, tty_term_string(tty->term, TTYC_CLEAR));
		tty_raw(tty, tty_term_string(tty->term, TTYC_RMKX));
		tty_raw(tty, tty_term_string(tty->term, TTYC_RMCUP));
		tty_raw(tty, tty_term_string(tty->term, TTYC_CNORM));
	}
}

void
tty_fill_acs(struct tty *tty)
{
	const char *ptr;

	memset(tty->acs, 0, sizeof tty->acs);
	if (!tty_term_has(tty->term, TTYC_ACSC))
		return;

	ptr = tty_term_string(tty->term, TTYC_ACSC);
	if (strlen(ptr) % 2 != 0)
		return;
	for (; *ptr != '\0'; ptr += 2)
		tty->acs[(u_char) ptr[0]] = ptr[1];
}

u_char
tty_get_acs(struct tty *tty, u_char ch)
{
	if (tty->acs[ch] != '\0')
		return (tty->acs[ch]);
	return (ch);
}

void
tty_close(struct tty *tty, int no_stop)
{
	if (tty->fd == -1)
		return;

	if (tty->log_fd != -1) {
		close(tty->log_fd);
		tty->log_fd = -1;
	}

	if (!no_stop)
		tty_stop_tty(tty);

	tty_term_free(tty->term);
	tty_keys_free(tty);

	close(tty->fd);
	tty->fd = -1;

	buffer_destroy(tty->in);
	buffer_destroy(tty->out);
}

void
tty_free(struct tty *tty, int no_stop)
{
	tty_close(tty, no_stop);

	if (tty->path != NULL)
		xfree(tty->path);
	if (tty->termname != NULL)
		xfree(tty->termname);
}

void
tty_raw(struct tty *tty, const char *s)
{
	write(tty->fd, s, strlen(s));
}

void
tty_putcode(struct tty *tty, enum tty_code_code code)
{
	tty_puts(tty, tty_term_string(tty->term, code));
}

void
tty_putcode1(struct tty *tty, enum tty_code_code code, int a)
{
	if (a < 0)
		return;
	tty_puts(tty, tty_term_string1(tty->term, code, a));
}

void
tty_putcode2(struct tty *tty, enum tty_code_code code, int a, int b)
{
	if (a < 0 || b < 0)
		return;
	tty_puts(tty, tty_term_string2(tty->term, code, a, b));
}

void
tty_puts(struct tty *tty, const char *s)
{
	if (*s == '\0')
		return;
	buffer_write(tty->out, s, strlen(s));

	if (tty->log_fd != -1)
		write(tty->log_fd, s, strlen(s));
}

void
tty_putc(struct tty *tty, char ch)
{
	if (tty->cell.attr & GRID_ATTR_CHARSET)
		ch = tty_get_acs(tty, ch);
	buffer_write8(tty->out, ch);

	if (ch >= 0x20)
		tty->cx++;	/* This is right most of the time. */

	if (tty->log_fd != -1)
		write(tty->log_fd, &ch, 1);
}

void
tty_set_title(struct tty *tty, const char *title)
{
	if (strstr(tty->termname, "xterm") == NULL &&
	    strstr(tty->termname, "rxvt") == NULL &&
	    strcmp(tty->termname, "screen") != 0)
		return;

	tty_puts(tty, "\033]0;");
	tty_puts(tty, title);
	tty_putc(tty, '\007');
}

void
tty_emulate_repeat(
    struct tty *tty, enum tty_code_code code, enum tty_code_code code1, u_int n)
{
	if (tty_term_has(tty->term, code))
		tty_putcode1(tty, code, n);
	else {
		while (n-- > 0)
			tty_putcode(tty, code1);
	}
}

void
tty_write(struct tty *tty, struct screen *s, u_int oy, enum tty_cmd cmd, ...)
{
	va_list	ap;

	va_start(ap, cmd);
	tty_vwrite(tty, s, oy, cmd, ap);
	va_end(ap);
}

void
tty_vwrite(
    struct tty *tty, struct screen *s, u_int oy, enum tty_cmd cmd, va_list ap)
{
	if (tty->flags & TTY_FREEZE || tty->term == NULL)
		return;
	if (tty_cmds[cmd] != NULL)
		tty_cmds[cmd](tty, s, oy, ap);
}

void
tty_cmd_insertcharacter(
    struct tty *tty, unused struct screen *s, u_int oy, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);
 	tty_cursor(tty, s->cx, s->cy, oy);

	if (tty_term_has(tty->term, TTYC_ICH) || 
	    tty_term_has(tty->term, TTYC_ICH1))
	    tty_emulate_repeat(tty, TTYC_ICH, TTYC_ICH1, ua);
	else {
		tty_putcode(tty, TTYC_SMIR);
		while (ua-- > 0)
			tty_putc(tty, ' ');
		tty_putcode(tty, TTYC_RMIR);
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
	}
}

void
tty_cmd_deletecharacter(
    struct tty *tty, unused struct screen *s, u_int oy, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);
 	tty_cursor(tty, s->cx, s->cy, oy);

	tty_emulate_repeat(tty, TTYC_DCH, TTYC_DCH1, ua);
}

void
tty_cmd_insertline(
    struct tty *tty, unused struct screen *s, u_int oy, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);
 	tty_region(tty, s, oy);
 	tty_cursor(tty, s->cx, s->cy, oy);

	tty_emulate_repeat(tty, TTYC_IL, TTYC_IL1, ua);
}

void
tty_cmd_deleteline(
    struct tty *tty, unused struct screen *s, u_int oy, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);
 	tty_region(tty, s, oy);
 	tty_cursor(tty, s->cx, s->cy, oy);

	tty_emulate_repeat(tty, TTYC_DL, TTYC_DL1, ua);
}

void
tty_cmd_clearline(
    struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	u_int	i;

	tty_reset(tty);
 	tty_cursor(tty, s->cx, s->cy, oy);

	if (tty_term_has(tty->term, TTYC_EL)) {
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, 0);
		tty_putcode(tty, TTYC_EL);
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
	} else {
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, 0);
		for (i = 0; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
	}
}

void
tty_cmd_clearendofline(
    struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	u_int	i;

	tty_reset(tty);
 	tty_cursor(tty, s->cx, s->cy, oy);

	if (tty_term_has(tty->term, TTYC_EL))
		tty_putcode(tty, TTYC_EL);
	else {
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
		for (i = s->cx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
	}
}

void
tty_cmd_clearstartofline(
    struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	u_int	i;

	tty_reset(tty);
 	tty_cursor(tty, s->cx, s->cy, oy);

	if (tty_term_has(tty->term, TTYC_EL1))
		tty_putcode(tty, TTYC_EL1);
	else {
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, 0);
		for (i = 0; i < s->cx + 1; i++)
			tty_putc(tty, ' ');
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
	}
}

void
tty_cmd_cursormode(
    struct tty *tty, unused struct screen *s, unused u_int oy, va_list ap)
{
	int	ua;

	ua = va_arg(ap, int);

	if (tty->cursor == ua)
		return;
	tty->cursor = ua;

	if (ua && !(tty->flags & TTY_NOCURSOR))
		tty_putcode(tty, TTYC_CNORM);
	else
		tty_putcode(tty, TTYC_CIVIS);
}

void
tty_cmd_reverseindex(
    struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	tty_reset(tty);
 	tty_region(tty, s, oy);
	tty_cursor(tty, s->cx, s->cy, oy);

	tty_putcode(tty, TTYC_RI);
}

void
tty_cmd_insertmode(unused struct tty *tty,
    unused struct screen *s, unused u_int oy, va_list ap)
{
	int	ua;

	ua = va_arg(ap, int);

#if 0
	/* XXX */
	if (ua)
		tty_puts(tty, enter_insert_mode);
	else
		tty_puts(tty, exit_insert_mode);
#endif
}

void
tty_cmd_mousemode(
    struct tty *tty, unused struct screen *s, unused u_int oy, va_list ap)
{
	int	ua;

	if (!tty_term_has(tty->term, TTYC_KMOUS))
		return;

	ua = va_arg(ap, int);

	if (ua)
		tty_puts(tty, "\033[?1000h");
	else
		tty_puts(tty, "\033[?1000l");
}

void
tty_cmd_kcursormode(unused struct tty *tty,
    unused struct screen *s, unused u_int oy, unused va_list ap)
{
}

void
tty_cmd_kkeypadmode(unused struct tty *tty,
    unused struct screen *s, unused u_int oy, unused va_list ap)
{
}

void
tty_cmd_linefeed(struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	tty_reset(tty);
 	tty_region(tty, s, oy);
	tty_cursor(tty, s->cx, s->cy, oy);

	tty_putc(tty, '\n');
}

void
tty_cmd_bell(struct tty *tty,
    unused struct screen *s, unused u_int oy, unused va_list ap)
{
	tty_putcode(tty, TTYC_BEL);
}

void
tty_cmd_clearendofscreen(
    struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	u_int	i, j;

	tty_reset(tty);
	tty_cursor(tty, s->cx, s->cy, oy);

	if (tty_term_has(tty->term, TTYC_EL)) {
		for (i = oy + s->cy; i < oy + screen_size_y(s); i++) {
			tty_putcode(tty, TTYC_EL);
			if (i != screen_size_y(s) - 1)
				tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
		}
	} else {
		for (i = s->cx; i < screen_size_y(s); i++)
			tty_putc(tty, ' ');
		for (j = oy + s->cy; j < oy + screen_size_y(s); j++) {
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
}

void
tty_cmd_clearstartofscreen(
    struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	u_int	i, j;

	tty_reset(tty);
	tty_cursor(tty, s->cx, s->cy, oy);

	tty_putcode2(tty, TTYC_CUP, oy, 0);
	if (tty_term_has(tty->term, TTYC_EL)) {
		for (i = 0; i < oy + s->cy; i++) {
			tty_putcode(tty, TTYC_EL);
			tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
		}
		tty_putcode2(tty, TTYC_CUP, oy + s->cy, 0);
	} else {
		for (j = 0; j < oy + s->cy; j++) {
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	for (i = 0; i < s->cx; i++)
		tty_putc(tty, ' ');
	tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
}

void
tty_cmd_clearscreen(
    struct tty *tty, struct screen *s, u_int oy, unused va_list ap)
{
	u_int	i, j;

	tty_reset(tty);
	tty_cursor(tty, s->cx, s->cy, oy);

	tty_putcode2(tty, TTYC_CUP, oy, 0);
	if (tty_term_has(tty->term, TTYC_EL)) {
		for (i = 0; i < screen_size_y(s); i++) {
			tty_putcode(tty, TTYC_EL);
			if (i != screen_size_y(s) - 1)
				tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
		}
	} else {
		for (j = 0; j < screen_size_y(s); j++) {
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	tty_putcode2(tty, TTYC_CUP, oy + s->cy, s->cx);
}

void
tty_cmd_cell(struct tty *tty, struct screen *s, u_int oy, va_list ap)
{
	struct grid_cell       *gc;
	u_int			i, width;
	u_char			out[4];

	tty_cursor(tty, s->cx, s->cy, oy);

	gc = va_arg(ap, struct grid_cell *);

	/* If this is a padding character, do nothing. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;

	/* Handle special characters. Should never come into this function.*/
	if (gc->data < 0x20 || gc->data == 0x7f)
		return;

	/* Set the attributes. */
	tty_attributes(tty, gc);

	/* If not UTF8 multibyte, write directly. */
	if (gc->data <= 0xff) {
		tty_putc(tty, gc->data);
		return;
	}

	/* If the terminal doesn't support UTF8, write _s. */
	if (!(tty->flags & TTY_UTF8)) {
		width = utf8_width(gc->data);
		while (width-- > 0)
			tty_putc(tty, '_');
		return;
	}

	/* Unpack UTF-8 and write it. */
	utf8_split(gc->data, out);
	for (i = 0; i < 4; i++) {
		if (out[i] == 0xff)
			break;
		tty_putc(tty, out[i]);
	}
}

void
tty_reset(struct tty *tty)
{
	struct grid_cell	*tc = &tty->cell;

	tc->data = grid_default_cell.data;
	if (memcmp(tc, &grid_default_cell, sizeof *tc) == 0)
		return;

	if (tty_term_has(tty->term, TTYC_RMACS) && tc->attr & GRID_ATTR_CHARSET)
		tty_putcode(tty, TTYC_RMACS);
	tty_putcode(tty, TTYC_SGR0);
	memcpy(tc, &grid_default_cell, sizeof *tc);
}

void
tty_region(struct tty *tty, struct screen *s, u_int oy)
{
	if (tty->rlower != oy + s->rlower || tty->rupper != oy + s->rupper) {
		tty->rlower = oy + s->rlower;
		tty->rupper = oy + s->rupper;
		tty->cx = 0;
		tty->cy = 0;
		tty_putcode2(tty, TTYC_CSR, tty->rupper, tty->rlower);
	}
}

void
tty_cursor(struct tty *tty, u_int cx, u_int cy, u_int oy)
{
	if (cx == 0 && tty->cx != 0 && tty->cy == oy + cy) {
		tty->cx = 0;
		tty_putc(tty, '\r');
	} else if (tty->cx != cx || tty->cy != oy + cy) {
		tty->cx = cx;
		tty->cy = oy + cy;
		tty_putcode2(tty, TTYC_CUP, tty->cy, tty->cx);
	}
}

void
tty_attributes(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;
	u_char			 changed;

	/* If any bits are being cleared, reset everything. */
	if (tc->attr & ~gc->attr)
		tty_reset(tty);

	/* Filter out attribute bits already set. */
	changed = gc->attr & ~tc->attr;
	tc->attr = gc->attr;

	/* Set the attributes. */
	if (changed & GRID_ATTR_BRIGHT)
		tty_putcode(tty, TTYC_BOLD);
	if (changed & GRID_ATTR_DIM)
		tty_putcode(tty, TTYC_DIM);
	if (changed & GRID_ATTR_ITALICS)
		tty_putcode(tty, TTYC_SMSO);
	if (changed & GRID_ATTR_UNDERSCORE)
		tty_putcode(tty, TTYC_SMUL);
	if (changed & GRID_ATTR_BLINK)
		tty_putcode(tty, TTYC_BLINK);
	if (changed & GRID_ATTR_REVERSE)
		tty_putcode(tty, TTYC_REV);
	if (changed & GRID_ATTR_HIDDEN)
		tty_putcode(tty, TTYC_INVIS);
	if (changed & GRID_ATTR_CHARSET)
		tty_putcode(tty, TTYC_SMACS);

	/* Set foreground colour. */
	if (gc->fg != tc->fg ||
	    (gc->flags & GRID_FLAG_FG256) != (tc->flags & GRID_FLAG_FG256)) {
		tty_attributes_fg(tty, gc);
		tc->fg = gc->fg;
	}

	/* Set background colour. */
	if (gc->bg != tc->bg ||
	    (gc->flags & GRID_FLAG_BG256) != (tc->flags & GRID_FLAG_BG256)) {
		tty_attributes_bg(tty, gc);
		tc->bg = gc->bg;
	}
}

void
tty_attributes_fg(struct tty *tty, const struct grid_cell *gc)
{
	char	s[32];
	u_char	fg = gc->fg;

	if (gc->flags & GRID_FLAG_FG256) {
		if ((tty->term->flags & TERM_256COLOURS) ||
		    (tty->term_flags & TERM_256COLOURS)) {
			xsnprintf(s, sizeof s, "\033[38;5;%hhum", fg);
			tty_puts(tty, s);
			return;
		}
		fg = colour_translate256(fg);
		if (fg & 8) {
			fg &= 7;
			tty_putcode(tty, TTYC_BOLD);
			tty->cell.attr |= GRID_ATTR_BRIGHT;
		} else if (tty->cell.attr & GRID_ATTR_BRIGHT)
			tty_reset(tty);
	}

	if (fg == 8 &&
	    !(tty->term->flags & TERM_HASDEFAULTS) &&
	    !(tty->term_flags & TERM_HASDEFAULTS))
		fg = 7;
	if (fg == 8)
		tty_puts(tty, "\033[39m");
	else
		tty_putcode1(tty, TTYC_SETAF, fg);
}

void
tty_attributes_bg(struct tty *tty, const struct grid_cell *gc)
{
	char	s[32];
	u_char	bg = gc->bg;

	if (gc->flags & GRID_FLAG_BG256) {
		if ((tty->term->flags & TERM_256COLOURS) ||
		    (tty->term_flags & TERM_256COLOURS)) {
			xsnprintf(s, sizeof s, "\033[48;5;%hhum", bg);
			tty_puts(tty, s);
			return;
		}
		bg = colour_translate256(bg);
		if (bg & 8) {
			/*
			 * Bold background; can't do this on standard
			 * terminals...
			 */
#if 0
			xsnprintf(s, sizeof s, "\033[%hhum", 92 + bg);
			tty_puts(tty, s);
			return;
#endif
			bg &= 7;
		}
	}

	if (bg == 8 &&
	    !(tty->term->flags & TERM_HASDEFAULTS) &&
	    !(tty->term_flags & TERM_HASDEFAULTS))
		bg = 0;
	if (bg == 8)
		tty_puts(tty, "\033[49m");
	else
		tty_putcode1(tty, TTYC_SETAB, bg);
}
