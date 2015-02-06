/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Open pipe to redirect pane output. If already open, close first.
 */

enum cmd_retval	 cmd_pipe_pane_exec(struct cmd *, struct cmd_q *);

void	cmd_pipe_pane_error_callback(struct bufferevent *, short, void *);

const struct cmd_entry cmd_pipe_pane_entry = {
	"pipe-pane", "pipep",
	"ot:", 0, 1,
	"[-o] " CMD_TARGET_PANE_USAGE " [command]",
	0,
	cmd_pipe_pane_exec
};

enum cmd_retval
cmd_pipe_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c;
	struct session		*s;
	struct winlink		*wl;
	struct window_pane	*wp;
	char			*cmd;
	int			 old_fd, pipe_fd[2], null_fd;
	struct format_tree	*ft;

	if ((wl = cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp)) == NULL)
		return (CMD_RETURN_ERROR);
	c = cmd_find_client(cmdq, NULL, 1);

	/* Destroy the old pipe. */
	old_fd = wp->pipe_fd;
	if (wp->pipe_fd != -1) {
		bufferevent_free(wp->pipe_event);
		close(wp->pipe_fd);
		wp->pipe_fd = -1;
	}

	/* If no pipe command, that is enough. */
	if (args->argc == 0 || *args->argv[0] == '\0')
		return (CMD_RETURN_NORMAL);

	/*
	 * With -o, only open the new pipe if there was no previous one. This
	 * allows a pipe to be toggled with a single key, for example:
	 *
	 *	bind ^p pipep -o 'cat >>~/output'
	 */
	if (args_has(self->args, 'o') && old_fd != -1)
		return (CMD_RETURN_NORMAL);

	/* Open the new pipe. */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_fd) != 0) {
		cmdq_error(cmdq, "socketpair error: %s", strerror(errno));
		return (CMD_RETURN_ERROR);
	}

	/* Expand the command. */
	ft = format_create();
	format_defaults(ft, c, s, wl, wp);
	cmd = format_expand_time(ft, args->argv[0], time(NULL));
	format_free(ft);

	/* Fork the child. */
	switch (fork()) {
	case -1:
		cmdq_error(cmdq, "fork error: %s", strerror(errno));

		free(cmd);
		return (CMD_RETURN_ERROR);
	case 0:
		/* Child process. */
		close(pipe_fd[0]);
		clear_signals(1);

		if (dup2(pipe_fd[1], STDIN_FILENO) == -1)
			_exit(1);
		if (pipe_fd[1] != STDIN_FILENO)
			close(pipe_fd[1]);

		null_fd = open(_PATH_DEVNULL, O_WRONLY, 0);
		if (dup2(null_fd, STDOUT_FILENO) == -1)
			_exit(1);
		if (dup2(null_fd, STDERR_FILENO) == -1)
			_exit(1);
		if (null_fd != STDOUT_FILENO && null_fd != STDERR_FILENO)
			close(null_fd);

		closefrom(STDERR_FILENO + 1);

		execl(_PATH_BSHELL, "sh", "-c", cmd, (char *) NULL);
		_exit(1);
	default:
		/* Parent process. */
		close(pipe_fd[1]);

		wp->pipe_fd = pipe_fd[0];
		wp->pipe_off = EVBUFFER_LENGTH(wp->event->input);

		wp->pipe_event = bufferevent_new(wp->pipe_fd,
		    NULL, NULL, cmd_pipe_pane_error_callback, wp);
		bufferevent_enable(wp->pipe_event, EV_WRITE);

		setblocking(wp->pipe_fd, 0);

		free(cmd);
		return (CMD_RETURN_NORMAL);
	}
}

void
cmd_pipe_pane_error_callback(
    unused struct bufferevent *bufev, unused short what, void *data)
{
	struct window_pane	*wp = data;

	bufferevent_free(wp->pipe_event);
	close(wp->pipe_fd);
	wp->pipe_fd = -1;
}
