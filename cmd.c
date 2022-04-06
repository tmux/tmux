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
extern const struct cmd_entry cmd_clear_prompt_history_entry;
extern const struct cmd_entry cmd_clock_mode_entry;
extern const struct cmd_entry cmd_command_prompt_entry;
extern const struct cmd_entry cmd_confirm_before_entry;
extern const struct cmd_entry cmd_copy_mode_entry;
extern const struct cmd_entry cmd_customize_mode_entry;
extern const struct cmd_entry cmd_delete_buffer_entry;
extern const struct cmd_entry cmd_detach_client_entry;
extern const struct cmd_entry cmd_display_menu_entry;
extern const struct cmd_entry cmd_display_message_entry;
extern const struct cmd_entry cmd_display_popup_entry;
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
extern const struct cmd_entry cmd_resize_window_entry;
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
extern const struct cmd_entry cmd_server_access_entry;
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
extern const struct cmd_entry cmd_show_prompt_history_entry;
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
	&cmd_clear_prompt_history_entry,
	&cmd_clock_mode_entry,
	&cmd_command_prompt_entry,
	&cmd_confirm_before_entry,
	&cmd_copy_mode_entry,
	&cmd_customize_mode_entry,
	&cmd_delete_buffer_entry,
	&cmd_detach_client_entry,
	&cmd_display_menu_entry,
	&cmd_display_message_entry,
	&cmd_display_popup_entry,
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
	&cmd_resize_window_entry,
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
	&cmd_server_access_entry,
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
	&cmd_show_prompt_history_entry,
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

/* Instance of a command. */
struct cmd {
	const struct cmd_entry	 *entry;
	struct args		 *args;
	u_int			  group;

	char			 *file;
	u_int			  line;

	TAILQ_ENTRY(cmd)	  qentry;
};
TAILQ_HEAD(cmds, cmd);

/* Next group number for new command list. */
static u_int cmd_list_next_group = 1;

/* Log an argument vector. */
void printflike(3, 4)
cmd_log_argv(int argc, char **argv, const char *fmt, ...)
{
	char	*prefix;
	va_list	 ap;
	int	 i;

	va_start(ap, fmt);
	xvasprintf(&prefix, fmt, ap);
	va_end(ap);

	for (i = 0; i < argc; i++)
		log_debug("%s: argv[%d]=%s", prefix, i, argv[i]);
	free(prefix);
}

/* Prepend to an argument vector. */
void
cmd_prepend_argv(int *argc, char ***argv, const char *arg)
{
	char	**new_argv;
	int	  i;

	new_argv = xreallocarray(NULL, (*argc) + 1, sizeof *new_argv);
	new_argv[0] = xstrdup(arg);
	for (i = 0; i < *argc; i++)
		new_argv[1 + i] = (*argv)[i];

	free(*argv);
	*argv = new_argv;
	(*argc)++;
}

/* Append to an argument vector. */
void
cmd_append_argv(int *argc, char ***argv, const char *arg)
{
	*argv = xreallocarray(*argv, (*argc) + 1, sizeof **argv);
	(*argv)[(*argc)++] = xstrdup(arg);
}

/* Pack an argument vector up into a buffer. */
int
cmd_pack_argv(int argc, char **argv, char *buf, size_t len)
{
	size_t	arglen;
	int	i;

	if (argc == 0)
		return (0);
	cmd_log_argv(argc, argv, "%s", __func__);

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

/* Unpack an argument vector from a packed buffer. */
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
	cmd_log_argv(argc, *argv, "%s", __func__);

	return (0);
}

/* Copy an argument vector, ensuring it is terminated by NULL. */
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

/* Free an argument vector. */
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

/* Convert argument vector to a string. */
char *
cmd_stringify_argv(int argc, char **argv)
{
	char	*buf = NULL, *s;
	size_t	 len = 0;
	int	 i;

	if (argc == 0)
		return (xstrdup(""));

	for (i = 0; i < argc; i++) {
		s = args_escape(argv[i]);
		log_debug("%s: %u %s = %s", __func__, i, argv[i], s);

		len += strlen(s) + 1;
		buf = xrealloc(buf, len);

		if (i == 0)
			*buf = '\0';
		else
			strlcat(buf, " ", len);
		strlcat(buf, s, len);

		free(s);
	}
	return (buf);
}

/* Get entry for command. */
const struct cmd_entry *
cmd_get_entry(struct cmd *cmd)
{
	return (cmd->entry);
}

/* Get arguments for command. */
struct args *
cmd_get_args(struct cmd *cmd)
{
	return (cmd->args);
}

/* Get group for command. */
u_int
cmd_get_group(struct cmd *cmd)
{
	return (cmd->group);
}

/* Get file and line for command. */
void
cmd_get_source(struct cmd *cmd, const char **file, u_int *line)
{
	if (file != NULL)
		*file = cmd->file;
	if (line != NULL)
		*line = cmd->line;
}

/* Look for an alias for a command. */
char *
cmd_get_alias(const char *name)
{
	struct options_entry		*o;
	struct options_array_item	*a;
	union options_value		*ov;
	size_t				 wanted, n;
	const char			*equals;

	o = options_get_only(global_options, "command-alias");
	if (o == NULL)
		return (NULL);
	wanted = strlen(name);

	a = options_array_first(o);
	while (a != NULL) {
		ov = options_array_item_value(a);

		equals = strchr(ov->string, '=');
		if (equals != NULL) {
			n = equals - ov->string;
			if (n == wanted && strncmp(name, ov->string, n) == 0)
				return (xstrdup(equals + 1));
		}

		a = options_array_next(a);
	}
	return (NULL);
}

/* Look up a command entry by name. */
static const struct cmd_entry *
cmd_find(const char *name, char **cause)
{
	const struct cmd_entry	**loop, *entry, *found = NULL;
	int			  ambiguous;
	char			  s[8192];

	ambiguous = 0;
	for (loop = cmd_table; *loop != NULL; loop++) {
		entry = *loop;
		if (entry->alias != NULL && strcmp(entry->alias, name) == 0) {
			ambiguous = 0;
			found = entry;
			break;
		}

		if (strncmp(entry->name, name, strlen(name)) != 0)
			continue;
		if (found != NULL)
			ambiguous = 1;
		found = entry;

		if (strcmp(entry->name, name) == 0)
			break;
	}
	if (ambiguous)
		goto ambiguous;
	if (found == NULL) {
		xasprintf(cause, "unknown command: %s", name);
		return (NULL);
	}
	return (found);

ambiguous:
	*s = '\0';
	for (loop = cmd_table; *loop != NULL; loop++) {
		entry = *loop;
		if (strncmp(entry->name, name, strlen(name)) != 0)
			continue;
		if (strlcat(s, entry->name, sizeof s) >= sizeof s)
			break;
		if (strlcat(s, ", ", sizeof s) >= sizeof s)
			break;
	}
	s[strlen(s) - 2] = '\0';
	xasprintf(cause, "ambiguous command: %s, could be: %s", name, s);
	return (NULL);
}

/* Parse a single command from an argument vector. */
struct cmd *
cmd_parse(struct args_value *values, u_int count, const char *file, u_int line,
    char **cause)
{
	const struct cmd_entry	*entry;
	struct cmd		*cmd;
	struct args		*args;
	char			*error = NULL;

	if (count == 0 || values[0].type != ARGS_STRING) {
		xasprintf(cause, "no command");
		return (NULL);
	}
	entry = cmd_find(values[0].string, cause);
	if (entry == NULL)
		return (NULL);

	args = args_parse(&entry->args, values, count, &error);
	if (args == NULL && error == NULL) {
		xasprintf(cause, "usage: %s %s", entry->name, entry->usage);
		return (NULL);
	}
	if (args == NULL) {
		xasprintf(cause, "command %s: %s", entry->name, error);
		free(error);
		return (NULL);
	}

	cmd = xcalloc(1, sizeof *cmd);
	cmd->entry = entry;
	cmd->args = args;

	if (file != NULL)
		cmd->file = xstrdup(file);
	cmd->line = line;

	return (cmd);
}

/* Free a command. */
void
cmd_free(struct cmd *cmd)
{
	free(cmd->file);

	args_free(cmd->args);
	free(cmd);
}

/* Copy a command. */
struct cmd *
cmd_copy(struct cmd *cmd, int argc, char **argv)
{
	struct cmd	*new_cmd;

	new_cmd = xcalloc(1, sizeof *new_cmd);
	new_cmd->entry = cmd->entry;
	new_cmd->args = args_copy(cmd->args, argc, argv);

	if (cmd->file != NULL)
		new_cmd->file = xstrdup(cmd->file);
	new_cmd->line = cmd->line;

	return (new_cmd);
}

/* Get a command as a string. */
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

/* Create a new command list. */
struct cmd_list *
cmd_list_new(void)
{
	struct cmd_list	*cmdlist;

	cmdlist = xcalloc(1, sizeof *cmdlist);
	cmdlist->references = 1;
	cmdlist->group = cmd_list_next_group++;
	cmdlist->list = xcalloc(1, sizeof *cmdlist->list);
	TAILQ_INIT(cmdlist->list);
	return (cmdlist);
}

/* Append a command to a command list. */
void
cmd_list_append(struct cmd_list *cmdlist, struct cmd *cmd)
{
	cmd->group = cmdlist->group;
	TAILQ_INSERT_TAIL(cmdlist->list, cmd, qentry);
}

/* Append all commands from one list to another.  */
void
cmd_list_append_all(struct cmd_list *cmdlist, struct cmd_list *from)
{
	struct cmd	*cmd;

	TAILQ_FOREACH(cmd, from->list, qentry)
		cmd->group = cmdlist->group;
	TAILQ_CONCAT(cmdlist->list, from->list, qentry);
}

/* Move all commands from one command list to another. */
void
cmd_list_move(struct cmd_list *cmdlist, struct cmd_list *from)
{
	TAILQ_CONCAT(cmdlist->list, from->list, qentry);
	cmdlist->group = cmd_list_next_group++;
}

/* Free a command list. */
void
cmd_list_free(struct cmd_list *cmdlist)
{
	struct cmd	*cmd, *cmd1;

	if (--cmdlist->references != 0)
		return;

	TAILQ_FOREACH_SAFE(cmd, cmdlist->list, qentry, cmd1) {
		TAILQ_REMOVE(cmdlist->list, cmd, qentry);
		cmd_free(cmd);
	}
	free(cmdlist->list);
	free(cmdlist);
}

/* Copy a command list, expanding %s in arguments. */
struct cmd_list *
cmd_list_copy(struct cmd_list *cmdlist, int argc, char **argv)
{
	struct cmd	*cmd;
	struct cmd_list	*new_cmdlist;
	struct cmd	*new_cmd;
	u_int		 group = cmdlist->group;
	char		*s;

	s = cmd_list_print(cmdlist, 0);
	log_debug("%s: %s", __func__, s);
	free(s);

	new_cmdlist = cmd_list_new();
	TAILQ_FOREACH(cmd, cmdlist->list, qentry) {
		if (cmd->group != group) {
			new_cmdlist->group = cmd_list_next_group++;
			group = cmd->group;
		}
		new_cmd = cmd_copy(cmd, argc, argv);
		cmd_list_append(new_cmdlist, new_cmd);
	}

	s = cmd_list_print(new_cmdlist, 0);
	log_debug("%s: %s", __func__, s);
	free(s);

	return (new_cmdlist);
}

/* Get a command list as a string. */
char *
cmd_list_print(struct cmd_list *cmdlist, int escaped)
{
	struct cmd	*cmd, *next;
	char		*buf, *this;
	size_t		 len;

	len = 1;
	buf = xcalloc(1, len);

	TAILQ_FOREACH(cmd, cmdlist->list, qentry) {
		this = cmd_print(cmd);

		len += strlen(this) + 6;
		buf = xrealloc(buf, len);

		strlcat(buf, this, len);

		next = TAILQ_NEXT(cmd, qentry);
		if (next != NULL) {
			if (cmd->group != next->group) {
				if (escaped)
					strlcat(buf, " \\;\\; ", len);
				else
					strlcat(buf, " ;; ", len);
			} else {
				if (escaped)
					strlcat(buf, " \\; ", len);
				else
					strlcat(buf, " ; ", len);
			}
		}

		free(this);
	}

	return (buf);
}

/* Get first command in list. */
struct cmd *
cmd_list_first(struct cmd_list *cmdlist)
{
	return (TAILQ_FIRST(cmdlist->list));
}

/* Get next command in list. */
struct cmd *
cmd_list_next(struct cmd *cmd)
{
	return (TAILQ_NEXT(cmd, qentry));
}

/* Do all of the commands in this command list have this flag? */
int
cmd_list_all_have(struct cmd_list *cmdlist, int flag)
{
	struct cmd	*cmd;

	TAILQ_FOREACH(cmd, cmdlist->list, qentry) {
		if (~cmd->entry->flags & flag)
			return (0);
	}
	return (1);
}

/* Do any of the commands in this command list have this flag? */
int
cmd_list_any_have(struct cmd_list *cmdlist, int flag)
{
	struct cmd	*cmd;

	TAILQ_FOREACH(cmd, cmdlist->list, qentry) {
		if (cmd->entry->flags & flag)
			return (1);
	}
	return (0);
}

/* Adjust current mouse position for a pane. */
int
cmd_mouse_at(struct window_pane *wp, struct mouse_event *m, u_int *xp,
    u_int *yp, int last)
{
	u_int	x, y;

	if (last) {
		x = m->lx + m->ox;
		y = m->ly + m->oy;
	} else {
		x = m->x + m->ox;
		y = m->y + m->oy;
	}
	log_debug("%s: x=%u, y=%u%s", __func__, x, y, last ? " (last)" : "");

	if (m->statusat == 0 && y >= m->statuslines)
		y -= m->statuslines;

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
	struct winlink	*wl;

	if (!m->valid)
		return (NULL);
	if (m->s == -1 || (s = session_find_by_id(m->s)) == NULL)
		return (NULL);
	if (m->w == -1)
		wl = s->curw;
	else {
		if ((w = window_find_by_id(m->w)) == NULL)
			return (NULL);
		wl = winlink_find_by_window(&s->windows, w);
	}
	if (sp != NULL)
		*sp = s;
	return (wl);
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
	const char	*ptr, *cp, quote[] = "\"\\$;~";
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
