#include "tmux.h"

#if defined(TMUX_ACL)

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ctype.h>

#define TMUX_ACL_LOG "[access control list]"

#ifndef TMUX_ACL_WHITELIST
#define TMUX_ACL_WHITELIST "./tmux-acl-whitelist"
#endif

#define ERRNOBUFSZ 512
static char errno_buf[ERRNOBUFSZ] = {0};

static char* errnostr(void)
{
	memset(errno_buf, 0, ERRNOBUFSZ);
	if (strerror_r(errno, errno_buf, ERRNOBUFSZ) != 0) {
		char num[32] = {0};
		strcat(errno_buf, "strerror_r failure. errno is ");
		snprintf(num, 31, "%i", errno);
		strcat(errno_buf, num);
	}
	return errno_buf;
}

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
	FILE* username_file; 
	uid_t host_uid;

	host_uid = getuid();
	
	SLIST_INIT(&acl_entries);
	
	/* 
	 * need to insert host username 
	 */
	server_acl_user_allow(host_uid, 1);
	
	/* 
	 * User may not care about ACL whitelisting for their session, 
	 * so if it doesn't exist it's reasonable to not create it. 
	 */
	username_file = fopen(TMUX_ACL_WHITELIST, "rb");
	
	if (username_file != NULL) {
		uid_t uid = 0;
		struct passwd* user_data;
			
		char username[256] = {0}; 	
		int add_count = 0;
		/* 
		 * Reads TMUX_ACL_WHITELIST for line-delimited usernames, 
		 * then allows said users into the shared session 
		 */
		while (fgets(username, 256, username_file) != NULL) {
			size_t username_len = strlen(username);
			if (username_len > 0 && isalnum(username[0])) {
				/* trim last character if necessary */
				if (isspace(username[username_len-1])) {
					username[username_len-1] = '\0';
				}
				user_data = getpwnam(username);
				if (user_data != NULL) {
					uid = user_data->pw_uid;
					if (uid != host_uid) {
						add_count++;
						server_acl_user_allow(uid, 0);
					}
					else {
						log_debug(TMUX_ACL_LOG "Warning: %s contains the username of the host",
									TMUX_ACL_WHITELIST);
					}
				}
				else {
					log_debug(TMUX_ACL_LOG " ERROR: getpwnam failed to find UID for username %s: %s", 
								username, 
								errnostr());

				}
			}
		}
		fclose(username_file);

		/* We do have whitelisted users, so we should open the socket*/
		if (add_count > 0) {
			if (chmod(socket_path, S_IRGRP | S_IWGRP | S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH) != 0) {
				log_debug(TMUX_ACL_LOG " Warning: chmod for %s failed with error %s", socket_path, errnostr());
			}
		}
	}
	else {
		log_debug(TMUX_ACL_LOG " Warning: Could not open %s: %s", TMUX_ACL_WHITELIST, errnostr()); 
	}
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
			fatal(TMUX_ACL_LOG " Could not insert user_id %i\n", uid);
		}
	}
}

/*
 * Uses newfd, which is returned by the call to accept(), in server_accept(), to get user id of client
 * and confirm it's in the allow list.
 */

int server_acl_accept_validate(int newfd, struct clients clients)
{
	int len;
	struct ucred ucred;
	struct client	*c;
	struct passwd *pws;

	len = sizeof(struct ucred);

	if (getsockopt(newfd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
		log_debug(TMUX_ACL_LOG " SO_PEERCRED FAILURE errno = %s (0x%x)\n", errnostr(), errno);
		return 0;
	}

	pws = getpwuid(ucred.uid);
	
	log_debug(TMUX_ACL_LOG " SO_PEERCRED SUCCESS: pid=%li, euid=%li, egid=%li\n",
				(long)ucred.pid,
				(long)ucred.uid,
				(long)ucred.gid);

	if (!server_acl_is_allowed(ucred.uid)) {
		TAILQ_FOREACH(c, &clients, entry) {
			status_message_set(c, 3000, 1, 0, "%s rejected from joining session", pws->pw_name);
		}
		log_debug(TMUX_ACL_LOG " denying user id %li", (long) ucred.uid);
		return 0;
	}
	TAILQ_FOREACH(c, &clients, entry) {
		status_message_set(c, 3000, 1, 0, "%s joined the session", pws->pw_name);
	}

	log_debug(TMUX_ACL_LOG " allowing user id %li", (long) ucred.uid);

	return 1;
}

#endif /* TMUX_ACL */
