/* $Id: cmd-split-window.c,v 1.34 2010-01-08 16:31:35 tcunha Exp $ */

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

#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Split a window (add a new pane).
 */

int	cmd_split_window_parse(struct cmd *, int, char **, char **);
int	cmd_split_window_exec(struct cmd *, struct cmd_ctx *);
void	cmd_split_window_free(struct cmd *);
void	cmd_split_window_init(struct cmd *, int);
size_t	cmd_split_window_print(struct cmd *, char *, size_t);

struct cmd_split_window_data {
	char	*target;
	char	*cmd;
	int	 flag_detached;
	int	 flag_horizontal;
	int	 percentage;
	int	 size;
};

const struct cmd_entry cmd_split_window_entry = {
	"split-window", "splitw",
	"[-dhv] [-p percentage|-l size] [-t target-pane] [command]",
	0, "",
	cmd_split_window_init,
	cmd_split_window_parse,
	cmd_split_window_exec,
	cmd_split_window_free,
	cmd_split_window_print
};

void
cmd_split_window_init(struct cmd *self, int key)
{
	struct cmd_split_window_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->cmd = NULL;
	data->flag_detached = 0;
	data->flag_horizontal = 0;
	data->percentage = -1;
	data->size = -1;

	switch (key) {
	case '%':
		data->flag_horizontal = 1;
		break;
	case '"':
		data->flag_horizontal = 0;
		break;
	}
}

int
cmd_split_window_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_split_window_data	*data;
	int				 opt;
	const char			*errstr;

	self->entry->init(self, KEYC_NONE);
	data = self->data;

	while ((opt = getopt(argc, argv, "dhl:p:t:v")) != -1) {
		switch (opt) {
		case 'd':
			data->flag_detached = 1;
			break;
		case 'h':
			data->flag_horizontal = 1;
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		case 'l':
			if (data->percentage != -1 || data->size != -1)
				break;
			data->size = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "size %s", errstr);
				goto error;
			}
			break;
		case 'p':
			if (data->size != -1 || data->percentage != -1)
				break;
			data->percentage = strtonum(optarg, 1, 100, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "percentage %s", errstr);
				goto error;
			}
			break;
		case 'v':
			data->flag_horizontal = 0;
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0 && argc != 1)
		goto usage;

	if (argc == 1)
		data->cmd = xstrdup(argv[0]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

int
cmd_split_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_split_window_data	*data = self->data;
	struct session			*s;
	struct winlink			*wl;
	struct window			*w;
	struct window_pane		*wp, *new_wp = NULL;
	struct environ			 env;
	char		 		*cmd, *cwd, *cause;
	const char			*shell;
	u_int				 hlimit;
	int				 size;
	enum layout_type		 type;
	struct layout_cell		*lc;

	if ((wl = cmd_find_pane(ctx, data->target, &s, &wp)) == NULL)
		return (-1);
	w = wl->window;

	environ_init(&env);
	environ_copy(&global_environ, &env);
	environ_copy(&s->environ, &env);
	server_fill_environ(s, &env);

	cmd = data->cmd;
	if (cmd == NULL)
		cmd = options_get_string(&s->options, "default-command");
	if (ctx->cmdclient == NULL || ctx->cmdclient->cwd == NULL)
		cwd = options_get_string(&s->options, "default-path");
	else
		cwd = ctx->cmdclient->cwd;

	type = LAYOUT_TOPBOTTOM;
	if (data->flag_horizontal)
		type = LAYOUT_LEFTRIGHT;

	size = -1;
	if (data->size != -1)
		size = data->size;
	else if (data->percentage != -1) {
		if (type == LAYOUT_TOPBOTTOM)
			size = (wp->sy * data->percentage) / 100;
		else
			size = (wp->sx * data->percentage) / 100;
	}
	hlimit = options_get_number(&s->options, "history-limit");

	shell = options_get_string(&s->options, "default-shell");
	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;

	if ((lc = layout_split_pane(wp, type, size)) == NULL) {
		cause = xstrdup("pane too small");
		goto error;
	}
	new_wp = window_add_pane(w, hlimit);
	if (window_pane_spawn(
	    new_wp, cmd, shell, cwd, &env, s->tio, &cause) != 0)
		goto error;
	layout_assign_pane(lc, new_wp);

	server_redraw_window(w);

	if (!data->flag_detached) {
		window_set_active_pane(w, new_wp);
		session_select(s, wl->idx);
		server_redraw_session(s);
	} else
		server_status_session(s);

	environ_free(&env);
	return (0);

error:
	environ_free(&env);
	if (new_wp != NULL)
		window_remove_pane(w, new_wp);
	ctx->error(ctx, "create pane failed: %s", cause);
	xfree(cause);
	return (-1);
}

void
cmd_split_window_free(struct cmd *self)
{
	struct cmd_split_window_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
}

size_t
cmd_split_window_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_split_window_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->flag_detached)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->flag_horizontal)
		off += xsnprintf(buf + off, len - off, " -h");
	if (off < len && data->size > 0)
		off += xsnprintf(buf + off, len - off, " -l %d", data->size);
	if (off < len && data->percentage > 0) {
		off += xsnprintf(
		    buf + off, len - off, " -p %d", data->percentage);
	}
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	if (off < len && data->cmd != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->cmd);
	return (off);
}
