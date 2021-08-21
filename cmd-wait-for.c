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

static enum cmd_retval cmd_wait_for_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_wait_for_entry = {
	.name = "wait-for",
	.alias = "wait",

	.args = { "LSU", 1, 1, NULL },
	.usage = "[-L|-S|-U] channel",

	.flags = 0,
	.exec = cmd_wait_for_exec
};

struct wait_item {
	struct cmdq_item	*item;
	TAILQ_ENTRY(wait_item)	 entry;
};

struct wait_channel {
	const char	       *name;
	int			locked;
	int			woken;

	TAILQ_HEAD(, wait_item)	waiters;
	TAILQ_HEAD(, wait_item)	lockers;

	RB_ENTRY(wait_channel)	entry;
};
RB_HEAD(wait_channels, wait_channel);
static struct wait_channels wait_channels = RB_INITIALIZER(wait_channels);

static int wait_channel_cmp(struct wait_channel *, struct wait_channel *);
RB_GENERATE_STATIC(wait_channels, wait_channel, entry, wait_channel_cmp);

static int
wait_channel_cmp(struct wait_channel *wc1, struct wait_channel *wc2)
{
	return (strcmp(wc1->name, wc2->name));
}

static enum cmd_retval	cmd_wait_for_signal(struct cmdq_item *, const char *,
			    struct wait_channel *);
static enum cmd_retval	cmd_wait_for_wait(struct cmdq_item *, const char *,
			    struct wait_channel *);
static enum cmd_retval	cmd_wait_for_lock(struct cmdq_item *, const char *,
			    struct wait_channel *);
static enum cmd_retval	cmd_wait_for_unlock(struct cmdq_item *, const char *,
			    struct wait_channel *);

static struct wait_channel	*cmd_wait_for_add(const char *);
static void			 cmd_wait_for_remove(struct wait_channel *);

static struct wait_channel *
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

static void
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

static enum cmd_retval
cmd_wait_for_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args     	*args = cmd_get_args(self);
	const char		*name = args_string(args, 0);
	struct wait_channel	*wc, find;

	find.name = name;
	wc = RB_FIND(wait_channels, &wait_channels, &find);

	if (args_has(args, 'S'))
		return (cmd_wait_for_signal(item, name, wc));
	if (args_has(args, 'L'))
		return (cmd_wait_for_lock(item, name, wc));
	if (args_has(args, 'U'))
		return (cmd_wait_for_unlock(item, name, wc));
	return (cmd_wait_for_wait(item, name, wc));
}

static enum cmd_retval
cmd_wait_for_signal(__unused struct cmdq_item *item, const char *name,
    struct wait_channel *wc)
{
	struct wait_item	*wi, *wi1;

	if (wc == NULL)
		wc = cmd_wait_for_add(name);

	if (TAILQ_EMPTY(&wc->waiters) && !wc->woken) {
		log_debug("signal wait channel %s, no waiters", wc->name);
		wc->woken = 1;
		return (CMD_RETURN_NORMAL);
	}
	log_debug("signal wait channel %s, with waiters", wc->name);

	TAILQ_FOREACH_SAFE(wi, &wc->waiters, entry, wi1) {
		cmdq_continue(wi->item);

		TAILQ_REMOVE(&wc->waiters, wi, entry);
		free(wi);
	}

	cmd_wait_for_remove(wc);
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_wait_for_wait(struct cmdq_item *item, const char *name,
    struct wait_channel *wc)
{
	struct client		*c = cmdq_get_client(item);
	struct wait_item	*wi;

	if (c == NULL) {
		cmdq_error(item, "not able to wait");
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

	wi = xcalloc(1, sizeof *wi);
	wi->item = item;
	TAILQ_INSERT_TAIL(&wc->waiters, wi, entry);

	return (CMD_RETURN_WAIT);
}

static enum cmd_retval
cmd_wait_for_lock(struct cmdq_item *item, const char *name,
    struct wait_channel *wc)
{
	struct wait_item	*wi;

	if (cmdq_get_client(item) == NULL) {
		cmdq_error(item, "not able to lock");
		return (CMD_RETURN_ERROR);
	}

	if (wc == NULL)
		wc = cmd_wait_for_add(name);

	if (wc->locked) {
		wi = xcalloc(1, sizeof *wi);
		wi->item = item;
		TAILQ_INSERT_TAIL(&wc->lockers, wi, entry);
		return (CMD_RETURN_WAIT);
	}
	wc->locked = 1;

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_wait_for_unlock(struct cmdq_item *item, const char *name,
    struct wait_channel *wc)
{
	struct wait_item	*wi;

	if (wc == NULL || !wc->locked) {
		cmdq_error(item, "channel %s not locked", name);
		return (CMD_RETURN_ERROR);
	}

	if ((wi = TAILQ_FIRST(&wc->lockers)) != NULL) {
		cmdq_continue(wi->item);
		TAILQ_REMOVE(&wc->lockers, wi, entry);
		free(wi);
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
	struct wait_item	*wi, *wi1;

	RB_FOREACH_SAFE(wc, wait_channels, &wait_channels, wc1) {
		TAILQ_FOREACH_SAFE(wi, &wc->waiters, entry, wi1) {
			cmdq_continue(wi->item);
			TAILQ_REMOVE(&wc->waiters, wi, entry);
			free(wi);
		}
		wc->woken = 1;
		TAILQ_FOREACH_SAFE(wi, &wc->lockers, entry, wi1) {
			cmdq_continue(wi->item);
			TAILQ_REMOVE(&wc->lockers, wi, entry);
			free(wi);
		}
		wc->locked = 0;
		cmd_wait_for_remove(wc);
	}
}
