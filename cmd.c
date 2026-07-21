/* $OpenBSD: cmd.c,v 1.189 2026/07/17 08:29:34 nicm Exp $ */

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
extern const struct cmd_entry cmd_new_pane_entry;
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
extern const struct cmd_entry cmd_switch_mode_entry;
extern const struct cmd_entry cmd_unbind_key_entry;
extern const struct cmd_entry cmd_unlink_window_entry;
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
	&cmd_new_pane_entry,
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
	&cmd_switch_mode_entry,
	&cmd_unbind_key_entry,
	&cmd_unlink_window_entry,
	&cmd_wait_for_entry,
	NULL
};

/* Instance of a command. */
struct cmd {
	const struct cmd_entry	 *entry;
	struct args		 *args;

	char			 *file;
	u_int			  line;
	int			  parse_flags;
};

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
	if (argc < 0 || argc > 1000)
		return (-1);
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

/* Get file and line for command. */
void
cmd_get_source(struct cmd *cmd, const char **file, u_int *line)
{
	if (file != NULL)
		*file = cmd->file;
	if (line != NULL)
		*line = cmd->line;
}

/* Get parse flags for command. */
int
cmd_get_parse_flags(struct cmd *cmd)
{
	return (cmd->parse_flags);
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
const struct cmd_entry *
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
    int parse_flags, char **cause)
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
	cmd->parse_flags = parse_flags;

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

	if ((int)x < wp->xoff || (int)x >= wp->xoff + (int)wp->sx)
		return (-1);
	if ((int)y < wp->yoff || (int)y >= wp->yoff + (int)wp->sy)
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
	if (m->wp == -1)
		wp = wl->window->active;
	else {
		if ((wp = window_pane_find_by_id(m->wp)) == NULL)
			return (NULL);
		if (!window_has_pane(wl->window, wp))
			return (NULL);
	}
	if (wl->window->modal != NULL && wp != wl->window->modal)
		return (NULL);

	if (wlp != NULL)
		*wlp = wl;
	return (wp);
}

/*
 * Replace the first %% or any %idx in template by s. %% is intended for use in
 * single quotes, so ' is escaped. %%% and %idx% are for double quotes, so a
 * list of special characters is escaped. %idx is left unescaped.
 */
char *
cmd_template_replace(const char *template, const char *s, int idx)
{
	char			 ch, *buf;
	const char		*ptr, *cp, dquote[] = "\"\\$;~";
	int			 replaced;
	size_t			 len, slen;
	enum { NQ, SQ, DQ }	 quote;

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
			if (*ptr >= '1' && *ptr <= '9' && *ptr - '0' == idx) {
				ptr++;
				quote = NQ;
				if (*ptr == '%') {
					quote = DQ;
					ptr++;
				}
			} else {
				if (*ptr != '%' || replaced)
					break;
				replaced = 1;
				ptr++;
				quote = SQ;
				if (*ptr == '%') {
					quote = DQ;
					ptr++;
				}
			}

			slen = strlen(s);
			if (slen >= SIZE_MAX / 4 ||
			    len > SIZE_MAX - (slen * 4) - 1)
				fatalx("argument too long");
			buf = xrealloc(buf, len + (slen * 4) + 1);
			for (cp = s; *cp != '\0'; cp++) {
				if (quote == SQ && *cp == '\'') {
					buf[len++] = '\'';
					buf[len++] = '\\';
					buf[len++] = '\'';
					buf[len++] = '\'';
					continue;
				}
				if (quote == DQ && strchr(dquote, *cp) != NULL)
					buf[len++] = '\\';
				buf[len++] = *cp;
			}
			buf[len] = '\0';
			continue;
		}
		if (len > SIZE_MAX - 2)
			fatalx("argument too long");
		buf = xrealloc(buf, len + 2);
		buf[len++] = ch;
		buf[len] = '\0';
	}

	log_debug("%s: %s -> %s", __func__, template, buf);
	return (buf);
}
