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
#include <paths.h>

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
	"left", "centre", "right", NULL
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
static const char *options_table_set_clipboard_list[] = {
	"off", "external", "on", NULL
};
static const char *options_table_window_size_list[] = {
	"largest", "smallest", "manual", "latest", NULL
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

/* Top-level options. */
const struct options_table_entry options_table[] = {
	/* Server options. */
	{ .name = "backspace",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = '\177',
	},

	{ .name = "buffer-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 50
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
	  .separator = ","
	},

	{ .name = "copy-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = ""
	},

	{ .name = "default-terminal",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "screen"
	},

	{ .name = "escape-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 500
	},

	{ .name = "exit-empty",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 1
	},

	{ .name = "exit-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 0
	},

	{ .name = "focus-events",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 0
	},

	{ .name = "history-file",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = ""
	},

	{ .name = "message-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 100
	},

	{ .name = "set-clipboard",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SERVER,
	  .choices = options_table_set_clipboard_list,
	  .default_num = 1
	},

	{ .name = "terminal-overrides",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "tmux*:XT,screen*:XT,xterm*:XT",
	  .separator = ","
	},

	{ .name = "terminal-features",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "xterm*:clipboard:ccolour:cstyle:title,"
	                 "screen*:title",
	  .separator = ","
	},

	{ .name = "user-keys",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "",
	  .separator = ","
	},

	/* Session options. */
	{ .name = "activity-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_OTHER
	},

	{ .name = "assume-paste-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 1,
	},

	{ .name = "base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "bell-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_ANY
	},

	{ .name = "default-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = ""
	},

	{ .name = "default-shell",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = _PATH_BSHELL
	},

	{ .name = "default-size",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .pattern = "[0-9]*x[0-9]*",
	  .default_str = "80x24"
	},

	{ .name = "destroy-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "detach-on-destroy",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 1
	},

	{ .name = "display-panes-active-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 1
	},

	{ .name = "display-panes-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 4
	},

	{ .name = "display-panes-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 1000
	},

	{ .name = "display-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 750
	},

	{ .name = "history-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 2000
	},

	{ .name = "key-table",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "root"
	},

	{ .name = "lock-after-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "lock-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "lock -np"
	},

	{ .name = "message-command-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=black,fg=yellow"
	},

	{ .name = "message-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=yellow,fg=black"
	},

	{ .name = "mouse",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "prefix",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = '\002',
	},

	{ .name = "prefix2",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = KEYC_NONE,
	},

	{ .name = "renumber-windows",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "repeat-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 500
	},

	{ .name = "set-titles",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "set-titles-string",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "#S:#I:#W - \"#T\" #{session_alerts}"
	},

	{ .name = "silence-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_OTHER
	},

	{ .name = "status",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_list,
	  .default_num = 1
	},

	{ .name = "status-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 2,
	},

	{ .name = "status-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	},

	{ .name = "status-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_arr = options_table_status_format_default,
	},

	{ .name = "status-interval",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 15
	},

	{ .name = "status-justify",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_justify_list,
	  .default_num = 0
	},

	{ .name = "status-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_keys_list,
	  .default_num = MODEKEY_EMACS
	},

	{ .name = "status-left",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "[#S] "
	},

	{ .name = "status-left-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 10
	},

	{ .name = "status-left-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "default"
	},

	{ .name = "status-position",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_position_list,
	  .default_num = 1
	},

	{ .name = "status-right",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "#{?window_bigger,"
	                 "[#{window_offset_x}#,#{window_offset_y}] ,}"
	                 "\"#{=21:pane_title}\" %H:%M %d-%b-%y"
	},

	{ .name = "status-right-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 40
	},

	{ .name = "status-right-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "default"
	},

	{ .name = "status-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=green,fg=black"
	},

	{ .name = "update-environment",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .flags = OPTIONS_TABLE_IS_ARRAY,
	  .default_str = "DISPLAY KRB5CCNAME SSH_ASKPASS SSH_AUTH_SOCK "
	  		 "SSH_AGENT_PID SSH_CONNECTION WINDOWID XAUTHORITY"
	},

	{ .name = "visual-activity",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF
	},

	{ .name = "visual-bell",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF
	},

	{ .name = "visual-silence",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF
	},

	{ .name = "word-separators",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = " "
	},

	/* Window options. */
	{ .name = "aggressive-resize",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "allow-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_num = 0
	},

	{ .name = "alternate-screen",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_num = 1
	},

	{ .name = "automatic-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = "automatic-rename-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#{?pane_in_mode,[tmux],#{pane_current_command}}"
			 "#{?pane_dead,[dead],}"
	},

	{ .name = "clock-mode-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 4
	},

	{ .name = "clock-mode-style",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_clock_mode_style_list,
	  .default_num = 1
	},

	{ .name = "main-pane-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 24
	},

	{ .name = "main-pane-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 80
	},

	{ .name = "mode-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_mode_keys_list,
	  .default_num = MODEKEY_EMACS
	},

	{ .name = "mode-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "bg=yellow,fg=black"
	},

	{ .name = "monitor-activity",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "monitor-bell",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = "monitor-silence",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "other-pane-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "other-pane-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "pane-active-border-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "fg=green"
	},

	{ .name = "pane-base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = USHRT_MAX,
	  .default_num = 0
	},

	{ .name = "pane-border-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#{?pane_active,#[reverse],}#{pane_index}#[default] "
			 "\"#{pane_title}\""
	},

	{ .name = "pane-border-status",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_pane_status_list,
	  .default_num = PANE_STATUS_OFF
	},

	{ .name = "pane-border-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "remain-on-exit",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_num = 0
	},

	{ .name = "synchronize-panes",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "window-active-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_str = "default"
	},

	{ .name = "window-size",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_window_size_list,
	  .default_num = WINDOW_SIZE_LATEST
	},

	{ .name = "window-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE,
	  .default_str = "default"
	},

	{ .name = "window-status-activity-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "reverse"
	},

	{ .name = "window-status-bell-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "reverse"
	},

	{ .name = "window-status-current-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#I:#W#{?window_flags,#{window_flags}, }"
	},

	{ .name = "window-status-current-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "window-status-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#I:#W#{?window_flags,#{window_flags}, }"
	},

	{ .name = "window-status-last-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "window-status-separator",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = " "
	},

	{ .name = "window-status-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "wrap-search",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = "xterm-keys",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
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
