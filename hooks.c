/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Thomas Adam <thomas@xteddy.org>
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

struct hooks {
	RB_HEAD(hooks_tree, hook) tree;
	struct hooks	*parent;
};

static int	hooks_cmp(struct hook *, struct hook *);
RB_PROTOTYPE(hooks_tree, hook, entry, hooks_cmp);
RB_GENERATE(hooks_tree, hook, entry, hooks_cmp);

static struct hook	*hooks_find1(struct hooks *, const char *);
static void		 hooks_free1(struct hooks *, struct hook *);
static void		 hooks_emptyfn(struct cmd_q *);

static int
hooks_cmp(struct hook *hook1, struct hook *hook2)
{
	return (strcmp(hook1->name, hook2->name));
}

struct hooks *
hooks_get(struct session *s)
{
	if (s != NULL)
		return (s->hooks);
	return (global_hooks);
}

struct hooks *
hooks_create(struct hooks *parent)
{
	struct hooks	*hooks;

	hooks = xcalloc(1, sizeof *hooks);
	RB_INIT(&hooks->tree);
	hooks->parent = parent;
	return (hooks);
}

static void
hooks_free1(struct hooks *hooks, struct hook *hook)
{
	RB_REMOVE(hooks_tree, &hooks->tree, hook);
	cmd_list_free(hook->cmdlist);
	free((char *)hook->name);
	free(hook);
}

void
hooks_free(struct hooks *hooks)
{
	struct hook	*hook, *hook1;

	RB_FOREACH_SAFE(hook, hooks_tree, &hooks->tree, hook1)
		hooks_free1(hooks, hook);
	free(hooks);
}

struct hook *
hooks_first(struct hooks *hooks)
{
	return (RB_MIN(hooks_tree, &hooks->tree));
}

struct hook *
hooks_next(struct hook *hook)
{
	return (RB_NEXT(hooks_tree, &hooks->tree, hook));
}

void
hooks_add(struct hooks *hooks, const char *name, struct cmd_list *cmdlist)
{
	struct hook	*hook;

	if ((hook = hooks_find1(hooks, name)) != NULL)
		hooks_free1(hooks, hook);

	hook = xcalloc(1, sizeof *hook);
	hook->name = xstrdup(name);
	hook->cmdlist = cmdlist;
	hook->cmdlist->references++;
	RB_INSERT(hooks_tree, &hooks->tree, hook);
}

void
hooks_remove(struct hooks *hooks, const char *name)
{
	struct hook	*hook;

	if ((hook = hooks_find1(hooks, name)) != NULL)
		hooks_free1(hooks, hook);
}

static struct hook *
hooks_find1(struct hooks *hooks, const char *name)
{
	struct hook	hook;

	hook.name = name;
	return (RB_FIND(hooks_tree, &hooks->tree, &hook));
}

struct hook *
hooks_find(struct hooks *hooks, const char *name)
{
	struct hook	 hook0, *hook;

	hook0.name = name;
	hook = RB_FIND(hooks_tree, &hooks->tree, &hook0);
	while (hook == NULL) {
		hooks = hooks->parent;
		if (hooks == NULL)
			break;
		hook = RB_FIND(hooks_tree, &hooks->tree, &hook0);
	}
	return (hook);
}

static void
hooks_emptyfn(struct cmd_q *hooks_cmdq)
{
	struct cmd_q	*cmdq = hooks_cmdq->data;

	if (cmdq != NULL) {
		if (hooks_cmdq->client_exit >= 0)
			cmdq->client_exit = hooks_cmdq->client_exit;
		if (!cmdq_free(cmdq))
			cmdq_continue(cmdq);
	}
	cmdq_free(hooks_cmdq);
}

int
hooks_run(struct hooks *hooks, struct client *c, struct cmd_find_state *fs,
    const char *fmt, ...)
{
	struct hook	*hook;
	struct cmd_q	*hooks_cmdq;
	va_list		 ap;
	char		*name;

	va_start(ap, fmt);
	xvasprintf(&name, fmt, ap);
	va_end(ap);

	hook = hooks_find(hooks, name);
	if (hook == NULL) {
		free(name);
		return (-1);
	}
	log_debug("running hook %s", name);
	free(name);

	hooks_cmdq = cmdq_new(c);
	hooks_cmdq->flags |= CMD_Q_NOHOOKS;

	if (fs != NULL)
		cmd_find_copy_state(&hooks_cmdq->current, fs);
	hooks_cmdq->parent = NULL;

	cmdq_run(hooks_cmdq, hook->cmdlist, NULL);
	cmdq_free(hooks_cmdq);
	return (0);
}

int
hooks_wait(struct hooks *hooks, struct cmd_q *cmdq, struct cmd_find_state *fs,
    const char *fmt, ...)
{
	struct hook	*hook;
	struct cmd_q	*hooks_cmdq;
	va_list		 ap;
	char		*name;

	va_start(ap, fmt);
	xvasprintf(&name, fmt, ap);
	va_end(ap);

	hook = hooks_find(hooks, name);
	if (hook == NULL) {
		free(name);
		return (-1);
	}
	log_debug("running hook %s (parent %p)", name, cmdq);
	free(name);

	hooks_cmdq = cmdq_new(cmdq->client);
	hooks_cmdq->flags |= CMD_Q_NOHOOKS;

	if (fs != NULL)
		cmd_find_copy_state(&hooks_cmdq->current, fs);
	hooks_cmdq->parent = cmdq;

	hooks_cmdq->emptyfn = hooks_emptyfn;
	hooks_cmdq->data = cmdq;

	if (cmdq != NULL)
		cmdq->references++;
	cmdq_run(hooks_cmdq, hook->cmdlist, NULL);
	return (0);
}
