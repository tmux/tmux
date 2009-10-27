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

#include <unistd.h>

#include "tmux.h"

/* Register jobs for poll. */
void
server_job_prepare(void)
{
	struct job	*job;

	SLIST_FOREACH(job, &all_jobs, lentry) {
		if (job->fd == -1)
			continue;
		server_poll_add(job->fd, POLLIN, server_job_callback, job);
	}
}

/* Process a single job event. */
void
server_job_callback(int fd, int events, void *data)
{
	struct job	*job = data;

	if (job->fd == -1)
		return;

	if (buffer_poll(fd, events, job->out, NULL) != 0) {
		close(job->fd);
		job->fd = -1;
	}
}

/* Job functions that happen once a loop. */
void
server_job_loop(void)
{
	struct job	*job;
	
restart:
	SLIST_FOREACH(job, &all_jobs, lentry) {
		if (job->flags & JOB_DONE || job->fd != -1 || job->pid != -1)
			continue;
		job->flags |= JOB_DONE;

		if (job->callbackfn != NULL) {
			job->callbackfn(job);
			goto restart;	/* could be freed by callback */
		}
	}
}
