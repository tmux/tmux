/* $OpenBSD$ */

/*
 * Copyright (c) 2013 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2013 Thiago de Arruda <tpadilha84@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Block or wake a client on a named wait channel.
 */

enum cmd_retval cmd_wait_for_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_wait_for_entry = {
	.name = "wait-for",
	.alias = "wait",

	.args = { "LSU", 1, 1 },
	.usage = "[-L|-S|-U] channel",

	.flags = 0,
	.exec = cmd_wait_for_exec
};

struct wait_channel {
	const char	       *name;
	int			locked;
	int			woken;

	TAILQ_HEAD(, cmd_q)	waiters;
	TAILQ_HEAD(, cmd_q)	lockers;

	RB_ENTRY(wait_channel)	entry;
};
RB_HEAD(wait_channels, wait_channel);
struct wait_channels wait_channels = RB_INITIALIZER(wait_channels);

int	wait_channel_cmp(struct wait_channel *, struct wait_channel *);
RB_PROTOTYPE(wait_channels, wait_channel, entry, wait_channel_cmp);
RB_GENERATE(wait_channels, wait_channel, entry, wait_channel_cmp);

int
wait_channel_cmp(struct wait_channel *wc1, struct wait_channel *wc2)
{
	return (strcmp(wc1->name, wc2->name));
}

enum cmd_retval	cmd_wait_for_signal(struct cmd_q *, const char *,
		    struct wait_channel *);
enum cmd_retval	cmd_wait_for_wait(struct cmd_q *, const char *,
		    struct wait_channel *);
enum cmd_retval	cmd_wait_for_lock(struct cmd_q *, const char *,
		    struct wait_channel *);
enum cmd_retval	cmd_wait_for_unlock(struct cmd_q *, const char *,
		    struct wait_channel *);

struct wait_channel	*cmd_wait_for_add(const char *);
void			 cmd_wait_for_remove(struct wait_channel *wc);

struct wait_channel *
cmd_wait_for_add(const char *name)
{
	struct wait_channel *wc;

	wc = xmalloc(sizeof *wc);
	wc->name = xstrdup(name);

	wc->locked = 0;
	wc->woken = 0;

	TAILQ_INIT(&wc->waiters);
	TAILQ_INIT(&wc->lockers);

	RB_INSERT(wait_channels, &wait_channels, wc);

	log_debug("add wait channel %s", wc->name);

	return (wc);
}

void
cmd_wait_for_remove(struct wait_channel *wc)
{
	if (wc->locked)
		return;
	if (!TAILQ_EMPTY(&wc->waiters) || !wc->woken)
		return;

	log_debug("remove wait channel %s", wc->name);

	RB_REMOVE(wait_channels, &wait_channels, wc);

	free((void *)wc->name);
	free(wc);
}

enum cmd_retval
cmd_wait_for_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args     	*args = self->args;
	const char		*name = args->argv[0];
	struct wait_channel	*wc, wc0;

	wc0.name = name;
	wc = RB_FIND(wait_channels, &wait_channels, &wc0);

	if (args_has(args, 'S'))
		return (cmd_wait_for_signal(cmdq, name, wc));
	if (args_has(args, 'L'))
		return (cmd_wait_for_lock(cmdq, name, wc));
	if (args_has(args, 'U'))
		return (cmd_wait_for_unlock(cmdq, name, wc));
	return (cmd_wait_for_wait(cmdq, name, wc));
}

enum cmd_retval
cmd_wait_for_signal(__unused struct cmd_q *cmdq, const char *name,
    struct wait_channel *wc)
{
	struct cmd_q	*wq, *wq1;

	if (wc == NULL)
		wc = cmd_wait_for_add(name);

	if (TAILQ_EMPTY(&wc->waiters) && !wc->woken) {
		log_debug("signal wait channel %s, no waiters", wc->name);
		wc->woken = 1;
		return (CMD_RETURN_NORMAL);
	}
	log_debug("signal wait channel %s, with waiters", wc->name);

	TAILQ_FOREACH_SAFE(wq, &wc->waiters, waitentry, wq1) {
		TAILQ_REMOVE(&wc->waiters, wq, waitentry);
		if (!cmdq_free(wq))
			cmdq_continue(wq);
	}

	cmd_wait_for_remove(wc);
	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_wait_for_wait(struct cmd_q *cmdq, const char *name,
    struct wait_channel *wc)
{
	struct client	*c = cmdq->client;

	if (c == NULL || c->session != NULL) {
		cmdq_error(cmdq, "not able to wait");
		return (CMD_RETURN_ERROR);
	}

	if (wc == NULL)
		wc = cmd_wait_for_add(name);

	if (wc->woken) {
		log_debug("wait channel %s already woken (%p)", wc->name, c);
		cmd_wait_for_remove(wc);
		return (CMD_RETURN_NORMAL);
	}
	log_debug("wait channel %s not woken (%p)", wc->name, c);

	TAILQ_INSERT_TAIL(&wc->waiters, cmdq, waitentry);
	cmdq->references++;

	return (CMD_RETURN_WAIT);
}

enum cmd_retval
cmd_wait_for_lock(struct cmd_q *cmdq, const char *name,
    struct wait_channel *wc)
{
	if (cmdq->client == NULL || cmdq->client->session != NULL) {
		cmdq_error(cmdq, "not able to lock");
		return (CMD_RETURN_ERROR);
	}

	if (wc == NULL)
		wc = cmd_wait_for_add(name);

	if (wc->locked) {
		TAILQ_INSERT_TAIL(&wc->lockers, cmdq, waitentry);
		cmdq->references++;
		return (CMD_RETURN_WAIT);
	}
	wc->locked = 1;

	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_wait_for_unlock(struct cmd_q *cmdq, const char *name,
    struct wait_channel *wc)
{
	struct cmd_q	*wq;

	if (wc == NULL || !wc->locked) {
		cmdq_error(cmdq, "channel %s not locked", name);
		return (CMD_RETURN_ERROR);
	}

	if ((wq = TAILQ_FIRST(&wc->lockers)) != NULL) {
		TAILQ_REMOVE(&wc->lockers, wq, waitentry);
		if (!cmdq_free(wq))
			cmdq_continue(wq);
	} else {
		wc->locked = 0;
		cmd_wait_for_remove(wc);
	}

	return (CMD_RETURN_NORMAL);
}

void
cmd_wait_for_flush(void)
{
	struct wait_channel	*wc, *wc1;
	struct cmd_q		*wq, *wq1;

	RB_FOREACH_SAFE(wc, wait_channels, &wait_channels, wc1) {
		TAILQ_FOREACH_SAFE(wq, &wc->waiters, waitentry, wq1) {
			TAILQ_REMOVE(&wc->waiters, wq, waitentry);
			if (!cmdq_free(wq))
				cmdq_continue(wq);
		}
		wc->woken = 1;
		TAILQ_FOREACH_SAFE(wq, &wc->lockers, waitentry, wq1) {
			TAILQ_REMOVE(&wc->lockers, wq, waitentry);
			if (!cmdq_free(wq))
				cmdq_continue(wq);
		}
		wc->locked = 0;
		cmd_wait_for_remove(wc);
	}
}
