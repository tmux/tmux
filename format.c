/* $OpenBSD$ */

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
#include <sys/param.h>
#include <sys/wait.h>

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

struct format_entry;
typedef void (*format_cb)(struct format_tree *, struct format_entry *);

void	 format_job_callback(struct job *);
const char *format_job_get(struct format_tree *, const char *);
void	 format_job_timer(int, short, void *);

void	 format_cb_host(struct format_tree *, struct format_entry *);
void	 format_cb_host_short(struct format_tree *, struct format_entry *);
void	 format_cb_pid(struct format_tree *, struct format_entry *);
void	 format_cb_session_alerts(struct format_tree *, struct format_entry *);
void	 format_cb_window_layout(struct format_tree *, struct format_entry *);
void	 format_cb_start_command(struct format_tree *, struct format_entry *);
void	 format_cb_current_command(struct format_tree *, struct format_entry *);
void	 format_cb_current_path(struct format_tree *, struct format_entry *);
void	 format_cb_history_bytes(struct format_tree *, struct format_entry *);
void	 format_cb_pane_tabs(struct format_tree *, struct format_entry *);

void	 format_add_cb(struct format_tree *, const char *, format_cb);
int	 format_replace(struct format_tree *, const char *, size_t, char **,
	     size_t *, size_t *);
char	*format_time_string(time_t);

void	 format_defaults_pane_tabs(struct format_tree *, struct window_pane *);
void	 format_defaults_session(struct format_tree *, struct session *);
void	 format_defaults_client(struct format_tree *, struct client *);
void	 format_defaults_winlink(struct format_tree *, struct session *,
	     struct winlink *);

/* Entry in format job tree. */
struct format_job {
	const char		*cmd;

	time_t			 last;
	char			*out;

	struct job		*job;
	int			 status;

	RB_ENTRY(format_job)	 entry;
};

/* Format job tree. */
struct event format_job_event;
int	format_job_cmp(struct format_job *, struct format_job *);
RB_HEAD(format_job_tree, format_job) format_jobs = RB_INITIALIZER();
RB_PROTOTYPE(format_job_tree, format_job, entry, format_job_cmp);
RB_GENERATE(format_job_tree, format_job, entry, format_job_cmp);

/* Format job tree comparison function. */
int
format_job_cmp(struct format_job *fj1, struct format_job *fj2)
{
	return (strcmp(fj1->cmd, fj2->cmd));
}

/* Entry in format tree. */
struct format_entry {
	char			*key;
	char			*value;
	format_cb		 cb;
	RB_ENTRY(format_entry)	 entry;
};

/* Format entry tree. */
struct format_tree {
	struct window		*w;
	struct session		*s;
	struct window_pane	*wp;

	int			 flags;

	RB_HEAD(format_entry_tree, format_entry) tree;
};
int	format_entry_cmp(struct format_entry *, struct format_entry *);
RB_PROTOTYPE(format_entry_tree, format_entry, entry, format_entry_cmp);
RB_GENERATE(format_entry_tree, format_entry, entry, format_entry_cmp);

/* Format entry tree comparison function. */
int
format_entry_cmp(struct format_entry *fe1, struct format_entry *fe2)
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

/* Format job callback. */
void
format_job_callback(struct job *job)
{
	struct format_job	*fj = job->data;
	char			*line, *buf;
	size_t			 len;
	struct client		*c;

	fj->job = NULL;
	free(fj->out);

	buf = NULL;
	if ((line = evbuffer_readline(job->event->input)) == NULL) {
		len = EVBUFFER_LENGTH(job->event->input);
		buf = xmalloc(len + 1);
		if (len != 0)
			memcpy(buf, EVBUFFER_DATA(job->event->input), len);
		buf[len] = '\0';
	} else
		buf = line;
	fj->out = buf;

	if (fj->status) {
		TAILQ_FOREACH(c, &clients, entry)
		    server_status_client(c);
		fj->status = 0;
	}

	log_debug("%s: %s: %s", __func__, fj->cmd, fj->out);
}

/* Find a job. */
const char *
format_job_get(struct format_tree *ft, const char *cmd)
{
	struct format_job	fj0, *fj;
	time_t			t;

	fj0.cmd = cmd;
	if ((fj = RB_FIND(format_job_tree, &format_jobs, &fj0)) == NULL) {
		fj = xcalloc(1, sizeof *fj);
		fj->cmd = xstrdup(cmd);

		xasprintf(&fj->out, "<'%s' not ready>", fj->cmd);

		RB_INSERT(format_job_tree, &format_jobs, fj);
	}

	t = time(NULL);
	if (fj->job == NULL && ((ft->flags & FORMAT_FORCE) || fj->last != t)) {
		fj->job = job_run(fj->cmd, NULL, -1, format_job_callback,
		    NULL, fj);
		if (fj->job == NULL) {
			free(fj->out);
			xasprintf(&fj->out, "<'%s' didn't start>", fj->cmd);
		}
		fj->last = t;
	}

	if (ft->flags & FORMAT_STATUS)
		fj->status = 1;

	return (fj->out);
}

/* Remove old jobs. */
void
format_job_timer(unused int fd, unused short events, unused void *arg)
{
	struct format_job	*fj, *fj1;
	time_t			 now;
	struct timeval		 tv = { .tv_sec = 60 };

	now = time(NULL);
	RB_FOREACH_SAFE(fj, format_job_tree, &format_jobs, fj1) {
		if (fj->last > now || now - fj->last < 3600)
			continue;
		RB_REMOVE(format_job_tree, &format_jobs, fj);

		log_debug("%s: %s", __func__, fj->cmd);

		if (fj->job != NULL)
			job_free(fj->job);

		free((void *)fj->cmd);
		free(fj->out);

		free(fj);
	}

	evtimer_del(&format_job_event);
	evtimer_add(&format_job_event, &tv);
}

/* Callback for host. */
void
format_cb_host(unused struct format_tree *ft, struct format_entry *fe)
{
	char host[HOST_NAME_MAX + 1];

	if (gethostname(host, sizeof host) != 0)
		fe->value = xstrdup("");
	else
		fe->value = xstrdup(host);
}

/* Callback for host_short. */
void
format_cb_host_short(unused struct format_tree *ft, struct format_entry *fe)
{
	char host[HOST_NAME_MAX + 1], *cp;

	if (gethostname(host, sizeof host) != 0)
		fe->value = xstrdup("");
	else {
		if ((cp = strchr(host, '.')) != NULL)
			*cp = '\0';
		fe->value = xstrdup(host);
	}
}

/* Callback for pid. */
void
format_cb_pid(unused struct format_tree *ft, struct format_entry *fe)
{
	xasprintf(&fe->value, "%ld", (long)getpid());
}

/* Callback for session_alerts. */
void
format_cb_session_alerts(struct format_tree *ft, struct format_entry *fe)
{
	struct session	*s = ft->s;
	struct winlink	*wl;
	char		 alerts[256], tmp[16];

	if (s == NULL)
		return;

	*alerts = '\0';
	RB_FOREACH(wl, winlinks, &s->windows) {
		if ((wl->flags & WINLINK_ALERTFLAGS) == 0)
			continue;
		xsnprintf(tmp, sizeof tmp, "%u", wl->idx);

		if (*alerts != '\0')
			strlcat(alerts, ",", sizeof alerts);
		strlcat(alerts, tmp, sizeof alerts);
		if (wl->flags & WINLINK_ACTIVITY)
			strlcat(alerts, "#", sizeof alerts);
		if (wl->flags & WINLINK_BELL)
			strlcat(alerts, "!", sizeof alerts);
		if (wl->flags & WINLINK_SILENCE)
			strlcat(alerts, "~", sizeof alerts);
	}
	fe->value = xstrdup(alerts);
}

/* Callback for window_layout. */
void
format_cb_window_layout(struct format_tree *ft, struct format_entry *fe)
{
	struct window	*w = ft->w;

	if (w == NULL)
		return;

	if (w->saved_layout_root != NULL)
		fe->value = layout_dump(w->saved_layout_root);
	else
		fe->value = layout_dump(w->layout_root);
}

/* Callback for pane_start_command. */
void
format_cb_start_command(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;

	if (wp == NULL)
		return;

	fe->value = cmd_stringify_argv(wp->argc, wp->argv);
}

/* Callback for pane_current_command. */
void
format_cb_current_command(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;
	char			*cmd;

	if (wp == NULL)
		return;

	cmd = osdep_get_name(wp->fd, wp->tty);
	if (cmd == NULL || *cmd == '\0') {
		free(cmd);
		cmd = cmd_stringify_argv(wp->argc, wp->argv);
		if (cmd == NULL || *cmd == '\0') {
			free(cmd);
			cmd = xstrdup(wp->shell);
		}
	}
	fe->value = parse_window_name(cmd);
	free(cmd);
}

/* Callback for pane_current_path. */
void
format_cb_current_path(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;
	char			*cwd;

	if (wp == NULL)
		return;

	cwd = osdep_get_cwd(wp->fd);
	if (cwd != NULL)
		fe->value = xstrdup(cwd);
}

/* Callback for history_bytes. */
void
format_cb_history_bytes(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;
	struct grid		*gd;
	struct grid_line	*gl;
	unsigned long long	 size;
	u_int			 i;

	if (wp == NULL)
		return;
	gd = wp->base.grid;

	size = 0;
	for (i = 0; i < gd->hsize; i++) {
		gl = &gd->linedata[i];
		size += gl->cellsize * sizeof *gl->celldata;
	}
	size += gd->hsize * sizeof *gd->linedata;

	xasprintf(&fe->value, "%llu", size);
}

/* Callback for pane_tabs. */
void
format_cb_pane_tabs(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;
	struct evbuffer		*buffer;
	u_int			 i;
	int			 size;

	if (wp == NULL)
		return;

	buffer = evbuffer_new();
	for (i = 0; i < wp->base.grid->sx; i++) {
		if (!bit_test(wp->base.tabs, i))
			continue;

		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%u", i);
	}
	size = EVBUFFER_LENGTH(buffer);
	xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Create a new tree. */
struct format_tree *
format_create(void)
{
	return (format_create_flags(0));
}

/* Create a new tree for the status line. */
struct format_tree *
format_create_flags(int flags)
{
	struct format_tree	*ft;

	if (!event_initialized(&format_job_event)) {
		evtimer_set(&format_job_event, format_job_timer, NULL);
		format_job_timer(-1, 0, NULL);
	}

	ft = xcalloc(1, sizeof *ft);
	RB_INIT(&ft->tree);
	ft->flags = flags;

	format_add_cb(ft, "host", format_cb_host);
	format_add_cb(ft, "host_short", format_cb_host_short);
	format_add_cb(ft, "pid", format_cb_pid);

	return (ft);
}

/* Free a tree. */
void
format_free(struct format_tree *ft)
{
	struct format_entry	*fe, *fe1;

	RB_FOREACH_SAFE(fe, format_entry_tree, &ft->tree, fe1) {
		RB_REMOVE(format_entry_tree, &ft->tree, fe);
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

	fe_now = RB_INSERT(format_entry_tree, &ft->tree, fe);
	if (fe_now != NULL) {
		free(fe->key);
		free(fe);
		free(fe_now->value);
		fe = fe_now;
	}

	fe->cb = NULL;

	va_start(ap, fmt);
	xvasprintf(&fe->value, fmt, ap);
	va_end(ap);
}

/* Add a key and function. */
void
format_add_cb(struct format_tree *ft, const char *key, format_cb cb)
{
	struct format_entry	*fe;
	struct format_entry	*fe_now;

	fe = xmalloc(sizeof *fe);
	fe->key = xstrdup(key);

	fe_now = RB_INSERT(format_entry_tree, &ft->tree, fe);
	if (fe_now != NULL) {
		free(fe->key);
		free(fe);
		free(fe_now->value);
		fe = fe_now;
	}

	fe->cb = cb;

	fe->value = NULL;
}

/* Find a format entry. */
const char *
format_find(struct format_tree *ft, const char *key)
{
	struct format_entry	*fe, fe_find;
	struct options_entry	*o;
	struct environ_entry	*envent;
	static char		 s[16];

	o = options_find(&global_options, key);
	if (o == NULL && ft->w != NULL)
		o = options_find(&ft->w->options, key);
	if (o == NULL)
		o = options_find(&global_w_options, key);
	if (o == NULL && ft->s != NULL)
		o = options_find(&ft->s->options, key);
	if (o == NULL)
		o = options_find(&global_s_options, key);
	if (o != NULL) {
		switch (o->type) {
		case OPTIONS_STRING:
			return (o->str);
		case OPTIONS_NUMBER:
			xsnprintf(s, sizeof s, "%lld", o->num);
			return (s);
		case OPTIONS_STYLE:
			return (style_tostring(&o->style));
		}
	}

	fe_find.key = (char *) key;
	fe = RB_FIND(format_entry_tree, &ft->tree, &fe_find);
	if (fe != NULL) {
		if (fe->value == NULL && fe->cb != NULL)
			fe->cb(ft, fe);
		return (fe->value);
	}

	envent = NULL;
	if (ft->s != NULL)
		envent = environ_find(&ft->s->environ, key);
	if (envent == NULL)
		envent = environ_find(&global_environ, key);
	if (envent != NULL)
		return (envent->value);

	return (NULL);
}

/*
 * Replace a key/value pair in buffer. #{blah} is expanded directly,
 * #{?blah,a,b} is replace with a if blah exists and is nonzero else b.
 */
int
format_replace(struct format_tree *ft, const char *key, size_t keylen,
    char **buf, size_t *len, size_t *off)
{
	char		*copy, *copy0, *endptr, *ptr, *saved, *trimmed;
	const char	*value;
	size_t		 valuelen;
	u_long		 limit = 0;

	/* Make a copy of the key. */
	copy0 = copy = xmalloc(keylen + 1);
	memcpy(copy, key, keylen);
	copy[keylen] = '\0';

	/* Is there a length limit or whatnot? */
	if (!isalpha((u_char) *copy) && *copy != '@' && *copy != '?') {
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
		if (value != NULL && *value != '\0' &&
		    (value[0] != '0' || value[1] != '\0')) {
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

	/* Truncate the value if needed. */
	if (limit != 0) {
		value = trimmed = utf8_trimcstr(value, limit);
		free(saved);
		saved = trimmed;
	}
	valuelen = strlen(value);

	/* Expand the buffer and copy in the value. */
	while (*len - *off < valuelen + 1) {
		*buf = xreallocarray(*buf, 2, *len);
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

/* Expand keys in a template, passing through strftime first. */
char *
format_expand_time(struct format_tree *ft, const char *fmt, time_t t)
{
	char		*tmp, *expanded;
	size_t		 tmplen;
	struct tm	*tm;

	if (fmt == NULL || *fmt == '\0')
		return (xstrdup(""));

	tm = localtime(&t);

	tmp = NULL;
	tmplen = strlen(fmt);

	do {
		tmp = xreallocarray(tmp, 2, tmplen);
		tmplen *= 2;
	} while (strftime(tmp, tmplen, fmt, tm) == 0);

	expanded = format_expand(ft, tmp);
	free(tmp);

	return (expanded);
}

/* Expand keys in a template. */
char *
format_expand(struct format_tree *ft, const char *fmt)
{
	char		*buf, *tmp, *cmd;
	const char	*ptr, *s, *saved = fmt;
	size_t		 off, len, n, slen;
	int     	 ch, brackets;

	if (fmt == NULL)
		return (xstrdup(""));

	len = 64;
	buf = xmalloc(len);
	off = 0;

	while (*fmt != '\0') {
		if (*fmt != '#') {
			while (len - off < 2) {
				buf = xreallocarray(buf, 2, len);
				len *= 2;
			}
			buf[off++] = *fmt++;
			continue;
		}
		fmt++;

		ch = (u_char) *fmt++;
		switch (ch) {
		case '(':
			brackets = 1;
			for (ptr = fmt; *ptr != '\0'; ptr++) {
				if (*ptr == '(')
					brackets++;
				if (*ptr == ')' && --brackets == 0)
					break;
			}
			if (*ptr != ')' || brackets != 0)
				break;
			n = ptr - fmt;

			tmp = xmalloc(n + 1);
			memcpy(tmp, fmt, n);
			tmp[n] = '\0';
			cmd = format_expand(ft, tmp);

			s = format_job_get(ft, cmd);
			slen = strlen(s);

			free(cmd);
			free(tmp);

			while (len - off < slen + 1) {
				buf = xreallocarray(buf, 2, len);
				len *= 2;
			}
			memcpy(buf + off, s, slen);
			off += slen;

			fmt += n + 1;
			continue;
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
		case '#':
			while (len - off < 2) {
				buf = xreallocarray(buf, 2, len);
				len *= 2;
			}
			buf[off++] = '#';
			continue;
		default:
			s = NULL;
			if (ch >= 'A' && ch <= 'Z')
				s = format_upper[ch - 'A'];
			else if (ch >= 'a' && ch <= 'z')
				s = format_lower[ch - 'a'];
			if (s == NULL) {
				while (len - off < 3) {
					buf = xreallocarray(buf, 2, len);
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

	log_debug("format '%s' -> '%s'", saved, buf);
	return (buf);
}

/* Get time as a string. */
char *
format_time_string(time_t t)
{
	char	*tim;

	tim = ctime(&t);
	*strchr(tim, '\n') = '\0';

	return (tim);
}

/* Set defaults for any of arguments that are not NULL. */
void
format_defaults(struct format_tree *ft, struct client *c, struct session *s,
    struct winlink *wl, struct window_pane *wp)
{
	if (s == NULL && c != NULL)
		s = c->session;
	if (wl == NULL && s != NULL)
		wl = s->curw;
	if (wp == NULL && wl != NULL)
		wp = wl->window->active;

	if (c != NULL)
		format_defaults_client(ft, c);
	if (s != NULL)
		format_defaults_session(ft, s);
	if (s != NULL && wl != NULL)
		format_defaults_winlink(ft, s, wl);
	if (wp != NULL)
		format_defaults_pane(ft, wp);
}

/* Set default format keys for a session. */
void
format_defaults_session(struct format_tree *ft, struct session *s)
{
	struct session_group	*sg;
	time_t			 t;

	ft->s = s;

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
	format_add(ft, "session_created_string", "%s", format_time_string(t));

	t = s->last_attached_time.tv_sec;
	if (t != 0) { /* zero if never attached */
		format_add(ft, "session_last_attached", "%lld", (long long) t);
		format_add(ft, "session_last_attached_string", "%s",
		    format_time_string(t));
	}

	t = s->activity_time.tv_sec;
	format_add(ft, "session_activity", "%lld", (long long) t);
	format_add(ft, "session_activity_string", "%s", format_time_string(t));

	format_add(ft, "session_attached", "%u", s->attached);
	format_add(ft, "session_many_attached", "%d", s->attached > 1);

	format_add_cb(ft, "session_alerts", format_cb_session_alerts);
}

/* Set default format keys for a client. */
void
format_defaults_client(struct format_tree *ft, struct client *c)
{
	struct session	*s;
	time_t		 t;

	if (ft->s == NULL)
		ft->s = c->session;

	format_add(ft, "client_pid", "%ld", (long) c->pid);
	format_add(ft, "client_height", "%u", c->tty.sy);
	format_add(ft, "client_width", "%u", c->tty.sx);
	if (c->tty.path != NULL)
		format_add(ft, "client_tty", "%s", c->tty.path);
	if (c->tty.termname != NULL)
		format_add(ft, "client_termname", "%s", c->tty.termname);
	format_add(ft, "client_control_mode", "%d",
		!!(c->flags & CLIENT_CONTROL));

	t = c->creation_time.tv_sec;
	format_add(ft, "client_created", "%lld", (long long) t);
	format_add(ft, "client_created_string", "%s", format_time_string(t));

	t = c->activity_time.tv_sec;
	format_add(ft, "client_activity", "%lld", (long long) t);
	format_add(ft, "client_activity_string", "%s", format_time_string(t));

	if (strcmp(c->keytable->name, "root") == 0)
		format_add(ft, "client_prefix", "%d", 0);
	else
		format_add(ft, "client_prefix", "%d", 1);
	format_add(ft, "client_key_table", "%s", c->keytable->name);

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
format_defaults_window(struct format_tree *ft, struct window *w)
{
	time_t	 t;

	ft->w = w;

	t = w->activity_time.tv_sec;
	format_add(ft, "window_activity", "%lld", (long long) t);
	format_add(ft, "window_activity_string", "%s", format_time_string(t));

	format_add(ft, "window_id", "@%u", w->id);
	format_add(ft, "window_name", "%s", w->name);
	format_add(ft, "window_width", "%u", w->sx);
	format_add(ft, "window_height", "%u", w->sy);
	format_add_cb(ft, "window_layout", format_cb_window_layout);
	format_add(ft, "window_panes", "%u", window_count_panes(w));
	format_add(ft, "window_zoomed_flag", "%d",
	    !!(w->flags & WINDOW_ZOOMED));
}

/* Set default format keys for a winlink. */
void
format_defaults_winlink(struct format_tree *ft, struct session *s,
    struct winlink *wl)
{
	struct window	*w = wl->window;
	char		*flags;

	if (ft->w == NULL)
		ft->w = wl->window;

	flags = window_printable_flags(s, wl);

	format_defaults_window(ft, w);

	format_add(ft, "window_index", "%d", wl->idx);
	format_add(ft, "window_flags", "%s", flags);
	format_add(ft, "window_active", "%d", wl == s->curw);

	format_add(ft, "window_bell_flag", "%d",
	    !!(wl->flags & WINLINK_BELL));
	format_add(ft, "window_activity_flag", "%d",
	    !!(wl->flags & WINLINK_ACTIVITY));
	format_add(ft, "window_silence_flag", "%d",
	    !!(wl->flags & WINLINK_SILENCE));
	format_add(ft, "window_last_flag", "%d",
	    !!(wl == TAILQ_FIRST(&s->lastw)));
	format_add(ft, "window_linked", "%d", session_is_linked(s, wl->window));

	free(flags);
}

/* Set default format keys for a window pane. */
void
format_defaults_pane(struct format_tree *ft, struct window_pane *wp)
{
	struct grid	*gd = wp->base.grid;
	u_int		 idx;
	int  		 status;

	if (ft->w == NULL)
		ft->w = wp->window;
	ft->wp = wp;

	format_add(ft, "history_size", "%u", gd->hsize);
	format_add(ft, "history_limit", "%u", gd->hlimit);
	format_add_cb(ft, "history_bytes", format_cb_history_bytes);

	if (window_pane_index(wp, &idx) != 0)
		fatalx("index not found");
	format_add(ft, "pane_index", "%u", idx);

	format_add(ft, "pane_width", "%u", wp->sx);
	format_add(ft, "pane_height", "%u", wp->sy);
	format_add(ft, "pane_title", "%s", wp->base.title);
	format_add(ft, "pane_id", "%%%u", wp->id);
	format_add(ft, "pane_active", "%d", wp == wp->window->active);
	format_add(ft, "pane_input_off", "%d", !!(wp->flags & PANE_INPUTOFF));

	status = wp->status;
	if (wp->fd == -1 && WIFEXITED(status))
		format_add(ft, "pane_dead_status", "%d", WEXITSTATUS(status));
	format_add(ft, "pane_dead", "%d", wp->fd == -1);

	if (window_pane_visible(wp)) {
		format_add(ft, "pane_left", "%u", wp->xoff);
		format_add(ft, "pane_top", "%u", wp->yoff);
		format_add(ft, "pane_right", "%u", wp->xoff + wp->sx - 1);
		format_add(ft, "pane_bottom", "%u", wp->yoff + wp->sy - 1);
	}

	format_add(ft, "pane_in_mode", "%d", wp->screen != &wp->base);
	format_add(ft, "pane_synchronized", "%d",
	    !!options_get_number(&wp->window->options, "synchronize-panes"));

	format_add(ft, "pane_tty", "%s", wp->tty);
	format_add(ft, "pane_pid", "%ld", (long) wp->pid);
	format_add_cb(ft, "pane_start_command", format_cb_start_command);
	format_add_cb(ft, "pane_current_command", format_cb_current_command);
	format_add_cb(ft, "pane_current_path", format_cb_current_path);

	format_add(ft, "cursor_x", "%u", wp->base.cx);
	format_add(ft, "cursor_y", "%u", wp->base.cy);
	format_add(ft, "scroll_region_upper", "%u", wp->base.rupper);
	format_add(ft, "scroll_region_lower", "%u", wp->base.rlower);

	format_add(ft, "alternate_on", "%d", wp->saved_grid ? 1 : 0);
	format_add(ft, "alternate_saved_x", "%u", wp->saved_cx);
	format_add(ft, "alternate_saved_y", "%u", wp->saved_cy);

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

	format_add(ft, "mouse_any_flag", "%d",
	    !!(wp->base.mode & (MODE_MOUSE_STANDARD|MODE_MOUSE_BUTTON)));
	format_add(ft, "mouse_standard_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_STANDARD));
	format_add(ft, "mouse_button_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_BUTTON));
	format_add(ft, "mouse_utf8_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_UTF8));

	format_add_cb(ft, "pane_tabs", format_cb_pane_tabs);
}

/* Set default format keys for paste buffer. */
void
format_defaults_paste_buffer(struct format_tree *ft, struct paste_buffer *pb,
    int utf8flag)
{
	size_t	 bufsize;
	char	*s;

	paste_buffer_data(pb, &bufsize);
	format_add(ft, "buffer_size", "%zu", bufsize);
	format_add(ft, "buffer_name", "%s", paste_buffer_name(pb));

	s = paste_make_sample(pb, utf8flag);
	format_add(ft, "buffer_sample", "%s", s);
	free(s);
}
