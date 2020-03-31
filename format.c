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
static void	 format_defaults_winlink(struct format_tree *, struct winlink *);

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
#define FORMAT_QUOTE 0x8
#define FORMAT_LITERAL 0x10
#define FORMAT_EXPAND 0x20
#define FORMAT_EXPANDTIME 0x40
#define FORMAT_SESSIONS 0x80
#define FORMAT_WINDOWS 0x100
#define FORMAT_PANES 0x200

/* Limit on recursion. */
#define FORMAT_LOOP_LIMIT 10

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
	struct client		*c;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;

	struct cmdq_item	*item;
	struct client		*client;
	u_int			 tag;
	int			 flags;
	time_t			 time;
	u_int			 loop;

	struct mouse_event	 m;

	RB_HEAD(format_entry_tree, format_entry) tree;
};
static int format_entry_cmp(struct format_entry *, struct format_entry *);
RB_GENERATE_STATIC(format_entry_tree, format_entry, entry, format_entry_cmp);

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
format_log1(struct format_tree *ft, const char *from, const char *fmt, ...)
{
	va_list			 ap;
	char			*s;
	static const char	 spaces[] = "          ";

	if (!format_logging(ft))
		return;

	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);

	log_debug("%s: %s", from, s);
	if (ft->item != NULL && (ft->flags & FORMAT_VERBOSE))
		cmdq_print(ft->item, "#%.*s%s", ft->loop, spaces, s);

	free(s);
}
#define format_log(ft, fmt, ...) format_log1(ft, __func__, fmt, ##__VA_ARGS__)

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
	if (force && fj->job != NULL)
	       job_free(fj->job);
	if (force || (fj->job == NULL && fj->last != t)) {
		fj->job = job_run(expanded, NULL,
		    server_client_get_cwd(ft->client, NULL), format_job_update,
		    format_job_complete, NULL, fj, JOB_NOWAIT, -1, -1);
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

/* Callback for session_attached_list. */
static void
format_cb_session_attached_list(struct format_tree *ft, struct format_entry *fe)
{
	struct session	*s = ft->s;
	struct client	*loop;
	struct evbuffer	*buffer;
	int		 size;

	if (s == NULL)
		return;

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
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
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

/* Callback for window_linked_sessions_list. */
static void
format_cb_window_linked_sessions_list(struct format_tree *ft,
    struct format_entry *fe)
{
	struct window	*w = ft->wl->window;
	struct winlink	*wl;
	struct evbuffer	*buffer;
	int		 size;

	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%s", wl->session->name);
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Callback for window_active_sessions. */
static void
format_cb_window_active_sessions(struct format_tree *ft,
    struct format_entry *fe)
{
	struct window	*w = ft->wl->window;
	struct winlink	*wl;
	u_int		 n = 0;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session->curw == wl)
			n++;
	}

	xasprintf(&fe->value, "%u", n);
}

/* Callback for window_active_sessions_list. */
static void
format_cb_window_active_sessions_list(struct format_tree *ft,
    struct format_entry *fe)
{
	struct window	*w = ft->wl->window;
	struct winlink	*wl;
	struct evbuffer	*buffer;
	int		 size;

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
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Callback for window_active_clients. */
static void
format_cb_window_active_clients(struct format_tree *ft, struct format_entry *fe)
{
	struct window	*w = ft->wl->window;
	struct client	*loop;
	struct session	*client_session;
	u_int		 n = 0;

	TAILQ_FOREACH(loop, &clients, entry) {
		client_session = loop->session;
		if (client_session == NULL)
			continue;

		if (w == client_session->curw->window)
			n++;
	}

	xasprintf(&fe->value, "%u", n);
}

/* Callback for window_active_clients_list. */
static void
format_cb_window_active_clients_list(struct format_tree *ft,
    struct format_entry *fe)
{
	struct window	*w = ft->wl->window;
	struct client	*loop;
	struct session	*client_session;
	struct evbuffer	*buffer;
	int		 size;

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
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
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

	if (wp == NULL || wp->shell == NULL)
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
	if (buffer == NULL)
		fatalx("out of memory");

	TAILQ_FOREACH(loop, &sg->sessions, gentry) {
		if (EVBUFFER_LENGTH(buffer) > 0)
			evbuffer_add(buffer, ",", 1);
		evbuffer_add_printf(buffer, "%s", loop->name);
	}

	if ((size = EVBUFFER_LENGTH(buffer)) != 0)
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Callback for session_group_attached_list. */
static void
format_cb_session_group_attached_list(struct format_tree *ft,
    struct format_entry *fe)
{
	struct session		*s = ft->s, *client_session, *session_loop;
	struct session_group	*sg;
	struct client		*loop;
	struct evbuffer		*buffer;
	int			 size;

	if (s == NULL)
		return;
	sg = session_group_contains(s);
	if (sg == NULL)
		return;

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
		xasprintf(&fe->value, "%.*s", size, EVBUFFER_DATA(buffer));
	evbuffer_free(buffer);
}

/* Callback for pane_in_mode. */
static void
format_cb_pane_in_mode(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane		*wp = ft->wp;
	u_int				 n = 0;
	struct window_mode_entry	*wme;

	if (wp == NULL)
		return;

	TAILQ_FOREACH(wme, &wp->modes, entry)
	    n++;
	xasprintf(&fe->value, "%u", n);
}

/* Callback for pane_at_top. */
static void
format_cb_pane_at_top(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;
	struct window		*w = wp->window;
	int			 status, flag;

	if (wp == NULL)
		return;

	status = options_get_number(w->options, "pane-border-status");
	if (status == PANE_STATUS_TOP)
		flag = (wp->yoff == 1);
	else
		flag = (wp->yoff == 0);
	xasprintf(&fe->value, "%d", flag);
}

/* Callback for pane_at_bottom. */
static void
format_cb_pane_at_bottom(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;
	struct window		*w = wp->window;
	int			 status, flag;

	if (wp == NULL)
		return;

	status = options_get_number(w->options, "pane-border-status");
	if (status == PANE_STATUS_BOTTOM)
		flag = (wp->yoff + wp->sy == w->sy - 1);
	else
		flag = (wp->yoff + wp->sy == w->sy);
	xasprintf(&fe->value, "%d", flag);
}

/* Callback for cursor_character. */
static void
format_cb_cursor_character(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp = ft->wp;
	struct grid_cell	 gc;

	if (wp == NULL)
		return;

	grid_view_get_cell(wp->base.grid, wp->base.cx, wp->base.cy, &gc);
	if (~gc.flags & GRID_FLAG_PADDING)
		xasprintf(&fe->value, "%.*s", (int)gc.data.size, gc.data.data);
}

/* Return word at given coordinates. Caller frees. */
char *
format_grid_word(struct grid *gd, u_int x, u_int y)
{
	struct grid_line	*gl;
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
			gl = &gd->linedata[y - 1];
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
				gl = &gd->linedata[y];
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

/* Callback for mouse_word. */
static void
format_cb_mouse_word(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp;
	struct grid		*gd;
	u_int			 x, y;
	char			*s;

	if (!ft->m.valid)
		return;
	wp = cmd_mouse_pane(&ft->m, NULL, NULL);
	if (wp == NULL)
		return;
	if (cmd_mouse_at(wp, &ft->m, &x, &y, 0) != 0)
		return;

	if (!TAILQ_EMPTY(&wp->modes)) {
		if (TAILQ_FIRST(&wp->modes)->mode == &window_copy_mode ||
		    TAILQ_FIRST(&wp->modes)->mode == &window_view_mode)
			s = window_copy_get_word(wp, x, y);
		else
			s = NULL;
	} else {
		gd = wp->base.grid;
		s = format_grid_word(gd, x, gd->hsize + y);
	}
	if (s != NULL)
		fe->value = s;
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

/* Callback for mouse_line. */
static void
format_cb_mouse_line(struct format_tree *ft, struct format_entry *fe)
{
	struct window_pane	*wp;
	struct grid		*gd;
	u_int			 x, y;
	char			*s;

	if (!ft->m.valid)
		return;
	wp = cmd_mouse_pane(&ft->m, NULL, NULL);
	if (wp == NULL)
		return;
	if (cmd_mouse_at(wp, &ft->m, &x, &y, 0) != 0)
		return;

	if (!TAILQ_EMPTY(&wp->modes)) {
		if (TAILQ_FIRST(&wp->modes)->mode == &window_copy_mode ||
		    TAILQ_FIRST(&wp->modes)->mode == &window_view_mode)
			s = window_copy_get_line(wp, y);
		else
			s = NULL;
	} else {
		gd = wp->base.grid;
		s = format_grid_line(gd, gd->hsize + y);
	}
	if (s != NULL)
		fe->value = s;
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

/* Add item bits to tree. */
static void
format_create_add_item(struct format_tree *ft, struct cmdq_item *item)
{
	struct mouse_event	*m;
	struct window_pane	*wp;
	u_int			 x, y;

	if (item->cmd != NULL)
		format_add(ft, "command", "%s", item->cmd->entry->name);

	if (item->shared == NULL)
		return;
	if (item->shared->formats != NULL)
		format_merge(ft, item->shared->formats);

	m = &item->shared->mouse;
	if (m->valid && ((wp = cmd_mouse_pane(m, NULL, NULL)) != NULL)) {
		format_add(ft, "mouse_pane", "%%%u", wp->id);
		if (cmd_mouse_at(wp, m, &x, &y, 0) == 0) {
			format_add(ft, "mouse_x", "%u", x);
			format_add(ft, "mouse_y", "%u", y);
			format_add_cb(ft, "mouse_word", format_cb_mouse_word);
			format_add_cb(ft, "mouse_line", format_cb_mouse_line);
		}
	}
	memcpy(&ft->m, m, sizeof ft->m);
}

/* Create a new tree. */
struct format_tree *
format_create(struct client *c, struct cmdq_item *item, int tag, int flags)
{
	struct format_tree		 *ft;
	const struct window_mode	**wm;
	char				  tmp[64];

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
	ft->item = item;

	ft->tag = tag;
	ft->flags = flags;
	ft->time = time(NULL);

	format_add(ft, "version", "%s", getversion());
	format_add_cb(ft, "host", format_cb_host);
	format_add_cb(ft, "host_short", format_cb_host_short);
	format_add_cb(ft, "pid", format_cb_pid);
	format_add(ft, "socket_path", "%s", socket_path);
	format_add_tv(ft, "start_time", &start_time);

	for (wm = all_window_modes; *wm != NULL; wm++) {
		if ((*wm)->default_format != NULL) {
			xsnprintf(tmp, sizeof tmp, "%s_format", (*wm)->name);
			tmp[strcspn(tmp, "-")] = '_';
			format_add(ft, tmp, "%s", (*wm)->default_format);
		}
	}

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
	struct format_entry	*fe;
	char			 s[64];

	RB_FOREACH(fe, format_entry_tree, &ft->tree) {
		if (fe->t != 0) {
			xsnprintf(s, sizeof s, "%lld", (long long)fe->t);
			cb(fe->key, s, arg);
		} else {
			if (fe->value == NULL && fe->cb != NULL) {
				fe->cb(ft, fe);
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
	fe->t = 0;

	va_start(ap, fmt);
	xvasprintf(&fe->value, fmt, ap);
	va_end(ap);
}

/* Add a key and time. */
static void
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

/* Quote special characters in string. */
static char *
format_quote(const char *s)
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

/* Find a format entry. */
static char *
format_find(struct format_tree *ft, const char *key, int modifiers)
{
	struct format_entry	*fe, fe_find;
	struct environ_entry	*envent;
	static char		 s[64];
	struct options_entry	*o;
	int			 idx;
	char			*found, *saved;

	if (~modifiers & FORMAT_TIMESTRING) {
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
			found = xstrdup(s);
			goto found;
		}
		if (fe->t != 0) {
			xasprintf(&found, "%lld", (long long)fe->t);
			goto found;
		}
		if (fe->value == NULL && fe->cb != NULL)
			fe->cb(ft, fe);
		if (fe->value == NULL)
			fe->value = xstrdup("");
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
	if (found == NULL)
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
	if (modifiers & FORMAT_QUOTE) {
		saved = found;
		found = xstrdup(format_quote(saved));
		free(saved);
	}
	return (found);
}

/* Skip until end. */
const char *
format_skip(const char *s, const char *end)
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
		if (strchr(end, *s) != NULL && brackets == 0)
			break;
	}
	if (*s == '\0')
		return (NULL);
	return (s);
}

/* Return left and right alternatives separated by commas. */
static int
format_choose(struct format_tree *ft, const char *s, char **left, char **right,
    int expand)
{
	const char	*cp;
	char		*left0, *right0;

	cp = format_skip(s, ",");
	if (cp == NULL)
		return (-1);
	left0 = xstrndup(s, cp - s);
	right0 = xstrdup(cp + 1);

	if (expand) {
		*left = format_expand(ft, left0);
		free(left0);
		*right = format_expand(ft, right0);
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
format_build_modifiers(struct format_tree *ft, const char **s, u_int *count)
{
	const char		*cp = *s, *end;
	struct format_modifier	*list = NULL;
	char			 c, last[] = "X;:", **argv, *value;
	int			 argc;

	/*
	 * Modifiers are a ; separated list of the forms:
	 *      l,m,C,b,d,t,q,E,T,S,W,P,<,>
	 *	=a
	 *	=/a
	 *      =/a/
	 *	s/a/b/
	 *	s/a/b
	 *	||,&&,!=,==,<=,>=
	 */

	*count = 0;

	while (*cp != '\0' && *cp != ':') {
		/* Skip and separator character. */
		if (*cp == ';')
			cp++;

		/* Check single character modifiers with no arguments. */
		if (strchr("lbdtqETSWP<>", cp[0]) != NULL &&
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
		if (strchr("mCs=pe", cp[0]) == NULL)
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
			argv[0] = format_expand(ft, value);
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
			argv[argc++] = format_expand(ft, value);
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
	return list;
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

/* Loop over sessions. */
static char *
format_loop_sessions(struct format_tree *ft, const char *fmt)
{
	struct client		*c = ft->client;
	struct cmdq_item	*item = ft->item;
	struct format_tree	*nft;
	char			*expanded, *value;
	size_t			 valuelen;
	struct session		*s;

	value = xcalloc(1, 1);
	valuelen = 1;

	RB_FOREACH(s, sessions, &sessions) {
		format_log(ft, "session loop: $%u", s->id);
		nft = format_create(c, item, FORMAT_NONE, ft->flags);
		nft->loop = ft->loop;
		format_defaults(nft, ft->c, s, NULL, NULL);
		expanded = format_expand(nft, fmt);
		format_free(nft);

		valuelen += strlen(expanded);
		value = xrealloc(value, valuelen);

		strlcat(value, expanded, valuelen);
		free(expanded);
	}

	return (value);
}

/* Loop over windows. */
static char *
format_loop_windows(struct format_tree *ft, const char *fmt)
{
	struct client		*c = ft->client;
	struct cmdq_item	*item = ft->item;
	struct format_tree	*nft;
	char			*all, *active, *use, *expanded, *value;
	size_t			 valuelen;
	struct winlink		*wl;
	struct window		*w;

	if (ft->s == NULL) {
		format_log(ft, "window loop but no session");
		return (NULL);
	}

	if (format_choose(ft, fmt, &all, &active, 0) != 0) {
		all = xstrdup(fmt);
		active = NULL;
	}

	value = xcalloc(1, 1);
	valuelen = 1;

	RB_FOREACH(wl, winlinks, &ft->s->windows) {
		w = wl->window;
		format_log(ft, "window loop: %u @%u", wl->idx, w->id);
		if (active != NULL && wl == ft->s->curw)
			use = active;
		else
			use = all;
		nft = format_create(c, item, FORMAT_WINDOW|w->id, ft->flags);
		nft->loop = ft->loop;
		format_defaults(nft, ft->c, ft->s, wl, NULL);
		expanded = format_expand(nft, use);
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
format_loop_panes(struct format_tree *ft, const char *fmt)
{
	struct client		*c = ft->client;
	struct cmdq_item	*item = ft->item;
	struct format_tree	*nft;
	char			*all, *active, *use, *expanded, *value;
	size_t			 valuelen;
	struct window_pane	*wp;

	if (ft->w == NULL) {
		format_log(ft, "pane loop but no window");
		return (NULL);
	}

	if (format_choose(ft, fmt, &all, &active, 0) != 0) {
		all = xstrdup(fmt);
		active = NULL;
	}

	value = xcalloc(1, 1);
	valuelen = 1;

	TAILQ_FOREACH(wp, &ft->w->panes, entry) {
		format_log(ft, "pane loop: %%%u", wp->id);
		if (active != NULL && wp == ft->w->active)
			use = active;
		else
			use = all;
		nft = format_create(c, item, FORMAT_PANE|wp->id, ft->flags);
		nft->loop = ft->loop;
		format_defaults(nft, ft->c, ft->s, ft->wl, wp);
		expanded = format_expand(nft, use);
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
format_replace_expression(struct format_modifier *mexp, struct format_tree *ft,
    const char *copy)
{
	int		 argc = mexp->argc;
	const char	*errstr;
	char		*endch, *value, *left = NULL, *right = NULL;
	int		 use_fp = 0;
	u_int		 prec = 0;
	double		 mleft, mright, result;
	enum { ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULUS } operator;

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
	else {
		format_log(ft, "expression has no valid operator: '%s'",
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
			format_log (ft, "expression precision %s: %s", errstr,
			    mexp->argv[2]);
			goto fail;
		}
	}

	if (format_choose(ft, copy, &left, &right, 1) != 0) {
		format_log(ft, "expression syntax error");
		goto fail;
	}

	mleft = strtod(left, &endch);
	if (*endch != '\0') {
		format_log(ft, "expression left side is invalid: %s", left);
		goto fail;
	}

	mright = strtod(right, &endch);
	if (*endch != '\0') {
		format_log(ft, "expression right side is invalid: %s", right);
		goto fail;
	}

	if (!use_fp) {
		mleft = (long long)mleft;
		mright = (long long)mright;
	}
	format_log(ft, "expression left side is: %.*f", prec, mleft);
	format_log(ft, "expression right side is:  %.*f", prec, mright);

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
	}
	if (use_fp)
		xasprintf(&value, "%.*f", prec, result);
	else
		xasprintf(&value, "%.*f", prec, (double)(long long)result);
	format_log(ft, "expression result is %s", value);

	free(right);
	free(left);
	return value;

fail:
	free(right);
	free(left);
	return (NULL);
}

/* Replace a key. */
static int
format_replace(struct format_tree *ft, const char *key, size_t keylen,
    char **buf, size_t *len, size_t *off)
{
	struct window_pane	 *wp = ft->wp;
	const char		 *errptr, *copy, *cp, *marker = NULL;
	char			 *copy0, *condition, *found, *new;
	char			 *value, *left, *right;
	size_t			  valuelen;
	int			  modifiers = 0, limit = 0, width = 0, j;
	struct format_modifier   *list, *fm, *cmp = NULL, *search = NULL;
	struct format_modifier	**sub = NULL, *mexp = NULL;
	u_int			  i, count, nsub = 0;

	/* Make a copy of the key. */
	copy = copy0 = xstrndup(key, keylen);

	/* Process modifier list. */
	list = format_build_modifiers(ft, &copy, &count);
	for (i = 0; i < count; i++) {
		fm = &list[i];
		if (format_logging(ft)) {
			format_log(ft, "modifier %u is %s", i, fm->modifier);
			for (j = 0; j < fm->argc; j++) {
				format_log(ft, "modifier %u argument %d: %s", i,
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
				    &errptr);
				if (errptr != NULL)
					limit = 0;
				if (fm->argc >= 2 && fm->argv[1] != NULL)
					marker = fm->argv[1];
				break;
			case 'p':
				if (fm->argc < 1)
					break;
				width = strtonum(fm->argv[0], INT_MIN, INT_MAX,
				    &errptr);
				if (errptr != NULL)
					width = 0;
				break;
			case 'e':
				if (fm->argc < 1 || fm->argc > 3)
					break;
			    mexp = fm;
				break;
			case 'l':
				modifiers |= FORMAT_LITERAL;
				break;
			case 'b':
				modifiers |= FORMAT_BASENAME;
				break;
			case 'd':
				modifiers |= FORMAT_DIRNAME;
				break;
			case 't':
				modifiers |= FORMAT_TIMESTRING;
				break;
			case 'q':
				modifiers |= FORMAT_QUOTE;
				break;
			case 'E':
				modifiers |= FORMAT_EXPAND;
				break;
			case 'T':
				modifiers |= FORMAT_EXPANDTIME;
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

	/* Is this a loop, comparison or condition? */
	if (modifiers & FORMAT_SESSIONS) {
		value = format_loop_sessions(ft, copy);
		if (value == NULL)
			goto fail;
	} else if (modifiers & FORMAT_WINDOWS) {
		value = format_loop_windows(ft, copy);
		if (value == NULL)
			goto fail;
	} else if (modifiers & FORMAT_PANES) {
		value = format_loop_panes(ft, copy);
		if (value == NULL)
			goto fail;
	} else if (search != NULL) {
		/* Search in pane. */
		new = format_expand(ft, copy);
		if (wp == NULL) {
			format_log(ft, "search '%s' but no pane", new);
			value = xstrdup("0");
		} else {
			format_log(ft, "search '%s' pane %%%u", new,  wp->id);
			value = format_search(fm, wp, new);
		}
		free(new);
	} else if (cmp != NULL) {
		/* Comparison of left and right. */
		if (format_choose(ft, copy, &left, &right, 1) != 0) {
			format_log(ft, "compare %s syntax error: %s",
			    cmp->modifier, copy);
			goto fail;
		}
		format_log(ft, "compare %s left is: %s", cmp->modifier, left);
		format_log(ft, "compare %s right is: %s", cmp->modifier, right);

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
			format_log(ft, "condition syntax error: %s", copy + 1);
			goto fail;
		}
		condition = xstrndup(copy + 1, cp - (copy + 1));
		format_log(ft, "condition is: %s", condition);

		found = format_find(ft, condition, modifiers);
		if (found == NULL) {
			/*
			 * If the condition not found, try to expand it. If
			 * the expansion doesn't have any effect, then assume
			 * false.
			 */
			found = format_expand(ft, condition);
			if (strcmp(found, condition) == 0) {
				free(found);
				found = xstrdup("");
				format_log(ft, "condition '%s' found: %s",
				    condition, found);
			} else {
				format_log(ft,
				    "condition '%s' not found; assuming false",
				    condition);
			}
		} else
			format_log(ft, "condition '%s' found", condition);

		if (format_choose(ft, cp + 1, &left, &right, 0) != 0) {
			format_log(ft, "condition '%s' syntax error: %s",
			    condition, cp + 1);
			free(found);
			goto fail;
		}
		if (format_true(found)) {
			format_log(ft, "condition '%s' is true", condition);
			value = format_expand(ft, left);
		} else {
			format_log(ft, "condition '%s' is false", condition);
			value = format_expand(ft, right);
		}
		free(right);
		free(left);

		free(condition);
		free(found);
	} else if (mexp != NULL) {
		value = format_replace_expression(mexp, ft, copy);
		if (value == NULL)
			value = xstrdup("");
	} else {
		/* Neither: look up directly. */
		value = format_find(ft, copy, modifiers);
		if (value == NULL) {
			format_log(ft, "format '%s' not found", copy);
			value = xstrdup("");
		} else
			format_log(ft, "format '%s' found: %s", copy, value);
	}

done:
	/* Expand again if required. */
	if (modifiers & FORMAT_EXPAND) {
		new = format_expand(ft, value);
		free(value);
		value = new;
	}
	else if (modifiers & FORMAT_EXPANDTIME) {
		new = format_expand_time(ft, value);
		free(value);
		value = new;
	}

	/* Perform substitution if any. */
	for (i = 0; i < nsub; i++) {
		left = format_expand(ft, sub[i]->argv[0]);
		right = format_expand(ft, sub[i]->argv[1]);
		new = format_sub(sub[i], value, left, right);
		format_log(ft, "substitute '%s' to '%s': %s", left, right, new);
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
		format_log(ft, "applied length limit %d: %s", limit, value);
	} else if (limit < 0) {
		new = format_trim_right(value, -limit);
		if (marker != NULL && strcmp(new, value) != 0) {
			free(value);
			xasprintf(&value, "%s%s", marker, new);
		} else {
			free(value);
			value = new;
		}
		format_log(ft, "applied length limit %d: %s", limit, value);
	}

	/* Pad the value if needed. */
	if (width > 0) {
		new = utf8_padcstr(value, width);
		free(value);
		value = new;
		format_log(ft, "applied padding width %d: %s", width, value);
	} else if (width < 0) {
		new = utf8_rpadcstr(value, -width);
		free(value);
		value = new;
		format_log(ft, "applied padding width %d: %s", width, value);
	}

	/* Expand the buffer and copy in the value. */
	valuelen = strlen(value);
	while (*len - *off < valuelen + 1) {
		*buf = xreallocarray(*buf, 2, *len);
		*len *= 2;
	}
	memcpy(*buf + *off, value, valuelen);
	*off += valuelen;

	format_log(ft, "replaced '%s' with '%s'", copy0, value);
	free(value);

	free(sub);
	format_free_modifiers(list, count);
	free(copy0);
	return (0);

fail:
	format_log(ft, "failed %s", copy0);

	free(sub);
	format_free_modifiers(list, count);
	free(copy0);
	return (-1);
}

/* Expand keys in a template. */
static char *
format_expand1(struct format_tree *ft, const char *fmt, int time)
{
	char		*buf, *out, *name;
	const char	*ptr, *s;
	size_t		 off, len, n, outlen;
	int     	 ch, brackets;
	struct tm	*tm;
	char		 expanded[8192];

	if (fmt == NULL || *fmt == '\0')
		return (xstrdup(""));

	if (ft->loop == FORMAT_LOOP_LIMIT)
		return (xstrdup(""));
	ft->loop++;

	format_log(ft, "expanding format: %s", fmt);

	if (time) {
		tm = localtime(&ft->time);
		if (strftime(expanded, sizeof expanded, fmt, tm) == 0) {
			format_log(ft, "format is too long");
			return (xstrdup(""));
		}
		if (format_logging(ft) && strcmp(expanded, fmt) != 0)
			format_log(ft, "after time expanded: %s", expanded);
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
			format_log(ft, "found #(): %s", name);

			if (ft->flags & FORMAT_NOJOBS) {
				out = xstrdup("");
				format_log(ft, "#() is disabled");
			} else {
				out = format_job_get(ft, name);
				format_log(ft, "#() result: %s", out);
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

			format_log(ft, "found #{}: %.*s", (int)n, fmt);
			if (format_replace(ft, fmt, n, &buf, &len, &off) != 0)
				break;
			fmt += n + 1;
			continue;
		case '}':
		case '#':
		case ',':
			format_log(ft, "found #%c", ch);
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
			format_log(ft, "found #%c: %s", ch, s);
			if (format_replace(ft, s, n, &buf, &len, &off) != 0)
				break;
			continue;
		}

		break;
	}
	buf[off] = '\0';

	format_log(ft, "result is: %s", buf);
	ft->loop--;

	return (buf);
}

/* Expand keys in a template, passing through strftime first. */
char *
format_expand_time(struct format_tree *ft, const char *fmt)
{
	return (format_expand1(ft, fmt, 1));
}

/* Expand keys in a template. */
char *
format_expand(struct format_tree *ft, const char *fmt)
{
	return (format_expand1(ft, fmt, 0));
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
		log_debug("%s: wl=%u w=@%u", __func__, wl->idx, wl->window->id);
	else
		log_debug("%s: wl=none", __func__);
	if (wp != NULL)
		log_debug("%s: wp=%%%u", __func__, wp->id);
	else
		log_debug("%s: wp=none", __func__);

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

	pb = paste_get_top (NULL);
	if (pb != NULL)
		format_defaults_paste_buffer(ft, pb);
}

/* Set default format keys for a session. */
static void
format_defaults_session(struct format_tree *ft, struct session *s)
{
	struct session_group	*sg;

	ft->s = s;

	format_add(ft, "session_name", "%s", s->name);
	format_add(ft, "session_path", "%s", s->cwd);
	format_add(ft, "session_windows", "%u", winlink_count(&s->windows));
	format_add(ft, "session_id", "$%u", s->id);

	sg = session_group_contains(s);
	format_add(ft, "session_grouped", "%d", sg != NULL);
	if (sg != NULL) {
		format_add(ft, "session_group", "%s", sg->name);
		format_add(ft, "session_group_size", "%u",
		    session_group_count (sg));
		format_add(ft, "session_group_attached", "%u",
		    session_group_attached_count (sg));
		format_add(ft, "session_group_many_attached", "%u",
		    session_group_attached_count (sg) > 1);
		format_add_cb(ft, "session_group_list",
		    format_cb_session_group_list);
		format_add_cb(ft, "session_group_attached_list",
		    format_cb_session_group_attached_list);
	}

	format_add_tv(ft, "session_created", &s->creation_time);
	format_add_tv(ft, "session_last_attached", &s->last_attached_time);
	format_add_tv(ft, "session_activity", &s->activity_time);

	format_add(ft, "session_attached", "%u", s->attached);
	format_add(ft, "session_many_attached", "%d", s->attached > 1);
	format_add_cb(ft, "session_attached_list",
	    format_cb_session_attached_list);

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

	if (ft->s == NULL)
		ft->s = c->session;
	ft->c = c;

	format_add(ft, "client_name", "%s", c->name);
	format_add(ft, "client_pid", "%ld", (long) c->pid);
	format_add(ft, "client_height", "%u", tty->sy);
	format_add(ft, "client_width", "%u", tty->sx);
	format_add(ft, "client_cell_width", "%u", tty->xpixel);
	format_add(ft, "client_cell_height", "%u", tty->ypixel);
	format_add(ft, "client_tty", "%s", c->ttyname);
	format_add(ft, "client_control_mode", "%d",
		!!(c->flags & CLIENT_CONTROL));

	if (tty->term_name != NULL)
		format_add(ft, "client_termname", "%s", tty->term_name);

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
	format_add(ft, "window_cell_width", "%u", w->xpixel);
	format_add(ft, "window_cell_height", "%u", w->ypixel);
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
	struct client	*c = ft->c;
	struct session	*s = wl->session;
	struct window	*w = wl->window;
	int		 flag;
	u_int		 ox, oy, sx, sy;

	if (ft->w == NULL)
		ft->w = wl->window;
	ft->wl = wl;

	format_defaults_window(ft, w);

	if (c != NULL) {
		flag = tty_window_offset(&c->tty, &ox, &oy, &sx, &sy);
		format_add(ft, "window_bigger", "%d", flag);
		if (flag) {
			format_add(ft, "window_offset_x", "%u", ox);
			format_add(ft, "window_offset_y", "%u", oy);
		}
	}

	format_add(ft, "window_index", "%d", wl->idx);
	format_add_cb(ft, "window_stack_index", format_cb_window_stack_index);
	format_add(ft, "window_flags", "%s", window_printable_flags(wl));
	format_add(ft, "window_active", "%d", wl == s->curw);
	format_add_cb(ft, "window_active_sessions",
	    format_cb_window_active_sessions);
	format_add_cb(ft, "window_active_sessions_list",
	    format_cb_window_active_sessions_list);
	format_add_cb(ft, "window_active_clients",
	    format_cb_window_active_clients);
	format_add_cb(ft, "window_active_clients_list",
	    format_cb_window_active_clients_list);

	format_add(ft, "window_start_flag", "%d",
	    !!(wl == RB_MIN(winlinks, &s->windows)));
	format_add(ft, "window_end_flag", "%d",
	    !!(wl == RB_MAX(winlinks, &s->windows)));

	if (server_check_marked() && marked_pane.wl == wl)
	    format_add(ft, "window_marked_flag", "1");
	else
	    format_add(ft, "window_marked_flag", "0");

	format_add(ft, "window_bell_flag", "%d",
	    !!(wl->flags & WINLINK_BELL));
	format_add(ft, "window_activity_flag", "%d",
	    !!(wl->flags & WINLINK_ACTIVITY));
	format_add(ft, "window_silence_flag", "%d",
	    !!(wl->flags & WINLINK_SILENCE));
	format_add(ft, "window_last_flag", "%d",
	    !!(wl == TAILQ_FIRST(&s->lastw)));
	format_add(ft, "window_linked", "%d", session_is_linked(s, wl->window));

	format_add_cb(ft, "window_linked_sessions_list",
	    format_cb_window_linked_sessions_list);
	format_add(ft, "window_linked_sessions", "%u",
	    wl->window->references);
}

/* Set default format keys for a window pane. */
void
format_defaults_pane(struct format_tree *ft, struct window_pane *wp)
{
	struct window			*w = wp->window;
	struct grid			*gd = wp->base.grid;
	int  				 status = wp->status;
	u_int				 idx;
	struct window_mode_entry	*wme;

	if (ft->w == NULL)
		ft->w = w;
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
	if (wp->base.path != NULL)
	    format_add(ft, "pane_path", "%s", wp->base.path);
	format_add(ft, "pane_id", "%%%u", wp->id);
	format_add(ft, "pane_active", "%d", wp == w->active);
	format_add(ft, "pane_input_off", "%d", !!(wp->flags & PANE_INPUTOFF));
	format_add(ft, "pane_pipe", "%d", wp->pipe_fd != -1);

	if ((wp->flags & PANE_STATUSREADY) && WIFEXITED(status))
		format_add(ft, "pane_dead_status", "%d", WEXITSTATUS(status));
	if (~wp->flags & PANE_EMPTY)
		format_add(ft, "pane_dead", "%d", wp->fd == -1);
	else
		format_add(ft, "pane_dead", "0");

	if (server_check_marked() && marked_pane.wp == wp)
		format_add(ft, "pane_marked", "1");
	else
		format_add(ft, "pane_marked", "0");
	format_add(ft, "pane_marked_set", "%d", server_check_marked());

	format_add(ft, "pane_left", "%u", wp->xoff);
	format_add(ft, "pane_top", "%u", wp->yoff);
	format_add(ft, "pane_right", "%u", wp->xoff + wp->sx - 1);
	format_add(ft, "pane_bottom", "%u", wp->yoff + wp->sy - 1);
	format_add(ft, "pane_at_left", "%d", wp->xoff == 0);
	format_add_cb(ft, "pane_at_top", format_cb_pane_at_top);
	format_add(ft, "pane_at_right", "%d", wp->xoff + wp->sx == w->sx);
	format_add_cb(ft, "pane_at_bottom", format_cb_pane_at_bottom);

	wme = TAILQ_FIRST(&wp->modes);
	if (wme != NULL) {
		format_add(ft, "pane_mode", "%s", wme->mode->name);
		if (wme->mode->formats != NULL)
			wme->mode->formats(wme, ft);
	}
	format_add_cb(ft, "pane_in_mode", format_cb_pane_in_mode);

	format_add(ft, "pane_synchronized", "%d",
	    !!options_get_number(w->options, "synchronize-panes"));
	if (wp->searchstr != NULL)
		format_add(ft, "pane_search_string", "%s", wp->searchstr);

	format_add(ft, "pane_tty", "%s", wp->tty);
	format_add(ft, "pane_pid", "%ld", (long) wp->pid);
	format_add_cb(ft, "pane_start_command", format_cb_start_command);
	format_add_cb(ft, "pane_current_command", format_cb_current_command);
	format_add_cb(ft, "pane_current_path", format_cb_current_path);

	format_add(ft, "cursor_x", "%u", wp->base.cx);
	format_add(ft, "cursor_y", "%u", wp->base.cy);
	format_add_cb(ft, "cursor_character", format_cb_cursor_character);

	format_add(ft, "scroll_region_upper", "%u", wp->base.rupper);
	format_add(ft, "scroll_region_lower", "%u", wp->base.rlower);

	format_add(ft, "alternate_on", "%d", wp->base.saved_grid != NULL);
	if (wp->base.saved_cx != UINT_MAX)
		format_add(ft, "alternate_saved_x", "%u", wp->base.saved_cx);
	if (wp->base.saved_cy != UINT_MAX)
		format_add(ft, "alternate_saved_y", "%u", wp->base.saved_cy);

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
	format_add(ft, "origin_flag", "%d",
	    !!(wp->base.mode & MODE_ORIGIN));

	format_add(ft, "mouse_any_flag", "%d",
	    !!(wp->base.mode & ALL_MOUSE_MODES));
	format_add(ft, "mouse_standard_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_STANDARD));
	format_add(ft, "mouse_button_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_BUTTON));
	format_add(ft, "mouse_all_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_ALL));
	format_add(ft, "mouse_utf8_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_UTF8));
	format_add(ft, "mouse_sgr_flag", "%d",
	    !!(wp->base.mode & MODE_MOUSE_SGR));

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
