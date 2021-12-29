/* $OpenBSD$ */

/*
 * Copyright (c) 2021 Holland Schutte, Jayson Morberg, Dallas Lyons
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

#include "tmux.h"

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ctype.h>
/*
 * User ID allow list for session extras.
 *
 * 	The owner field is a boolean. If true, the user id of the corresponding entry
 * 	is the user id which created the server.
 */
SLIST_HEAD(acl_user_entries, acl_user) acl_entries = SLIST_HEAD_INITIALIZER(acl_entries);

static struct acl_user* server_acl_user_create(void)
{
	struct acl_user* n = xmalloc(sizeof(*n));
	/* xmalloc will call fatal() if malloc fails */
	n->user_id = (uid_t)-1;
	n->is_owner = 0;
	n->entry.sle_next = NULL;
	return n;
}

static int server_acl_is_allowed(uid_t uid)
{
	int ok = 0;
	struct acl_user* iter = NULL;
	struct acl_user* next = NULL;
	SLIST_FOREACH_SAFE(iter, &acl_entries, entry, next) {
		if (iter->user_id == uid) {
			ok = 1;
			break;
		}
	}
	return ok;
}

/*
 * Public API
 */

struct acl_user* server_acl_user_find(uid_t uid)
{
	struct acl_user* ret = NULL;
	struct acl_user* iter = NULL;
	struct acl_user* next = NULL;
	SLIST_FOREACH_SAFE(iter, &acl_entries, entry, next) {
		if (iter->user_id == uid) {
			ret = iter;
			break;
		}
	}
	return ret;
}

int server_acl_check_host(uid_t uid)
{
	struct acl_user* user = server_acl_user_find(uid);
	if (user != NULL) {
		return user->is_owner;
	} else {
		return 0;
	}
}


void server_acl_init(void)
{
	uid_t host_uid;

	host_uid = getuid();
	
	SLIST_INIT(&acl_entries);

	/* 
	 * need to insert host username 
	 */
	server_acl_user_allow(host_uid, 1);

}

void server_acl_user_allow(uid_t uid, int owner)
{
	/* Ensure entry doesn't already exist */
	struct acl_user* iter = NULL;
	struct acl_user* next = NULL;
	int exists = 0;
	SLIST_FOREACH_SAFE(iter, &acl_entries, entry, next) {
		if (iter->user_id == uid) {
			/* ASSERT */
			if (owner != iter->is_owner) {
				fatal(" owner mismatch for uid = %i\n", uid);
			}
			exists = 1;
			break;
		}
	}
	if (!exists) {
		int did_insert;
		struct acl_user* e = server_acl_user_create();
		e->is_owner = owner;
		e->user_id = uid;
		did_insert = 0;
		SLIST_INSERT_HEAD(&acl_entries, e, entry);
		SLIST_FOREACH_SAFE(iter, &acl_entries, entry, next) {
			if (iter == e) {
				did_insert = 1;
				break;
			}
		}
		if (!did_insert) {
			fatal(" Could not insert user_id %i\n", uid);
		}
	}
}

/*
* Remove user from acl list.
*/
void server_acl_user_deny(uid_t uid)
{
	struct acl_user* iter = NULL;
	struct acl_user* next = NULL;
	int exists = 0;
	SLIST_FOREACH_SAFE(iter, &acl_entries, entry, next) {
		if (iter->user_id == uid) {
			/* ASSERT */
			if (iter->is_owner) {
				fatal(" Attempt to remove host from acl list.");
			}
			exists = 1;
			break;
		}
	}
	if (exists) {
		SLIST_REMOVE(&acl_entries, iter, acl_user, entry);
	} else if (!exists) {
		log_debug(" server_acl_deny warning: user %i was not found in acl list.\n", uid);
	}
}

/*
 * Uses newfd, which is returned by the call to accept(), in server_accept(), to get user id of client
 * and confirm it's in the allow list.
 */

int server_acl_accept_validate(int newfd, struct clients clientz)
{
	int len;
	struct ucred ucred;
	struct client	*c;
	struct passwd *pws;

	len = sizeof(struct ucred);

	if (getsockopt(newfd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
		log_debug(" SO_PEERCRED FAILURE errno = %d \n", errno); /*(0x%x)\*/
		return 0;
	}

	pws = getpwuid(ucred.uid);
	if (pws == NULL) {
		log_debug(" SO_PEERCRED FAILURE: pid=%li, euid=%li, egid=%li\n",
					(long)ucred.pid,
					(long)ucred.uid,
					(long)ucred.gid);
		return 0;
	}
	
	log_debug(" SO_PEERCRED SUCCESS: pid=%li, euid=%li, egid=%li\n",
				(long)ucred.pid,
				(long)ucred.uid,
				(long)ucred.gid);

	if (!server_acl_is_allowed(ucred.uid)) {
		TAILQ_FOREACH(c, &clientz, entry) {
			status_message_set(c, 3000, 1, 0, "%s rejected from joining session", pws->pw_name);
		}
		log_debug(" denying user id %li", (long) ucred.uid);
		return 0;
	}
	TAILQ_FOREACH(c, &clientz, entry) {
		status_message_set(c, 3000, 1, 0, "%s joined the session", pws->pw_name);
	}

	log_debug(" allowing user id %li", (long) ucred.uid);

	return 1;
}

void server_acl_user_allow_write(struct passwd* user_data)
{
	struct acl_user* user = server_acl_user_find(user_data->pw_uid);
	if (user != NULL) {
		struct client* c = NULL;
		TAILQ_FOREACH(c, &clients, entry) {
			struct ucred cred = {0};
			if (proc_acl_get_ucred(c->peer, &cred)) {
				if (cred.uid == user->user_id) {
					c->flags &= (~CLIENT_READONLY);
					break;
				}
			}
			else {
				log_debug(
					" [acl-allow-write] bad client for user %s", 
					c->name
				);
			}
		}
	}
}

void server_acl_user_deny_write(struct passwd* user_data)
{
	struct acl_user* user = server_acl_user_find(user_data->pw_uid);
	if (user != NULL) {
		struct client* c = NULL;
		TAILQ_FOREACH(c, &clients, entry) {
			struct ucred cred = {0};
			if (proc_acl_get_ucred(c->peer, &cred)) {
				if (cred.uid == user->user_id) {
					c->flags &= (CLIENT_READONLY);
					break;
				}
			}
			else {
				log_debug(
					" [acl-allow-write] bad client, %s, found for user %s", 
					c->name, 
					user_data->pw_name
				);
			}
		}
	}
	else {
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
int server_acl_attach_session(struct client *c)
{
	struct ucred cred = {0};
	int ret = 0;
	if (proc_acl_get_ucred(c->peer, &cred)) {
		struct acl_user *user = server_acl_user_find(cred.uid);
		if (user != NULL) {
			ret = 1;
		} else {
			log_debug(
				" [acl_attach] invalid client attached : name = %s, uid = %i\n",
				c->name, cred.uid
			);
		}
	}
	return ret;
}
