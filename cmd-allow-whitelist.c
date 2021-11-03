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
 * Adds a new user to the whitelist
 *
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
  struct cmd_find_state *target = cmdq_get_target(item);
  struct session *s = target->s;
  const char *template;
  struct format_tree *ft;
  char *newname;
  char name[100];
  struct passwd *user_data;

  // Do nothing if no arguements present
  if (args->argc == 0) {
    return (CMD_RETURN_NORMAL);
  }
  
  // Pass username arguement into 'newname'
  template = args->argv[0];
  ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
  newname = format_expand_time(ft, template);

  // Check that the username is valid
  user_data = getpwnam(newname);
  if (user_data != NULL) {
    server_acl_user_allow(user_data->pw_uid, 0);
  } else {
    free(newname);
    format_free(ft);
  
    return (CMD_RETURN_NORMAL);
  }  
  
  free(newname);
  format_free(ft);
  
  return (CMD_RETURN_NORMAL);
}
