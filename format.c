/* $OpenBSD$ */

/*
 * Copyright (c) 2011 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <libgen.h>
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

static char	*format_job_get(struct format_tree *, const char *);
static void	 format_job_timer(int, short, void *);

static char	*format_find(struct format_tree *, const char *, int);
static void	 format_add_cb(struct format_tree *, const char *, format_cb);
static void	 format_add_tv(struct format_tree *, const char *,
		     struct timeval *);
static int	 format_replace(struct format_tree *, const char *, size_t,
		     char **, size_t *, size_t *);

static void	 format_defaults_session(struct format_tree *,
		     struct session *);
static void	 format_defaults_client(struct format_tree *, struct client *);
static void	 format_defaults_winlink(struct format_tree *,
		     struct winlink *);

/* Entry in format job tree. */
struct format_job {
	struct client		*client;
	u_int			 tag;
	const char		*cmd;
	const char		*expanded;

	time_t			 last;
	char			*out;
	int			 updated;

	struct job		*job;
	int			 status;

	RB_ENTRY(format_job)	 entry;
};

/* Format job tree. */
static struct event format_job_event;
static int format_job_cmp(struct format_job *, struct format_job *);
static RB_HEAD(format_job_tree, format_job) format_jobs = RB_INITIALIZER();
RB_GENERATE_STATIC(format_job_tree, format_job, entry, format_job_cmp);

/* Format job tree comparison function. */
static int
format_job_cmp(struct format_job *fj1, struct format_job *fj2)
{
	if (fj1->tag < fj2->tag)
		return (-1);
	if (fj1->tag > fj2->tag)
		return (1);
	return (strcmp(fj1->cmd, fj2->cmd));
}

/* Format modifiers. */
#define FORMAT_TIMESTRING 0x1
#define FORMAT_BASENAME 0x2
#define FORMAT_DIRNAME 0x4
#define FORMAT_SUBSTITUTE 0x8

/* Entry in format tree. */
struct format_entry {
	char			*key;
	char			*value;
	time_t			 t;
	format_cb		 cb;
	RB_ENTRY(format_entry)	 entry;
};

/* Format entry tree. */
struct format_tree {
	struct window		*w;
	struct winlink		*wl;
	struct session		*s;
	struct window_pane	*wp;

	struct client		*client;
	u_int			 tag;
	int			 flags;

	RB_HEAD(format_entry_tree, format_entry) tree;
};
static int format_entry_cmp(struct format_entry *, struct format_entry *);
RB_GENERATE_STATIC(format_entry_tree, format_entry, entry, format_entry_cmp);

/* Format entry tree comparison function. */
static int
format_entry_cmp(struct format_entry *fe1, struct format_entry *fe2)
{
	return (strcmp(fe1->key, fe2->key));
}

/* Single-character uppercase aliases. */
static const char *format_upper[] = {
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
static const char *format_lower[] = {
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

/* Format job update callback. */
static void
format_job_update(struct job *job)
{
	struct format_job	*fj = job->data;
	struct evbuffer		*evb = job->event->input;
	char			*line = NULL, *next;
	time_t			 t;

	while ((next = evbuffer_readline(evb)) != NULL) {
		free(line);
		line = next;
	}
	if (line == NULL)
		return;
	fj->updated = 1;

	free(fj->out);
	fj->out = line;

	log_debug("%s: %p %s: %s", __func__, fj, fj->cmd, fj->out);

	t = time(NULL);
	if (fj->status && fj->last != t) {
		if (fj->client != NULL)
			server_status_client(fj->client);
		fj->last = t;
	}
}

/* Format job complete callback. */
static void
format_job_complete(struct job *job)
{
	struct format_job	*fj = job->data;
	char			*line, *buf;
	size_t			 len;

	fj->job = NULL;

	buf = NULL;
	if ((line = evbuffer_readline(job->event->input)) == NULL) {
		len = EVBUFFER_LENGTH(job->event->input);
		buf = xmalloc(len + 1);
		if (len != 0)
			memcpy(buf, EVBUFFER_DATA(job->event->input), len);
		buf[len] = '\0';
	} else
		buf = line;

	log_debug("%s: %p %s: %s", __func__, fj, fj->cmd, buf);

	if (*buf != '\0' || !fj->updated) {
		free(fj->out);
		fj->out = buf;
	} else
		free(buf);

	if (fj->status) {
		if (fj->client != NULL)
			server_status_client(fj->client);
		fj->status = 0;
	}
}

/* Find a job. */
static char *
format_job_get(struct format_tree *ft, const char *cmd)
{
	struct format_job_tree	*jobs;
	struct format_job	 fj0, *fj;
	time_t			 t;
	char			*expanded;
	int			 force;

	if (ft->client == NULL)
		jobs = &format_jobs;
	else if (ft->client->jobs != NULL)
		jobs = ft->client->jobs;
	else {
		jobs = ft->client->jobs = xmalloc(sizeof *ft->client->jobs);
		RB_INIT(jobs);
	}

	fj0.tag = ft->tag;
	fj0.cmd = cmd;
	if ((fj = RB_FIND(format_job_tree, jobs, &fj0)) == NULL) {
		fj = xcalloc(1, sizeof *fj);
		fj->client = ft->client;
		fj->tag = ft->tag;
		fj->cmd = xstrdup(cmd);
		fj->expanded = NULL;

		xasprintf(&fj->out, "<'%s' not ready>", fj->cmd);

		RB_INSERT(format_job_tree, jobs, fj);
	}

	expanded = format_expand(ft, cmd);
	if (fj->expanded == NULL || strcmp(expanded, fj->expanded) != 0) {
		free((void *)fj->expanded);
		fj->expanded = xstrdup(expanded);
		force = 1;
	} else
		force = (ft->flags & FORMAT_FORCE);

	t = time(NULL);
	if (fj->job == NULL && (force || fj->last != t)) {
		fj->job = job_run(expanded, NULL, NULL, format_job_update,
		    format_job_complete, NULL, fj, JOB_NOWAIT);
		if (fj->job == NULL) {
			free(fj->out);
			xasprintf(&fj->out, "<'%s' didn't start>", fj->cmd);
		}
		fj->last = t;
		fj->updated = 0;
	}

	if (ft->flags & FORMAT_STATUS)
		fj->status = 1;

	free(expanded);
	return (format_expand(ft, fj->out));
}

/* Remove old jobs. */
static void
format_job_tidy(struct format_job_tree *jobs, int force)
{
	struct format_job	*fj, *fj1;
	time_t			 now;

	now = time(NULL);
	RB_FOREACH_SAFE(fj, format_job_tree, jobs, fj1) {
		if (!force && (fj->last > now || now - fj->last < 3600))
			continue;
		RB_REMOVE(format_job_tree, jobs, fj);

		log_debug("%s: %s", __func__, fj->cmd);

		if (fj->job != NULL)
			job_free(fj->job);

		free((void *)fj->expanded);
		free((void *)fj->cmd);
		free(fj->out);

		free(fj);
	}
}

/* Remove old jobs for client. */
void
format_lost_client(struct client *c)
{
	if (c->jobs != NULL)
		format_job_tidy(c->jobs, 1);
	free(c->jobs);
}

/* Remove old jobs periodically. */
static void
format_job_timer(__unused int fd, __unused short events, __unused void *arg)
{
	struct client	*c;
	struct timeval	 tv = { .tv_sec = 60 };

	format_job_tidy(&format_jobs, 0);
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->jobs != NULL)
			format_job_tidy(c->jobs, 0);
	}

	evtimer_del(&format_job_event);
	evtimer_add(&format_job_event, &tv);
}

/* Callback for host. */
static void
format_cb_host(__unused struct format_tree *ft, struct format_entry *fe)
{
	char host[HOST_NAME_MAX + 1];

	if (gethostname(host, sizeof host) != 0)
		fe->value = xstrdup("");
	else
		fe->value = xstrdup(host);
}

/* Callback for host_short. */
static void
format_cb_host_short(__unused struct format_tree *ft, struct format_entry *fe)
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
static void
format_cb_pid(__unused struct format_tree *ft, struct format_entry *fe)
{
	xasprintf(&fe->value, "%ld", (long)getpid());
}

/* Callback for session_alerts. */
static void
format_cb_session_alerts(struct format_tree *ft, struct format_entry *fe)
{
	struct session	*s = ft->s;
	struct winlink	*wl;
	char		 alerts[1024], tmp[16];

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

/* Callback for session_stack. */
static void
format_cb_session_stack(struct format_tree *ft, struct format_entry *fe)
{
	struct session	*s = ft->s;
	struct winlink	*wl;
	char		 result[1024], tmp[16];

	if (s == NULL)
		return;

	xsnprintf(result, sizeof result, "%u", s->curw->idx);
	TAILQ_FOREACH(wl, &s->lastw, sentry) {
		xsnprintf(tmp, sizeof tmp, "%u", wl->idx);

		if (*result != '\0')
			strlcat(result, ",", sizeof result);
		strlcat(result, tmp, sizeof result);
	}
	fe->value = xstrdup(result);
}

/* Callback for window_stack_index. */
static void
format_cb_window_stack_index(struct format_tree *ft, struct format_entry *fe)
{
	struct session	*s = ft->wl->session;
	struct winlink	*wl;
	u_int		 idx;

	idx = 0;
	TAILQ_FOREACH(wl, &s->lastw, sentry) {
		idx++;
		if (wl == ft->wl)
			break;
	}
	if (wl != NULL)
		xasprintf(&fe->value, "%u", idx);
	else
		fe->value = xstrdup("0");
}

/* Callback for window_layout. */
static void
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

/* Callback for window_visible_layout. */
static void
format_cb_window_visible_layout(struct format_tree *ft, struct format_entry *fe)
{
	struct window	*w = ft->w;

	if (w == NULL)
		return;

	fe->value = layout_dump(w->layout_root);
}

/* Callback for pane_start_command. */
static void
format_cb_start_command(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;

	if (wp == NULL)
		return;

	fe->value = cmd_stringify_argv(wp->argc, wp->argv);
}

/* Callback for pane_current_command. */
static void
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
static void
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
static void
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
		gl = grid_get_line(gd, i);
		size += gl->cellsize * sizeof *gl->celldata;
		size += gl->extdsize * sizeof *gl->extddata;
	}
	size += gd->hsize * sizeof *gl;

	xasprintf(&fe->value, "%llu", size);
}

/* Callback for pane_tabs. */
static void
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
	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Callback for session_group_list. */
static void
format_cb_session_group_list(struct format_tree *ft, struct format_entry *fe)
{
	struct session		*s = ft->s;
	struct session_group	*sg;
	struct session		*loop;
	struct evbuffer		*buffer;
	int			 size;

	if (s == NULL)
		return;
	sg = session_group_contains(s);
	if (sg == NULL)
		return;

	buffer = evbuffer_new();
	TAILQ_FOREACH(loop, &sg->sessions, gentry) {
		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%s", loop->name);
	}
	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Merge a format tree. */
static void
format_merge(struct format_tree *ft, struct format_tree *from)
{
	struct format_entry	*fe;

	RB_FOREACH(fe, format_entry_tree, &from->tree) {
		if (fe->value != NULL)
			format_add(ft, fe->key, "%s", fe->value);
	}
}

/* Create a new tree. */
struct format_tree *
format_create(struct client *c, struct cmdq_item *item, int tag, int flags)
{
	struct format_tree	*ft;

	if (!event_initialized(&format_job_event)) {
		evtimer_set(&format_job_event, format_job_timer, NULL);
		format_job_timer(-1, 0, NULL);
	}

	ft = xcalloc(1, sizeof *ft);
	RB_INIT(&ft->tree);

	if (c != NULL) {
		ft->client = c;
		ft->client->references++;
	}

	ft->tag = tag;
	ft->flags = flags;

	format_add(ft, "version", "%s", VERSION);
	format_add_cb(ft, "host", format_cb_host);
	format_add_cb(ft, "host_short", format_cb_host_short);
	format_add_cb(ft, "pid", format_cb_pid);
	format_add(ft, "socket_path", "%s", socket_path);
	format_add_tv(ft, "start_time", &start_time);

	if (item != NULL) {
		if (item->cmd != NULL)
			format_add(ft, "command", "%s", item->cmd->entry->name);
		if (item->shared != NULL && item->shared->formats != NULL)
			format_merge(ft, item->shared->formats);
	}

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

	if (ft->client != NULL)
		server_client_unref(ft->client);
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
	fe->t = 0;

	va_start(ap, fmt);
	xvasprintf(&fe->value, fmt, ap);
	va_end(ap);
}

/* Add a key and time. */
static void
format_add_tv(struct format_tree *ft, const char *key, struct timeval *tv)
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

	fe->cb = NULL;
	fe->t = tv->tv_sec;

	fe->value = NULL;
}

/* Add a key and function. */
static void
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
	fe->t = 0;

	fe->value = NULL;
}

/* Find a format entry. */
static char *
format_find(struct format_tree *ft, const char *key, int modifiers)
{
	struct format_entry	*fe, fe_find;
	struct environ_entry	*envent;
	static char		 s[64];
	struct options_entry	*o;
	const char		*found;
	int			 idx;
	char			*copy, *saved;

	if (~modifiers & FORMAT_TIMESTRING) {
		o = options_parse_get(global_options, key, &idx, 0);
		if (o == NULL && ft->w != NULL)
			o = options_parse_get(ft->w->options, key, &idx, 0);
		if (o == NULL)
			o = options_parse_get(global_w_options, key, &idx, 0);
		if (o == NULL && ft->s != NULL)
			o = options_parse_get(ft->s->options, key, &idx, 0);
		if (o == NULL)
			o = options_parse_get(global_s_options, key, &idx, 0);
		if (o != NULL) {
			found = options_tostring(o, idx, 1);
			goto found;
		}
	}
	found = NULL;

	fe_find.key = (char *) key;
	fe = RB_FIND(format_entry_tree, &ft->tree, &fe_find);
	if (fe != NULL) {
		if (modifiers & FORMAT_TIMESTRING) {
			if (fe->t == 0)
				return (NULL);
			ctime_r(&fe->t, s);
			s[strcspn(s, "\n")] = '\0';
			found = s;
			goto found;
		}
		if (fe->t != 0) {
			xsnprintf(s, sizeof s, "%lld", (long long)fe->t);
			found = s;
			goto found;
		}
		if (fe->value == NULL && fe->cb != NULL) {
			fe->cb(ft, fe);
			if (fe->value == NULL)
				fe->value = xstrdup("");
		}
		found = fe->value;
		goto found;
	}

	if (~modifiers & FORMAT_TIMESTRING) {
		envent = NULL;
		if (ft->s != NULL)
			envent = environ_find(ft->s->environ, key);
		if (envent == NULL)
			envent = environ_find(global_environ, key);
		if (envent != NULL) {
			found = envent->value;
			goto found;
		}
	}

	return (NULL);

found:
	if (found == NULL)
		return (NULL);
	copy = xstrdup(found);
	if (modifiers & FORMAT_BASENAME) {
		saved = copy;
		copy = xstrdup(basename(saved));
		free(saved);
	}
	if (modifiers & FORMAT_DIRNAME) {
		saved = copy;
		copy = xstrdup(dirname(saved));
		free(saved);
	}
	return (copy);
}

/* Skip until end. */
static const char *
format_skip(const char *s, char end)
{
	int	brackets = 0;

	for (; *s != '\0'; s++) {
		if (*s == '#' && s[1] == '{')
			brackets++;
		if (*s == '#' && strchr(",#{}", s[1]) != NULL) {
			s++;
			continue;
		}
		if (*s == '}')
			brackets--;
		if (*s == end && brackets == 0)
			break;
	}
	if (*s == '\0')
		return (NULL);
	return (s);
}

/* Return left and right alternatives separated by commas. */
static int
format_choose(char *s, char **left, char **right)
{
	char	*cp;

	cp = (char *)format_skip(s, ',');
	if (cp == NULL)
		return (-1);
	*cp = '\0';

	*left = s;
	*right = cp + 1;
	return (0);
}

/* Is this true? */
int
format_true(const char *s)
{
	if (s != NULL && *s != '\0' && (s[0] != '0' || s[1] != '\0'))
		return (1);
	return (0);
}

/* Replace a key. */
static int
format_replace(struct format_tree *ft, const char *key, size_t keylen,
    char **buf, size_t *len, size_t *off)
{
	struct window_pane	*wp = ft->wp;
	char			*copy, *copy0, *endptr, *ptr, *found, *new, sep;
	char			*value, *from = NULL, *to = NULL, *left, *right;
	size_t			 valuelen, newlen, fromlen, tolen, used;
	long			 limit = 0;
	int			 modifiers = 0, compare = 0, search = 0;
	int			 literal = 0;

	/* Make a copy of the key. */
	copy0 = copy = xmalloc(keylen + 1);
	memcpy(copy, key, keylen);
	copy[keylen] = '\0';

	/* Is there a length limit or whatnot? */
	switch (copy[0]) {
	case 'l':
		if (copy[1] != ':')
			break;
		literal = 1;
		copy += 2;
		break;
	case 'm':
		if (copy[1] != ':')
			break;
		compare = -2;
		copy += 2;
		break;
	case 'C':
		if (copy[1] != ':')
			break;
		search = 1;
		copy += 2;
		break;
	case '|':
		if (copy[1] != '|' || copy[2] != ':')
			break;
		compare = -3;
		copy += 3;
		break;
	case '&':
		if (copy[1] != '&' || copy[2] != ':')
			break;
		compare = -4;
		copy += 3;
		break;
	case '!':
		if (copy[1] == '=' && copy[2] == ':') {
			compare = -1;
			copy += 3;
			break;
		}
		break;
	case '=':
		if (copy[1] == '=' && copy[2] == ':') {
			compare = 1;
			copy += 3;
			break;
		}
		errno = 0;
		limit = strtol(copy + 1, &endptr, 10);
		if (errno == ERANGE && (limit == LONG_MIN || limit == LONG_MAX))
			break;
		if (*endptr != ':')
			break;
		copy = endptr + 1;
		break;
	case 'b':
		if (copy[1] != ':')
			break;
		modifiers |= FORMAT_BASENAME;
		copy += 2;
		break;
	case 'd':
		if (copy[1] != ':')
			break;
		modifiers |= FORMAT_DIRNAME;
		copy += 2;
		break;
	case 't':
		if (copy[1] != ':')
			break;
		modifiers |= FORMAT_TIMESTRING;
		copy += 2;
		break;
	case 's':
		sep = copy[1];
		if (sep == ':' || !ispunct((u_char)sep))
			break;
		from = copy + 2;
		for (copy = from; *copy != '\0' && *copy != sep; copy++)
			/* nothing */;
		if (copy[0] != sep || copy == from) {
			copy = copy0;
			break;
		}
		copy[0] = '\0';
		to = copy + 1;
		for (copy = to; *copy != '\0' && *copy != sep; copy++)
			/* nothing */;
		if (copy[0] != sep || copy[1] != ':') {
			copy = copy0;
			break;
		}
		copy[0] = '\0';

		modifiers |= FORMAT_SUBSTITUTE;
		copy += 2;
		break;
	}

	/* Is this a literal string? */
	if (literal) {
		value = xstrdup(copy);
		goto done;
	}

	/* Is this a comparison or a conditional? */
	if (search) {
		/* Search in pane. */
		if (wp == NULL)
			value = xstrdup("0");
		else
			xasprintf(&value, "%u", window_pane_search(wp, copy));
	} else if (compare != 0) {
		/* Comparison: compare comma-separated left and right. */
		if (format_choose(copy, &left, &right) != 0)
			goto fail;
		left = format_expand(ft, left);
		right = format_expand(ft, right);
		if (compare == -3 &&
		    (format_true(left) || format_true(right)))
			value = xstrdup("1");
		else if (compare == -4 &&
		    (format_true(left) && format_true(right)))
			value = xstrdup("1");
		else if (compare == 1 && strcmp(left, right) == 0)
			value = xstrdup("1");
		else if (compare == -1 && strcmp(left, right) != 0)
			value = xstrdup("1");
		else if (compare == -2 && fnmatch(left, right, 0) == 0)
			value = xstrdup("1");
		else
			value = xstrdup("0");
		free(right);
		free(left);
	} else if (*copy == '?') {
		/* Conditional: check first and choose second or third. */
		ptr = (char *)format_skip(copy, ',');
		if (ptr == NULL)
			goto fail;
		*ptr = '\0';

		found = format_find(ft, copy + 1, modifiers);
		if (found == NULL) {
			/*
			 * If the conditional not found, try to expand it. If
			 * the expansion doesn't have any effect, then assume
			 * false.
			 */
			found = format_expand(ft, copy + 1);
			if (strcmp(found, copy + 1) == 0) {
				free(found);
				found = xstrdup("");
			}
		}
		if (format_choose(ptr + 1, &left, &right) != 0)
			goto fail;

		if (format_true(found))
			value = format_expand(ft, left);
		else
			value = format_expand(ft, right);
		free(found);
	} else {
		/* Neither: look up directly. */
		value = format_find(ft, copy, modifiers);
		if (value == NULL)
			value = xstrdup("");
	}

	/* Perform substitution if any. */
	if (modifiers & FORMAT_SUBSTITUTE) {
		fromlen = strlen(from);
		tolen = strlen(to);

		newlen = strlen(value) + 1;
		copy = new = xmalloc(newlen);
		for (ptr = value; *ptr != '\0'; /* nothing */) {
			if (strncmp(ptr, from, fromlen) != 0) {
				*new++ = *ptr++;
				continue;
			}
			used = new - copy;

			newlen += tolen;
			copy = xrealloc(copy, newlen);

			new = copy + used;
			memcpy(new, to, tolen);

			new += tolen;
			ptr += fromlen;
		}
		*new = '\0';
		free(value);
		value = copy;
	}

	/* Truncate the value if needed. */
	if (limit > 0) {
		new = utf8_trimcstr(value, limit);
		free(value);
		value = new;
	} else if (limit < 0) {
		new = utf8_rtrimcstr(value, -limit);
		free(value);
		value = new;
	}

done:
	/* Expand the buffer and copy in the value. */
	valuelen = strlen(value);
	while (*len - *off < valuelen + 1) {
		*buf = xreallocarray(*buf, 2, *len);
		*len *= 2;
	}
	memcpy(*buf + *off, value, valuelen);
	*off += valuelen;

	free(value);
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
	struct tm	*tm;
	char		 s[2048];

	if (fmt == NULL || *fmt == '\0')
		return (xstrdup(""));

	tm = localtime(&t);

	if (strftime(s, sizeof s, fmt, tm) == 0)
		return (xstrdup(""));

	return (format_expand(ft, s));
}

/* Expand keys in a template. */
char *
format_expand(struct format_tree *ft, const char *fmt)
{
	char		*buf, *out, *name;
	const char	*ptr, *s, *saved = fmt;
	size_t		 off, len, n, outlen;
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

			if (ft->flags & FORMAT_NOJOBS)
				out = xstrdup("");
			else {
				name = xstrndup(fmt, n);
				out = format_job_get(ft, name);
				free(name);
			}
			outlen = strlen(out);

			while (len - off < outlen + 1) {
				buf = xreallocarray(buf, 2, len);
				len *= 2;
			}
			memcpy(buf + off, out, outlen);
			off += outlen;

			free(out);

			fmt += n + 1;
			continue;
		case '{':
			ptr = format_skip(fmt - 2, '}');
			if (ptr == NULL)
				break;
			n = ptr - fmt;

			if (format_replace(ft, fmt, n, &buf, &len, &off) != 0)
				break;
			fmt += n + 1;
			continue;
		case '}':
		case '#':
		case ',':
			while (len - off < 2) {
				buf = xreallocarray(buf, 2, len);
				len *= 2;
			}
			buf[off++] = ch;
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

/* Expand a single string. */
char *
format_single(struct cmdq_item *item, const char *fmt, struct client *c,
    struct session *s, struct winlink *wl, struct window_pane *wp)
{
	struct format_tree	*ft;
	char			*expanded;

	if (item != NULL)
		ft = format_create(item->client, item, FORMAT_NONE, 0);
	else
		ft = format_create(NULL, item, FORMAT_NONE, 0);
	format_defaults(ft, c, s, wl, wp);

	expanded = format_expand(ft, fmt);
	format_free(ft);
	return (expanded);
}

/* Set defaults for any of arguments that are not NULL. */
void
format_defaults(struct format_tree *ft, struct client *c, struct session *s,
    struct winlink *wl, struct window_pane *wp)
{
	if (c != NULL && s != NULL && c->session != s)
		log_debug("%s: session does not match", __func__);

	format_add(ft, "session_format", "%d", s != NULL);
	format_add(ft, "window_format", "%d", wl != NULL);
	format_add(ft, "pane_format", "%d", wp != NULL);

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
	if (wl != NULL)
		format_defaults_winlink(ft, wl);
	if (wp != NULL)
		format_defaults_pane(ft, wp);
}

/* Set default format keys for a session. */
static void
format_defaults_session(struct format_tree *ft, struct session *s)
{
	struct session_group	*sg;

	ft->s = s;

	format_add(ft, "session_name", "%s", s->name);
	format_add(ft, "session_windows", "%u", winlink_count(&s->windows));
	format_add(ft, "session_width", "%u", s->sx);
	format_add(ft, "session_height", "%u", s->sy);
	format_add(ft, "session_id", "$%u", s->id);

	sg = session_group_contains(s);
	format_add(ft, "session_grouped", "%d", sg != NULL);
	if (sg != NULL) {
		format_add(ft, "session_group", "%s", sg->name);
		format_add(ft, "session_group_size", "%u",
		    session_group_count (sg));
		format_add_cb(ft, "session_group_list",
		    format_cb_session_group_list);
	}

	format_add_tv(ft, "session_created", &s->creation_time);
	format_add_tv(ft, "session_last_attached", &s->last_attached_time);
	format_add_tv(ft, "session_activity", &s->activity_time);

	format_add(ft, "session_attached", "%u", s->attached);
	format_add(ft, "session_many_attached", "%d", s->attached > 1);

	format_add_cb(ft, "session_alerts", format_cb_session_alerts);
	format_add_cb(ft, "session_stack", format_cb_session_stack);
}

/* Set default format keys for a client. */
static void
format_defaults_client(struct format_tree *ft, struct client *c)
{
	struct session	*s;
	const char	*name;
	struct tty	*tty = &c->tty;
	const char	*types[] = TTY_TYPES;

	if (ft->s == NULL)
		ft->s = c->session;

	format_add(ft, "client_name", "%s", c->name);
	format_add(ft, "client_pid", "%ld", (long) c->pid);
	format_add(ft, "client_height", "%u", tty->sy);
	format_add(ft, "client_width", "%u", tty->sx);
	format_add(ft, "client_tty", "%s", c->ttyname);
	format_add(ft, "client_control_mode", "%d",
		!!(c->flags & CLIENT_CONTROL));

	if (tty->term_name != NULL)
		format_add(ft, "client_termname", "%s", tty->term_name);
	if (tty->term_name != NULL)
		format_add(ft, "client_termtype", "%s", types[tty->term_type]);

	format_add_tv(ft, "client_created", &c->creation_time);
	format_add_tv(ft, "client_activity", &c->activity_time);

	format_add(ft, "client_written", "%zu", c->written);
	format_add(ft, "client_discarded", "%zu", c->discarded);

	name = server_client_get_key_table(c);
	if (strcmp(c->keytable->name, name) == 0)
		format_add(ft, "client_prefix", "%d", 0);
	else
		format_add(ft, "client_prefix", "%d", 1);
	format_add(ft, "client_key_table", "%s", c->keytable->name);

	if (tty->flags & TTY_UTF8)
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
	ft->w = w;

	format_add_tv(ft, "window_activity", &w->activity_time);
	format_add(ft, "window_id", "@%u", w->id);
	format_add(ft, "window_name", "%s", w->name);
	format_add(ft, "window_width", "%u", w->sx);
	format_add(ft, "window_height", "%u", w->sy);
	format_add_cb(ft, "window_layout", format_cb_window_layout);
	format_add_cb(ft, "window_visible_layout",
	    format_cb_window_visible_layout);
	format_add(ft, "window_panes", "%u", window_count_panes(w));
	format_add(ft, "window_zoomed_flag", "%d",
	    !!(w->flags & WINDOW_ZOOMED));
}

/* Set default format keys for a winlink. */
static void
format_defaults_winlink(struct format_tree *ft, struct winlink *wl)
{
	struct session	*s = wl->session;
	struct window	*w = wl->window;

	if (ft->w == NULL)
		ft->w = wl->window;
	ft->wl = wl;

	format_defaults_window(ft, w);

	format_add(ft, "window_index", "%d", wl->idx);
	format_add_cb(ft, "window_stack_index", format_cb_window_stack_index);
	format_add(ft, "window_flags", "%s", window_printable_flags(wl));
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
}

/* Set default format keys for a window pane. */
void
format_defaults_pane(struct format_tree *ft, struct window_pane *wp)
{
	struct grid	*gd = wp->base.grid;
	int  		 status = wp->status;
	u_int		 idx;

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
	format_add(ft, "pane_pipe", "%d", wp->pipe_fd != -1);

	if ((wp->flags & PANE_STATUSREADY) && WIFEXITED(status))
		format_add(ft, "pane_dead_status", "%d", WEXITSTATUS(status));
	format_add(ft, "pane_dead", "%d", wp->fd == -1);

	if (window_pane_visible(wp)) {
		format_add(ft, "pane_left", "%u", wp->xoff);
		format_add(ft, "pane_top", "%u", wp->yoff);
		format_add(ft, "pane_right", "%u", wp->xoff + wp->sx - 1);
		format_add(ft, "pane_bottom", "%u", wp->yoff + wp->sy - 1);
		format_add(ft, "pane_at_left", "%d", wp->xoff == 0);
		format_add(ft, "pane_at_top", "%d", wp->yoff == 0);
		format_add(ft, "pane_at_right", "%d",
		    wp->xoff + wp->sx == wp->window->sx);
		format_add(ft, "pane_at_bottom", "%d",
		    wp->yoff + wp->sy == wp->window->sy);
	}

	format_add(ft, "pane_in_mode", "%d", wp->screen != &wp->base);
	if (wp->mode != NULL)
		format_add(ft, "pane_mode", "%s", wp->mode->name);

	format_add(ft, "pane_synchronized", "%d",
	    !!options_get_number(wp->window->options, "synchronize-panes"));
	if (wp->searchstr != NULL)
		format_add(ft, "pane_search_string", "%s", wp->searchstr);

	format_add(ft, "pane_tty", "%s", wp->tty);
	format_add(ft, "pane_pid", "%ld", (long) wp->pid);
	format_add_cb(ft, "pane_start_command", format_cb_start_command);
	format_add_cb(ft, "pane_current_command", format_cb_current_command);
	format_add_cb(ft, "pane_current_path", format_cb_current_path);

	format_add(ft, "cursor_x", "%u", wp->base.cx);
	format_add(ft, "cursor_y", "%u", wp->base.cy);
	format_add(ft, "scroll_region_upper", "%u", wp->base.rupper);
	format_add(ft, "scroll_region_lower", "%u", wp->base.rlower);

	window_copy_add_formats(wp, ft);

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
	    !!(wp->base.mode & ALL_MOUSE_MODES));
	format_add(ft, "mouse_standard_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_STANDARD));
	format_add(ft, "mouse_button_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_BUTTON));
	format_add(ft, "mouse_all_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_ALL));

	format_add_cb(ft, "pane_tabs", format_cb_pane_tabs);
}

/* Set default format keys for paste buffer. */
void
format_defaults_paste_buffer(struct format_tree *ft, struct paste_buffer *pb)
{
	struct timeval	 tv;
	size_t		 size;
	char		*s;

	timerclear(&tv);
	tv.tv_sec = paste_buffer_created(pb);
	paste_buffer_data(pb, &size);

	format_add(ft, "buffer_size", "%zu", size);
	format_add(ft, "buffer_name", "%s", paste_buffer_name(pb));
	format_add_tv(ft, "buffer_created", &tv);

	s = paste_make_sample(pb);
	format_add(ft, "buffer_sample", "%s", s);
	free(s);
}
