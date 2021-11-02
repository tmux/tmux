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
/*
 * Removes a user from the whitelist
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
    struct cmd_find_state *target = cmdq_get_target(item);
    struct session *s = target->s;
    const char *template;
    struct format_tree *ft;
    char *oldname;
    char name[100];
    struct client *client;
    struct passwd *user_data;

    FILE* username_file = fopen(TMUX_ACL_WHITELIST, "r");
    FILE* temp_file = fopen("temp_whitelist", "w");

    // *TEMP* Check for an empty command arguement
    if (args->argc == 0) {
      return (CMD_RETURN_NORMAL);
    }
      
    // Check that both files were opened successfully
    if (username_file == NULL) {
      notify_session("Could not open whitelist", s);
      return (CMD_RETURN_NORMAL);
    }
    if (temp_file == NULL) {
      notify_session("Could not open temp file", s);
    }
      
    // Pass username arguement into 'newname'
    template = args->argv[0];
    client = cmdq_get_client(item);
    ft = format_create(client, item, FORMAT_NONE, 0);
    oldname = format_expand_time(ft, template);

    // Transfer usernames to tmp file other than the name
    //  being removed
    while (fgets(name, sizeof(name), username_file)) {
      if (strstr(name, oldname) != NULL) {
	continue;
      } else {
	fprintf(temp_file, "%s", name);
      }
    }

    // Check that the username is valid and remove from the list
    user_data = getpwnam(oldname);
    if (user_data != NULL) {
      server_acl_user_deny(user_data->pw_uid);
      //proc_kill_peer(client->peer);
      //proc_update_event(client->peer);
    }
  
    fclose(username_file);
    fclose(temp_file);

    // Give the temp file the correct name
    rename("temp_whitelist", TMUX_ACL_WHITELIST);
    
    free(oldname);
    format_free(ft);
    
    return (CMD_RETURN_NORMAL);
}
