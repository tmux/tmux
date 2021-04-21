/* $OpenBSD$ */

/*
 * Copyright (c) 2011 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <string.h>

#include "tmux.h"

/*
 * This file has a tables with all the server, session and window
 * options. These tables are the master copy of the options with their real
 * (user-visible) types, range limits and default values. At start these are
 * copied into the runtime global options trees (which only has number and
 * string types). These tables are then used to look up the real type when the
 * user sets an option or its value needs to be shown.
 */

/* Choice option type lists. */
static const char *options_table_mode_keys_list[] = {
	"emacs", "vi", NULL
};
static const char *options_table_clock_mode_style_list[] = {
	"12", "24", NULL
};
static const char *options_table_status_list[] = {
	"off", "on", "2", "3", "4", "5", NULL
};
static const char *options_table_status_keys_list[] = {
	"emacs", "vi", NULL
};
static const char *options_table_status_justify_list[] = {
	"left", "centre", "right", "absolute-centre", NULL
};
static const char *options_table_status_position_list[] = {
	"top", "bottom", NULL
};
static const char *options_table_bell_action_list[] = {
	"none", "any", "current", "other", NULL
};
static const char *options_table_visual_bell_list[] = {
	"off", "on", "both", NULL
};
static const char *options_table_pane_status_list[] = {
	"off", "top", "bottom", NULL
};
static const char *options_table_pane_lines_list[] = {
	"single", "double", "heavy", "simple", "number", NULL
};
static const char *options_table_set_clipboard_list[] = {
	"off", "external", "on", NULL
};
static const char *options_table_window_size_list[] = {
	"largest", "smallest", "manual", "latest", NULL
};
static const char *options_table_remain_on_exit_list[] = {
	"off", "on", "failed", NULL
};
static const char *options_table_detach_on_destroy_list[] = {
	"off", "on", "no-detached", NULL
};
static const char *options_table_extended_keys_list[] = {
	"off", "on", "always", NULL
};

/* Status line format. */
#define OPTIONS_TABLE_STATUS_FORMAT1 \
	"#[align=left range=left #{status-left-style}]" \
	"#[push-default]" \
	"#{T;=/#{status-left-length}:status-left}" \
	"#[pop-default]" \
	"#[norange default]" \
	"#[list=on align=#{status-justify}]" \
	"#[list=left-marker]<#[list=right-marker]>#[list=on]" \
	"#{W:" \
		"#[range=window|#{window_index} " \
			"#{window-status-style}" \
			"#{?#{&&:#{window_last_flag}," \
				"#{!=:#{window-status-last-style},default}}, " \
				"#{window-status-last-style}," \
			"}" \
			"#{?#{&&:#{window_bell_flag}," \
				"#{!=:#{window-status-bell-style},default}}, " \
				"#{window-status-bell-style}," \
				"#{?#{&&:#{||:#{window_activity_flag}," \
					     "#{window_silence_flag}}," \
					"#{!=:" \
					"#{window-status-activity-style}," \
					"default}}, " \
					"#{window-status-activity-style}," \
				"}" \
			"}" \
		"]" \
		"#[push-default]" \
		"#{T:window-status-format}" \
		"#[pop-default]" \
		"#[norange default]" \
		"#{?window_end_flag,,#{window-status-separator}}" \
	"," \
		"#[range=window|#{window_index} list=focus " \
			"#{?#{!=:#{window-status-current-style},default}," \
				"#{window-status-current-style}," \
				"#{window-status-style}" \
			"}" \
			"#{?#{&&:#{window_last_flag}," \
				"#{!=:#{window-status-last-style},default}}, " \
				"#{window-status-last-style}," \
			"}" \
			"#{?#{&&:#{window_bell_flag}," \
				"#{!=:#{window-status-bell-style},default}}, " \
				"#{window-status-bell-style}," \
				"#{?#{&&:#{||:#{window_activity_flag}," \
					     "#{window_silence_flag}}," \
					"#{!=:" \
					"#{window-status-activity-style}," \
					"default}}, " \
					"#{window-status-activity-style}," \
				"}" \
			"}" \
		"]" \
		"#[push-default]" \
		"#{T:window-status-current-format}" \
		"#[pop-default]" \
		"#[norange list=on default]" \
		"#{?window_end_flag,,#{window-status-separator}}" \
	"}" \
	"#[nolist align=right range=right #{status-right-style}]" \
	"#[push-default]" \
	"#{T;=/#{status-right-length}:status-right}" \
	"#[pop-default]" \
	"#[norange default]"
#define OPTIONS_TABLE_STATUS_FORMAT2 \
	"#[align=centre]#{P:#{?pane_active,#[reverse],}" \
	"#{pane_index}[#{pane_width}x#{pane_height}]#[default] }"
static const char *options_table_status_format_default[] = {
	OPTIONS_TABLE_STATUS_FORMAT1, OPTIONS_TABLE_STATUS_FORMAT2, NULL
};

/* Helpers for hook options. */
#define OPTIONS_TABLE_HOOK(hook_name, default_value) \
	{ .name = hook_name, \
	  .type = OPTIONS_TABLE_COMMAND, \
	  .scope = OPTIONS_TABLE_SESSION, \
	  .flags = OPTIONS_TABLE_IS_ARRAY|OPTIONS_TABLE_IS_HOOK, \
	  .default_str = default_value,	\
	  .separator = "" \
	}

#define OPTIONS_TABLE_PANE_HOOK(hook_name, default_value) \
	{ .name = hook_name, \
	  .type = OPTIONS_TABLE_COMMAND, \
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE, \
	  .flags = OPTIONS_TABLE_IS_ARRAY|OPTIONS_TABLE_IS_HOOK, \
	  .default_str = default_value,	\
	  .separator = "" \
	}

#define OPTIONS_TABLE_WINDOW_HOOK(hook_name, default_value) \
	{ .name = hook_name, \
	  .type = OPTIONS_TABLE_COMMAND, \
	  .scope = OPTIONS_TABLE_WINDOW, \
	  .flags = OPTIONS_TABLE_IS_ARRAY|OPTIONS_TABLE_IS_HOOK, \
	  .default_str = default_value,	\
	  .separator = "" \
	}

/* Map of name conversions. */
const struct options_name_map options_other_names[] = {
	{ "display-panes-color", "display-panes-colour" },
	{ "display-panes-active-color", "display-panes-active-colour" },
	{ "clock-mode-color", "clock-mode-colour" },
	{ NULL, NULL }
};

/* Top-level options. */
const struct options_table_entry options_table[] = {
	/* Server options. */
	{ .name = "backspace",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = '\177',
	  .text = "The key to send for backspace."
	},

	{ .name = "buffer-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 50,
	  .text = "The maximum number of automatic buffers. "
		  "When this is reached, the oldest buffer is deleted."
	},

	{ .name = "command-alias",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "split-pane=split-window,"
			 "splitp=split-window,"
			 "server-info=show-messages -JT,"
			 "info=show-messages -JT,"
			 "choose-window=choose-tree -w,"
			 "choose-session=choose-tree -s",
	  .separator = ",",
	  .text = "Array of command aliases. "
		  "Each entry is an alias and a command separated by '='."
	},

	{ .name = "copy-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "",
	  .text = "Shell command run when text is copied. "
		  "If empty, no command is run."
	},

	{ .name = "default-terminal",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "screen",
	  .text = "Default for the 'TERM' environment variable."
	},

	{ .name = "editor",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = _PATH_VI,
	  .text = "Editor run to edit files."
	},

	{ .name = "escape-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 500,
	  .text = "Time to wait before assuming a key is Escape."
	},

	{ .name = "exit-empty",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 1,
	  .text = "Whether the server should exit if there are no sessions."
	},

	{ .name = "exit-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 0,
	  .text = "Whether the server should exit if there are no attached "
		  "clients."
	},

	{ .name = "extended-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SERVER,
	  .choices = options_table_extended_keys_list,
	  .default_num = 0,
	  .text = "Whether to request extended key sequences from terminals "
	          "that support it."
	},

	{ .name = "focus-events",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 0,
	  .text = "Whether to send focus events to applications."
	},

	{ .name = "history-file",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "",
	  .text = "Location of the command prompt history file. "
		  "Empty does not write a history file."
	},

	{ .name = "message-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 1000,
	  .text = "Maximum number of server messages to keep."
	},

	{ .name = "set-clipboard",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SERVER,
	  .choices = options_table_set_clipboard_list,
	  .default_num = 1,
	  .text = "Whether to attempt to set the system clipboard ('on' or "
		  "'external') and whether to allow applications to create "
		  "paste buffers with an escape sequence ('on' only)."
	},

	{ .name = "terminal-overrides",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "",
	  .separator = ",",
	  .text = "List of terminal capabilities overrides."
	},

	{ .name = "terminal-features",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "xterm*:clipboard:ccolour:cstyle:focus:title,"
			 "screen*:title",
	  .separator = ",",
	  .text = "List of terminal features, used if they cannot be "
		  "automatically detected."
	},

	{ .name = "user-keys",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "",
	  .separator = ",",
	  .text = "User key assignments. "
		  "Each sequence in the list is translated into a key: "
		  "'User0', 'User1' and so on."
	},

	/* Session options. */
	{ .name = "activity-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_OTHER,
	  .text = "Action to take on an activity alert."
	},

	{ .name = "assume-paste-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 1,
	  .unit = "milliseconds",
	  .text = "Maximum time between input to assume it pasting rather "
		  "than typing."
	},

	{ .name = "base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0,
	  .text = "Default index of the first window in each session."
	},

	{ .name = "bell-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_ANY,
	  .text = "Action to take on a bell alert."
	},

	{ .name = "default-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "",
	  .text = "Default command to run in new panes. If empty, a shell is "
		  "started."
	},

	{ .name = "default-shell",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = _PATH_BSHELL,
	  .text = "Location of default shell."
	},

	{ .name = "default-size",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .pattern = "[0-9]*x[0-9]*",
	  .default_str = "80x24",
	  .text = "Initial size of new sessions."
	},

	{ .name = "destroy-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .text = "Whether to destroy sessions when they have no attached "
		  "clients."
	},

	{ .name = "detach-on-destroy",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_detach_on_destroy_list,
	  .default_num = 1,
	  .text = "Whether to detach when a session is destroyed, or switch "
		  "the client to another session if any exist."
	},

	{ .name = "display-panes-active-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 1,
	  .text = "Colour of the active pane for 'display-panes'."
	},

	{ .name = "display-panes-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 4,
	  .text = "Colour of not active panes for 'display-panes'."
	},

	{ .name = "display-panes-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 1000,
	  .unit = "milliseconds",
	  .text = "Time for which 'display-panes' should show pane numbers."
	},

	{ .name = "display-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 750,
	  .unit = "milliseconds",
	  .text = "Time for which status line messages should appear."
	},

	{ .name = "history-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 2000,
	  .unit = "lines",
	  .text = "Maximum number of lines to keep in the history for each "
		  "pane. "
		  "If changed, the new value applies only to new panes."
	},

	{ .name = "key-table",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "root",
	  .text = "Default key table. "
		  "Key presses are first looked up in this table."
	},

	{ .name = "lock-after-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0,
	  .unit = "seconds",
	  .text = "Time after which a client is locked if not used."
	},

	{ .name = "lock-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "lock -np",
	  .text = "Shell command to run to lock a client."
	},

	{ .name = "message-command-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=black,fg=yellow",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the command prompt when in command mode, if "
		  "'mode-keys' is set to 'vi'."
	},

	{ .name = "message-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=yellow,fg=black",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the command prompt."
	},

	{ .name = "mouse",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .text = "Whether the mouse is recognised and mouse key bindings are "
		  "executed. "
		  "Applications inside panes can use the mouse even when 'off'."
	},

	{ .name = "prefix",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = '\002',
	  .text = "The prefix key."
	},

	{ .name = "prefix2",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = KEYC_NONE,
	  .text = "A second prefix key."
	},

	{ .name = "renumber-windows",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .text = "Whether windows are automatically renumbered rather than "
		  "leaving gaps."
	},

	{ .name = "repeat-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 500,
	  .unit = "milliseconds",
	  .text = "Time to wait for a key binding to repeat, if it is bound "
		  "with the '-r' flag."
	},

	{ .name = "set-titles",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .text = "Whether to set the terminal title, if supported."
	},

	{ .name = "set-titles-string",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "#S:#I:#W - \"#T\" #{session_alerts}",
	  .text = "Format of the terminal title to set."
	},

	{ .name = "silence-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_OTHER,
	  .text = "Action to take on a silence alert."
	},

	{ .name = "status",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_list,
	  .default_num = 1,
	  .text = "Number of lines in the status line."
	},

	{ .name = "status-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 8,
	  .text = "Background colour of the status line. This option is "
		  "deprecated, use 'status-style' instead."
	},

	{ .name = "status-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 8,
	  .text = "Foreground colour of the status line. This option is "
		  "deprecated, use 'status-style' instead."
	},

	{ .name = "status-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_arr = options_table_status_format_default,
	  .text = "Formats for the status lines. "
		  "Each array member is the format for one status line. "
		  "The default status line is made up of several components "
		  "which may be configured individually with other option such "
		  "as 'status-left'."
	},

	{ .name = "status-interval",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 15,
	  .unit = "seconds",
	  .text = "Number of seconds between status line updates."
	},

	{ .name = "status-justify",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_justify_list,
	  .default_num = 0,
	  .text = "Position of the window list in the status line."
	},

	{ .name = "status-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_keys_list,
	  .default_num = MODEKEY_EMACS,
	  .text = "Key set to use at the command prompt."
	},

	{ .name = "status-left",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "[#{session_name}] ",
	  .text = "Contents of the left side of the status line."
	},

	{ .name = "status-left-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 10,
	  .text = "Maximum width of the left side of the status line."
	},

	{ .name = "status-left-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the left side of the status line."
	},

	{ .name = "status-position",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_position_list,
	  .default_num = 1,
	  .text = "Position of the status line."
	},

	{ .name = "status-right",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "#{?window_bigger,"
			 "[#{window_offset_x}#,#{window_offset_y}] ,}"
			 "\"#{=21:pane_title}\" %H:%M %d-%b-%y",
	  .text = "Contents of the right side of the status line."

	},

	{ .name = "status-right-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 40,
	  .text = "Maximum width of the right side of the status line."
	},

	{ .name = "status-right-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the right side of the status line."
	},

	{ .name = "status-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=green,fg=black",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the status line."
	},

	{ .name = "update-environment",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "DISPLAY KRB5CCNAME SSH_ASKPASS SSH_AUTH_SOCK "
			 "SSH_AGENT_PID SSH_CONNECTION WINDOWID XAUTHORITY",
	  .text = "List of environment variables to update in the session "
		  "environment when a client is attached."
	},

	{ .name = "visual-activity",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF,
	  .text = "How activity alerts should be shown: a message ('on'), "
		  "a message and a bell ('both') or nothing ('off')."
	},

	{ .name = "visual-bell",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF,
	  .text = "How bell alerts should be shown: a message ('on'), "
		  "a message and a bell ('both') or nothing ('off')."
	},

	{ .name = "visual-silence",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF,
	  .text = "How silence alerts should be shown: a message ('on'), "
		  "a message and a bell ('both') or nothing ('off')."
	},

	{ .name = "word-separators",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = " ",
	  .text = "Characters considered to separate words."
	},

	/* Window options. */
	{ .name = "aggressive-resize",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0,
	  .text = "When 'window-size' is 'smallest', whether the maximum size "
		  "of a window is the smallest attached session where it is "
		  "the current window ('on') or the smallest session it is "
		  "linked to ('off')."
	},

	{ .name = "allow-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_num = 0,
	  .text = "Whether applications are allowed to use the escape sequence "
		  "to rename windows."
	},

	{ .name = "alternate-screen",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_num = 1,
	  .text = "Whether applications are allowed to use the alternate "
		  "screen."
	},

	{ .name = "automatic-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1,
	  .text = "Whether windows are automatically renamed."
	},

	{ .name = "automatic-rename-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#{?pane_in_mode,[tmux],#{pane_current_command}}"
			 "#{?pane_dead,[dead],}",
	  .text = "Format used to automatically rename windows."
	},

	{ .name = "clock-mode-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 4,
	  .text = "Colour of the clock in clock mode."
	},

	{ .name = "clock-mode-style",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_clock_mode_style_list,
	  .default_num = 1,
	  .text = "Time format of the clock in clock mode."
	},

	{ .name = "copy-mode-match-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "bg=cyan,fg=black",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of search matches in copy mode."
	},

	{ .name = "copy-mode-current-match-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "bg=magenta,fg=black",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the current search match in copy mode."
	},

	{ .name = "copy-mode-mark-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "bg=red,fg=black",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the marked line in copy mode."
	},

	{ .name = "main-pane-height",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "24",
	  .text = "Height of the main pane in the 'main-horizontal' layout. "
		  "This may be a percentage, for example '10%'."
	},

	{ .name = "main-pane-width",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "80",
	  .text = "Width of the main pane in the 'main-vertical' layout. "
		  "This may be a percentage, for example '10%'."
	},

	{ .name = "mode-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_mode_keys_list,
	  .default_num = MODEKEY_EMACS,
	  .text = "Key set used in copy mode."
	},

	{ .name = "mode-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "bg=yellow,fg=black",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of indicators and highlighting in modes."
	},

	{ .name = "monitor-activity",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0,
	  .text = "Whether an alert is triggered by activity."
	},

	{ .name = "monitor-bell",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1,
	  .text = "Whether an alert is triggered by a bell."
	},

	{ .name = "monitor-silence",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0,
	  .text = "Time after which an alert is triggered by silence. "
		  "Zero means no alert."

	},

	{ .name = "other-pane-height",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "0",
	  .text = "Height of the other panes in the 'main-horizontal' layout. "
		  "This may be a percentage, for example '10%'."
	},

	{ .name = "other-pane-width",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "0",
	  .text = "Height of the other panes in the 'main-vertical' layout. "
		  "This may be a percentage, for example '10%'."
	},

	{ .name = "pane-active-border-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#{?pane_in_mode,fg=yellow,#{?synchronize-panes,fg=red,fg=green}}",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the active pane border."
	},

	{ .name = "pane-base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = USHRT_MAX,
	  .default_num = 0,
	  .text = "Index of the first pane in each window."
	},

	{ .name = "pane-border-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#{?pane_active,#[reverse],}#{pane_index}#[default] "
			 "\"#{pane_title}\"",
	  .text = "Format of text in the pane status lines."
	},

	{ .name = "pane-border-lines",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_pane_lines_list,
	  .default_num = PANE_LINES_SINGLE,
	  .text = "Type of the pane type lines."
	},

	{ .name = "pane-border-status",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_pane_status_list,
	  .default_num = PANE_STATUS_OFF,
	  .text = "Position of the pane status lines."
	},

	{ .name = "pane-border-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the pane status lines."
	},

	{ .name = "remain-on-exit",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .choices = options_table_remain_on_exit_list,
	  .default_num = 0,
	  .text = "Whether panes should remain ('on') or be automatically "
		  "killed ('off' or 'failed') when the program inside exits."
	},

	{ .name = "synchronize-panes",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_num = 0,
	  .text = "Whether typing should be sent to all panes simultaneously."
	},

	{ .name = "window-active-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Default style of the active pane."
	},

	{ .name = "window-size",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_window_size_list,
	  .default_num = WINDOW_SIZE_LATEST,
	  .text = "How window size is calculated. "
		  "'latest' uses the size of the most recently used client, "
		  "'largest' the largest client, 'smallest' the smallest "
		  "client and 'manual' a size set by the 'resize-window' "
		  "command."
	},

	{ .name = "window-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Default style of panes that are not the active pane."
	},

	{ .name = "window-status-activity-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "reverse",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of windows in the status line with an activity alert."
	},

	{ .name = "window-status-bell-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "reverse",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of windows in the status line with a bell alert."
	},

	{ .name = "window-status-current-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#I:#W#{?window_flags,#{window_flags}, }",
	  .text = "Format of the current window in the status line."
	},

	{ .name = "window-status-current-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the current window in the status line."
	},

	{ .name = "window-status-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#I:#W#{?window_flags,#{window_flags}, }",
	  .text = "Format of windows in the status line, except the current "
		  "window."
	},

	{ .name = "window-status-last-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of the last window in the status line."
	},

	{ .name = "window-status-separator",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = " ",
	  .text = "Separator between windows in the status line."
	},

	{ .name = "window-status-style",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default",
	  .flags = OPTIONS_TABLE_IS_STYLE,
	  .separator = ",",
	  .text = "Style of windows in the status line, except the current and "
		  "last windows."
	},

	{ .name = "wrap-search",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1,
	  .text = "Whether searching in copy mode should wrap at the top or "
		  "bottom."
	},

	{ .name = "xterm-keys", /* no longer used */
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1,
	  .text = "Whether xterm-style function key sequences should be sent. "
	          "This option is no longer used."
	},

	/* Hook options. */
	OPTIONS_TABLE_HOOK("after-bind-key", ""),
	OPTIONS_TABLE_HOOK("after-capture-pane", ""),
	OPTIONS_TABLE_HOOK("after-copy-mode", ""),
	OPTIONS_TABLE_HOOK("after-display-message", ""),
	OPTIONS_TABLE_HOOK("after-display-panes", ""),
	OPTIONS_TABLE_HOOK("after-kill-pane", ""),
	OPTIONS_TABLE_HOOK("after-list-buffers", ""),
	OPTIONS_TABLE_HOOK("after-list-clients", ""),
	OPTIONS_TABLE_HOOK("after-list-keys", ""),
	OPTIONS_TABLE_HOOK("after-list-panes", ""),
	OPTIONS_TABLE_HOOK("after-list-sessions", ""),
	OPTIONS_TABLE_HOOK("after-list-windows", ""),
	OPTIONS_TABLE_HOOK("after-load-buffer", ""),
	OPTIONS_TABLE_HOOK("after-lock-server", ""),
	OPTIONS_TABLE_HOOK("after-new-session", ""),
	OPTIONS_TABLE_HOOK("after-new-window", ""),
	OPTIONS_TABLE_HOOK("after-paste-buffer", ""),
	OPTIONS_TABLE_HOOK("after-pipe-pane", ""),
	OPTIONS_TABLE_HOOK("after-queue", ""),
	OPTIONS_TABLE_HOOK("after-refresh-client", ""),
	OPTIONS_TABLE_HOOK("after-rename-session", ""),
	OPTIONS_TABLE_HOOK("after-rename-window", ""),
	OPTIONS_TABLE_HOOK("after-resize-pane", ""),
	OPTIONS_TABLE_HOOK("after-resize-window", ""),
	OPTIONS_TABLE_HOOK("after-save-buffer", ""),
	OPTIONS_TABLE_HOOK("after-select-layout", ""),
	OPTIONS_TABLE_HOOK("after-select-pane", ""),
	OPTIONS_TABLE_HOOK("after-select-window", ""),
	OPTIONS_TABLE_HOOK("after-send-keys", ""),
	OPTIONS_TABLE_HOOK("after-set-buffer", ""),
	OPTIONS_TABLE_HOOK("after-set-environment", ""),
	OPTIONS_TABLE_HOOK("after-set-hook", ""),
	OPTIONS_TABLE_HOOK("after-set-option", ""),
	OPTIONS_TABLE_HOOK("after-show-environment", ""),
	OPTIONS_TABLE_HOOK("after-show-messages", ""),
	OPTIONS_TABLE_HOOK("after-show-options", ""),
	OPTIONS_TABLE_HOOK("after-split-window", ""),
	OPTIONS_TABLE_HOOK("after-unbind-key", ""),
	OPTIONS_TABLE_HOOK("alert-activity", ""),
	OPTIONS_TABLE_HOOK("alert-bell", ""),
	OPTIONS_TABLE_HOOK("alert-silence", ""),
	OPTIONS_TABLE_HOOK("client-attached", ""),
	OPTIONS_TABLE_HOOK("client-detached", ""),
	OPTIONS_TABLE_HOOK("client-resized", ""),
	OPTIONS_TABLE_HOOK("client-session-changed", ""),
	OPTIONS_TABLE_PANE_HOOK("pane-died", ""),
	OPTIONS_TABLE_PANE_HOOK("pane-exited", ""),
	OPTIONS_TABLE_PANE_HOOK("pane-focus-in", ""),
	OPTIONS_TABLE_PANE_HOOK("pane-focus-out", ""),
	OPTIONS_TABLE_PANE_HOOK("pane-mode-changed", ""),
	OPTIONS_TABLE_PANE_HOOK("pane-set-clipboard", ""),
	OPTIONS_TABLE_PANE_HOOK("pane-title-changed", ""),
	OPTIONS_TABLE_HOOK("session-closed", ""),
	OPTIONS_TABLE_HOOK("session-created", ""),
	OPTIONS_TABLE_HOOK("session-renamed", ""),
	OPTIONS_TABLE_HOOK("session-window-changed", ""),
	OPTIONS_TABLE_WINDOW_HOOK("window-layout-changed", ""),
	OPTIONS_TABLE_WINDOW_HOOK("window-linked", ""),
	OPTIONS_TABLE_WINDOW_HOOK("window-pane-changed", ""),
	OPTIONS_TABLE_WINDOW_HOOK("window-renamed", ""),
	OPTIONS_TABLE_WINDOW_HOOK("window-unlinked", ""),

	{ .name = NULL }
};
