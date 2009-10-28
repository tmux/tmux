/* $Id: server-client.c,v 1.10 2009-10-28 23:14:15 tcunha Exp $ */

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

#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

void	server_client_handle_data(struct client *);
void	server_client_check_redraw(struct client *);
void	server_client_set_title(struct client *);
void	server_client_check_timers(struct client *);

int	server_client_msg_dispatch(struct client *);
void	server_client_msg_command(struct client *, struct msg_command_data *);
void	server_client_msg_identify(
    	    struct client *, struct msg_identify_data *, int);
void	server_client_msg_shell(struct client *);

void printflike2 server_client_msg_error(struct cmd_ctx *, const char *, ...);
void printflike2 server_client_msg_print(struct cmd_ctx *, const char *, ...);
void printflike2 server_client_msg_info(struct cmd_ctx *, const char *, ...);


/* Create a new client. */
void
server_client_create(int fd)
{
	struct client	*c;
	int		 mode;
	u_int		 i;

	if ((mode = fcntl(fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");

	c = xcalloc(1, sizeof *c);
	c->references = 0;
	imsg_init(&c->ibuf, fd);
	
	if (gettimeofday(&c->tv, NULL) != 0)
		fatal("gettimeofday failed");

	ARRAY_INIT(&c->prompt_hdata);

	c->tty.fd = -1;
	c->title = NULL;

	c->session = NULL;
	c->tty.sx = 80;
	c->tty.sy = 24;

	screen_init(&c->status, c->tty.sx, 1, 0);
	job_tree_init(&c->status_jobs);

	c->message_string = NULL;

	c->prompt_string = NULL;
	c->prompt_buffer = NULL;
	c->prompt_index = 0;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == NULL) {
			ARRAY_SET(&clients, i, c);
			return;
		}
	}
	ARRAY_ADD(&clients, c);
	log_debug("new client %d", fd);
}

/* Lost a client. */
void
server_client_lost(struct client *c)
{
	u_int	i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if (ARRAY_ITEM(&clients, i) == c)
			ARRAY_SET(&clients, i, NULL);
	}
	log_debug("lost client %d", c->ibuf.fd);

	/*
	 * If CLIENT_TERMINAL hasn't been set, then tty_init hasn't been called
	 * and tty_free might close an unrelated fd.
	 */
	if (c->flags & CLIENT_TERMINAL)
		tty_free(&c->tty);

	screen_free(&c->status);
	job_tree_free(&c->status_jobs);

	if (c->title != NULL)
		xfree(c->title);

	if (c->message_string != NULL)
		xfree(c->message_string);

	if (c->prompt_string != NULL)
		xfree(c->prompt_string);
	if (c->prompt_buffer != NULL)
		xfree(c->prompt_buffer);
	for (i = 0; i < ARRAY_LENGTH(&c->prompt_hdata); i++)
		xfree(ARRAY_ITEM(&c->prompt_hdata, i));
	ARRAY_FREE(&c->prompt_hdata);

	if (c->cwd != NULL)
		xfree(c->cwd);

	close(c->ibuf.fd);
	imsg_clear(&c->ibuf);

	for (i = 0; i < ARRAY_LENGTH(&dead_clients); i++) {
		if (ARRAY_ITEM(&dead_clients, i) == NULL) {
			ARRAY_SET(&dead_clients, i, c);
			break;
		}
	}
	if (i == ARRAY_LENGTH(&dead_clients))
		ARRAY_ADD(&dead_clients, c);
	c->flags |= CLIENT_DEAD;

	recalculate_sizes();
}

/* Register clients for poll. */
void
server_client_prepare(void)
{
	struct client	*c;
	u_int		 i;
	int		 events;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		if ((c = ARRAY_ITEM(&clients, i)) == NULL)
			continue;

		events = 0;
		if (!(c->flags & CLIENT_BAD))
			events |= POLLIN;
		if (c->ibuf.w.queued > 0)
			events |= POLLOUT;
		server_poll_add(c->ibuf.fd, events, server_client_callback, c);

		if (c->tty.fd == -1)
			continue;
		if (c->flags & CLIENT_SUSPENDED || c->session == NULL)
			continue;
		events = POLLIN;
		if (BUFFER_USED(c->tty.out) > 0)
			events |= POLLOUT;
		server_poll_add(c->tty.fd, events, server_client_callback, c);
	}
}

/* Process a single client event. */
void
server_client_callback(int fd, int events, void *data)
{
	struct client	*c = data;

	if (c->flags & CLIENT_DEAD)
		return;

	if (fd == c->ibuf.fd) {
		if (events & (POLLERR|POLLNVAL|POLLHUP))
			goto client_lost;

		if (events & POLLOUT && msgbuf_write(&c->ibuf.w) < 0)
			goto client_lost;

		if (c->flags & CLIENT_BAD) {
			if (c->ibuf.w.queued == 0)
				goto client_lost;
			return;
		}

		if (events & POLLIN && server_client_msg_dispatch(c) != 0)
			goto client_lost;
	}

	if (c->tty.fd != -1 && fd == c->tty.fd) {
		if (c->flags & CLIENT_SUSPENDED || c->session == NULL)
			return;

		if (buffer_poll(fd, events, c->tty.in, c->tty.out) != 0)
			goto client_lost;
	}

	return;

client_lost:
	server_client_lost(c);
}

/* Client functions that need to happen every loop. */
void
server_client_loop(void)
{
	struct client		*c;
	struct window		*w;
	struct window_pane	*wp;
	u_int		 	 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		server_client_handle_data(c);
		if (c->session != NULL) {
			server_client_check_timers(c);
			server_client_check_redraw(c);
		}
	}

	/*
	 * Any windows will have been redrawn as part of clients, so clear
	 * their flags now.
	 */
	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		w->flags &= ~WINDOW_REDRAW;
		TAILQ_FOREACH(wp, &w->panes, entry)
			wp->flags &= ~PANE_REDRAW;
	}
}

/* Handle data input or output from client. */
void
server_client_handle_data(struct client *c)
{
	struct window		*w;
	struct window_pane	*wp;
	struct screen		*s;
	struct options		*oo;
	struct timeval	 	 tv;
	struct key_binding	*bd;
	struct keylist		*keylist;
	struct mouse_event	 mouse;
	int		 	 key, status, xtimeout, mode, isprefix;
	u_int			 i;

	xtimeout = options_get_number(&c->session->options, "repeat-time");
	if (xtimeout != 0 && c->flags & CLIENT_REPEAT) {
		if (gettimeofday(&tv, NULL) != 0)
			fatal("gettimeofday failed");
		if (timercmp(&tv, &c->repeat_timer, >))
			c->flags &= ~(CLIENT_PREFIX|CLIENT_REPEAT);
	}

	/* Process keys. */
	keylist = options_get_data(&c->session->options, "prefix");
	while (tty_keys_next(&c->tty, &key, &mouse) == 0) {
		if (c->session == NULL)
			return;

		c->session->activity = time(NULL);
		w = c->session->curw->window;
		wp = w->active;	/* could die */
		oo = &c->session->options;

		/* Special case: number keys jump to pane in identify mode. */
		if (c->flags & CLIENT_IDENTIFY && key >= '0' && key <= '9') {	
			wp = window_pane_at_index(w, key - '0');
			if (wp != NULL && window_pane_visible(wp))
				window_set_active_pane(w, wp);
			server_clear_identify(c);
			continue;
		}
		
		status_message_clear(c);
		server_clear_identify(c);
		if (c->prompt_string != NULL) {
			status_prompt_key(c, key);
			continue;
		}

		/* Check for mouse keys. */
		if (key == KEYC_MOUSE) {
			if (options_get_number(oo, "mouse-select-pane")) {
				window_set_active_at(w, mouse.x, mouse.y);
				wp = w->active;
			}
			window_pane_mouse(wp, c, &mouse);
			continue;
		}

		/* Is this a prefix key? */
		isprefix = 0;
		for (i = 0; i < ARRAY_LENGTH(keylist); i++) {
			if (key == ARRAY_ITEM(keylist, i)) {
				isprefix = 1;
				break;
			}
		}

		/* No previous prefix key. */
		if (!(c->flags & CLIENT_PREFIX)) {
			if (isprefix)
				c->flags |= CLIENT_PREFIX;
			else {
				/* Try as a non-prefix key binding. */
				if ((bd = key_bindings_lookup(key)) == NULL)
					window_pane_key(wp, c, key);
				else
					key_bindings_dispatch(bd, c);
			}
			continue;
		}

		/* Prefix key already pressed. Reset prefix and lookup key. */
		c->flags &= ~CLIENT_PREFIX;
		if ((bd = key_bindings_lookup(key | KEYC_PREFIX)) == NULL) {
			/* If repeating, treat this as a key, else ignore. */
			if (c->flags & CLIENT_REPEAT) {
				c->flags &= ~CLIENT_REPEAT;
				if (isprefix)
					c->flags |= CLIENT_PREFIX;
				else
					window_pane_key(wp, c, key);
			}
			continue;
		}

		/* If already repeating, but this key can't repeat, skip it. */
		if (c->flags & CLIENT_REPEAT && !bd->can_repeat) {
			c->flags &= ~CLIENT_REPEAT;
			if (isprefix)
				c->flags |= CLIENT_PREFIX;
			else
				window_pane_key(wp, c, key);
			continue;
		}

		/* If this key can repeat, reset the repeat flags and timer. */
		if (xtimeout != 0 && bd->can_repeat) {
			c->flags |= CLIENT_PREFIX|CLIENT_REPEAT;

			tv.tv_sec = xtimeout / 1000;
			tv.tv_usec = (xtimeout % 1000) * 1000L;
			if (gettimeofday(&c->repeat_timer, NULL) != 0)
				fatal("gettimeofday failed");
			timeradd(&c->repeat_timer, &tv, &c->repeat_timer);
		}

		/* Dispatch the command. */
		key_bindings_dispatch(bd, c);
	}
	if (c->session == NULL)
		return;
	w = c->session->curw->window;
	wp = w->active;
	oo = &c->session->options;
	s = wp->screen;

	/*
	 * Update cursor position and mode settings. The scroll region and
	 * attributes are cleared across poll(2) as this is the most likely
	 * time a user may interrupt tmux, for example with ~^Z in ssh(1). This
	 * is a compromise between excessive resets and likelihood of an
	 * interrupt.
	 *
	 * tty_region/tty_reset/tty_update_mode already take care of not
	 * resetting things that are already in their default state.
	 */
	tty_region(&c->tty, 0, c->tty.sy - 1);

	status = options_get_number(oo, "status");
	if (!window_pane_visible(wp) || wp->yoff + s->cy >= c->tty.sy - status)
		tty_cursor(&c->tty, 0, 0);
	else
		tty_cursor(&c->tty, wp->xoff + s->cx, wp->yoff + s->cy);

	mode = s->mode;
	if (TAILQ_NEXT(TAILQ_FIRST(&w->panes), entry) != NULL &&
	    options_get_number(oo, "mouse-select-pane"))
		mode |= MODE_MOUSE;
	tty_update_mode(&c->tty, mode);
	tty_reset(&c->tty);
}

/* Check for client redraws. */
void
server_client_check_redraw(struct client *c)
{
	struct session		*s = c->session;
	struct window_pane	*wp;
	int		 	 flags, redraw;

	flags = c->tty.flags & TTY_FREEZE;
	c->tty.flags &= ~TTY_FREEZE;

	if (c->flags & (CLIENT_REDRAW|CLIENT_STATUS)) {
		if (options_get_number(&s->options, "set-titles"))
			server_client_set_title(c);
	
		if (c->message_string != NULL)
			redraw = status_message_redraw(c);
		else if (c->prompt_string != NULL)
			redraw = status_prompt_redraw(c);
		else
			redraw = status_redraw(c);
		if (!redraw)
			c->flags &= ~CLIENT_STATUS;
	}

	if (c->flags & CLIENT_REDRAW) {
		screen_redraw_screen(c, 0);
		c->flags &= ~CLIENT_STATUS;
	} else {
		TAILQ_FOREACH(wp, &c->session->curw->window->panes, entry) {
			if (wp->flags & PANE_REDRAW)
				screen_redraw_pane(c, wp);
		}
	}

	if (c->flags & CLIENT_STATUS)
		screen_redraw_screen(c, 1);

	c->tty.flags |= flags;

	c->flags &= ~(CLIENT_REDRAW|CLIENT_STATUS);
}

/* Set client title. */
void
server_client_set_title(struct client *c)
{
	struct session	*s = c->session;
	const char	*template;
	char		*title;

	template = options_get_string(&s->options, "set-titles-string");
	
	title = status_replace(c, template, time(NULL));
	if (c->title == NULL || strcmp(title, c->title) != 0) {
		if (c->title != NULL)
			xfree(c->title);
		c->title = xstrdup(title);
		tty_set_title(&c->tty, c->title);
	}
	xfree(title);
}

/* Check client timers. */
void
server_client_check_timers(struct client *c)
{
	struct session	*s = c->session;
	struct job	*job;
	struct timeval	 tv;
	u_int		 interval;

	if (gettimeofday(&tv, NULL) != 0)
		fatal("gettimeofday failed");

	if (c->flags & CLIENT_IDENTIFY && timercmp(&tv, &c->identify_timer, >))
		server_clear_identify(c);

	if (c->message_string != NULL && timercmp(&tv, &c->message_timer, >))
		status_message_clear(c);

	if (c->message_string != NULL || c->prompt_string != NULL) {
		/*
		 * Don't need timed redraw for messages/prompts so bail now.
		 * The status timer isn't reset when they are redrawn anyway.
		 */
		return;

	}
	if (!options_get_number(&s->options, "status"))
		return;

	/* Check timer; resolution is only a second so don't be too clever. */
	interval = options_get_number(&s->options, "status-interval");
	if (interval == 0)
		return;
	if (tv.tv_sec < c->status_timer.tv_sec ||
	    ((u_int) tv.tv_sec) - c->status_timer.tv_sec >= interval) {
		/* Run the jobs for this client and schedule for redraw. */
		RB_FOREACH(job, jobs, &c->status_jobs)
			job_run(job);
		c->flags |= CLIENT_STATUS;
	}
}

/* Dispatch message from client. */
int
server_client_msg_dispatch(struct client *c)
{
	struct imsg		 imsg;
	struct msg_command_data	 commanddata;
	struct msg_identify_data identifydata;
	struct msg_environ_data	 environdata;
	ssize_t			 n, datalen;

	if ((n = imsg_read(&c->ibuf)) == -1 || n == 0)
		return (-1);

	for (;;) {
		if ((n = imsg_get(&c->ibuf, &imsg)) == -1)
			return (-1);
		if (n == 0)
			return (0);
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		if (imsg.hdr.peerid != PROTOCOL_VERSION) {
			server_write_client(c, MSG_VERSION, NULL, 0);
			c->flags |= CLIENT_BAD;
			imsg_free(&imsg);
			continue;
		}

		log_debug("got %d from client %d", imsg.hdr.type, c->ibuf.fd);
		switch (imsg.hdr.type) {
		case MSG_COMMAND:
			if (datalen != sizeof commanddata)
				fatalx("bad MSG_COMMAND size");
			memcpy(&commanddata, imsg.data, sizeof commanddata);

			server_client_msg_command(c, &commanddata);
			break;
		case MSG_IDENTIFY:
			if (datalen != sizeof identifydata)
				fatalx("bad MSG_IDENTIFY size");
			if (imsg.fd == -1)
				fatalx("MSG_IDENTIFY missing fd");
			memcpy(&identifydata, imsg.data, sizeof identifydata);

			server_client_msg_identify(c, &identifydata, imsg.fd);
			break;
		case MSG_RESIZE:
			if (datalen != 0)
				fatalx("bad MSG_RESIZE size");

			tty_resize(&c->tty);
			recalculate_sizes();
			server_redraw_client(c);
			break;
		case MSG_EXITING:
			if (datalen != 0)
				fatalx("bad MSG_EXITING size");

			c->session = NULL;
			tty_close(&c->tty);
			server_write_client(c, MSG_EXITED, NULL, 0);
			break;
		case MSG_WAKEUP:
		case MSG_UNLOCK:
			if (datalen != 0)
				fatalx("bad MSG_WAKEUP size");

			if (!(c->flags & CLIENT_SUSPENDED))
				break;
			c->flags &= ~CLIENT_SUSPENDED;
			tty_start_tty(&c->tty);
			server_redraw_client(c);
			recalculate_sizes();
			if (c->session != NULL)
				c->session->activity = time(NULL);
			break;
		case MSG_ENVIRON:
			if (datalen != sizeof environdata)
				fatalx("bad MSG_ENVIRON size");
			memcpy(&environdata, imsg.data, sizeof environdata);

			environdata.var[(sizeof environdata.var) - 1] = '\0';
			if (strchr(environdata.var, '=') != NULL)
				environ_put(&c->environ, environdata.var);
			break;
		case MSG_SHELL:
			if (datalen != 0)
				fatalx("bad MSG_SHELL size");

			server_client_msg_shell(c);
			break;
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
	}
}

/* Callback to send error message to client. */
void printflike2
server_client_msg_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_ERROR, &data, sizeof data);
}

/* Callback to send print message to client. */
void printflike2
server_client_msg_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_PRINT, &data, sizeof data);
}

/* Callback to send print message to client, if not quiet. */
void printflike2
server_client_msg_info(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct msg_print_data	data;
	va_list			ap;

	if (be_quiet)
		return;

	va_start(ap, fmt);
	xvsnprintf(data.msg, sizeof data.msg, fmt, ap);
	va_end(ap);

	server_write_client(ctx->cmdclient, MSG_PRINT, &data, sizeof data);
}

/* Handle command message. */
void
server_client_msg_command(struct client *c, struct msg_command_data *data)
{
	struct cmd_ctx	 ctx;
	struct cmd_list	*cmdlist = NULL;
	struct cmd	*cmd;
	int		 argc;
	char	       **argv, *cause;

	if (c->session != NULL)
		c->session->activity = time(NULL);

	ctx.error = server_client_msg_error;
	ctx.print = server_client_msg_print;
	ctx.info = server_client_msg_info;

	ctx.msgdata = data;
	ctx.curclient = NULL;

	ctx.cmdclient = c;

	argc = data->argc;
	data->argv[(sizeof data->argv) - 1] = '\0';
	if (cmd_unpack_argv(data->argv, sizeof data->argv, argc, &argv) != 0) {
		server_client_msg_error(&ctx, "command too long");
		goto error;
	}

	if (argc == 0) {
		argc = 1;
		argv = xcalloc(1, sizeof *argv);
		*argv = xstrdup("new-session");
	}

	if ((cmdlist = cmd_list_parse(argc, argv, &cause)) == NULL) {
		server_client_msg_error(&ctx, "%s", cause);
		cmd_free_argv(argc, argv);
		goto error;
	}
	cmd_free_argv(argc, argv);

	if (data->pid != -1) {
		TAILQ_FOREACH(cmd, cmdlist, qentry) {
			if (cmd->entry->flags & CMD_CANTNEST) {
				server_client_msg_error(&ctx,
				    "sessions should be nested with care. "
				    "unset $TMUX to force");
				goto error;
			}
		}
	}

	if (cmd_list_exec(cmdlist, &ctx) != 1)
		server_write_client(c, MSG_EXIT, NULL, 0);
	cmd_list_free(cmdlist);
	return;

error:
	if (cmdlist != NULL)
		cmd_list_free(cmdlist);
	server_write_client(c, MSG_EXIT, NULL, 0);
}

/* Handle identify message. */
void
server_client_msg_identify(
    struct client *c, struct msg_identify_data *data, int fd)
{
	c->cwd = NULL;
	data->cwd[(sizeof data->cwd) - 1] = '\0';
	if (*data->cwd != '\0')
		c->cwd = xstrdup(data->cwd);

	data->term[(sizeof data->term) - 1] = '\0';
	tty_init(&c->tty, fd, data->term);
	if (data->flags & IDENTIFY_UTF8)
		c->tty.flags |= TTY_UTF8;
	if (data->flags & IDENTIFY_256COLOURS)
		c->tty.term_flags |= TERM_256COLOURS;
	else if (data->flags & IDENTIFY_88COLOURS)
		c->tty.term_flags |= TERM_88COLOURS;

	tty_resize(&c->tty);

	c->flags |= CLIENT_TERMINAL;
}

/* Handle shell message. */
void
server_client_msg_shell(struct client *c)
{
	struct msg_shell_data	 data;
	const char		*shell;
	
	shell = options_get_string(&global_s_options, "default-shell");

	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;
	if (strlcpy(data.shell, shell, sizeof data.shell) >= sizeof data.shell)
		strlcpy(data.shell, _PATH_BSHELL, sizeof data.shell);
	
	server_write_client(c, MSG_SHELL, &data, sizeof data);
	c->flags |= CLIENT_BAD;	/* it will die after exec */
}
