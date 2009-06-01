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

#include <sys/types.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

void	tty_fill_acs(struct tty *);

void	tty_raw(struct tty *, const char *);

int	tty_try_256(struct tty *, u_char, const char *);
int	tty_try_88(struct tty *, u_char, const char *);

void	tty_attributes(struct tty *, const struct grid_cell *);
void	tty_attributes_fg(struct tty *, const struct grid_cell *);
void	tty_attributes_bg(struct tty *, const struct grid_cell *);

void	tty_cmd_cell(struct tty *, struct window_pane *, va_list);
void	tty_cmd_clearendofline(struct tty *, struct window_pane *, va_list);
void	tty_cmd_clearendofscreen(struct tty *, struct window_pane *, va_list);
void	tty_cmd_clearline(struct tty *, struct window_pane *, va_list);
void	tty_cmd_clearscreen(struct tty *, struct window_pane *, va_list);
void	tty_cmd_clearstartofline(struct tty *, struct window_pane *, va_list);
void	tty_cmd_clearstartofscreen(struct tty *, struct window_pane *, va_list);
void	tty_cmd_deletecharacter(struct tty *, struct window_pane *, va_list);
void	tty_cmd_deleteline(struct tty *, struct window_pane *, va_list);
void	tty_cmd_insertcharacter(struct tty *, struct window_pane *, va_list);
void	tty_cmd_insertline(struct tty *, struct window_pane *, va_list);
void	tty_cmd_linefeed(struct tty *, struct window_pane *, va_list);
void	tty_cmd_raw(struct tty *, struct window_pane *, va_list);
void	tty_cmd_reverseindex(struct tty *, struct window_pane *, va_list);

void (*tty_cmds[])(struct tty *, struct window_pane *, va_list) = {
	tty_cmd_cell,
	tty_cmd_clearendofline,
	tty_cmd_clearendofscreen,
	tty_cmd_clearline,
	tty_cmd_clearscreen,
	tty_cmd_clearstartofline,
	tty_cmd_clearstartofscreen,
	tty_cmd_deletecharacter,
	tty_cmd_deleteline,
	tty_cmd_insertcharacter,
	tty_cmd_insertline,
	tty_cmd_linefeed,
	tty_cmd_raw,
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
		fatal("fcntl failed");
	if (fcntl(tty->fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failedo");
	if (fcntl(tty->fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");

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
	int		 what;

	tty_detect_utf8(tty);

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

	what = 0;
	if (ioctl(tty->fd, TIOCFLUSH, &what) != 0)
		fatal("ioctl(TIOCFLUSH)");

	tty_putcode(tty, TTYC_IS1);
	tty_putcode(tty, TTYC_IS2);
	tty_putcode(tty, TTYC_IS3);

	tty_putcode(tty, TTYC_SMCUP);
	tty_putcode(tty, TTYC_SMKX);
	tty_putcode(tty, TTYC_ENACS);
	tty_putcode(tty, TTYC_CLEAR);

	tty_putcode(tty, TTYC_CNORM);
	if (tty_term_has(tty->term, TTYC_KMOUS))
		tty_puts(tty, "\033[?1000l");

	memcpy(&tty->cell, &grid_default_cell, sizeof tty->cell);

	tty->cx = UINT_MAX;
	tty->cy = UINT_MAX;

	tty->rlower = UINT_MAX;
	tty->rupper = UINT_MAX;

	tty->mode = MODE_CURSOR;
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
	if (ioctl(tty->fd, TIOCGWINSZ, &ws) == -1)
		return;
	if (tcsetattr(tty->fd, TCSANOW, &tty->tio) == -1)
		return;

	tty_raw(tty, tty_term_string2(tty->term, TTYC_CSR, 0, ws.ws_row - 1));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMACS));
	tty_raw(tty, tty_term_string(tty->term, TTYC_SGR0));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMKX));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMCUP));
	tty_raw(tty, tty_term_string(tty->term, TTYC_CLEAR));

	tty_raw(tty, tty_term_string(tty->term, TTYC_CNORM));
	if (tty_term_has(tty->term, TTYC_KMOUS))
		tty_raw(tty, "\033[?1000l");
}

void
tty_detect_utf8(struct tty *tty)
{
	struct pollfd	pfd;
	char	      	buf[7];
	size_t		len;
	ssize_t		n;
	int		nfds;
	struct termios	tio, old_tio;
	int		 what;

	if (tty->flags & TTY_UTF8)
		return;

	/*
	 * If the terminal looks reasonably likely to support this, try to
	 * write a three-byte UTF-8 wide character to the terminal, then read
	 * the cursor position.
	 *
	 * XXX This entire function is a hack.
	 */

	/* Check if the terminal looks sort of vt100. */
	if (strstr(tty_term_string(tty->term, TTYC_CLEAR), "[2J") == NULL ||
	    strstr(tty_term_string(tty->term, TTYC_CUP), "H") == NULL)
		return;

	if (tcgetattr(tty->fd, &old_tio) != 0)
		fatal("tcgetattr failed");
	cfmakeraw(&tio);
	if (tcsetattr(tty->fd, TCSANOW, &tio) != 0)
		fatal("tcsetattr failed");

	what = 0;
	if (ioctl(tty->fd, TIOCFLUSH, &what) != 0)
		fatal("ioctl(TIOCFLUSH)");

#define UTF8_TEST_DATA "\033[H\357\277\246\033[6n"
	if (write(tty->fd, UTF8_TEST_DATA, (sizeof UTF8_TEST_DATA) - 1) == -1)
		fatal("write failed");
#undef UTF8_TEST_DATA

	len = 0;
	for (;;) {
		pfd.fd = tty->fd;
		pfd.events = POLLIN;

		nfds = poll(&pfd, 1, 500);
		if (nfds == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}
		if (nfds == 0)
			break;
		if (pfd.revents & (POLLERR|POLLNVAL|POLLHUP))
			break;
		if (!(pfd.revents & POLLIN))
			continue;

		if ((n = read(tty->fd, buf + len, 1)) != 1)
			break;
		buf[++len] = '\0';

		if (len == (sizeof buf) - 1) {
			if (strcmp(buf, "\033[1;3R") == 0)
				tty->flags |= TTY_UTF8;
			break;
		}
	}

	if (tcsetattr(tty->fd, TCSANOW, &old_tio) != 0)
		fatal("tcsetattr failed");
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
tty_putc(struct tty *tty, u_char ch)
{
	u_int	sx;

	if (tty->cell.attr & GRID_ATTR_CHARSET)
		ch = tty_get_acs(tty, ch);
	buffer_write8(tty->out, ch);

	if (ch >= 0x20 && ch != 0x7f) {
		sx = tty->sx;
		if (tty->term->flags & TERM_EARLYWRAP)
			sx--;

		if (tty->cx == sx) {
			tty->cx = 0;
			tty->cy++;
		} else
			tty->cx++;
	}

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
tty_update_mode(struct tty *tty, int mode)
{
	int	changed;

	if (tty->flags & TTY_NOCURSOR)
		mode &= ~MODE_CURSOR;

	changed = mode ^ tty->mode;
	if (changed & MODE_CURSOR) {
		if (mode & MODE_CURSOR)
			tty_putcode(tty, TTYC_CNORM);
		else
			tty_putcode(tty, TTYC_CIVIS);
	}
	if (changed & MODE_MOUSE) {
		if (mode & MODE_MOUSE)
			tty_puts(tty, "\033[?1000h");
		else
			tty_puts(tty, "\033[?1000l");
	}
	tty->mode = mode;
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

/*
 * Redraw scroll region using data from screen (already updated). Used when
 * CSR not supported, or window is a pane that doesn't take up the full
 * width of the terminal.
 */
void
tty_redraw_region(struct tty *tty, struct window_pane *wp)
{
	struct screen	*s = wp->screen;
	u_int		 i;

	/*
	 * If region is >= 50% of the screen, just schedule a window redraw. In
	 * most cases, this is likely to be followed by some more scrolling -
	 * without this, the entire pane ends up being redrawn many times which
	 * can be much more data.
	 */
	if (s->old_rupper - s->old_rlower >= screen_size_y(s) / 2) {
		wp->flags |= PANE_REDRAW;
		return;
	}

	if (s->old_cy < s->old_rupper || s->old_cy > s->old_rlower) {
		for (i = s->old_cy; i < screen_size_y(s); i++)
			tty_draw_line(tty, s, i, wp->xoff, wp->yoff);
	} else {
		for (i = s->old_rupper; i <= s->old_rlower; i++)
			tty_draw_line(tty, s, i, wp->xoff, wp->yoff);
	}
}

void
tty_draw_line(struct tty *tty, struct screen *s, u_int py, u_int ox, u_int oy)
{
	const struct grid_cell	*gc;
	const struct grid_utf8	*gu;
	u_int			 i, sx;

	sx = screen_size_x(s);
	if (sx > s->grid->size[s->grid->hsize + py])
		sx = s->grid->size[s->grid->hsize + py];
	if (sx > tty->sx)
		sx = tty->sx;

	tty_cursor(tty, 0, py, ox, oy);
	for (i = 0; i < sx; i++) {
		gc = grid_view_peek_cell(s->grid, i, py);

		gu = NULL;
		if (gc->flags & GRID_FLAG_UTF8)
			gu = grid_view_peek_utf8(s->grid, i, py);

		if (screen_check_selection(s, i, py)) {
			s->sel.cell.data = gc->data;
			tty_cell(tty, &s->sel.cell, gu);
		} else
			tty_cell(tty, gc, gu);
	}

	if (sx >= tty->sx)
		return;
	tty_reset(tty);

	tty_cursor(tty, sx, py, ox, oy);
	if (screen_size_x(s) >= tty->sx && tty_term_has(tty->term, TTYC_EL))
		tty_putcode(tty, TTYC_EL);
	else {
		for (i = sx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
	}
}

void
tty_write(struct tty *tty, struct window_pane *wp, enum tty_cmd cmd, ...)
{
	va_list	ap;

	va_start(ap, cmd);
	tty_vwrite(tty, wp, cmd, ap);
	va_end(ap);
}

void
tty_vwrite(
    struct tty *tty, struct window_pane *wp, enum tty_cmd cmd, va_list ap)
{
	if (tty->flags & TTY_FREEZE || tty->term == NULL)
		return;
	if (tty_cmds[cmd] != NULL)
		tty_cmds[cmd](tty, wp, ap);
}

void
tty_cmd_insertcharacter(struct tty *tty, struct window_pane *wp, va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 ua;

	if (wp->xoff != 0 || screen_size_x(s) < tty->sx) {
		tty_draw_line(tty, wp->screen, s->old_cy, wp->xoff, wp->yoff);
		return;
	}

	ua = va_arg(ap, u_int);

	tty_reset(tty);

 	tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
	if (tty_term_has(tty->term, TTYC_ICH) ||
	    tty_term_has(tty->term, TTYC_ICH1))
		tty_emulate_repeat(tty, TTYC_ICH, TTYC_ICH1, ua);
	else {
		tty_putcode(tty, TTYC_SMIR);
		while (ua-- > 0)
			tty_putc(tty, ' ');
		tty_putcode(tty, TTYC_RMIR);
	}
}

void
tty_cmd_deletecharacter(struct tty *tty, struct window_pane *wp, va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 ua;

	if (wp->xoff != 0 || screen_size_x(s) < tty->sx) {
		tty_draw_line(tty, wp->screen, s->old_cy, wp->xoff, wp->yoff);
		return;
	}

	ua = va_arg(ap, u_int);

	tty_reset(tty);

 	tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
	tty_emulate_repeat(tty, TTYC_DCH, TTYC_DCH1, ua);
}

void
tty_cmd_insertline(struct tty *tty, struct window_pane *wp, va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 ua;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR)) {
		tty_redraw_region(tty, wp);
		return;
	}

	ua = va_arg(ap, u_int);

	tty_reset(tty);

 	tty_region(tty, s->old_rupper, s->old_rlower, wp->yoff);

 	tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
	tty_emulate_repeat(tty, TTYC_IL, TTYC_IL1, ua);
}

void
tty_cmd_deleteline(struct tty *tty, struct window_pane *wp, va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 ua;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR)) {
		tty_redraw_region(tty, wp);
		return;
	}

	ua = va_arg(ap, u_int);

	tty_reset(tty);

 	tty_region(tty, s->old_rupper, s->old_rlower, wp->yoff);

 	tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
	tty_emulate_repeat(tty, TTYC_DL, TTYC_DL1, ua);
}

void
tty_cmd_clearline(struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 i;

	tty_reset(tty);

 	tty_cursor(tty, 0, s->old_cy, wp->xoff, wp->yoff);
	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL)) {
		tty_putcode(tty, TTYC_EL);
	} else {
		for (i = 0; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
	}
}

void
tty_cmd_clearendofline(
    struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 i;

	tty_reset(tty);

	tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL))
		tty_putcode(tty, TTYC_EL);
	else {
		for (i = s->old_cx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
	}
}

void
tty_cmd_clearstartofline(
    struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 i;

	tty_reset(tty);

	if (wp->xoff == 0 && tty_term_has(tty->term, TTYC_EL1)) {
		tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
		tty_putcode(tty, TTYC_EL1);
	} else {
		tty_cursor(tty, 0, s->old_cy, wp->xoff, wp->yoff);
		for (i = 0; i < s->old_cx + 1; i++)
			tty_putc(tty, ' ');
	}
}

void
tty_cmd_reverseindex(struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR)) {
		tty_redraw_region(tty, wp);
		return;
	}

	tty_reset(tty);

 	tty_region(tty, s->old_rupper, s->old_rlower, wp->yoff);

	if (s->old_cy == s->old_rupper) {
		tty_cursor(tty, s->old_cx, s->old_rupper, wp->xoff, wp->yoff);
		tty_putcode(tty, TTYC_RI);
	}
}

void
tty_cmd_linefeed(struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR)) {
		tty_redraw_region(tty, wp);
		return;
	}

	tty_reset(tty);

 	tty_region(tty, s->old_rupper, s->old_rlower, wp->yoff);

	if (s->old_cy == s->old_rlower) {
		tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
		tty_putc(tty, '\n');
	}
}

void
tty_cmd_clearendofscreen(
    struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 i, j, oy;

	oy = wp->yoff;

	tty_reset(tty);

	tty_region(tty, 0, screen_size_y(s) - 1, wp->yoff);
	tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);
	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL)) {
		tty_putcode(tty, TTYC_EL);
		if (s->old_cy != screen_size_y(s) - 1) {
			tty_cursor(tty, 0, s->old_cy + 1, wp->xoff, wp->yoff);
			for (i = s->old_cy + 1; i < screen_size_y(s); i++) {
				tty_putcode(tty, TTYC_EL);
				if (i == screen_size_y(s) - 1)
					continue;
				tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
				tty->cy++;
			}
		}
	} else {
		for (i = s->old_cx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
		for (j = s->old_cy; j < screen_size_y(s); j++) {
			tty_cursor(tty, 0, j, wp->xoff, wp->yoff);
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
}

void
tty_cmd_clearstartofscreen(
    struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 i, j;

	tty_reset(tty);

	tty_region(tty, 0, screen_size_y(s) - 1, wp->yoff);
	tty_cursor(tty, 0, 0, wp->xoff, wp->yoff);
	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL)) {
		for (i = 0; i < s->old_cy; i++) {
			tty_putcode(tty, TTYC_EL);
			tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
			tty->cy++;
		}
	} else {
		for (j = 0; j < s->old_cy; j++) {
			tty_cursor(tty, 0, j, wp->xoff, wp->yoff);
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	for (i = 0; i < s->old_cx; i++)
		tty_putc(tty, ' ');
}

void
tty_cmd_clearscreen(
    struct tty *tty, struct window_pane *wp, unused va_list ap)
{
	struct screen	*s = wp->screen;
	u_int		 i, j;

	tty_reset(tty);

	tty_region(tty, 0, screen_size_y(s) - 1, wp->yoff);
	tty_cursor(tty, 0, 0, wp->xoff, wp->yoff);
	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL)) {
		for (i = 0; i < screen_size_y(s); i++) {
			tty_putcode(tty, TTYC_EL);
			if (i != screen_size_y(s) - 1) {
				tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
				tty->cy++;
			}
		}
	} else {
		for (j = 0; j < screen_size_y(s); j++) {
			tty_cursor(tty, 0, j, wp->xoff, wp->yoff);
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
}

void
tty_cmd_cell(struct tty *tty, struct window_pane *wp, va_list ap)
{
	struct screen		*s = wp->screen;
	struct grid_cell	*gc;
	struct grid_utf8	*gu;

	gc = va_arg(ap, struct grid_cell *);
	gu = va_arg(ap, struct grid_utf8 *);

	tty_cursor(tty, s->old_cx, s->old_cy, wp->xoff, wp->yoff);

	tty_cell(tty, gc, gu);
}

void
tty_cmd_raw(struct tty *tty, unused struct window_pane *wp, va_list ap)
{
	u_char	*buf;
	size_t	 i, len;

	buf = va_arg(ap, u_char *);
	len = va_arg(ap, size_t);

	for (i = 0; i < len; i++)
		tty_putc(tty, buf[i]);
}

void
tty_cell(
    struct tty *tty, const struct grid_cell *gc, const struct grid_utf8 *gu)
{
	u_int	i;

	/* Skip last character if terminal is stupid. */
	if (tty->term->flags & TERM_EARLYWRAP &&
	    tty->cy == tty->sy - 1 && tty->cx == tty->sx - 1)
		return;

	/* If this is a padding character, do nothing. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;

	/* Set the attributes. */
	tty_attributes(tty, gc);

	/* If not UTF-8, write directly. */
	if (!(gc->flags & GRID_FLAG_UTF8)) {
		if (gc->data < 0x20 || gc->data == 0x7f)
			return;
		tty_putc(tty, gc->data);
		return;
	}

	/* If the terminal doesn't support UTF-8, write underscores. */
	if (!(tty->flags & TTY_UTF8)) {
		for (i = 0; i < gu->width; i++)
			tty_putc(tty, '_');
		return;
	}

	/* Otherwise, write UTF-8. */
	for (i = 0; i < UTF8_SIZE; i++) {
		if (gu->data[i] == 0xff)
			break;
		tty_putc(tty, gu->data[i]);
	}
}

void
tty_reset(struct tty *tty)
{
	struct grid_cell	*gc = &tty->cell;

	if (memcmp(gc, &grid_default_cell, sizeof *gc) == 0)
		return;

	if (tty_term_has(tty->term, TTYC_RMACS) && gc->attr & GRID_ATTR_CHARSET)
		tty_putcode(tty, TTYC_RMACS);
	tty_putcode(tty, TTYC_SGR0);
	memcpy(gc, &grid_default_cell, sizeof *gc);
}

void
tty_region(struct tty *tty, u_int rupper, u_int rlower, u_int oy)
{
	if (!tty_term_has(tty->term, TTYC_CSR))
		return;
	if (tty->rlower != oy + rlower || tty->rupper != oy + rupper) {
		tty->rlower = oy + rlower;
		tty->rupper = oy + rupper;
		tty->cx = 0;
	 	tty->cy = 0;
		tty_putcode2(tty, TTYC_CSR, tty->rupper, tty->rlower);
	}
}

void
tty_cursor(struct tty *tty, u_int cx, u_int cy, u_int ox, u_int oy)
{
	if (ox + cx == 0 && tty->cx != 0 && tty->cy == oy + cy) {
		tty->cx = 0;
		tty_putc(tty, '\r');
	} else if (tty->cx != ox + cx || tty->cy != oy + cy) {
		tty->cx = ox + cx;
		tty->cy = oy + cy;
		tty_putcode2(tty, TTYC_CUP, tty->cy, tty->cx);
	}
}

void
tty_attributes(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;
	u_char			 changed;
	u_int			 fg, bg;

	/* If any bits are being cleared, reset everything. */
	if (tc->attr & ~gc->attr)
		tty_reset(tty);

	/* Filter out attribute bits already set. */
	changed = gc->attr & ~tc->attr;
	tc->attr = gc->attr;

	/* Set the attributes. */
	fg = gc->fg;
	bg = gc->bg;
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
	if (changed & GRID_ATTR_REVERSE) {
		if (tty_term_has(tty->term, TTYC_REV))
			tty_putcode(tty, TTYC_REV);
		else if (tty_term_has(tty->term, TTYC_SMSO))
			tty_putcode(tty, TTYC_SMSO);
	}
	if (changed & GRID_ATTR_HIDDEN)
		tty_putcode(tty, TTYC_INVIS);
	if (changed & GRID_ATTR_CHARSET)
		tty_putcode(tty, TTYC_SMACS);

	/* Set foreground colour. */
	if (fg != tc->fg ||
	    (gc->flags & GRID_FLAG_FG256) != (tc->flags & GRID_FLAG_FG256)) {
		tty_attributes_fg(tty, gc);
		tc->fg = fg;
	}

	/* Set background colour. */
	if (bg != tc->bg ||
	    (gc->flags & GRID_FLAG_BG256) != (tc->flags & GRID_FLAG_BG256)) {
		tty_attributes_bg(tty, gc);
		tc->bg = bg;
	}
}

int
tty_try_256(struct tty *tty, u_char colour, const char *type)
{
	char	s[32];

	if (!(tty->term->flags & TERM_256COLOURS) &&
	    !(tty->term_flags & TERM_256COLOURS))
		return (-1);

	xsnprintf(s, sizeof s, "\033[%s;5;%hhum", type, colour);
	tty_puts(tty, s);
	return (0);
}

int
tty_try_88(struct tty *tty, u_char colour, const char *type)
{
	char	s[32];

	if (!(tty->term->flags & TERM_88COLOURS) &&
	    !(tty->term_flags & TERM_88COLOURS))
		return (-1);
	colour = colour_256to88(colour);

	xsnprintf(s, sizeof s, "\033[%s;5;%hhum", type, colour);
	tty_puts(tty, s);
	return (0);
}

void
tty_attributes_fg(struct tty *tty, const struct grid_cell *gc)
{
	u_char	fg;

	fg = gc->fg;
	if (gc->flags & GRID_FLAG_FG256) {
		if (tty_try_256(tty, fg, "38") == 0)
			return;
		if (tty_try_88(tty, fg, "38") == 0)
			return;
		fg = colour_256to16(fg);
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
	u_char	bg;

	bg = gc->bg;
	if (gc->flags & GRID_FLAG_BG256) {
		if (tty_try_256(tty, bg, "48") == 0)
			return;
		if (tty_try_88(tty, bg, "48") == 0)
			return;
		bg = colour_256to16(bg);
		if (bg & 8)
			bg &= 7;
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
