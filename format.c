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
#include <math.h>
#include <regex.h>
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

struct format_expand_state;

static char	*format_job_get(struct format_expand_state *, const char *);
static char	*format_expand1(struct format_expand_state *, const char *);
static int	 format_replace(struct format_expand_state *, const char *,
		     size_t, char **, size_t *, size_t *);
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
#define FORMAT_QUOTE_SHELL 0x8
#define FORMAT_LITERAL 0x10
#define FORMAT_EXPAND 0x20
#define FORMAT_EXPANDTIME 0x40
#define FORMAT_SESSIONS 0x80
#define FORMAT_WINDOWS 0x100
#define FORMAT_PANES 0x200
#define FORMAT_PRETTY 0x400
#define FORMAT_LENGTH 0x800
#define FORMAT_WIDTH 0x1000
#define FORMAT_QUOTE_STYLE 0x2000
#define FORMAT_WINDOW_NAME 0x4000
#define FORMAT_SESSION_NAME 0x8000
#define FORMAT_CHARACTER 0x10000

/* Limit on recursion. */
#define FORMAT_LOOP_LIMIT 100

/* Format expand flags. */
#define FORMAT_EXPAND_TIME 0x1
#define FORMAT_EXPAND_NOJOBS 0x2

/* Entry in format tree. */
struct format_entry {
	char			*key;
	char			*value;
	time_t			 time;
	format_cb		 cb;
	RB_ENTRY(format_entry)	 entry;
};

/* Format type. */
enum format_type {
	FORMAT_TYPE_UNKNOWN,
	FORMAT_TYPE_SESSION,
	FORMAT_TYPE_WINDOW,
	FORMAT_TYPE_PANE
};

struct format_tree {
	enum format_type	 type;

	struct client		*c;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	struct paste_buffer	*pb;

	struct cmdq_item	*item;
	struct client		*client;
	int			 flags;
	u_int			 tag;

	struct mouse_event	 m;

	RB_HEAD(format_entry_tree, format_entry) tree;
};
static int format_entry_cmp(struct format_entry *, struct format_entry *);
RB_GENERATE_STATIC(format_entry_tree, format_entry, entry, format_entry_cmp);

/* Format expand state. */
struct format_expand_state {
	struct format_tree	*ft;
	u_int			 loop;
	time_t			 time;
	struct tm		 tm;
	int			 flags;
};

/* Format modifier. */
struct format_modifier {
	char	  modifier[3];
	u_int	  size;

	char	**argv;
	int	  argc;
};

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

/* Is logging enabled? */
static inline int
format_logging(struct format_tree *ft)
{
	return (log_get_level() != 0 || (ft->flags & FORMAT_VERBOSE));
}

/* Log a message if verbose. */
static void printflike(3, 4)
format_log1(struct format_expand_state *es, const char *from, const char *fmt,
    ...)
{
	struct format_tree	*ft = es->ft;
	va_list			 ap;
	char			*s;
	static const char	 spaces[] = "          ";

	if (!format_logging(ft))
		return;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);

	log_debug("%s: %s", from, s);
	if (ft->item != NULL && (ft->flags & FORMAT_VERBOSE))
		cmdq_print(ft->item, "#%.*s%s", es->loop, spaces, s);

	free(s);
}
#define format_log(es, fmt, ...) format_log1(es, __func__, fmt, ##__VA_ARGS__)

/* Copy expand state. */
static void
format_copy_state(struct format_expand_state *to,
    struct format_expand_state *from, int flags)
{
	to->ft = from->ft;
	to->loop = from->loop;
	to->time = from->time;
	memcpy(&to->tm, &from->tm, sizeof to->tm);
	to->flags = from->flags|flags;
}

/* Format job update callback. */
static void
format_job_update(struct job *job)
{
	struct format_job	*fj = job_get_data(job);
	struct evbuffer		*evb = job_get_event(job)->input;
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
	struct format_job	*fj = job_get_data(job);
	struct evbuffer		*evb = job_get_event(job)->input;
	char			*line, *buf;
	size_t			 len;

	fj->job = NULL;

	buf = NULL;
	if ((line = evbuffer_readline(evb)) == NULL) {
		len = EVBUFFER_LENGTH(evb);
		buf = xmalloc(len + 1);
		if (len != 0)
			memcpy(buf, EVBUFFER_DATA(evb), len);
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
format_job_get(struct format_expand_state *es, const char *cmd)
{
	struct format_tree		*ft = es->ft;
	struct format_job_tree		*jobs;
	struct format_job		 fj0, *fj;
	time_t				 t;
	char				*expanded;
	int				 force;
	struct format_expand_state	 next;

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

	format_copy_state(&next, es, FORMAT_EXPAND_NOJOBS);
	next.flags &= ~FORMAT_EXPAND_TIME;

	expanded = format_expand1(&next, cmd);
	if (fj->expanded == NULL || strcmp(expanded, fj->expanded) != 0) {
		free((void *)fj->expanded);
		fj->expanded = xstrdup(expanded);
		force = 1;
	} else
		force = (ft->flags & FORMAT_FORCE);

	t = time(NULL);
	if (force && fj->job != NULL)
	       job_free(fj->job);
	if (force || (fj->job == NULL && fj->last != t)) {
		fj->job = job_run(expanded, 0, NULL, NULL,
		    server_client_get_cwd(ft->client, NULL), format_job_update,
		    format_job_complete, NULL, fj, JOB_NOWAIT, -1, -1);
		if (fj->job == NULL) {
			free(fj->out);
			xasprintf(&fj->out, "<'%s' didn't start>", fj->cmd);
		}
		fj->last = t;
		fj->updated = 0;
	}
	free(expanded);

	if (ft->flags & FORMAT_STATUS)
		fj->status = 1;
	return (format_expand1(&next, fj->out));
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

/* Tidy old jobs for all clients. */
void
format_tidy_jobs(void)
{
	struct client	*c;

	format_job_tidy(&format_jobs, 0);
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->jobs != NULL)
			format_job_tidy(c->jobs, 0);
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

/* Wrapper for asprintf. */
static char * printflike(1, 2)
format_printf(const char *fmt, ...)
{
	va_list	 ap;
	char	*s;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);
	return (s);
}

/* Callback for host. */
static void *
format_cb_host(__unused struct format_tree *ft)
{
	char host[HOST_NAME_MAX + 1];

	if (gethostname(host, sizeof host) != 0)
		return (xstrdup(""));
	return (xstrdup(host));
}

/* Callback for host_short. */
static void *
format_cb_host_short(__unused struct format_tree *ft)
{
	char host[HOST_NAME_MAX + 1], *cp;

	if (gethostname(host, sizeof host) != 0)
		return (xstrdup(""));
	if ((cp = strchr(host, '.')) != NULL)
		*cp = '\0';
	return (xstrdup(host));
}

/* Callback for pid. */
static void *
format_cb_pid(__unused struct format_tree *ft)
{
	char	*value;

	xasprintf(&value, "%ld", (long)getpid());
	return (value);
}

/* Callback for session_attached_list. */
static void *
format_cb_session_attached_list(struct format_tree *ft)
{
	struct session	*s = ft->s;
	struct client	*loop;
	struct evbuffer	*buffer;
	int		 size;
	char		*value = NULL;

	if (s == NULL)
		return (NULL);

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(loop, &clients, entry) {
		if (loop->session == s) {
			if (EVBUFFER_LENGTH(buffer) > 0)
				evbuffer_add(buffer, ",", 1);
			evbuffer_add_printf(buffer, "%s", loop->name);
		}
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
	return (value);
}

/* Callback for session_alerts. */
static void *
format_cb_session_alerts(struct format_tree *ft)
{
	struct session	*s = ft->s;
	struct winlink	*wl;
	char		 alerts[1024], tmp[16];

	if (s == NULL)
		return (NULL);

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
	return (xstrdup(alerts));
}

/* Callback for session_stack. */
static void *
format_cb_session_stack(struct format_tree *ft)
{
	struct session	*s = ft->s;
	struct winlink	*wl;
	char		 result[1024], tmp[16];

	if (s == NULL)
		return (NULL);

	xsnprintf(result, sizeof result, "%u", s->curw->idx);
	TAILQ_FOREACH(wl, &s->lastw, sentry) {
		xsnprintf(tmp, sizeof tmp, "%u", wl->idx);

		if (*result != '\0')
			strlcat(result, ",", sizeof result);
		strlcat(result, tmp, sizeof result);
	}
	return (xstrdup(result));
}

/* Callback for window_stack_index. */
static void *
format_cb_window_stack_index(struct format_tree *ft)
{
	struct session	*s;
	struct winlink	*wl;
	u_int		 idx;
	char		*value = NULL;

	if (ft->wl == NULL)
		return (NULL);
	s = ft->wl->session;

	idx = 0;
	TAILQ_FOREACH(wl, &s->lastw, sentry) {
		idx++;
		if (wl == ft->wl)
			break;
	}
	if (wl == NULL)
		return (xstrdup("0"));
	xasprintf(&value, "%u", idx);
	return (value);
}

/* Callback for window_linked_sessions_list. */
static void *
format_cb_window_linked_sessions_list(struct format_tree *ft)
{
	struct window	*w;
	struct winlink	*wl;
	struct evbuffer	*buffer;
	int		 size;
	char		*value = NULL;

	if (ft->wl == NULL)
		return (NULL);
	w = ft->wl->window;

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%s", wl->session->name);
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
	return (value);
}

/* Callback for window_active_sessions. */
static void *
format_cb_window_active_sessions(struct format_tree *ft)
{
	struct window	*w;
	struct winlink	*wl;
	u_int		 n = 0;
	char		*value;

	if (ft->wl == NULL)
		return (NULL);
	w = ft->wl->window;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session->curw == wl)
			n++;
	}

	xasprintf(&value, "%u", n);
	return (value);
}

/* Callback for window_active_sessions_list. */
static void *
format_cb_window_active_sessions_list(struct format_tree *ft)
{
	struct window	*w;
	struct winlink	*wl;
	struct evbuffer	*buffer;
	int		 size;
	char		*value = NULL;

	if (ft->wl == NULL)
		return (NULL);
	w = ft->wl->window;

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session->curw == wl) {
			if (EVBUFFER_LENGTH(buffer) > 0)
				evbuffer_add(buffer, ",", 1);
			evbuffer_add_printf(buffer, "%s", wl->session->name);
		}
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
	return (value);
}

/* Callback for window_active_clients. */
static void *
format_cb_window_active_clients(struct format_tree *ft)
{
	struct window	*w;
	struct client	*loop;
	struct session	*client_session;
	u_int		 n = 0;
	char		*value;

	if (ft->wl == NULL)
		return (NULL);
	w = ft->wl->window;

	TAILQ_FOREACH(loop, &clients, entry) {
		client_session = loop->session;
		if (client_session == NULL)
			continue;

		if (w == client_session->curw->window)
			n++;
	}

	xasprintf(&value, "%u", n);
	return (value);
}

/* Callback for window_active_clients_list. */
static void *
format_cb_window_active_clients_list(struct format_tree *ft)
{
	struct window	*w;
	struct client	*loop;
	struct session	*client_session;
	struct evbuffer	*buffer;
	int		 size;
	char		*value = NULL;

	if (ft->wl == NULL)
		return (NULL);
	w = ft->wl->window;

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(loop, &clients, entry) {
		client_session = loop->session;
		if (client_session == NULL)
			continue;

		if (w == client_session->curw->window) {
			if (EVBUFFER_LENGTH(buffer) > 0)
				evbuffer_add(buffer, ",", 1);
			evbuffer_add_printf(buffer, "%s", loop->name);
		}
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
	return (value);
}

/* Callback for window_layout. */
static void *
format_cb_window_layout(struct format_tree *ft)
{
	struct window	*w = ft->w;

	if (w == NULL)
		return (NULL);

	if (w->saved_layout_root != NULL)
		return (layout_dump(w->saved_layout_root));
	return (layout_dump(w->layout_root));
}

/* Callback for window_visible_layout. */
static void *
format_cb_window_visible_layout(struct format_tree *ft)
{
	struct window	*w = ft->w;

	if (w == NULL)
		return (NULL);

	return (layout_dump(w->layout_root));
}

/* Callback for pane_start_command. */
static void *
format_cb_start_command(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;

	if (wp == NULL)
		return (NULL);

	return (cmd_stringify_argv(wp->argc, wp->argv));
}

/* Callback for pane_current_command. */
static void *
format_cb_current_command(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	char			*cmd, *value;

	if (wp == NULL || wp->shell == NULL)
		return (NULL);

	cmd = osdep_get_name(wp->fd, wp->tty);
	if (cmd == NULL || *cmd == '\0') {
		free(cmd);
		cmd = cmd_stringify_argv(wp->argc, wp->argv);
		if (cmd == NULL || *cmd == '\0') {
			free(cmd);
			cmd = xstrdup(wp->shell);
		}
	}
	value = parse_window_name(cmd);
	free(cmd);
	return (value);
}

/* Callback for pane_current_path. */
static void *
format_cb_current_path(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	char			*cwd;

	if (wp == NULL)
		return (NULL);

	cwd = osdep_get_cwd(wp->fd);
	if (cwd == NULL)
		return (NULL);
	return (xstrdup(cwd));
}

/* Callback for history_bytes. */
static void *
format_cb_history_bytes(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct grid		*gd;
	struct grid_line	*gl;
	size_t		         size = 0;
	u_int			 i;
	char			*value;

	if (wp == NULL)
		return (NULL);
	gd = wp->base.grid;

	for (i = 0; i < gd->hsize + gd->sy; i++) {
		gl = grid_get_line(gd, i);
		size += gl->cellsize * sizeof *gl->celldata;
		size += gl->extdsize * sizeof *gl->extddata;
	}
	size += (gd->hsize + gd->sy) * sizeof *gl;

	xasprintf(&value, "%zu", size);
	return (value);
}

/* Callback for history_all_bytes. */
static void *
format_cb_history_all_bytes(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct grid		*gd;
	struct grid_line	*gl;
	u_int			 i, lines, cells = 0, extended_cells = 0;
	char			*value;

	if (wp == NULL)
		return (NULL);
	gd = wp->base.grid;

	lines = gd->hsize + gd->sy;
	for (i = 0; i < lines; i++) {
		gl = grid_get_line(gd, i);
		cells += gl->cellsize;
		extended_cells += gl->extdsize;
	}

	xasprintf(&value, "%u,%zu,%u,%zu,%u,%zu", lines,
	    lines * sizeof *gl, cells, cells * sizeof *gl->celldata,
	    extended_cells, extended_cells * sizeof *gl->extddata);
	return (value);
}

/* Callback for pane_tabs. */
static void *
format_cb_pane_tabs(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct evbuffer		*buffer;
	u_int			 i;
	int			 size;
	char			*value = NULL;

	if (wp == NULL)
		return (NULL);

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");
	for (i = 0; i < wp->base.grid->sx; i++) {
		if (!bit_test(wp->base.tabs, i))
			continue;

		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%u", i);
	}
	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
	return (value);
}

/* Callback for pane_fg. */
static void *
format_cb_pane_fg(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct grid_cell	 gc;

	tty_default_colours(&gc, wp);
	return (xstrdup(colour_tostring(gc.fg)));
}

/* Callback for pane_bg. */
static void *
format_cb_pane_bg(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct grid_cell	 gc;

	tty_default_colours(&gc, wp);
	return (xstrdup(colour_tostring(gc.bg)));
}

/* Callback for session_group_list. */
static void *
format_cb_session_group_list(struct format_tree *ft)
{
	struct session		*s = ft->s;
	struct session_group	*sg;
	struct session		*loop;
	struct evbuffer		*buffer;
	int			 size;
	char			*value = NULL;

	if (s == NULL)
		return (NULL);
	sg = session_group_contains(s);
	if (sg == NULL)
		return (NULL);

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(loop, &sg->sessions, gentry) {
		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%s", loop->name);
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
	return (value);
}

/* Callback for session_group_attached_list. */
static void *
format_cb_session_group_attached_list(struct format_tree *ft)
{
	struct session		*s = ft->s, *client_session, *session_loop;
	struct session_group	*sg;
	struct client		*loop;
	struct evbuffer		*buffer;
	int			 size;
	char			*value = NULL;

	if (s == NULL)
		return (NULL);
	sg = session_group_contains(s);
	if (sg == NULL)
		return (NULL);

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(loop, &clients, entry) {
		client_session = loop->session;
		if (client_session == NULL)
			continue;
		TAILQ_FOREACH(session_loop, &sg->sessions, gentry) {
			if (session_loop == client_session){
				if (EVBUFFER_LENGTH(buffer) > 0)
					evbuffer_add(buffer, ",", 1);
				evbuffer_add_printf(buffer, "%s", loop->name);
			}
		}
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
	return (value);
}

/* Callback for pane_in_mode. */
static void *
format_cb_pane_in_mode(struct format_tree *ft)
{
	struct window_pane		*wp = ft->wp;
	u_int				 n = 0;
	struct window_mode_entry	*wme;
	char				*value;

	if (wp == NULL)
		return (NULL);

	TAILQ_FOREACH(wme, &wp->modes, entry)
		n++;
	xasprintf(&value, "%u", n);
	return (value);
}

/* Callback for pane_at_top. */
static void *
format_cb_pane_at_top(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct window		*w;
	int			 status, flag;
	char			*value;

	if (wp == NULL)
		return (NULL);
	w = wp->window;

	status = options_get_number(w->options, "pane-border-status");
	if (status == PANE_STATUS_TOP)
		flag = (wp->yoff == 1);
	else
		flag = (wp->yoff == 0);
	xasprintf(&value, "%d", flag);
	return (value);
}

/* Callback for pane_at_bottom. */
static void *
format_cb_pane_at_bottom(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct window		*w;
	int			 status, flag;
	char			*value;

	if (wp == NULL)
		return (NULL);
	w = wp->window;

	status = options_get_number(w->options, "pane-border-status");
	if (status == PANE_STATUS_BOTTOM)
		flag = (wp->yoff + wp->sy == w->sy - 1);
	else
		flag = (wp->yoff + wp->sy == w->sy);
	xasprintf(&value, "%d", flag);
	return (value);
}

/* Callback for cursor_character. */
static void *
format_cb_cursor_character(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;
	struct grid_cell	 gc;
	char			*value = NULL;

	if (wp == NULL)
		return (NULL);

	grid_view_get_cell(wp->base.grid, wp->base.cx, wp->base.cy, &gc);
	if (~gc.flags & GRID_FLAG_PADDING)
		xasprintf(&value, "%.*s", (int)gc.data.size, gc.data.data);
	return (value);
}

/* Callback for mouse_word. */
static void *
format_cb_mouse_word(struct format_tree *ft)
{
	struct window_pane	*wp;
	struct grid		*gd;
	u_int			 x, y;
	char			*s;

	if (!ft->m.valid)
		return (NULL);
	wp = cmd_mouse_pane(&ft->m, NULL, NULL);
	if (wp == NULL)
		return (NULL);
	if (cmd_mouse_at(wp, &ft->m, &x, &y, 0) != 0)
		return (NULL);

	if (!TAILQ_EMPTY(&wp->modes)) {
		if (TAILQ_FIRST(&wp->modes)->mode == &window_copy_mode ||
		    TAILQ_FIRST(&wp->modes)->mode == &window_view_mode)
			return (s = window_copy_get_word(wp, x, y));
		return (NULL);
	}
	gd = wp->base.grid;
	return (format_grid_word(gd, x, gd->hsize + y));
}

/* Callback for mouse_line. */
static void *
format_cb_mouse_line(struct format_tree *ft)
{
	struct window_pane	*wp;
	struct grid		*gd;
	u_int			 x, y;

	if (!ft->m.valid)
		return (NULL);
	wp = cmd_mouse_pane(&ft->m, NULL, NULL);
	if (wp == NULL)
		return (NULL);
	if (cmd_mouse_at(wp, &ft->m, &x, &y, 0) != 0)
		return (NULL);

	if (!TAILQ_EMPTY(&wp->modes)) {
		if (TAILQ_FIRST(&wp->modes)->mode == &window_copy_mode ||
		    TAILQ_FIRST(&wp->modes)->mode == &window_view_mode)
			return (window_copy_get_line(wp, y));
		return (NULL);
	}
	gd = wp->base.grid;
	return (format_grid_line(gd, gd->hsize + y));
}

/* Callback for alternate_on. */
static void *
format_cb_alternate_on(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.saved_grid != NULL)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for alternate_saved_x. */
static void *
format_cb_alternate_saved_x(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.saved_cx));
	return (NULL);
}

/* Callback for alternate_saved_y. */
static void *
format_cb_alternate_saved_y(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.saved_cy));
	return (NULL);
}

/* Callback for buffer_name. */
static void *
format_cb_buffer_name(struct format_tree *ft)
{
	if (ft->pb != NULL)
		return (xstrdup(paste_buffer_name(ft->pb)));
	return (NULL);
}

/* Callback for buffer_sample. */
static void *
format_cb_buffer_sample(struct format_tree *ft)
{
	if (ft->pb != NULL)
		return (paste_make_sample(ft->pb));
	return (NULL);
}

/* Callback for buffer_size. */
static void *
format_cb_buffer_size(struct format_tree *ft)
{
	size_t	size;

	if (ft->pb != NULL) {
		paste_buffer_data(ft->pb, &size);
		return (format_printf("%zu", size));
	}
	return (NULL);
}

/* Callback for client_cell_height. */
static void *
format_cb_client_cell_height(struct format_tree *ft)
{
	if (ft->c != NULL && (ft->c->tty.flags & TTY_STARTED))
		return (format_printf("%u", ft->c->tty.ypixel));
	return (NULL);
}

/* Callback for client_cell_width. */
static void *
format_cb_client_cell_width(struct format_tree *ft)
{
	if (ft->c != NULL && (ft->c->tty.flags & TTY_STARTED))
		return (format_printf("%u", ft->c->tty.xpixel));
	return (NULL);
}

/* Callback for client_control_mode. */
static void *
format_cb_client_control_mode(struct format_tree *ft)
{
	if (ft->c != NULL) {
		if (ft->c->flags & CLIENT_CONTROL)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for client_discarded. */
static void *
format_cb_client_discarded(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (format_printf("%zu", ft->c->discarded));
	return (NULL);
}

/* Callback for client_flags. */
static void *
format_cb_client_flags(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (xstrdup(server_client_get_flags(ft->c)));
	return (NULL);
}

/* Callback for client_height. */
static void *
format_cb_client_height(struct format_tree *ft)
{
	if (ft->c != NULL && (ft->c->tty.flags & TTY_STARTED))
		return (format_printf("%u", ft->c->tty.sy));
	return (NULL);
}

/* Callback for client_key_table. */
static void *
format_cb_client_key_table(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (xstrdup(ft->c->keytable->name));
	return (NULL);
}

/* Callback for client_last_session. */
static void *
format_cb_client_last_session(struct format_tree *ft)
{
	if (ft->c != NULL &&
	    ft->c->last_session != NULL &&
	    session_alive(ft->c->last_session))
		return (xstrdup(ft->c->last_session->name));
	return (NULL);
}

/* Callback for client_name. */
static void *
format_cb_client_name(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (xstrdup(ft->c->name));
	return (NULL);
}

/* Callback for client_pid. */
static void *
format_cb_client_pid(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (format_printf("%ld", (long)ft->c->pid));
	return (NULL);
}

/* Callback for client_prefix. */
static void *
format_cb_client_prefix(struct format_tree *ft)
{
	const char	*name;

	if (ft->c != NULL) {
		name = server_client_get_key_table(ft->c);
		if (strcmp(ft->c->keytable->name, name) == 0)
			return (xstrdup("0"));
		return (xstrdup("1"));
	}
	return (NULL);
}

/* Callback for client_readonly. */
static void *
format_cb_client_readonly(struct format_tree *ft)
{
	if (ft->c != NULL) {
		if (ft->c->flags & CLIENT_READONLY)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for client_session. */
static void *
format_cb_client_session(struct format_tree *ft)
{
	if (ft->c != NULL && ft->c->session != NULL)
		return (xstrdup(ft->c->session->name));
	return (NULL);
}

/* Callback for client_termfeatures. */
static void *
format_cb_client_termfeatures(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (xstrdup(tty_get_features(ft->c->term_features)));
	return (NULL);
}

/* Callback for client_termname. */
static void *
format_cb_client_termname(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (xstrdup(ft->c->term_name));
	return (NULL);
}

/* Callback for client_termtype. */
static void *
format_cb_client_termtype(struct format_tree *ft)
{
	if (ft->c != NULL) {
		if (ft->c->term_type == NULL)
			return (xstrdup(""));
		return (xstrdup(ft->c->term_type));
	}
	return (NULL);
}

/* Callback for client_tty. */
static void *
format_cb_client_tty(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (xstrdup(ft->c->ttyname));
	return (NULL);
}

/* Callback for client_utf8. */
static void *
format_cb_client_utf8(struct format_tree *ft)
{
	if (ft->c != NULL) {
		if (ft->c->flags & CLIENT_UTF8)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for client_width. */
static void *
format_cb_client_width(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (format_printf("%u", ft->c->tty.sx));
	return (NULL);
}

/* Callback for client_written. */
static void *
format_cb_client_written(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (format_printf("%zu", ft->c->written));
	return (NULL);
}

/* Callback for config_files. */
static void *
format_cb_config_files(__unused struct format_tree *ft)
{
	char	*s = NULL;
	size_t	 slen = 0;
	u_int	 i;
	size_t	 n;

	for (i = 0; i < cfg_nfiles; i++) {
		n = strlen(cfg_files[i]) + 1;
		s = xrealloc(s, slen + n + 1);
		slen += xsnprintf(s + slen, n + 1, "%s,", cfg_files[i]);
	}
	if (s == NULL)
		return (xstrdup(""));
	s[slen - 1] = '\0';
	return (s);
}

/* Callback for cursor_flag. */
static void *
format_cb_cursor_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_CURSOR)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for cursor_x. */
static void *
format_cb_cursor_x(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.cx));
	return (NULL);
}

/* Callback for cursor_y. */
static void *
format_cb_cursor_y(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.cy));
	return (NULL);
}

/* Callback for history_limit. */
static void *
format_cb_history_limit(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.grid->hlimit));
	return (NULL);
}

/* Callback for history_size. */
static void *
format_cb_history_size(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.grid->hsize));
	return (NULL);
}

/* Callback for insert_flag. */
static void *
format_cb_insert_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_INSERT)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for keypad_cursor_flag. */
static void *
format_cb_keypad_cursor_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_KCURSOR)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for keypad_flag. */
static void *
format_cb_keypad_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_KKEYPAD)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for mouse_all_flag. */
static void *
format_cb_mouse_all_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_MOUSE_ALL)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for mouse_any_flag. */
static void *
format_cb_mouse_any_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & ALL_MOUSE_MODES)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for mouse_button_flag. */
static void *
format_cb_mouse_button_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_MOUSE_BUTTON)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for mouse_pane. */
static void *
format_cb_mouse_pane(struct format_tree *ft)
{
	struct window_pane	*wp;

	if (ft->m.valid) {
		wp = cmd_mouse_pane(&ft->m, NULL, NULL);
		if (wp != NULL)
			return (format_printf("%%%u", wp->id));
		return (NULL);
	}
	return (NULL);
}

/* Callback for mouse_sgr_flag. */
static void *
format_cb_mouse_sgr_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_MOUSE_SGR)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for mouse_standard_flag. */
static void *
format_cb_mouse_standard_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_MOUSE_STANDARD)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for mouse_utf8_flag. */
static void *
format_cb_mouse_utf8_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_MOUSE_UTF8)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for mouse_x. */
static void *
format_cb_mouse_x(struct format_tree *ft)
{
	struct window_pane	*wp;
	u_int			 x, y;

	if (ft->m.valid) {
		wp = cmd_mouse_pane(&ft->m, NULL, NULL);
		if (wp != NULL && cmd_mouse_at(wp, &ft->m, &x, &y, 0) == 0)
			return (format_printf("%u", x));
		return (NULL);
	}
	return (NULL);
}

/* Callback for mouse_y. */
static void *
format_cb_mouse_y(struct format_tree *ft)
{
	struct window_pane	*wp;
	u_int			 x, y;

	if (ft->m.valid) {
		wp = cmd_mouse_pane(&ft->m, NULL, NULL);
		if (wp != NULL && cmd_mouse_at(wp, &ft->m, &x, &y, 0) == 0)
			return (format_printf("%u", y));
		return (NULL);
	}
	return (NULL);
}

/* Callback for origin_flag. */
static void *
format_cb_origin_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_ORIGIN)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_active. */
static void *
format_cb_pane_active(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp == ft->wp->window->active)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_at_left. */
static void *
format_cb_pane_at_left(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->xoff == 0)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_at_right. */
static void *
format_cb_pane_at_right(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->xoff + ft->wp->sx == ft->wp->window->sx)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_bottom. */
static void *
format_cb_pane_bottom(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->yoff + ft->wp->sy - 1));
	return (NULL);
}

/* Callback for pane_dead. */
static void *
format_cb_pane_dead(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->fd == -1)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_dead_status. */
static void *
format_cb_pane_dead_status(struct format_tree *ft)
{
	struct window_pane	*wp = ft->wp;

	if (wp != NULL) {
		if ((wp->flags & PANE_STATUSREADY) && WIFEXITED(wp->status))
			return (format_printf("%d", WEXITSTATUS(wp->status)));
		return (NULL);
	}
	return (NULL);
}

/* Callback for pane_format. */
static void *
format_cb_pane_format(struct format_tree *ft)
{
	if (ft->type == FORMAT_TYPE_PANE)
		return (xstrdup("1"));
	return (xstrdup("0"));
}

/* Callback for pane_height. */
static void *
format_cb_pane_height(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->sy));
	return (NULL);
}

/* Callback for pane_id. */
static void *
format_cb_pane_id(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%%%u", ft->wp->id));
	return (NULL);
}

/* Callback for pane_index. */
static void *
format_cb_pane_index(struct format_tree *ft)
{
	u_int	idx;

	if (ft->wp != NULL && window_pane_index(ft->wp, &idx) == 0)
		return (format_printf("%u", idx));
	return (NULL);
}

/* Callback for pane_input_off. */
static void *
format_cb_pane_input_off(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->flags & PANE_INPUTOFF)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_last. */
static void *
format_cb_pane_last(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp == ft->wp->window->last)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_left. */
static void *
format_cb_pane_left(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->xoff));
	return (NULL);
}

/* Callback for pane_marked. */
static void *
format_cb_pane_marked(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (server_check_marked() && marked_pane.wp == ft->wp)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_marked_set. */
static void *
format_cb_pane_marked_set(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (server_check_marked())
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_mode. */
static void *
format_cb_pane_mode(struct format_tree *ft)
{
	struct window_mode_entry	*wme;

	if (ft->wp != NULL) {
		wme = TAILQ_FIRST(&ft->wp->modes);
		if (wme != NULL)
			return (xstrdup(wme->mode->name));
		return (NULL);
	}
	return (NULL);
}

/* Callback for pane_path. */
static void *
format_cb_pane_path(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.path == NULL)
			return (xstrdup(""));
		return (xstrdup(ft->wp->base.path));
	}
	return (NULL);
}

/* Callback for pane_pid. */
static void *
format_cb_pane_pid(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%ld", (long)ft->wp->pid));
	return (NULL);
}

/* Callback for pane_pipe. */
static void *
format_cb_pane_pipe(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->pipe_fd != -1)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_right. */
static void *
format_cb_pane_right(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->xoff + ft->wp->sx - 1));
	return (NULL);
}

/* Callback for pane_search_string. */
static void *
format_cb_pane_search_string(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->searchstr == NULL)
			return (xstrdup(""));
		return (xstrdup(ft->wp->searchstr));
	}
	return (NULL);
}

/* Callback for pane_synchronized. */
static void *
format_cb_pane_synchronized(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (options_get_number(ft->wp->options, "synchronize-panes"))
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for pane_title. */
static void *
format_cb_pane_title(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (xstrdup(ft->wp->base.title));
	return (NULL);
}

/* Callback for pane_top. */
static void *
format_cb_pane_top(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->yoff));
	return (NULL);
}

/* Callback for pane_tty. */
static void *
format_cb_pane_tty(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (xstrdup(ft->wp->tty));
	return (NULL);
}

/* Callback for pane_width. */
static void *
format_cb_pane_width(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->sx));
	return (NULL);
}

/* Callback for scroll_region_lower. */
static void *
format_cb_scroll_region_lower(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.rlower));
	return (NULL);
}

/* Callback for scroll_region_upper. */
static void *
format_cb_scroll_region_upper(struct format_tree *ft)
{
	if (ft->wp != NULL)
		return (format_printf("%u", ft->wp->base.rupper));
	return (NULL);
}

/* Callback for session_attached. */
static void *
format_cb_session_attached(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (format_printf("%u", ft->s->attached));
	return (NULL);
}

/* Callback for session_format. */
static void *
format_cb_session_format(struct format_tree *ft)
{
	if (ft->type == FORMAT_TYPE_SESSION)
		return (xstrdup("1"));
	return (xstrdup("0"));
}

/* Callback for session_group. */
static void *
format_cb_session_group(struct format_tree *ft)
{
	struct session_group	*sg;

	if (ft->s != NULL && (sg = session_group_contains(ft->s)) != NULL)
		return (xstrdup(sg->name));
	return (NULL);
}

/* Callback for session_group_attached. */
static void *
format_cb_session_group_attached(struct format_tree *ft)
{
	struct session_group	*sg;

	if (ft->s != NULL && (sg = session_group_contains(ft->s)) != NULL)
		return (format_printf("%u", session_group_attached_count (sg)));
	return (NULL);
}

/* Callback for session_group_many_attached. */
static void *
format_cb_session_group_many_attached(struct format_tree *ft)
{
	struct session_group	*sg;

	if (ft->s != NULL && (sg = session_group_contains(ft->s)) != NULL) {
		if (session_group_attached_count (sg) > 1)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for session_group_size. */
static void *
format_cb_session_group_size(struct format_tree *ft)
{
	struct session_group	*sg;

	if (ft->s != NULL && (sg = session_group_contains(ft->s)) != NULL)
		return (format_printf("%u", session_group_count (sg)));
	return (NULL);
}

/* Callback for session_grouped. */
static void *
format_cb_session_grouped(struct format_tree *ft)
{
	if (ft->s != NULL) {
		if (session_group_contains(ft->s) != NULL)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for session_id. */
static void *
format_cb_session_id(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (format_printf("$%u", ft->s->id));
	return (NULL);
}

/* Callback for session_many_attached. */
static void *
format_cb_session_many_attached(struct format_tree *ft)
{
	if (ft->s != NULL) {
		if (ft->s->attached > 1)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for session_marked. */
static void *
format_cb_session_marked(struct format_tree *ft)
{
	if (ft->s != NULL) {
		if (server_check_marked() && marked_pane.s == ft->s)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for session_name. */
static void *
format_cb_session_name(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (xstrdup(ft->s->name));
	return (NULL);
}

/* Callback for session_path. */
static void *
format_cb_session_path(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (xstrdup(ft->s->cwd));
	return (NULL);
}

/* Callback for session_windows. */
static void *
format_cb_session_windows(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (format_printf ("%u", winlink_count(&ft->s->windows)));
	return (NULL);
}

/* Callback for socket_path. */
static void *
format_cb_socket_path(__unused struct format_tree *ft)
{
	return (xstrdup(socket_path));
}

/* Callback for version. */
static void *
format_cb_version(__unused struct format_tree *ft)
{
	return (xstrdup(getversion()));
}

/* Callback for active_window_index. */
static void *
format_cb_active_window_index(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (format_printf("%u", ft->s->curw->idx));
	return (NULL);
}

/* Callback for last_window_index. */
static void *
format_cb_last_window_index(struct format_tree *ft)
{
	struct winlink	*wl;

	if (ft->s != NULL) {
		wl = RB_MAX(winlinks, &ft->s->windows);
		return (format_printf("%u", wl->idx));
	}
	return (NULL);
}

/* Callback for window_active. */
static void *
format_cb_window_active(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (ft->wl == ft->wl->session->curw)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_activity_flag. */
static void *
format_cb_window_activity_flag(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (ft->wl->flags & WINLINK_ACTIVITY)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_bell_flag. */
static void *
format_cb_window_bell_flag(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (ft->wl->flags & WINLINK_BELL)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_bigger. */
static void *
format_cb_window_bigger(struct format_tree *ft)
{
	u_int	ox, oy, sx, sy;

	if (ft->c != NULL) {
		if (tty_window_offset(&ft->c->tty, &ox, &oy, &sx, &sy))
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_cell_height. */
static void *
format_cb_window_cell_height(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (format_printf("%u", ft->w->ypixel));
	return (NULL);
}

/* Callback for window_cell_width. */
static void *
format_cb_window_cell_width(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (format_printf("%u", ft->w->xpixel));
	return (NULL);
}

/* Callback for window_end_flag. */
static void *
format_cb_window_end_flag(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (ft->wl == RB_MAX(winlinks, &ft->wl->session->windows))
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_flags. */
static void *
format_cb_window_flags(struct format_tree *ft)
{
	if (ft->wl != NULL)
		return (xstrdup(window_printable_flags(ft->wl, 1)));
	return (NULL);
}

/* Callback for window_format. */
static void *
format_cb_window_format(struct format_tree *ft)
{
	if (ft->type == FORMAT_TYPE_WINDOW)
		return (xstrdup("1"));
	return (xstrdup("0"));
}

/* Callback for window_height. */
static void *
format_cb_window_height(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (format_printf("%u", ft->w->sy));
	return (NULL);
}

/* Callback for window_id. */
static void *
format_cb_window_id(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (format_printf("@%u", ft->w->id));
	return (NULL);
}

/* Callback for window_index. */
static void *
format_cb_window_index(struct format_tree *ft)
{
	if (ft->wl != NULL)
		return (format_printf("%d", ft->wl->idx));
	return (NULL);
}

/* Callback for window_last_flag. */
static void *
format_cb_window_last_flag(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (ft->wl == TAILQ_FIRST(&ft->wl->session->lastw))
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_linked. */
static void *
format_cb_window_linked(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (session_is_linked(ft->wl->session, ft->wl->window))
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_linked_sessions. */
static void *
format_cb_window_linked_sessions(struct format_tree *ft)
{
	if (ft->wl != NULL)
		return (format_printf("%u", ft->wl->window->references));
	return (NULL);
}

/* Callback for window_marked_flag. */
static void *
format_cb_window_marked_flag(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (server_check_marked() && marked_pane.wl == ft->wl)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_name. */
static void *
format_cb_window_name(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (format_printf("%s", ft->w->name));
	return (NULL);
}

/* Callback for window_offset_x. */
static void *
format_cb_window_offset_x(struct format_tree *ft)
{
	u_int	ox, oy, sx, sy;

	if (ft->c != NULL) {
		if (tty_window_offset(&ft->c->tty, &ox, &oy, &sx, &sy))
			return (format_printf("%u", ox));
		return (NULL);
	}
	return (NULL);
}

/* Callback for window_offset_y. */
static void *
format_cb_window_offset_y(struct format_tree *ft)
{
	u_int	ox, oy, sx, sy;

	if (ft->c != NULL) {
		if (tty_window_offset(&ft->c->tty, &ox, &oy, &sx, &sy))
			return (format_printf("%u", oy));
		return (NULL);
	}
	return (NULL);
}

/* Callback for window_panes. */
static void *
format_cb_window_panes(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (format_printf("%u", window_count_panes(ft->w)));
	return (NULL);
}

/* Callback for window_raw_flags. */
static void *
format_cb_window_raw_flags(struct format_tree *ft)
{
	if (ft->wl != NULL)
		return (xstrdup(window_printable_flags(ft->wl, 0)));
	return (NULL);
}

/* Callback for window_silence_flag. */
static void *
format_cb_window_silence_flag(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (ft->wl->flags & WINLINK_SILENCE)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_start_flag. */
static void *
format_cb_window_start_flag(struct format_tree *ft)
{
	if (ft->wl != NULL) {
		if (ft->wl == RB_MIN(winlinks, &ft->wl->session->windows))
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for window_width. */
static void *
format_cb_window_width(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (format_printf("%u", ft->w->sx));
	return (NULL);
}

/* Callback for window_zoomed_flag. */
static void *
format_cb_window_zoomed_flag(struct format_tree *ft)
{
	if (ft->w != NULL) {
		if (ft->w->flags & WINDOW_ZOOMED)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for wrap_flag. */
static void *
format_cb_wrap_flag(struct format_tree *ft)
{
	if (ft->wp != NULL) {
		if (ft->wp->base.mode & MODE_WRAP)
			return (xstrdup("1"));
		return (xstrdup("0"));
	}
	return (NULL);
}

/* Callback for buffer_created. */
static void *
format_cb_buffer_created(struct format_tree *ft)
{
	static struct timeval	 tv;

	if (ft->pb != NULL) {
		timerclear(&tv);
		tv.tv_sec = paste_buffer_created(ft->pb);
		return (&tv);
	}
	return (NULL);
}

/* Callback for client_activity. */
static void *
format_cb_client_activity(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (&ft->c->activity_time);
	return (NULL);
}

/* Callback for client_created. */
static void *
format_cb_client_created(struct format_tree *ft)
{
	if (ft->c != NULL)
		return (&ft->c->creation_time);
	return (NULL);
}

/* Callback for session_activity. */
static void *
format_cb_session_activity(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (&ft->s->activity_time);
	return (NULL);
}

/* Callback for session_created. */
static void *
format_cb_session_created(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (&ft->s->creation_time);
	return (NULL);
}

/* Callback for session_last_attached. */
static void *
format_cb_session_last_attached(struct format_tree *ft)
{
	if (ft->s != NULL)
		return (&ft->s->last_attached_time);
	return (NULL);
}

/* Callback for start_time. */
static void *
format_cb_start_time(__unused struct format_tree *ft)
{
	return (&start_time);
}

/* Callback for window_activity. */
static void *
format_cb_window_activity(struct format_tree *ft)
{
	if (ft->w != NULL)
		return (&ft->w->activity_time);
	return (NULL);
}

/* Callback for buffer_mode_format, */
static void *
format_cb_buffer_mode_format(__unused struct format_tree *ft)
{
	return (xstrdup(window_buffer_mode.default_format));
}

/* Callback for client_mode_format, */
static void *
format_cb_client_mode_format(__unused struct format_tree *ft)
{
	return (xstrdup(window_client_mode.default_format));
}

/* Callback for tree_mode_format, */
static void *
format_cb_tree_mode_format(__unused struct format_tree *ft)
{
	return (xstrdup(window_tree_mode.default_format));
}

/* Format table type. */
enum format_table_type {
	FORMAT_TABLE_STRING,
	FORMAT_TABLE_TIME
};

/* Format table entry. */
struct format_table_entry {
	const char		*key;
	enum format_table_type	 type;
	format_cb		 cb;
};

/*
 * Format table. Default format variables (that are almost always in the tree
 * and where the value is expanded by a callback in this file) are listed here.
 * Only variables which are added by the caller go into the tree.
 */
static const struct format_table_entry format_table[] = {
	{ "active_window_index", FORMAT_TABLE_STRING,
	  format_cb_active_window_index
	},
	{ "alternate_on", FORMAT_TABLE_STRING,
	  format_cb_alternate_on
	},
	{ "alternate_saved_x", FORMAT_TABLE_STRING,
	  format_cb_alternate_saved_x
	},
	{ "alternate_saved_y", FORMAT_TABLE_STRING,
	  format_cb_alternate_saved_y
	},
	{ "buffer_created", FORMAT_TABLE_TIME,
	  format_cb_buffer_created
	},
	{ "buffer_mode_format", FORMAT_TABLE_STRING,
	  format_cb_buffer_mode_format
	},
	{ "buffer_name", FORMAT_TABLE_STRING,
	  format_cb_buffer_name
	},
	{ "buffer_sample", FORMAT_TABLE_STRING,
	  format_cb_buffer_sample
	},
	{ "buffer_size", FORMAT_TABLE_STRING,
	  format_cb_buffer_size
	},
	{ "client_activity", FORMAT_TABLE_TIME,
	  format_cb_client_activity
	},
	{ "client_cell_height", FORMAT_TABLE_STRING,
	  format_cb_client_cell_height
	},
	{ "client_cell_width", FORMAT_TABLE_STRING,
	  format_cb_client_cell_width
	},
	{ "client_control_mode", FORMAT_TABLE_STRING,
	  format_cb_client_control_mode
	},
	{ "client_created", FORMAT_TABLE_TIME,
	  format_cb_client_created
	},
	{ "client_discarded", FORMAT_TABLE_STRING,
	  format_cb_client_discarded
	},
	{ "client_flags", FORMAT_TABLE_STRING,
	  format_cb_client_flags
	},
	{ "client_height", FORMAT_TABLE_STRING,
	  format_cb_client_height
	},
	{ "client_key_table", FORMAT_TABLE_STRING,
	  format_cb_client_key_table
	},
	{ "client_last_session", FORMAT_TABLE_STRING,
	  format_cb_client_last_session
	},
	{ "client_mode_format", FORMAT_TABLE_STRING,
	  format_cb_client_mode_format
	},
	{ "client_name", FORMAT_TABLE_STRING,
	  format_cb_client_name
	},
	{ "client_pid", FORMAT_TABLE_STRING,
	  format_cb_client_pid
	},
	{ "client_prefix", FORMAT_TABLE_STRING,
	  format_cb_client_prefix
	},
	{ "client_readonly", FORMAT_TABLE_STRING,
	  format_cb_client_readonly
	},
	{ "client_session", FORMAT_TABLE_STRING,
	  format_cb_client_session
	},
	{ "client_termfeatures", FORMAT_TABLE_STRING,
	  format_cb_client_termfeatures
	},
	{ "client_termname", FORMAT_TABLE_STRING,
	  format_cb_client_termname
	},
	{ "client_termtype", FORMAT_TABLE_STRING,
	  format_cb_client_termtype
	},
	{ "client_tty", FORMAT_TABLE_STRING,
	  format_cb_client_tty
	},
	{ "client_utf8", FORMAT_TABLE_STRING,
	  format_cb_client_utf8
	},
	{ "client_width", FORMAT_TABLE_STRING,
	  format_cb_client_width
	},
	{ "client_written", FORMAT_TABLE_STRING,
	  format_cb_client_written
	},
	{ "config_files", FORMAT_TABLE_STRING,
	  format_cb_config_files
	},
	{ "cursor_character", FORMAT_TABLE_STRING,
	  format_cb_cursor_character
	},
	{ "cursor_flag", FORMAT_TABLE_STRING,
	  format_cb_cursor_flag
	},
	{ "cursor_x", FORMAT_TABLE_STRING,
	  format_cb_cursor_x
	},
	{ "cursor_y", FORMAT_TABLE_STRING,
	  format_cb_cursor_y
	},
	{ "history_all_bytes", FORMAT_TABLE_STRING,
	  format_cb_history_all_bytes
	},
	{ "history_bytes", FORMAT_TABLE_STRING,
	  format_cb_history_bytes
	},
	{ "history_limit", FORMAT_TABLE_STRING,
	  format_cb_history_limit
	},
	{ "history_size", FORMAT_TABLE_STRING,
	  format_cb_history_size
	},
	{ "host", FORMAT_TABLE_STRING,
	  format_cb_host
	},
	{ "host_short", FORMAT_TABLE_STRING,
	  format_cb_host_short
	},
	{ "insert_flag", FORMAT_TABLE_STRING,
	  format_cb_insert_flag
	},
	{ "keypad_cursor_flag", FORMAT_TABLE_STRING,
	  format_cb_keypad_cursor_flag
	},
	{ "keypad_flag", FORMAT_TABLE_STRING,
	  format_cb_keypad_flag
	},
	{ "last_window_index", FORMAT_TABLE_STRING,
	  format_cb_last_window_index
	},
	{ "mouse_all_flag", FORMAT_TABLE_STRING,
	  format_cb_mouse_all_flag
	},
	{ "mouse_any_flag", FORMAT_TABLE_STRING,
	  format_cb_mouse_any_flag
	},
	{ "mouse_button_flag", FORMAT_TABLE_STRING,
	  format_cb_mouse_button_flag
	},
	{ "mouse_line", FORMAT_TABLE_STRING,
	  format_cb_mouse_line
	},
	{ "mouse_pane", FORMAT_TABLE_STRING,
	  format_cb_mouse_pane
	},
	{ "mouse_sgr_flag", FORMAT_TABLE_STRING,
	  format_cb_mouse_sgr_flag
	},
	{ "mouse_standard_flag", FORMAT_TABLE_STRING,
	  format_cb_mouse_standard_flag
	},
	{ "mouse_utf8_flag", FORMAT_TABLE_STRING,
	  format_cb_mouse_utf8_flag
	},
	{ "mouse_word", FORMAT_TABLE_STRING,
	  format_cb_mouse_word
	},
	{ "mouse_x", FORMAT_TABLE_STRING,
	  format_cb_mouse_x
	},
	{ "mouse_y", FORMAT_TABLE_STRING,
	  format_cb_mouse_y
	},
	{ "origin_flag", FORMAT_TABLE_STRING,
	  format_cb_origin_flag
	},
	{ "pane_active", FORMAT_TABLE_STRING,
	  format_cb_pane_active
	},
	{ "pane_at_bottom", FORMAT_TABLE_STRING,
	  format_cb_pane_at_bottom
	},
	{ "pane_at_left", FORMAT_TABLE_STRING,
	  format_cb_pane_at_left
	},
	{ "pane_at_right", FORMAT_TABLE_STRING,
	  format_cb_pane_at_right
	},
	{ "pane_at_top", FORMAT_TABLE_STRING,
	  format_cb_pane_at_top
	},
	{ "pane_bg", FORMAT_TABLE_STRING,
	  format_cb_pane_bg
	},
	{ "pane_bottom", FORMAT_TABLE_STRING,
	  format_cb_pane_bottom
	},
	{ "pane_current_command", FORMAT_TABLE_STRING,
	  format_cb_current_command
	},
	{ "pane_current_path", FORMAT_TABLE_STRING,
	  format_cb_current_path
	},
	{ "pane_dead", FORMAT_TABLE_STRING,
	  format_cb_pane_dead
	},
	{ "pane_dead_status", FORMAT_TABLE_STRING,
	  format_cb_pane_dead_status
	},
	{ "pane_fg", FORMAT_TABLE_STRING,
	  format_cb_pane_fg
	},
	{ "pane_format", FORMAT_TABLE_STRING,
	  format_cb_pane_format
	},
	{ "pane_height", FORMAT_TABLE_STRING,
	  format_cb_pane_height
	},
	{ "pane_id", FORMAT_TABLE_STRING,
	  format_cb_pane_id
	},
	{ "pane_in_mode", FORMAT_TABLE_STRING,
	  format_cb_pane_in_mode
	},
	{ "pane_index", FORMAT_TABLE_STRING,
	  format_cb_pane_index
	},
	{ "pane_input_off", FORMAT_TABLE_STRING,
	  format_cb_pane_input_off
	},
	{ "pane_last", FORMAT_TABLE_STRING,
	  format_cb_pane_last
	},
	{ "pane_left", FORMAT_TABLE_STRING,
	  format_cb_pane_left
	},
	{ "pane_marked", FORMAT_TABLE_STRING,
	  format_cb_pane_marked
	},
	{ "pane_marked_set", FORMAT_TABLE_STRING,
	  format_cb_pane_marked_set
	},
	{ "pane_mode", FORMAT_TABLE_STRING,
	  format_cb_pane_mode
	},
	{ "pane_path", FORMAT_TABLE_STRING,
	  format_cb_pane_path
	},
	{ "pane_pid", FORMAT_TABLE_STRING,
	  format_cb_pane_pid
	},
	{ "pane_pipe", FORMAT_TABLE_STRING,
	  format_cb_pane_pipe
	},
	{ "pane_right", FORMAT_TABLE_STRING,
	  format_cb_pane_right
	},
	{ "pane_search_string", FORMAT_TABLE_STRING,
	  format_cb_pane_search_string
	},
	{ "pane_start_command", FORMAT_TABLE_STRING,
	  format_cb_start_command
	},
	{ "pane_synchronized", FORMAT_TABLE_STRING,
	  format_cb_pane_synchronized
	},
	{ "pane_tabs", FORMAT_TABLE_STRING,
	  format_cb_pane_tabs
	},
	{ "pane_title", FORMAT_TABLE_STRING,
	  format_cb_pane_title
	},
	{ "pane_top", FORMAT_TABLE_STRING,
	  format_cb_pane_top
	},
	{ "pane_tty", FORMAT_TABLE_STRING,
	  format_cb_pane_tty
	},
	{ "pane_width", FORMAT_TABLE_STRING,
	  format_cb_pane_width
	},
	{ "pid", FORMAT_TABLE_STRING,
	  format_cb_pid
	},
	{ "scroll_region_lower", FORMAT_TABLE_STRING,
	  format_cb_scroll_region_lower
	},
	{ "scroll_region_upper", FORMAT_TABLE_STRING,
	  format_cb_scroll_region_upper
	},
	{ "session_activity", FORMAT_TABLE_TIME,
	  format_cb_session_activity
	},
	{ "session_alerts", FORMAT_TABLE_STRING,
	  format_cb_session_alerts
	},
	{ "session_attached", FORMAT_TABLE_STRING,
	  format_cb_session_attached
	},
	{ "session_attached_list", FORMAT_TABLE_STRING,
	  format_cb_session_attached_list
	},
	{ "session_created", FORMAT_TABLE_TIME,
	  format_cb_session_created
	},
	{ "session_format", FORMAT_TABLE_STRING,
	  format_cb_session_format
	},
	{ "session_group", FORMAT_TABLE_STRING,
	  format_cb_session_group
	},
	{ "session_group_attached", FORMAT_TABLE_STRING,
	  format_cb_session_group_attached
	},
	{ "session_group_attached_list", FORMAT_TABLE_STRING,
	  format_cb_session_group_attached_list
	},
	{ "session_group_list", FORMAT_TABLE_STRING,
	  format_cb_session_group_list
	},
	{ "session_group_many_attached", FORMAT_TABLE_STRING,
	  format_cb_session_group_many_attached
	},
	{ "session_group_size", FORMAT_TABLE_STRING,
	  format_cb_session_group_size
	},
	{ "session_grouped", FORMAT_TABLE_STRING,
	  format_cb_session_grouped
	},
	{ "session_id", FORMAT_TABLE_STRING,
	  format_cb_session_id
	},
	{ "session_last_attached", FORMAT_TABLE_TIME,
	  format_cb_session_last_attached
	},
	{ "session_many_attached", FORMAT_TABLE_STRING,
	  format_cb_session_many_attached
	},
	{ "session_marked", FORMAT_TABLE_STRING,
	  format_cb_session_marked,
	},
	{ "session_name", FORMAT_TABLE_STRING,
	  format_cb_session_name
	},
	{ "session_path", FORMAT_TABLE_STRING,
	  format_cb_session_path
	},
	{ "session_stack", FORMAT_TABLE_STRING,
	  format_cb_session_stack
	},
	{ "session_windows", FORMAT_TABLE_STRING,
	  format_cb_session_windows
	},
	{ "socket_path", FORMAT_TABLE_STRING,
	  format_cb_socket_path
	},
	{ "start_time", FORMAT_TABLE_TIME,
	  format_cb_start_time
	},
	{ "tree_mode_format", FORMAT_TABLE_STRING,
	  format_cb_tree_mode_format
	},
	{ "version", FORMAT_TABLE_STRING,
	  format_cb_version
	},
	{ "window_active", FORMAT_TABLE_STRING,
	  format_cb_window_active
	},
	{ "window_active_clients", FORMAT_TABLE_STRING,
	  format_cb_window_active_clients
	},
	{ "window_active_clients_list", FORMAT_TABLE_STRING,
	  format_cb_window_active_clients_list
	},
	{ "window_active_sessions", FORMAT_TABLE_STRING,
	  format_cb_window_active_sessions
	},
	{ "window_active_sessions_list", FORMAT_TABLE_STRING,
	  format_cb_window_active_sessions_list
	},
	{ "window_activity", FORMAT_TABLE_TIME,
	  format_cb_window_activity
	},
	{ "window_activity_flag", FORMAT_TABLE_STRING,
	  format_cb_window_activity_flag
	},
	{ "window_bell_flag", FORMAT_TABLE_STRING,
	  format_cb_window_bell_flag
	},
	{ "window_bigger", FORMAT_TABLE_STRING,
	  format_cb_window_bigger
	},
	{ "window_cell_height", FORMAT_TABLE_STRING,
	  format_cb_window_cell_height
	},
	{ "window_cell_width", FORMAT_TABLE_STRING,
	  format_cb_window_cell_width
	},
	{ "window_end_flag", FORMAT_TABLE_STRING,
	  format_cb_window_end_flag
	},
	{ "window_flags", FORMAT_TABLE_STRING,
	  format_cb_window_flags
	},
	{ "window_format", FORMAT_TABLE_STRING,
	  format_cb_window_format
	},
	{ "window_height", FORMAT_TABLE_STRING,
	  format_cb_window_height
	},
	{ "window_id", FORMAT_TABLE_STRING,
	  format_cb_window_id
	},
	{ "window_index", FORMAT_TABLE_STRING,
	  format_cb_window_index
	},
	{ "window_last_flag", FORMAT_TABLE_STRING,
	  format_cb_window_last_flag
	},
	{ "window_layout", FORMAT_TABLE_STRING,
	  format_cb_window_layout
	},
	{ "window_linked", FORMAT_TABLE_STRING,
	  format_cb_window_linked
	},
	{ "window_linked_sessions", FORMAT_TABLE_STRING,
	  format_cb_window_linked_sessions
	},
	{ "window_linked_sessions_list", FORMAT_TABLE_STRING,
	  format_cb_window_linked_sessions_list
	},
	{ "window_marked_flag", FORMAT_TABLE_STRING,
	  format_cb_window_marked_flag
	},
	{ "window_name", FORMAT_TABLE_STRING,
	  format_cb_window_name
	},
	{ "window_offset_x", FORMAT_TABLE_STRING,
	  format_cb_window_offset_x
	},
	{ "window_offset_y", FORMAT_TABLE_STRING,
	  format_cb_window_offset_y
	},
	{ "window_panes", FORMAT_TABLE_STRING,
	  format_cb_window_panes
	},
	{ "window_raw_flags", FORMAT_TABLE_STRING,
	  format_cb_window_raw_flags
	},
	{ "window_silence_flag", FORMAT_TABLE_STRING,
	  format_cb_window_silence_flag
	},
	{ "window_stack_index", FORMAT_TABLE_STRING,
	  format_cb_window_stack_index
	},
	{ "window_start_flag", FORMAT_TABLE_STRING,
	  format_cb_window_start_flag
	},
	{ "window_visible_layout", FORMAT_TABLE_STRING,
	  format_cb_window_visible_layout
	},
	{ "window_width", FORMAT_TABLE_STRING,
	  format_cb_window_width
	},
	{ "window_zoomed_flag", FORMAT_TABLE_STRING,
	  format_cb_window_zoomed_flag
	},
	{ "wrap_flag", FORMAT_TABLE_STRING,
	  format_cb_wrap_flag
	}
};

/* Compare format table entries. */
static int
format_table_compare(const void *key0, const void *entry0)
{
	const char			*key = key0;
	const struct format_table_entry	*entry = entry0;

	return (strcmp(key, entry->key));
}

/* Get a format callback. */
static struct format_table_entry *
format_table_get(const char *key)
{
	return (bsearch(key, format_table, nitems(format_table),
	    sizeof *format_table, format_table_compare));
}

/* Merge one format tree into another. */
void
format_merge(struct format_tree *ft, struct format_tree *from)
{
	struct format_entry	*fe;

	RB_FOREACH(fe, format_entry_tree, &from->tree) {
		if (fe->value != NULL)
			format_add(ft, fe->key, "%s", fe->value);
	}
}

/* Get format pane. */
struct window_pane *
format_get_pane(struct format_tree *ft)
{
	return (ft->wp);
}

/* Add item bits to tree. */
static void
format_create_add_item(struct format_tree *ft, struct cmdq_item *item)
{
	struct key_event	*event = cmdq_get_event(item);
	struct mouse_event	*m = &event->m;

	cmdq_merge_formats(item, ft);
	memcpy(&ft->m, m, sizeof ft->m);
}

/* Create a new tree. */
struct format_tree *
format_create(struct client *c, struct cmdq_item *item, int tag, int flags)
{
	struct format_tree	*ft;

	ft = xcalloc(1, sizeof *ft);
	RB_INIT(&ft->tree);

	if (c != NULL) {
		ft->client = c;
		ft->client->references++;
	}
	ft->item = item;

	ft->tag = tag;
	ft->flags = flags;

	if (item != NULL)
		format_create_add_item(ft, item);

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

/* Walk each format. */
void
format_each(struct format_tree *ft, void (*cb)(const char *, const char *,
    void *), void *arg)
{
	const struct format_table_entry	*fte;
	struct format_entry		*fe;
	u_int				 i;
	char				 s[64];
	void				*value;
	struct timeval			*tv;

	for (i = 0; i < nitems(format_table); i++) {
		fte = &format_table[i];

		value = fte->cb(ft);
		if (value == NULL)
			continue;
		if (fte->type == FORMAT_TABLE_TIME) {
			tv = value;
			xsnprintf(s, sizeof s, "%lld", (long long)tv->tv_sec);
			cb(fte->key, s, arg);
		} else {
			cb(fte->key, value, arg);
			free(value);
		}
	}
	RB_FOREACH(fe, format_entry_tree, &ft->tree) {
		if (fe->time != 0) {
			xsnprintf(s, sizeof s, "%lld", (long long)fe->time);
			cb(fe->key, s, arg);
		} else {
			if (fe->value == NULL && fe->cb != NULL) {
				fe->value = fe->cb(ft);
				if (fe->value == NULL)
					fe->value = xstrdup("");
			}
			cb(fe->key, fe->value, arg);
		}
	}
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
	fe->time = 0;

	va_start(ap, fmt);
	xvasprintf(&fe->value, fmt, ap);
	va_end(ap);
}

/* Add a key and time. */
void
format_add_tv(struct format_tree *ft, const char *key, struct timeval *tv)
{
	struct format_entry	*fe, *fe_now;

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
	fe->time = tv->tv_sec;

	fe->value = NULL;
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
	fe->time = 0;

	fe->value = NULL;
}

/* Quote shell special characters in string. */
static char *
format_quote_shell(const char *s)
{
	const char	*cp;
	char		*out, *at;

	at = out = xmalloc(strlen(s) * 2 + 1);
	for (cp = s; *cp != '\0'; cp++) {
		if (strchr("|&;<>()$`\\\"'*?[# =%", *cp) != NULL)
			*at++ = '\\';
		*at++ = *cp;
	}
	*at = '\0';
	return (out);
}

/* Quote #s in string. */
static char *
format_quote_style(const char *s)
{
	const char	*cp;
	char		*out, *at;

	at = out = xmalloc(strlen(s) * 2 + 1);
	for (cp = s; *cp != '\0'; cp++) {
		if (*cp == '#')
			*at++ = '#';
		*at++ = *cp;
	}
	*at = '\0';
	return (out);
}

/* Make a prettier time. */
static char *
format_pretty_time(time_t t)
{
	struct tm       now_tm, tm;
	time_t		now, age;
	char		s[6];

	time(&now);
	if (now < t)
		now = t;
	age = now - t;

	localtime_r(&now, &now_tm);
	localtime_r(&t, &tm);

	/* Last 24 hours. */
	if (age < 24 * 3600) {
		strftime(s, sizeof s, "%H:%M", &tm);
		return (xstrdup(s));
	}

	/* This month or last 28 days. */
	if ((tm.tm_year == now_tm.tm_year && tm.tm_mon == now_tm.tm_mon) ||
	    age < 28 * 24 * 3600) {
		strftime(s, sizeof s, "%a%d", &tm);
		return (xstrdup(s));
	}

	/* Last 12 months. */
	if ((tm.tm_year == now_tm.tm_year && tm.tm_mon < now_tm.tm_mon) ||
	    (tm.tm_year == now_tm.tm_year - 1 && tm.tm_mon > now_tm.tm_mon)) {
		strftime(s, sizeof s, "%d%b", &tm);
		return (xstrdup(s));
	}

	/* Older than that. */
	strftime(s, sizeof s, "%h%y", &tm);
	return (xstrdup(s));
}

/* Find a format entry. */
static char *
format_find(struct format_tree *ft, const char *key, int modifiers,
    const char *time_format)
{
	struct format_table_entry	*fte;
	void				*value;
	struct format_entry		*fe, fe_find;
	struct environ_entry		*envent;
	struct options_entry		*o;
	int				 idx;
	char				*found = NULL, *saved, s[512];
	const char			*errstr;
	time_t				 t = 0;
	struct tm			 tm;

	o = options_parse_get(global_options, key, &idx, 0);
	if (o == NULL && ft->wp != NULL)
		o = options_parse_get(ft->wp->options, key, &idx, 0);
	if (o == NULL && ft->w != NULL)
		o = options_parse_get(ft->w->options, key, &idx, 0);
	if (o == NULL)
		o = options_parse_get(global_w_options, key, &idx, 0);
	if (o == NULL && ft->s != NULL)
		o = options_parse_get(ft->s->options, key, &idx, 0);
	if (o == NULL)
		o = options_parse_get(global_s_options, key, &idx, 0);
	if (o != NULL) {
		found = options_to_string(o, idx, 1);
		goto found;
	}

	fte = format_table_get(key);
	if (fte != NULL) {
		value = fte->cb(ft);
		if (fte->type == FORMAT_TABLE_TIME)
			t = ((struct timeval *)value)->tv_sec;
		else
			found = value;
		goto found;
	}
	fe_find.key = (char *)key;
	fe = RB_FIND(format_entry_tree, &ft->tree, &fe_find);
	if (fe != NULL) {
		if (fe->time != 0) {
			t = fe->time;
			goto found;
		}
		if (fe->value == NULL && fe->cb != NULL) {
			fe->value = fe->cb(ft);
			if (fe->value == NULL)
				fe->value = xstrdup("");
		}
		found = xstrdup(fe->value);
		goto found;
	}

	if (~modifiers & FORMAT_TIMESTRING) {
		envent = NULL;
		if (ft->s != NULL)
			envent = environ_find(ft->s->environ, key);
		if (envent == NULL)
			envent = environ_find(global_environ, key);
		if (envent != NULL && envent->value != NULL) {
			found = xstrdup(envent->value);
			goto found;
		}
	}

	return (NULL);

found:
	if (modifiers & FORMAT_TIMESTRING) {
		if (t == 0 && found != NULL) {
			t = strtonum(found, 0, INT64_MAX, &errstr);
			if (errstr != NULL)
				t = 0;
			free(found);
		}
		if (t == 0)
			return (NULL);
		if (modifiers & FORMAT_PRETTY)
			found = format_pretty_time(t);
		else {
			if (time_format != NULL) {
				localtime_r(&t, &tm);
				strftime(s, sizeof s, time_format, &tm);
			} else {
				ctime_r(&t, s);
				s[strcspn(s, "\n")] = '\0';
			}
			found = xstrdup(s);
		}
		return (found);
	}

	if (t != 0)
		xasprintf(&found, "%lld", (long long)t);
	else if (found == NULL)
		return (NULL);
	if (modifiers & FORMAT_BASENAME) {
		saved = found;
		found = xstrdup(basename(saved));
		free(saved);
	}
	if (modifiers & FORMAT_DIRNAME) {
		saved = found;
		found = xstrdup(dirname(saved));
		free(saved);
	}
	if (modifiers & FORMAT_QUOTE_SHELL) {
		saved = found;
		found = xstrdup(format_quote_shell(saved));
		free(saved);
	}
	if (modifiers & FORMAT_QUOTE_STYLE) {
		saved = found;
		found = xstrdup(format_quote_style(saved));
		free(saved);
	}
	return (found);
}

/* Remove escaped characters from string. */
static char *
format_strip(const char *s)
{
	char	*out, *cp;
	int	 brackets = 0;

	cp = out = xmalloc(strlen(s) + 1);
	for (; *s != '\0'; s++) {
		if (*s == '#' && s[1] == '{')
			brackets++;
		if (*s == '#' && strchr(",#{}:", s[1]) != NULL) {
			if (brackets != 0)
				*cp++ = *s;
			continue;
		}
		if (*s == '}')
			brackets--;
		*cp++ = *s;
	}
	*cp = '\0';
	return (out);
}

/* Skip until end. */
const char *
format_skip(const char *s, const char *end)
{
	int	brackets = 0;

	for (; *s != '\0'; s++) {
		if (*s == '#' && s[1] == '{')
			brackets++;
		if (*s == '#' && strchr(",#{}:", s[1]) != NULL) {
			s++;
			continue;
		}
		if (*s == '}')
			brackets--;
		if (strchr(end, *s) != NULL && brackets == 0)
			break;
	}
	if (*s == '\0')
		return (NULL);
	return (s);
}

/* Return left and right alternatives separated by commas. */
static int
format_choose(struct format_expand_state *es, const char *s, char **left,
    char **right, int expand)
{
	const char	*cp;
	char		*left0, *right0;

	cp = format_skip(s, ",");
	if (cp == NULL)
		return (-1);
	left0 = xstrndup(s, cp - s);
	right0 = xstrdup(cp + 1);

	if (expand) {
		*left = format_expand1(es, left0);
		free(left0);
		*right = format_expand1(es, right0);
		free(right0);
	} else {
		*left = left0;
		*right = right0;
	}
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

/* Check if modifier end. */
static int
format_is_end(char c)
{
	return (c == ';' || c == ':');
}

/* Add to modifier list. */
static void
format_add_modifier(struct format_modifier **list, u_int *count,
    const char *c, size_t n, char **argv, int argc)
{
	struct format_modifier *fm;

	*list = xreallocarray(*list, (*count) + 1, sizeof **list);
	fm = &(*list)[(*count)++];

	memcpy(fm->modifier, c, n);
	fm->modifier[n] = '\0';
	fm->size = n;

	fm->argv = argv;
	fm->argc = argc;
}

/* Free modifier list. */
static void
format_free_modifiers(struct format_modifier *list, u_int count)
{
	u_int	i;

	for (i = 0; i < count; i++)
		cmd_free_argv(list[i].argc, list[i].argv);
	free(list);
}

/* Build modifier list. */
static struct format_modifier *
format_build_modifiers(struct format_expand_state *es, const char **s,
    u_int *count)
{
	const char		*cp = *s, *end;
	struct format_modifier	*list = NULL;
	char			 c, last[] = "X;:", **argv, *value;
	int			 argc;

	/*
	 * Modifiers are a ; separated list of the forms:
	 *      l,m,C,a,b,d,n,t,w,q,E,T,S,W,P,<,>
	 *	=a
	 *	=/a
	 *      =/a/
	 *	s/a/b/
	 *	s/a/b
	 *	||,&&,!=,==,<=,>=
	 */

	*count = 0;

	while (*cp != '\0' && *cp != ':') {
		/* Skip any separator character. */
		if (*cp == ';')
			cp++;

		/* Check single character modifiers with no arguments. */
		if (strchr("labdnwETSWP<>", cp[0]) != NULL &&
		    format_is_end(cp[1])) {
			format_add_modifier(&list, count, cp, 1, NULL, 0);
			cp++;
			continue;
		}

		/* Then try double character with no arguments. */
		if ((memcmp("||", cp, 2) == 0 ||
		    memcmp("&&", cp, 2) == 0 ||
		    memcmp("!=", cp, 2) == 0 ||
		    memcmp("==", cp, 2) == 0 ||
		    memcmp("<=", cp, 2) == 0 ||
		    memcmp(">=", cp, 2) == 0) &&
		    format_is_end(cp[2])) {
			format_add_modifier(&list, count, cp, 2, NULL, 0);
			cp += 2;
			continue;
		}

		/* Now try single character with arguments. */
		if (strchr("mCNst=peq", cp[0]) == NULL)
			break;
		c = cp[0];

		/* No arguments provided. */
		if (format_is_end(cp[1])) {
			format_add_modifier(&list, count, cp, 1, NULL, 0);
			cp++;
			continue;
		}
		argv = NULL;
		argc = 0;

		/* Single argument with no wrapper character. */
		if (!ispunct(cp[1]) || cp[1] == '-') {
			end = format_skip(cp + 1, ":;");
			if (end == NULL)
				break;

			argv = xcalloc(1, sizeof *argv);
			value = xstrndup(cp + 1, end - (cp + 1));
			argv[0] = format_expand1(es, value);
			free(value);
			argc = 1;

			format_add_modifier(&list, count, &c, 1, argv, argc);
			cp = end;
			continue;
		}

		/* Multiple arguments with a wrapper character. */
		last[0] = cp[1];
		cp++;
		do {
			if (cp[0] == last[0] && format_is_end(cp[1])) {
				cp++;
				break;
			}
			end = format_skip(cp + 1, last);
			if (end == NULL)
				break;
			cp++;

			argv = xreallocarray (argv, argc + 1, sizeof *argv);
			value = xstrndup(cp, end - cp);
			argv[argc++] = format_expand1(es, value);
			free(value);

			cp = end;
		} while (!format_is_end(cp[0]));
		format_add_modifier(&list, count, &c, 1, argv, argc);
	}
	if (*cp != ':') {
		format_free_modifiers(list, *count);
		*count = 0;
		return (NULL);
	}
	*s = cp + 1;
	return (list);
}

/* Match against an fnmatch(3) pattern or regular expression. */
static char *
format_match(struct format_modifier *fm, const char *pattern, const char *text)
{
	const char	*s = "";
	regex_t		 r;
	int		 flags = 0;

	if (fm->argc >= 1)
		s = fm->argv[0];
	if (strchr(s, 'r') == NULL) {
		if (strchr(s, 'i') != NULL)
			flags |= FNM_CASEFOLD;
		if (fnmatch(pattern, text, flags) != 0)
			return (xstrdup("0"));
	} else {
		flags = REG_EXTENDED|REG_NOSUB;
		if (strchr(s, 'i') != NULL)
			flags |= REG_ICASE;
		if (regcomp(&r, pattern, flags) != 0)
			return (xstrdup("0"));
		if (regexec(&r, text, 0, NULL, 0) != 0) {
			regfree(&r);
			return (xstrdup("0"));
		}
		regfree(&r);
	}
	return (xstrdup("1"));
}

/* Perform substitution in string. */
static char *
format_sub(struct format_modifier *fm, const char *text, const char *pattern,
    const char *with)
{
	char	*value;
	int	 flags = REG_EXTENDED;

	if (fm->argc >= 3 && strchr(fm->argv[2], 'i') != NULL)
		flags |= REG_ICASE;
	value = regsub(pattern, with, text, flags);
	if (value == NULL)
		return (xstrdup(text));
	return (value);
}

/* Search inside pane. */
static char *
format_search(struct format_modifier *fm, struct window_pane *wp, const char *s)
{
	int	 ignore = 0, regex = 0;
	char	*value;

	if (fm->argc >= 1) {
		if (strchr(fm->argv[0], 'i') != NULL)
			ignore = 1;
		if (strchr(fm->argv[0], 'r') != NULL)
			regex = 1;
	}
	xasprintf(&value, "%u", window_pane_search(wp, s, regex, ignore));
	return (value);
}

/* Does session name exist? */
static char *
format_session_name(struct format_expand_state *es, const char *fmt)
{
	char		*name;
	struct session	*s;

	name = format_expand1(es, fmt);
	RB_FOREACH(s, sessions, &sessions) {
		if (strcmp(s->name, name) == 0) {
			free(name);
			return (xstrdup("1"));
		}
	}
	free(name);
	return (xstrdup("0"));
}

/* Loop over sessions. */
static char *
format_loop_sessions(struct format_expand_state *es, const char *fmt)
{
	struct format_tree		*ft = es->ft;
	struct client			*c = ft->client;
	struct cmdq_item		*item = ft->item;
	struct format_tree		*nft;
	struct format_expand_state	 next;
	char				*expanded, *value;
	size_t				 valuelen;
	struct session			*s;

	value = xcalloc(1, 1);
	valuelen = 1;

	RB_FOREACH(s, sessions, &sessions) {
		format_log(es, "session loop: $%u", s->id);
		nft = format_create(c, item, FORMAT_NONE, ft->flags);
		format_defaults(nft, ft->c, s, NULL, NULL);
		format_copy_state(&next, es, 0);
		next.ft = nft;
		expanded = format_expand1(&next, fmt);
		format_free(next.ft);

		valuelen += strlen(expanded);
		value = xrealloc(value, valuelen);

		strlcat(value, expanded, valuelen);
		free(expanded);
	}

	return (value);
}

/* Does window name exist? */
static char *
format_window_name(struct format_expand_state *es, const char *fmt)
{
	struct format_tree	*ft = es->ft;
	char			*name;
	struct winlink		*wl;

	if (ft->s == NULL) {
		format_log(es, "window name but no session");
		return (NULL);
	}

	name = format_expand1(es, fmt);
	RB_FOREACH(wl, winlinks, &ft->s->windows) {
		if (strcmp(wl->window->name, name) == 0) {
			free(name);
			return (xstrdup("1"));
		}
	}
	free(name);
	return (xstrdup("0"));
}

/* Loop over windows. */
static char *
format_loop_windows(struct format_expand_state *es, const char *fmt)
{
	struct format_tree		*ft = es->ft;
	struct client			*c = ft->client;
	struct cmdq_item		*item = ft->item;
	struct format_tree		*nft;
	struct format_expand_state	 next;
	char				*all, *active, *use, *expanded, *value;
	size_t				 valuelen;
	struct winlink			*wl;
	struct window			*w;

	if (ft->s == NULL) {
		format_log(es, "window loop but no session");
		return (NULL);
	}

	if (format_choose(es, fmt, &all, &active, 0) != 0) {
		all = xstrdup(fmt);
		active = NULL;
	}

	value = xcalloc(1, 1);
	valuelen = 1;

	RB_FOREACH(wl, winlinks, &ft->s->windows) {
		w = wl->window;
		format_log(es, "window loop: %u @%u", wl->idx, w->id);
		if (active != NULL && wl == ft->s->curw)
			use = active;
		else
			use = all;
		nft = format_create(c, item, FORMAT_WINDOW|w->id, ft->flags);
		format_defaults(nft, ft->c, ft->s, wl, NULL);
		format_copy_state(&next, es, 0);
		next.ft = nft;
		expanded = format_expand1(&next, use);
		format_free(nft);

		valuelen += strlen(expanded);
		value = xrealloc(value, valuelen);

		strlcat(value, expanded, valuelen);
		free(expanded);
	}

	free(active);
	free(all);

	return (value);
}

/* Loop over panes. */
static char *
format_loop_panes(struct format_expand_state *es, const char *fmt)
{
	struct format_tree		*ft = es->ft;
	struct client			*c = ft->client;
	struct cmdq_item		*item = ft->item;
	struct format_tree		*nft;
	struct format_expand_state	 next;
	char				*all, *active, *use, *expanded, *value;
	size_t				 valuelen;
	struct window_pane		*wp;

	if (ft->w == NULL) {
		format_log(es, "pane loop but no window");
		return (NULL);
	}

	if (format_choose(es, fmt, &all, &active, 0) != 0) {
		all = xstrdup(fmt);
		active = NULL;
	}

	value = xcalloc(1, 1);
	valuelen = 1;

	TAILQ_FOREACH(wp, &ft->w->panes, entry) {
		format_log(es, "pane loop: %%%u", wp->id);
		if (active != NULL && wp == ft->w->active)
			use = active;
		else
			use = all;
		nft = format_create(c, item, FORMAT_PANE|wp->id, ft->flags);
		format_defaults(nft, ft->c, ft->s, ft->wl, wp);
		format_copy_state(&next, es, 0);
		next.ft = nft;
		expanded = format_expand1(&next, use);
		format_free(nft);

		valuelen += strlen(expanded);
		value = xrealloc(value, valuelen);

		strlcat(value, expanded, valuelen);
		free(expanded);
	}

	free(active);
	free(all);

	return (value);
}

static char *
format_replace_expression(struct format_modifier *mexp,
    struct format_expand_state *es, const char *copy)
{
	int			 argc = mexp->argc;
	const char		*errstr;
	char			*endch, *value, *left = NULL, *right = NULL;
	int			 use_fp = 0;
	u_int			 prec = 0;
	double			 mleft, mright, result;
	enum { ADD,
	       SUBTRACT,
	       MULTIPLY,
	       DIVIDE,
	       MODULUS,
	       EQUAL,
	       NOT_EQUAL,
	       GREATER_THAN,
	       GREATER_THAN_EQUAL,
	       LESS_THAN,
	       LESS_THAN_EQUAL } operator;

	if (strcmp(mexp->argv[0], "+") == 0)
		operator = ADD;
	else if (strcmp(mexp->argv[0], "-") == 0)
		operator = SUBTRACT;
	else if (strcmp(mexp->argv[0], "*") == 0)
		operator = MULTIPLY;
	else if (strcmp(mexp->argv[0], "/") == 0)
		operator = DIVIDE;
	else if (strcmp(mexp->argv[0], "%") == 0 ||
	    strcmp(mexp->argv[0], "m") == 0)
		operator = MODULUS;
	else if (strcmp(mexp->argv[0], "==") == 0)
		operator = EQUAL;
	else if (strcmp(mexp->argv[0], "!=") == 0)
		operator = NOT_EQUAL;
	else if (strcmp(mexp->argv[0], ">") == 0)
		operator = GREATER_THAN;
	else if (strcmp(mexp->argv[0], "<") == 0)
		operator = LESS_THAN;
	else if (strcmp(mexp->argv[0], ">=") == 0)
		operator = GREATER_THAN_EQUAL;
	else if (strcmp(mexp->argv[0], "<=") == 0)
		operator = LESS_THAN_EQUAL;
	else {
		format_log(es, "expression has no valid operator: '%s'",
		    mexp->argv[0]);
		goto fail;
	}

	/* The second argument may be flags. */
	if (argc >= 2 && strchr(mexp->argv[1], 'f') != NULL) {
		use_fp = 1;
		prec = 2;
	}

	/* The third argument may be precision. */
	if (argc >= 3) {
		prec = strtonum(mexp->argv[2], INT_MIN, INT_MAX, &errstr);
		if (errstr != NULL) {
			format_log(es, "expression precision %s: %s", errstr,
			    mexp->argv[2]);
			goto fail;
		}
	}

	if (format_choose(es, copy, &left, &right, 1) != 0) {
		format_log(es, "expression syntax error");
		goto fail;
	}

	mleft = strtod(left, &endch);
	if (*endch != '\0') {
		format_log(es, "expression left side is invalid: %s", left);
		goto fail;
	}

	mright = strtod(right, &endch);
	if (*endch != '\0') {
		format_log(es, "expression right side is invalid: %s", right);
		goto fail;
	}

	if (!use_fp) {
		mleft = (long long)mleft;
		mright = (long long)mright;
	}
	format_log(es, "expression left side is: %.*f", prec, mleft);
	format_log(es, "expression right side is: %.*f", prec, mright);

	switch (operator) {
	case ADD:
		result = mleft + mright;
		break;
	case SUBTRACT:
		result = mleft - mright;
		break;
	case MULTIPLY:
		result = mleft * mright;
		break;
	case DIVIDE:
		result = mleft / mright;
		break;
	case MODULUS:
		result = fmod(mleft, mright);
		break;
	case EQUAL:
		result = fabs(mleft - mright) < 1e-9;
		break;
	case NOT_EQUAL:
		result = fabs(mleft - mright) > 1e-9;
		break;
	case GREATER_THAN:
		result = (mleft > mright);
		break;
	case GREATER_THAN_EQUAL:
		result = (mleft >= mright);
		break;
	case LESS_THAN:
		result = (mleft < mright);
		break;
	case LESS_THAN_EQUAL:
		result = (mleft <= mright);
		break;
	}
	if (use_fp)
		xasprintf(&value, "%.*f", prec, result);
	else
		xasprintf(&value, "%.*f", prec, (double)(long long)result);
	format_log(es, "expression result is %s", value);

	free(right);
	free(left);
	return (value);

fail:
	free(right);
	free(left);
	return (NULL);
}

/* Replace a key. */
static int
format_replace(struct format_expand_state *es, const char *key, size_t keylen,
    char **buf, size_t *len, size_t *off)
{
	struct format_tree		 *ft = es->ft;
	struct window_pane		 *wp = ft->wp;
	const char			 *errstr, *copy, *cp, *marker = NULL;
	const char			 *time_format = NULL;
	char				 *copy0, *condition, *found, *new;
	char				 *value, *left, *right, c;
	size_t				  valuelen;
	int				  modifiers = 0, limit = 0, width = 0;
	int				  j;
	struct format_modifier		 *list, *cmp = NULL, *search = NULL;
	struct format_modifier		**sub = NULL, *mexp = NULL, *fm;
	u_int				  i, count, nsub = 0;
	struct format_expand_state	  next;

	/* Make a copy of the key. */
	copy = copy0 = xstrndup(key, keylen);

	/* Process modifier list. */
	list = format_build_modifiers(es, &copy, &count);
	for (i = 0; i < count; i++) {
		fm = &list[i];
		if (format_logging(ft)) {
			format_log(es, "modifier %u is %s", i, fm->modifier);
			for (j = 0; j < fm->argc; j++) {
				format_log(es, "modifier %u argument %d: %s", i,
				    j, fm->argv[j]);
			}
		}
		if (fm->size == 1) {
			switch (fm->modifier[0]) {
			case 'm':
			case '<':
			case '>':
				cmp = fm;
				break;
			case 'C':
				search = fm;
				break;
			case 's':
				if (fm->argc < 2)
					break;
				sub = xreallocarray (sub, nsub + 1,
				    sizeof *sub);
				sub[nsub++] = fm;
				break;
			case '=':
				if (fm->argc < 1)
					break;
				limit = strtonum(fm->argv[0], INT_MIN, INT_MAX,
				    &errstr);
				if (errstr != NULL)
					limit = 0;
				if (fm->argc >= 2 && fm->argv[1] != NULL)
					marker = fm->argv[1];
				break;
			case 'p':
				if (fm->argc < 1)
					break;
				width = strtonum(fm->argv[0], INT_MIN, INT_MAX,
				    &errstr);
				if (errstr != NULL)
					width = 0;
				break;
			case 'w':
				modifiers |= FORMAT_WIDTH;
				break;
			case 'e':
				if (fm->argc < 1 || fm->argc > 3)
					break;
				mexp = fm;
				break;
			case 'l':
				modifiers |= FORMAT_LITERAL;
				break;
			case 'a':
				modifiers |= FORMAT_CHARACTER;
				break;
			case 'b':
				modifiers |= FORMAT_BASENAME;
				break;
			case 'd':
				modifiers |= FORMAT_DIRNAME;
				break;
			case 'n':
				modifiers |= FORMAT_LENGTH;
				break;
			case 't':
				modifiers |= FORMAT_TIMESTRING;
				if (fm->argc < 1)
					break;
				if (strchr(fm->argv[0], 'p') != NULL)
					modifiers |= FORMAT_PRETTY;
				else if (fm->argc >= 2 &&
				    strchr(fm->argv[0], 'f') != NULL)
					time_format = format_strip(fm->argv[1]);
				break;
			case 'q':
				if (fm->argc < 1)
					modifiers |= FORMAT_QUOTE_SHELL;
				else if (strchr(fm->argv[0], 'e') != NULL ||
				    strchr(fm->argv[0], 'h') != NULL)
					modifiers |= FORMAT_QUOTE_STYLE;
				break;
			case 'E':
				modifiers |= FORMAT_EXPAND;
				break;
			case 'T':
				modifiers |= FORMAT_EXPANDTIME;
				break;
			case 'N':
				if (fm->argc < 1 ||
				    strchr(fm->argv[0], 'w') != NULL)
					modifiers |= FORMAT_WINDOW_NAME;
				else if (strchr(fm->argv[0], 's') != NULL)
					modifiers |= FORMAT_SESSION_NAME;
				break;
			case 'S':
				modifiers |= FORMAT_SESSIONS;
				break;
			case 'W':
				modifiers |= FORMAT_WINDOWS;
				break;
			case 'P':
				modifiers |= FORMAT_PANES;
				break;
			}
		} else if (fm->size == 2) {
			if (strcmp(fm->modifier, "||") == 0 ||
			    strcmp(fm->modifier, "&&") == 0 ||
			    strcmp(fm->modifier, "==") == 0 ||
			    strcmp(fm->modifier, "!=") == 0 ||
			    strcmp(fm->modifier, ">=") == 0 ||
			    strcmp(fm->modifier, "<=") == 0)
				cmp = fm;
		}
	}

	/* Is this a literal string? */
	if (modifiers & FORMAT_LITERAL) {
		value = xstrdup(copy);
		goto done;
	}

	/* Is this a character? */
	if (modifiers & FORMAT_CHARACTER) {
		new = format_expand1(es, copy);
		c = strtonum(new, 32, 126, &errstr);
		if (errstr != NULL)
			value = xstrdup("");
		else
			xasprintf(&value, "%c", c);
		free (new);
		goto done;
	}

	/* Is this a loop, comparison or condition? */
	if (modifiers & FORMAT_SESSIONS) {
		value = format_loop_sessions(es, copy);
		if (value == NULL)
			goto fail;
	} else if (modifiers & FORMAT_WINDOWS) {
		value = format_loop_windows(es, copy);
		if (value == NULL)
			goto fail;
	} else if (modifiers & FORMAT_PANES) {
		value = format_loop_panes(es, copy);
		if (value == NULL)
			goto fail;
	} else if (modifiers & FORMAT_WINDOW_NAME) {
		value = format_window_name(es, copy);
		if (value == NULL)
			goto fail;
	} else if (modifiers & FORMAT_SESSION_NAME) {
		value = format_session_name(es, copy);
		if (value == NULL)
			goto fail;
	} else if (search != NULL) {
		/* Search in pane. */
		new = format_expand1(es, copy);
		if (wp == NULL) {
			format_log(es, "search '%s' but no pane", new);
			value = xstrdup("0");
		} else {
			format_log(es, "search '%s' pane %%%u", new, wp->id);
			value = format_search(search, wp, new);
		}
		free(new);
	} else if (cmp != NULL) {
		/* Comparison of left and right. */
		if (format_choose(es, copy, &left, &right, 1) != 0) {
			format_log(es, "compare %s syntax error: %s",
			    cmp->modifier, copy);
			goto fail;
		}
		format_log(es, "compare %s left is: %s", cmp->modifier, left);
		format_log(es, "compare %s right is: %s", cmp->modifier, right);

		if (strcmp(cmp->modifier, "||") == 0) {
			if (format_true(left) || format_true(right))
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, "&&") == 0) {
			if (format_true(left) && format_true(right))
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, "==") == 0) {
			if (strcmp(left, right) == 0)
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, "!=") == 0) {
			if (strcmp(left, right) != 0)
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, "<") == 0) {
			if (strcmp(left, right) < 0)
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, ">") == 0) {
			if (strcmp(left, right) > 0)
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, "<=") == 0) {
			if (strcmp(left, right) <= 0)
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, ">=") == 0) {
			if (strcmp(left, right) >= 0)
				value = xstrdup("1");
			else
				value = xstrdup("0");
		} else if (strcmp(cmp->modifier, "m") == 0)
			value = format_match(cmp, left, right);

		free(right);
		free(left);
	} else if (*copy == '?') {
		/* Conditional: check first and choose second or third. */
		cp = format_skip(copy + 1, ",");
		if (cp == NULL) {
			format_log(es, "condition syntax error: %s", copy + 1);
			goto fail;
		}
		condition = xstrndup(copy + 1, cp - (copy + 1));
		format_log(es, "condition is: %s", condition);

		found = format_find(ft, condition, modifiers, time_format);
		if (found == NULL) {
			/*
			 * If the condition not found, try to expand it. If
			 * the expansion doesn't have any effect, then assume
			 * false.
			 */
			found = format_expand1(es, condition);
			if (strcmp(found, condition) == 0) {
				free(found);
				found = xstrdup("");
				format_log(es, "condition '%s' found: %s",
				    condition, found);
			} else {
				format_log(es,
				    "condition '%s' not found; assuming false",
				    condition);
			}
		} else
			format_log(es, "condition '%s' found", condition);

		if (format_choose(es, cp + 1, &left, &right, 0) != 0) {
			format_log(es, "condition '%s' syntax error: %s",
			    condition, cp + 1);
			free(found);
			goto fail;
		}
		if (format_true(found)) {
			format_log(es, "condition '%s' is true", condition);
			value = format_expand1(es, left);
		} else {
			format_log(es, "condition '%s' is false", condition);
			value = format_expand1(es, right);
		}
		free(right);
		free(left);

		free(condition);
		free(found);
	} else if (mexp != NULL) {
		value = format_replace_expression(mexp, es, copy);
		if (value == NULL)
			value = xstrdup("");
	} else {
		if (strstr(copy, "#{") != 0) {
			format_log(es, "expanding inner format '%s'", copy);
			value = format_expand1(es, copy);
		} else {
			value = format_find(ft, copy, modifiers, time_format);
			if (value == NULL) {
				format_log(es, "format '%s' not found", copy);
				value = xstrdup("");
			} else {
				format_log(es, "format '%s' found: %s", copy,
				    value);
			}
		}
	}

done:
	/* Expand again if required. */
	if (modifiers & FORMAT_EXPAND) {
		new = format_expand1(es, value);
		free(value);
		value = new;
	} else if (modifiers & FORMAT_EXPANDTIME) {
		format_copy_state(&next, es, FORMAT_EXPAND_TIME);
		new = format_expand1(&next, value);
		free(value);
		value = new;
	}

	/* Perform substitution if any. */
	for (i = 0; i < nsub; i++) {
		left = format_expand1(es, sub[i]->argv[0]);
		right = format_expand1(es, sub[i]->argv[1]);
		new = format_sub(sub[i], value, left, right);
		format_log(es, "substitute '%s' to '%s': %s", left, right, new);
		free(value);
		value = new;
		free(right);
		free(left);
	}

	/* Truncate the value if needed. */
	if (limit > 0) {
		new = format_trim_left(value, limit);
		if (marker != NULL && strcmp(new, value) != 0) {
			free(value);
			xasprintf(&value, "%s%s", new, marker);
		} else {
			free(value);
			value = new;
		}
		format_log(es, "applied length limit %d: %s", limit, value);
	} else if (limit < 0) {
		new = format_trim_right(value, -limit);
		if (marker != NULL && strcmp(new, value) != 0) {
			free(value);
			xasprintf(&value, "%s%s", marker, new);
		} else {
			free(value);
			value = new;
		}
		format_log(es, "applied length limit %d: %s", limit, value);
	}

	/* Pad the value if needed. */
	if (width > 0) {
		new = utf8_padcstr(value, width);
		free(value);
		value = new;
		format_log(es, "applied padding width %d: %s", width, value);
	} else if (width < 0) {
		new = utf8_rpadcstr(value, -width);
		free(value);
		value = new;
		format_log(es, "applied padding width %d: %s", width, value);
	}

	/* Replace with the length or width if needed. */
	if (modifiers & FORMAT_LENGTH) {
		xasprintf(&new, "%zu", strlen(value));
		free(value);
		value = new;
		format_log(es, "replacing with length: %s", new);
	}
	if (modifiers & FORMAT_WIDTH) {
		xasprintf(&new, "%u", format_width(value));
		free(value);
		value = new;
		format_log(es, "replacing with width: %s", new);
	}

	/* Expand the buffer and copy in the value. */
	valuelen = strlen(value);
	while (*len - *off < valuelen + 1) {
		*buf = xreallocarray(*buf, 2, *len);
		*len *= 2;
	}
	memcpy(*buf + *off, value, valuelen);
	*off += valuelen;

	format_log(es, "replaced '%s' with '%s'", copy0, value);
	free(value);

	free(sub);
	format_free_modifiers(list, count);
	free(copy0);
	return (0);

fail:
	format_log(es, "failed %s", copy0);

	free(sub);
	format_free_modifiers(list, count);
	free(copy0);
	return (-1);
}

/* Expand keys in a template. */
static char *
format_expand1(struct format_expand_state *es, const char *fmt)
{
	struct format_tree	*ft = es->ft;
	char			*buf, *out, *name;
	const char		*ptr, *s;
	size_t			 off, len, n, outlen;
	int     		 ch, brackets;
	char			 expanded[8192];

	if (fmt == NULL || *fmt == '\0')
		return (xstrdup(""));

	if (es->loop == FORMAT_LOOP_LIMIT) {
		format_log(es, "reached loop limit (%u)", FORMAT_LOOP_LIMIT);
		return (xstrdup(""));
	}
	es->loop++;

	format_log(es, "expanding format: %s", fmt);

	if ((es->flags & FORMAT_EXPAND_TIME) && strchr(fmt, '%') != NULL) {
		if (es->time == 0) {
			es->time = time(NULL);
			localtime_r(&es->time, &es->tm);
		}
		if (strftime(expanded, sizeof expanded, fmt, &es->tm) == 0) {
			format_log(es, "format is too long");
			return (xstrdup(""));
		}
		if (format_logging(ft) && strcmp(expanded, fmt) != 0)
			format_log(es, "after time expanded: %s", expanded);
		fmt = expanded;
	}

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

		ch = (u_char)*fmt++;
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

			name = xstrndup(fmt, n);
			format_log(es, "found #(): %s", name);

			if ((ft->flags & FORMAT_NOJOBS) ||
			    (es->flags & FORMAT_EXPAND_NOJOBS)) {
				out = xstrdup("");
				format_log(es, "#() is disabled");
			} else {
				out = format_job_get(es, name);
				format_log(es, "#() result: %s", out);
			}
			free(name);

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
			ptr = format_skip((char *)fmt - 2, "}");
			if (ptr == NULL)
				break;
			n = ptr - fmt;

			format_log(es, "found #{}: %.*s", (int)n, fmt);
			if (format_replace(es, fmt, n, &buf, &len, &off) != 0)
				break;
			fmt += n + 1;
			continue;
		case '#':
			/*
			 * If ##[ (with two or more #s), then it is a style and
			 * can be left for format_draw to handle.
			 */
			ptr = fmt;
			n = 2;
			while (*ptr == '#') {
				ptr++;
				n++;
			}
			if (*ptr == '[') {
				format_log(es, "found #*%zu[", n);
				while (len - off < n + 2) {
					buf = xreallocarray(buf, 2, len);
					len *= 2;
				}
				memcpy(buf + off, fmt - 2, n + 1);
				off += n + 1;
				fmt = ptr + 1;
				continue;
			}
			/* FALLTHROUGH */
		case '}':
		case ',':
			format_log(es, "found #%c", ch);
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
			format_log(es, "found #%c: %s", ch, s);
			if (format_replace(es, s, n, &buf, &len, &off) != 0)
				break;
			continue;
		}

		break;
	}
	buf[off] = '\0';

	format_log(es, "result is: %s", buf);
	es->loop--;

	return (buf);
}

/* Expand keys in a template, passing through strftime first. */
char *
format_expand_time(struct format_tree *ft, const char *fmt)
{
	struct format_expand_state	es;

	memset(&es, 0, sizeof es);
	es.ft = ft;
	es.flags = FORMAT_EXPAND_TIME;
	return (format_expand1(&es, fmt));
}

/* Expand keys in a template. */
char *
format_expand(struct format_tree *ft, const char *fmt)
{
	struct format_expand_state	es;

	memset(&es, 0, sizeof es);
	es.ft = ft;
	es.flags = 0;
	return (format_expand1(&es, fmt));
}

/* Expand a single string. */
char *
format_single(struct cmdq_item *item, const char *fmt, struct client *c,
    struct session *s, struct winlink *wl, struct window_pane *wp)
{
	struct format_tree	*ft;
	char			*expanded;

	ft = format_create_defaults(item, c, s, wl, wp);
	expanded = format_expand(ft, fmt);
	format_free(ft);
	return (expanded);
}

/* Expand a single string using state. */
char *
format_single_from_state(struct cmdq_item *item, const char *fmt,
    struct client *c, struct cmd_find_state *fs)
{
	return (format_single(item, fmt, c, fs->s, fs->wl, fs->wp));
}

/* Expand a single string using target. */
char *
format_single_from_target(struct cmdq_item *item, const char *fmt)
{
	struct client	*tc = cmdq_get_target_client(item);

	return (format_single_from_state(item, fmt, tc, cmdq_get_target(item)));
}

/* Create and add defaults. */
struct format_tree *
format_create_defaults(struct cmdq_item *item, struct client *c,
    struct session *s, struct winlink *wl, struct window_pane *wp)
{
	struct format_tree	*ft;

	if (item != NULL)
		ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	else
		ft = format_create(NULL, item, FORMAT_NONE, 0);
	format_defaults(ft, c, s, wl, wp);
	return (ft);
}

/* Create and add defaults using state. */
struct format_tree *
format_create_from_state(struct cmdq_item *item, struct client *c,
    struct cmd_find_state *fs)
{
	return (format_create_defaults(item, c, fs->s, fs->wl, fs->wp));
}

/* Create and add defaults using target. */
struct format_tree *
format_create_from_target(struct cmdq_item *item)
{
	struct client	*tc = cmdq_get_target_client(item);

	return (format_create_from_state(item, tc, cmdq_get_target(item)));
}

/* Set defaults for any of arguments that are not NULL. */
void
format_defaults(struct format_tree *ft, struct client *c, struct session *s,
    struct winlink *wl, struct window_pane *wp)
{
	struct paste_buffer	*pb;

	if (c != NULL && c->name != NULL)
		log_debug("%s: c=%s", __func__, c->name);
	else
		log_debug("%s: c=none", __func__);
	if (s != NULL)
		log_debug("%s: s=$%u", __func__, s->id);
	else
		log_debug("%s: s=none", __func__);
	if (wl != NULL)
		log_debug("%s: wl=%u", __func__, wl->idx);
	else
		log_debug("%s: wl=none", __func__);
	if (wp != NULL)
		log_debug("%s: wp=%%%u", __func__, wp->id);
	else
		log_debug("%s: wp=none", __func__);

	if (c != NULL && s != NULL && c->session != s)
		log_debug("%s: session does not match", __func__);

	if (wp != NULL)
		ft->type = FORMAT_TYPE_PANE;
	else if (wl != NULL)
		ft->type = FORMAT_TYPE_WINDOW;
	else if (s != NULL)
		ft->type = FORMAT_TYPE_SESSION;
	else
		ft->type = FORMAT_TYPE_UNKNOWN;

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

	pb = paste_get_top (NULL);
	if (pb != NULL)
		format_defaults_paste_buffer(ft, pb);
}

/* Set default format keys for a session. */
static void
format_defaults_session(struct format_tree *ft, struct session *s)
{
	ft->s = s;
}

/* Set default format keys for a client. */
static void
format_defaults_client(struct format_tree *ft, struct client *c)
{
	if (ft->s == NULL)
		ft->s = c->session;
	ft->c = c;
}

/* Set default format keys for a window. */
void
format_defaults_window(struct format_tree *ft, struct window *w)
{
	ft->w = w;
}

/* Set default format keys for a winlink. */
static void
format_defaults_winlink(struct format_tree *ft, struct winlink *wl)
{
	if (ft->w == NULL)
		format_defaults_window(ft, wl->window);
	ft->wl = wl;
}

/* Set default format keys for a window pane. */
void
format_defaults_pane(struct format_tree *ft, struct window_pane *wp)
{
	struct window_mode_entry	*wme;

	if (ft->w == NULL)
		format_defaults_window(ft, wp->window);
	ft->wp = wp;

	wme = TAILQ_FIRST(&wp->modes);
	if (wme != NULL && wme->mode->formats != NULL)
		wme->mode->formats(wme, ft);
}

/* Set default format keys for paste buffer. */
void
format_defaults_paste_buffer(struct format_tree *ft, struct paste_buffer *pb)
{
	ft->pb = pb;
}

/* Return word at given coordinates. Caller frees. */
char *
format_grid_word(struct grid *gd, u_int x, u_int y)
{
	const struct grid_line	*gl;
	struct grid_cell	 gc;
	const char		*ws;
	struct utf8_data	*ud = NULL;
	u_int			 end;
	size_t			 size = 0;
	int			 found = 0;
	char			*s = NULL;

	ws = options_get_string(global_s_options, "word-separators");

	for (;;) {
		grid_get_cell(gd, x, y, &gc);
		if (gc.flags & GRID_FLAG_PADDING)
			break;
		if (utf8_cstrhas(ws, &gc.data)) {
			found = 1;
			break;
		}

		if (x == 0) {
			if (y == 0)
				break;
			gl = grid_peek_line(gd, y - 1);
			if (~gl->flags & GRID_LINE_WRAPPED)
				break;
			y--;
			x = grid_line_length(gd, y);
			if (x == 0)
				break;
		}
		x--;
	}
	for (;;) {
		if (found) {
			end = grid_line_length(gd, y);
			if (end == 0 || x == end - 1) {
				if (y == gd->hsize + gd->sy - 1)
					break;
				gl = grid_peek_line(gd, y);
				if (~gl->flags & GRID_LINE_WRAPPED)
					break;
				y++;
				x = 0;
			} else
				x++;
		}
		found = 1;

		grid_get_cell(gd, x, y, &gc);
		if (gc.flags & GRID_FLAG_PADDING)
			break;
		if (utf8_cstrhas(ws, &gc.data))
			break;

		ud = xreallocarray(ud, size + 2, sizeof *ud);
		memcpy(&ud[size++], &gc.data, sizeof *ud);
	}
	if (size != 0) {
		ud[size].size = 0;
		s = utf8_tocstr(ud);
		free(ud);
	}
	return (s);
}

/* Return line at given coordinates. Caller frees. */
char *
format_grid_line(struct grid *gd, u_int y)
{
	struct grid_cell	 gc;
	struct utf8_data	*ud = NULL;
	u_int			 x;
	size_t			 size = 0;
	char			*s = NULL;

	for (x = 0; x < grid_line_length(gd, y); x++) {
		grid_get_cell(gd, x, y, &gc);
		if (gc.flags & GRID_FLAG_PADDING)
			break;

		ud = xreallocarray(ud, size + 2, sizeof *ud);
		memcpy(&ud[size++], &gc.data, sizeof *ud);
	}
	if (size != 0) {
		ud[size].size = 0;
		s = utf8_tocstr(ud);
		free(ud);
	}
	return (s);
}
