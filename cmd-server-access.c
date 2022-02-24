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

#include <sys/stat.h>
#include <sys/types.h>

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tmux.h"

/*
 *  Controls access to session
 */
static enum cmd_retval cmd_server_access_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_server_access_entry = {
    .name = "server-access",
    .alias = NULL,

    .args = { "adwr", 1, 1, NULL },
    .usage = "[-adrw]"
             CMD_TARGET_PANE_USAGE " [username]",

    .flags = CMD_CLIENT_CANFAIL,
    .exec = cmd_server_access_exec
};

static enum cmd_retval
cmd_server_access_allow(struct cmdq_item *item, struct passwd *user_data, char *name) 
{

    if (server_acl_check_host(user_data->pw_uid) == 0) {
        if (server_acl_user_find(user_data->pw_uid) == 0) {
            server_acl_user_allow(user_data->pw_uid);
        } else {
            cmdq_error(item, "user %s is already added", name);
            return (CMD_RETURN_ERROR);
        }
    } else if (server_acl_check_host(user_data->pw_uid)) {
        cmdq_error(item, "cannot add: user %s is the host", name);
        return (CMD_RETURN_ERROR);
    }

    return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_server_access_deny(struct cmdq_item *item, struct passwd *user_data, char *name) 
{

    struct client *loop;
    struct ucred u_cred = {0};

    if (server_acl_check_host(user_data->pw_uid) == 0) {
        if (server_acl_user_find(user_data->pw_uid)) {
            TAILQ_FOREACH(loop, &clients, entry) {
                struct acl_user* user = 
                server_acl_user_find(user_data->pw_uid);

                if (proc_acl_get_ucred(loop->peer, &u_cred)) {
                    if (u_cred.uid == server_acl_get_uid(user)) {
                        loop->flags |= CLIENT_EXIT;
                        break;
                    }
                }
            }
        }
    }

    if (server_acl_check_host(user_data->pw_uid)) {
        cmdq_error(item, "cannot remove: user %s is the host", name);
        return (CMD_RETURN_ERROR);
    } else if (server_acl_user_find(user_data->pw_uid)) {
        server_acl_user_deny(user_data->pw_uid); 
    } else {
        cmdq_error(item, "user %s not found", name);
        return (CMD_RETURN_ERROR);
    }

    return (CMD_RETURN_NORMAL);
}

static enum cmd_retval 
cmd_server_access_exec(struct cmd *self, struct cmdq_item *item) 
{

    struct args *args = cmd_get_args(self);
    struct client *c = cmdq_get_target_client(item);
    const char  *template;
    char        *name;
    struct passwd *user_data;

    template = args_string(args, 0);
    name = format_single(item, template, c, NULL, NULL, NULL);
    user_data = getpwnam(name);

    /* User doesn't exist */
    if (user_data == NULL) {
            cmdq_error(item, "unknown user: %s", name);
            return (CMD_RETURN_ERROR);
    }

    /* Contradictions */
    if ((args_has(args, 'a') && args_has(args, 'd')) || 
        (args_has(args, 'w') && args_has(args, 'r'))) {
        
            cmdq_error(item, "contradicting flags");
            free(name);
            return (CMD_RETURN_ERROR);
    }

    /* Deny user */
    if (args_has(args, 'd')) {
        if (cmd_server_access_deny(item, user_data, name) == 
                CMD_RETURN_NORMAL) {
            status_message_set(c, 1000, 0, 0, "user %s has been removed",
		        name);
            return (CMD_RETURN_NORMAL);
        }
        
        return (CMD_RETURN_ERROR);
    }

    /* Allow user */
    if (args_has(args, 'a')) {
        if (cmd_server_access_allow(item, user_data, name) == 
                CMD_RETURN_NORMAL) {
            status_message_set(c, 1000, 0, 0, "user %s has been added",
		        name);
        }
    }

    /* Give write permission */
    if (args_has(args, 'w')) {

            if (server_acl_check_host(user_data->pw_uid) == 0) {
                if (server_acl_user_find(user_data->pw_uid)) {
                    server_acl_user_allow_write(user_data);
                    status_message_set(c, 1000, 0, 0, 
                        "user %s has write privilege", name);
                } else {
                    cmdq_error(item, "user %s not found in ACL", name);
                    free(name);
                    return (CMD_RETURN_ERROR);
                }
            } else {
                cmdq_error(item, "cannot change hosts write privileges");
                free(name);
                return (CMD_RETURN_ERROR);
            }
    }

    /* Remove write permission */
    if (args_has(args, 'r')) {

            if (server_acl_check_host(user_data->pw_uid) == 0) {
                if (server_acl_user_find(user_data->pw_uid)) {
                    server_acl_user_deny_write(user_data);
                    status_message_set(c, 1000, 0, 0, 
                        "removed user %s write privilege", name);
                } else {
                    cmdq_error(item, "user %s not found in whitelist", name);
                    free(name);
                    return (CMD_RETURN_ERROR);
                }
            } else {
                cmdq_error(item, "cannot change hosts write privilege");
                free(name);
                return (CMD_RETURN_ERROR);
            }
    }

    free(name);

    return (CMD_RETURN_NORMAL);
}