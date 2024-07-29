/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
 * Copyright (c) 2009 Nicholas Marriott <nicm@openbsd.org>
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
#include <sys/wait.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Runs a command without a window.
 */

static enum args_parse_type	cmd_run_shell_args_parse(struct args *, u_int,
				    char **);
static enum cmd_retval		cmd_run_shell_exec(struct cmd *,
				    struct cmdq_item *);

static void	cmd_run_shell_timer(int, short, void *);
static void	cmd_run_shell_callback(struct job *);
static void	cmd_run_shell_free(void *);
static void	cmd_run_shell_print(struct job *, const char *);

const struct cmd_entry cmd_run_shell_entry = {
	.name = "run-shell",
	.alias = "run",

	.args = { "bd:Ct:c:", 0, 2, cmd_run_shell_args_parse },
	.usage = "[-bC] [-c start-directory] [-d delay] " CMD_TARGET_PANE_USAGE
	         " [shell-command]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = 0,
	.exec = cmd_run_shell_exec
};

struct cmd_run_shell_data {
	struct client			*client;
	char				*cmd;
	struct args_command_state	*state;
	char				*cwd;
	struct cmdq_item		*item;
	struct session			*s;
	int				 wp_id;
	struct event			 timer;
	int				 flags;
};

static enum args_parse_type
cmd_run_shell_args_parse(struct args *args, __unused u_int idx,
    __unused char **cause)
{
	if (args_has(args, 'C'))
		return (ARGS_PARSE_COMMANDS_OR_STRING);
	return (ARGS_PARSE_STRING);
}

static void
cmd_run_shell_print(struct job *job, const char *msg)
{
	struct cmd_run_shell_data	*cdata = job_get_data(job);
	struct window_pane		*wp = NULL;
	struct cmd_find_state		 fs;
	struct window_mode_entry	*wme;

	if (cdata->wp_id != -1)
		wp = window_pane_find_by_id(cdata->wp_id);
	if (wp == NULL) {
		if (cdata->item != NULL) {
			cmdq_print(cdata->item, "%s", msg);
			return;
		}
		if (cdata->item != NULL && cdata->client != NULL)
			wp = server_client_get_pane(cdata->client);
		if (wp == NULL && cmd_find_from_nothing(&fs, 0) == 0)
			wp = fs.wp;
		if (wp == NULL)
			return;
	}

	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->mode != &window_view_mode)
		window_pane_set_mode(wp, NULL, &window_view_mode, NULL, NULL);
	window_copy_add(wp, 1, "%s", msg);
}

static enum cmd_retval
cmd_run_shell_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct cmd_run_shell_data	*cdata;
	struct client			*c = cmdq_get_client(item);
	struct client			*tc = cmdq_get_target_client(item);
	struct session			*s = target->s;
	struct window_pane		*wp = target->wp;
	const char			*delay, *cmd;
	double				 d;
	struct timeval			 tv;
	char				*end;
	int				 wait = !args_has(args, 'b');

	if ((delay = args_get(args, 'd')) != NULL) {
		d = strtod(delay, &end);
		if (*end != '\0') {
			cmdq_error(item, "invalid delay time: %s", delay);
			return (CMD_RETURN_ERROR);
		}
	} else if (args_count(args) == 0)
		return (CMD_RETURN_NORMAL);

	cdata = xcalloc(1, sizeof *cdata);
	if (!args_has(args, 'C')) {
		cmd = args_string(args, 0);
		if (cmd != NULL)
			cdata->cmd = format_single_from_target(item, cmd);
	} else {
		cdata->state = args_make_commands_prepare(self, item, 0, NULL,
		    wait, 1);
	}

	if (args_has(args, 't') && wp != NULL)
		cdata->wp_id = wp->id;
	else
		cdata->wp_id = -1;

	if (wait) {
		cdata->client = c;
		cdata->item = item;
	} else {
		cdata->client = tc;
		cdata->flags |= JOB_NOWAIT;
	}
	if (cdata->client != NULL)
		cdata->client->references++;
	if (args_has(args, 'c'))
		cdata->cwd = xstrdup(args_get(args, 'c'));
	else
		cdata->cwd = xstrdup(server_client_get_cwd(c, s));

	cdata->s = s;
	if (s != NULL)
		session_add_ref(s, __func__);

	evtimer_set(&cdata->timer, cmd_run_shell_timer, cdata);
	if (delay != NULL) {
		timerclear(&tv);
		tv.tv_sec = (time_t)d;
		tv.tv_usec = (d - (double)tv.tv_sec) * 1000000U;
		evtimer_add(&cdata->timer, &tv);
	} else
		event_active(&cdata->timer, EV_TIMEOUT, 1);

	if (!wait)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static void
cmd_run_shell_timer(__unused int fd, __unused short events, void* arg)
{
	struct cmd_run_shell_data	*cdata = arg;
	struct client			*c = cdata->client;
	const char			*cmd = cdata->cmd;
	struct cmdq_item		*item = cdata->item, *new_item;
	struct cmd_list			*cmdlist;
	char				*error;

	if (cdata->state == NULL) {
		if (cmd == NULL) {
			if (cdata->item != NULL)
				cmdq_continue(cdata->item);
			cmd_run_shell_free(cdata);
			return;
		}
		if (job_run(cmd, 0, NULL, NULL, cdata->s, cdata->cwd, NULL,
		    cmd_run_shell_callback, cmd_run_shell_free, cdata,
		    cdata->flags, -1, -1) == NULL)
			cmd_run_shell_free(cdata);
		return;
	}

	cmdlist = args_make_commands(cdata->state, 0, NULL, &error);
	if (cmdlist == NULL) {
		if (cdata->item == NULL) {
			*error = toupper((u_char)*error);
			status_message_set(c, -1, 1, 0, "%s", error);
		} else
			cmdq_error(cdata->item, "%s", error);
		free(error);
	} else if (item == NULL) {
		new_item = cmdq_get_command(cmdlist, NULL);
		cmdq_append(c, new_item);
	} else {
		new_item = cmdq_get_command(cmdlist, cmdq_get_state(item));
		cmdq_insert_after(item, new_item);
	}

	if (cdata->item != NULL)
		cmdq_continue(cdata->item);
	cmd_run_shell_free(cdata);
}

static void
cmd_run_shell_callback(struct job *job)
{
	struct cmd_run_shell_data	*cdata = job_get_data(job);
	struct bufferevent		*event = job_get_event(job);
	struct cmdq_item		*item = cdata->item;
	char				*cmd = cdata->cmd, *msg = NULL, *line;
	size_t				 size;
	int				 retcode, status;

	do {
		line = evbuffer_readln(event->input, NULL, EVBUFFER_EOL_LF);
		if (line != NULL) {
			cmd_run_shell_print(job, line);
			free(line);
		}
	} while (line != NULL);

	size = EVBUFFER_LENGTH(event->input);
	if (size != 0) {
		line = xmalloc(size + 1);
		memcpy(line, EVBUFFER_DATA(event->input), size);
		line[size] = '\0';

		cmd_run_shell_print(job, line);

		free(line);
	}

	status = job_get_status(job);
	if (WIFEXITED(status)) {
		if ((retcode = WEXITSTATUS(status)) != 0)
			xasprintf(&msg, "'%s' returned %d", cmd, retcode);
	} else if (WIFSIGNALED(status)) {
		retcode = WTERMSIG(status);
		xasprintf(&msg, "'%s' terminated by signal %d", cmd, retcode);
		retcode += 128;
	} else
		retcode = 0;
	if (msg != NULL)
		cmd_run_shell_print(job, msg);
	free(msg);

	if (item != NULL) {
		if (cmdq_get_client(item) != NULL &&
		    cmdq_get_client(item)->session == NULL)
			cmdq_get_client(item)->retval = retcode;
		cmdq_continue(item);
	}
}

static void
cmd_run_shell_free(void *data)
{
	struct cmd_run_shell_data	*cdata = data;

	evtimer_del(&cdata->timer);
	if (cdata->s != NULL)
		session_remove_ref(cdata->s, __func__);
	if (cdata->client != NULL)
		server_client_unref(cdata->client);
	if (cdata->state != NULL)
		args_make_commands_free(cdata->state);
	free(cdata->cwd);
	free(cdata->cmd);
	free(cdata);
}
