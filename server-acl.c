/* $OpenBSD: server-acl.c,v 1.3 2026/06/08 21:38:19 nicm Exp $ */

/*
 * Copyright (c) 2021 Holland Schutte, Jayson Morberg
 * Copyright (c) 2021 Dallas Lyons <dallasdlyons@gmail.com>
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
#include <sys/stat.h>
#include <sys/socket.h>

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

struct server_acl_entry {
	id_t				 id;

	int				 flags;

	RB_ENTRY(server_acl_entry)	 entry;
};

static int
server_acl_cmp(struct server_acl_entry *entry1,
    struct server_acl_entry *entry2)
{
	if ((entry1->flags ^ entry2->flags) & SERVER_ACL_IS_GROUP) {
		if (entry1->flags & SERVER_ACL_IS_GROUP)
			return (1);
		return (-1);
	}

	if (entry1->id < entry2->id)
		return (-1);
	return (entry1->id > entry2->id);
}

RB_HEAD(server_acl_entries, server_acl_entry) server_acl_entries;
RB_GENERATE_STATIC(server_acl_entries, server_acl_entry, entry, server_acl_cmp);

static struct server_acl_entry *
server_acl_entry_find(id_t id, int flags)
{
	struct server_acl_entry	find = {
		.id = id,
		.flags = flags & SERVER_ACL_IS_GROUP
	};

	return (RB_FIND(server_acl_entries, &server_acl_entries, &find));
}

static struct server_acl_entry *
server_acl_check(struct client *c)
{
	struct server_acl_entry	*entry;
	uid_t			 uid;
	gid_t			 gid;

	uid = proc_get_peer_uid(c->peer);
	if (uid == (uid_t)-1)
		return (NULL);

	entry = server_acl_entry_find(uid, 0);
	if (entry != NULL)
		return (entry);

	gid = proc_get_peer_gid(c->peer);
	if (gid == (gid_t)-1)
		return (NULL);

	return (server_acl_entry_find(gid, SERVER_ACL_IS_GROUP));
}

static void
server_acl_update(void)
{
	struct server_acl_entry	*entry;
	struct client		*c;

	TAILQ_FOREACH(c, &clients, entry) {
		entry = server_acl_check(c);
		if (entry == NULL) {
			c->exit_message = xstrdup("access not allowed");
			c->flags |= CLIENT_EXIT;
		} else if (entry->flags & SERVER_ACL_READONLY)
			c->flags |= CLIENT_READONLY;
		else
			c->flags &= ~CLIENT_READONLY;
	}
}

/* Initialize ACL tree. */
void
server_acl_init(void)
{
	RB_INIT(&server_acl_entries);

	if (getuid() != 0)
		server_acl_allow(0, 0);
	server_acl_allow(getuid(), 0);
}

/* Check if an ACL entry exists. */
int
server_acl_find(id_t id, int flags)
{
	return (server_acl_entry_find(id, flags) != NULL);
}

/* Display the tree. */
void
server_acl_display(struct cmdq_item *item)
{
	struct server_acl_entry	*loop;
	struct passwd		*pw;
	struct group		*gr;
	const char		*name;
	char			 type;

	RB_FOREACH(loop, server_acl_entries, &server_acl_entries) {
		if (~loop->flags & SERVER_ACL_IS_GROUP) {
			if (loop->id == 0)
				continue;
			if ((pw = getpwuid(loop->id)) != NULL)
				name = pw->pw_name;
			else
				name = "unknown";
			type = 'U';
		} else {
			if ((gr = getgrgid(loop->id)) != NULL)
				name = gr->gr_name;
			else
				name = "unknown";
			type = 'G';
		}
		if (loop->flags & SERVER_ACL_READONLY)
			cmdq_print(item, "%s (%c,R)", name, type);
		else
			cmdq_print(item, "%s (%c,W)", name, type);
	}
}

/* Allow an ACL entry. */
void
server_acl_allow(id_t id, int flags)
{
	struct server_acl_entry	*entry;

	entry = server_acl_entry_find(id, flags);
	if (entry == NULL) {
		entry = xcalloc(1, sizeof *entry);
		entry->id = id;
		entry->flags = flags & SERVER_ACL_IS_GROUP;
		RB_INSERT(server_acl_entries, &server_acl_entries, entry);
	}
}

/* Deny an ACL entry (remove it from the tree). */
void
server_acl_deny(id_t id, int flags)
{
	struct server_acl_entry	*entry;

	entry = server_acl_entry_find(id, flags);
	if (entry != NULL) {
		RB_REMOVE(server_acl_entries, &server_acl_entries, entry);
		free(entry);
		server_acl_update();
	}
}

/* Allow this ACL entry write access. */
void
server_acl_allow_write(id_t id, int flags)
{
	struct server_acl_entry	*entry;

	entry = server_acl_entry_find(id, flags);
	if (entry == NULL)
		return;
	entry->flags &= ~SERVER_ACL_READONLY;
	server_acl_update();
}

/* Deny this ACL entry write access. */
void
server_acl_deny_write(id_t id, int flags)
{
	struct server_acl_entry	*entry;

	entry = server_acl_entry_find(id, flags);
	if (entry == NULL)
		return;
	entry->flags |= SERVER_ACL_READONLY;
	server_acl_update();
}

/*
 * Check if the client's UID or GID exists in the ACL list and if so, set as
 * read only if needed. UID entries take precedence over GID entries. Return
 * false if no entry exists.
 */
int
server_acl_join(struct client *c)
{
	struct server_acl_entry	*entry;

	entry = server_acl_check(c);
	if (entry == NULL)
		return (0);
	if (entry->flags & SERVER_ACL_READONLY)
		c->flags |= CLIENT_READONLY;
	return (1);
}
