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
#include <time.h>
#include <unistd.h>

#include "tmux.h"

static int	tty_log_fd = -1;

static void	tty_set_italics(struct tty *);
static int	tty_try_colour(struct tty *, int, const char *);
static void	tty_force_cursor_colour(struct tty *, int);
static void	tty_cursor_pane(struct tty *, const struct tty_ctx *, u_int,
		    u_int);
static void	tty_cursor_pane_unless_wrap(struct tty *,
		    const struct tty_ctx *, u_int, u_int);
static void	tty_colours(struct tty *, const struct grid_cell *);
static void	tty_check_fg(struct tty *, struct colour_palette *,
    		    struct grid_cell *);
static void	tty_check_bg(struct tty *, struct colour_palette *,
    		    struct grid_cell *);
static void	tty_check_us(struct tty *, struct colour_palette *,
    		    struct grid_cell *);
static void	tty_colours_fg(struct tty *, const struct grid_cell *);
static void	tty_colours_bg(struct tty *, const struct grid_cell *);
static void	tty_colours_us(struct tty *, const struct grid_cell *);

static void	tty_region_pane(struct tty *, const struct tty_ctx *, u_int,
		    u_int);
static void	tty_region(struct tty *, u_int, u_int);
static void	tty_margin_pane(struct tty *, const struct tty_ctx *);
static void	tty_margin(struct tty *, u_int, u_int);
static int	tty_large_region(struct tty *, const struct tty_ctx *);
static int	tty_fake_bce(const struct tty *, const struct grid_cell *,
		    u_int);
static void	tty_redraw_region(struct tty *, const struct tty_ctx *);
static void	tty_emulate_repeat(struct tty *, enum tty_code_code,
		    enum tty_code_code, u_int);
static void	tty_repeat_space(struct tty *, u_int);
static void	tty_draw_pane(struct tty *, const struct tty_ctx *, u_int);
static void	tty_default_attributes(struct tty *, const struct grid_cell *,
		    struct colour_palette *, u_int, struct hyperlinks *);
static int	tty_check_overlay(struct tty *, u_int, u_int);
static void	tty_check_overlay_range(struct tty *, u_int, u_int, u_int,
		    struct overlay_ranges *);

#ifdef ENABLE_SIXEL
static void	tty_write_one(void (*)(struct tty *, const struct tty_ctx *),
		    struct client *, struct tty_ctx *);
#endif

#define tty_use_margin(tty) \
	(tty->term->flags & TERM_DECSLRM)
#define tty_full_width(tty, ctx) \
	((ctx)->xoff == 0 && (ctx)->sx >= (tty)->sx)

#define TTY_BLOCK_INTERVAL (100000 /* 100 milliseconds */)
#define TTY_BLOCK_START(tty) (1 + ((tty)->sx * (tty)->sy) * 8)
#define TTY_BLOCK_STOP(tty) (1 + ((tty)->sx * (tty)->sy) / 8)

#define TTY_QUERY_TIMEOUT 5
#define TTY_REQUEST_LIMIT 30

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
tty_init(struct tty *tty, struct client *c)
{
	if (!isatty(c->fd))
		return (-1);

	memset(tty, 0, sizeof *tty);
	tty->client = c;

	tty->cstyle = SCREEN_CURSOR_DEFAULT;
	tty->ccolour = -1;
	tty->fg = tty->bg = -1;

	if (tcgetattr(c->fd, &tty->tio) != 0)
		return (-1);
	return (0);
}

void
tty_resize(struct tty *tty)
{
	struct client	*c = tty->client;
	struct winsize	 ws;
	u_int		 sx, sy, xpixel, ypixel;

	if (ioctl(c->fd, TIOCGWINSZ, &ws) != -1) {
		sx = ws.ws_col;
		if (sx == 0) {
			sx = 80;
			xpixel = 0;
		} else
			xpixel = ws.ws_xpixel / sx;
		sy = ws.ws_row;
		if (sy == 0) {
			sy = 24;
			ypixel = 0;
		} else
			ypixel = ws.ws_ypixel / sy;

		if ((xpixel == 0 || ypixel == 0) &&
		    tty->out != NULL &&
		    !(tty->flags & TTY_WINSIZEQUERY) &&
		    (tty->term->flags & TERM_VT100LIKE)) {
			tty_puts(tty, "\033[18t\033[14t");
			tty->flags |= TTY_WINSIZEQUERY;
		}
	} else {
		sx = 80;
		sy = 24;
		xpixel = 0;
		ypixel = 0;
	}
	log_debug("%s: %s now %ux%u (%ux%u)", __func__, c->name, sx, sy,
	    xpixel, ypixel);
	tty_set_size(tty, sx, sy, xpixel, ypixel);
	tty_invalidate(tty);
}

void
tty_set_size(struct tty *tty, u_int sx, u_int sy, u_int xpixel, u_int ypixel)
{
	tty->sx = sx;
	tty->sy = sy;
	tty->xpixel = xpixel;
	tty->ypixel = ypixel;
}

static void
tty_read_callback(__unused int fd, __unused short events, void *data)
{
	struct tty	*tty = data;
	struct client	*c = tty->client;
	const char	*name = c->name;
	size_t		 size = EVBUFFER_LENGTH(tty->in);
	int		 nread;

	nread = evbuffer_read(tty->in, c->fd, -1);
	if (nread == 0 || nread == -1) {
		if (nread == 0)
			log_debug("%s: read closed", name);
		else
			log_debug("%s: read error: %s", name, strerror(errno));
		event_del(&tty->event_in);
		server_client_lost(tty->client);
		return;
	}
	log_debug("%s: read %d bytes (already %zu)", name, nread, size);

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

	c->flags |= CLIENT_ALLREDRAWFLAGS;
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

	if (size == 0)
		tty->flags &= ~TTY_NOBLOCK;
	else if (tty->flags & TTY_NOBLOCK)
		return (0);

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

	nwrite = evbuffer_write(tty->out, c->fd);
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
	struct client	*c = tty->client;

	tty->term = tty_term_create(tty, c->term_name, c->term_caps,
	    c->term_ncaps, &c->term_features, cause);
	if (tty->term == NULL) {
		tty_close(tty);
		return (-1);
	}
	tty->flags |= TTY_OPENED;

	tty->flags &= ~(TTY_NOCURSOR|TTY_FREEZE|TTY_BLOCK|TTY_TIMER);

	event_set(&tty->event_in, c->fd, EV_PERSIST|EV_READ,
	    tty_read_callback, tty);
	tty->in = evbuffer_new();
	if (tty->in == NULL)
		fatal("out of memory");

	event_set(&tty->event_out, c->fd, EV_WRITE, tty_write_callback, tty);
	tty->out = evbuffer_new();
	if (tty->out == NULL)
		fatal("out of memory");

	evtimer_set(&tty->timer, tty_timer_callback, tty);

	tty_start_tty(tty);
	tty_keys_build(tty);

	return (0);
}

static void
tty_start_timer_callback(__unused int fd, __unused short events, void *data)
{
	struct tty	*tty = data;
	struct client	*c = tty->client;

	log_debug("%s: start timer fired", c->name);
	if ((tty->flags & (TTY_HAVEDA|TTY_HAVEDA2|TTY_HAVEXDA)) == 0)
		tty_update_features(tty);
	tty->flags |= TTY_ALL_REQUEST_FLAGS;
}

void
tty_start_tty(struct tty *tty)
{
	struct client	*c = tty->client;
	struct termios	 tio;
	struct timeval	 tv = { .tv_sec = TTY_QUERY_TIMEOUT };

	setblocking(c->fd, 0);
	event_add(&tty->event_in, NULL);

	memcpy(&tio, &tty->tio, sizeof tio);
	tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR|IGNCR|IMAXBEL|ISTRIP);
	tio.c_iflag |= IGNBRK;
	tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET);
	tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHONL|ECHOCTL|ECHOPRT|
	    ECHOKE|ISIG);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	if (tcsetattr(c->fd, TCSANOW, &tio) == 0)
		tcflush(c->fd, TCOFLUSH);

	tty_putcode(tty, TTYC_SMCUP);

	tty_putcode(tty, TTYC_SMKX);
	tty_putcode(tty, TTYC_CLEAR);

	if (tty_acs_needed(tty)) {
		log_debug("%s: using capabilities for ACS", c->name);
		tty_putcode(tty, TTYC_ENACS);
	} else
		log_debug("%s: using UTF-8 for ACS", c->name);

	tty_putcode(tty, TTYC_CNORM);
	if (tty_term_has(tty->term, TTYC_KMOUS)) {
		tty_puts(tty, "\033[?1000l\033[?1002l\033[?1003l");
		tty_puts(tty, "\033[?1006l\033[?1005l");
	}
	if (tty_term_has(tty->term, TTYC_ENBP))
		tty_putcode(tty, TTYC_ENBP);

	evtimer_set(&tty->start_timer, tty_start_timer_callback, tty);
	evtimer_add(&tty->start_timer, &tv);

	tty->flags |= TTY_STARTED;
	tty_invalidate(tty);

	if (tty->ccolour != -1)
		tty_force_cursor_colour(tty, -1);

	tty->mouse_drag_flag = 0;
	tty->mouse_drag_update = NULL;
	tty->mouse_drag_release = NULL;
}

void
tty_send_requests(struct tty *tty)
{
	if (~tty->flags & TTY_STARTED)
		return;

	if (tty->term->flags & TERM_VT100LIKE) {
		if (~tty->term->flags & TTY_HAVEDA)
			tty_puts(tty, "\033[c");
		if (~tty->flags & TTY_HAVEDA2)
			tty_puts(tty, "\033[>c");
		if (~tty->flags & TTY_HAVEXDA)
			tty_puts(tty, "\033[>q");
		tty_puts(tty, "\033]10;?\033\\");
		tty_puts(tty, "\033]11;?\033\\");
	} else
		tty->flags |= TTY_ALL_REQUEST_FLAGS;
	tty->last_requests = time(NULL);
}

void
tty_repeat_requests(struct tty *tty)
{
	time_t	t = time(NULL);

	if (~tty->flags & TTY_STARTED)
		return;

	if (t - tty->last_requests <= TTY_REQUEST_LIMIT)
		return;
	tty->last_requests = t;

	if (tty->term->flags & TERM_VT100LIKE) {
		tty_puts(tty, "\033]10;?\033\\");
		tty_puts(tty, "\033]11;?\033\\");
	}
}

void
tty_stop_tty(struct tty *tty)
{
	struct client	*c = tty->client;
	struct winsize	 ws;

	if (!(tty->flags & TTY_STARTED))
		return;
	tty->flags &= ~TTY_STARTED;

	evtimer_del(&tty->start_timer);

	event_del(&tty->timer);
	tty->flags &= ~TTY_BLOCK;

	event_del(&tty->event_in);
	event_del(&tty->event_out);

	/*
	 * Be flexible about error handling and try not kill the server just
	 * because the fd is invalid. Things like ssh -t can easily leave us
	 * with a dead tty.
	 */
	if (ioctl(c->fd, TIOCGWINSZ, &ws) == -1)
		return;
	if (tcsetattr(c->fd, TCSANOW, &tty->tio) == -1)
		return;

	tty_raw(tty, tty_term_string_ii(tty->term, TTYC_CSR, 0, ws.ws_row - 1));
	if (tty_acs_needed(tty))
		tty_raw(tty, tty_term_string(tty->term, TTYC_RMACS));
	tty_raw(tty, tty_term_string(tty->term, TTYC_SGR0));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMKX));
	tty_raw(tty, tty_term_string(tty->term, TTYC_CLEAR));
	if (tty->cstyle != SCREEN_CURSOR_DEFAULT) {
		if (tty_term_has(tty->term, TTYC_SE))
			tty_raw(tty, tty_term_string(tty->term, TTYC_SE));
		else if (tty_term_has(tty->term, TTYC_SS))
			tty_raw(tty, tty_term_string_i(tty->term, TTYC_SS, 0));
	}
	if (tty->ccolour != -1)
		tty_raw(tty, tty_term_string(tty->term, TTYC_CR));

	tty_raw(tty, tty_term_string(tty->term, TTYC_CNORM));
	if (tty_term_has(tty->term, TTYC_KMOUS)) {
		tty_raw(tty, "\033[?1000l\033[?1002l\033[?1003l");
		tty_raw(tty, "\033[?1006l\033[?1005l");
	}
	if (tty_term_has(tty->term, TTYC_DSBP))
		tty_raw(tty, tty_term_string(tty->term, TTYC_DSBP));

	if (tty->term->flags & TERM_VT100LIKE)
		tty_raw(tty, "\033[?7727l");
	tty_raw(tty, tty_term_string(tty->term, TTYC_DSFCS));
	tty_raw(tty, tty_term_string(tty->term, TTYC_DSEKS));

	if (tty_use_margin(tty))
		tty_raw(tty, tty_term_string(tty->term, TTYC_DSMG));
	tty_raw(tty, tty_term_string(tty->term, TTYC_RMCUP));

	setblocking(c->fd, 1);
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
}

void
tty_free(struct tty *tty)
{
	tty_close(tty);
}

void
tty_update_features(struct tty *tty)
{
	struct client	*c = tty->client;

	if (tty_apply_features(tty->term, c->term_features))
		tty_term_apply_overrides(tty->term);

	if (tty_use_margin(tty))
		tty_putcode(tty, TTYC_ENMG);
	if (options_get_number(global_options, "extended-keys"))
		tty_puts(tty, tty_term_string(tty->term, TTYC_ENEKS));
	if (options_get_number(global_options, "focus-events"))
		tty_puts(tty, tty_term_string(tty->term, TTYC_ENFCS));
	if (tty->term->flags & TERM_VT100LIKE)
		tty_puts(tty, "\033[?7727h");

	/*
	 * Features might have changed since the first draw during attach. For
	 * example, this happens when DA responses are received.
	 */
	server_redraw_client(c);

	tty_invalidate(tty);
}

void
tty_raw(struct tty *tty, const char *s)
{
	struct client	*c = tty->client;
	ssize_t		 n, slen;
	u_int		 i;

	slen = strlen(s);
	for (i = 0; i < 5; i++) {
		n = write(c->fd, s, slen);
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
tty_putcode_i(struct tty *tty, enum tty_code_code code, int a)
{
	if (a < 0)
		return;
	tty_puts(tty, tty_term_string_i(tty->term, code, a));
}

void
tty_putcode_ii(struct tty *tty, enum tty_code_code code, int a, int b)
{
	if (a < 0 || b < 0)
		return;
	tty_puts(tty, tty_term_string_ii(tty->term, code, a, b));
}

void
tty_putcode_iii(struct tty *tty, enum tty_code_code code, int a, int b, int c)
{
	if (a < 0 || b < 0 || c < 0)
		return;
	tty_puts(tty, tty_term_string_iii(tty->term, code, a, b, c));
}

void
tty_putcode_s(struct tty *tty, enum tty_code_code code, const char *a)
{
	if (a != NULL)
		tty_puts(tty, tty_term_string_s(tty->term, code, a));
}

void
tty_putcode_ss(struct tty *tty, enum tty_code_code code, const char *a,
    const char *b)
{
	if (a != NULL && b != NULL)
		tty_puts(tty, tty_term_string_ss(tty->term, code, a, b));
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

	if ((tty->term->flags & TERM_NOAM) &&
	    ch >= 0x20 && ch != 0x7f &&
	    tty->cy == tty->sy - 1 &&
	    tty->cx + 1 >= tty->sx)
		return;

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
			 * On !am terminals, force the cursor position to where
			 * we think it should be after a line wrap - this means
			 * it works on sensible terminals as well.
			 */
			if (tty->term->flags & TERM_NOAM)
				tty_putcode_ii(tty, TTYC_CUP, tty->cy, tty->cx);
		} else
			tty->cx++;
	}
}

void
tty_putn(struct tty *tty, const void *buf, size_t len, u_int width)
{
	if ((tty->term->flags & TERM_NOAM) &&
	    tty->cy == tty->sy - 1 &&
	    tty->cx + len >= tty->sx)
		len = tty->sx - tty->cx - 1;

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

void
tty_set_path(struct tty *tty, const char *title)
{
	if (!tty_term_has(tty->term, TTYC_SWD) ||
	    !tty_term_has(tty->term, TTYC_FSL))
		return;

	tty_putcode(tty, TTYC_SWD);
	tty_puts(tty, title);
	tty_putcode(tty, TTYC_FSL);
}

static void
tty_force_cursor_colour(struct tty *tty, int c)
{
	u_char	r, g, b;
	char	s[13];

	if (c != -1)
		c = colour_force_rgb(c);
	if (c == tty->ccolour)
		return;
	if (c == -1)
		tty_putcode(tty, TTYC_CR);
	else {
		colour_split_rgb(c, &r, &g, &b);
		xsnprintf(s, sizeof s, "rgb:%02hhx/%02hhx/%02hhx", r, g, b);
		tty_putcode_s(tty, TTYC_CS, s);
	}
	tty->ccolour = c;
}

static int
tty_update_cursor(struct tty *tty, int mode, struct screen *s)
{
	enum screen_cursor_style	cstyle;
	int				ccolour, changed, cmode = mode;

	/* Set cursor colour if changed. */
	if (s != NULL) {
		ccolour = s->ccolour;
		if (s->ccolour == -1)
			ccolour = s->default_ccolour;
		tty_force_cursor_colour(tty, ccolour);
	}

	/* If cursor is off, set as invisible. */
	if (~cmode & MODE_CURSOR) {
		if (tty->mode & MODE_CURSOR)
			tty_putcode(tty, TTYC_CIVIS);
		return (cmode);
	}

	/* Check if blinking or very visible flag changed or style changed. */
	if (s == NULL)
		cstyle = tty->cstyle;
	else {
		cstyle = s->cstyle;
		if (cstyle == SCREEN_CURSOR_DEFAULT) {
			if (~cmode & MODE_CURSOR_BLINKING_SET) {
				if (s->default_mode & MODE_CURSOR_BLINKING)
					cmode |= MODE_CURSOR_BLINKING;
				else
					cmode &= ~MODE_CURSOR_BLINKING;
			}
			cstyle = s->default_cstyle;
		}
	}

	/* If nothing changed, do nothing. */
	changed = cmode ^ tty->mode;
	if ((changed & CURSOR_MODES) == 0 && cstyle == tty->cstyle)
		return (cmode);

	/*
	 * Set cursor style. If an explicit style has been set with DECSCUSR,
	 * set it if supported, otherwise send cvvis for blinking styles.
	 *
	 * If no style, has been set (SCREEN_CURSOR_DEFAULT), then send cvvis
	 * if either the blinking or very visible flags are set.
	 */
	tty_putcode(tty, TTYC_CNORM);
	switch (cstyle) {
	case SCREEN_CURSOR_DEFAULT:
		if (tty->cstyle != SCREEN_CURSOR_DEFAULT) {
			if (tty_term_has(tty->term, TTYC_SE))
				tty_putcode(tty, TTYC_SE);
			else
				tty_putcode_i(tty, TTYC_SS, 0);
		}
		if (cmode & (MODE_CURSOR_BLINKING|MODE_CURSOR_VERY_VISIBLE))
			tty_putcode(tty, TTYC_CVVIS);
		break;
	case SCREEN_CURSOR_BLOCK:
		if (tty_term_has(tty->term, TTYC_SS)) {
			if (cmode & MODE_CURSOR_BLINKING)
				tty_putcode_i(tty, TTYC_SS, 1);
			else
				tty_putcode_i(tty, TTYC_SS, 2);
		} else if (cmode & MODE_CURSOR_BLINKING)
			tty_putcode(tty, TTYC_CVVIS);
		break;
	case SCREEN_CURSOR_UNDERLINE:
		if (tty_term_has(tty->term, TTYC_SS)) {
			if (cmode & MODE_CURSOR_BLINKING)
				tty_putcode_i(tty, TTYC_SS, 3);
			else
				tty_putcode_i(tty, TTYC_SS, 4);
		} else if (cmode & MODE_CURSOR_BLINKING)
			tty_putcode(tty, TTYC_CVVIS);
		break;
	case SCREEN_CURSOR_BAR:
		if (tty_term_has(tty->term, TTYC_SS)) {
			if (cmode & MODE_CURSOR_BLINKING)
				tty_putcode_i(tty, TTYC_SS, 5);
			else
				tty_putcode_i(tty, TTYC_SS, 6);
		} else if (cmode & MODE_CURSOR_BLINKING)
			tty_putcode(tty, TTYC_CVVIS);
		break;
	}
	tty->cstyle = cstyle;
	return (cmode);
 }

void
tty_update_mode(struct tty *tty, int mode, struct screen *s)
{
	struct tty_term	*term = tty->term;
	struct client	*c = tty->client;
	int		 changed;

	if (tty->flags & TTY_NOCURSOR)
		mode &= ~MODE_CURSOR;

	if (tty_update_cursor(tty, mode, s) & MODE_CURSOR_BLINKING)
		mode |= MODE_CURSOR_BLINKING;
	else
		mode &= ~MODE_CURSOR_BLINKING;

	changed = mode ^ tty->mode;
	if (log_get_level() != 0 && changed != 0) {
		log_debug("%s: current mode %s", c->name,
		    screen_mode_to_string(tty->mode));
		log_debug("%s: setting mode %s", c->name,
		    screen_mode_to_string(mode));
	}

	if ((changed & ALL_MOUSE_MODES) && tty_term_has(term, TTYC_KMOUS)) {
		/*
		 * If the mouse modes have changed, clear then all and apply
		 * again. There are differences in how terminals track the
		 * various bits.
		 */
		tty_puts(tty, "\033[?1006l\033[?1000l\033[?1002l\033[?1003l");
		if (mode & ALL_MOUSE_MODES)
			tty_puts(tty, "\033[?1006h");
		if (mode & MODE_MOUSE_ALL)
			tty_puts(tty, "\033[?1000h\033[?1002h\033[?1003h");
		else if (mode & MODE_MOUSE_BUTTON)
			tty_puts(tty, "\033[?1000h\033[?1002h");
		else if (mode & MODE_MOUSE_STANDARD)
			tty_puts(tty, "\033[?1000h");
	}
	tty->mode = mode;
}

static void
tty_emulate_repeat(struct tty *tty, enum tty_code_code code,
    enum tty_code_code code1, u_int n)
{
	if (tty_term_has(tty->term, code))
		tty_putcode_i(tty, code, n);
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

/* Is this window bigger than the terminal? */
int
tty_window_bigger(struct tty *tty)
{
	struct client	*c = tty->client;
	struct window	*w = c->session->curw->window;

	return (tty->sx < w->sx || tty->sy - status_line_size(c) < w->sy);
}

/* What offset should this window be drawn at? */
int
tty_window_offset(struct tty *tty, u_int *ox, u_int *oy, u_int *sx, u_int *sy)
{
	*ox = tty->oox;
	*oy = tty->ooy;
	*sx = tty->osx;
	*sy = tty->osy;

	return (tty->oflag);
}

/* What offset should this window be drawn at? */
static int
tty_window_offset1(struct tty *tty, u_int *ox, u_int *oy, u_int *sx, u_int *sy)
{
	struct client		*c = tty->client;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp = server_client_get_pane(c);
	u_int			 cx, cy, lines;

	lines = status_line_size(c);

	if (tty->sx >= w->sx && tty->sy - lines >= w->sy) {
		*ox = 0;
		*oy = 0;
		*sx = w->sx;
		*sy = w->sy;

		c->pan_window = NULL;
		return (0);
	}

	*sx = tty->sx;
	*sy = tty->sy - lines;

	if (c->pan_window == w) {
		if (*sx >= w->sx)
			c->pan_ox = 0;
		else if (c->pan_ox + *sx > w->sx)
			c->pan_ox = w->sx - *sx;
		*ox = c->pan_ox;
		if (*sy >= w->sy)
			c->pan_oy = 0;
		else if (c->pan_oy + *sy > w->sy)
			c->pan_oy = w->sy - *sy;
		*oy = c->pan_oy;
		return (1);
	}

	if (~wp->screen->mode & MODE_CURSOR) {
		*ox = 0;
		*oy = 0;
	} else {
		cx = wp->xoff + wp->screen->cx;
		cy = wp->yoff + wp->screen->cy;

		if (cx < *sx)
			*ox = 0;
		else if (cx > w->sx - *sx)
			*ox = w->sx - *sx;
		else
			*ox = cx - *sx / 2;

		if (cy < *sy)
			*oy = 0;
		else if (cy > w->sy - *sy)
			*oy = w->sy - *sy;
		else
			*oy = cy - *sy / 2;
	}

	c->pan_window = NULL;
	return (1);
}

/* Update stored offsets for a window and redraw if necessary. */
void
tty_update_window_offset(struct window *w)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL &&
		    c->session->curw != NULL &&
		    c->session->curw->window == w)
			tty_update_client_offset(c);
	}
}

/* Update stored offsets for a client and redraw if necessary. */
void
tty_update_client_offset(struct client *c)
{
	u_int	ox, oy, sx, sy;

	if (~c->flags & CLIENT_TERMINAL)
		return;

	c->tty.oflag = tty_window_offset1(&c->tty, &ox, &oy, &sx, &sy);
	if (ox == c->tty.oox &&
	    oy == c->tty.ooy &&
	    sx == c->tty.osx &&
	    sy == c->tty.osy)
		return;

	log_debug ("%s: %s offset has changed (%u,%u %ux%u -> %u,%u %ux%u)",
	    __func__, c->name, c->tty.oox, c->tty.ooy, c->tty.osx, c->tty.osy,
	    ox, oy, sx, sy);

	c->tty.oox = ox;
	c->tty.ooy = oy;
	c->tty.osx = sx;
	c->tty.osy = sy;

	c->flags |= (CLIENT_REDRAWWINDOW|CLIENT_REDRAWSTATUS);
}

/*
 * Is the region large enough to be worth redrawing once later rather than
 * probably several times now? Currently yes if it is more than 50% of the
 * pane.
 */
static int
tty_large_region(__unused struct tty *tty, const struct tty_ctx *ctx)
{
	return (ctx->orlower - ctx->orupper >= ctx->sy / 2);
}

/*
 * Return if BCE is needed but the terminal doesn't have it - it'll need to be
 * emulated.
 */
static int
tty_fake_bce(const struct tty *tty, const struct grid_cell *gc, u_int bg)
{
	if (tty_term_flag(tty->term, TTYC_BCE))
		return (0);
	if (!COLOUR_DEFAULT(bg) || !COLOUR_DEFAULT(gc->bg))
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
	struct client		*c = tty->client;
	u_int			 i;

	/*
	 * If region is large, schedule a redraw. In most cases this is likely
	 * to be followed by some more scrolling.
	 */
	if (tty_large_region(tty, ctx)) {
		log_debug("%s: %s large redraw", __func__, c->name);
		ctx->redraw_cb(ctx);
		return;
	}

	for (i = ctx->orupper; i <= ctx->orlower; i++)
		tty_draw_pane(tty, ctx, i);
}

/* Is this position visible in the pane? */
static int
tty_is_visible(__unused struct tty *tty, const struct tty_ctx *ctx, u_int px,
    u_int py, u_int nx, u_int ny)
{
	u_int	xoff = ctx->rxoff + px, yoff = ctx->ryoff + py;

	if (!ctx->bigger)
		return (1);

	if (xoff + nx <= ctx->wox || xoff >= ctx->wox + ctx->wsx ||
	    yoff + ny <= ctx->woy || yoff >= ctx->woy + ctx->wsy)
		return (0);
	return (1);
}

/* Clamp line position to visible part of pane. */
static int
tty_clamp_line(struct tty *tty, const struct tty_ctx *ctx, u_int px, u_int py,
    u_int nx, u_int *i, u_int *x, u_int *rx, u_int *ry)
{
	u_int	xoff = ctx->rxoff + px;

	if (!tty_is_visible(tty, ctx, px, py, nx, 1))
		return (0);
	*ry = ctx->yoff + py - ctx->woy;

	if (xoff >= ctx->wox && xoff + nx <= ctx->wox + ctx->wsx) {
		/* All visible. */
		*i = 0;
		*x = ctx->xoff + px - ctx->wox;
		*rx = nx;
	} else if (xoff < ctx->wox && xoff + nx > ctx->wox + ctx->wsx) {
		/* Both left and right not visible. */
		*i = ctx->wox;
		*x = 0;
		*rx = ctx->wsx;
	} else if (xoff < ctx->wox) {
		/* Left not visible. */
		*i = ctx->wox - (ctx->xoff + px);
		*x = 0;
		*rx = nx - *i;
	} else {
		/* Right not visible. */
		*i = 0;
		*x = (ctx->xoff + px) - ctx->wox;
		*rx = ctx->wsx - *x;
	}
	if (*rx > nx)
		fatalx("%s: x too big, %u > %u", __func__, *rx, nx);

	return (1);
}

/* Clear a line. */
static void
tty_clear_line(struct tty *tty, const struct grid_cell *defaults, u_int py,
    u_int px, u_int nx, u_int bg)
{
	struct client		*c = tty->client;
	struct overlay_ranges	 r;
	u_int			 i;

	log_debug("%s: %s, %u at %u,%u", __func__, c->name, nx, px, py);

	/* Nothing to clear. */
	if (nx == 0)
		return;

	/* If genuine BCE is available, can try escape sequences. */
	if (c->overlay_check == NULL && !tty_fake_bce(tty, defaults, bg)) {
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
			tty_putcode_i(tty, TTYC_ECH, nx);
			return;
		}
	}

	/*
	 * Couldn't use an escape sequence, use spaces. Clear only the visible
	 * bit if there is an overlay.
	 */
	tty_check_overlay_range(tty, px, py, nx, &r);
	for (i = 0; i < OVERLAY_MAX_RANGES; i++) {
		if (r.nx[i] == 0)
			continue;
		tty_cursor(tty, r.px[i], py);
		tty_repeat_space(tty, r.nx[i]);
	}
}

/* Clear a line, adjusting to visible part of pane. */
static void
tty_clear_pane_line(struct tty *tty, const struct tty_ctx *ctx, u_int py,
    u_int px, u_int nx, u_int bg)
{
	struct client	*c = tty->client;
	u_int		 i, x, rx, ry;

	log_debug("%s: %s, %u at %u,%u", __func__, c->name, nx, px, py);

	if (tty_clamp_line(tty, ctx, px, py, nx, &i, &x, &rx, &ry))
		tty_clear_line(tty, &ctx->defaults, ry, x, rx, bg);
}

/* Clamp area position to visible part of pane. */
static int
tty_clamp_area(struct tty *tty, const struct tty_ctx *ctx, u_int px, u_int py,
    u_int nx, u_int ny, u_int *i, u_int *j, u_int *x, u_int *y, u_int *rx,
    u_int *ry)
{
	u_int	xoff = ctx->rxoff + px, yoff = ctx->ryoff + py;

	if (!tty_is_visible(tty, ctx, px, py, nx, ny))
		return (0);

	if (xoff >= ctx->wox && xoff + nx <= ctx->wox + ctx->wsx) {
		/* All visible. */
		*i = 0;
		*x = ctx->xoff + px - ctx->wox;
		*rx = nx;
	} else if (xoff < ctx->wox && xoff + nx > ctx->wox + ctx->wsx) {
		/* Both left and right not visible. */
		*i = ctx->wox;
		*x = 0;
		*rx = ctx->wsx;
	} else if (xoff < ctx->wox) {
		/* Left not visible. */
		*i = ctx->wox - (ctx->xoff + px);
		*x = 0;
		*rx = nx - *i;
	} else {
		/* Right not visible. */
		*i = 0;
		*x = (ctx->xoff + px) - ctx->wox;
		*rx = ctx->wsx - *x;
	}
	if (*rx > nx)
		fatalx("%s: x too big, %u > %u", __func__, *rx, nx);

	if (yoff >= ctx->woy && yoff + ny <= ctx->woy + ctx->wsy) {
		/* All visible. */
		*j = 0;
		*y = ctx->yoff + py - ctx->woy;
		*ry = ny;
	} else if (yoff < ctx->woy && yoff + ny > ctx->woy + ctx->wsy) {
		/* Both top and bottom not visible. */
		*j = ctx->woy;
		*y = 0;
		*ry = ctx->wsy;
	} else if (yoff < ctx->woy) {
		/* Top not visible. */
		*j = ctx->woy - (ctx->yoff + py);
		*y = 0;
		*ry = ny - *j;
	} else {
		/* Bottom not visible. */
		*j = 0;
		*y = (ctx->yoff + py) - ctx->woy;
		*ry = ctx->wsy - *y;
	}
	if (*ry > ny)
		fatalx("%s: y too big, %u > %u", __func__, *ry, ny);

	return (1);
}

/* Clear an area, adjusting to visible part of pane. */
static void
tty_clear_area(struct tty *tty, const struct grid_cell *defaults, u_int py,
    u_int ny, u_int px, u_int nx, u_int bg)
{
	struct client	*c = tty->client;
	u_int		 yy;
	char		 tmp[64];

	log_debug("%s: %s, %u,%u at %u,%u", __func__, c->name, nx, ny, px, py);

	/* Nothing to clear. */
	if (nx == 0 || ny == 0)
		return;

	/* If genuine BCE is available, can try escape sequences. */
	if (c->overlay_check == NULL && !tty_fake_bce(tty, defaults, bg)) {
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
		if ((tty->term->flags & TERM_DECFRA) && !COLOUR_DEFAULT(bg)) {
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
			tty_putcode_i(tty, TTYC_INDN, ny);
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
			tty_putcode_i(tty, TTYC_INDN, ny);
			return;
		}
	}

	/* Couldn't use an escape sequence, loop over the lines. */
	for (yy = py; yy < py + ny; yy++)
		tty_clear_line(tty, defaults, yy, px, nx, bg);
}

/* Clear an area in a pane. */
static void
tty_clear_pane_area(struct tty *tty, const struct tty_ctx *ctx, u_int py,
    u_int ny, u_int px, u_int nx, u_int bg)
{
	u_int	i, j, x, y, rx, ry;

	if (tty_clamp_area(tty, ctx, px, py, nx, ny, &i, &j, &x, &y, &rx, &ry))
		tty_clear_area(tty, &ctx->defaults, y, ry, x, rx, bg);
}

static void
tty_draw_pane(struct tty *tty, const struct tty_ctx *ctx, u_int py)
{
	struct screen	*s = ctx->s;
	u_int		 nx = ctx->sx, i, x, rx, ry;

	log_debug("%s: %s %u %d", __func__, tty->client->name, py, ctx->bigger);

	if (!ctx->bigger) {
		tty_draw_line(tty, s, 0, py, nx, ctx->xoff, ctx->yoff + py,
		    &ctx->defaults, ctx->palette);
		return;
	}
	if (tty_clamp_line(tty, ctx, 0, py, nx, &i, &x, &rx, &ry)) {
		tty_draw_line(tty, s, i, py, rx, x, ry, &ctx->defaults,
		    ctx->palette);
	}
}

static const struct grid_cell *
tty_check_codeset(struct tty *tty, const struct grid_cell *gc)
{
	static struct grid_cell	new;
	int			c;

	/* Characters less than 0x7f are always fine, no matter what. */
	if (gc->data.size == 1 && *gc->data.data < 0x7f)
		return (gc);
	if (gc->flags & GRID_FLAG_TAB)
		return (gc);

	/* UTF-8 terminal and a UTF-8 character - fine. */
	if (tty->client->flags & CLIENT_UTF8)
		return (gc);
	memcpy(&new, gc, sizeof new);

	/* See if this can be mapped to an ACS character. */
	c = tty_acs_reverse_get(tty, gc->data.data, gc->data.size);
	if (c != -1) {
		utf8_set(&new.data, c);
		new.attr |= GRID_ATTR_CHARSET;
		return (&new);
	}

	/* Replace by the right number of underscores. */
	new.data.size = gc->data.width;
	if (new.data.size > UTF8_SIZE)
		new.data.size = UTF8_SIZE;
	memset(new.data.data, '_', new.data.size);
	return (&new);
}

/*
 * Check if a single character is obstructed by the overlay and return a
 * boolean.
 */
static int
tty_check_overlay(struct tty *tty, u_int px, u_int py)
{
	struct overlay_ranges	r;

	/*
	 * A unit width range will always return nx[2] == 0 from a check, even
	 * with multiple overlays, so it's sufficient to check just the first
	 * two entries.
	 */
	tty_check_overlay_range(tty, px, py, 1, &r);
	if (r.nx[0] + r.nx[1] == 0)
		return (0);
	return (1);
}

/* Return parts of the input range which are visible. */
static void
tty_check_overlay_range(struct tty *tty, u_int px, u_int py, u_int nx,
    struct overlay_ranges *r)
{
	struct client	*c = tty->client;

	if (c->overlay_check == NULL) {
		r->px[0] = px;
		r->nx[0] = nx;
		r->px[1] = 0;
		r->nx[1] = 0;
		r->px[2] = 0;
		r->nx[2] = 0;
		return;
	}

	c->overlay_check(c, c->overlay_data, px, py, nx, r);
}

void
tty_draw_line(struct tty *tty, struct screen *s, u_int px, u_int py, u_int nx,
    u_int atx, u_int aty, const struct grid_cell *defaults,
    struct colour_palette *palette)
{
	struct grid		*gd = s->grid;
	struct grid_cell	 gc, last;
	const struct grid_cell	*gcp;
	struct grid_line	*gl;
	struct client		*c = tty->client;
	struct overlay_ranges	 r;
	u_int			 i, j, ux, sx, width, hidden, eux, nxx;
	u_int			 cellsize;
	int			 flags, cleared = 0, wrapped = 0;
	char			 buf[512];
	size_t			 len;

	log_debug("%s: px=%u py=%u nx=%u atx=%u aty=%u", __func__,
	    px, py, nx, atx, aty);
	log_debug("%s: defaults: fg=%d, bg=%d", __func__, defaults->fg,
	    defaults->bg);

	/*
	 * py is the line in the screen to draw.
	 * px is the start x and nx is the width to draw.
	 * atx,aty is the line on the terminal to draw it.
	 */

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
	if (nx > sx)
		nx = sx;
	cellsize = grid_get_line(gd, gd->hsize + py)->cellsize;
	if (sx > cellsize)
		sx = cellsize;
	if (sx > tty->sx)
		sx = tty->sx;
	if (sx > nx)
		sx = nx;
	ux = 0;

	if (py == 0)
		gl = NULL;
	else
		gl = grid_get_line(gd, gd->hsize + py - 1);
	if (gl == NULL ||
	    (~gl->flags & GRID_LINE_WRAPPED) ||
	    atx != 0 ||
	    tty->cx < tty->sx ||
	    nx < tty->sx) {
		if (nx < tty->sx &&
		    atx == 0 &&
		    px + sx != nx &&
		    tty_term_has(tty->term, TTYC_EL1) &&
		    !tty_fake_bce(tty, defaults, 8) &&
		    c->overlay_check == NULL) {
			tty_default_attributes(tty, defaults, palette, 8,
			    s->hyperlinks);
			tty_cursor(tty, nx - 1, aty);
			tty_putcode(tty, TTYC_EL1);
			cleared = 1;
		}
	} else {
		log_debug("%s: wrapped line %u", __func__, aty);
		wrapped = 1;
	}

	memcpy(&last, &grid_default_cell, sizeof last);
	len = 0;
	width = 0;

	for (i = 0; i < sx; i++) {
		grid_view_get_cell(gd, px + i, py, &gc);
		gcp = tty_check_codeset(tty, &gc);
		if (len != 0 &&
		    (!tty_check_overlay(tty, atx + ux + width, aty) ||
		    (gcp->attr & GRID_ATTR_CHARSET) ||
		    gcp->flags != last.flags ||
		    gcp->attr != last.attr ||
		    gcp->fg != last.fg ||
		    gcp->bg != last.bg ||
		    gcp->us != last.us ||
		    gcp->link != last.link ||
		    ux + width + gcp->data.width > nx ||
		    (sizeof buf) - len < gcp->data.size)) {
			tty_attributes(tty, &last, defaults, palette,
			    s->hyperlinks);
			if (last.flags & GRID_FLAG_CLEARED) {
				log_debug("%s: %zu cleared", __func__, len);
				tty_clear_line(tty, defaults, aty, atx + ux,
				    width, last.bg);
			} else {
				if (!wrapped || atx != 0 || ux != 0)
					tty_cursor(tty, atx + ux, aty);
				tty_putn(tty, buf, len, width);
			}
			ux += width;

			len = 0;
			width = 0;
			wrapped = 0;
		}

		if (gcp->flags & GRID_FLAG_SELECTED)
			screen_select_cell(s, &last, gcp);
		else
			memcpy(&last, gcp, sizeof last);

		tty_check_overlay_range(tty, atx + ux, aty, gcp->data.width,
		    &r);
		hidden = 0;
		for (j = 0; j < OVERLAY_MAX_RANGES; j++)
			hidden += r.nx[j];
		hidden = gcp->data.width - hidden;
		if (hidden != 0 && hidden == gcp->data.width) {
			if (~gcp->flags & GRID_FLAG_PADDING)
				ux += gcp->data.width;
		} else if (hidden != 0 || ux + gcp->data.width > nx) {
			if (~gcp->flags & GRID_FLAG_PADDING) {
				tty_attributes(tty, &last, defaults, palette,
				    s->hyperlinks);
				for (j = 0; j < OVERLAY_MAX_RANGES; j++) {
					if (r.nx[j] == 0)
						continue;
					/* Effective width drawn so far. */
					eux = r.px[j] - atx;
					if (eux < nx) {
						tty_cursor(tty, r.px[j], aty);
						nxx = nx - eux;
						if (r.nx[j] > nxx)
							r.nx[j] = nxx;
						tty_repeat_space(tty, r.nx[j]);
						ux = eux + r.nx[j];
					}
				}
			}
		} else if (gcp->attr & GRID_ATTR_CHARSET) {
			tty_attributes(tty, &last, defaults, palette,
			    s->hyperlinks);
			tty_cursor(tty, atx + ux, aty);
			for (j = 0; j < gcp->data.size; j++)
				tty_putc(tty, gcp->data.data[j]);
			ux += gcp->data.width;
		} else if (~gcp->flags & GRID_FLAG_PADDING) {
			memcpy(buf + len, gcp->data.data, gcp->data.size);
			len += gcp->data.size;
			width += gcp->data.width;
		}
	}
	if (len != 0 && ((~last.flags & GRID_FLAG_CLEARED) || last.bg != 8)) {
		tty_attributes(tty, &last, defaults, palette, s->hyperlinks);
		if (last.flags & GRID_FLAG_CLEARED) {
			log_debug("%s: %zu cleared (end)", __func__, len);
			tty_clear_line(tty, defaults, aty, atx + ux, width,
			    last.bg);
		} else {
			if (!wrapped || atx != 0 || ux != 0)
				tty_cursor(tty, atx + ux, aty);
			tty_putn(tty, buf, len, width);
		}
		ux += width;
	}

	if (!cleared && ux < nx) {
		log_debug("%s: %u to end of line (%zu cleared)", __func__,
		    nx - ux, len);
		tty_default_attributes(tty, defaults, palette, 8,
		    s->hyperlinks);
		tty_clear_line(tty, defaults, aty, atx + ux, nx - ux, 8);
	}

	tty->flags = (tty->flags & ~TTY_NOCURSOR) | flags;
	tty_update_mode(tty, tty->mode, s);
}

#ifdef ENABLE_SIXEL
/* Update context for client. */
static int
tty_set_client_cb(struct tty_ctx *ttyctx, struct client *c)
{
	struct window_pane	*wp = ttyctx->arg;

	if (c->session->curw->window != wp->window)
		return (0);
	if (wp->layout_cell == NULL)
		return (0);

	/* Set the properties relevant to the current client. */
	ttyctx->bigger = tty_window_offset(&c->tty, &ttyctx->wox, &ttyctx->woy,
	    &ttyctx->wsx, &ttyctx->wsy);

	ttyctx->yoff = ttyctx->ryoff = wp->yoff;
	if (status_at_line(c) == 0)
		ttyctx->yoff += status_line_size(c);

	return (1);
}

void
tty_draw_images(struct client *c, struct window_pane *wp, struct screen *s)
{
	struct image	*im;
	struct tty_ctx	 ttyctx;

	TAILQ_FOREACH(im, &s->images, entry) {
		memset(&ttyctx, 0, sizeof ttyctx);

		/* Set the client independent properties. */
		ttyctx.ocx = im->px;
		ttyctx.ocy = im->py;

		ttyctx.orlower = s->rlower;
		ttyctx.orupper = s->rupper;

		ttyctx.xoff = ttyctx.rxoff = wp->xoff;
		ttyctx.sx = wp->sx;
		ttyctx.sy = wp->sy;

		ttyctx.ptr = im;
		ttyctx.arg = wp;
		ttyctx.set_client_cb = tty_set_client_cb;
		ttyctx.allow_invisible_panes = 1;
		tty_write_one(tty_cmd_sixelimage, c, &ttyctx);
	}
}
#endif

void
tty_sync_start(struct tty *tty)
{
	if (tty->flags & TTY_BLOCK)
		return;
	if (tty->flags & TTY_SYNCING)
		return;
	tty->flags |= TTY_SYNCING;

	if (tty_term_has(tty->term, TTYC_SYNC)) {
		log_debug("%s sync start", tty->client->name);
		tty_putcode_i(tty, TTYC_SYNC, 1);
	}
}

void
tty_sync_end(struct tty *tty)
{
	if (tty->flags & TTY_BLOCK)
		return;
	if (~tty->flags & TTY_SYNCING)
		return;
	tty->flags &= ~TTY_SYNCING;

	if (tty_term_has(tty->term, TTYC_SYNC)) {
 		log_debug("%s sync end", tty->client->name);
		tty_putcode_i(tty, TTYC_SYNC, 2);
	}
}

static int
tty_client_ready(const struct tty_ctx *ctx, struct client *c)
{
	if (c->session == NULL || c->tty.term == NULL)
		return (0);
	if (c->flags & CLIENT_SUSPENDED)
		return (0);

	/*
	 * If invisible panes are allowed (used for passthrough), don't care if
	 * redrawing or frozen.
	 */
	if (ctx->allow_invisible_panes)
		return (1);

	if (c->flags & CLIENT_REDRAWWINDOW)
		return (0);
	if (c->tty.flags & TTY_FREEZE)
		return (0);
	return (1);
}

void
tty_write(void (*cmdfn)(struct tty *, const struct tty_ctx *),
    struct tty_ctx *ctx)
{
	struct client	*c;
	int		 state;

	if (ctx->set_client_cb == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (tty_client_ready(ctx, c)) {
			state = ctx->set_client_cb(ctx, c);
			if (state == -1)
				break;
			if (state == 0)
				continue;
			cmdfn(&c->tty, ctx);
		}
	}
}

#ifdef ENABLE_SIXEL
/* Only write to the incoming tty instead of every client. */
static void
tty_write_one(void (*cmdfn)(struct tty *, const struct tty_ctx *),
    struct client *c, struct tty_ctx *ctx)
{
	if (ctx->set_client_cb == NULL)
		return;
	if ((ctx->set_client_cb(ctx, c)) == 1)
		cmdfn(&c->tty, ctx);
}
#endif

void
tty_cmd_insertcharacter(struct tty *tty, const struct tty_ctx *ctx)
{
	struct client	*c = tty->client;

	if (ctx->bigger ||
	    !tty_full_width(tty, ctx) ||
	    tty_fake_bce(tty, &ctx->defaults, ctx->bg) ||
	    (!tty_term_has(tty->term, TTYC_ICH) &&
	    !tty_term_has(tty->term, TTYC_ICH1)) ||
	    c->overlay_check != NULL) {
		tty_draw_pane(tty, ctx, ctx->ocy);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_ICH, TTYC_ICH1, ctx->num);
}

void
tty_cmd_deletecharacter(struct tty *tty, const struct tty_ctx *ctx)
{
	struct client	*c = tty->client;

	if (ctx->bigger ||
	    !tty_full_width(tty, ctx) ||
	    tty_fake_bce(tty, &ctx->defaults, ctx->bg) ||
	    (!tty_term_has(tty->term, TTYC_DCH) &&
	    !tty_term_has(tty->term, TTYC_DCH1)) ||
	    c->overlay_check != NULL) {
		tty_draw_pane(tty, ctx, ctx->ocy);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_DCH, TTYC_DCH1, ctx->num);
}

void
tty_cmd_clearcharacter(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_clear_pane_line(tty, ctx, ctx->ocy, ctx->ocx, ctx->num, ctx->bg);
}

void
tty_cmd_insertline(struct tty *tty, const struct tty_ctx *ctx)
{
	struct client	*c = tty->client;

	if (ctx->bigger ||
	    !tty_full_width(tty, ctx) ||
	    tty_fake_bce(tty, &ctx->defaults, ctx->bg) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    !tty_term_has(tty->term, TTYC_IL1) ||
	    ctx->sx == 1 ||
	    ctx->sy == 1 ||
	    c->overlay_check != NULL) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_off(tty);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_IL, TTYC_IL1, ctx->num);
	tty->cx = tty->cy = UINT_MAX;
}

void
tty_cmd_deleteline(struct tty *tty, const struct tty_ctx *ctx)
{
	struct client	*c = tty->client;

	if (ctx->bigger ||
	    !tty_full_width(tty, ctx) ||
	    tty_fake_bce(tty, &ctx->defaults, ctx->bg) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    !tty_term_has(tty->term, TTYC_DL1) ||
	    ctx->sx == 1 ||
	    ctx->sy == 1 ||
	    c->overlay_check != NULL) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_off(tty);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->ocy);

	tty_emulate_repeat(tty, TTYC_DL, TTYC_DL1, ctx->num);
	tty->cx = tty->cy = UINT_MAX;
}

void
tty_cmd_clearline(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_clear_pane_line(tty, ctx, ctx->ocy, 0, ctx->sx, ctx->bg);
}

void
tty_cmd_clearendofline(struct tty *tty, const struct tty_ctx *ctx)
{
	u_int	nx = ctx->sx - ctx->ocx;

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_clear_pane_line(tty, ctx, ctx->ocy, ctx->ocx, nx, ctx->bg);
}

void
tty_cmd_clearstartofline(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_clear_pane_line(tty, ctx, ctx->ocy, 0, ctx->ocx + 1, ctx->bg);
}

void
tty_cmd_reverseindex(struct tty *tty, const struct tty_ctx *ctx)
{
	struct client	*c = tty->client;

	if (ctx->ocy != ctx->orupper)
		return;

	if (ctx->bigger ||
	    (!tty_full_width(tty, ctx) && !tty_use_margin(tty)) ||
	    tty_fake_bce(tty, &ctx->defaults, 8) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    (!tty_term_has(tty->term, TTYC_RI) &&
	    !tty_term_has(tty->term, TTYC_RIN)) ||
	    ctx->sx == 1 ||
	    ctx->sy == 1 ||
	    c->overlay_check != NULL) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_pane(tty, ctx);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->orupper);

	if (tty_term_has(tty->term, TTYC_RI))
		tty_putcode(tty, TTYC_RI);
	else
		tty_putcode_i(tty, TTYC_RIN, 1);
}

void
tty_cmd_linefeed(struct tty *tty, const struct tty_ctx *ctx)
{
	struct client	*c = tty->client;

	if (ctx->ocy != ctx->orlower)
		return;

	if (ctx->bigger ||
	    (!tty_full_width(tty, ctx) && !tty_use_margin(tty)) ||
	    tty_fake_bce(tty, &ctx->defaults, 8) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    ctx->sx == 1 ||
	    ctx->sy == 1 ||
	    c->overlay_check != NULL) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

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
	struct client	*c = tty->client;
	u_int		 i;

	if (ctx->bigger ||
	    (!tty_full_width(tty, ctx) && !tty_use_margin(tty)) ||
	    tty_fake_bce(tty, &ctx->defaults, 8) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    ctx->sx == 1 ||
	    ctx->sy == 1 ||
	    c->overlay_check != NULL) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

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
		if (tty->cy == UINT_MAX)
			tty_cursor(tty, 0, 0);
		else
			tty_cursor(tty, 0, tty->cy);
		tty_putcode_i(tty, TTYC_INDN, ctx->num);
	}
}

void
tty_cmd_scrolldown(struct tty *tty, const struct tty_ctx *ctx)
{
	u_int		 i;
	struct client	*c = tty->client;

	if (ctx->bigger ||
	    (!tty_full_width(tty, ctx) && !tty_use_margin(tty)) ||
	    tty_fake_bce(tty, &ctx->defaults, 8) ||
	    !tty_term_has(tty->term, TTYC_CSR) ||
	    (!tty_term_has(tty->term, TTYC_RI) &&
	    !tty_term_has(tty->term, TTYC_RIN)) ||
	    ctx->sx == 1 ||
	    ctx->sy == 1 ||
	    c->overlay_check != NULL) {
		tty_redraw_region(tty, ctx);
		return;
	}

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);
	tty_margin_pane(tty, ctx);
	tty_cursor_pane(tty, ctx, ctx->ocx, ctx->orupper);

	if (tty_term_has(tty->term, TTYC_RIN))
		tty_putcode_i(tty, TTYC_RIN, ctx->num);
	else {
		for (i = 0; i < ctx->num; i++)
			tty_putcode(tty, TTYC_RI);
	}
}

void
tty_cmd_clearendofscreen(struct tty *tty, const struct tty_ctx *ctx)
{
	u_int	px, py, nx, ny;

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, 0, ctx->sy - 1);
	tty_margin_off(tty);

	px = 0;
	nx = ctx->sx;
	py = ctx->ocy + 1;
	ny = ctx->sy - ctx->ocy - 1;

	tty_clear_pane_area(tty, ctx, py, ny, px, nx, ctx->bg);

	px = ctx->ocx;
	nx = ctx->sx - ctx->ocx;
	py = ctx->ocy;

	tty_clear_pane_line(tty, ctx, py, px, nx, ctx->bg);
}

void
tty_cmd_clearstartofscreen(struct tty *tty, const struct tty_ctx *ctx)
{
	u_int	px, py, nx, ny;

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, 0, ctx->sy - 1);
	tty_margin_off(tty);

	px = 0;
	nx = ctx->sx;
	py = 0;
	ny = ctx->ocy;

	tty_clear_pane_area(tty, ctx, py, ny, px, nx, ctx->bg);

	px = 0;
	nx = ctx->ocx + 1;
	py = ctx->ocy;

	tty_clear_pane_line(tty, ctx, py, px, nx, ctx->bg);
}

void
tty_cmd_clearscreen(struct tty *tty, const struct tty_ctx *ctx)
{
	u_int	px, py, nx, ny;

	tty_default_attributes(tty, &ctx->defaults, ctx->palette, ctx->bg,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, 0, ctx->sy - 1);
	tty_margin_off(tty);

	px = 0;
	nx = ctx->sx;
	py = 0;
	ny = ctx->sy;

	tty_clear_pane_area(tty, ctx, py, ny, px, nx, ctx->bg);
}

void
tty_cmd_alignmenttest(struct tty *tty, const struct tty_ctx *ctx)
{
	u_int	i, j;

	if (ctx->bigger) {
		ctx->redraw_cb(ctx);
		return;
	}

	tty_attributes(tty, &grid_default_cell, &ctx->defaults, ctx->palette,
	    ctx->s->hyperlinks);

	tty_region_pane(tty, ctx, 0, ctx->sy - 1);
	tty_margin_off(tty);

	for (j = 0; j < ctx->sy; j++) {
		tty_cursor_pane(tty, ctx, 0, j);
		for (i = 0; i < ctx->sx; i++)
			tty_putc(tty, 'E');
	}
}

void
tty_cmd_cell(struct tty *tty, const struct tty_ctx *ctx)
{
	const struct grid_cell	*gcp = ctx->cell;
	struct screen		*s = ctx->s;
	struct overlay_ranges	 r;
	u_int			 px, py, i, vis = 0;

	px = ctx->xoff + ctx->ocx - ctx->wox;
	py = ctx->yoff + ctx->ocy - ctx->woy;
	if (!tty_is_visible(tty, ctx, ctx->ocx, ctx->ocy, 1, 1) ||
	    (gcp->data.width == 1 && !tty_check_overlay(tty, px, py)))
		return;

	/* Handle partially obstructed wide characters. */
	if (gcp->data.width > 1) {
		tty_check_overlay_range(tty, px, py, gcp->data.width, &r);
		for (i = 0; i < OVERLAY_MAX_RANGES; i++)
			vis += r.nx[i];
		if (vis < gcp->data.width) {
			tty_draw_line(tty, s, s->cx, s->cy, gcp->data.width,
			    px, py, &ctx->defaults, ctx->palette);
			return;
		}
	}

	if (ctx->xoff + ctx->ocx - ctx->wox > tty->sx - 1 &&
	    ctx->ocy == ctx->orlower &&
	    tty_full_width(tty, ctx))
		tty_region_pane(tty, ctx, ctx->orupper, ctx->orlower);

	tty_margin_off(tty);
	tty_cursor_pane_unless_wrap(tty, ctx, ctx->ocx, ctx->ocy);

	tty_cell(tty, ctx->cell, &ctx->defaults, ctx->palette,
	    ctx->s->hyperlinks);

	if (ctx->num == 1)
		tty_invalidate(tty);
}

void
tty_cmd_cells(struct tty *tty, const struct tty_ctx *ctx)
{
	struct overlay_ranges	 r;
	u_int			 i, px, py, cx;
	char			*cp = ctx->ptr;

	if (!tty_is_visible(tty, ctx, ctx->ocx, ctx->ocy, ctx->num, 1))
		return;

	if (ctx->bigger &&
	    (ctx->xoff + ctx->ocx < ctx->wox ||
	    ctx->xoff + ctx->ocx + ctx->num > ctx->wox + ctx->wsx)) {
		if (!ctx->wrapped ||
		    !tty_full_width(tty, ctx) ||
		    (tty->term->flags & TERM_NOAM) ||
		    ctx->xoff + ctx->ocx != 0 ||
		    ctx->yoff + ctx->ocy != tty->cy + 1 ||
		    tty->cx < tty->sx ||
		    tty->cy == tty->rlower)
			tty_draw_pane(tty, ctx, ctx->ocy);
		else
			ctx->redraw_cb(ctx);
		return;
	}

	tty_margin_off(tty);
	tty_cursor_pane_unless_wrap(tty, ctx, ctx->ocx, ctx->ocy);
	tty_attributes(tty, ctx->cell, &ctx->defaults, ctx->palette, ctx->s->hyperlinks);

	/* Get tty position from pane position for overlay check. */
	px = ctx->xoff + ctx->ocx - ctx->wox;
	py = ctx->yoff + ctx->ocy - ctx->woy;

	tty_check_overlay_range(tty, px, py, ctx->num, &r);
	for (i = 0; i < OVERLAY_MAX_RANGES; i++) {
		if (r.nx[i] == 0)
			continue;
		/* Convert back to pane position for printing. */
		cx = r.px[i] - ctx->xoff + ctx->wox;
		tty_cursor_pane_unless_wrap(tty, ctx, cx, ctx->ocy);
		tty_putn(tty, cp + r.px[i] - px, r.nx[i], r.nx[i]);
	}
}

void
tty_cmd_setselection(struct tty *tty, const struct tty_ctx *ctx)
{
	tty_set_selection(tty, ctx->ptr2, ctx->ptr, ctx->num);
}

void
tty_set_selection(struct tty *tty, const char *flags, const char *buf,
    size_t len)
{
	char	*encoded;
	size_t	 size;

	if (~tty->flags & TTY_STARTED)
		return;
	if (!tty_term_has(tty->term, TTYC_MS))
		return;

	size = 4 * ((len + 2) / 3) + 1; /* storage for base64 */
	encoded = xmalloc(size);

	b64_ntop(buf, len, encoded, size);
	tty->flags |= TTY_NOBLOCK;
	tty_putcode_ss(tty, TTYC_MS, flags, encoded);

	free(encoded);
}

void
tty_cmd_rawstring(struct tty *tty, const struct tty_ctx *ctx)
{
	tty->flags |= TTY_NOBLOCK;
	tty_add(tty, ctx->ptr, ctx->num);
	tty_invalidate(tty);
}

#ifdef ENABLE_SIXEL
void
tty_cmd_sixelimage(struct tty *tty, const struct tty_ctx *ctx)
{
	struct image		*im = ctx->ptr;
	struct sixel_image	*si = im->data;
	struct sixel_image	*new;
	char			*data;
	size_t			 size;
	u_int			 cx = ctx->ocx, cy = ctx->ocy, sx, sy;
	u_int			 i, j, x, y, rx, ry;
	int			 fallback = 0;

	if ((~tty->term->flags & TERM_SIXEL) &&
            !tty_term_has(tty->term, TTYC_SXL))
		fallback = 1;
	if (tty->xpixel == 0 || tty->ypixel == 0)
		fallback = 1;

	sixel_size_in_cells(si, &sx, &sy);
	log_debug("%s: image is %ux%u", __func__, sx, sy);
	if (!tty_clamp_area(tty, ctx, cx, cy, sx, sy, &i, &j, &x, &y, &rx, &ry))
		return;
	log_debug("%s: clamping to %u,%u-%u,%u", __func__, i, j, rx, ry);

	if (fallback == 1) {
		data = xstrdup(im->fallback);
		size = strlen(data);
	} else {
		new = sixel_scale(si, tty->xpixel, tty->ypixel, i, j, rx, ry, 0);
		if (new == NULL)
			return;

		data = sixel_print(new, si, &size);
	}
	if (data != NULL) {
		log_debug("%s: %zu bytes: %s", __func__, size, data);
		tty_region_off(tty);
		tty_margin_off(tty);
		tty_cursor(tty, x, y);

		tty->flags |= TTY_NOBLOCK;
		tty_add(tty, data, size);
		tty_invalidate(tty);
		free(data);
	}

	if (fallback == 0)
		sixel_free(new);
}
#endif

void
tty_cmd_syncstart(struct tty *tty, const struct tty_ctx *ctx)
{
	if (ctx->num == 0x11) {
		/*
		 * This is an overlay and a command that moves the cursor so
		 * start synchronized updates.
		 */
		tty_sync_start(tty);
	} else if (~ctx->num & 0x10) {
		/*
		 * This is a pane. If there is an overlay, always start;
		 * otherwise, only if requested.
		 */
		if (ctx->num || tty->client->overlay_draw != NULL)
			tty_sync_start(tty);
	}
}

void
tty_cell(struct tty *tty, const struct grid_cell *gc,
    const struct grid_cell *defaults, struct colour_palette *palette,
    struct hyperlinks *hl)
{
	const struct grid_cell	*gcp;

	/* Skip last character if terminal is stupid. */
	if ((tty->term->flags & TERM_NOAM) &&
	    tty->cy == tty->sy - 1 &&
	    tty->cx == tty->sx - 1)
		return;

	/* If this is a padding character, do nothing. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;

	/* Check the output codeset and apply attributes. */
	gcp = tty_check_codeset(tty, gc);
	tty_attributes(tty, gcp, defaults, palette, hl);

	/* If it is a single character, write with putc to handle ACS. */
	if (gcp->data.size == 1) {
		tty_attributes(tty, gcp, defaults, palette, hl);
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
		if (gc->link != 0)
			tty_putcode_ss(tty, TTYC_HLS, "", "");
		if ((gc->attr & GRID_ATTR_CHARSET) && tty_acs_needed(tty))
			tty_putcode(tty, TTYC_RMACS);
		tty_putcode(tty, TTYC_SGR0);
		memcpy(gc, &grid_default_cell, sizeof *gc);
	}
	memcpy(&tty->last_cell, &grid_default_cell, sizeof tty->last_cell);
}

void
tty_invalidate(struct tty *tty)
{
	memcpy(&tty->cell, &grid_default_cell, sizeof tty->cell);
	memcpy(&tty->last_cell, &grid_default_cell, sizeof tty->last_cell);

	tty->cx = tty->cy = UINT_MAX;
	tty->rupper = tty->rleft = UINT_MAX;
	tty->rlower = tty->rright = UINT_MAX;

	if (tty->flags & TTY_STARTED) {
		if (tty_use_margin(tty))
			tty_putcode(tty, TTYC_ENMG);
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
	tty_region(tty, ctx->yoff + rupper - ctx->woy,
	    ctx->yoff + rlower - ctx->woy);
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
	if (tty->cx >= tty->sx) {
		if (tty->cy == UINT_MAX)
			tty_cursor(tty, 0, 0);
		else
			tty_cursor(tty, 0, tty->cy);
	}

	tty_putcode_ii(tty, TTYC_CSR, tty->rupper, tty->rlower);
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
	tty_margin(tty, ctx->xoff - ctx->wox,
	    ctx->xoff + ctx->sx - 1 - ctx->wox);
}

/* Set margin at absolute position. */
static void
tty_margin(struct tty *tty, u_int rleft, u_int rright)
{
	if (!tty_use_margin(tty))
		return;
	if (tty->rleft == rleft && tty->rright == rright)
		return;

	tty_putcode_ii(tty, TTYC_CSR, tty->rupper, tty->rlower);

	tty->rleft = rleft;
	tty->rright = rright;

	if (rleft == 0 && rright == tty->sx - 1)
		tty_putcode(tty, TTYC_CLMG);
	else
		tty_putcode_ii(tty, TTYC_CMG, rleft, rright);
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
	    !tty_full_width(tty, ctx) ||
	    (tty->term->flags & TERM_NOAM) ||
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
	tty_cursor(tty, ctx->xoff + cx - ctx->wox, ctx->yoff + cy - ctx->woy);
}

/* Move cursor to absolute position. */
void
tty_cursor(struct tty *tty, u_int cx, u_int cy)
{
	struct tty_term	*term = tty->term;
	u_int		 thisx, thisy;
	int		 change;

	if (tty->flags & TTY_BLOCK)
		return;

	thisx = tty->cx;
	thisy = tty->cy;

	/*
	 * If in the automargin space, and want to be there, do not move.
	 * Otherwise, force the cursor to be in range (and complain).
	 */
	if (cx == thisx && cy == thisy && cx == tty->sx)
		return;
	if (cx > tty->sx - 1) {
		log_debug("%s: x too big %u > %u", __func__, cx, tty->sx - 1);
		cx = tty->sx - 1;
	}

	/* No change. */
	if (cx == thisx && cy == thisy)
		return;

	/* Currently at the very end of the line - use absolute movement. */
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
			tty_putcode_i(tty, TTYC_HPA, cx);
			goto out;
		} else if (change > 0 &&
		    tty_term_has(term, TTYC_CUB) &&
		    !tty_use_margin(tty)) {
			if (change == 2 && tty_term_has(term, TTYC_CUB1)) {
				tty_putcode(tty, TTYC_CUB1);
				tty_putcode(tty, TTYC_CUB1);
				goto out;
			}
			tty_putcode_i(tty, TTYC_CUB, change);
			goto out;
		} else if (change < 0 &&
		    tty_term_has(term, TTYC_CUF) &&
		    !tty_use_margin(tty)) {
			tty_putcode_i(tty, TTYC_CUF, -change);
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
				    tty_putcode_i(tty, TTYC_VPA, cy);
				    goto out;
			    }
		} else if (change > 0 && tty_term_has(term, TTYC_CUU)) {
			tty_putcode_i(tty, TTYC_CUU, change);
			goto out;
		} else if (change < 0 && tty_term_has(term, TTYC_CUD)) {
			tty_putcode_i(tty, TTYC_CUD, -change);
			goto out;
		}
	}

absolute:
	/* Absolute movement. */
	tty_putcode_ii(tty, TTYC_CUP, cy, cx);

out:
	tty->cx = cx;
	tty->cy = cy;
}

static void
tty_hyperlink(struct tty *tty, const struct grid_cell *gc,
    struct hyperlinks *hl)
{
	const char	*uri, *id;

	if (gc->link == tty->cell.link)
		return;
	tty->cell.link = gc->link;

	if (hl == NULL)
		return;

	if (gc->link == 0 || !hyperlinks_get(hl, gc->link, &uri, NULL, &id))
		tty_putcode_ss(tty, TTYC_HLS, "", "");
	else
		tty_putcode_ss(tty, TTYC_HLS, id, uri);
}

void
tty_attributes(struct tty *tty, const struct grid_cell *gc,
    const struct grid_cell *defaults, struct colour_palette *palette,
    struct hyperlinks *hl)
{
	struct grid_cell	*tc = &tty->cell, gc2;
	int			 changed;

	/* Copy cell and update default colours. */
	memcpy(&gc2, gc, sizeof gc2);
	if (~gc->flags & GRID_FLAG_NOPALETTE) {
		if (gc2.fg == 8)
			gc2.fg = defaults->fg;
		if (gc2.bg == 8)
			gc2.bg = defaults->bg;
	}

	/* Ignore cell if it is the same as the last one. */
	if (gc2.attr == tty->last_cell.attr &&
	    gc2.fg == tty->last_cell.fg &&
	    gc2.bg == tty->last_cell.bg &&
	    gc2.us == tty->last_cell.us &&
		gc2.link == tty->last_cell.link)
		return;

	/*
	 * If no setab, try to use the reverse attribute as a best-effort for a
	 * non-default background. This is a bit of a hack but it doesn't do
	 * any serious harm and makes a couple of applications happier.
	 */
	if (!tty_term_has(tty->term, TTYC_SETAB)) {
		if (gc2.attr & GRID_ATTR_REVERSE) {
			if (gc2.fg != 7 && !COLOUR_DEFAULT(gc2.fg))
				gc2.attr &= ~GRID_ATTR_REVERSE;
		} else {
			if (gc2.bg != 0 && !COLOUR_DEFAULT(gc2.bg))
				gc2.attr |= GRID_ATTR_REVERSE;
		}
	}

	/* Fix up the colours if necessary. */
	tty_check_fg(tty, palette, &gc2);
	tty_check_bg(tty, palette, &gc2);
	tty_check_us(tty, palette, &gc2);

	/*
	 * If any bits are being cleared or the underline colour is now default,
	 * reset everything.
	 */
	if ((tc->attr & ~gc2.attr) || (tc->us != gc2.us && gc2.us == 0))
		tty_reset(tty);

	/*
	 * Set the colours. This may call tty_reset() (so it comes next) and
	 * may add to (NOT remove) the desired attributes.
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
	if (changed & GRID_ATTR_ALL_UNDERSCORE) {
		if ((changed & GRID_ATTR_UNDERSCORE) ||
		    !tty_term_has(tty->term, TTYC_SMULX))
			tty_putcode(tty, TTYC_SMUL);
		else if (changed & GRID_ATTR_UNDERSCORE_2)
			tty_putcode_i(tty, TTYC_SMULX, 2);
		else if (changed & GRID_ATTR_UNDERSCORE_3)
			tty_putcode_i(tty, TTYC_SMULX, 3);
		else if (changed & GRID_ATTR_UNDERSCORE_4)
			tty_putcode_i(tty, TTYC_SMULX, 4);
		else if (changed & GRID_ATTR_UNDERSCORE_5)
			tty_putcode_i(tty, TTYC_SMULX, 5);
	}
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
	if (changed & GRID_ATTR_OVERLINE)
		tty_putcode(tty, TTYC_SMOL);
	if ((changed & GRID_ATTR_CHARSET) && tty_acs_needed(tty))
		tty_putcode(tty, TTYC_SMACS);

	/* Set hyperlink if any. */
	tty_hyperlink(tty, gc, hl);

	memcpy(&tty->last_cell, &gc2, sizeof tty->last_cell);
}

static void
tty_colours(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;

	/* No changes? Nothing is necessary. */
	if (gc->fg == tc->fg && gc->bg == tc->bg && gc->us == tc->us)
		return;

	/*
	 * Is either the default colour? This is handled specially because the
	 * best solution might be to reset both colours to default, in which
	 * case if only one is default need to fall onward to set the other
	 * colour.
	 */
	if (COLOUR_DEFAULT(gc->fg) || COLOUR_DEFAULT(gc->bg)) {
		/*
		 * If don't have AX, send sgr0. This resets both colours to default.
		 * Otherwise, try to set the default colour only as needed.
		 */
		if (!tty_term_flag(tty->term, TTYC_AX))
			tty_reset(tty);
		else {
			if (COLOUR_DEFAULT(gc->fg) && !COLOUR_DEFAULT(tc->fg)) {
				tty_puts(tty, "\033[39m");
				tc->fg = gc->fg;
			}
			if (COLOUR_DEFAULT(gc->bg) && !COLOUR_DEFAULT(tc->bg)) {
				tty_puts(tty, "\033[49m");
				tc->bg = gc->bg;
			}
		}
	}

	/* Set the foreground colour. */
	if (!COLOUR_DEFAULT(gc->fg) && gc->fg != tc->fg)
		tty_colours_fg(tty, gc);

	/*
	 * Set the background colour. This must come after the foreground as
	 * tty_colours_fg() can call tty_reset().
	 */
	if (!COLOUR_DEFAULT(gc->bg) && gc->bg != tc->bg)
		tty_colours_bg(tty, gc);

	/* Set the underscore colour. */
	if (gc->us != tc->us)
		tty_colours_us(tty, gc);
}

static void
tty_check_fg(struct tty *tty, struct colour_palette *palette,
    struct grid_cell *gc)
{
	u_char	r, g, b;
	u_int	colours;
	int	c;

	/*
	 * Perform substitution if this pane has a palette. If the bright
	 * attribute is set and Nobr is not present, use the bright entry in
	 * the palette by changing to the aixterm colour
	 */
	if (~gc->flags & GRID_FLAG_NOPALETTE) {
		c = gc->fg;
		if (c < 8 &&
		    gc->attr & GRID_ATTR_BRIGHT &&
		    !tty_term_has(tty->term, TTYC_NOBR))
			c += 90;
		if ((c = colour_palette_get(palette, c)) != -1)
			gc->fg = c;
	}

	/* Is this a 24-bit colour? */
	if (gc->fg & COLOUR_FLAG_RGB) {
		/* Not a 24-bit terminal? Translate to 256-colour palette. */
		if (tty->term->flags & TERM_RGBCOLOURS)
			return;
		colour_split_rgb(gc->fg, &r, &g, &b);
		gc->fg = colour_find_rgb(r, g, b);
	}

	/* How many colours does this terminal have? */
	if (tty->term->flags & TERM_256COLOURS)
		colours = 256;
	else
		colours = tty_term_number(tty->term, TTYC_COLORS);

	/* Is this a 256-colour colour? */
	if (gc->fg & COLOUR_FLAG_256) {
		/* And not a 256 colour mode? */
		if (colours < 256) {
			gc->fg = colour_256to16(gc->fg);
			if (gc->fg & 8) {
				gc->fg &= 7;
				if (colours >= 16)
					gc->fg += 90;
				else if (gc->fg == 0 && gc->bg == 0)
					gc->fg = 7;
			}
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
tty_check_bg(struct tty *tty, struct colour_palette *palette,
    struct grid_cell *gc)
{
	u_char	r, g, b;
	u_int	colours;
	int	c;

	/* Perform substitution if this pane has a palette. */
	if (~gc->flags & GRID_FLAG_NOPALETTE) {
		if ((c = colour_palette_get(palette, gc->bg)) != -1)
			gc->bg = c;
	}

	/* Is this a 24-bit colour? */
	if (gc->bg & COLOUR_FLAG_RGB) {
		/* Not a 24-bit terminal? Translate to 256-colour palette. */
		if (tty->term->flags & TERM_RGBCOLOURS)
			return;
		colour_split_rgb(gc->bg, &r, &g, &b);
		gc->bg = colour_find_rgb(r, g, b);
	}

	/* How many colours does this terminal have? */
	if (tty->term->flags & TERM_256COLOURS)
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
		if (colours < 256) {
			gc->bg = colour_256to16(gc->bg);
			if (gc->bg & 8) {
				gc->bg &= 7;
				if (colours >= 16)
					gc->bg += 90;
			}
		}
		return;
	}

	/* Is this an aixterm colour? */
	if (gc->bg >= 90 && gc->bg <= 97 && colours < 16)
		gc->bg -= 90;
}

static void
tty_check_us(__unused struct tty *tty, struct colour_palette *palette,
    struct grid_cell *gc)
{
	int	c;

	/* Perform substitution if this pane has a palette. */
	if (~gc->flags & GRID_FLAG_NOPALETTE) {
		if ((c = colour_palette_get(palette, gc->us)) != -1)
			gc->us = c;
	}

	/* Convert underscore colour if only RGB can be supported. */
	if (!tty_term_has(tty->term, TTYC_SETULC1)) {
		    if ((c = colour_force_rgb (gc->us)) == -1)
			    gc->us = 8;
		    else
			    gc->us = c;
	}
}

static void
tty_colours_fg(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;
	char			 s[32];

	/*
	 * If the current colour is an aixterm bright colour and the new is not,
	 * reset because some terminals do not clear bright correctly.
	 */
	if (tty->cell.fg >= 90 &&
	    tty->cell.bg <= 97 &&
	    (gc->fg < 90 || gc->fg > 97))
		tty_reset(tty);

	/* Is this a 24-bit or 256-colour colour? */
	if (gc->fg & COLOUR_FLAG_RGB || gc->fg & COLOUR_FLAG_256) {
		if (tty_try_colour(tty, gc->fg, "38") == 0)
			goto save;
		/* Should not get here, already converted in tty_check_fg. */
		return;
	}

	/* Is this an aixterm bright colour? */
	if (gc->fg >= 90 && gc->fg <= 97) {
		if (tty->term->flags & TERM_256COLOURS) {
			xsnprintf(s, sizeof s, "\033[%dm", gc->fg);
			tty_puts(tty, s);
		} else
			tty_putcode_i(tty, TTYC_SETAF, gc->fg - 90 + 8);
		goto save;
	}

	/* Otherwise set the foreground colour. */
	tty_putcode_i(tty, TTYC_SETAF, gc->fg);

save:
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
			goto save;
		/* Should not get here, already converted in tty_check_bg. */
		return;
	}

	/* Is this an aixterm bright colour? */
	if (gc->bg >= 90 && gc->bg <= 97) {
		if (tty->term->flags & TERM_256COLOURS) {
			xsnprintf(s, sizeof s, "\033[%dm", gc->bg + 10);
			tty_puts(tty, s);
		} else
			tty_putcode_i(tty, TTYC_SETAB, gc->bg - 90 + 8);
		goto save;
	}

	/* Otherwise set the background colour. */
	tty_putcode_i(tty, TTYC_SETAB, gc->bg);

save:
	/* Save the new values in the terminal current cell. */
	tc->bg = gc->bg;
}

static void
tty_colours_us(struct tty *tty, const struct grid_cell *gc)
{
	struct grid_cell	*tc = &tty->cell;
	u_int			 c;
	u_char			 r, g, b;

	/* Clear underline colour. */
	if (COLOUR_DEFAULT(gc->us)) {
		tty_putcode(tty, TTYC_OL);
		goto save;
	}

	/*
	 * If this is not an RGB colour, use Setulc1 if it exists, otherwise
	 * convert.
	 */
	if (~gc->us & COLOUR_FLAG_RGB) {
		c = gc->us;
		if ((~c & COLOUR_FLAG_256) && (c >= 90 && c <= 97))
			c -= 82;
		tty_putcode_i(tty, TTYC_SETULC1, c & ~COLOUR_FLAG_256);
		return;
	}

	/*
	 * Setulc and setal follows the ncurses(3) one argument "direct colour"
	 * capability format. Calculate the colour value.
	 */
	colour_split_rgb(gc->us, &r, &g, &b);
	c = (65536 * r) + (256 * g) + b;

	/*
	 * Write the colour. Only use setal if the RGB flag is set because the
	 * non-RGB version may be wrong.
	 */
	if (tty_term_has(tty->term, TTYC_SETULC))
		tty_putcode_i(tty, TTYC_SETULC, c);
	else if (tty_term_has(tty->term, TTYC_SETAL) &&
	    tty_term_has(tty->term, TTYC_RGB))
		tty_putcode_i(tty, TTYC_SETAL, c);

save:
	/* Save the new values in the terminal current cell. */
	tc->us = gc->us;
}

static int
tty_try_colour(struct tty *tty, int colour, const char *type)
{
	u_char	r, g, b;

	if (colour & COLOUR_FLAG_256) {
		if (*type == '3' && tty_term_has(tty->term, TTYC_SETAF))
			tty_putcode_i(tty, TTYC_SETAF, colour & 0xff);
		else if (tty_term_has(tty->term, TTYC_SETAB))
			tty_putcode_i(tty, TTYC_SETAB, colour & 0xff);
		return (0);
	}

	if (colour & COLOUR_FLAG_RGB) {
		colour_split_rgb(colour & 0xffffff, &r, &g, &b);
		if (*type == '3' && tty_term_has(tty->term, TTYC_SETRGBF))
			tty_putcode_iii(tty, TTYC_SETRGBF, r, g, b);
		else if (tty_term_has(tty->term, TTYC_SETRGBB))
			tty_putcode_iii(tty, TTYC_SETRGBB, r, g, b);
		return (0);
	}

	return (-1);
}

static void
tty_window_default_style(struct grid_cell *gc, struct window_pane *wp)
{
	memcpy(gc, &grid_default_cell, sizeof *gc);
	gc->fg = wp->palette.fg;
	gc->bg = wp->palette.bg;
}

void
tty_default_colours(struct grid_cell *gc, struct window_pane *wp)
{
	struct options		*oo = wp->options;
	struct format_tree	*ft;

	memcpy(gc, &grid_default_cell, sizeof *gc);

	if (wp->flags & PANE_STYLECHANGED) {
		log_debug("%%%u: style changed", wp->id);
		wp->flags &= ~PANE_STYLECHANGED;

		ft = format_create(NULL, NULL, FORMAT_PANE|wp->id,
		    FORMAT_NOJOBS);
		format_defaults(ft, NULL, NULL, NULL, wp);
		tty_window_default_style(&wp->cached_active_gc, wp);
		style_add(&wp->cached_active_gc, oo, "window-active-style", ft);
		tty_window_default_style(&wp->cached_gc, wp);
		style_add(&wp->cached_gc, oo, "window-style", ft);
		format_free(ft);
	}

	if (gc->fg == 8) {
		if (wp == wp->window->active && wp->cached_active_gc.fg != 8)
			gc->fg = wp->cached_active_gc.fg;
		else
			gc->fg = wp->cached_gc.fg;
	}

	if (gc->bg == 8) {
		if (wp == wp->window->active && wp->cached_active_gc.bg != 8)
			gc->bg = wp->cached_active_gc.bg;
		else
			gc->bg = wp->cached_gc.bg;
	}
}

static void
tty_default_attributes(struct tty *tty, const struct grid_cell *defaults,
    struct colour_palette *palette, u_int bg, struct hyperlinks *hl)
{
	struct grid_cell	gc;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.bg = bg;
	tty_attributes(tty, &gc, defaults, palette, hl);
}

static void
tty_clipboard_query_callback(__unused int fd, __unused short events, void *data)
{
	struct tty	*tty = data;
	struct client	*c = tty->client;

	c->flags &= ~CLIENT_CLIPBOARDBUFFER;
	free(c->clipboard_panes);
	c->clipboard_panes = NULL;
	c->clipboard_npanes = 0;

	tty->flags &= ~TTY_OSC52QUERY;
}

void
tty_clipboard_query(struct tty *tty)
{
	struct timeval	 tv = { .tv_sec = TTY_QUERY_TIMEOUT };

	if ((~tty->flags & TTY_STARTED) || (tty->flags & TTY_OSC52QUERY))
		return;
	tty_putcode_ss(tty, TTYC_MS, "", "?");

	tty->flags |= TTY_OSC52QUERY;
	evtimer_set(&tty->clipboard_timer, tty_clipboard_query_callback, tty);
	evtimer_add(&tty->clipboard_timer, &tv);
}
