/* $OpenBSD: cmd-wait-for.c,v 1.23 2026/07/10 13:38:45 nicm Exp $ */

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

	.args = { "EF:LSUlvw:", 1, 1, NULL },
	.usage = "[-ELSUlv] [-F format] [-w waiter] name",

	.flags = 0,
	.exec = cmd_wait_for_exec
};

struct wait_item {
	struct cmdq_item	*item;
	TAILQ_ENTRY(wait_item)	 entry;
};

struct wait_event_item {
	struct cmdq_item		*item;
	struct events_sink		*sink;
	char				*name;
	char				*filter;
	int				 verbose;
	TAILQ_ENTRY(wait_event_item)	 entry;
};
static TAILQ_HEAD(, wait_event_item) wait_event_items =
    TAILQ_HEAD_INITIALIZER(wait_event_items);

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
static enum cmd_retval	cmd_wait_for_event(struct cmdq_item *, const char *,
			    struct args *);
static void		cmd_wait_for_event_cb(const char *,
			    struct event_payload *, void *);
static void		cmd_wait_for_event_free(struct wait_event_item *);
static enum cmd_retval	cmd_wait_for_event_list(struct cmdq_item *,
			    const char *);
static enum cmd_retval	cmd_wait_for_event_wake(struct cmdq_item *,
			    const char *, struct args *);
static enum cmd_retval	cmd_wait_for_list(struct cmdq_item *,
			    struct wait_channel *);
static enum cmd_retval	cmd_wait_for_wake(struct cmdq_item *, const char *,
			    struct args *, struct wait_channel *);

static struct wait_channel	*cmd_wait_for_add(const char *);
static void			 cmd_wait_for_remove(struct wait_channel *);
static void			 cmd_wait_for_remove_empty(
				    struct wait_channel *);
static const char		*cmd_wait_for_item_client_name(
				    struct cmdq_item *);
static const char		*cmd_wait_for_client_name(
				    struct wait_event_item *);

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

static void
cmd_wait_for_remove_empty(struct wait_channel *wc)
{
	if (wc->locked || wc->woken)
		return;
	if (!TAILQ_EMPTY(&wc->waiters) || !TAILQ_EMPTY(&wc->lockers))
		return;

	log_debug("remove empty wait channel %s", wc->name);

	RB_REMOVE(wait_channels, &wait_channels, wc);

	free((void *)wc->name);
	free(wc);
}

static const char *
cmd_wait_for_item_client_name(struct cmdq_item *item)
{
	struct client	*c = cmdq_get_client(item);

	if (c == NULL || c->name == NULL)
		return ("");
	return (c->name);
}

static const char *
cmd_wait_for_client_name(struct wait_event_item *wei)
{
	return (cmd_wait_for_item_client_name(wei->item));
}

static enum cmd_retval
cmd_wait_for_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	const char		*name = args_string(args, 0);
	struct wait_channel	*wc, find = { .name = name };

	if (args_has(args, 'E'))
		return (cmd_wait_for_event(item, name, args));

	wc = RB_FIND(wait_channels, &wait_channels, &find);
	if (args_has(args, 'l'))
		return (cmd_wait_for_list(item, wc));
	if (args_has(args, 'w'))
		return (cmd_wait_for_wake(item, name, args, wc));
	if (args_has(args, 'S'))
		return (cmd_wait_for_signal(item, name, wc));
	if (args_has(args, 'L'))
		return (cmd_wait_for_lock(item, name, wc));
	if (args_has(args, 'U'))
		return (cmd_wait_for_unlock(item, name, wc));
	return (cmd_wait_for_wait(item, name, wc));
}

static void
cmd_wait_for_event_print(struct wait_event_item *wei, struct event_payload *ep)
{
	struct event_payload_item	*epi;
	const char			*key;
	char				*value;

	epi = event_payload_first(ep);
	while (epi != NULL) {
		key = event_payload_item_name(epi);
		if (*key != '_') {
			value = event_payload_item_print(epi);
			cmdq_print(wei->item, "%s=%s", key, value);
			free(value);
		}
		epi = event_payload_next(epi);
	}
}

static void
cmd_wait_for_event_cb(__unused const char *name, struct event_payload *ep,
    void *item_data)
{
	struct wait_event_item	*wei = item_data;
	struct format_tree	*ft;
	char			*expanded;
	int			 flag;

	if (wei->verbose)
		cmd_wait_for_event_print(wei, ep);

	if (wei->filter != NULL) {
		ft = format_create(cmdq_get_client(wei->item), wei->item,
		    FORMAT_NONE, FORMAT_NOJOBS);
		event_payload_add_formats(ep, ft, NULL);
		expanded = format_expand(ft, wei->filter);
		flag = format_true(expanded);
		free(expanded);
		format_free(ft);

		if (!flag)
			return;
	}

	TAILQ_REMOVE(&wait_event_items, wei, entry);
	cmdq_continue(wei->item);
	cmd_wait_for_event_free(wei);
}

static void
cmd_wait_for_event_free(struct wait_event_item *wei)
{
	events_remove_sink(wei->sink);
	free(wei->name);
	free(wei->filter);
	free(wei);
}

static enum cmd_retval
cmd_wait_for_event(struct cmdq_item *item, const char *name, struct args *args)
{
	struct wait_event_item	*wei;
	const char		*filter = args_get(args, 'F');

	if (!hooks_valid_event_name(name)) {
		cmdq_error(item, "invalid event: %s", name);
		return (CMD_RETURN_ERROR);
	}
	if (args_has(args, 'l'))
		return (cmd_wait_for_event_list(item, name));
	if (args_has(args, 'w'))
		return (cmd_wait_for_event_wake(item, name, args));

	if (cmdq_get_client(item) == NULL) {
		cmdq_error(item, "not able to wait");
		return (CMD_RETURN_ERROR);
	}

	wei = xcalloc(1, sizeof *wei);
	wei->item = item;
	wei->name = xstrdup(name);
	wei->filter = (filter != NULL ? xstrdup(filter) : NULL);
	wei->verbose = args_has(args, 'v');
	wei->sink = events_add_sink(name, cmd_wait_for_event_cb, wei);
	TAILQ_INSERT_TAIL(&wait_event_items, wei, entry);

	return (CMD_RETURN_WAIT);
}

static enum cmd_retval
cmd_wait_for_event_list(struct cmdq_item *item, const char *name)
{
	struct wait_event_item	*wei;

	TAILQ_FOREACH(wei, &wait_event_items, entry) {
		if (strcmp(wei->name, name) == 0)
			cmdq_print(item, "%s", cmd_wait_for_client_name(wei));
	}

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_wait_for_event_wake(struct cmdq_item *item, const char *name,
    struct args *args)
{
	struct wait_event_item	*wei, *wei1;
	const char		*client_name = args_get(args, 'w');

	TAILQ_FOREACH_SAFE(wei, &wait_event_items, entry, wei1) {
		if (strcmp(wei->name, name) != 0)
			continue;
		if (strcmp(cmd_wait_for_client_name(wei), client_name) != 0)
			continue;

		TAILQ_REMOVE(&wait_event_items, wei, entry);
		cmdq_continue(wei->item);
		cmd_wait_for_event_free(wei);
		return (CMD_RETURN_NORMAL);
	}

	cmdq_error(item, "waiter %s not found", client_name);
	return (CMD_RETURN_ERROR);
}

static enum cmd_retval
cmd_wait_for_list(struct cmdq_item *item, struct wait_channel *wc)
{
	struct wait_item	*wi;

	if (wc == NULL)
		return (CMD_RETURN_NORMAL);

	TAILQ_FOREACH(wi, &wc->waiters, entry)
		cmdq_print(item, "%s", cmd_wait_for_item_client_name(wi->item));
	TAILQ_FOREACH(wi, &wc->lockers, entry)
		cmdq_print(item, "%s", cmd_wait_for_item_client_name(wi->item));

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_wait_for_wake(__unused struct cmdq_item *item, const char *name,
    struct args *args, struct wait_channel *wc)
{
	struct wait_item	*wi, *wi1;
	const char		*client_name = args_get(args, 'w');

	if (wc != NULL) {
		TAILQ_FOREACH_SAFE(wi, &wc->waiters, entry, wi1) {
			name = cmd_wait_for_item_client_name(wi->item);
			if (strcmp(name, client_name) != 0)
				continue;
			cmdq_continue(wi->item);
			TAILQ_REMOVE(&wc->waiters, wi, entry);
			free(wi);
			cmd_wait_for_remove_empty(wc);
			return (CMD_RETURN_NORMAL);
		}
		TAILQ_FOREACH_SAFE(wi, &wc->lockers, entry, wi1) {
			name = cmd_wait_for_item_client_name(wi->item);
			if (strcmp(name, client_name) != 0)
				continue;
			cmdq_continue(wi->item);
			TAILQ_REMOVE(&wc->lockers, wi, entry);
			free(wi);
			cmd_wait_for_remove_empty(wc);
			return (CMD_RETURN_NORMAL);
		}
	}
	return (CMD_RETURN_NORMAL);
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
	struct wait_event_item	*wei, *wei1;

	TAILQ_FOREACH_SAFE(wei, &wait_event_items, entry, wei1) {
		TAILQ_REMOVE(&wait_event_items, wei, entry);
		cmdq_continue(wei->item);
		cmd_wait_for_event_free(wei);
	}

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
