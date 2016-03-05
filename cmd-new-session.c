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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Create a new session and attach to the current terminal unless -d is given.
 */

#define NEW_SESSION_TEMPLATE "#{session_name}:"

enum cmd_retval	 cmd_new_session_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_new_session_entry = {
	.name = "new-session",
	.alias = "new",

	.args = { "Ac:dDEF:n:Ps:t:x:y:", 0, -1 },
	.usage = "[-AdDEP] [-c start-directory] [-F format] [-n window-name] "
		 "[-s session-name] " CMD_TARGET_SESSION_USAGE " [-x width] "
		 "[-y height] [command]",

	.tflag = CMD_SESSION_CANFAIL,

	.flags = CMD_STARTSERVER,
	.exec = cmd_new_session_exec
};

const struct cmd_entry cmd_has_session_entry = {
	.name = "has-session",
	.alias = "has",

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_SESSION_USAGE,

	.tflag = CMD_SESSION,

	.flags = 0,
	.exec = cmd_new_session_exec
};

enum cmd_retval
cmd_new_session_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c = cmdq->client;
	struct session		*s, *as;
	struct session		*groupwith = cmdq->state.tflag.s;
	struct window		*w;
	struct environ		*env;
	struct termios		 tio, *tiop;
	const char		*newname, *target, *update, *errstr, *template;
	const char		*path, *cwd, *to_free = NULL;
	char		       **argv, *cmd, *cause, *cp;
	int			 detached, already_attached, idx, argc;
	u_int			 sx, sy;
	struct format_tree	*ft;
	struct environ_entry	*envent;

	if (self->entry == &cmd_has_session_entry) {
		/*
		 * cmd_prepare() will fail if the session cannot be found,
		 * hence always return success here.
		 */
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 't') && (args->argc != 0 || args_has(args, 'n'))) {
		cmdq_error(cmdq, "command or window name given with target");
		return (CMD_RETURN_ERROR);
	}

	newname = args_get(args, 's');
	if (newname != NULL) {
		if (!session_check_name(newname)) {
			cmdq_error(cmdq, "bad session name: %s", newname);
			return (CMD_RETURN_ERROR);
		}
		if ((as = session_find(newname)) != NULL) {
			if (args_has(args, 'A')) {
				/*
				 * This cmdq is now destined for
				 * attach-session.  Because attach-session
				 * will have already been prepared, copy this
				 * session into its tflag so it can be used.
				 */
				cmd_find_from_session(&cmdq->state.tflag, as);
				return (cmd_attach_session(cmdq,
				    args_has(args, 'D'), 0, NULL,
				    args_has(args, 'E')));
			}
			cmdq_error(cmdq, "duplicate session: %s", newname);
			return (CMD_RETURN_ERROR);
		}
	}

	if ((target = args_get(args, 't')) != NULL) {
		if (groupwith == NULL) {
			cmdq_error(cmdq, "no such session: %s", target);
			goto error;
		}
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

	/* Get the new session working directory. */
	if (args_has(args, 'c')) {
		ft = format_create(cmdq, 0);
		format_defaults(ft, c, NULL, NULL, NULL);
		to_free = cwd = format_expand(ft, args_get(args, 'c'));
		format_free(ft);
	} else if (c != NULL && c->session == NULL && c->cwd != NULL)
		cwd = c->cwd;
	else
		cwd = ".";

	/*
	 * If this is a new client, check for nesting and save the termios
	 * settings (part of which is used for new windows in this session).
	 *
	 * tcgetattr() is used rather than using tty.tio since if the client is
	 * detached, tty_open won't be called. It must be done before opening
	 * the terminal as that calls tcsetattr() to prepare for tmux taking
	 * over.
	 */
	if (!detached && !already_attached && c->tty.fd != -1) {
		if (server_client_check_nested(cmdq->client)) {
			cmdq_error(cmdq, "sessions should be nested with care, "
			    "unset $TMUX to force");
			return (CMD_RETURN_ERROR);
		}
		if (tcgetattr(c->tty.fd, &tio) != 0)
			fatal("tcgetattr failed");
		tiop = &tio;
	} else
		tiop = NULL;

	/* Open the terminal if necessary. */
	if (!detached && !already_attached) {
		if (server_client_open(c, &cause) != 0) {
			cmdq_error(cmdq, "open terminal failed: %s", cause);
			free(cause);
			goto error;
		}
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
			goto error;
		}
	}
	if (detached && args_has(args, 'y')) {
		sy = strtonum(args_get(args, 'y'), 1, USHRT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(cmdq, "height %s", errstr);
			goto error;
		}
	}
	if (sy > 0 && options_get_number(global_s_options, "status"))
		sy--;
	if (sx == 0)
		sx = 1;
	if (sy == 0)
		sy = 1;

	/* Figure out the command for the new window. */
	argc = -1;
	argv = NULL;
	if (!args_has(args, 't') && args->argc != 0) {
		argc = args->argc;
		argv = args->argv;
	} else if (groupwith == NULL) {
		cmd = options_get_string(global_s_options, "default-command");
		if (cmd != NULL && *cmd != '\0') {
			argc = 1;
			argv = &cmd;
		} else {
			argc = 0;
			argv = NULL;
		}
	}

	path = NULL;
	if (c != NULL && c->session == NULL)
		envent = environ_find(c->environ, "PATH");
	else
		envent = environ_find(global_environ, "PATH");
	if (envent != NULL)
		path = envent->value;

	/* Construct the environment. */
	env = environ_create();
	if (c != NULL && !args_has(args, 'E')) {
		update = options_get_string(global_s_options,
		    "update-environment");
		environ_update(update, c->environ, env);
	}

	/* Create the new session. */
	idx = -1 - options_get_number(global_s_options, "base-index");
	s = session_create(newname, argc, argv, path, cwd, env, tiop, idx, sx,
	    sy, &cause);
	environ_free(env);
	if (s == NULL) {
		cmdq_error(cmdq, "create session failed: %s", cause);
		free(cause);
		goto error;
	}

	/* Set the initial window name if one given. */
	if (argc >= 0 && args_has(args, 'n')) {
		w = s->curw->window;
		window_set_name(w, args_get(args, 'n'));
		options_set_number(w->options, "automatic-rename", 0);
	}

	/*
	 * If a target session is given, this is to be part of a session group,
	 * so add it to the group and synchronize.
	 */
	if (groupwith != NULL) {
		session_group_add(groupwith, s);
		session_group_synchronize_to(s);
		session_select(s, RB_MIN(winlinks, &s->windows)->idx);
	}

	/*
	 * Set the client to the new session. If a command client exists, it is
	 * taking this session and needs to get MSG_READY and stay around.
	 */
	if (!detached) {
		if (!already_attached) {
			if (~c->flags & CLIENT_CONTROL)
				proc_send(c->peer, MSG_READY, -1, NULL, 0);
		} else if (c->session != NULL)
			c->last_session = c->session;
		c->session = s;
		server_client_set_key_table(c, NULL);
		status_timer_start(c);
		notify_attached_session_changed(c);
		session_update_activity(s, NULL);
		gettimeofday(&s->last_attached_time, NULL);
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

		ft = format_create(cmdq, 0);
		format_defaults(ft, c, s, NULL, NULL);

		cp = format_expand(ft, template);
		cmdq_print(cmdq, "%s", cp);
		free(cp);

		format_free(ft);
	}

	if (!detached)
		cmdq->client_exit = 0;

	if (to_free != NULL)
		free((void *)to_free);
	return (CMD_RETURN_NORMAL);

error:
	if (to_free != NULL)
		free((void *)to_free);
	return (CMD_RETURN_ERROR);
}
