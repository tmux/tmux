#include "tmux.h"

#if defined(TMUX_ACL)

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pwd.h>


#define TMUX_ACL_LOG "[access control list]"


/*
 * User ID allow list for session extras.
 *
 * 	The owner field is a boolean. If true, the user id of the corresponding entry
 * 	is the user id which created the server.
 */

struct acl_user {
	uid_t user_id;
	int is_owner;

	SLIST_ENTRY(acl_user) entry;
};
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

void server_acl_init(void)
{
	uid_t host_uid = getuid();
	uid_t uid = 0;
	struct passwd* user_data;
	FILE * username_file = fopen("whitelist.txt", "r+");
	char * username = malloc(128);
	
	if (username == NULL) {
		fatal(TMUX_ACL_LOG " malloc failed in server_acl_init");
	}
	if (username_file == NULL) {
		log_debug(TMUX_ACL_LOG " server-acl.c was unable to open whitelist.txt");
	}
	
	SLIST_INIT(&acl_entries);
	/* need to insert host username */
	server_acl_user_allow(host_uid, 1);
	chmod(socket_path, S_IRGRP | S_IWGRP | S_IRUSR | S_IWUSR);

	/* Reads uid.txt for usernames, then allows said users to the ACL whitelist */
    while (!feof(username_file)) {
		
        fscanf(username_file, "%s", username);
		user_data = getpwnam(username);
		uid = user_data->pw_uid;

		if (uid != host_uid) {
			server_acl_user_allow(uid, 0);
		}
		
		if (user_data == NULL) {
			log_debug(TMUX_ACL_LOG " getpwnam failed to find UID for username %s", username);
		}
		if (uid == host_uid) {
			log_debug(TMUX_ACL_LOG " whitelist.txt contains the username of the host");
		}
		
		else {
			server_acl_user_allow(uid, 0);
		}
    }
    fclose(username_file);
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
				fatal(TMUX_ACL_LOG " owner mismatch for uid = %i\n", uid);
			}
			exists = 1;
			break;
		}
	}
	log_debug(TMUX_ACL_LOG " allow user before (uid, owner, already exists) = (%li, %i, %i)",
			  (long int) uid,
			  owner,
			  exists);
	if (!exists) {
		struct acl_user* e = server_acl_user_create();
		e->is_owner = owner;
		e->user_id = uid;
		SLIST_INSERT_HEAD(&acl_entries, e, entry);
		SLIST_FOREACH_SAFE(iter, &acl_entries, entry, next) {
			if (iter == e) {
				log_debug(TMUX_ACL_LOG " allow user after (uid, owner) = (%li, %i)",
						  (long int) uid,
						  owner);
				break;
			}
		}
	}
}

/*
 * Uses newfd, which is returned by the call to accept(), in server_accept(), to get user id of client
 * and confirm it's in the allow list.
 */

int server_acl_accept_validate(int newfd)
{
	int len;
	struct ucred ucred;

	len = sizeof(struct ucred);

	if (getsockopt(newfd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
		log_debug(TMUX_ACL_LOG " SO_PEERCRED FAILURE errno = %s (0x%x)\n", strerror(errno), errno);
		return 0;
	}

	log_debug(TMUX_ACL_LOG " SO_PEERCRED SUCCESS: pid=%li, euid=%li, egid=%li\n",
			  (long)ucred.pid,
			  (long)ucred.uid,
			  (long)ucred.gid);

	if (!server_acl_is_allowed(ucred.uid)) {
		log_debug(TMUX_ACL_LOG " denying user id %li", (long) ucred.uid);
		return 0;
	}

	log_debug(TMUX_ACL_LOG " allowing user id %li", (long) ucred.uid);

	return 1;
}

#endif /* TMUX_ACL */
