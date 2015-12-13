/* $OpenBSD$ */

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
#include <sys/time.h>

#include <fnmatch.h>
#include <paths.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

extern const struct cmd_entry cmd_attach_session_entry;
extern const struct cmd_entry cmd_bind_key_entry;
extern const struct cmd_entry cmd_break_pane_entry;
extern const struct cmd_entry cmd_capture_pane_entry;
extern const struct cmd_entry cmd_choose_buffer_entry;
extern const struct cmd_entry cmd_choose_client_entry;
extern const struct cmd_entry cmd_choose_session_entry;
extern const struct cmd_entry cmd_choose_tree_entry;
extern const struct cmd_entry cmd_choose_window_entry;
extern const struct cmd_entry cmd_clear_history_entry;
extern const struct cmd_entry cmd_clock_mode_entry;
extern const struct cmd_entry cmd_command_prompt_entry;
extern const struct cmd_entry cmd_confirm_before_entry;
extern const struct cmd_entry cmd_copy_mode_entry;
extern const struct cmd_entry cmd_delete_buffer_entry;
extern const struct cmd_entry cmd_detach_client_entry;
extern const struct cmd_entry cmd_display_message_entry;
extern const struct cmd_entry cmd_display_panes_entry;
extern const struct cmd_entry cmd_down_pane_entry;
extern const struct cmd_entry cmd_find_window_entry;
extern const struct cmd_entry cmd_has_session_entry;
extern const struct cmd_entry cmd_if_shell_entry;
extern const struct cmd_entry cmd_join_pane_entry;
extern const struct cmd_entry cmd_kill_pane_entry;
extern const struct cmd_entry cmd_kill_server_entry;
extern const struct cmd_entry cmd_kill_session_entry;
extern const struct cmd_entry cmd_kill_window_entry;
extern const struct cmd_entry cmd_last_pane_entry;
extern const struct cmd_entry cmd_last_window_entry;
extern const struct cmd_entry cmd_link_window_entry;
extern const struct cmd_entry cmd_list_buffers_entry;
extern const struct cmd_entry cmd_list_clients_entry;
extern const struct cmd_entry cmd_list_commands_entry;
extern const struct cmd_entry cmd_list_keys_entry;
extern const struct cmd_entry cmd_list_panes_entry;
extern const struct cmd_entry cmd_list_sessions_entry;
extern const struct cmd_entry cmd_list_windows_entry;
extern const struct cmd_entry cmd_load_buffer_entry;
extern const struct cmd_entry cmd_lock_client_entry;
extern const struct cmd_entry cmd_lock_server_entry;
extern const struct cmd_entry cmd_lock_session_entry;
extern const struct cmd_entry cmd_move_pane_entry;
extern const struct cmd_entry cmd_move_window_entry;
extern const struct cmd_entry cmd_new_session_entry;
extern const struct cmd_entry cmd_new_window_entry;
extern const struct cmd_entry cmd_next_layout_entry;
extern const struct cmd_entry cmd_next_window_entry;
extern const struct cmd_entry cmd_paste_buffer_entry;
extern const struct cmd_entry cmd_pipe_pane_entry;
extern const struct cmd_entry cmd_previous_layout_entry;
extern const struct cmd_entry cmd_previous_window_entry;
extern const struct cmd_entry cmd_refresh_client_entry;
extern const struct cmd_entry cmd_rename_session_entry;
extern const struct cmd_entry cmd_rename_window_entry;
extern const struct cmd_entry cmd_resize_pane_entry;
extern const struct cmd_entry cmd_respawn_pane_entry;
extern const struct cmd_entry cmd_respawn_window_entry;
extern const struct cmd_entry cmd_rotate_window_entry;
extern const struct cmd_entry cmd_run_shell_entry;
extern const struct cmd_entry cmd_save_buffer_entry;
extern const struct cmd_entry cmd_select_layout_entry;
extern const struct cmd_entry cmd_select_pane_entry;
extern const struct cmd_entry cmd_select_window_entry;
extern const struct cmd_entry cmd_send_keys_entry;
extern const struct cmd_entry cmd_send_prefix_entry;
extern const struct cmd_entry cmd_server_info_entry;
extern const struct cmd_entry cmd_set_buffer_entry;
extern const struct cmd_entry cmd_set_environment_entry;
extern const struct cmd_entry cmd_set_hook_entry;
extern const struct cmd_entry cmd_set_option_entry;
extern const struct cmd_entry cmd_set_window_option_entry;
extern const struct cmd_entry cmd_show_buffer_entry;
extern const struct cmd_entry cmd_show_environment_entry;
extern const struct cmd_entry cmd_show_hooks_entry;
extern const struct cmd_entry cmd_show_messages_entry;
extern const struct cmd_entry cmd_show_options_entry;
extern const struct cmd_entry cmd_show_window_options_entry;
extern const struct cmd_entry cmd_source_file_entry;
extern const struct cmd_entry cmd_split_window_entry;
extern const struct cmd_entry cmd_start_server_entry;
extern const struct cmd_entry cmd_suspend_client_entry;
extern const struct cmd_entry cmd_swap_pane_entry;
extern const struct cmd_entry cmd_swap_window_entry;
extern const struct cmd_entry cmd_switch_client_entry;
extern const struct cmd_entry cmd_unbind_key_entry;
extern const struct cmd_entry cmd_unlink_window_entry;
extern const struct cmd_entry cmd_up_pane_entry;
extern const struct cmd_entry cmd_wait_for_entry;

const struct cmd_entry *cmd_table[] = {
	&cmd_attach_session_entry,
	&cmd_bind_key_entry,
	&cmd_break_pane_entry,
	&cmd_capture_pane_entry,
	&cmd_choose_buffer_entry,
	&cmd_choose_client_entry,
	&cmd_choose_session_entry,
	&cmd_choose_tree_entry,
	&cmd_choose_window_entry,
	&cmd_clear_history_entry,
	&cmd_clock_mode_entry,
	&cmd_command_prompt_entry,
	&cmd_confirm_before_entry,
	&cmd_copy_mode_entry,
	&cmd_delete_buffer_entry,
	&cmd_detach_client_entry,
	&cmd_display_message_entry,
	&cmd_display_panes_entry,
	&cmd_find_window_entry,
	&cmd_has_session_entry,
	&cmd_if_shell_entry,
	&cmd_join_pane_entry,
	&cmd_kill_pane_entry,
	&cmd_kill_server_entry,
	&cmd_kill_session_entry,
	&cmd_kill_window_entry,
	&cmd_last_pane_entry,
	&cmd_last_window_entry,
	&cmd_link_window_entry,
	&cmd_list_buffers_entry,
	&cmd_list_clients_entry,
	&cmd_list_commands_entry,
	&cmd_list_keys_entry,
	&cmd_list_panes_entry,
	&cmd_list_sessions_entry,
	&cmd_list_windows_entry,
	&cmd_load_buffer_entry,
	&cmd_lock_client_entry,
	&cmd_lock_server_entry,
	&cmd_lock_session_entry,
	&cmd_move_pane_entry,
	&cmd_move_window_entry,
	&cmd_new_session_entry,
	&cmd_new_window_entry,
	&cmd_next_layout_entry,
	&cmd_next_window_entry,
	&cmd_paste_buffer_entry,
	&cmd_pipe_pane_entry,
	&cmd_previous_layout_entry,
	&cmd_previous_window_entry,
	&cmd_refresh_client_entry,
	&cmd_rename_session_entry,
	&cmd_rename_window_entry,
	&cmd_resize_pane_entry,
	&cmd_respawn_pane_entry,
	&cmd_respawn_window_entry,
	&cmd_rotate_window_entry,
	&cmd_run_shell_entry,
	&cmd_save_buffer_entry,
	&cmd_select_layout_entry,
	&cmd_select_pane_entry,
	&cmd_select_window_entry,
	&cmd_send_keys_entry,
	&cmd_send_prefix_entry,
	&cmd_server_info_entry,
	&cmd_set_buffer_entry,
	&cmd_set_environment_entry,
	&cmd_set_hook_entry,
	&cmd_set_option_entry,
	&cmd_set_window_option_entry,
	&cmd_show_buffer_entry,
	&cmd_show_environment_entry,
	&cmd_show_hooks_entry,
	&cmd_show_messages_entry,
	&cmd_show_options_entry,
	&cmd_show_window_options_entry,
	&cmd_source_file_entry,
	&cmd_split_window_entry,
	&cmd_start_server_entry,
	&cmd_suspend_client_entry,
	&cmd_swap_pane_entry,
	&cmd_swap_window_entry,
	&cmd_switch_client_entry,
	&cmd_unbind_key_entry,
	&cmd_unlink_window_entry,
	&cmd_wait_for_entry,
	NULL
};

static void	cmd_clear_state(struct cmd_state *);
static int	cmd_set_state_flag(struct cmd *, struct cmd_q *, char);

int
cmd_pack_argv(int argc, char **argv, char *buf, size_t len)
{
	size_t	arglen;
	int	i;

	if (argc == 0)
		return (0);

	*buf = '\0';
	for (i = 0; i < argc; i++) {
		if (strlcpy(buf, argv[i], len) >= len)
			return (-1);
		arglen = strlen(argv[i]) + 1;
		buf += arglen;
		len -= arglen;
	}

	return (0);
}

int
cmd_unpack_argv(char *buf, size_t len, int argc, char ***argv)
{
	int	i;
	size_t	arglen;

	if (argc == 0)
		return (0);
	*argv = xcalloc(argc, sizeof **argv);

	buf[len - 1] = '\0';
	for (i = 0; i < argc; i++) {
		if (len == 0) {
			cmd_free_argv(argc, *argv);
			return (-1);
		}

		arglen = strlen(buf) + 1;
		(*argv)[i] = xstrdup(buf);
		buf += arglen;
		len -= arglen;
	}

	return (0);
}

char **
cmd_copy_argv(int argc, char **argv)
{
	char	**new_argv;
	int	  i;

	if (argc == 0)
		return (NULL);
	new_argv = xcalloc(argc + 1, sizeof *new_argv);
	for (i = 0; i < argc; i++) {
		if (argv[i] != NULL)
			new_argv[i] = xstrdup(argv[i]);
	}
	return (new_argv);
}

void
cmd_free_argv(int argc, char **argv)
{
	int	i;

	if (argc == 0)
		return;
	for (i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}

char *
cmd_stringify_argv(int argc, char **argv)
{
	char	*buf;
	int	 i;
	size_t	 len;

	if (argc == 0)
		return (xstrdup(""));

	len = 0;
	buf = NULL;

	for (i = 0; i < argc; i++) {
		len += strlen(argv[i]) + 1;
		buf = xrealloc(buf, len);

		if (i == 0)
			*buf = '\0';
		else
			strlcat(buf, " ", len);
		strlcat(buf, argv[i], len);
	}
	return (buf);
}

struct cmd *
cmd_parse(int argc, char **argv, const char *file, u_int line, char **cause)
{
	const struct cmd_entry **entryp, *entry;
	struct cmd		*cmd;
	struct args		*args;
	char			 s[BUFSIZ];
	int			 ambiguous = 0;

	*cause = NULL;
	if (argc == 0) {
		xasprintf(cause, "no command");
		return (NULL);
	}

	entry = NULL;
	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		if ((*entryp)->alias != NULL &&
		    strcmp((*entryp)->alias, argv[0]) == 0) {
			ambiguous = 0;
			entry = *entryp;
			break;
		}

		if (strncmp((*entryp)->name, argv[0], strlen(argv[0])) != 0)
			continue;
		if (entry != NULL)
			ambiguous = 1;
		entry = *entryp;

		/* Bail now if an exact match. */
		if (strcmp(entry->name, argv[0]) == 0)
			break;
	}
	if (ambiguous)
		goto ambiguous;
	if (entry == NULL) {
		xasprintf(cause, "unknown command: %s", argv[0]);
		return (NULL);
	}

	args = args_parse(entry->args_template, argc, argv);
	if (args == NULL)
		goto usage;
	if (entry->args_lower != -1 && args->argc < entry->args_lower)
		goto usage;
	if (entry->args_upper != -1 && args->argc > entry->args_upper)
		goto usage;

	cmd = xcalloc(1, sizeof *cmd);
	cmd->entry = entry;
	cmd->args = args;

	if (file != NULL)
		cmd->file = xstrdup(file);
	cmd->line = line;

	return (cmd);

ambiguous:
	*s = '\0';
	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		if (strncmp((*entryp)->name, argv[0], strlen(argv[0])) != 0)
			continue;
		if (strlcat(s, (*entryp)->name, sizeof s) >= sizeof s)
			break;
		if (strlcat(s, ", ", sizeof s) >= sizeof s)
			break;
	}
	s[strlen(s) - 2] = '\0';
	xasprintf(cause, "ambiguous command: %s, could be: %s", argv[0], s);
	return (NULL);

usage:
	if (args != NULL)
		args_free(args);
	xasprintf(cause, "usage: %s %s", entry->name, entry->usage);
	return (NULL);
}

static void
cmd_clear_state(struct cmd_state *state)
{
	state->c = NULL;

	state->tflag.s = NULL;
	state->tflag.wl = NULL;
	state->tflag.wp = NULL;
	state->tflag.idx = -1;

	state->sflag.s = NULL;
	state->sflag.wl = NULL;
	state->sflag.wp = NULL;
	state->sflag.idx = -1;
}

static int
cmd_set_state_flag(struct cmd *cmd, struct cmd_q *cmdq, char c)
{
	struct cmd_state	*state = &cmdq->state;
	struct cmd_find_state	*fsf = NULL;
	const char		*flag;
	int			 flags = cmd->entry->flags, everything = 0;
	int			 allflags = 0, targetflags, error;
	struct session		*s;
	struct window		*w;
	struct winlink		*wl;
	struct window_pane	*wp;

	/* Set up state for either -t or -s. */
	if (c == 't') {
		fsf = &cmdq->state.tflag;
		allflags = CMD_ALL_T;
	} else if (c == 's') {
		fsf = &cmdq->state.sflag;
		allflags = CMD_ALL_S;
	}

	/*
	 * If the command wants something and no argument is present, use the
	 * base command instead.
	 */
	flag = args_get(cmd->args, c);
	if (flag == NULL) {
		if ((flags & allflags) == 0)
			return (0); /* doesn't care about flag */
		cmd = cmdq->cmd;
		everything = 1;
		flag = args_get(cmd->args, c);
	}

	/*
	 * If no flag and the current command is allowed to fail, just skip to
	 * fill in as much we can, otherwise continue and fail later if needed.
	 */
	if (flag == NULL && (flags & CMD_CANFAIL))
		goto complete_everything;

	/* Fill in state using command (current or base) flags. */
	if (flags & CMD_PREFERUNATTACHED)
		targetflags = CMD_FIND_PREFER_UNATTACHED;
	else
		targetflags = 0;
	switch (cmd->entry->flags & allflags) {
	case 0:
		break;
	case CMD_SESSION_T|CMD_PANE_T:
	case CMD_SESSION_S|CMD_PANE_S:
		if (flag != NULL && flag[strcspn(flag, ":.")] != '\0') {
			error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_PANE,
			    targetflags);
			if (error != 0)
				return (-1);
		} else {
			error = cmd_find_target(fsf, cmdq, flag,
			    CMD_FIND_SESSION, targetflags);
			if (error != 0)
				return (-1);

			if (flag == NULL) {
				fsf->wl = fsf->s->curw;
				fsf->wp = fsf->s->curw->window->active;
			} else {
				s = fsf->s;
				if ((w = window_find_by_id_str(flag)) != NULL)
					wp = w->active;
				else {
					wp = window_pane_find_by_id_str(flag);
					if (wp != NULL)
						w = wp->window;
				}
				wl = winlink_find_by_window(&s->windows, w);
				if (wl != NULL) {
					fsf->wl = wl;
					fsf->wp = wp;
				}
			}
		}
		break;
	case CMD_MOVEW_R|CMD_INDEX_T:
	case CMD_MOVEW_R|CMD_INDEX_S:
		error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_SESSION,
		    targetflags|CMD_FIND_QUIET);
		if (error != 0) {
			error = cmd_find_target(fsf, cmdq, flag,
			    CMD_FIND_WINDOW, CMD_FIND_WINDOW_INDEX);
			if (error != 0)
				return (-1);
		}
		break;
	case CMD_SESSION_T:
	case CMD_SESSION_S:
		error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_SESSION,
		    targetflags);
		if (error != 0)
			return (-1);
		break;
	case CMD_WINDOW_MARKED_T:
	case CMD_WINDOW_MARKED_S:
		targetflags |= CMD_FIND_DEFAULT_MARKED;
		/* FALLTHROUGH */
	case CMD_WINDOW_T:
	case CMD_WINDOW_S:
		error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_WINDOW,
		    targetflags);
		if (error != 0)
			return (-1);
		break;
	case CMD_PANE_MARKED_T:
	case CMD_PANE_MARKED_S:
		targetflags |= CMD_FIND_DEFAULT_MARKED;
		/* FALLTHROUGH */
	case CMD_PANE_T:
	case CMD_PANE_S:
		error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_PANE,
		    targetflags);
		if (error != 0)
			return (-1);
		break;
	case CMD_INDEX_T:
	case CMD_INDEX_S:
		error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_WINDOW,
		    CMD_FIND_WINDOW_INDEX);
		if (error != 0)
			return (-1);
		break;
	default:
		fatalx("too many -%c for %s", c, cmd->entry->name);
	}

	/*
	 * If this is still the current command, it wants what it asked for and
	 * nothing more. If it's the base command, fill in as much as possible
	 * because the current command may have different flags.
	 */
	if (!everything)
		return (0);

complete_everything:
	if (fsf->s == NULL) {
		if (state->c != NULL)
			fsf->s = state->c->session;
		if (fsf->s == NULL) {
			error = cmd_find_target(fsf, cmdq, NULL,
			    CMD_FIND_SESSION, CMD_FIND_QUIET);
			if (error != 0)
				fsf->s = NULL;
		}
		if (fsf->s == NULL) {
			if (flags & CMD_CANFAIL)
				return (0);
			cmdq_error(cmdq, "no current session");
			return (-1);
		}
	}
	if (fsf->wl == NULL) {
		error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_WINDOW, 0);
		if (error != 0)
			return (-1);
	}
	if (fsf->wp == NULL) {
		error = cmd_find_target(fsf, cmdq, flag, CMD_FIND_PANE, 0);
		if (error != 0)
			return (-1);
	}
	return (0);
}

int
cmd_prepare_state(struct cmd *cmd, struct cmd_q *cmdq)
{
	struct cmd_state	*state = &cmdq->state;
	struct args		*args = cmd->args;
	char			*tmp;
	int			 error;

	tmp = cmd_print(cmd);
	log_debug("preparing state for: %s (client %p)", tmp, cmdq->client);
	free(tmp);

	/* Start with an empty state. */
	cmd_clear_state(state);

	/*
	 * If the command wants a client and provides -c or -t, use it. If not,
	 * try the base command instead via cmd_get_state_client. No client is
	 * allowed if no flags, otherwise it must be available.
	 */
	switch (cmd->entry->flags & (CMD_CLIENT_C|CMD_CLIENT_T)) {
	case 0:
		state->c = cmd_find_client(cmdq, NULL, 1);
		break;
	case CMD_CLIENT_C:
		state->c = cmd_find_client(cmdq, args_get(args, 'c'), 0);
		if (state->c == NULL)
			return (-1);
		break;
	case CMD_CLIENT_T:
		state->c = cmd_find_client(cmdq, args_get(args, 't'), 0);
		if (state->c == NULL)
			return (-1);
		break;
	default:
		fatalx("both -c and -t for %s", cmd->entry->name);
	}

	error = cmd_set_state_flag(cmd, cmdq, 't');
	if (error == 0)
		error = cmd_set_state_flag(cmd, cmdq, 's');
	return (error);
}

char *
cmd_print(struct cmd *cmd)
{
	char	*out, *s;

	s = args_print(cmd->args);
	if (*s != '\0')
		xasprintf(&out, "%s %s", cmd->entry->name, s);
	else
		out = xstrdup(cmd->entry->name);
	free(s);

	return (out);
}

/* Adjust current mouse position for a pane. */
int
cmd_mouse_at(struct window_pane *wp, struct mouse_event *m, u_int *xp,
    u_int *yp, int last)
{
	u_int	x, y;

	if (last) {
		x = m->lx;
		y = m->ly;
	} else {
		x = m->x;
		y = m->y;
	}

	if (m->statusat == 0 && y > 0)
		y--;
	else if (m->statusat > 0 && y >= (u_int)m->statusat)
		y = m->statusat - 1;

	if (x < wp->xoff || x >= wp->xoff + wp->sx)
		return (-1);
	if (y < wp->yoff || y >= wp->yoff + wp->sy)
		return (-1);

	*xp = x - wp->xoff;
	*yp = y - wp->yoff;
	return (0);
}

/* Get current mouse window if any. */
struct winlink *
cmd_mouse_window(struct mouse_event *m, struct session **sp)
{
	struct session	*s;
	struct window	*w;

	if (!m->valid || m->s == -1 || m->w == -1)
		return (NULL);
	if ((s = session_find_by_id(m->s)) == NULL)
		return (NULL);
	if ((w = window_find_by_id(m->w)) == NULL)
		return (NULL);

	if (sp != NULL)
		*sp = s;
	return (winlink_find_by_window(&s->windows, w));
}

/* Get current mouse pane if any. */
struct window_pane *
cmd_mouse_pane(struct mouse_event *m, struct session **sp,
    struct winlink **wlp)
{
	struct winlink		*wl;
	struct window_pane     	*wp;

	if ((wl = cmd_mouse_window(m, sp)) == NULL)
		return (NULL);
	if ((wp = window_pane_find_by_id(m->wp)) == NULL)
		return (NULL);
	if (!window_has_pane(wl->window, wp))
		return (NULL);

	if (wlp != NULL)
		*wlp = wl;
	return (wp);
}

/* Replace the first %% or %idx in template by s. */
char *
cmd_template_replace(const char *template, const char *s, int idx)
{
	char		 ch, *buf;
	const char	*ptr;
	int		 replaced;
	size_t		 len;

	if (strchr(template, '%') == NULL)
		return (xstrdup(template));

	buf = xmalloc(1);
	*buf = '\0';
	len = 0;
	replaced = 0;

	ptr = template;
	while (*ptr != '\0') {
		switch (ch = *ptr++) {
		case '%':
			if (*ptr < '1' || *ptr > '9' || *ptr - '0' != idx) {
				if (*ptr != '%' || replaced)
					break;
				replaced = 1;
			}
			ptr++;

			len += strlen(s);
			buf = xrealloc(buf, len + 1);
			strlcat(buf, s, len + 1);
			continue;
		}
		buf = xrealloc(buf, len + 2);
		buf[len++] = ch;
		buf[len] = '\0';
	}

	return (buf);
}
