/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

void	name_time_callback(int, short, void *);
int	name_time_expired(struct window *, struct timeval *);

void
name_time_callback(__unused int fd, __unused short events, void *arg)
{
	struct window	*w = arg;

	/* The event loop will call check_window_name for us on the way out. */
	log_debug("@%u name timer expired", w->id);
}

int
name_time_expired(struct window *w, struct timeval *tv)
{
	struct timeval	offset;

	timersub(tv, &w->name_time, &offset);
	if (offset.tv_sec != 0 || offset.tv_usec > NAME_INTERVAL)
		return (0);
	return (NAME_INTERVAL - offset.tv_usec);
}

void
check_window_name(struct window *w)
{
	struct timeval	 tv, next;
	char		*name;
	int		 left;

	if (w->active == NULL)
		return;

	if (!options_get_number(w->options, "automatic-rename"))
		return;

	if (~w->active->flags & PANE_CHANGED) {
		log_debug("@%u active pane not changed", w->id);
		return;
	}
	log_debug("@%u active pane changed", w->id);

	gettimeofday(&tv, NULL);
	left = name_time_expired(w, &tv);
	if (left != 0) {
		if (!event_initialized(&w->name_event))
			evtimer_set(&w->name_event, name_time_callback, w);
		if (!evtimer_pending(&w->name_event, NULL)) {
			log_debug("@%u name timer queued (%d left)", w->id, left);
			timerclear(&next);
			next.tv_usec = left;
			event_add(&w->name_event, &next);
		} else
			log_debug("@%u name timer already queued (%d left)", w->id, left);
		return;
	}
	memcpy(&w->name_time, &tv, sizeof w->name_time);
	if (event_initialized(&w->name_event))
		evtimer_del(&w->name_event);

	w->active->flags &= ~PANE_CHANGED;

	name = format_window_name(w);
	if (strcmp(name, w->name) != 0) {
		log_debug("@%u new name %s (was %s)", w->id, name, w->name);
		window_set_name(w, name);
		server_status_window(w);
	} else
		log_debug("@%u name not changed (still %s)", w->id, w->name);

	free(name);
}

char *
default_window_name(struct window *w)
{
	char    *cmd, *s;

	cmd = cmd_stringify_argv(w->active->argc, w->active->argv);
	if (cmd != NULL && *cmd != '\0')
		s = parse_window_name(cmd);
	else
		s = parse_window_name(w->active->shell);
	free(cmd);
	return (s);
}

char *
format_window_name(struct window *w)
{
	struct format_tree	*ft;
	char			*fmt, *name;

	ft = format_create(NULL, 0);
	format_defaults_window(ft, w);
	format_defaults_pane(ft, w->active);

	fmt = options_get_string(w->options, "automatic-rename-format");
	name = format_expand(ft, fmt);

	format_free(ft);
	return (name);
}

char *
parse_window_name(const char *in)
{
	char	*copy, *name, *ptr;

	name = copy = xstrdup(in);
	if (strncmp(name, "exec ", (sizeof "exec ") - 1) == 0)
		name = name + (sizeof "exec ") - 1;

	while (*name == ' ' || *name == '-')
		name++;
	if ((ptr = strchr(name, ' ')) != NULL)
		*ptr = '\0';

	if (*name != '\0') {
		ptr = name + strlen(name) - 1;
		while (ptr > name && !isalnum((u_char)*ptr))
			*ptr-- = '\0';
	}

	if (*name == '/')
		name = basename(name);
	name = xstrdup(name);
	free(copy);
	return (name);
}
