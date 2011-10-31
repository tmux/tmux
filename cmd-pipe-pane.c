/* $Id$ */

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
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Open pipe to redirect pane output. If already open, close first.
 */

int	cmd_pipe_pane_exec(struct cmd *, struct cmd_ctx *);

void	cmd_pipe_pane_error_callback(struct bufferevent *, short, void *);

const struct cmd_entry cmd_pipe_pane_entry = {
	"pipe-pane", "pipep",
	"ot:", 0, 1,
	"[-o] " CMD_TARGET_PANE_USAGE " [command]",
	0,
	NULL,
	NULL,
	cmd_pipe_pane_exec
};

int
cmd_pipe_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct client		*c;
	struct window_pane	*wp;
	char			*command;
	int			 old_fd, pipe_fd[2], null_fd;

	if (cmd_find_pane(ctx, args_get(args, 't'), NULL, &wp) == NULL)
		return (-1);
	c = cmd_find_client(ctx, NULL);

	/* Destroy the old pipe. */
	old_fd = wp->pipe_fd;
	if (wp->pipe_fd != -1) {
		bufferevent_free(wp->pipe_event);
		close(wp->pipe_fd);
		wp->pipe_fd = -1;
	}

	/* If no pipe command, that is enough. */
	if (args->argc == 0 || *args->argv[0] == '\0')
		return (0);

	/*
	 * With -o, only open the new pipe if there was no previous one. This
	 * allows a pipe to be toggled with a single key, for example:
	 *
	 *	bind ^p pipep -o 'cat >>~/output'
	 */
	if (args_has(self->args, 'o') && old_fd != -1)
		return (0);

	/* Open the new pipe. */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_fd) != 0) {
		ctx->error(ctx, "socketpair error: %s", strerror(errno));
		return (-1);
	}

	/* Fork the child. */
	switch (fork()) {
	case -1:
		ctx->error(ctx, "fork error: %s", strerror(errno));
		return (-1);
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

		command = status_replace(
		    c, NULL, NULL, NULL, args->argv[0], time(NULL), 0);
		execl(_PATH_BSHELL, "sh", "-c", command, (char *) NULL);
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
		return (0);
	}
}

/* ARGSUSED */
void
cmd_pipe_pane_error_callback(
    unused struct bufferevent *bufev, unused short what, void *data)
{
	struct window_pane	*wp = data;

	bufferevent_free(wp->pipe_event);
	close(wp->pipe_fd);
	wp->pipe_fd = -1;
}
