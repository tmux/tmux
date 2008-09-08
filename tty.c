/* $Id: tty.c,v 1.39 2008-09-08 21:04:59 nicm Exp $ */

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

#include <ncurses.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

struct tty_term *tty_find_term(char *, int,char **);
void 	tty_free_term(struct tty_term *);

void	tty_fill_acs(struct tty *);
u_char	tty_get_acs(struct tty *, u_char);

const char *tty_strip(const char *);
void	tty_raw(struct tty *, const char *);
void	tty_puts(struct tty *, const char *);
void	tty_putc(struct tty *, char);

void	tty_attributes(struct tty *, u_char, u_char, u_char);
char	tty_translate(char);

TAILQ_HEAD(, tty_term) tty_terms = TAILQ_HEAD_INITIALIZER(tty_terms);

void
tty_init(struct tty *tty, char *path, char *term)
{
	tty->path = xstrdup(path);
	if (term == NULL)
		tty->termname = xstrdup("unknown");
	else
		tty->termname = xstrdup(term);
}

int
tty_open(struct tty *tty, char **cause)
{
	struct termios	 tio;
	int		 what, mode;

	tty->fd = open(tty->path, O_RDWR|O_NONBLOCK);
	if (tty->fd == -1) {
		xasprintf(cause, "%s: %s", tty->path, strerror(errno));
		return (-1);
	}
	
	if ((mode = fcntl(tty->fd, F_GETFL)) == -1)
		fatal("fcntl");
	if (fcntl(tty->fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl");

	if ((tty->term = tty_find_term(tty->termname, tty->fd, cause)) == NULL)
		goto error;

	tty->in = buffer_create(BUFSIZ);
	tty->out = buffer_create(BUFSIZ);

	tty->attr = 0;
	tty->fg = 8;
	tty->bg = 8;

	tty->flags = 0;

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

	if (init_1string != NULL)
		tty_puts(tty, init_1string);
	if (init_2string != NULL)
		tty_puts(tty, init_2string);
	if (init_3string != NULL)
		tty_puts(tty, init_3string);

	if (enter_ca_mode != NULL)
		tty_puts(tty, enter_ca_mode);
	if (keypad_xmit != NULL)
		tty_puts(tty, keypad_xmit);
	if (ena_acs != NULL)
		tty_puts(tty, ena_acs);
	tty_puts(tty, clear_screen);

	tty_keys_init(tty);

	tty_fill_acs(tty);

	return (0);

error:
	close(tty->fd);
	tty->fd = -1;

	return (-1);
}

void
tty_close(struct tty *tty)
{
	struct winsize	ws;

	if (tty->fd == -1)
		return;

	/*
	 * Skip any writing if the fd is invalid. Things like ssh -t can
	 * easily leave us with a dead tty.
	 */
	if (ioctl(tty->fd, TIOCGWINSZ, &ws) == -1) {
		if (errno != EBADF && errno != ENXIO)
			fatal("ioctl(TIOCGWINSZ)");
	} else {
		 if (tcsetattr(tty->fd, TCSANOW, &tty->tio) != 0)
			 fatal("tcsetattr failed");

		 tty_raw(tty, tparm(change_scroll_region, 0, ws.ws_row - 1));
		 if (keypad_local != NULL)
			 tty_raw(tty, keypad_local);
		 if (exit_ca_mode != NULL)
			 tty_raw(tty, exit_ca_mode);
		 tty_raw(tty, clear_screen);
		 if (cursor_normal != NULL)
			 tty_raw(tty, cursor_normal);
		 if (exit_attribute_mode != NULL)
			 tty_raw(tty, exit_attribute_mode);
	}

	tty_free_term(tty->term);
	tty_keys_free(tty);

	close(tty->fd);
	tty->fd = -1;

	buffer_destroy(tty->in);
	buffer_destroy(tty->out);
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

struct tty_term *
tty_find_term(char *name, int fd, char **cause)
{
	struct tty_term	*term;
	int		 error;

	TAILQ_FOREACH(term, &tty_terms, entry) {
		if (strcmp(term->name, name) == 0) {
			term->references++;
			return (term);
		}
	}

	term = xmalloc(sizeof *term);
	term->name = xstrdup(name);
	term->term = NULL;
	term->references = 1;
	TAILQ_INSERT_HEAD(&tty_terms, term, entry);

	if (setupterm(name, fd, &error) != OK) {
		switch (error) {
		case 0:
			xasprintf(cause, "can't use hardcopy terminal");
			break;
		case 1:
			xasprintf(cause, "missing or unsuitable terminal");
			break;
		case 2:
			xasprintf(cause, "can't find terminfo database");
			break;
		default:
			xasprintf(cause, "unknown error");
			break;
		}
		goto error;
	}
	term->term = cur_term;

	if (clear_screen == NULL) {
		xasprintf(cause, "clear_screen missing");
		goto error;
	}
	if (cursor_down == NULL) {
		xasprintf(cause, "cursor_down missing");
		goto error;
	}
	if (carriage_return == NULL) {
		xasprintf(cause, "carriage_return missing");
		goto error;
	}
	if (cursor_left == NULL) {
		xasprintf(cause, "cursor_left missing");
		goto error;
	}
	if (parm_up_cursor == NULL && cursor_up == NULL) {
		xasprintf(cause, "parm_up_cursor missing");
		goto error;
	}
	if (parm_down_cursor == NULL && cursor_down == NULL) {
		xasprintf(cause, "parm_down_cursor missing");
		goto error;
	}
	if (parm_right_cursor == NULL && cursor_right == NULL) {
		xasprintf(cause, "parm_right_cursor missing");
		goto error;
	}
	if (parm_left_cursor == NULL && cursor_left == NULL) {
		xasprintf(cause, "parm_left_cursor missing");
		goto error;
	}
	if (cursor_address == NULL) {
		xasprintf(cause, "cursor_address missing");
		goto error;
	}
	if (parm_insert_line == NULL && insert_line == NULL) {
		xasprintf(cause, "parm_insert_line missing");
		goto error;
	}
	if (parm_delete_line == NULL && delete_line == NULL) {
		xasprintf(cause, "parm_delete_line missing");
		goto error;
	}
	if (parm_ich == NULL && insert_character == NULL &&
	    (enter_insert_mode == NULL || exit_insert_mode == NULL)) {
		xasprintf(cause, "parm_ich missing");
		goto error;
	}
	if (scroll_reverse == NULL) {
		xasprintf(cause, "scroll_reverse missing");
		goto error;
	}
	if (change_scroll_region == NULL) {
		xasprintf(cause, "change_scroll_region missing");
		goto error;
	}

	if (tigetflag("AX") == TRUE)
		term->flags |= TERM_HASDEFAULTS;

	/*
	 * Try to figure out if we have 256 colours. The standard xterm
	 * definitions are broken (well, or the way they are parsed is: in
	 * any case they end up returning 8). So also do a hack.
	 */
	if (max_colors == 256 || strstr(name, "256col") != NULL) /* XXX HACK */
		term->flags |= TERM_256COLOURS;
	
	return (term);

error:
	tty_free_term(term);
	return (NULL);
}

void
tty_free_term(struct tty_term *term)
{
	if (--term->references != 0)
		return;

	TAILQ_REMOVE(&tty_terms, term, entry);

#ifdef __FreeBSD___
/*
 * XXX XXX XXX FIXME FIXME
 * FreeBSD 6.2 crashes with a double-free somewhere under here.
 */
	if (term->term != NULL)
		del_curterm(term->term);
#endif

	xfree(term->name);
	xfree(term);
}

void
tty_fill_acs(struct tty *tty)
{
	char	*ptr;

	memset(tty->acs, 0, sizeof tty->acs);
	if (acs_chars == NULL || (strlen(acs_chars) % 2) != 0)
		return;
	for (ptr = acs_chars; *ptr != '\0'; ptr += 2)
		tty->acs[(u_char) ptr[0]] = ptr[1];
}

u_char
tty_get_acs(struct tty *tty, u_char ch)
{
	if (tty->acs[ch] != '\0')
		return (tty->acs[ch]);
	return (ch);
}

const char *
tty_strip(const char *s)
{
	const char     *ptr;
	static char	buf[BUFSIZ];
	size_t		len;

	/* Ignore strings with no padding. */
	if (strchr(s, '$') == NULL)
		return (s);

	len = 0;
	for (ptr = s; *ptr != '\0'; ptr++) {
		if (*ptr == '$' && *(ptr + 1) == '<') {
			while (*ptr != '\0' && *ptr != '>')
				ptr++;
			if (*ptr == '>')
				ptr++;
		}
		
		buf[len++] = *ptr;
		if (len == (sizeof buf) - 1)
			break;
	}
	buf[len] = '\0';
	
	return (buf);
}

void
tty_raw(struct tty *tty, const char *s)
{
	const char	*t;

	t = tty_strip(s);
	write(tty->fd, t, strlen(t));
}

void
tty_puts(struct tty *tty, const char *s)
{
	const char	*t;

	t = tty_strip(s);
	buffer_write(tty->out, t, strlen(t));
}

void
tty_putc(struct tty *tty, char ch)
{
	if (tty->attr & ATTR_CHARSET)
		ch = tty_get_acs(tty, ch);
	buffer_write8(tty->out, ch);
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
tty_vwrite(struct tty *tty, struct screen *s, int cmd, va_list ap)
{
	char	ch;
	u_int	i, ua, ub, uc;

	if (tty->flags & TTY_FREEZE)
		return;
	
	if (tty->term == NULL) /* XXX XXX */
		return;
	set_curterm(tty->term->term);

	switch (cmd) {
	case TTY_CHARACTER:
		ch = va_arg(ap, int);
		switch (ch) {
		case '\n':	/* LF */
			tty_putc(tty, '\n');
			break;
		case '\r':	/* CR */
			tty_puts(tty, carriage_return);
			break;
		case '\007':	/* BEL */
			if (bell != NULL)
				tty_puts(tty, bell);
			break;
		case '\010':	/* BS */
			tty_puts(tty, cursor_left);
			break;
		default:
			tty_putc(tty, ch);
			break;
		}
		break;
	case TTY_CURSORUP:
		ua = va_arg(ap, u_int);
		if (parm_up_cursor != NULL)
			tty_puts(tty, tparm(parm_up_cursor, ua));
		else {
			while (ua-- > 0)
				tty_puts(tty, cursor_up);
		}
		break;
	case TTY_CURSORDOWN:
		ua = va_arg(ap, u_int);
		if (parm_down_cursor != NULL)
			tty_puts(tty, tparm(parm_down_cursor, ua));
		else {
			while (ua-- > 0)
				tty_puts(tty, cursor_down);
		}
		break;
	case TTY_CURSORRIGHT:
		ua = va_arg(ap, u_int);
		if (parm_right_cursor != NULL)
			tty_puts(tty, tparm(parm_right_cursor, ua));
		else {
			while (ua-- > 0)
				tty_puts(tty, cursor_right);
		}
		break;
	case TTY_CURSORLEFT:
		ua = va_arg(ap, u_int);
		if (parm_left_cursor != NULL)
			tty_puts(tty, tparm(parm_left_cursor, ua));
		else {
			while (ua-- > 0)
				tty_puts(tty, cursor_left);
		}
		break;
	case TTY_CURSORMOVE:
		ua = va_arg(ap, u_int);
		ub = va_arg(ap, u_int);
		tty_puts(tty, tparm(cursor_address, ua, ub));
		break;
	case TTY_CLEARENDOFLINE:
		if (clr_eol != NULL)
			tty_puts(tty, clr_eol);
		else {
			tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
			for (i = s->cx; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
			tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
		}
		break;
	case TTY_CLEARSTARTOFLINE:
		if (clr_bol != NULL)
			tty_puts(tty, clr_bol);
		else {
			tty_puts(tty, tparm(cursor_address, s->cy, 0));
			for (i = 0; i < s->cx + 1; i++)
				tty_putc(tty, ' ');
			tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
		}
		break;
	case TTY_CLEARLINE:
		if (clr_eol != NULL) {
			tty_puts(tty, tparm(cursor_address, s->cy, 0));
			tty_puts(tty, clr_eol);
			tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
		} else {
			tty_puts(tty, tparm(cursor_address, s->cy, 0));
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
			tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
		}
		break;
	case TTY_INSERTLINE:
		ua = va_arg(ap, u_int);
		if (parm_insert_line != NULL)
			tty_puts(tty, tparm(parm_insert_line, ua));
		else {
			while (ua-- > 0)
				tty_puts(tty, insert_line);
		}
		break;
	case TTY_DELETELINE:
		ua = va_arg(ap, u_int);
		if (parm_delete_line != NULL)
			tty_puts(tty, tparm(parm_delete_line, ua));
		else {
			while (ua-- > 0)
				tty_puts(tty, delete_line);
		}
		break;
	case TTY_INSERTCHARACTER:
		ua = va_arg(ap, u_int);
		if (parm_ich != NULL)
			tty_puts(tty, tparm(parm_ich, ua));
		else if (insert_character != NULL) {
			while (ua-- > 0)
				tty_puts(tty, insert_character);
		} else if (enter_insert_mode != NULL) {
			tty_puts(tty, enter_insert_mode);
			while (ua-- > 0)
				tty_putc(tty, ' ');
			tty_puts(tty, exit_insert_mode);
			tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
		}
		break;
	case TTY_DELETECHARACTER:
		ua = va_arg(ap, u_int);
		if (parm_dch != NULL)
			tty_puts(tty, tparm(parm_dch, ua));
		else if (delete_character != NULL) {
			while (ua-- > 0)
				tty_puts(tty, delete_character);
		} else {
			while (ua-- > 0)
				tty_putc(tty, '\010');
		}
		break;
	case TTY_CURSORON:
		if (!(tty->flags & TTY_NOCURSOR) && cursor_normal != NULL)
			tty_puts(tty, cursor_normal);
		break;
	case TTY_CURSOROFF:
		if (cursor_invisible != NULL)
			tty_puts(tty, cursor_invisible);
		break;
	case TTY_REVERSEINDEX:
		tty_puts(tty, scroll_reverse);
		break;
	case TTY_SCROLLREGION:
		ua = va_arg(ap, u_int);
		ub = va_arg(ap, u_int);
		tty_puts(tty, tparm(change_scroll_region, ua, ub));
		break;
#if 0
	case TTY_INSERTON:
		if (enter_insert_mode != NULL)
			tty_puts(tty, enter_insert_mode);
		break;
	case TTY_INSERTOFF:
		if (exit_insert_mode != NULL)
			tty_puts(tty, exit_insert_mode);
		break;
#endif
	case TTY_MOUSEOFF:
		if (key_mouse != NULL)
			tty_puts(tty, "\033[?1000l");
		break;
	case TTY_MOUSEON:
		if (key_mouse != NULL)
			tty_puts(tty, "\033[?1000h");
		break;
	case TTY_ATTRIBUTES:
		ua = va_arg(ap, u_int);
		ub = va_arg(ap, u_int);
		uc = va_arg(ap, u_int);
		tty_attributes(tty, ua, ub, uc);
		break;
	}
}

void
tty_attributes(struct tty *tty, u_char attr, u_char fg, u_char bg)
{
	char	s[32];

	if (attr == tty->attr && fg == tty->fg && bg == tty->bg)
		return;

	/* If any bits are being cleared, reset everything. */
	if (tty->attr & ~attr) {
		if ((tty->attr & ATTR_CHARSET) &&
		    exit_alt_charset_mode != NULL)
			tty_puts(tty, exit_alt_charset_mode);
		tty_puts(tty, exit_attribute_mode);
		tty->fg = 8;
		tty->bg = 8;
		tty->attr = 0;
	}

	/* Filter out bits already set. */
	attr &= ~tty->attr;
	tty->attr |= attr;

	if ((attr & ATTR_BRIGHT) && enter_bold_mode != NULL)
		tty_puts(tty, enter_bold_mode);
	if ((attr & ATTR_DIM) && enter_dim_mode != NULL)
		tty_puts(tty, enter_dim_mode);
	if ((attr & ATTR_ITALICS) && enter_standout_mode != NULL)
		tty_puts(tty, enter_standout_mode);
	if ((attr & ATTR_UNDERSCORE) && enter_underline_mode != NULL)
		tty_puts(tty, enter_underline_mode);
	if ((attr & ATTR_BLINK) && enter_blink_mode != NULL)
		tty_puts(tty, enter_blink_mode);
	if ((attr & ATTR_REVERSE) && enter_reverse_mode != NULL)
		tty_puts(tty, enter_reverse_mode);
	if ((attr & ATTR_HIDDEN) && enter_secure_mode != NULL)
		tty_puts(tty, enter_secure_mode);
	if ((attr & ATTR_CHARSET) && enter_alt_charset_mode != NULL)
		tty_puts(tty, enter_alt_charset_mode);

	if (fg != tty->fg) {
		if (fg > 15 && tty->term->flags & TERM_256COLOURS) {
			xsnprintf(s, sizeof s, "\033[38;5;%hhum", fg);
			tty_puts(tty, s);
		} else {
			if (fg > 7)
				fg = 8;
			if (fg == 8 && !(tty->term->flags & TERM_HASDEFAULTS))
				fg = 7;
			if (fg == 8)
				tty_puts(tty, "\033[39m");
			else if (set_a_foreground != NULL)
				tty_puts(tty, tparm(set_a_foreground, fg));
		}
	}

	if (bg != tty->bg) {
		if (bg > 15 && tty->term->flags & TERM_256COLOURS) {
			xsnprintf(s, sizeof s, "\033[48;5;%hhum", bg);
			tty_puts(tty, s);
		} else {
			if (bg > 7)
				bg = 8;
			if (bg == 8 && !(tty->term->flags & TERM_HASDEFAULTS))
				bg = 0;
			if (bg == 8)
				tty_puts(tty, "\033[49m");
			else if (set_a_background != NULL)
				tty_puts(tty, tparm(set_a_background, bg));
		}
	}

	tty->fg = fg;
	tty->bg = bg;
}
