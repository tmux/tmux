/* $OpenBSD$ */

/*
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>

#include "tmux.h"

/*
 *  Controls access to session
 */
static enum cmd_retval cmd_server_access_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_server_access_entry = {
    .name = "server-access",
    .alias = NULL,

    .args = { "adwr", 0, 1, NULL },
    .usage = "[-a allow] [-d deny] [-w write] [-r read] "
             CMD_TARGET_PANE_USAGE " [username]",

    .flags = CMD_CLIENT_CANFAIL,
    .exec = cmd_server_access_exec
};

static enum cmd_retval 
cmd_server_access_exec(struct cmd *self, struct cmdq_item *item) {

    struct args *args = cmd_get_args(self);
    struct client *c = cmdq_get_target_client(item), *loop;
    const char *template;
    struct format_tree *ft;
    char *name;
    struct passwd *user_data;
    struct ucred u_cred = {0};

    template = args_string(args, 0);
    ft = format_create(c, item, FORMAT_NONE, 0);
    name = format_expand_time(ft, template);
    user_data = getpwnam(name);

    /* User doesn't exist */
    if (user_data == NULL) {
        cmdq_error(item, " unknown user: %s", name);
        return (CMD_RETURN_NORMAL);
    }

    /* Contradictions */
    if ((args_has(args, 'a') && args_has(args, 'd')) || 
        (args_has(args, 'w') && args_has(args, 'r'))) {
        
        cmdq_error(item, " contradicting flags");
        
        return (CMD_RETURN_NORMAL);
    }

    /* Deny user */
    if (args_has(args, 'd')) {

        if (!server_acl_check_host(user_data->pw_uid)) {
            if (server_acl_user_find(user_data->pw_uid)) {
                TAILQ_FOREACH(loop, &clients, entry) {
                    struct acl_user* user = 
                    server_acl_user_find(user_data->pw_uid);

                    if (proc_acl_get_ucred(loop->peer, &u_cred)) {
                        if (u_cred.uid == user->user_id) {
                            loop->flags |= CLIENT_EXIT;
                            break;
                        }
                    }
                }
            }
        }
        
        if (server_acl_check_host(user_data->pw_uid)) {
            cmdq_error(item, " cannot remove: user %s is the host", name);
        } else if (server_acl_user_find(user_data->pw_uid)) {
            server_acl_user_deny(user_data->pw_uid); 
            cmdq_error(item, " user %s has been removed", name);
        } else {
            cmdq_error(item, " user %s not found", name);
        }

        free(name);
        format_free(ft);

        return (CMD_RETURN_NORMAL);
    }

    /* Allow user */
    if (args_has(args, 'a')) {

        if (!server_acl_check_host(user_data->pw_uid)) {
            if (!server_acl_user_find(user_data->pw_uid)) {
                server_acl_user_allow(user_data->pw_uid, 0);
                cmdq_error(item, " user %s has been added", name);
            } else {
                cmdq_error(item, " user %s is already added", name);
            }
        } else if (server_acl_check_host(user_data->pw_uid)) {
            cmdq_error(item, " cannot add: user %s is the host", name);
        }
    }

    /* Give write permission */
    if (args_has(args, 'w')) {

        if (!server_acl_check_host(user_data->pw_uid)) {
            if (server_acl_user_find(user_data->pw_uid)) {
                server_acl_user_allow_write(user_data);
                cmdq_error(item, " user %s has write privilege", name);
            } else {
                cmdq_error(item, " user %s not found in whitelist", name);
            }
        } else {
            cmdq_error(item, " cannot change hosts write privileges");
        }
    }

    /* Remove write permission */
    if (args_has(args, 'r')) {

        if (!server_acl_check_host(user_data->pw_uid)) {
            if (server_acl_user_find(user_data->pw_uid)) {
                server_acl_user_deny_write(user_data);
                cmdq_error(item, " removed user %s write privilege", name);
            } else {
                cmdq_error(item, " user %s not found in whitelist", name);
            }
        } else {
            cmdq_error(item, " cannot change hosts write privilege");
        }
    }

    free(name);
    format_free(ft);

    return (CMD_RETURN_NORMAL);
}