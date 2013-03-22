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
enum cmd_retval	 cmd_new_session_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_new_session_entry = {
	"new-session", "new",
	"AdDF:n:Ps:t:x:y:", 0, 1,
	"[-AdDP] [-F format] [-n window-name] [-s session-name] "
	CMD_TARGET_SESSION_USAGE " [-x width] [-y height] [command]",
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
cmd_new_session_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c = cmdq->client;
	struct session		*s, *groupwith;
	struct window		*w;
	struct environ		 env;
	struct termios		 tio, *tiop;
	struct passwd		*pw;
	const char		*newname, *target, *update, *cwd, *errstr;
	const char		*template;
	char			*cmd, *cause, *cp;
	int			 detached, idx;
	u_int			 sx, sy;
	int			 already_attached;
	struct format_tree	*ft;

	newname = args_get(args, 's');
	if (newname != NULL) {
		if (!session_check_name(newname)) {
			cmdq_error(cmdq, "bad session name: %s", newname);
			return (CMD_RETURN_ERROR);
		}
		if (session_find(newname) != NULL) {
			if (args_has(args, 'A')) {
				return (cmd_attach_session(cmdq, newname,
				    args_has(args, 'D'), 0));
			}
			cmdq_error(cmdq, "duplicate session: %s", newname);
			return (CMD_RETURN_ERROR);
		}
	}

	target = args_get(args, 't');
	if (target != NULL) {
		groupwith = cmd_find_session(cmdq, target, 0);
		if (groupwith == NULL)
			return (CMD_RETURN_ERROR);
	} else
		groupwith = NULL;

	/* Set -d if no client. */
	detached = args_has(args, 'd');
	if (c == NULL)
		detached = 1;

	/* Is this client already attached? */
	already_attached = 0;
	if (c != NULL && c->session != NULL)
		already_attached = 1;

	/*
	 * Save the termios settings, part of which is used for new windows in
	 * this session.
	 *
	 * This is read again with tcgetattr() rather than using tty.tio as if
	 * detached, tty_open won't be called. Because of this, it must be done
	 * before opening the terminal as that calls tcsetattr() to prepare for
	 * tmux taking over.
	 */
	if (!detached && !already_attached && c->tty.fd != -1) {
		if (tcgetattr(c->tty.fd, &tio) != 0)
			fatal("tcgetattr failed");
		tiop = &tio;
	} else
		tiop = NULL;

	/* Open the terminal if necessary. */
	if (!detached && !already_attached) {
		if (server_client_open(c, NULL, &cause) != 0) {
			cmdq_error(cmdq, "open terminal failed: %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	/* Get the new session working directory. */
	if (c != NULL && c->cwd != NULL)
		cwd = c->cwd;
	else {
		pw = getpwuid(getuid());
		if (pw->pw_dir != NULL && *pw->pw_dir != '\0')
			cwd = pw->pw_dir;
		else
			cwd = "/";
	}

	/* Find new session size. */
	if (c != NULL) {
		sx = c->tty.sx;
		sy = c->tty.sy;
	} else {
		sx = 80;
		sy = 24;
	}
	if (detached && args_has(args, 'x')) {
		sx = strtonum(args_get(args, 'x'), 1, USHRT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(cmdq, "width %s", errstr);
			return (CMD_RETURN_ERROR);
		}
	}
	if (detached && args_has(args, 'y')) {
		sy = strtonum(args_get(args, 'y'), 1, USHRT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(cmdq, "height %s", errstr);
			return (CMD_RETURN_ERROR);
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
	if (c != NULL)
		environ_update(update, &c->environ, &env);

	/* Create the new session. */
	idx = -1 - options_get_number(&global_s_options, "base-index");
	s = session_create(newname, cmd, cwd, &env, tiop, idx, sx, sy, &cause);
	if (s == NULL) {
		cmdq_error(cmdq, "create session failed: %s", cause);
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
		if (!already_attached)
			server_write_ready(c);
		else if (c->session != NULL)
			c->last_session = c->session;
		c->session = s;
		notify_attached_session_changed(c);
		session_update_activity(s);
		server_redraw_client(c);
	}
	recalculate_sizes();
	server_update_socket();

	/*
	 * If there are still configuration file errors to display, put the new
	 * session's current window into more mode and display them now.
	 */
	if (cfg_finished)
		cfg_show_causes(s);

	/* Print if requested. */
	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = NEW_SESSION_TEMPLATE;

		ft = format_create();
		if ((c = cmd_find_client(cmdq, NULL, 1)) != NULL)
			format_client(ft, c);
		format_session(ft, s);

		cp = format_expand(ft, template);
		cmdq_print(cmdq, "%s", cp);
		free(cp);

		format_free(ft);
	}

	if (!detached)
		cmdq->client_exit = 0;
	return (CMD_RETURN_NORMAL);
}
