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
#include <pwd.h>

#include "tmux.h"

#define TMUX_ACL_WHITELIST "./tmux-acl-whitelist"
#define MSG TMUX_ACL_LOG
/*
 * Adds a new user to the acl list
 */

static enum cmd_retval cmd_allow_whitelist_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_allow_whitelist_entry = {
  .name = "allow-whitelist",
  .alias = "allow",

  .args = { "", 0, 1 },
  .usage = "[username]", 

  .flags = CMD_AFTERHOOK,
  .exec = cmd_allow_whitelist_exec
};

static enum cmd_retval cmd_allow_whitelist_exec(struct cmd *self, struct cmdq_item *item) {

  struct args *args = cmd_get_args(self);
  const char *template;
  struct format_tree *ft;
  char *newname;
  struct passwd *user_data;

  if (args->argc == 0) {
    cmdq_error(item, " argument <username> not provided");
    return (CMD_RETURN_NORMAL);
  }
  
  template = args->argv[0];
  ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
  newname = format_expand_time(ft, template);
  user_data = getpwnam(newname);

  if(user_data != NULL && server_acl_check_host(user_data->pw_uid)){
    cmdq_error(item, " cannot add host to whitelist");
    return (CMD_RETURN_NORMAL);
  }

  if (user_data != NULL) {
    if (!server_acl_user_find(user_data->pw_uid)) {
      cmdq_error(item, " user %s has been added", newname);
      server_acl_user_allow(user_data->pw_uid, 0);
    } else {
      cmdq_error(item, " user %s is already added", newname);
    }
  } else {
    cmdq_error(item, " user %s not found", newname);

    free(newname);
    format_free(ft);
  
    return (CMD_RETURN_NORMAL);
  }  
  
  free(newname);
  format_free(ft);
  
  return (CMD_RETURN_NORMAL);
}
