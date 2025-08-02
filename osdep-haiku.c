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

#include <unistd.h>
#include <kernel/OS.h>

#include "tmux.h"

char *
osdep_get_name(int fd, __unused char *tty)
{
	pid_t		tid;
	team_info	tinfo;

	if ((tid = tcgetpgrp(fd)) == -1)
		return (NULL);

	if (get_team_info(tid, &tinfo) != B_OK)
		return (NULL);

	/* Up to the first 64 characters. */
	return (xstrdup(tinfo.args));
}

char *
osdep_get_cwd(__unused int fd)
{
	return (NULL);
}

struct event_base *
osdep_event_init(void)
{
	return (event_init());
}
