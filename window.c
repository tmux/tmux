/* $Id: window.c,v 1.5 2007-09-19 15:16:23 nicm Exp $ */

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

#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#define TTYDEFCHARS
#include <termios.h>
#include <unistd.h>
#include <util.h>

#include "tmux.h"

/*
 * Each window is attached to a pty. This file contains code to handle them.
 *
 * A window has two buffers attached, these are filled and emptied by the main
 * server poll loop. Output data is received from pty's in screen format,
 * translated and returned as a series of escape sequences and strings.
 * Input data is received in screen format and written directly to the pty
 * (translation is done in the client).
 *
 * Each window also has a "virtual" screen (screen.c) which contains the
 * current state and is redisplayed when the window is reattached to a client.
 *
 * A global list of windows is maintained, and a window may also be a member
 * of any number of sessions. A reference count is maintained and a window
 * removed from the global list and destroyed when it reaches zero.
 */

/* Global window list. */
struct windows	windows;

/* Create a new window. */
struct window *
window_create(const char *cmd, u_int sx, u_int sy)
{
	struct window	*w;
	struct winsize	 ws;
	struct termios	 tio;
	int		 fd, mode;
	char		 pid[16], *ptr, *name;

	memset(&ws, 0, sizeof ws);
	ws.ws_col = sx;
	ws.ws_row = sy;

	memset(&tio, 0, sizeof tio);
	tio.c_iflag = TTYDEF_IFLAG;
	tio.c_oflag = TTYDEF_OFLAG;
	tio.c_lflag = TTYDEF_LFLAG;
	tio.c_cflag = TTYDEF_CFLAG;
	memcpy(&tio.c_cc, ttydefchars, sizeof tio.c_cc);
	cfsetspeed(&tio, TTYDEF_SPEED);

	xsnprintf(pid, sizeof pid, "%ld", (long) getpid());
	switch (forkpty(&fd, NULL, &tio, &ws)) {
	case -1:
		return (NULL);
	case 0:
		if (setenv("TMUX", pid, 1) != 0)
			fatal("setenv failed");
		if (setenv("TERM", "screen", 1) != 0)
			fatal("setenv failed");
		log_close();

		execl(_PATH_BSHELL, "sh", "-c", cmd, (char *) NULL);
		fatal("execl failed");
	}

	if ((mode = fcntl(fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");

	mode = 1;
	if (ioctl(fd, TIOCPKT, &mode) == -1)
		fatal("ioctl failed");

	w = xmalloc(sizeof *w);
	w->fd = fd;
	w->in = buffer_create(BUFSIZ);
	w->out = buffer_create(BUFSIZ);
	screen_create(&w->screen, sx, sy);

	name = xstrdup(cmd);
	if ((ptr = strchr(name, ' ')) != NULL) {
		if (ptr != name && ptr[-1] != '\\')
			*ptr = '\0';
		else {
			while ((ptr = strchr(ptr + 1, ' ')) != NULL) {
				if (ptr[-1] != '\\') {
					*ptr = '\0';
					break;
				}
			}
		}
	}
	strlcpy(w->name, xbasename(name), sizeof w->name);
	xfree(name);

	window_add(&windows, w);
	w->references = 0;

	return (w);
}

/* Find window index in list. */
int
window_index(struct windows *ww, struct window *w, u_int *i)
{
	for (*i = 0; *i < ARRAY_LENGTH(ww); (*i)++) {
		if (w == ARRAY_ITEM(ww, *i))
			return (0);
	}
	return (-1);
}
	
/* Add a window to a list. */
void
window_add(struct windows *ww, struct window *w)
{
	u_int	i;

	if (window_index(ww, NULL, &i) != 0)
		ARRAY_ADD(ww, w);
	else
		ARRAY_SET(ww, i, w);

	w->references++;
}

/* Remove a window from a list. */
void
window_remove(struct windows *ww, struct window *w)
{
	u_int	i;

	if (window_index(ww, w, &i) != 0)
		fatalx("window not found");
	ARRAY_SET(ww, i, NULL);

	w->references--;
	if (w->references == 0) {
		window_remove(&windows, w);
		window_destroy(w);
	}
}

/* Destroy a window. */
void
window_destroy(struct window *w)
{
	close(w->fd);

	buffer_destroy(w->in);
	buffer_destroy(w->out);
	xfree(w);
}

/* Locate next window in list. */
struct window *
window_next(struct windows *ww, struct window *w)
{
	u_int	i;

	if (window_index(ww, w, &i) != 0)
		fatalx("window not found");

	if (i == ARRAY_LENGTH(ww) - 1)
		return (NULL);
	do {
		i++;
		w = window_at(ww, i);
		if (w != NULL)
			return (w);
	} while (i != ARRAY_LENGTH(ww) - 1);
	return (NULL);
} 

/* Locate previous window in list. */
struct window *
window_previous(struct windows *ww, struct window *w)
{
	u_int	i;

	if (window_index(ww, w, &i) != 0)
		fatalx("window not found");
	if (i == 0)
		return (NULL);
	do {
		i--;
		w = window_at(ww, i);
		if (w != NULL)
			return (w);
	} while (i != 0);
	return (NULL);
}

/* Locate window at specific position in list. */
struct window *
window_at(struct windows *ww, u_int i)
{
	if (i >= ARRAY_LENGTH(ww))
		return (NULL);
	return (ARRAY_ITEM(ww, i));
} 

/* Resize a window. */
int
window_resize(struct window *w, u_int sx, u_int sy)
{
	struct winsize	ws;

	if (sx == w->screen.sx && sy == w->screen.sy)
		return (-1);
		
	memset(&ws, 0, sizeof ws);
	ws.ws_col = sx;
	ws.ws_row = sy;

	screen_resize(&w->screen, sx, sy);

	if (ioctl(w->fd, TIOCSWINSZ, &ws) == -1)
		fatal("ioctl failed");
	return (0);
}

/* Handle window poll results. This is special because of TIOCPKT. */
int
window_poll(struct window *w, struct pollfd *pfd)
{
	struct termios	 tio;
	size_t	 	 size;
	u_char		*ptr;

	size = BUFFER_USED(w->in);
	if (buffer_poll(pfd, w->in, w->out) != 0)
		return (-1);

	if (BUFFER_USED(w->in) == size)
		return (0);
	ptr = BUFFER_IN(w->in) - (BUFFER_USED(w->in) - size);

	log_debug("window packet: %hhu", *ptr);
	switch (*ptr) {
	case TIOCPKT_DATA:
	case TIOCPKT_FLUSHREAD:
	case TIOCPKT_FLUSHWRITE:
	case TIOCPKT_STOP:
	case TIOCPKT_START:
	case TIOCPKT_DOSTOP:
	case TIOCPKT_NOSTOP:
		buffer_delete_range(w->in, size, 1);
		break;
	case TIOCPKT_IOCTL:
		buffer_delete_range(w->in, size, 1 + sizeof tio);
		break;
	}

	return (0);
}

/* Process window input. */
void
window_input(struct window *w, struct buffer *b, size_t size)
{
	int	key;

	while (size != 0) {
		if (size < 1)
			break;
		size--;
		key = input_extract8(b);
		if (key == '\e') {
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			key = (int16_t) input_extract16(b);
		}
		input_key(w->out, key);
	}
}

/*
 * Process window output. Output is translated into a series of escape
 * sequences and strings and returned.
 */
void
window_output(struct window *w, struct buffer *b)
{
	size_t	used;

	used = input_parse(
	    BUFFER_OUT(w->in), BUFFER_USED(w->in), b, &w->screen);
	if (used != 0)
		buffer_remove(w->in, used);
}
