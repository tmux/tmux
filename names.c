/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <string.h>
#include <unistd.h>

#include "tmux.h"

char	*parse_window_name(const char *);

void
set_window_names(void)
{
	struct window	*w;
	u_int		 i;
	char		*name, *wname;
	struct timeval	 tv, tv2;

	if (gettimeofday(&tv, NULL) != 0)
		fatal("gettimeofday");

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL || w->active == NULL)
			continue;
		if (!options_get_number(&w->options, "automatic-rename"))
			continue;

		if (timercmp(&tv, &w->name_timer, <))
			continue;
		memcpy(&w->name_timer, &tv, sizeof w->name_timer);
		tv2.tv_sec = 0;
		tv2.tv_usec = NAME_INTERVAL * 1000L;
		timeradd(&w->name_timer, &tv2, &w->name_timer);

		if (w->active->screen != &w->active->base)
			name = NULL;
		else
			name = get_proc_name(w->active->fd, w->active->tty);
		if (name == NULL)
			wname = default_window_name(w);
		else {
			wname = parse_window_name(name);
			xfree(name);
		}

		if (strcmp(wname, w->name) == 0)
			xfree(wname);
		else {
			xfree(w->name);
			w->name = wname;
			server_status_window(w);
		}
	}
}

char *
default_window_name(struct window *w)
{
	if (w->active->screen != &w->active->base)
		return (xstrdup("[tmux]"));
	return (parse_window_name(w->active->cmd));
}

char *
parse_window_name(const char *in)
{
	char	*copy, *name, *ptr;

	name = copy = xstrdup(in);
	if (strncmp(name, "exec ", (sizeof "exec ") - 1) == 0)
		name = name + (sizeof "exec ") - 1;

	while (*name == ' ')
		name++;
	if ((ptr = strchr(name, ' ')) != NULL)
		*ptr = '\0';

	if (*name != '\0') {
		ptr = name + strlen(name) - 1;
		while (ptr > name && !isalnum(*ptr))
			*ptr-- = '\0';
	}

	if (*name == '/')
		name = basename(name);
	name = xstrdup(name);
	xfree(copy);
	return (name);
}

