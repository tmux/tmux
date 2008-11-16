/* $Id: tty.c,v 1.50 2008-11-16 10:10:26 nicm Exp $ */

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

void	tty_reset(struct tty *);
void	tty_attributes(struct tty *, const struct grid_cell *);
void	tty_attributes_fg(struct tty *, const struct grid_cell *);
void	tty_attributes_bg(struct tty *, const struct grid_cell *);

void	tty_cmd_cursorup(struct tty *, struct screen *, va_list);
void	tty_cmd_cursordown(struct tty *, struct screen *, va_list);
void	tty_cmd_cursorright(struct tty *, struct screen *, va_list);
void	tty_cmd_cursorleft(struct tty *, struct screen *, va_list);
void	tty_cmd_insertcharacter(struct tty *, struct screen *, va_list);
void	tty_cmd_deletecharacter(struct tty *, struct screen *, va_list);
void	tty_cmd_insertline(struct tty *, struct screen *, va_list);
void	tty_cmd_deleteline(struct tty *, struct screen *, va_list);
void	tty_cmd_clearline(struct tty *, struct screen *, va_list);
void	tty_cmd_clearendofline(struct tty *, struct screen *, va_list);
void	tty_cmd_clearstartofline(struct tty *, struct screen *, va_list);
void	tty_cmd_cursormove(struct tty *, struct screen *, va_list);
void	tty_cmd_cursormode(struct tty *, struct screen *, va_list);
void	tty_cmd_reverseindex(struct tty *, struct screen *, va_list);
void	tty_cmd_scrollregion(struct tty *, struct screen *, va_list);
void	tty_cmd_insertmode(struct tty *, struct screen *, va_list);
void	tty_cmd_mousemode(struct tty *, struct screen *, va_list);
void	tty_cmd_linefeed(struct tty *, struct screen *, va_list);
void	tty_cmd_carriagereturn(struct tty *, struct screen *, va_list);
void	tty_cmd_bell(struct tty *, struct screen *, va_list);
void	tty_cmd_clearendofscreen(struct tty *, struct screen *, va_list);
void	tty_cmd_clearstartofscreen(struct tty *, struct screen *, va_list);
void	tty_cmd_clearscreen(struct tty *, struct screen *, va_list);
void	tty_cmd_cell(struct tty *, struct screen *, va_list);

void (*tty_cmds[])(struct tty *, struct screen *, va_list) = {
	tty_cmd_cursorup,
	tty_cmd_cursordown,
	tty_cmd_cursorright,
	tty_cmd_cursorleft,
	tty_cmd_insertcharacter,
	tty_cmd_deletecharacter,
	tty_cmd_insertline,
	tty_cmd_deleteline,
	tty_cmd_clearline,
	tty_cmd_clearendofline,
	tty_cmd_clearstartofline,
	tty_cmd_cursormove,
	tty_cmd_cursormode,
	tty_cmd_reverseindex,
	tty_cmd_scrollregion,
	tty_cmd_insertmode,
	tty_cmd_mousemode,
	tty_cmd_linefeed,
	tty_cmd_carriagereturn,
	tty_cmd_bell,
	NULL,
	NULL,
	tty_cmd_clearendofscreen,
	tty_cmd_clearstartofscreen,
	tty_cmd_clearscreen,
	tty_cmd_cell,
};

SLIST_HEAD(, tty_term) tty_terms = SLIST_HEAD_INITIALIZER(tty_terms);

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

	if (debug_level > 3)
		tty->log_fd = open("tmux.out", O_WRONLY|O_CREAT|O_TRUNC, 0644);
	else
		tty->log_fd = -1;

	if ((tty->term = tty_find_term(tty->termname, tty->fd, cause)) == NULL)
		goto error;

	tty->in = buffer_create(BUFSIZ);
	tty->out = buffer_create(BUFSIZ);

	tty->flags &= TTY_UTF8;
	memcpy(&tty->cell, &grid_default_cell, sizeof tty->cell);

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

	if (tty->log_fd != -1) {
		close(tty->log_fd);
		tty->log_fd = -1;
	}

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
		tty_raw(tty, tparm(change_scroll_region, 0, ws.ws_row - 1));
		if (exit_alt_charset_mode != NULL)
			tty_puts(tty, exit_alt_charset_mode);
		if (exit_attribute_mode != NULL)
			tty_raw(tty, exit_attribute_mode);
		tty_raw(tty, clear_screen);
		if (keypad_local != NULL)
			tty_raw(tty, keypad_local);
		if (exit_ca_mode != NULL)
			tty_raw(tty, exit_ca_mode);
		if (cursor_normal != NULL)
			tty_raw(tty, cursor_normal);
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
	char		*s;

	SLIST_FOREACH(term, &tty_terms, entry) {
		if (strcmp(term->name, name) == 0) {
			term->references++;
			return (term);
		}
	}

	term = xmalloc(sizeof *term);
	term->name = xstrdup(name);
	term->term = NULL;
	term->references = 1;
	SLIST_INSERT_HEAD(&tty_terms, term, entry);

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
	s = tigetstr("orig_pair");
	if (s != NULL && s != (char *) -1 && strcmp(s, "\033[39;49m") == 0)
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

	SLIST_REMOVE(&tty_terms, term, tty_term, entry);

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
	if (*t == '\0')
		return;
	buffer_write(tty->out, t, strlen(t));

	if (tty->log_fd != -1)
		write(tty->log_fd, t, strlen(t));
}

void
tty_putc(struct tty *tty, char ch)
{
	if (tty->cell.attr & GRID_ATTR_CHARSET)
		ch = tty_get_acs(tty, ch);
	buffer_write8(tty->out, ch);

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
tty_vwrite(struct tty *tty, struct screen *s, int cmd, va_list ap)
{
	if (tty->flags & TTY_FREEZE)
		return;

	if (tty->term == NULL) /* XXX XXX */
		return;
	set_curterm(tty->term->term);

	if (tty_cmds[cmd] != NULL)
		tty_cmds[cmd](tty, s, ap);
}

void
tty_cmd_cursorup(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int ua;

	ua = va_arg(ap, u_int);

	if (parm_up_cursor != NULL)
		tty_puts(tty, tparm(parm_up_cursor, ua));
	else {
		while (ua-- > 0)
			tty_puts(tty, cursor_up);
	}
}

void
tty_cmd_cursordown(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	if (parm_down_cursor != NULL)
		tty_puts(tty, tparm(parm_down_cursor, ua));
	else {
		while (ua-- > 0)
			tty_puts(tty, cursor_down);
	}
}

void
tty_cmd_cursorright(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	if (parm_right_cursor != NULL)
		tty_puts(tty, tparm(parm_right_cursor, ua));
	else {
		while (ua-- > 0)
			tty_puts(tty, cursor_right);
	}
}

void
tty_cmd_cursorleft(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	if (parm_left_cursor != NULL)
		tty_puts(tty, tparm(parm_left_cursor, ua));
	else {
		while (ua-- > 0)
			tty_puts(tty, cursor_left);
	}
}

void
tty_cmd_insertcharacter(struct tty *tty, struct screen *s, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);

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
}

void
tty_cmd_deletecharacter(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);

	if (parm_dch != NULL)
		tty_puts(tty, tparm(parm_dch, ua));
	else if (delete_character != NULL) {
		while (ua-- > 0)
			tty_puts(tty, delete_character);
	} else {
		while (ua-- > 0)
			tty_putc(tty, '\010');
	}
}

void
tty_cmd_insertline(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);

	if (parm_insert_line != NULL)
		tty_puts(tty, tparm(parm_insert_line, ua));
	else {
		while (ua-- > 0)
			tty_puts(tty, insert_line);
	}
}

void
tty_cmd_deleteline(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	ua = va_arg(ap, u_int);

	tty_reset(tty);

	if (parm_delete_line != NULL)
		tty_puts(tty, tparm(parm_delete_line, ua));
	else {
		while (ua-- > 0)
			tty_puts(tty, delete_line);
	}
}

void
tty_cmd_clearline(struct tty *tty, struct screen *s, unused va_list ap)
{
	u_int	i;

	tty_reset(tty);

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
}

void
tty_cmd_clearendofline(struct tty *tty, struct screen *s, unused va_list ap)
{
	u_int	i;

	tty_reset(tty);

	if (clr_eol != NULL)
		tty_puts(tty, clr_eol);
	else {
		tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
		for (i = s->cx; i < screen_size_x(s); i++)
			tty_putc(tty, ' ');
		tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
	}
}

void
tty_cmd_clearstartofline(struct tty *tty, struct screen *s, unused va_list ap)
{
	u_int	i;

	tty_reset(tty);

	if (clr_bol != NULL)
		tty_puts(tty, clr_bol);
	else {
		tty_puts(tty, tparm(cursor_address, s->cy, 0));
		for (i = 0; i < s->cx + 1; i++)
			tty_putc(tty, ' ');
		tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
	}
}

void
tty_cmd_cursormove(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua, ub;

	ua = va_arg(ap, u_int);
	ub = va_arg(ap, u_int);

	tty_puts(tty, tparm(cursor_address, ub, ua));
}

void
tty_cmd_cursormode(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	if (cursor_normal == NULL || cursor_invisible == NULL)
		return;

	ua = va_arg(ap, int);

	if (ua && !(tty->flags & TTY_NOCURSOR))
		tty_puts(tty, cursor_normal);
	else
		tty_puts(tty, cursor_invisible);
}

void
tty_cmd_reverseindex(
    struct tty *tty, unused struct screen *s, unused va_list ap)
{
	tty_reset(tty);
	tty_puts(tty, scroll_reverse);
}

void
tty_cmd_scrollregion(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua, ub;

	ua = va_arg(ap, u_int);
	ub = va_arg(ap, u_int);

	tty_puts(tty, tparm(change_scroll_region, ua, ub));
}

void
tty_cmd_insertmode(unused struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	if (enter_insert_mode == NULL || exit_insert_mode == NULL)
		return;

	ua = va_arg(ap, int);

#if 0
	if (ua)
		tty_puts(tty, enter_insert_mode);
	else
		tty_puts(tty, exit_insert_mode);
#endif
}

void
tty_cmd_mousemode(struct tty *tty, unused struct screen *s, va_list ap)
{
	u_int	ua;

	if (key_mouse == NULL)
		return;

	ua = va_arg(ap, int);

	if (ua)
		tty_puts(tty, "\033[?1000h");
	else
		tty_puts(tty, "\033[?1000l");
}

void
tty_cmd_linefeed(struct tty *tty, unused struct screen *s, unused va_list ap)
{
	tty_reset(tty);

	tty_putc(tty, '\n');
}

void
tty_cmd_carriagereturn(
    struct tty *tty, unused struct screen *s, unused va_list ap)
{
	tty_reset(tty);

	if (carriage_return)
		tty_puts(tty, carriage_return);
	else
		tty_putc(tty, '\r');
}

void
tty_cmd_bell(struct tty *tty, unused struct screen *s, unused va_list ap)
{
	if (bell)
		tty_puts(tty, bell);
}

void
tty_cmd_clearendofscreen(struct tty *tty, struct screen *s, unused va_list ap)
{
	u_int	i, j;

	tty_reset(tty);

	if (clr_eol != NULL) {
		for (i = s->cy; i < screen_size_y(s); i++) {
			tty_puts(tty, clr_eol);
			if (i != screen_size_y(s) - 1)
				tty_puts(tty, cursor_down);
		}
	} else {
		for (i = s->cx; i < screen_size_y(s); i++)
			tty_putc(tty, ' ');
		for (j = s->cy; j < screen_size_y(s); j++) {
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
}

void
tty_cmd_clearstartofscreen(struct tty *tty, struct screen *s, unused va_list ap)
{
	u_int	i, j;

	tty_reset(tty);

	tty_puts(tty, tparm(cursor_address, 0, 0));
	if (clr_eol) {
		for (i = 0; i < s->cy; i++)
			tty_puts(tty, clr_eol);
		tty_puts(tty, tparm(cursor_address, s->cy, 0));
	} else {
		for (j = 0; j < s->cy; j++) {
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	for (i = 0; i < s->cx; i++)
		tty_putc(tty, ' ');
	tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
}

void
tty_cmd_clearscreen(struct tty *tty, struct screen *s, unused va_list ap)
{
	u_int	i, j;

	tty_reset(tty);

	if (clr_eol) {
		tty_puts(tty, tparm(cursor_address, 0, 0));
		for (i = 0; i < screen_size_y(s); i++) {
			tty_puts(tty, clr_eol);
			if (i != screen_size_y(s) - 1)
				tty_puts(tty, cursor_down);
		}
	} else {
 		tty_puts(tty, tparm(cursor_address, 0, 0));
		for (j = 0; j < screen_size_y(s); j++) {
			for (i = 0; i < screen_size_x(s); i++)
				tty_putc(tty, ' ');
		}
	}
	tty_puts(tty, tparm(cursor_address, s->cy, s->cx));
}

void
tty_cmd_cell(struct tty *tty, unused struct screen *s, va_list ap)
{
	struct grid_cell       *gc;
	u_int			i, width;
	u_char			out[4];

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
	if (gc->data < 0xff) {
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

	if (exit_alt_charset_mode != NULL && tc->attr & GRID_ATTR_CHARSET)
		tty_puts(tty, exit_alt_charset_mode);
	tty_puts(tty, exit_attribute_mode);
	memcpy(tc, &grid_default_cell, sizeof *tc);
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
	if ((changed & GRID_ATTR_BRIGHT) && enter_bold_mode != NULL)
		tty_puts(tty, enter_bold_mode);
	if ((changed & GRID_ATTR_DIM) && enter_dim_mode != NULL)
		tty_puts(tty, enter_dim_mode);
	if ((changed & GRID_ATTR_ITALICS) && enter_standout_mode != NULL)
		tty_puts(tty, enter_standout_mode);
	if ((changed & GRID_ATTR_UNDERSCORE) && enter_underline_mode != NULL)
		tty_puts(tty, enter_underline_mode);
	if ((changed & GRID_ATTR_BLINK) && enter_blink_mode != NULL)
		tty_puts(tty, enter_blink_mode);
	if ((changed & GRID_ATTR_REVERSE) && enter_reverse_mode != NULL)
		tty_puts(tty, enter_reverse_mode);
	if ((changed & GRID_ATTR_HIDDEN) && enter_secure_mode != NULL)
		tty_puts(tty, enter_secure_mode);
	if ((changed & GRID_ATTR_CHARSET) && enter_alt_charset_mode != NULL)
		tty_puts(tty, enter_alt_charset_mode);

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
			if (enter_bold_mode != NULL)
				tty_puts(tty, enter_bold_mode);
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
	else if (set_a_foreground != NULL)
		tty_puts(tty, tparm(set_a_foreground, fg));
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
	else if (set_a_background != NULL)
		tty_puts(tty, tparm(set_a_background, bg));
}
