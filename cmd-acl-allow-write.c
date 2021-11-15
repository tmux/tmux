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

static enum cmd_retval cmd_acl_allow_write_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_acl_allow_write_entry = {
  .name = "acl-allow-write",
  .alias = "acl-write",

  .args = { "", 0, 1 },
  .usage = "<username>",

  .flags = 0,
  .exec = cmd_acl_allow_write_exec
};

enum cmd_retval cmd_acl_allow_write_exec(struct cmd *self, struct cmdq_item *item) {
    struct args *args = cmd_get_args(self);
    const char *template;
    struct format_tree *ft;
    char *oldname;
    struct passwd *user_data;

    // *TEMP* Check for an empty command arguement
    if (args->argc == 0) {
      return (CMD_RETURN_NORMAL);
    }
      
    // Pass username arguement into 'newname'
    template = args->argv[0];
    ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
    oldname = format_expand_time(ft, template);

    // Check that the username is valid and remove from the list
    user_data = getpwnam(oldname);

    if(server_acl_check_host(user_data->pw_uid)){
      cmdq_error(item, " cannot change hosts write privileges");
      return (CMD_RETURN_NORMAL);
    }

    if (user_data != NULL) {
        server_acl_user_allow_write(user_data);
    }
    else {
        struct client* c = NULL;
		TAILQ_FOREACH(c, &clients, entry) {
			status_message_set(c, 3000, 1, 0, "[acl-allow-write] unknown user: %s", template);
		}
    }
    
    free(oldname);
    format_free(ft);
    
    return (CMD_RETURN_NORMAL);
}
