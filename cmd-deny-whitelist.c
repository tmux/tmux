/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>

#include "tmux.h"

#define TMUX_ACL_WHITELIST "./tmux-acl-whitelist"

struct acl_user {
	uid_t user_id;
	int is_owner;

	SLIST_ENTRY(acl_user) entry;
};

/*
 * Removes a user from the acl-list
 */

static enum cmd_retval cmd_deny_whitelist_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_deny_whitelist_entry = {
  .name = "deny-whitelist",
  .alias = "deny",

  .args = { "", 0, 1 },
  .usage = "<username>",

  .flags = 0,
    .exec = cmd_deny_whitelist_exec
};

enum cmd_retval cmd_deny_whitelist_exec(struct cmd *self, struct cmdq_item *item) {

  struct args *args = cmd_get_args(self);
  struct client *c = cmdq_get_target_client(item), *loop;
  const char *template;
  struct format_tree *ft;
  char *oldname;
  struct passwd *user_data;
  struct ucred u_cred = {0};

  if (args->argc == 0) {
    cmdq_error(item, " argument <username> not provided");
    return (CMD_RETURN_NORMAL);
  }

  template = args->argv[0];
  ft = format_create(c, item, FORMAT_NONE, 0);
  oldname = format_expand_time(ft, template);
  
  user_data = getpwnam(oldname);

  if (user_data != NULL && !server_acl_check_host(user_data->pw_uid)) {
    if (server_acl_user_find(user_data->pw_uid)) {
      TAILQ_FOREACH(loop, &clients, entry) {
        struct acl_user* user = server_acl_user_find(user_data->pw_uid);
        if (proc_acl_get_ucred(loop->peer, &u_cred)) {
          if (u_cred.uid == user->user_id) {
            loop->flags |= CLIENT_EXIT;
            break;
          }
        }
      }
    }
  }

  if (user_data != NULL) {
    if (server_acl_check_host(user_data->pw_uid)) {
      cmdq_error(item, " cannot remove: user %s is the host", oldname);
    } else if (server_acl_user_find(user_data->pw_uid)) {
      server_acl_user_deny(user_data->pw_uid); 
      cmdq_error(item, " user %s has been removed", oldname);
    } else {
      cmdq_error(item, " user %s not found", oldname);
    }
  }

  free(oldname);
  format_free(ft);

  return (CMD_RETURN_NORMAL);
}
