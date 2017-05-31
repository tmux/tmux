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
#include <sys/time.h>

#include <fnmatch.h>
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
extern const struct cmd_entry cmd_choose_tree_entry;
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
	&cmd_choose_tree_entry,
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

static int
cmd_try_alias(int *argc, char ***argv)
{
	struct options_entry	 *o;
	int			  old_argc = *argc, new_argc;
	char			**old_argv = *argv, **new_argv;
	u_int			  size, idx;
	int			  i;
	size_t			  wanted;
	const char		 *s, *cp = NULL;

	o = options_get_only(global_options, "command-alias");
	if (o == NULL || options_array_size(o, &size) == -1 || size == 0)
		return (-1);

	wanted = strlen(old_argv[0]);
	for (idx = 0; idx < size; idx++) {
		s = options_array_get(o, idx);
		if (s == NULL)
			continue;

		cp = strchr(s, '=');
		if (cp == NULL || (size_t)(cp - s) != wanted)
			continue;
		if (strncmp(old_argv[0], s, wanted) == 0)
			break;
	}
	if (idx == size)
		return (-1);

	if (cmd_string_split(cp + 1, &new_argc, &new_argv) != 0)
		return (-1);

	*argc = new_argc + old_argc - 1;
	*argv = xcalloc((*argc) + 1, sizeof **argv);

	for (i = 0; i < new_argc; i++)
		(*argv)[i] = xstrdup(new_argv[i]);
	for (i = 1; i < old_argc; i++)
		(*argv)[new_argc + i - 1] = xstrdup(old_argv[i]);

	log_debug("alias: %s=%s", old_argv[0], cp + 1);
	for (i = 0; i < *argc; i++)
		log_debug("alias: argv[%d] = %s", i, (*argv)[i]);

	cmd_free_argv(new_argc, new_argv);
	return (0);
}

struct cmd *
cmd_parse(int argc, char **argv, const char *file, u_int line, char **cause)
{
	const char		*name;
	const struct cmd_entry **entryp, *entry;
	struct cmd		*cmd;
	struct args		*args;
	char			 s[BUFSIZ];
	int			 ambiguous, allocated = 0;

	*cause = NULL;
	if (argc == 0) {
		xasprintf(cause, "no command");
		return (NULL);
	}
	name = argv[0];

retry:
	ambiguous = 0;
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
	if ((ambiguous || entry == NULL) &&
	    server_proc != NULL &&
	    !allocated &&
	    cmd_try_alias(&argc, &argv) == 0) {
		allocated = 1;
		goto retry;
	}
	if (ambiguous)
		goto ambiguous;
	if (entry == NULL) {
		xasprintf(cause, "unknown command: %s", name);
		return (NULL);
	}

	args = args_parse(entry->args.template, argc, argv);
	if (args == NULL)
		goto usage;
	if (entry->args.lower != -1 && args->argc < entry->args.lower)
		goto usage;
	if (entry->args.upper != -1 && args->argc > entry->args.upper)
		goto usage;

	cmd = xcalloc(1, sizeof *cmd);
	cmd->entry = entry;
	cmd->args = args;

	if (file != NULL)
		cmd->file = xstrdup(file);
	cmd->line = line;

	if (allocated)
		cmd_free_argv(argc, argv);
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
	xasprintf(cause, "ambiguous command: %s, could be: %s", name, s);
	return (NULL);

usage:
	if (args != NULL)
		args_free(args);
	xasprintf(cause, "usage: %s %s", entry->name, entry->usage);
	return (NULL);
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

	if (xp != NULL)
		*xp = x - wp->xoff;
	if (yp != NULL)
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
	const char	*ptr, *cp, quote[] = "\"\\$";
	int		 replaced, quoted;
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

			quoted = (*ptr == '%');
			if (quoted)
				ptr++;

			buf = xrealloc(buf, len + (strlen(s) * 3) + 1);
			for (cp = s; *cp != '\0'; cp++) {
				if (quoted && strchr(quote, *cp) != NULL)
					buf[len++] = '\\';
				if (quoted && *cp == ';') {
					buf[len++] = '\\';
					buf[len++] = '\\';
				}
				buf[len++] = *cp;
			}
			buf[len] = '\0';
			continue;
		}
		buf = xrealloc(buf, len + 2);
		buf[len++] = ch;
		buf[len] = '\0';
	}

	return (buf);
}
