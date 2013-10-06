/* $Id$ */

/*
 * Copyright (c) 2011 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Build a list of key-value pairs and use them to expand #{key} entries in a
 * string.
 */

int	 format_replace(struct format_tree *, const char *, size_t, char **,
	     size_t *, size_t *);
char	*format_get_command(struct window_pane *);
void	 format_window_pane_tabs(struct format_tree *, struct window_pane *);

/* Format key-value replacement entry. */
RB_GENERATE(format_tree, format_entry, entry, format_cmp);

/* Format tree comparison function. */
int
format_cmp(struct format_entry *fe1, struct format_entry *fe2)
{
	return (strcmp(fe1->key, fe2->key));
}

/* Single-character uppercase aliases. */
const char *format_upper[] = {
	NULL,		/* A */
	NULL,		/* B */
	NULL,		/* C */
	"pane_id",	/* D */
	NULL,		/* E */
	"window_flags",	/* F */
	NULL,		/* G */
	"host",		/* H */
	"window_index",	/* I */
	NULL,		/* J */
	NULL,		/* K */
	NULL,		/* L */
	NULL,		/* M */
	NULL,		/* N */
	NULL,		/* O */
	"pane_index",	/* P */
	NULL,		/* Q */
	NULL,		/* R */
	"session_name",	/* S */
	"pane_title",	/* T */
	NULL,		/* U */
	NULL,		/* V */
	"window_name",	/* W */
	NULL,		/* X */
	NULL,		/* Y */
	NULL 		/* Z */
};

/* Single-character lowercase aliases. */
const char *format_lower[] = {
	NULL,		/* a */
	NULL,		/* b */
	NULL,		/* c */
	NULL,		/* d */
	NULL,		/* e */
	NULL,		/* f */
	NULL,		/* g */
	"host_short",	/* h */
	NULL,		/* i */
	NULL,		/* j */
	NULL,		/* k */
	NULL,		/* l */
	NULL,		/* m */
	NULL,		/* n */
	NULL,		/* o */
	NULL,		/* p */
	NULL,		/* q */
	NULL,		/* r */
	NULL,		/* s */
	NULL,		/* t */
	NULL,		/* u */
	NULL,		/* v */
	NULL,		/* w */
	NULL,		/* x */
	NULL,		/* y */
	NULL		/* z */
};

/* Create a new tree. */
struct format_tree *
format_create(void)
{
	struct format_tree	*ft;
	char			 host[MAXHOSTNAMELEN], *ptr;

	ft = xmalloc(sizeof *ft);
	RB_INIT(ft);

	if (gethostname(host, sizeof host) == 0) {
		format_add(ft, "host", "%s", host);
		if ((ptr = strchr(host, '.')) != NULL)
			*ptr = '\0';
		format_add(ft, "host_short", "%s", host);
	}

	return (ft);
}

/* Free a tree. */
void
format_free(struct format_tree *ft)
{
	struct format_entry	*fe, *fe_next;

	fe_next = RB_MIN(format_tree, ft);
	while (fe_next != NULL) {
		fe = fe_next;
		fe_next = RB_NEXT(format_tree, ft, fe);

		RB_REMOVE(format_tree, ft, fe);
		free(fe->value);
		free(fe->key);
		free(fe);
	}

	free(ft);
}

/* Add a key-value pair. */
void
format_add(struct format_tree *ft, const char *key, const char *fmt, ...)
{
	struct format_entry	*fe;
	struct format_entry	*fe_now;
	va_list			 ap;

	fe = xmalloc(sizeof *fe);
	fe->key = xstrdup(key);

	va_start(ap, fmt);
	xvasprintf(&fe->value, fmt, ap);
	va_end(ap);

	fe_now = RB_INSERT(format_tree, ft, fe);
	if (fe_now != NULL) {
		free(fe_now->value);
		fe_now->value = fe->value;
		free(fe->key);
		free(fe);
	}
}

/* Find a format entry. */
const char *
format_find(struct format_tree *ft, const char *key)
{
	struct format_entry	*fe, fe_find;

	fe_find.key = (char *) key;
	fe = RB_FIND(format_tree, ft, &fe_find);
	if (fe == NULL)
		return (NULL);
	return (fe->value);
}

/*
 * Replace a key/value pair in buffer. #{blah} is expanded directly,
 * #{?blah,a,b} is replace with a if blah exists and is nonzero else b.
 */
int
format_replace(struct format_tree *ft, const char *key, size_t keylen,
    char **buf, size_t *len, size_t *off)
{
	char		*copy, *copy0, *endptr, *ptr, *saved;
	const char	*value;
	size_t		 valuelen;
	u_long		 limit = ULONG_MAX;

	/* Make a copy of the key. */
	copy0 = copy = xmalloc(keylen + 1);
	memcpy(copy, key, keylen);
	copy[keylen] = '\0';

	/* Is there a length limit or whatnot? */
	if (!islower((u_char) *copy) && *copy != '?') {
		while (*copy != ':' && *copy != '\0') {
			switch (*copy) {
			case '=':
				errno = 0;
				limit = strtoul(copy + 1, &endptr, 10);
				if (errno == ERANGE && limit == ULONG_MAX)
					goto fail;
				copy = endptr;
				break;
			default:
				copy++;
				break;
			}
		}
		if (*copy != ':')
			goto fail;
		copy++;
	}

	/*
	 * Is this a conditional? If so, check it exists and extract either the
	 * first or second element. If not, look up the key directly.
	 */
	if (*copy == '?') {
		ptr = strchr(copy, ',');
		if (ptr == NULL)
			goto fail;
		*ptr = '\0';

		value = format_find(ft, copy + 1);
		if (value != NULL && (value[0] != '0' || value[1] != '\0')) {
			value = ptr + 1;
			ptr = strchr(value, ',');
			if (ptr == NULL)
				goto fail;
			*ptr = '\0';
		} else {
			ptr = strchr(ptr + 1, ',');
			if (ptr == NULL)
				goto fail;
			value = ptr + 1;
		}
		saved = format_expand(ft, value);
		value = saved;
	} else {
		value = format_find(ft, copy);
		if (value == NULL)
			value = "";
		saved = NULL;
	}
	valuelen = strlen(value);

	/* Truncate the value if needed. */
	if (valuelen > limit)
		valuelen = limit;

	/* Expand the buffer and copy in the value. */
	while (*len - *off < valuelen + 1) {
		*buf = xrealloc(*buf, 2, *len);
		*len *= 2;
	}
	memcpy(*buf + *off, value, valuelen);
	*off += valuelen;

	free(saved);
	free(copy0);
	return (0);

fail:
	free(copy0);
	return (-1);
}

/* Expand keys in a template. */
char *
format_expand(struct format_tree *ft, const char *fmt)
{
	char		*buf;
	const char	*ptr, *s;
	size_t		 off, len, n;
	int     	 ch, brackets;

	len = 64;
	buf = xmalloc(len);
	off = 0;

	while (*fmt != '\0') {
		if (*fmt != '#') {
			while (len - off < 2) {
				buf = xrealloc(buf, 2, len);
				len *= 2;
			}
			buf[off++] = *fmt++;
			continue;
		}
		fmt++;

		ch = (u_char) *fmt++;
		switch (ch) {
		case '{':
			brackets = 1;
			for (ptr = fmt; *ptr != '\0'; ptr++) {
				if (*ptr == '{')
					brackets++;
				if (*ptr == '}' && --brackets == 0)
					break;
			}
			if (*ptr != '}' || brackets != 0)
				break;
			n = ptr - fmt;

			if (format_replace(ft, fmt, n, &buf, &len, &off) != 0)
				break;
			fmt += n + 1;
			continue;
		default:
			s = NULL;
			if (ch >= 'A' && ch <= 'Z')
				s = format_upper[ch - 'A'];
			else if (ch >= 'a' && ch <= 'z')
				s = format_lower[ch - 'a'];
			if (s == NULL) {
				while (len - off < 3) {
					buf = xrealloc(buf, 2, len);
					len *= 2;
				}
				buf[off++] = '#';
				buf[off++] = ch;
				continue;
			}
			n = strlen(s);
			if (format_replace(ft, s, n, &buf, &len, &off) != 0)
				break;
			continue;
		}

		break;
	}
	buf[off] = '\0';

	return (buf);
}

/* Get command name for format. */
char *
format_get_command(struct window_pane *wp)
{
	char	*cmd;

	cmd = osdep_get_name(wp->fd, wp->tty);
	if (cmd == NULL || *cmd == '\0') {
		cmd = wp->cmd;
		if (cmd == NULL || *cmd == '\0')
			cmd = wp->shell;
	}
	return (parse_window_name(cmd));
}

/* Set default format keys for a session. */
void
format_session(struct format_tree *ft, struct session *s)
{
	struct session_group	*sg;
	char			*tim;
	time_t			 t;

	format_add(ft, "session_name", "%s", s->name);
	format_add(ft, "session_windows", "%u", winlink_count(&s->windows));
	format_add(ft, "session_width", "%u", s->sx);
	format_add(ft, "session_height", "%u", s->sy);
	format_add(ft, "session_id", "$%u", s->id);

	sg = session_group_find(s);
	format_add(ft, "session_grouped", "%d", sg != NULL);
	if (sg != NULL)
		format_add(ft, "session_group", "%u", session_group_index(sg));

	t = s->creation_time.tv_sec;
	format_add(ft, "session_created", "%lld", (long long) t);
	tim = ctime(&t);
	*strchr(tim, '\n') = '\0';
	format_add(ft, "session_created_string", "%s", tim);

	if (s->flags & SESSION_UNATTACHED)
		format_add(ft, "session_attached", "%d", 0);
	else
		format_add(ft, "session_attached", "%d", 1);
}

/* Set default format keys for a client. */
void
format_client(struct format_tree *ft, struct client *c)
{
	char		*tim;
	time_t		 t;
	struct session	*s;

	format_add(ft, "client_height", "%u", c->tty.sy);
	format_add(ft, "client_width", "%u", c->tty.sx);
	if (c->tty.path != NULL)
		format_add(ft, "client_tty", "%s", c->tty.path);
	if (c->tty.termname != NULL)
		format_add(ft, "client_termname", "%s", c->tty.termname);

	t = c->creation_time.tv_sec;
	format_add(ft, "client_created", "%lld", (long long) t);
	tim = ctime(&t);
	*strchr(tim, '\n') = '\0';
	format_add(ft, "client_created_string", "%s", tim);

	t = c->activity_time.tv_sec;
	format_add(ft, "client_activity", "%lld", (long long) t);
	tim = ctime(&t);
	*strchr(tim, '\n') = '\0';
	format_add(ft, "client_activity_string", "%s", tim);

	format_add(ft, "client_prefix", "%d", !!(c->flags & CLIENT_PREFIX));

	if (c->tty.flags & TTY_UTF8)
		format_add(ft, "client_utf8", "%d", 1);
	else
		format_add(ft, "client_utf8", "%d", 0);

	if (c->flags & CLIENT_READONLY)
		format_add(ft, "client_readonly", "%d", 1);
	else
		format_add(ft, "client_readonly", "%d", 0);

	s = c->session;
	if (s != NULL)
		format_add(ft, "client_session", "%s", s->name);
	s = c->last_session;
	if (s != NULL && session_alive(s))
		format_add(ft, "client_last_session", "%s", s->name);
}

/* Set default format keys for a window. */
void
format_window(struct format_tree *ft, struct window *w)
{
	char	*layout;

	layout = layout_dump(w);

	format_add(ft, "window_id", "@%u", w->id);
	format_add(ft, "window_name", "%s", w->name);
	format_add(ft, "window_width", "%u", w->sx);
	format_add(ft, "window_height", "%u", w->sy);
	format_add(ft, "window_layout", "%s", layout);
	format_add(ft, "window_panes", "%u", window_count_panes(w));

	free(layout);
}

/* Set default format keys for a winlink. */
void
format_winlink(struct format_tree *ft, struct session *s, struct winlink *wl)
{
	struct window	*w = wl->window;
	char		*flags;

	flags = window_printable_flags(s, wl);

	format_window(ft, w);

	format_add(ft, "window_index", "%d", wl->idx);
	format_add(ft, "window_flags", "%s", flags);
	format_add(ft, "window_active", "%d", wl == s->curw);

	format_add(ft, "window_bell_flag", "%u",
	    !!(wl->flags & WINLINK_BELL));
	format_add(ft, "window_content_flag", "%u",
	    !!(wl->flags & WINLINK_CONTENT));
	format_add(ft, "window_activity_flag", "%u",
	    !!(wl->flags & WINLINK_ACTIVITY));
	format_add(ft, "window_silence_flag", "%u",
	    !!(wl->flags & WINLINK_SILENCE));


	free(flags);
}

/* Add window pane tabs. */
void
format_window_pane_tabs(struct format_tree *ft, struct window_pane *wp)
{
	struct evbuffer	*buffer;
	u_int		 i;

	buffer = evbuffer_new();
	for (i = 0; i < wp->base.grid->sx; i++) {
		if (!bit_test(wp->base.tabs, i))
			continue;

		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%d", i);
	}

	format_add(ft, "pane_tabs", "%.*s", (int) EVBUFFER_LENGTH(buffer),
	    EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Set default format keys for a window pane. */
void
format_window_pane(struct format_tree *ft, struct window_pane *wp)
{
	struct grid		*gd = wp->base.grid;
	struct grid_line	*gl;
	unsigned long long	 size;
	u_int			 i, idx;
	const char		*cwd;
	char			*cmd;

	size = 0;
	for (i = 0; i < gd->hsize; i++) {
		gl = &gd->linedata[i];
		size += gl->cellsize * sizeof *gl->celldata;
	}
	size += gd->hsize * sizeof *gd->linedata;
	format_add(ft, "history_size", "%u", gd->hsize);
	format_add(ft, "history_limit", "%u", gd->hlimit);
	format_add(ft, "history_bytes", "%llu", size);

	if (window_pane_index(wp, &idx) != 0)
		fatalx("index not found");
	format_add(ft, "pane_index", "%u", idx);

	format_add(ft, "pane_width", "%u", wp->sx);
	format_add(ft, "pane_height", "%u", wp->sy);
	format_add(ft, "pane_title", "%s", wp->base.title);
	format_add(ft, "pane_id", "%%%u", wp->id);
	format_add(ft, "pane_active", "%d", wp == wp->window->active);
	format_add(ft, "pane_dead", "%d", wp->fd == -1);

	format_add(ft, "pane_in_mode", "%d", wp->screen != &wp->base);
	format_add(ft, "pane_synchronized", "%d",
	    !!options_get_number(&wp->window->options, "synchronize-panes"));

	if (wp->tty != NULL)
		format_add(ft, "pane_tty", "%s", wp->tty);
	format_add(ft, "pane_pid", "%ld", (long) wp->pid);
	if (wp->cmd != NULL)
		format_add(ft, "pane_start_command", "%s", wp->cmd);
	if ((cwd = osdep_get_cwd(wp->fd)) != NULL)
		format_add(ft, "pane_current_path", "%s", cwd);
	if ((cmd = format_get_command(wp)) != NULL) {
		format_add(ft, "pane_current_command", "%s", cmd);
		free(cmd);
	}

	format_add(ft, "cursor_x", "%d", wp->base.cx);
	format_add(ft, "cursor_y", "%d", wp->base.cy);
	format_add(ft, "scroll_region_upper", "%d", wp->base.rupper);
	format_add(ft, "scroll_region_lower", "%d", wp->base.rlower);
	format_add(ft, "saved_cursor_x", "%d", wp->ictx.old_cx);
	format_add(ft, "saved_cursor_y", "%d", wp->ictx.old_cy);

	format_add(ft, "alternate_on", "%d", wp->saved_grid ? 1 : 0);
	format_add(ft, "alternate_saved_x", "%d", wp->saved_cx);
	format_add(ft, "alternate_saved_y", "%d", wp->saved_cy);

	format_add(ft, "cursor_flag", "%d",
	    !!(wp->base.mode & MODE_CURSOR));
	format_add(ft, "insert_flag", "%d",
	    !!(wp->base.mode & MODE_INSERT));
	format_add(ft, "keypad_cursor_flag", "%d",
	    !!(wp->base.mode & MODE_KCURSOR));
	format_add(ft, "keypad_flag", "%d",
	    !!(wp->base.mode & MODE_KKEYPAD));
	format_add(ft, "wrap_flag", "%d",
	    !!(wp->base.mode & MODE_WRAP));

	format_add(ft, "mouse_standard_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_STANDARD));
	format_add(ft, "mouse_button_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_BUTTON));
	format_add(ft, "mouse_any_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_ANY));
	format_add(ft, "mouse_utf8_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_UTF8));

	format_window_pane_tabs(ft, wp);
}

/* Set default format keys for paste buffer. */
void
format_paste_buffer(struct format_tree *ft, struct paste_buffer *pb)
{
	char	*pb_print = paste_print(pb, 50);

	format_add(ft, "buffer_size", "%zu", pb->size);
	format_add(ft, "buffer_sample", "%s", pb_print);

	free(pb_print);
}
