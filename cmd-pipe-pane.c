/* $Id: cmd-pipe-pane.c,v 1.4 2009-11-08 22:40:36 tcunha Exp $ */

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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Open pipe to redirect pane output. If already open, close first.
 */

int	cmd_pipe_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_pipe_pane_entry = {
	"pipe-pane", "pipep",
	CMD_TARGET_PANE_USAGE "[-o] [command]",
	CMD_ARG01, CMD_CHFLAG('o'),
	cmd_target_init,
	cmd_target_parse,
	cmd_pipe_pane_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_pipe_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct window_pane	*wp;
	int			 old_fd, pipe_fd[2], null_fd, mode;

	if (cmd_find_pane(ctx, data->target, NULL, &wp) == NULL)
		return (-1);

	/* Destroy the old pipe. */
	old_fd = wp->pipe_fd;
	if (wp->pipe_fd != -1) {
		buffer_destroy(wp->pipe_buf);
		close(wp->pipe_fd);
		wp->pipe_fd = -1;
	}

	/* If no pipe command, that is enough. */
	if (data->arg == NULL || *data->arg == '\0')
		return (0);

	/*
	 * With -o, only open the new pipe if there was no previous one. This
	 * allows a pipe to be toggled with a single key, for example:
	 *
	 *	bind ^p pipep -o 'cat >>~/output'
	 */
	if (data->chflags & CMD_CHFLAG('o') && old_fd != -1)
		return (0);

	/* Open the new pipe. */
	if (pipe(pipe_fd) != 0) {
		ctx->error(ctx, "pipe error: %s", strerror(errno));
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
		server_signal_clear();

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

		execl(_PATH_BSHELL, "sh", "-c", data->arg, (char *) NULL);
		_exit(1);
	default:
		/* Parent process. */
		close(pipe_fd[1]);

		wp->pipe_fd = pipe_fd[0];
		wp->pipe_buf = buffer_create(BUFSIZ);
		wp->pipe_off = BUFFER_USED(wp->in);
		
		if ((mode = fcntl(wp->pipe_fd, F_GETFL)) == -1)
			fatal("fcntl failed");
		if (fcntl(wp->pipe_fd, F_SETFL, mode|O_NONBLOCK) == -1)
			fatal("fcntl failed");
		if (fcntl(wp->pipe_fd, F_SETFD, FD_CLOEXEC) == -1)
			fatal("fcntl failed");	
		return (0);
	}

	return (0);
}
