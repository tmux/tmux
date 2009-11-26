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
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

void	tty_read_callback(struct bufferevent *, void *);
void	tty_error_callback(struct bufferevent *, short, void *);

void	tty_fill_acs(struct tty *);

int	tty_try_256(struct tty *, u_char, const char *);
int	tty_try_88(struct tty *, u_char, const char *);

void	tty_colours(struct tty *, const struct grid_cell *, int *);
void	tty_colours_fg(struct tty *, const struct grid_cell *, int *);
void	tty_colours_bg(struct tty *, const struct grid_cell *, int *);

void	tty_redraw_region(struct tty *, const struct tty_ctx *);
void	tty_emulate_repeat(
	    struct tty *, enum tty_code_code, enum tty_code_code, u_int);
void	tty_cell(struct tty *,
    	    const struct grid_cell *, const struct grid_utf8 *);

void
tty_init(struct tty *tty, int fd, char *term)
{
	char	*path;

	memset(tty, 0, sizeof *tty);
	tty->log_fd = -1;

	if (term == NULL || *term == '\0')
		tty->termname = xstrdup("unknown");
	else
		tty->termname = xstrdup(term);

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");
	tty->fd = fd;

	if ((path = ttyname(fd)) == NULL)
		fatalx("ttyname failed");
	tty->path = xstrdup(path);

	tty->flags = 0;
	tty->term_flags = 0;
}

void
tty_resize(struct tty *tty)
{
	struct winsize	ws;

	if (ioctl(tty->fd, TIOCGWINSZ, &ws) != -1) {
		tty->sx = ws.ws_col;
		tty->sy = ws.ws_row;
	}
	if (tty->sx == 0)
		tty->sx = 80;
	if (tty->sy == 0)
		tty->sy = 24;

	tty->cx = UINT_MAX;
	tty->cy = UINT_MAX;

	tty->rupper = UINT_MAX;
	tty->rlower = UINT_MAX;
}

int
tty_open(struct tty *tty, const char *overrides, char **cause)
{
	int	fd;

	if (debug_level > 3) {
		fd = open("tmux.out", O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if (fd != -1 && fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
			fatal("fcntl failed");
		tty->log_fd = fd;
	}

	tty->term = tty_term_find(tty->termname, tty->fd, overrides, cause);
	if (tty->term == NULL) {
		tty_close(tty);
		return (-1);
	}
	tty->flags |= TTY_OPENED;

	tty->flags &= ~(TTY_NOCURSOR|TTY_FREEZE|TTY_ESCAPE);

	tty->event = bufferevent_new(
	    tty->fd, tty_read_callback, NULL, tty_error_callback, tty);

	tty_start_tty(tty);

	tty_keys_init(tty);

	tty_fill_acs(tty);

	return (0);
}

void
tty_read_callback(unused struct bufferevent *bufev, void *data)
{
	struct tty	*tty = data;

	while (tty_keys_next(tty))
		;
}

void
tty_error_callback(
    unused struct bufferevent *bufev, unused short what, unused void *data)
{
}

void
tty_start_tty(struct tty *tty)
{
	struct termios	 tio;
	int		 what, mode;

	if (tty->fd == -1)
		return;

	if ((mode = fcntl(tty->fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(tty->fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");

	bufferevent_enable(tty->event, EV_READ|EV_WRITE);

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

	tty_putcode(tty, TTYC_SMCUP);

	tty_putcode(tty, TTYC_SGR0);
	memcpy(&tty->cell, &grid_default_cell, sizeof tty->cell);

	tty_putcode(tty, TTYC_SMKX);
	tty_putcode(tty, TTYC_ENACS);
	tty_putcode(tty, TTYC_CLEAR);

	tty_putcode(tty, TTYC_CNORM);
	if (tty_term_has(tty->term, TTYC_KMOUS))
		tty_puts(tty, "\033[?1000l");

	tty->cx = UINT_MAX;
	tty->cy = UINT_MAX;

	tty->rlower = UINT_MAX;
	tty->rupper = UINT_MAX;

	tty->mode = MODE_CURSOR;

	tty->flags |= TTY_STARTED;
}

void
tty_stop_tty(struct tty *tty)
{
	struct winsize	ws;
	int		mode;

	if (!(tty->flags & TTY_STARTED))
		return;
	tty->flags &= ~TTY_STARTED;

	bufferevent_disable(tty->event, EV_READ|EV_WRITE);

	/*
	 * Be flexible about error handling and try not kill the server just
	 * because the fd is invalid. Things like ssh -t can easily leave us
	 * with a dead tty.
	 */
	if ((mode = fcntl(tty->fd, F_GETFL)) == -1)
		return;
	if (fcntl(tty->fd, F_SETFL, mode & ~O_NONBLOCK) == -1)
		return;
	if (ioctl(tty->fd, TIOCGWINSZ, &ws) == -1)
		return;
	if (tcsetattr(tty->fd, TCSANOW, &tty->tio) == -1)
		return;

	tty_raw(tty, tty_term_string2(tty->term, TTYC_CSR, 0, ws.ws_row - 1));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMACS));
	tty_raw(tty, tty_term_string(tty->term, TTYC_SGR0));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMKX));
	tty_raw(tty, tty_term_string(tty->term, TTYC_CLEAR));

	tty_raw(tty, tty_term_string(tty->term, TTYC_CNORM));
	if (tty_term_has(tty->term, TTYC_KMOUS))
		tty_raw(tty, "\033[?1000l");

	tty_raw(tty, tty_term_string(tty->term, TTYC_RMCUP));
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
tty_close(struct tty *tty)
{
	if (tty->log_fd != -1) {
		close(tty->log_fd);
		tty->log_fd = -1;
	}

	evtimer_del(&tty->key_timer);
	tty_stop_tty(tty);

	if (tty->flags & TTY_OPENED) {
		bufferevent_free(tty->event);

		tty_term_free(tty->term);
		tty_keys_free(tty);

		tty->flags &= ~TTY_OPENED;
	}

	if (tty->fd != -1) {
		close(tty->fd);
		tty->fd = -1;
	}
}

void
tty_free(struct tty *tty)
{
	tty_close(tty);

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
	bufferevent_write(tty->event, s, strlen(s));

	if (tty->log_fd != -1)
		write(tty->log_fd, s, strlen(s));
}

void
tty_putc(struct tty *tty, u_char ch)
{
	u_int	sx;

	if (tty->cell.attr & GRID_ATTR_CHARSET)
		ch = tty_get_acs(tty, ch);
	bufferevent_write(tty->event, &ch, 1);

	if (ch >= 0x20 && ch != 0x7f) {
		sx = tty->sx;
		if (tty->term->flags & TERM_EARLYWRAP)
			sx--;

		if (tty->cx >= sx) {
			tty->cx = 1;
			if (tty->cy != tty->rlower)
				tty->cy++;
		} else
			tty->cx++;
	}

	if (tty->log_fd != -1)
		write(tty->log_fd, &ch, 1);
}

void
tty_pututf8(struct tty *tty, const struct grid_utf8 *gu)
{
	size_t	size;

	size = grid_utf8_size(gu);
	bufferevent_write(tty->event, gu->data, size);
	if (tty->log_fd != -1)
		write(tty->log_fd, gu->data, size);
	tty->cx += gu->width;
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
tty_redraw_region(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int		 	 i;

	/*
	 * If region is >= 50% of the screen, just schedule a window redraw. In
	 * most cases, this is likely to be followed by some more scrolling -
	 * without this, the entire pane ends up being redrawn many times which
	 * can be much more data.
	 */
	if (ctx->orupper - ctx->orlower >= screen_size_y(s) / 2) {
		wp->flags |= PANE_REDRAW;
		return;
	}

	if (ctx->ocy < ctx->orupper || ctx->ocy > ctx->orlower) {
		for (i = ctx->ocy; i < screen_size_y(s); i++)
			tty_draw_line(tty, s, i, wp->xoff, wp->yoff);
	} else {
		for (i = ctx->orupper; i <= ctx->orlower; i++)
			tty_draw_line(tty, s, i, wp->xoff, wp->yoff);
	}
}

void
tty_draw_line(struct tty *tty, struct screen *s, u_int py, u_int ox, u_int oy)
{
	const struct grid_cell	*gc;
	struct grid_line	*gl;
	struct grid_cell	 tmpgc;
	const struct grid_utf8	*gu;
	u_int			 i, sx;

	tty_update_mode(tty, tty->mode & ~MODE_CURSOR);

	sx = screen_size_x(s);
	if (sx > s->grid->linedata[s->grid->hsize + py].cellsize)
		sx = s->grid->linedata[s->grid->hsize + py].cellsize;
	if (sx > tty->sx)
		sx = tty->sx;

	/*
	 * Don't move the cursor to the start permission if it will wrap there
	 * itself.
	 */
	gl = NULL;
	if (py != 0)
		gl = &s->grid->linedata[s->grid->hsize + py - 1];
	if (oy + py == 0 || (gl != NULL && !(gl->flags & GRID_LINE_WRAPPED)) ||
	    tty->cx < tty->sx || ox != 0 ||
	    (oy + py != tty->cy + 1 && tty->cy != s->rlower + oy))
		tty_cursor(tty, ox, oy + py);

	for (i = 0; i < sx; i++) {
		gc = grid_view_peek_cell(s->grid, i, py);

		gu = NULL;
		if (gc->flags & GRID_FLAG_UTF8)
			gu = grid_view_peek_utf8(s->grid, i, py);

		if (screen_check_selection(s, i, py)) {
			memcpy(&tmpgc, &s->sel.cell, sizeof tmpgc);
			tmpgc.data = gc->data;
			tmpgc.flags = gc->flags & 
			    ~(GRID_FLAG_FG256|GRID_FLAG_BG256);
			tmpgc.flags |= s->sel.cell.flags &
			    (GRID_FLAG_FG256|GRID_FLAG_BG256);
			tty_cell(tty, &tmpgc, gu);
		} else
			tty_cell(tty, gc, gu);
	}

	if (sx >= tty->sx) {
		tty_update_mode(tty, tty->mode);
		return;
	}
	tty_reset(tty);

	tty_cursor(tty, ox + sx, oy + py);
	if (screen_size_x(s) >= tty->sx && tty_term_has(tty->term, TTYC_EL))
		tty_putcode(tty, TTYC_EL);
	else {
		for (i = sx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
	}
	tty_update_mode(tty, tty->mode);
}

void
tty_write(void (*cmdfn)(
    struct tty *, const struct tty_ctx *), const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	struct client		*c;
	u_int		 	 i;

	if (wp == NULL)
		return;

	if (wp->window->flags & WINDOW_REDRAW || wp->flags & PANE_REDRAW)
		return;
	if (wp->window->flags & WINDOW_HIDDEN || !window_pane_visible(wp))
		return;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->flags & CLIENT_SUSPENDED)
			continue;

		if (c->session->curw->window == wp->window) {
			if (c->tty.flags & TTY_FREEZE || c->tty.term == NULL)
				continue;
			cmdfn(&c->tty, ctx);
		}
	}
}

void
tty_cmd_insertcharacter(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int			 i;

	if (wp->xoff != 0 || screen_size_x(s) < tty->sx) {
		tty_draw_line(tty, wp->screen, ctx->ocy, wp->xoff, wp->yoff);
		return;
	}

	tty_reset(tty);

 	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	if (tty_term_has(tty->term, TTYC_ICH) ||
	    tty_term_has(tty->term, TTYC_ICH1))
		tty_emulate_repeat(tty, TTYC_ICH, TTYC_ICH1, ctx->num);
	else if (tty_term_has(tty->term, TTYC_SMIR) && 
	    tty_term_has(tty->term, TTYC_RMIR)) {
		tty_putcode(tty, TTYC_SMIR);
		for (i = 0; i < ctx->num; i++)
			tty_putc(tty, ' ');
		tty_putcode(tty, TTYC_RMIR);
	} else
		tty_draw_line(tty, wp->screen, ctx->ocy, wp->xoff, wp->yoff);
}

void
tty_cmd_deletecharacter(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;

	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    (!tty_term_has(tty->term, TTYC_DCH) &&
	    !tty_term_has(tty->term, TTYC_DCH1))) {
		tty_draw_line(tty, wp->screen, ctx->ocy, wp->xoff, wp->yoff);
		return;
	}

	tty_reset(tty);

 	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	if (tty_term_has(tty->term, TTYC_DCH) ||
	    tty_term_has(tty->term, TTYC_DCH1))
		tty_emulate_repeat(tty, TTYC_DCH, TTYC_DCH1, ctx->num);
}

void
tty_cmd_insertline(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR) || 
	    !tty_term_has(tty->term, TTYC_IL1)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_reset(tty);

 	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
 	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_IL, TTYC_IL1, ctx->num);
}

void
tty_cmd_deleteline(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    !tty_term_has(tty->term, TTYC_DL1)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_reset(tty);

 	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
 	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_DL, TTYC_DL1, ctx->num);
}

void
tty_cmd_clearline(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int		 	 i;

	tty_reset(tty);

 	tty_cursor_pane(tty, ctx, 0, ctx->ocy);

	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL)) {
		tty_putcode(tty, TTYC_EL);
	} else {
		for (i = 0; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
	}
}

void
tty_cmd_clearendofline(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int		 	 i;

	tty_reset(tty);

	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL))
		tty_putcode(tty, TTYC_EL);
	else {
		for (i = ctx->ocx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
	}
}

void
tty_cmd_clearstartofline(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	u_int		 	 i;

	tty_reset(tty);

	if (wp->xoff == 0 && tty_term_has(tty->term, TTYC_EL1)) {
		tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);
		tty_putcode(tty, TTYC_EL1);
	} else {
		tty_cursor_pane(tty, ctx, 0, ctx->ocy);
		for (i = 0; i < ctx->ocx + 1; i++)
			tty_putc(tty, ' ');
	}
}

void
tty_cmd_reverseindex(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;

	if (ctx->ocy != ctx->orupper)
		return;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    !tty_term_has(tty->term, TTYC_RI)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_reset(tty);
	
	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->orupper);
	
	tty_putcode(tty, TTYC_RI);
}

void
tty_cmd_linefeed(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;

	if (ctx->ocy != ctx->orlower)
		return;

 	if (wp->xoff != 0 || screen_size_x(s) < tty->sx ||
	    !tty_term_has(tty->term, TTYC_CSR)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	/*
	 * If this line wrapped naturally (ctx->num is nonzero), don't do
	 * anything - the cursor can just be moved to the last cell and wrap
	 * naturally.
	 */
	if (ctx->num && !(tty->term->flags & TERM_EARLYWRAP))
		return;

	tty_reset(tty);
	
	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);
	
	tty_putc(tty, '\n');
}

void
tty_cmd_clearendofscreen(struct tty *tty, const struct tty_ctx *ctx)
{
  	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int		 	 i, j;

	tty_reset(tty);

	tty_region_pane(tty, ctx, 0, screen_size_y(s) - 1);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL)) {
		tty_putcode(tty, TTYC_EL);
		if (ctx->ocy != screen_size_y(s) - 1) {
			tty_cursor_pane(tty, ctx, 0, ctx->ocy + 1);
			for (i = ctx->ocy + 1; i < screen_size_y(s); i++) {
				tty_putcode(tty, TTYC_EL);
				if (i == screen_size_y(s) - 1)
					continue;
				tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
				tty->cy++;
			}
		}
	} else {
		for (i = ctx->ocx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
		for (j = ctx->ocy + 1; j < screen_size_y(s); j++) {
			tty_cursor_pane(tty, ctx, 0, j);
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
}

void
tty_cmd_clearstartofscreen(struct tty *tty, const struct tty_ctx *ctx)
{
 	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int		 	 i, j;

	tty_reset(tty);

	tty_region_pane(tty, ctx, 0, screen_size_y(s) - 1);
	tty_cursor_pane(tty, ctx, 0, 0);

	if (wp->xoff == 0 && screen_size_x(s) >= tty->sx &&
	    tty_term_has(tty->term, TTYC_EL)) {
		for (i = 0; i < ctx->ocy; i++) {
			tty_putcode(tty, TTYC_EL);
			tty_emulate_repeat(tty, TTYC_CUD, TTYC_CUD1, 1);
			tty->cy++;
		}
	} else {
		for (j = 0; j < ctx->ocy; j++) {
			tty_cursor_pane(tty, ctx, 0, j);
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	for (i = 0; i <= ctx->ocx; i++)
		tty_putc(tty, ' ');
}

void
tty_cmd_clearscreen(struct tty *tty, const struct tty_ctx *ctx)
{
 	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int		 	 i, j;

	tty_reset(tty);

	tty_region_pane(tty, ctx, 0, screen_size_y(s) - 1);
	tty_cursor_pane(tty, ctx, 0, 0);

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
			tty_cursor_pane(tty, ctx, 0, j);
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
}

void
tty_cmd_alignmenttest(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int			 i, j;

	tty_reset(tty);

	tty_region_pane(tty, ctx, 0, screen_size_y(s) - 1);

	for (j = 0; j < screen_size_y(s); j++) {
		tty_cursor_pane(tty, ctx, 0, j);
		for (i = 0; i < screen_size_x(s); i++)
			tty_putc(tty, 'E');
	}
}

void
tty_cmd_cell(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int			 cx;

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);

	/* Is the cursor in the very last position? */
	if (ctx->ocx > wp->sx - ctx->last_width) {
		if (wp->xoff != 0 || wp->sx != tty->sx) {
			/*
			 * The pane doesn't fill the entire line, the linefeed
			 * will already have happened, so just move the cursor.
			 */
			tty_cursor_pane(tty, ctx, 0, ctx->ocy + 1);
		} else if (tty->cx < tty->sx) {
			/*
			 * The cursor isn't in the last position already, so
			 * move as far left as possinble and redraw the last
			 * cell to move into the last position.
			 */
			cx = screen_size_x(s) - ctx->last_width;
			tty_cursor_pane(tty, ctx, cx, ctx->ocy);
			tty_cell(tty, &ctx->last_cell, &ctx->last_utf8);
		}
	} else
		tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_cell(tty, ctx->cell, ctx->utf8);
}

void
tty_cmd_utf8character(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;

	/*
	 * Cannot rely on not being a partial character, so just redraw the
	 * whole line.
	 */
	tty_draw_line(tty, wp->screen, ctx->ocy, wp->xoff, wp->yoff);	
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
	tty_pututf8(tty, gu);
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

/* Set region inside pane. */
void
tty_region_pane(
    struct tty *tty, const struct tty_ctx *ctx, u_int rupper, u_int rlower)
{
  	struct window_pane	*wp = ctx->wp;

	tty_region(tty, wp->yoff + rupper, wp->yoff + rlower);
}

/* Set region at absolute position. */
void
tty_region(struct tty *tty, u_int rupper, u_int rlower)
{
	if (tty->rlower == rlower && tty->rupper == rupper)
		return;
	if (!tty_term_has(tty->term, TTYC_CSR))
		return;

	tty->rupper = rupper;
	tty->rlower = rlower;

	/*
	 * Some terminals (such as PuTTY) do not correctly reset the cursor to
	 * 0,0 if it is beyond the last column (they do not reset their wrap
	 * flag so further output causes a line feed). As a workaround, do an
	 * explicit move to 0 first.
	 */
	if (tty->cx >= tty->sx)
		tty_cursor(tty, 0, tty->cy);

	tty->cx = 0;
	tty->cy = 0;

	tty_putcode2(tty, TTYC_CSR, tty->rupper, tty->rlower);
}

/* Move cursor inside pane. */
void
tty_cursor_pane(struct tty *tty, const struct tty_ctx *ctx, u_int cx, u_int cy)
{
  	struct window_pane	*wp = ctx->wp;

	tty_cursor(tty, wp->xoff + cx, wp->yoff + cy);
}

/* Move cursor to absolute position. */
void
tty_cursor(struct tty *tty, u_int cx, u_int cy)
{
	struct tty_term	*term = tty->term;
	u_int		 thisx, thisy;
	int		 change;
	
	if (cx > tty->sx - 1)
		cx = tty->sx - 1;

	thisx = tty->cx;
	thisy = tty->cy;

	/* No change. */
	if (cx == thisx && cy == thisy)
		return;

	/* Very end of the line, just use absolute movement. */
	if (thisx > tty->sx - 1)
		goto absolute;

	/* Move to home position (0, 0). */
	if (cx == 0 && cy == 0 && tty_term_has(term, TTYC_HOME)) {
		tty_putcode(tty, TTYC_HOME);
		goto out;
	}

	/* Zero on the next line. */
	if (cx == 0 && cy == thisy + 1 && thisy != tty->rlower) {
		tty_putc(tty, '\r');
		tty_putc(tty, '\n');
		goto out;
	}

	/* Moving column or row. */
	if (cy == thisy) {
		/*
		 * Moving column only, row staying the same.
		 */

		/* To left edge. */
		if (cx == 0)	{
			tty_putc(tty, '\r');
			goto out;
		}

		/* One to the left. */
		if (cx == thisx - 1 && tty_term_has(term, TTYC_CUB1)) {
			tty_putcode(tty, TTYC_CUB1);
			goto out;
		}

		/* One to the right. */
		if (cx == thisx + 1 && tty_term_has(term, TTYC_CUF1)) {
			tty_putcode(tty, TTYC_CUF1);
			goto out;
		}

		/* Calculate difference. */
		change = thisx - cx;	/* +ve left, -ve right */

		/*
		 * Use HPA if change is larger than absolute, otherwise move
		 * the cursor with CUB/CUF.
		 */
		if (abs(change) > cx && tty_term_has(term, TTYC_HPA)) {
			tty_putcode1(tty, TTYC_HPA, cx);
			goto out;
		} else if (change > 0 && tty_term_has(term, TTYC_CUB)) {
			tty_putcode1(tty, TTYC_CUB, change);
			goto out;
		} else if (change < 0 && tty_term_has(term, TTYC_CUF)) {
			tty_putcode1(tty, TTYC_CUF, -change);
			goto out;
		}
	} else if (cx == thisx) {
		/*
		 * Moving row only, column staying the same.
		 */

		/* One above. */
		if (thisy != tty->rupper && 
		    cy == thisy - 1 && tty_term_has(term, TTYC_CUU1)) {
			tty_putcode(tty, TTYC_CUU1);
			goto out;
		}

		/* One below. */
		if (thisy != tty->rlower &&
		    cy == thisy + 1 && tty_term_has(term, TTYC_CUD1)) {
			tty_putcode(tty, TTYC_CUD1);
			goto out;
		}

		/* Calculate difference. */
		change = thisy - cy;	/* +ve up, -ve down */

		/*
		 * Try to use VPA if change is larger than absolute or if this change
		 * would cross the scroll region, otherwise use CUU/CUD.
		 */
		if (abs(change) > cy ||
		    (change < 0 && cy - change > tty->rlower) ||
		    (change > 0 && cy - change < tty->rupper)) {
			    if (tty_term_has(term, TTYC_VPA)) {
				    tty_putcode1(tty, TTYC_VPA, cy);
				    goto out;
			    }
		} else if (change > 0 && tty_term_has(term, TTYC_CUU)) {
			tty_putcode1(tty, TTYC_CUU, change);
			goto out;
		} else if (change < 0 && tty_term_has(term, TTYC_CUD)) {
			tty_putcode1(tty, TTYC_CUD, -change);
			goto out;
		}
	}

absolute:
	/* Absolute movement. */
	tty_putcode2(tty, TTYC_CUP, cy, cx);

out:
	tty->cx = cx;
	tty->cy = cy;
}

void
tty_attributes(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell, gc2;
	u_char			 changed;
	u_int			 new_attr;

	/* If the character is space, don't care about foreground. */
	if (gc->data == ' ' && !(gc->flags & GRID_FLAG_UTF8)) {
		memcpy(&gc2, gc, sizeof gc2);
		if (gc->attr & GRID_ATTR_REVERSE)
			gc2.bg = tc->bg;
		else
			gc2.fg = tc->fg;
		gc = &gc2;
	}

	/*
	 * If no setab, try to use the reverse attribute as a best-effort for a
	 * non-default background. This is a bit of a hack but it doesn't do
	 * any serious harm and makes a couple of applications happier.
	 */
	if (!tty_term_has(tty->term, TTYC_SETAB)) {
		if (gc != &gc2) {
			memcpy(&gc2, gc, sizeof gc2);
			gc = &gc2;
		}

		if (gc->attr & GRID_ATTR_REVERSE) {
			if (gc->fg != 7 && gc->fg != 8)
				gc2.attr &= ~GRID_ATTR_REVERSE;
		} else {
			if (gc->bg != 0 && gc->bg != 8)
				gc2.attr |= GRID_ATTR_REVERSE;
		}
	}

	/* If any bits are being cleared, reset everything. */
	if (tc->attr & ~gc->attr)
		tty_reset(tty);

	/*
	 * Set the colours. This may call tty_reset() (so it comes next) and
	 * may add to (NOT remove) the desired attributes by changing new_attr.
	 */
	new_attr = gc->attr;
	tty_colours(tty, gc, &new_attr);

	/* Filter out attribute bits already set. */
	changed = new_attr & ~tc->attr;
	tc->attr = new_attr;

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
}

void
tty_colours(struct tty *tty, const struct grid_cell *gc, int *attr)
{
	struct grid_cell	*tc = &tty->cell;
	u_char			 fg = gc->fg, bg = gc->bg, flags = gc->flags;
	int			 have_ax, fg_default, bg_default;

	/* No changes? Nothing is necessary. */
	if (fg == tc->fg && bg == tc->bg &&
	    ((flags ^ tc->flags) & (GRID_FLAG_FG256|GRID_FLAG_BG256)) == 0)
		return;

	/*
	 * Is either the default colour? This is handled specially because the
	 * best solution might be to reset both colours to default, in which
	 * case if only one is default need to fall onward to set the other
	 * colour.
	 */
	fg_default = (fg == 8 && !(flags & GRID_FLAG_FG256));
	bg_default = (bg == 8 && !(flags & GRID_FLAG_BG256));
	if (fg_default || bg_default) {
		/*
		 * If don't have AX but do have op, send sgr0 (op can't
		 * actually be used because it is sometimes the same as sgr0
		 * and sometimes isn't). This resets both colours to default.
		 *
		 * Otherwise, try to set the default colour only as needed.
		 */
		have_ax = tty_term_has(tty->term, TTYC_AX);
		if (!have_ax && tty_term_has(tty->term, TTYC_OP))
			tty_reset(tty);
		else {
			if (fg_default &&
			    fg != tc->fg && !(tc->flags & GRID_FLAG_FG256)) {
				if (have_ax)
					tty_puts(tty, "\033[39m");
				else if (tc->fg != 7)
					tty_putcode1(tty, TTYC_SETAF, 7);
				tc->fg = 8;
				tc->flags &= ~GRID_FLAG_FG256;
			}
			if (bg_default &&
			    bg != tc->bg && !(tc->flags & GRID_FLAG_BG256)) {
				if (have_ax)					
					tty_puts(tty, "\033[49m");
				else if (tc->bg != 0)
					tty_putcode1(tty, TTYC_SETAB, 0);
				tc->bg = 8;
				tc->flags &= ~GRID_FLAG_BG256;
			}
		}
	}

	/* Set the foreground colour. */
	if (!fg_default && (fg != tc->fg ||
	    ((flags & GRID_FLAG_FG256) != (tc->flags & GRID_FLAG_FG256))))
		tty_colours_fg(tty, gc, attr);

	/*
	 * Set the background colour. This must come after the foreground as
	 * tty_colour_fg() can call tty_reset().
	 */
	if (!bg_default && (bg != tc->bg ||
	    ((flags & GRID_FLAG_BG256) != (tc->flags & GRID_FLAG_BG256))))
		tty_colours_bg(tty, gc, attr);
}

void
tty_colours_fg(struct tty *tty, const struct grid_cell *gc, int *attr)
{
	struct grid_cell	*tc = &tty->cell;
	u_char			 fg = gc->fg;

	/* Is this a 256-colour colour? */
	if (gc->flags & GRID_FLAG_FG256) {
		/* Try as 256 colours or translating to 88. */
		if (tty_try_256(tty, fg, "38") == 0)
			goto save_fg;
		if (tty_try_88(tty, fg, "38") == 0)
			goto save_fg;

		/* Translate to 16-colour palette, updating bold if needed. */
		fg = colour_256to16(fg);
		if (fg & 8) {
			fg &= 7;
			(*attr) |= GRID_ATTR_BRIGHT;
		} else
			tty_reset(tty);		/* turn off bold */
	}

	/* Otherwise set the foreground colour. */
	tty_putcode1(tty, TTYC_SETAF, fg);

save_fg:	
	/* Save the new values in the terminal current cell. */
	tc->fg = fg;
	tc->flags &= ~GRID_FLAG_FG256;
	tc->flags |= gc->flags & GRID_FLAG_FG256;
}

void
tty_colours_bg(struct tty *tty, const struct grid_cell *gc, unused int *attr)
{
	struct grid_cell	*tc = &tty->cell;
	u_char			 bg = gc->bg;

	/* Is this a 256-colour colour? */
	if (gc->flags & GRID_FLAG_BG256) {
		/* Try as 256 colours or translating to 88. */
		if (tty_try_256(tty, bg, "48") == 0)
			goto save_bg;
		if (tty_try_88(tty, bg, "48") == 0)
			goto save_bg;

		/*
		 * Translate to 16-colour palette. Bold background doesn't
		 * exist portably, so just discard the bold bit if set.
		 */
		bg = colour_256to16(bg);
		if (bg & 8)
			bg &= 7;
	}

	/* Otherwise set the background colour. */
	tty_putcode1(tty, TTYC_SETAB, bg);

save_bg:
	/* Save the new values in the terminal current cell. */
	tc->bg = bg;
	tc->flags &= ~GRID_FLAG_BG256;
	tc->flags |= gc->flags & GRID_FLAG_BG256;
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
