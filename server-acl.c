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

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ctype.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

struct acl_user {
	RB_ENTRY(acl_user) entry;
	uid_t user_id;
};

/* Comparison for rb_tree */
static int 
uid_cmp(struct acl_user *user1, struct acl_user *user2) 
{
	if (user1->user_id < user2->user_id) {
		return -1;
	} else {
		return user1->user_id > user2->user_id;
	}
}

RB_HEAD(acl_user_entries, acl_user) acl_entries = RB_INITIALIZER(&acl_entries);
RB_GENERATE_STATIC(acl_user_entries, acl_user, entry, uid_cmp);

static struct acl_user* 
server_acl_user_create(void)
{
	struct acl_user* n = xmalloc(sizeof(*n));
	/* xmalloc will call fatal() if malloc fails */
	n->user_id = (uid_t)-1;
	return n;
}

static int 
server_acl_is_allowed(uid_t uid)
{
	int ok = 0;
	struct acl_user* iter = NULL;
	RB_FOREACH(iter, acl_user_entries, &acl_entries) {
		if (iter->user_id == uid) {
				ok = 1;
				break;
		}
	}
	return ok;
}

struct acl_user* 
server_acl_user_find(uid_t uid)
{
	struct acl_user* iter = NULL;
	RB_FOREACH(iter, acl_user_entries, &acl_entries) {
			if (iter->user_id == uid) {
				break;
			}
	}
	return iter;
}

int 
server_acl_check_host(uid_t uid)
{
	return uid == getuid();
}


void 
server_acl_init(void)
{
	uid_t host_uid;

	host_uid = getuid();
	server_acl_user_allow(host_uid);
}

void 
server_acl_user_allow(uid_t uid)
{
	/* Ensure entry doesn't already exist */
	struct acl_user* iter = NULL;
	int exists = 0;
	RB_FOREACH(iter, acl_user_entries, &acl_entries) {
			if (iter->user_id == uid) {
				/* ASSERT */
				if (getuid() != iter->user_id) {
					fatal(" owner mismatch for uid = %i\n", uid);
				}
				exists = 1;
				break;
			}
	}
	if (exists == 0) {
			int did_insert;
			struct acl_user* e = server_acl_user_create();
			e->user_id = uid;
			did_insert = 0;
			RB_INSERT(acl_user_entries, &acl_entries, e);
			RB_FOREACH(iter, acl_user_entries, &acl_entries) {
				if (iter == e) {
					did_insert = 1;
					break;
				}
			}
			if (did_insert == 0) {
				fatal(" Could not insert user_id %i\n", uid);
			}
	}
}

/*
* Remove user from acl list.
*/
void 
server_acl_user_deny(uid_t uid)
{
	int exists = 0;
	struct acl_user* iter = NULL;
	RB_FOREACH(iter, acl_user_entries, &acl_entries) {
			if (iter->user_id == uid) {
				/* ASSERT */
				if (iter->user_id == getuid()) {
					fatal(" Attempt to remove host from acl list.");
				}
				exists = 1;
				break;
			}
	}
	if (exists) {
			RB_REMOVE(acl_user_entries, &acl_entries, iter);
	} else if (exists == 0) {
			log_debug(
			" server_acl_deny warning: user %i was not found in acl list.\n", 
			uid);
	}
}

/*
 * Uses newfd, which is returned by the call to accept(), in server_accept(), to get user id of client
 * and confirm it's in the allow list.
 */
int 
server_acl_accept_validate(int newfd)
{
	struct client *c;
	struct passwd *pws;
	uid_t		uid;
	gid_t		gid;

	if (getpeereid(newfd, &uid, &gid) != 0) {
		log_debug(" SO_PEERCRED FAILURE: uid=%ld", (long)uid);
		return 0;	
	}

	pws = getpwuid(uid);
	if (pws == NULL) {
		log_debug(" SO_PEERCRED FAILURE: uid=%ld", (long)uid);
		return 0;
	}

	log_debug(" SO_PEERCRED SUCCESS: uid=%ld", (long)uid);

	if (server_acl_is_allowed(uid) == 0) {
			TAILQ_FOREACH(c, &clients, entry) {
				status_message_set(c, 3000, 1, 0, 
				"%s rejected from joining ", pws->pw_name);
			}
			log_debug(" denying user id %li", (long)uid);
			return 0;
	}
	TAILQ_FOREACH(c, &clients, entry) {
			status_message_set(c, 3000, 1, 0,
			 "%s joined the session", pws->pw_name);
	}

	log_debug(" allowing user id %li", (long)uid);

	return 1;
}

void 
server_acl_user_allow_write(struct passwd* user_data)
{
	struct acl_user* user = server_acl_user_find(user_data->pw_uid);
	if (user != NULL) {
			struct client* c = NULL;
			TAILQ_FOREACH(c, &clients, entry) {
				uid_t uid = proc_get_peer_uid(c->peer);
				if (uid != (uid_t)-1) {
					c->flags &= (~CLIENT_READONLY);
					break;
				} else {
					log_debug(
						" [acl-allow-write] bad client for user %s", 
						c->name
					);
				}
			}
	}
}

void 
server_acl_user_deny_write(struct passwd* user_data)
{
	struct acl_user* user = server_acl_user_find(user_data->pw_uid);
	if (user != NULL) {
			struct client* c = NULL;
			TAILQ_FOREACH(c, &clients, entry) {
				uid_t uid = proc_get_peer_uid(c->peer);
				if (uid == user->user_id) {
					c->flags &= (CLIENT_READONLY);
					break;
				}
			}
	} else {
			struct client* c = NULL;
			TAILQ_FOREACH(c, &clients, entry) {
				status_message_set(
					c, 3000, 1, 0, 
					"[acl-allow-write] WARNING: user %s is not in the acl", 
					user_data->pw_name
				);
			}
	}
}

/* 
 * Verify that the client's UID exists in the ACL list, 
 * and then set the access of the client to read only for the session.
 *
 * The call to proc_acl_get_ucred() will log an error message if it fails.
 */
int 
server_acl_join(struct client *c)
{
	uid_t		uid = proc_get_peer_uid(c->peer);
	struct acl_user *user;

	user = server_acl_user_find(uid);
	if (user != NULL) {
		if (user->user_id != getuid()) 
			c->flags |= CLIENT_READONLY;
		return (1);
	}

	return (0);
}

uid_t
server_acl_get_uid(struct acl_user* user) 
{
	return user->user_id;
}