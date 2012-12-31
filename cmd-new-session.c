/* $Id$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Create a new session and attach to the current terminal unless -d is given.
 */

enum cmd_retval	 cmd_new_session_check(struct args *);
enum cmd_retval	 cmd_new_session_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_new_session_entry = {
	"new-session", "new",
	"dn:s:t:x:y:", 0, 1,
	"[-d] [-n window-name] [-s session-name] " CMD_TARGET_SESSION_USAGE
	" [-x width] [-y height] [command]",
	CMD_STARTSERVER|CMD_CANTNEST|CMD_SENDENVIRON,
	NULL,
	cmd_new_session_check,
	cmd_new_session_exec
};

enum cmd_retval
cmd_new_session_check(struct args *args)
{
	if (args_has(args, 't') && (args->argc != 0 || args_has(args, 'n')))
		return (CMD_RETURN_ERROR);
	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_new_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct session		*s, *old_s, *groupwith;
	struct window		*w;
	struct environ		 env;
	struct termios		 tio, *tiop;
	struct passwd		*pw;
	const char		*newname, *target, *update, *cwd, *errstr;
	char			*cmd, *cause;
	int			 detached, idx;
	u_int			 sx, sy;

	newname = args_get(args, 's');
	if (newname != NULL) {
		if (!session_check_name(newname)) {
			ctx->error(ctx, "bad session name: %s", newname);
			return (CMD_RETURN_ERROR);
		}
		if (session_find(newname) != NULL) {
			ctx->error(ctx, "duplicate session: %s", newname);
			return (CMD_RETURN_ERROR);
		}
	}

	target = args_get(args, 't');
	if (target != NULL) {
		groupwith = cmd_find_session(ctx, target, 0);
		if (groupwith == NULL)
			return (CMD_RETURN_ERROR);
	} else
		groupwith = NULL;

	/*
	 * There are three cases:
	 *
	 * 1. If cmdclient is non-NULL, new-session has been called from the
	 *    command-line - cmdclient is to become a new attached, interactive
	 *    client. Unless -d is given, the terminal must be opened and then
	 *    the client sent MSG_READY.
	 *
	 * 2. If cmdclient is NULL, new-session has been called from an
	 *    existing client (such as a key binding).
	 *
	 * 3. Both are NULL, the command was in the configuration file. Treat
	 *    this as if -d was given even if it was not.
	 *
	 * In all cases, a new additional session needs to be created and
	 * (unless -d) set as the current session for the client.
	 */

	/* Set -d if no client. */
	detached = args_has(args, 'd');
	if (ctx->cmdclient == NULL && ctx->curclient == NULL)
		detached = 1;

	/*
	 * Save the termios settings, part of which is used for new windows in
	 * this session.
	 *
	 * This is read again with tcgetattr() rather than using tty.tio as if
	 * detached, tty_open won't be called. Because of this, it must be done
	 * before opening the terminal as that calls tcsetattr() to prepare for
	 * tmux taking over.
	 */
	if (ctx->cmdclient != NULL && ctx->cmdclient->tty.fd != -1) {
		if (tcgetattr(ctx->cmdclient->tty.fd, &tio) != 0)
			fatal("tcgetattr failed");
		tiop = &tio;
	} else
		tiop = NULL;

	/* Open the terminal if necessary. */
	if (!detached && ctx->cmdclient != NULL) {
		if (server_client_open(ctx->cmdclient, NULL, &cause) != 0) {
			ctx->error(ctx, "open terminal failed: %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	/* Get the new session working directory. */
	if (ctx->cmdclient != NULL && ctx->cmdclient->cwd != NULL)
		cwd = ctx->cmdclient->cwd;
	else {
		pw = getpwuid(getuid());
		if (pw->pw_dir != NULL && *pw->pw_dir != '\0')
			cwd = pw->pw_dir;
		else
			cwd = "/";
	}

	/* Find new session size. */
	if (ctx->cmdclient != NULL) {
		sx = ctx->cmdclient->tty.sx;
		sy = ctx->cmdclient->tty.sy;
	} else if (ctx->curclient != NULL) {
		sx = ctx->curclient->tty.sx;
		sy = ctx->curclient->tty.sy;
	} else {
		sx = 80;
		sy = 24;
	}
	if (detached) {
		if (args_has(args, 'x')) {
			sx = strtonum(
			    args_get(args, 'x'), 1, USHRT_MAX, &errstr);
			if (errstr != NULL) {
				ctx->error(ctx, "width %s", errstr);
				return (CMD_RETURN_ERROR);
			}
		}
		if (args_has(args, 'y')) {
			sy = strtonum(
			    args_get(args, 'y'), 1, USHRT_MAX, &errstr);
			if (errstr != NULL) {
				ctx->error(ctx, "height %s", errstr);
				return (CMD_RETURN_ERROR);
			}
		}
	}
	if (sy > 0 && options_get_number(&global_s_options, "status"))
		sy--;
	if (sx == 0)
		sx = 1;
	if (sy == 0)
		sy = 1;

	/* Figure out the command for the new window. */
	if (target != NULL)
		cmd = NULL;
	else if (args->argc != 0)
		cmd = args->argv[0];
	else
		cmd = options_get_string(&global_s_options, "default-command");

	/* Construct the environment. */
	environ_init(&env);
	update = options_get_string(&global_s_options, "update-environment");
	if (ctx->cmdclient != NULL)
		environ_update(update, &ctx->cmdclient->environ, &env);

	/* Create the new session. */
	idx = -1 - options_get_number(&global_s_options, "base-index");
	s = session_create(newname, cmd, cwd, &env, tiop, idx, sx, sy, &cause);
	if (s == NULL) {
		ctx->error(ctx, "create session failed: %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}
	environ_free(&env);

	/* Set the initial window name if one given. */
	if (cmd != NULL && args_has(args, 'n')) {
		w = s->curw->window;

		window_set_name(w, args_get(args, 'n'));

		options_set_number(&w->options, "automatic-rename", 0);
	}

	/*
	 * If a target session is given, this is to be part of a session group,
	 * so add it to the group and synchronize.
	 */
	if (groupwith != NULL) {
		session_group_add(groupwith, s);
		session_group_synchronize_to(s);
		session_select(s, RB_ROOT(&s->windows)->idx);
	}

	/*
	 * Set the client to the new session. If a command client exists, it is
	 * taking this session and needs to get MSG_READY and stay around.
	 */
	if (!detached) {
		if (ctx->cmdclient != NULL) {
			server_write_ready(ctx->cmdclient);

			old_s = ctx->cmdclient->session;
			if (old_s != NULL)
				ctx->cmdclient->last_session = old_s;
			ctx->cmdclient->session = s;
			notify_attached_session_changed(ctx->cmdclient);
			session_update_activity(s);
			server_redraw_client(ctx->cmdclient);
		} else {
			old_s = ctx->curclient->session;
			if (old_s != NULL)
				ctx->curclient->last_session = old_s;
			ctx->curclient->session = s;
			notify_attached_session_changed(ctx->curclient);
			session_update_activity(s);
			server_redraw_client(ctx->curclient);
		}
	}
	recalculate_sizes();
	server_update_socket();

	/*
	 * If there are still configuration file errors to display, put the new
	 * session's current window into more mode and display them now.
	 */
	if (cfg_finished)
		show_cfg_causes(s);

	return (detached ? CMD_RETURN_NORMAL : CMD_RETURN_ATTACH);
}
