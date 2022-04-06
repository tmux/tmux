/* $OpenBSD$ */

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
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

struct server_acl_user {
	uid_t				uid;

	int				flags;
#define SERVER_ACL_READONLY 0x1

	RB_ENTRY(server_acl_user)	entry;
};

static int
server_acl_cmp(struct server_acl_user *user1, struct server_acl_user *user2)
{
	if (user1->uid < user2->uid)
		return (-1);
	return (user1->uid > user2->uid);
}

RB_HEAD(server_acl_entries, server_acl_user) server_acl_entries;
RB_GENERATE_STATIC(server_acl_entries, server_acl_user, entry, server_acl_cmp);

/* Initialize server_acl tree. */
void
server_acl_init(void)
{
	RB_INIT(&server_acl_entries);

	if (getuid() != 0)
		server_acl_user_allow(0);
	server_acl_user_allow(getuid());
}

/* Find user entry. */
struct server_acl_user*
server_acl_user_find(uid_t uid)
{
	struct server_acl_user	find = { .uid = uid };

	return (RB_FIND(server_acl_entries, &server_acl_entries, &find));
}

/* Display the tree. */
void
server_acl_display(struct cmdq_item *item)
{
	struct server_acl_user	*loop;
	struct passwd		*pw;
	const char		*name;

	RB_FOREACH(loop, server_acl_entries, &server_acl_entries) {
		if (loop->uid == 0)
			continue;
		if ((pw = getpwuid(loop->uid)) != NULL)
			name = pw->pw_name;
		else
			name = "unknown";
		if (loop->flags == SERVER_ACL_READONLY)
			cmdq_print(item, "%s (R)", name);
		else
			cmdq_print(item, "%s (W)", name);
	}
}

/* Allow a user. */
void
server_acl_user_allow(uid_t uid)
{
	struct server_acl_user	*user;

	user = server_acl_user_find(uid);
	if (user == NULL) {
		user = xcalloc(1, sizeof *user);
		user->uid = uid;
		RB_INSERT(server_acl_entries, &server_acl_entries, user);
	}
}

/* Deny a user (remove from the tree). */
void
server_acl_user_deny(uid_t uid)
{
	struct server_acl_user	*user;

	user = server_acl_user_find(uid);
	if (user != NULL) {
		RB_REMOVE(server_acl_entries, &server_acl_entries, user);
		free(user);
	}
}

/* Allow this user write access. */
void
server_acl_user_allow_write(uid_t uid)
{
	struct server_acl_user	*user;
	struct client		*c;

	user = server_acl_user_find(uid);
	if (user == NULL)
		return;
	user->flags &= ~SERVER_ACL_READONLY;

	TAILQ_FOREACH(c, &clients, entry) {
		uid = proc_get_peer_uid(c->peer);
		if (uid != (uid_t)-1 && uid == user->uid)
			c->flags &= ~CLIENT_READONLY;
	}
}

/* Deny this user write access. */
void
server_acl_user_deny_write(uid_t uid)
{
	struct server_acl_user	*user;
	struct client		*c;

	user = server_acl_user_find(uid);
	if (user == NULL)
		return;
	user->flags |= SERVER_ACL_READONLY;

	TAILQ_FOREACH(c, &clients, entry) {
		uid = proc_get_peer_uid(c->peer);
		if (uid != (uid_t)-1 && uid == user->uid)
			c->flags |= CLIENT_READONLY;
	}
}

/*
 * Check if the client's UID exists in the ACL list and if so, set as read only
 * if needed. Return false if the user does not exist.
 */
int
server_acl_join(struct client *c)
{
	struct server_acl_user	*user;
	uid_t			 uid;

	uid = proc_get_peer_uid(c->peer);
	if (uid == (uid_t)-1)
		return (0);

	user = server_acl_user_find(uid);
	if (user == NULL)
		return (0);
	if (user->flags & SERVER_ACL_READONLY)
		c->flags |= CLIENT_READONLY;
	return (1);
}

/* Get UID for user entry. */
uid_t
server_acl_get_uid(struct server_acl_user *user)
{
	return (user->uid);
}
