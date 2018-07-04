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

#include <sys/types.h>
#include <sys/ioctl.h>

#include <netinet/in.h>

#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

static int	tty_log_fd = -1;

static int	tty_client_ready(struct client *, struct window_pane *);

static void	tty_set_italics(struct tty *);
static int	tty_try_colour(struct tty *, int, const char *);
static void	tty_force_cursor_colour(struct tty *, const char *);
static void	tty_cursor_pane(struct tty *, const struct tty_ctx *, u_int,
		    u_int);
static void	tty_cursor_pane_unless_wrap(struct tty *,
		    const struct tty_ctx *, u_int, u_int);
static void	tty_invalidate(struct tty *);
static void	tty_colours(struct tty *, const struct grid_cell *);
static void	tty_check_fg(struct tty *, const struct window_pane *,
		    struct grid_cell *);
static void	tty_check_bg(struct tty *, const struct window_pane *,
		    struct grid_cell *);
static void	tty_colours_fg(struct tty *, const struct grid_cell *);
static void	tty_colours_bg(struct tty *, const struct grid_cell *);

static void	tty_region_pane(struct tty *, const struct tty_ctx *, u_int,
		    u_int);
static void	tty_region(struct tty *, u_int, u_int);
static void	tty_margin_pane(struct tty *, const struct tty_ctx *);
static void	tty_margin(struct tty *, u_int, u_int);
static int	tty_large_region(struct tty *, const struct tty_ctx *);
static int	tty_fake_bce(const struct tty *, const struct window_pane *,
		    u_int);
static void	tty_redraw_region(struct tty *, const struct tty_ctx *);
static void	tty_emulate_repeat(struct tty *, enum tty_code_code,
		    enum tty_code_code, u_int);
static void	tty_repeat_space(struct tty *, u_int);
static void	tty_cell(struct tty *, const struct grid_cell *,
		    const struct window_pane *);
static void	tty_default_colours(struct grid_cell *,
		    const struct window_pane *);
static void	tty_default_attributes(struct tty *, const struct window_pane *,
		    u_int);

#define tty_use_margin(tty) \
	((tty)->term_type == TTY_VT420)

#define tty_pane_full_width(tty, ctx) \
	((ctx)->xoff == 0 && screen_size_x((ctx)->wp->screen) >= (tty)->sx)

#define TTY_BLOCK_INTERVAL (100000 /* 100 milliseconds */)
#define TTY_BLOCK_START(tty) (1 + ((tty)->sx * (tty)->sy) * 8)
#define TTY_BLOCK_STOP(tty) (1 + ((tty)->sx * (tty)->sy) / 8)

void
tty_create_log(void)
{
	char	name[64];

	xsnprintf(name, sizeof name, "tmux-out-%ld.log", (long)getpid());

	tty_log_fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (tty_log_fd != -1 && fcntl(tty_log_fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");
}

int
tty_init(struct tty *tty, struct client *c, int fd, char *term)
{
	if (!isatty(fd))
		return (-1);

	memset(tty, 0, sizeof *tty);

	if (term == NULL || *term == '\0')
		tty->term_name = xstrdup("unknown");
	else
		tty->term_name = xstrdup(term);

	tty->fd = fd;
	tty->client = c;

	tty->cstyle = 0;
	tty->ccolour = xstrdup("");

	tty->flags = 0;

	tty->term_flags = 0;
	tty->term_type = TTY_UNKNOWN;

	return (0);
}

void
tty_resize(struct tty *tty)
{
	struct client	*c = tty->client;
	struct winsize	 ws;
	u_int		 sx, sy;

	if (ioctl(tty->fd, TIOCGWINSZ, &ws) != -1) {
		sx = ws.ws_col;
		if (sx == 0)
			sx = 80;
		sy = ws.ws_row;
		if (sy == 0)
			sy = 24;
	} else {
		sx = 80;
		sy = 24;
	}
	log_debug("%s: %s now %ux%u", __func__, c->name, sx, sy);
	tty_set_size(tty, sx, sy);
	tty_invalidate(tty);
}

void
tty_set_size(struct tty *tty, u_int sx, u_int sy)
{
	tty->sx = sx;
	tty->sy = sy;
}

static void
tty_read_callback(__unused int fd, __unused short events, void *data)
{
	struct tty	*tty = data;
	struct client	*c = tty->client;
	size_t		 size = EVBUFFER_LENGTH(tty->in);
	int		 nread;

	nread = evbuffer_read(tty->in, tty->fd, -1);
	if (nread == 0 || nread == -1) {
		event_del(&tty->event_in);
		server_client_lost(tty->client);
		return;
	}
	log_debug("%s: read %d bytes (already %zu)", c->name, nread, size);

	while (tty_keys_next(tty))
		;
}

static void
tty_timer_callback(__unused int fd, __unused short events, void *data)
{
	struct tty	*tty = data;
	struct client	*c = tty->client;
	struct timeval	 tv = { .tv_usec = TTY_BLOCK_INTERVAL };

	log_debug("%s: %zu discarded", c->name, tty->discarded);

	c->flags |= CLIENT_REDRAW;
	c->discarded += tty->discarded;

	if (tty->discarded < TTY_BLOCK_STOP(tty)) {
		tty->flags &= ~TTY_BLOCK;
		tty_invalidate(tty);
		return;
	}
	tty->discarded = 0;
	evtimer_add(&tty->timer, &tv);
}

static int
tty_block_maybe(struct tty *tty)
{
	struct client	*c = tty->client;
	size_t		 size = EVBUFFER_LENGTH(tty->out);
	struct timeval	 tv = { .tv_usec = TTY_BLOCK_INTERVAL };

	if (size < TTY_BLOCK_START(tty))
		return (0);

	if (tty->flags & TTY_BLOCK)
		return (1);
	tty->flags |= TTY_BLOCK;

	log_debug("%s: can't keep up, %zu discarded", c->name, size);

	evbuffer_drain(tty->out, size);
	c->discarded += size;

	tty->discarded = 0;
	evtimer_add(&tty->timer, &tv);
	return (1);
}

static void
tty_write_callback(__unused int fd, __unused short events, void *data)
{
	struct tty	*tty = data;
	struct client	*c = tty->client;
	size_t		 size = EVBUFFER_LENGTH(tty->out);
	int		 nwrite;

	nwrite = evbuffer_write(tty->out, tty->fd);
	if (nwrite == -1)
		return;
	log_debug("%s: wrote %d bytes (of %zu)", c->name, nwrite, size);

	if (c->redraw > 0) {
		if ((size_t)nwrite >= c->redraw)
			c->redraw = 0;
		else
			c->redraw -= nwrite;
		log_debug("%s: waiting for redraw, %zu bytes left", c->name,
		    c->redraw);
	} else if (tty_block_maybe(tty))
		return;

	if (EVBUFFER_LENGTH(tty->out) != 0)
		event_add(&tty->event_out, NULL);
}

int
tty_open(struct tty *tty, char **cause)
{
	tty->term = tty_term_find(tty->term_name, tty->fd, cause);
	if (tty->term == NULL) {
		tty_close(tty);
		return (-1);
	}
	tty->flags |= TTY_OPENED;

	tty->flags &= ~(TTY_NOCURSOR|TTY_FREEZE|TTY_BLOCK|TTY_TIMER);

	event_set(&tty->event_in, tty->fd, EV_PERSIST|EV_READ,
	    tty_read_callback, tty);
	tty->in = evbuffer_new();

	event_set(&tty->event_out, tty->fd, EV_WRITE, tty_write_callback, tty);
	tty->out = evbuffer_new();

	evtimer_set(&tty->timer, tty_timer_callback, tty);

	tty_start_tty(tty);

	tty_keys_build(tty);

	return (0);
}

void
tty_start_tty(struct tty *tty)
{
	struct client	*c = tty->client;
	struct termios	 tio;

	if (tty->fd != -1 && tcgetattr(tty->fd, &tty->tio) == 0) {
		setblocking(tty->fd, 0);
		event_add(&tty->event_in, NULL);

		memcpy(&tio, &tty->tio, sizeof tio);
		tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR|IGNCR|IMAXBEL|ISTRIP);
		tio.c_iflag |= IGNBRK;
		tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET);
		tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHONL|ECHOCTL|
		    ECHOPRT|ECHOKE|ISIG);
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
		if (tcsetattr(tty->fd, TCSANOW, &tio) == 0)
			tcflush(tty->fd, TCIOFLUSH);
	}

	tty_putcode(tty, TTYC_SMCUP);

	tty_putcode(tty, TTYC_SMKX);
	tty_putcode(tty, TTYC_CLEAR);

	if (tty_acs_needed(tty)) {
		log_debug("%s: using capabilities for ACS", c->name);
		tty_putcode(tty, TTYC_ENACS);
	} else
		log_debug("%s: using UTF-8 for ACS", c->name);

	tty_putcode(tty, TTYC_CNORM);
	if (tty_term_has(tty->term, TTYC_KMOUS))
		tty_puts(tty, "\033[?1000l\033[?1002l\033[?1006l\033[?1005l");

	if (tty_term_flag(tty->term, TTYC_XT)) {
		if (options_get_number(global_options, "focus-events")) {
			tty->flags |= TTY_FOCUS;
			tty_puts(tty, "\033[?1004h");
		}
		tty_puts(tty, "\033[c");
	}

	tty->flags |= TTY_STARTED;
	tty_invalidate(tty);

	tty_force_cursor_colour(tty, "");

	tty->mouse_drag_flag = 0;
	tty->mouse_drag_update = NULL;
	tty->mouse_drag_release = NULL;
}

void
tty_stop_tty(struct tty *tty)
{
	struct winsize	ws;

	if (!(tty->flags & TTY_STARTED))
		return;
	tty->flags &= ~TTY_STARTED;

	event_del(&tty->timer);
	tty->flags &= ~TTY_BLOCK;

	event_del(&tty->event_in);
	event_del(&tty->event_out);

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
	if (tty_acs_needed(tty))
		tty_raw(tty, tty_term_string(tty->term, TTYC_RMACS));
	tty_raw(tty, tty_term_string(tty->term, TTYC_SGR0));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMKX));
	tty_raw(tty, tty_term_string(tty->term, TTYC_CLEAR));
	if (tty_term_has(tty->term, TTYC_SS) && tty->cstyle != 0) {
		if (tty_term_has(tty->term, TTYC_SE))
			tty_raw(tty, tty_term_string(tty->term, TTYC_SE));
		else
			tty_raw(tty, tty_term_string1(tty->term, TTYC_SS, 0));
	}
	if (tty->mode & MODE_BRACKETPASTE)
		tty_raw(tty, "\033[?2004l");
	tty_raw(tty, tty_term_string(tty->term, TTYC_CR));

	tty_raw(tty, tty_term_string(tty->term, TTYC_CNORM));
	if (tty_term_has(tty->term, TTYC_KMOUS))
		tty_raw(tty, "\033[?1000l\033[?1002l\033[?1006l\033[?1005l");

	if (tty_term_flag(tty->term, TTYC_XT)) {
		if (tty->flags & TTY_FOCUS) {
			tty->flags &= ~TTY_FOCUS;
			tty_raw(tty, "\033[?1004l");
		}
	}

	if (tty_use_margin(tty))
		tty_raw(tty, "\033[?69l"); /* DECLRMM */
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMCUP));

	setblocking(tty->fd, 1);
}

void
tty_close(struct tty *tty)
{
	if (event_initialized(&tty->key_timer))
		evtimer_del(&tty->key_timer);
	tty_stop_tty(tty);

	if (tty->flags & TTY_OPENED) {
		evbuffer_free(tty->in);
		event_del(&tty->event_in);
		evbuffer_free(tty->out);
		event_del(&tty->event_out);

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

	free(tty->ccolour);
	free(tty->term_name);
}

void
tty_set_type(struct tty *tty, int type)
{
	tty->term_type = type;

	if (tty_use_margin(tty))
		tty_puts(tty, "\033[?69h"); /* DECLRMM */
}

void
tty_raw(struct tty *tty, const char *s)
{
	ssize_t	n, slen;
	u_int	i;

	slen = strlen(s);
	for (i = 0; i < 5; i++) {
		n = write(tty->fd, s, slen);
		if (n >= 0) {
			s += n;
			slen -= n;
			if (slen == 0)
				break;
		} else if (n == -1 && errno != EAGAIN)
			break;
		usleep(100);
	}
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
tty_putcode3(struct tty *tty, enum tty_code_code code, int a, int b, int c)
{
	if (a < 0 || b < 0 || c < 0)
		return;
	tty_puts(tty, tty_term_string3(tty->term, code, a, b, c));
}

void
tty_putcode_ptr1(struct tty *tty, enum tty_code_code code, const void *a)
{
	if (a != NULL)
		tty_puts(tty, tty_term_ptr1(tty->term, code, a));
}

void
tty_putcode_ptr2(struct tty *tty, enum tty_code_code code, const void *a,
    const void *b)
{
	if (a != NULL && b != NULL)
		tty_puts(tty, tty_term_ptr2(tty->term, code, a, b));
}

static void
tty_add(struct tty *tty, const char *buf, size_t len)
{
	struct client	*c = tty->client;

	if (tty->flags & TTY_BLOCK) {
		tty->discarded += len;
		return;
	}

	evbuffer_add(tty->out, buf, len);
	log_debug("%s: %.*s", c->name, (int)len, buf);
	c->written += len;

	if (tty_log_fd != -1)
		write(tty_log_fd, buf, len);
	if (tty->flags & TTY_STARTED)
		event_add(&tty->event_out, NULL);
}

void
tty_puts(struct tty *tty, const char *s)
{
	if (*s != '\0')
		tty_add(tty, s, strlen(s));
}

void
tty_putc(struct tty *tty, u_char ch)
{
	const char	*acs;

	if (tty->cell.attr & GRID_ATTR_CHARSET) {
		acs = tty_acs_get(tty, ch);
		if (acs != NULL)
			tty_add(tty, acs, strlen(acs));
		else
			tty_add(tty, &ch, 1);
	} else
		tty_add(tty, &ch, 1);

	if (ch >= 0x20 && ch != 0x7f) {
		if (tty->cx >= tty->sx) {
			tty->cx = 1;
			if (tty->cy != tty->rlower)
				tty->cy++;

			/*
			 * On !xenl terminals, force the cursor position to
			 * where we think it should be after a line wrap - this
			 * means it works on sensible terminals as well.
			 */
			if (tty->term->flags & TERM_EARLYWRAP)
				tty_putcode2(tty, TTYC_CUP, tty->cy, tty->cx);
		} else
			tty->cx++;
	}
}

void
tty_putn(struct tty *tty, const void *buf, size_t len, u_int width)
{
	tty_add(tty, buf, len);
	if (tty->cx + width > tty->sx) {
		tty->cx = (tty->cx + width) - tty->sx;
		if (tty->cx <= tty->sx)
			tty->cy++;
		else
			tty->cx = tty->cy = UINT_MAX;
	} else
		tty->cx += width;
}

static void
tty_set_italics(struct tty *tty)
{
	const char	*s;

	if (tty_term_has(tty->term, TTYC_SITM)) {
		s = options_get_string(global_options, "default-terminal");
		if (strcmp(s, "screen") != 0 && strncmp(s, "screen-", 7) != 0) {
			tty_putcode(tty, TTYC_SITM);
			return;
		}
	}
	tty_putcode(tty, TTYC_SMSO);
}

void
tty_set_title(struct tty *tty, const char *title)
{
	if (!tty_term_has(tty->term, TTYC_TSL) ||
	    !tty_term_has(tty->term, TTYC_FSL))
		return;

	tty_putcode(tty, TTYC_TSL);
	tty_puts(tty, title);
	tty_putcode(tty, TTYC_FSL);
}

static void
tty_force_cursor_colour(struct tty *tty, const char *ccolour)
{
	if (*ccolour == '\0')
		tty_putcode(tty, TTYC_CR);
	else
		tty_putcode_ptr1(tty, TTYC_CS, ccolour);
	free(tty->ccolour);
	tty->ccolour = xstrdup(ccolour);
}

void
tty_update_mode(struct tty *tty, int mode, struct screen *s)
{
	int	changed;

	if (s != NULL && strcmp(s->ccolour, tty->ccolour) != 0)
		tty_force_cursor_colour(tty, s->ccolour);

	if (tty->flags & TTY_NOCURSOR)
		mode &= ~MODE_CURSOR;

	changed = mode ^ tty->mode;
	if (changed & MODE_BLINKING) {
		if (tty_term_has(tty->term, TTYC_CVVIS))
			tty_putcode(tty, TTYC_CVVIS);
		else
			tty_putcode(tty, TTYC_CNORM);
		changed |= MODE_CURSOR;
	}
	if (changed & MODE_CURSOR) {
		if (mode & MODE_CURSOR)
			tty_putcode(tty, TTYC_CNORM);
		else
			tty_putcode(tty, TTYC_CIVIS);
	}
	if (s != NULL && tty->cstyle != s->cstyle) {
		if (tty_term_has(tty->term, TTYC_SS)) {
			if (s->cstyle == 0 &&
			    tty_term_has(tty->term, TTYC_SE))
				tty_putcode(tty, TTYC_SE);
			else
				tty_putcode1(tty, TTYC_SS, s->cstyle);
		}
		tty->cstyle = s->cstyle;
	}
	if (changed & ALL_MOUSE_MODES) {
		if (mode & ALL_MOUSE_MODES) {
			/*
			 * Enable the SGR (1006) extension unconditionally, as
			 * it is safe from misinterpretation.
			 */
			tty_puts(tty, "\033[?1006h");
			if (mode & MODE_MOUSE_ALL)
				tty_puts(tty, "\033[?1003h");
			else if (mode & MODE_MOUSE_BUTTON)
				tty_puts(tty, "\033[?1002h");
			else if (mode & MODE_MOUSE_STANDARD)
				tty_puts(tty, "\033[?1000h");
		} else {
			if (tty->mode & MODE_MOUSE_ALL)
				tty_puts(tty, "\033[?1003l");
			else if (tty->mode & MODE_MOUSE_BUTTON)
				tty_puts(tty, "\033[?1002l");
			else if (tty->mode & MODE_MOUSE_STANDARD)
				tty_puts(tty, "\033[?1000l");
			tty_puts(tty, "\033[?1006l");
		}
	}
	if (changed & MODE_BRACKETPASTE) {
		if (mode & MODE_BRACKETPASTE)
			tty_puts(tty, "\033[?2004h");
		else
			tty_puts(tty, "\033[?2004l");
	}
	tty->mode = mode;
}

static void
tty_emulate_repeat(struct tty *tty, enum tty_code_code code,
    enum tty_code_code code1, u_int n)
{
	if (tty_term_has(tty->term, code))
		tty_putcode1(tty, code, n);
	else {
		while (n-- > 0)
			tty_putcode(tty, code1);
	}
}

static void
tty_repeat_space(struct tty *tty, u_int n)
{
	static char s[500];

	if (*s != ' ')
		memset(s, ' ', sizeof s);

	while (n > sizeof s) {
		tty_putn(tty, s, sizeof s, sizeof s);
		n -= sizeof s;
	}
	if (n != 0)
		tty_putn(tty, s, n, n);
}

/*
 * Is the region large enough to be worth redrawing once later rather than
 * probably several times now? Currently yes if it is more than 50% of the
 * pane.
 */
static int
tty_large_region(__unused struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;

	return (ctx->orlower - ctx->orupper >= screen_size_y(wp->screen) / 2);
}

/*
 * Return if BCE is needed but the terminal doesn't have it - it'll need to be
 * emulated.
 */
static int
tty_fake_bce(const struct tty *tty, const struct window_pane *wp, u_int bg)
{
	struct grid_cell	gc;

	if (tty_term_flag(tty->term, TTYC_BCE))
		return (0);

	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (wp != NULL)
		tty_default_colours(&gc, wp);

	if (bg != 8 || gc.bg != 8)
		return (1);
	return (0);
}

/*
 * Redraw scroll region using data from screen (already updated). Used when
 * CSR not supported, or window is a pane that doesn't take up the full
 * width of the terminal.
 */
static void
tty_redraw_region(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int			 i;

	/*
	 * If region is large, schedule a window redraw. In most cases this is
	 * likely to be followed by some more scrolling.
	 */
	if (tty_large_region(tty, ctx)) {
		wp->flags |= PANE_REDRAW;
		return;
	}

	if (ctx->ocy < ctx->orupper || ctx->ocy > ctx->orlower) {
		for (i = ctx->ocy; i < screen_size_y(s); i++)
			tty_draw_pane(tty, wp, i, ctx->xoff, ctx->yoff);
	} else {
		for (i = ctx->orupper; i <= ctx->orlower; i++)
			tty_draw_pane(tty, wp, i, ctx->xoff, ctx->yoff);
	}
}

static void
tty_clear_line(struct tty *tty, const struct window_pane *wp, u_int py,
    u_int px, u_int nx, u_int bg)
{
	log_debug("%s: %u at %u,%u", __func__, nx, px, py);

	/* Nothing to clear. */
	if (nx == 0)
		return;

	/* If genuine BCE is available, can try escape sequences. */
	if (!tty_fake_bce(tty, wp, bg)) {
		/* Off the end of the line, use EL if available. */
		if (px + nx >= tty->sx && tty_term_has(tty->term, TTYC_EL)) {
			tty_cursor(tty, px, py);
			tty_putcode(tty, TTYC_EL);
			return;
		}

		/* At the start of the line. Use EL1. */
		if (px == 0 && tty_term_has(tty->term, TTYC_EL1)) {
			tty_cursor(tty, px + nx - 1, py);
			tty_putcode(tty, TTYC_EL1);
			return;
		}

		/* Section of line. Use ECH if possible. */
		if (tty_term_has(tty->term, TTYC_ECH)) {
			tty_cursor(tty, px, py);
			tty_putcode1(tty, TTYC_ECH, nx);
			return;
		}
	}

	/* Couldn't use an escape sequence, use spaces. */
	tty_cursor(tty, px, py);
	tty_repeat_space(tty, nx);
}

static void
tty_clear_area(struct tty *tty, const struct window_pane *wp, u_int py,
    u_int ny, u_int px, u_int nx, u_int bg)
{
	u_int	yy;
	char	tmp[64];

	log_debug("%s: %u,%u at %u,%u", __func__, nx, ny, px, py);

	/* Nothing to clear. */
	if (nx == 0 || ny == 0)
		return;

	/* If genuine BCE is available, can try escape sequences. */
	if (!tty_fake_bce(tty, wp, bg)) {
		/* Use ED if clearing off the bottom of the terminal. */
		if (px == 0 &&
		    px + nx >= tty->sx &&
		    py + ny >= tty->sy &&
		    tty_term_has(tty->term, TTYC_ED)) {
			tty_cursor(tty, 0, py);
			tty_putcode(tty, TTYC_ED);
			return;
		}

		/*
		 * On VT420 compatible terminals we can use DECFRA if the
		 * background colour isn't default (because it doesn't work
		 * after SGR 0).
		 */
		if (tty->term_type == TTY_VT420 && bg != 8) {
			xsnprintf(tmp, sizeof tmp, "\033[32;%u;%u;%u;%u$x",
			    py + 1, px + 1, py + ny, px + nx);
			tty_puts(tty, tmp);
			return;
		}

		/* Full lines can be scrolled away to clear them. */
		if (px == 0 &&
		    px + nx >= tty->sx &&
		    ny > 2 &&
		    tty_term_has(tty->term, TTYC_CSR) &&
		    tty_term_has(tty->term, TTYC_INDN)) {
			tty_region(tty, py, py + ny - 1);
			tty_margin_off(tty);
			tty_putcode1(tty, TTYC_INDN, ny);
			return;
		}

		/*
		 * If margins are supported, can just scroll the area off to
		 * clear it.
		 */
		if (nx > 2 &&
		    ny > 2 &&
		    tty_term_has(tty->term, TTYC_CSR) &&
		    tty_use_margin(tty) &&
		    tty_term_has(tty->term, TTYC_INDN)) {
			tty_region(tty, py, py + ny - 1);
			tty_margin(tty, px, px + nx - 1);
			tty_putcode1(tty, TTYC_INDN, ny);
			return;
		}
	}

	/* Couldn't use an escape sequence, loop over the lines. */
	for (yy = py; yy < py + ny; yy++)
		tty_clear_line(tty, wp, yy, px, nx, bg);
}

void
tty_draw_pane(struct tty *tty, const struct window_pane *wp, u_int py, u_int ox,
    u_int oy)
{
	tty_draw_line(tty, wp, wp->screen, py, ox, oy);
}

static const struct grid_cell *
tty_check_codeset(struct tty *tty, const struct grid_cell *gc)
{
	static struct grid_cell	new;
	u_int			n;

	/* Characters less than 0x7f are always fine, no matter what. */
	if (gc->data.size == 1 && *gc->data.data < 0x7f)
		return (gc);

	/* UTF-8 terminal and a UTF-8 character - fine. */
	if (tty->flags & TTY_UTF8)
		return (gc);

	/* Replace by the right number of underscores. */
	n = gc->data.width;
	if (n > UTF8_SIZE)
		n = UTF8_SIZE;
	memcpy(&new, gc, sizeof new);
	new.data.size = n;
	memset(new.data.data, '_', n);
	return (&new);
}

void
tty_draw_line(struct tty *tty, const struct window_pane *wp,
    struct screen *s, u_int py, u_int ox, u_int oy)
{
	struct grid		*gd = s->grid;
	struct grid_cell	 gc, last;
	const struct grid_cell	*gcp;
	u_int			 i, j, ux, sx, nx, width;
	int			 flags, cleared = 0;
	char			 buf[512];
	size_t			 len, old_len;
	u_int			 cellsize;

	flags = (tty->flags & TTY_NOCURSOR);
	tty->flags |= TTY_NOCURSOR;
	tty_update_mode(tty, tty->mode, s);

	tty_region_off(tty);
	tty_margin_off(tty);

	/*
	 * Clamp the width to cellsize - note this is not cellused, because
	 * there may be empty background cells after it (from BCE).
	 */
	sx = screen_size_x(s);

	cellsize = grid_get_line(gd, gd->hsize + py)->cellsize;
	if (sx > cellsize)
		sx = cellsize;
	if (sx > tty->sx)
		sx = tty->sx;
	ux = 0;

	if (wp == NULL ||
	    py == 0 ||
	    (~grid_get_line(gd, gd->hsize + py - 1)->flags & GRID_LINE_WRAPPED) ||
	    ox != 0 ||
	    tty->cx < tty->sx ||
	    screen_size_x(s) < tty->sx) {
		if (screen_size_x(s) < tty->sx &&
		    ox == 0 &&
		    sx != screen_size_x(s) &&
		    tty_term_has(tty->term, TTYC_EL1) &&
		    !tty_fake_bce(tty, wp, 8)) {
			tty_default_attributes(tty, wp, 8);
			tty_cursor(tty, screen_size_x(s) - 1, oy + py);
			tty_putcode(tty, TTYC_EL1);
			cleared = 1;
		}
		if (sx != 0)
			tty_cursor(tty, ox, oy + py);
	} else
		log_debug("%s: wrapped line %u", __func__, oy + py);

	memcpy(&last, &grid_default_cell, sizeof last);
	len = 0;
	width = 0;

	for (i = 0; i < sx; i++) {
		grid_view_get_cell(gd, i, py, &gc);
		gcp = tty_check_codeset(tty, &gc);
		if (len != 0 &&
		    ((gcp->attr & GRID_ATTR_CHARSET) ||
		    gcp->flags != last.flags ||
		    gcp->attr != last.attr ||
		    gcp->fg != last.fg ||
		    gcp->bg != last.bg ||
		    ux + width + gcp->data.width >= screen_size_x(s) ||
		    (sizeof buf) - len < gcp->data.size)) {
			tty_attributes(tty, &last, wp);
			tty_putn(tty, buf, len, width);
			ux += width;

			len = 0;
			width = 0;
		}

		if (gcp->flags & GRID_FLAG_SELECTED)
			screen_select_cell(s, &last, gcp);
		else
			memcpy(&last, gcp, sizeof last);
		if (ux + gcp->data.width > screen_size_x(s)) {
			tty_attributes(tty, &last, wp);
			for (j = 0; j < gcp->data.width; j++) {
				if (ux + j > screen_size_x(s))
					break;
				tty_putc(tty, ' ');
				ux++;
			}
		} else if (gcp->attr & GRID_ATTR_CHARSET) {
			tty_attributes(tty, &last, wp);
			for (j = 0; j < gcp->data.size; j++)
				tty_putc(tty, gcp->data.data[j]);
			ux += gc.data.width;
		} else {
			memcpy(buf + len, gcp->data.data, gcp->data.size);
			len += gcp->data.size;
			width += gcp->data.width;
		}
	}
	if (len != 0) {
		if (grid_cells_equal(&last, &grid_default_cell)) {
			old_len = len;
			while (len > 0 && buf[len - 1] == ' ') {
				len--;
				width--;
			}
			log_debug("%s: trimmed %zu spaces", __func__,
			    old_len - len);
		}
		if (len != 0) {
			tty_attributes(tty, &last, wp);
			tty_putn(tty, buf, len, width);
			ux += width;
		}
	}

	if (!cleared && ux < screen_size_x(s)) {
		nx = screen_size_x(s) - ux;
		tty_default_attributes(tty, wp, 8);
		tty_clear_line(tty, wp, oy + py, ox + ux, nx, 8);
	}

	tty->flags = (tty->flags & ~TTY_NOCURSOR) | flags;
	tty_update_mode(tty, tty->mode, s);
}

static int
tty_client_ready(struct client *c, struct window_pane *wp)
{
	if (c->session == NULL || c->tty.term == NULL)
		return (0);
	if (c->flags & (CLIENT_REDRAW|CLIENT_SUSPENDED))
		return (0);
	if (c->tty.flags & TTY_FREEZE)
		return (0);
	if (c->session->curw->window != wp->window)
		return (0);
	return (1);
}

void
tty_write(void (*cmdfn)(struct tty *, const struct tty_ctx *),
    struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	struct client		*c;

	/* wp can be NULL if updating the screen but not the terminal. */
	if (wp == NULL)
		return;

	if ((wp->flags & (PANE_REDRAW|PANE_DROP)) || !window_pane_visible(wp))
		return;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!tty_client_ready(c, wp))
			continue;

		ctx->xoff = wp->xoff;
		ctx->yoff = wp->yoff;
		if (status_at_line(c) == 0)
			ctx->yoff += status_line_size(c->session);

		cmdfn(&c->tty, ctx);
	}
}

void
tty_cmd_insertcharacter(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;

	if (!tty_pane_full_width(tty, ctx) ||
	    tty_fake_bce(tty, wp, ctx->bg) ||
	    (!tty_term_has(tty->term, TTYC_ICH) &&
	    !tty_term_has(tty->term, TTYC_ICH1))) {
		tty_draw_pane(tty, wp, ctx->ocy, ctx->xoff, ctx->yoff);
		return;
	}

	tty_default_attributes(tty, wp, ctx->bg);

	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_ICH, TTYC_ICH1, ctx->num);
}

void
tty_cmd_deletecharacter(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;

	if (!tty_pane_full_width(tty, ctx) ||
	    tty_fake_bce(tty, wp, ctx->bg) ||
	    (!tty_term_has(tty->term, TTYC_DCH) &&
	    !tty_term_has(tty->term, TTYC_DCH1))) {
		tty_draw_pane(tty, wp, ctx->ocy, ctx->xoff, ctx->yoff);
		return;
	}

	tty_default_attributes(tty, wp, ctx->bg);

	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_DCH, TTYC_DCH1, ctx->num);
}

void
tty_cmd_clearcharacter(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_default_attributes(tty, ctx->wp, ctx->bg);

	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	if (tty_term_has(tty->term, TTYC_ECH) &&
	    !tty_fake_bce(tty, ctx->wp, 8))
		tty_putcode1(tty, TTYC_ECH, ctx->num);
	else
		tty_repeat_space(tty, ctx->num);
}

void
tty_cmd_insertline(struct tty *tty, const struct tty_ctx *ctx)
{
	if (!tty_pane_full_width(tty, ctx) ||
	    tty_fake_bce(tty, ctx->wp, ctx->bg) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    !tty_term_has(tty->term, TTYC_IL1)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, ctx->wp, ctx->bg);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_off(tty);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_IL, TTYC_IL1, ctx->num);
	tty->cx = tty->cy = UINT_MAX;
}

void
tty_cmd_deleteline(struct tty *tty, const struct tty_ctx *ctx)
{
	if (!tty_pane_full_width(tty, ctx) ||
	    tty_fake_bce(tty, ctx->wp, ctx->bg) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    !tty_term_has(tty->term, TTYC_DL1)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, ctx->wp, ctx->bg);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_off(tty);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_DL, TTYC_DL1, ctx->num);
	tty->cx = tty->cy = UINT_MAX;
}

void
tty_cmd_clearline(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	u_int			 nx, py = ctx->yoff + ctx->ocy;

	tty_default_attributes(tty, wp, ctx->bg);

	nx = screen_size_x(wp->screen);
	tty_clear_line(tty, wp, py, ctx->xoff, nx, ctx->bg);
}

void
tty_cmd_clearendofline(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	u_int			 nx, py = ctx->yoff + ctx->ocy;

	tty_default_attributes(tty, wp, ctx->bg);

	nx = screen_size_x(wp->screen) - ctx->ocx;
	tty_clear_line(tty, wp, py, ctx->xoff + ctx->ocx, nx, ctx->bg);
}

void
tty_cmd_clearstartofline(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	u_int			 py = ctx->yoff + ctx->ocy;

	tty_default_attributes(tty, wp, ctx->bg);

	tty_clear_line(tty, wp, py, ctx->xoff, ctx->ocx + 1, ctx->bg);
}

void
tty_cmd_reverseindex(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;

	if (ctx->ocy != ctx->orupper)
		return;

	if (!tty_pane_full_width(tty, ctx) ||
	    tty_fake_bce(tty, wp, 8) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    !tty_term_has(tty->term, TTYC_RI)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, wp, ctx->bg);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_off(tty);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->orupper);

	tty_putcode(tty, TTYC_RI);
}

void
tty_cmd_linefeed(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;

	if (ctx->ocy != ctx->orlower)
		return;

	if ((!tty_pane_full_width(tty, ctx) && !tty_use_margin(tty)) ||
	    tty_fake_bce(tty, wp, 8) ||
	    !tty_term_has(tty->term, TTYC_CSR)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, wp, ctx->bg);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_pane(tty, ctx);

	/*
	 * If we want to wrap a pane while using margins, the cursor needs to
	 * be exactly on the right of the region. If the cursor is entirely off
	 * the edge - move it back to the right. Some terminals are funny about
	 * this and insert extra spaces, so only use the right if margins are
	 * enabled.
	 */
	if (ctx->xoff + ctx->ocx > tty->rright) {
		if (!tty_use_margin(tty))
			tty_cursor(tty, 0, ctx->yoff + ctx->ocy);
		else
			tty_cursor(tty, tty->rright, ctx->yoff + ctx->ocy);
	} else
		tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_putc(tty, '\n');
}

void
tty_cmd_scrollup(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	u_int			 i;

	if ((!tty_pane_full_width(tty, ctx) && !tty_use_margin(tty)) ||
	    tty_fake_bce(tty, wp, 8) ||
	    !tty_term_has(tty->term, TTYC_CSR)) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, wp, ctx->bg);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_pane(tty, ctx);

	if (ctx->num == 1 || !tty_term_has(tty->term, TTYC_INDN)) {
		if (!tty_use_margin(tty))
			tty_cursor(tty, 0, tty->rlower);
		else
			tty_cursor(tty, tty->rright, tty->rlower);
		for (i = 0; i < ctx->num; i++)
			tty_putc(tty, '\n');
	} else {
		tty_cursor(tty, 0, tty->cy);
		tty_putcode1(tty, TTYC_INDN, ctx->num);
	}
}

void
tty_cmd_clearendofscreen(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	u_int			 px, py, nx, ny;

	tty_default_attributes(tty, wp, ctx->bg);

	tty_region_pane(tty, ctx, 0, screen_size_y(wp->screen) - 1);
	tty_margin_off(tty);

	px = ctx->xoff;
	nx = screen_size_x(wp->screen);
	py = ctx->yoff + ctx->ocy + 1;
	ny = screen_size_y(wp->screen) - ctx->ocy - 1;

	tty_clear_area(tty, wp, py, ny, px, nx, ctx->bg);

	px = ctx->xoff + ctx->ocx;
	nx = screen_size_x(wp->screen) - ctx->ocx;
	py = ctx->yoff + ctx->ocy;

	tty_clear_line(tty, wp, py, px, nx, ctx->bg);
}

void
tty_cmd_clearstartofscreen(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	u_int			 px, py, nx, ny;

	tty_default_attributes(tty, wp, ctx->bg);

	tty_region_pane(tty, ctx, 0, screen_size_y(wp->screen) - 1);
	tty_margin_off(tty);

	px = ctx->xoff;
	nx = screen_size_x(wp->screen);
	py = ctx->yoff;
	ny = ctx->ocy - 1;

	tty_clear_area(tty, wp, py, ny, px, nx, ctx->bg);

	px = ctx->xoff;
	nx = ctx->ocx + 1;
	py = ctx->yoff + ctx->ocy;

	tty_clear_line(tty, wp, py, px, nx, ctx->bg);
}

void
tty_cmd_clearscreen(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	u_int			 px, py, nx, ny;

	tty_default_attributes(tty, wp, ctx->bg);

	tty_region_pane(tty, ctx, 0, screen_size_y(wp->screen) - 1);
	tty_margin_off(tty);

	px = ctx->xoff;
	nx = screen_size_x(wp->screen);
	py = ctx->yoff;
	ny = screen_size_y(wp->screen);

	tty_clear_area(tty, wp, py, ny, px, nx, ctx->bg);
}

void
tty_cmd_alignmenttest(struct tty *tty, const struct tty_ctx *ctx)
{
	struct window_pane	*wp = ctx->wp;
	struct screen		*s = wp->screen;
	u_int			 i, j;

	tty_attributes(tty, &grid_default_cell, wp);

	tty_region_pane(tty, ctx, 0, screen_size_y(s) - 1);
	tty_margin_off(tty);

	for (j = 0; j < screen_size_y(s); j++) {
		tty_cursor_pane(tty, ctx, 0, j);
		for (i = 0; i < screen_size_x(s); i++)
			tty_putc(tty, 'E');
	}
}

void
tty_cmd_cell(struct tty *tty, const struct tty_ctx *ctx)
{
	if (ctx->xoff + ctx->ocx > tty->sx - 1 && ctx->ocy == ctx->orlower) {
		if (tty_pane_full_width(tty, ctx))
			tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
		else
			tty_margin_off(tty);
	}

	tty_cursor_pane_unless_wrap(tty, ctx, ctx->ocx, ctx->ocy);

	tty_cell(tty, ctx->cell, ctx->wp);
}

void
tty_cmd_cells(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_cursor_pane_unless_wrap(tty, ctx, ctx->ocx, ctx->ocy);

	tty_attributes(tty, ctx->cell, ctx->wp);
	tty_putn(tty, ctx->ptr, ctx->num, ctx->num);
}

void
tty_cmd_setselection(struct tty *tty, const struct tty_ctx *ctx)
{
	char	*buf;
	size_t	 off;

	if (!tty_term_has(tty->term, TTYC_MS))
		return;

	off = 4 * ((ctx->num + 2) / 3) + 1; /* storage for base64 */
	buf = xmalloc(off);

	b64_ntop(ctx->ptr, ctx->num, buf, off);
	tty_putcode_ptr2(tty, TTYC_MS, "", buf);

	free(buf);
}

void
tty_cmd_rawstring(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_add(tty, ctx->ptr, ctx->num);
	tty_invalidate(tty);
}

static void
tty_cell(struct tty *tty, const struct grid_cell *gc,
    const struct window_pane *wp)
{
	const struct grid_cell	*gcp;

	/* Skip last character if terminal is stupid. */
	if ((tty->term->flags & TERM_EARLYWRAP) &&
	    tty->cy == tty->sy - 1 &&
	    tty->cx == tty->sx - 1)
		return;

	/* If this is a padding character, do nothing. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;

	/* Set the attributes. */
	tty_attributes(tty, gc, wp);

	/* Get the cell and if ASCII write with putc to do ACS translation. */
	gcp = tty_check_codeset(tty, gc);
	if (gcp->data.size == 1) {
		if (*gcp->data.data < 0x20 || *gcp->data.data == 0x7f)
			return;
		tty_putc(tty, *gcp->data.data);
		return;
	}

	/* Write the data. */
	tty_putn(tty, gcp->data.data, gcp->data.size, gcp->data.width);
}

void
tty_reset(struct tty *tty)
{
	struct grid_cell	*gc = &tty->cell;

	if (!grid_cells_equal(gc, &grid_default_cell)) {
		if ((gc->attr & GRID_ATTR_CHARSET) && tty_acs_needed(tty))
			tty_putcode(tty, TTYC_RMACS);
		tty_putcode(tty, TTYC_SGR0);
		memcpy(gc, &grid_default_cell, sizeof *gc);
	}

	memcpy(&tty->last_cell, &grid_default_cell, sizeof tty->last_cell);
	tty->last_wp = -1;
}

static void
tty_invalidate(struct tty *tty)
{
	memcpy(&tty->cell, &grid_default_cell, sizeof tty->cell);

	memcpy(&tty->last_cell, &grid_default_cell, sizeof tty->last_cell);
	tty->last_wp = -1;

	tty->cx = tty->cy = UINT_MAX;

	tty->rupper = tty->rleft = UINT_MAX;
	tty->rlower = tty->rright = UINT_MAX;

	if (tty->flags & TTY_STARTED) {
		tty_putcode(tty, TTYC_SGR0);

		tty->mode = ALL_MODES;
		tty_update_mode(tty, MODE_CURSOR, NULL);

		tty_cursor(tty, 0, 0);
		tty_region_off(tty);
		tty_margin_off(tty);
	} else
		tty->mode = MODE_CURSOR;
}

/* Turn off margin. */
void
tty_region_off(struct tty *tty)
{
	tty_region(tty, 0, tty->sy - 1);
}

/* Set region inside pane. */
static void
tty_region_pane(struct tty *tty, const struct tty_ctx *ctx, u_int rupper,
    u_int rlower)
{
	tty_region(tty, ctx->yoff + rupper, ctx->yoff + rlower);
}

/* Set region at absolute position. */
static void
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

	tty_putcode2(tty, TTYC_CSR, tty->rupper, tty->rlower);
	tty->cx = tty->cy = UINT_MAX;
}

/* Turn off margin. */
void
tty_margin_off(struct tty *tty)
{
	tty_margin(tty, 0, tty->sx - 1);
}

/* Set margin inside pane. */
static void
tty_margin_pane(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_margin(tty, ctx->xoff, ctx->xoff + ctx->wp->sx - 1);
}

/* Set margin at absolute position. */
static void
tty_margin(struct tty *tty, u_int rleft, u_int rright)
{
	char s[64];

	if (!tty_use_margin(tty))
		return;
	if (tty->rleft == rleft && tty->rright == rright)
		return;

	tty_putcode2(tty, TTYC_CSR, tty->rupper, tty->rlower);

	tty->rleft = rleft;
	tty->rright = rright;

	if (rleft == 0 && rright == tty->sx - 1)
		snprintf(s, sizeof s, "\033[s");
	else
		snprintf(s, sizeof s, "\033[%u;%us", rleft + 1, rright + 1);
	tty_puts(tty, s);
	tty->cx = tty->cy = UINT_MAX;
}

/*
 * Move the cursor, unless it would wrap itself when the next character is
 * printed.
 */
static void
tty_cursor_pane_unless_wrap(struct tty *tty, const struct tty_ctx *ctx,
    u_int cx, u_int cy)
{
	if (!ctx->wrapped ||
	    !tty_pane_full_width(tty, ctx) ||
	    (tty->term->flags & TERM_EARLYWRAP) ||
	    ctx->xoff + cx != 0 ||
	    ctx->yoff + cy != tty->cy + 1 ||
	    tty->cx < tty->sx ||
	    tty->cy == tty->rlower)
		tty_cursor_pane(tty, ctx, cx, cy);
	else
		log_debug("%s: will wrap at %u,%u", __func__, tty->cx, tty->cy);
}

/* Move cursor inside pane. */
static void
tty_cursor_pane(struct tty *tty, const struct tty_ctx *ctx, u_int cx, u_int cy)
{
	tty_cursor(tty, ctx->xoff + cx, ctx->yoff + cy);
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
	if (cx == 0 && cy == thisy + 1 && thisy != tty->rlower &&
	    (!tty_use_margin(tty) || tty->rleft == 0)) {
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
		if (cx == 0 && (!tty_use_margin(tty) || tty->rleft == 0)) {
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
		if ((u_int) abs(change) > cx && tty_term_has(term, TTYC_HPA)) {
			tty_putcode1(tty, TTYC_HPA, cx);
			goto out;
		} else if (change > 0 && tty_term_has(term, TTYC_CUB)) {
			if (change == 2 && tty_term_has(term, TTYC_CUB1)) {
				tty_putcode(tty, TTYC_CUB1);
				tty_putcode(tty, TTYC_CUB1);
				goto out;
			}
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
		 * Try to use VPA if change is larger than absolute or if this
		 * change would cross the scroll region, otherwise use CUU/CUD.
		 */
		if ((u_int) abs(change) > cy ||
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
tty_attributes(struct tty *tty, const struct grid_cell *gc,
    const struct window_pane *wp)
{
	struct grid_cell	*tc = &tty->cell, gc2;
	int			 changed;

	/* Ignore cell if it is the same as the last one. */
	if (wp != NULL &&
	    (int)wp->id == tty->last_wp &&
	    ~(wp->window->flags & WINDOW_STYLECHANGED) &&
	    gc->attr == tty->last_cell.attr &&
	    gc->fg == tty->last_cell.fg &&
	    gc->bg == tty->last_cell.bg)
		return;
	tty->last_wp = (wp != NULL ? (int)wp->id : -1);
	memcpy(&tty->last_cell, gc, sizeof tty->last_cell);

	/* Copy cell and update default colours. */
	memcpy(&gc2, gc, sizeof gc2);
	if (wp != NULL)
		tty_default_colours(&gc2, wp);

	/*
	 * If no setab, try to use the reverse attribute as a best-effort for a
	 * non-default background. This is a bit of a hack but it doesn't do
	 * any serious harm and makes a couple of applications happier.
	 */
	if (!tty_term_has(tty->term, TTYC_SETAB)) {
		if (gc2.attr & GRID_ATTR_REVERSE) {
			if (gc2.fg != 7 && gc2.fg != 8)
				gc2.attr &= ~GRID_ATTR_REVERSE;
		} else {
			if (gc2.bg != 0 && gc2.bg != 8)
				gc2.attr |= GRID_ATTR_REVERSE;
		}
	}

	/* Fix up the colours if necessary. */
	tty_check_fg(tty, wp, &gc2);
	tty_check_bg(tty, wp, &gc2);

	/* If any bits are being cleared, reset everything. */
	if (tc->attr & ~gc2.attr)
		tty_reset(tty);

	/*
	 * Set the colours. This may call tty_reset() (so it comes next) and
	 * may add to (NOT remove) the desired attributes by changing new_attr.
	 */
	tty_colours(tty, &gc2);

	/* Filter out attribute bits already set. */
	changed = gc2.attr & ~tc->attr;
	tc->attr = gc2.attr;

	/* Set the attributes. */
	if (changed & GRID_ATTR_BRIGHT)
		tty_putcode(tty, TTYC_BOLD);
	if (changed & GRID_ATTR_DIM)
		tty_putcode(tty, TTYC_DIM);
	if (changed & GRID_ATTR_ITALICS)
		tty_set_italics(tty);
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
	if (changed & GRID_ATTR_STRIKETHROUGH)
		tty_putcode(tty, TTYC_SMXX);
	if ((changed & GRID_ATTR_CHARSET) && tty_acs_needed(tty))
		tty_putcode(tty, TTYC_SMACS);
}

static void
tty_colours(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;
	int			 have_ax;

	/* No changes? Nothing is necessary. */
	if (gc->fg == tc->fg && gc->bg == tc->bg)
		return;

	/*
	 * Is either the default colour? This is handled specially because the
	 * best solution might be to reset both colours to default, in which
	 * case if only one is default need to fall onward to set the other
	 * colour.
	 */
	if (gc->fg == 8 || gc->bg == 8) {
		/*
		 * If don't have AX but do have op, send sgr0 (op can't
		 * actually be used because it is sometimes the same as sgr0
		 * and sometimes isn't). This resets both colours to default.
		 *
		 * Otherwise, try to set the default colour only as needed.
		 */
		have_ax = tty_term_flag(tty->term, TTYC_AX);
		if (!have_ax && tty_term_has(tty->term, TTYC_OP))
			tty_reset(tty);
		else {
			if (gc->fg == 8 && tc->fg != 8) {
				if (have_ax)
					tty_puts(tty, "\033[39m");
				else if (tc->fg != 7)
					tty_putcode1(tty, TTYC_SETAF, 7);
				tc->fg = 8;
			}
			if (gc->bg == 8 && tc->bg != 8) {
				if (have_ax)
					tty_puts(tty, "\033[49m");
				else if (tc->bg != 0)
					tty_putcode1(tty, TTYC_SETAB, 0);
				tc->bg = 8;
			}
		}
	}

	/* Set the foreground colour. */
	if (gc->fg != 8 && gc->fg != tc->fg)
		tty_colours_fg(tty, gc);

	/*
	 * Set the background colour. This must come after the foreground as
	 * tty_colour_fg() can call tty_reset().
	 */
	if (gc->bg != 8 && gc->bg != tc->bg)
		tty_colours_bg(tty, gc);
}

static void
tty_check_fg(struct tty *tty, const struct window_pane *wp,
    struct grid_cell *gc)
{
	u_char	r, g, b;
	u_int	colours;
	int	c;

	/*
	 * Perform substitution if this pane has a palette. If the bright
	 * attribute is set, use the bright entry in the palette by changing to
	 * the aixterm colour.
	 */
	if (~gc->flags & GRID_FLAG_NOPALETTE) {
		c = gc->fg;
		if (c < 8 && gc->attr & GRID_ATTR_BRIGHT)
			c += 90;
		if ((c = window_pane_get_palette(wp, c)) != -1)
			gc->fg = c;
	}

	/* Is this a 24-bit colour? */
	if (gc->fg & COLOUR_FLAG_RGB) {
		/* Not a 24-bit terminal? Translate to 256-colour palette. */
		if (!tty_term_has(tty->term, TTYC_SETRGBF)) {
			colour_split_rgb(gc->fg, &r, &g, &b);
			gc->fg = colour_find_rgb(r, g, b);
		} else
			return;
	}

	/* How many colours does this terminal have? */
	if ((tty->term->flags|tty->term_flags) & TERM_256COLOURS)
		colours = 256;
	else
		colours = tty_term_number(tty->term, TTYC_COLORS);

	/* Is this a 256-colour colour? */
	if (gc->fg & COLOUR_FLAG_256) {
		/* And not a 256 colour mode? */
		if (colours != 256) {
			gc->fg = colour_256to16(gc->fg);
			if (gc->fg & 8) {
				gc->fg &= 7;
				if (colours >= 16)
					gc->fg += 90;
				else
					gc->attr |= GRID_ATTR_BRIGHT;
			} else
				gc->attr &= ~GRID_ATTR_BRIGHT;
		}
		return;
	}

	/* Is this an aixterm colour? */
	if (gc->fg >= 90 && gc->fg <= 97 && colours < 16) {
		gc->fg -= 90;
		gc->attr |= GRID_ATTR_BRIGHT;
	}
}

static void
tty_check_bg(struct tty *tty, const struct window_pane *wp,
    struct grid_cell *gc)
{
	u_char	r, g, b;
	u_int	colours;
	int	c;

	/* Perform substitution if this pane has a palette. */
	if (~gc->flags & GRID_FLAG_NOPALETTE) {
		if ((c = window_pane_get_palette(wp, gc->bg)) != -1)
			gc->bg = c;
	}

	/* Is this a 24-bit colour? */
	if (gc->bg & COLOUR_FLAG_RGB) {
		/* Not a 24-bit terminal? Translate to 256-colour palette. */
		if (!tty_term_has(tty->term, TTYC_SETRGBB)) {
			colour_split_rgb(gc->bg, &r, &g, &b);
			gc->bg = colour_find_rgb(r, g, b);
		} else
			return;
	}

	/* How many colours does this terminal have? */
	if ((tty->term->flags|tty->term_flags) & TERM_256COLOURS)
		colours = 256;
	else
		colours = tty_term_number(tty->term, TTYC_COLORS);

	/* Is this a 256-colour colour? */
	if (gc->bg & COLOUR_FLAG_256) {
		/*
		 * And not a 256 colour mode? Translate to 16-colour
		 * palette. Bold background doesn't exist portably, so just
		 * discard the bold bit if set.
		 */
		if (colours != 256) {
			gc->bg = colour_256to16(gc->bg);
			if (gc->bg & 8) {
				gc->bg &= 7;
				if (colours >= 16)
					gc->fg += 90;
			}
		}
		return;
	}

	/* Is this an aixterm colour? */
	if (gc->bg >= 90 && gc->bg <= 97 && colours < 16)
		gc->bg -= 90;
}

static void
tty_colours_fg(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;
	char			 s[32];

	/* Is this a 24-bit or 256-colour colour? */
	if (gc->fg & COLOUR_FLAG_RGB || gc->fg & COLOUR_FLAG_256) {
		if (tty_try_colour(tty, gc->fg, "38") == 0)
			goto save_fg;
		/* Should not get here, already converted in tty_check_fg. */
		return;
	}

	/* Is this an aixterm bright colour? */
	if (gc->fg >= 90 && gc->fg <= 97) {
		xsnprintf(s, sizeof s, "\033[%dm", gc->fg);
		tty_puts(tty, s);
		goto save_fg;
	}

	/* Otherwise set the foreground colour. */
	tty_putcode1(tty, TTYC_SETAF, gc->fg);

save_fg:
	/* Save the new values in the terminal current cell. */
	tc->fg = gc->fg;
}

static void
tty_colours_bg(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;
	char			 s[32];

	/* Is this a 24-bit or 256-colour colour? */
	if (gc->bg & COLOUR_FLAG_RGB || gc->bg & COLOUR_FLAG_256) {
		if (tty_try_colour(tty, gc->bg, "48") == 0)
			goto save_bg;
		/* Should not get here, already converted in tty_check_bg. */
		return;
	}

	/* Is this an aixterm bright colour? */
	if (gc->bg >= 90 && gc->bg <= 97) {
		xsnprintf(s, sizeof s, "\033[%dm", gc->bg + 10);
		tty_puts(tty, s);
		goto save_bg;
	}

	/* Otherwise set the background colour. */
	tty_putcode1(tty, TTYC_SETAB, gc->bg);

save_bg:
	/* Save the new values in the terminal current cell. */
	tc->bg = gc->bg;
}

static int
tty_try_colour(struct tty *tty, int colour, const char *type)
{
	u_char	r, g, b;
	char	s[32];

	if (colour & COLOUR_FLAG_256) {
		/*
		 * If the user has specified -2 to the client (meaning
		 * TERM_256COLOURS is set), setaf and setab may not work (or
		 * they may not want to use them), so send the usual sequence.
		 *
		 * Also if RGB is set, setaf and setab do not support the 256
		 * colour palette so use the sequences directly there too.
		 */
		if ((tty->term_flags & TERM_256COLOURS) ||
		    tty_term_has(tty->term, TTYC_RGB))
			goto fallback_256;

		/*
		 * If the terminfo entry has 256 colours and setaf and setab
		 * exist, assume that they work correctly.
		 */
		if (tty->term->flags & TERM_256COLOURS) {
			if (*type == '3') {
				if (!tty_term_has(tty->term, TTYC_SETAF))
					goto fallback_256;
				tty_putcode1(tty, TTYC_SETAF, colour & 0xff);
			} else {
				if (!tty_term_has(tty->term, TTYC_SETAB))
					goto fallback_256;
				tty_putcode1(tty, TTYC_SETAB, colour & 0xff);
			}
			return (0);
		}
		goto fallback_256;
	}

	if (colour & COLOUR_FLAG_RGB) {
		if (*type == '3') {
			if (!tty_term_has(tty->term, TTYC_SETRGBF))
				return (-1);
			colour_split_rgb(colour & 0xffffff, &r, &g, &b);
			tty_putcode3(tty, TTYC_SETRGBF, r, g, b);
		} else {
			if (!tty_term_has(tty->term, TTYC_SETRGBB))
				return (-1);
			colour_split_rgb(colour & 0xffffff, &r, &g, &b);
			tty_putcode3(tty, TTYC_SETRGBB, r, g, b);
		}
		return (0);
	}

	return (-1);

fallback_256:
	xsnprintf(s, sizeof s, "\033[%s;5;%dm", type, colour & 0xff);
	log_debug("%s: 256 colour fallback: %s", tty->client->name, s);
	tty_puts(tty, s);
	return (0);
}

static void
tty_default_colours(struct grid_cell *gc, const struct window_pane *wp)
{
	struct window		*w = wp->window;
	struct options		*oo = w->options;
	const struct grid_cell	*agc, *pgc, *wgc;
	int			 c;

	if (w->flags & WINDOW_STYLECHANGED) {
		w->flags &= ~WINDOW_STYLECHANGED;
		agc = options_get_style(oo, "window-active-style");
		memcpy(&w->active_style, agc, sizeof w->active_style);
		wgc = options_get_style(oo, "window-style");
		memcpy(&w->style, wgc, sizeof w->style);
	} else {
		agc = &w->active_style;
		wgc = &w->style;
	}
	pgc = &wp->colgc;

	if (gc->fg == 8) {
		if (pgc->fg != 8)
			gc->fg = pgc->fg;
		else if (wp == w->active && agc->fg != 8)
			gc->fg = agc->fg;
		else
			gc->fg = wgc->fg;

		if (gc->fg != 8 &&
		    (c = window_pane_get_palette(wp, gc->fg)) != -1)
			gc->fg = c;
	}

	if (gc->bg == 8) {
		if (pgc->bg != 8)
			gc->bg = pgc->bg;
		else if (wp == w->active && agc->bg != 8)
			gc->bg = agc->bg;
		else
			gc->bg = wgc->bg;

		if (gc->bg != 8 &&
		    (c = window_pane_get_palette(wp, gc->bg)) != -1)
			gc->bg = c;
	}
}

static void
tty_default_attributes(struct tty *tty, const struct window_pane *wp, u_int bg)
{
	static struct grid_cell gc;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.bg = bg;
	tty_attributes(tty, &gc, wp);
}
