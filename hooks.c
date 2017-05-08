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
        struct hooksTree_tree tree;
	struct hooks	*parent;
};

static int	hooks_cmp(struct hook *, struct hook *);

static struct hook *get_hook(struct hooksTree_head *head)
{
        return (struct hook *)((char *) head - offsetof(struct hook, entry));
}

static struct hooksTree_head *get_hook_entry(struct hook *hook)
{
        return &hook->entry;
}

RB3_GEN_INLINE_PROTO(hooksTree, struct hook, get_hook_entry, get_hook);
RB3_GEN_INLINE(hooksTree, struct hook, get_hook_entry, get_hook);
RB3_GEN_NODECMP_PROTO(hooksTree, /* no suffix */, struct hook, get_hook_entry, get_hook, hooks_cmp);
RB3_GEN_NODECMP(hooksTree, /* no suffix */, struct hook, get_hook_entry, get_hook, hooks_cmp);

static struct hook	*hooks_find1(struct hooks *, const char *);
static void		 hooks_free1(struct hooks *, struct hook *);

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
	hooksTree_init(&hooks->tree);
	hooks->parent = parent;
	return (hooks);
}

static void
hooks_free1(struct hooks *hooks, struct hook *hook)
{
	hooksTree_delete(hook, &hooks->tree);
	cmd_list_free(hook->cmdlist);
	free((char *)hook->name);
	free(hook);
}

void
hooks_free(struct hooks *hooks)
{
	struct hook	*hook, *hook1;

	RB3_FOREACH_SAFE(hooksTree, &hooks->tree, hook, hook1)
		hooks_free1(hooks, hook);
	free(hooks);
}

struct hook *
hooks_first(struct hooks *hooks)
{
	return hooksTree_get_min(&hooks->tree);
}

struct hook *
hooks_next(struct hook *hook)
{
	return hooksTree_get_next(hook);
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
	hooksTree_insert(hook, &hooks->tree);
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
	return (hooksTree_find(&hooks->tree, &hook));
}

struct hook *
hooks_find(struct hooks *hooks, const char *name)
{
	struct hook	 hook0, *hook;

	hook0.name = name;
	hook = hooksTree_find(&hooks->tree, &hook0);
	while (hook == NULL) {
		hooks = hooks->parent;
		if (hooks == NULL)
			break;
		hook = hooksTree_find(&hooks->tree, &hook0);
	}
	return (hook);
}

void
hooks_run(struct hooks *hooks, struct client *c, struct cmd_find_state *fs,
    const char *fmt, ...)
{
	struct hook		*hook;
	va_list			 ap;
	char			*name;
	struct cmdq_item	*new_item;

	va_start(ap, fmt);
	xvasprintf(&name, fmt, ap);
	va_end(ap);

	hook = hooks_find(hooks, name);
	if (hook == NULL) {
		free(name);
		return;
	}
	log_debug("running hook %s", name);

	new_item = cmdq_get_command(hook->cmdlist, fs, NULL, CMDQ_NOHOOKS);
	cmdq_format(new_item, "hook", "%s", name);
	cmdq_append(c, new_item);

	free(name);
}

void
hooks_insert(struct hooks *hooks, struct cmdq_item *item,
    struct cmd_find_state *fs, const char *fmt, ...)
{
	struct hook		*hook;
	va_list			 ap;
	char			*name;
	struct cmdq_item	*new_item;

	if (item->flags & CMDQ_NOHOOKS)
		return;

	va_start(ap, fmt);
	xvasprintf(&name, fmt, ap);
	va_end(ap);

	hook = hooks_find(hooks, name);
	if (hook == NULL) {
		free(name);
		return;
	}
	log_debug("running hook %s (parent %p)", name, item);

	new_item = cmdq_get_command(hook->cmdlist, fs, NULL, CMDQ_NOHOOKS);
	cmdq_format(new_item, "hook", "%s", name);
	if (item != NULL)
		cmdq_insert_after(item, new_item);
	else
		cmdq_append(NULL, new_item);

	free(name);
}
