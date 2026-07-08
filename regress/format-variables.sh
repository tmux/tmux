#!/bin/sh

# Tests that every format variable listed in tmux(1) (the format_table in
# format.c) can be expanded without crashing the server, and checks the value
# of a stable subset.
#
# The main point is coverage and crash-safety: each variable is expanded in a
# rich context - a real attached client (from a nested tmux), a control-mode
# client, a grouped session, two windows with a bell alert, a window with two
# panes running cat, a paste buffer and options - so the per-variable callbacks
# actually run.  format-modifiers.sh covers the modifier machinery; this covers
# the variable callbacks.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
# A second server on its own socket provides a real terminal (an inner client
# attached inside one of its panes) so client terminal variables are populated.
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

# Every variable name in format_table[].  Kept as a plain word list so it can be
# iterated with normal shell word splitting.
NAMES="
active_window_index
alternate_on
alternate_saved_x
alternate_saved_y
bracket_paste_flag
buffer_created
buffer_full
buffer_mode_format
buffer_name
buffer_sample
buffer_size
client_activity
client_cell_height
client_cell_width
client_colours
client_control_mode
client_created
client_discarded
client_flags
client_height
client_key_table
client_last_session
client_mode_format
client_name
client_pid
client_prefix
client_readonly
client_session
client_termfeatures
client_termname
client_termtype
client_theme
client_tty
client_uid
client_user
client_utf8
client_width
client_written
config_files
cursor_blinking
cursor_character
cursor_colour
cursor_flag
cursor_shape
cursor_very_visible
cursor_x
cursor_y
history_all_bytes
history_bytes
history_limit
history_size
host
host_short
insert_flag
keypad_cursor_flag
keypad_flag
last_window_index
mouse_all_flag
mouse_any_flag
mouse_button_flag
mouse_hyperlink
mouse_line
mouse_pane
mouse_sgr_flag
mouse_standard_flag
mouse_status_line
mouse_status_range
mouse_utf8_flag
mouse_word
mouse_x
mouse_y
next_session_id
origin_flag
pane_active
pane_at_bottom
pane_at_left
pane_at_right
pane_at_top
pane_bg
pane_bottom
pane_current_command
pane_current_path
pane_dead
pane_dead_signal
pane_dead_status
pane_dead_time
pane_fg
pane_flags
pane_floating_flag
pane_format
pane_height
pane_id
pane_in_mode
pane_index
pane_input_off
pane_key_mode
pane_last
pane_left
pane_marked
pane_marked_set
pane_mode
pane_path
pane_pb_progress
pane_pb_state
pane_pid
pane_pipe
pane_pipe_pid
pane_right
pane_search_string
pane_start_command
pane_start_command_list
pane_start_path
pane_synchronized
pane_tabs
pane_title
pane_top
pane_tty
pane_unseen_changes
pane_width
pane_x
pane_y
pane_z
pane_zoomed_flag
pid
scroll_region_lower
scroll_region_upper
server_sessions
session_active
session_activity
session_activity_flag
session_alert
session_alerts
session_attached
session_attached_list
session_bell_flag
session_created
session_format
session_group
session_group_attached
session_group_attached_list
session_group_list
session_group_many_attached
session_group_size
session_grouped
session_id
session_last_attached
session_many_attached
session_marked
session_name
session_path
session_silence_flag
session_stack
session_windows
sixel_support
socket_path
start_time
synchronized_output_flag
tree_mode_format
uid
user
version
window_active
window_active_clients
window_active_clients_list
window_active_sessions
window_active_sessions_list
window_activity
window_activity_flag
window_bell_flag
window_bigger
window_cell_height
window_cell_width
window_end_flag
window_flags
window_format
window_height
window_id
window_index
window_last_flag
window_layout
window_linked
window_linked_sessions
window_linked_sessions_list
window_marked_flag
window_name
window_offset_x
window_offset_y
window_panes
window_raw_flags
window_silence_flag
window_stack_index
window_start_flag
window_visible_layout
window_width
window_zoomed_flag
wrap_flag
"

# test_var $name $expected [$extra_args...]
#
# Expand a single #{name} and compare against $expected.  Any extra arguments
# are passed straight to display-message (e.g. -c or -t).
test_var()
{
	name="$1"
	exp="$2"
	shift 2

	out=$($TMUX display-message "$@" -p "#{$name}")
	if [ "$out" != "$exp" ]; then
		echo "Variable test failed for '#{$name}'."
		echo "Expected: '$exp'"
		echo "But got   '$out'"
		exit 1
	fi
}

assert_alive()
{
	if [ "$($TMUX display-message -p alive)" != "alive" ]; then
		echo "Server did not survive: $1"
		exit 1
	fi
}

FIFO="${TMPDIR:-/tmp}/fmt-vars-$$"
HOLD=""
CC=""

cleanup()
{
	[ -n "$HOLD" ] && kill $HOLD 2>/dev/null
	[ -n "$CC" ] && kill $CC 2>/dev/null
	rm -f "$FIFO"
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
}
fail()
{
	echo "$1"
	cleanup
	exit 1
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

# A session "cov" with a window "win0" holding two panes running cat, plus a
# second window, an option and a paste buffer.
$TMUX new-session -d -s cov -x 80 -y 24 -n win0 'cat' || exit 1
$TMUX set -g automatic-rename off
$TMUX set -g monitor-bell on
$TMUX set -g monitor-activity on
$TMUX split-window -t cov:win0 -d 'cat' || exit 1
$TMUX new-window -d -t cov:1 -n win1 'cat' || exit 1
$TMUX set -g @opt 'optionvalue' || exit 1
$TMUX set-buffer -b buf0 'somebuffer' || exit 1

# A second session grouped with cov, so the session_group_* variables have real
# data to report.
$TMUX new-session -d -s cov2 -t cov || exit 1

sleep 1
$TMUX send-keys -t cov:win0.0 'some pane content' Enter
# Ring the bell in the non-current window so a bell alert is raised on the
# session (this populates session_alert/session_alerts and window_bell_flag).
$TMUX send-keys -t cov:win1.0 C-g
sleep 1

# Attach a control-mode client, held open by a background process keeping the
# write end of a FIFO open, so client_* variables have a client to read.
rm -f "$FIFO"
mkfifo "$FIFO" || exit 1
sleep 30 >"$FIFO" &
HOLD=$!
$TMUX -C attach -t cov <"$FIFO" >/dev/null 2>&1 &
CC=$!

# Attach a real client too: an inner tmux running inside a pane of the second
# server gets a genuine terminal, which populates the terminal-dependent client
# variables (client_termname, cursor_shape, the I modifier, ...).
$TMUX2 new-session -d -x 90 -y 30 "$TMUX attach -t cov" || exit 1
sleep 1

# The real (terminal) client, identified by not being in control mode.
RC=$($TMUX list-clients -F '#{client_control_mode} #{client_name}' |
    awk '$1==0 { print $2; exit }')
# The control client.
CLIENT=$($TMUX list-clients -F '#{client_control_mode} #{client_name}' |
    awk '$1==1 { print $2; exit }')
[ -n "$RC" ] || fail "No real client attached."
[ -n "$CLIENT" ] || fail "No control client attached."

# Expand every variable at once, with the real terminal client and a target
# pane in context, and confirm the server survives.  This runs every callback.
FMT=""
for n in $NAMES; do
	FMT="$FMT#{$n}"
done
$TMUX display-message -c "$RC" -t cov:win0.0 -p "$FMT" >/dev/null 2>&1
assert_alive "expanding all variables together"

# Expand each variable on its own too, so a crash can be pinned to one name.
for n in $NAMES; do
	$TMUX display-message -c "$RC" -t cov:win0.0 -p "#{$n}" >/dev/null 2>&1
	assert_alive "expanding #{$n}"
done

# Deterministic checks on stable variables (targeting pane 0 of window 0).
TGT="cov:win0.0"
test_var session_name "cov" -t "$TGT"
test_var window_name "win0" -t "$TGT"
test_var window_index "0" -t "$TGT"
test_var window_panes "2" -t "$TGT"
test_var session_windows "2" -t "$TGT"
test_var pane_index "0" -t "$TGT"
test_var pane_in_mode "0" -t "$TGT"
test_var pane_at_top "1" -t "$TGT"
test_var pane_at_left "1" -t "$TGT"
test_var last_window_index "1" -t "$TGT"
test_var pid "$($TMUX display-message -p '#{pid}')" -t "$TGT"
test_var pane_start_command "cat" -t "$TGT"
test_var pane_start_command_list "'cat'" -t "$TGT"

# The grouped session is reported as such.
test_var session_grouped "1" -t "cov:"
test_var session_group_size "2" -t "cov:"

# list-buffers -F formats each paste buffer (this fills in the paste-buffer
# format defaults).
if [ "$($TMUX list-buffers -F '#{buffer_name}=#{buffer_sample}')" != \
    "buf0=somebuffer" ]; then
	fail "Unexpected list-buffers format output."
fi

# Version reported by the variable matches tmux -V.
VER=$($TMUX -V | sed 's/^tmux //')
test_var version "$VER" -t "$TGT"

# Client variables from each kind of client.
test_var client_name "$CLIENT" -c "$CLIENT"
test_var client_control_mode "1" -c "$CLIENT"
test_var client_control_mode "0" -c "$RC"
test_var socket_path "$($TMUX display-message -p '#{socket_path}')" -c "$CLIENT"
# The real client has a terminal, so termcap/feature/environ queries work.
test_var "I/e:TERM" "$($TMUX display-message -c "$RC" -p '#{client_termname}')" \
    -c "$RC"
# Termcap and feature queries against a real terminal return a boolean.
case "$($TMUX display-message -c "$RC" -p '#{I/c:colors}')" in
0|1) ;;
*) fail "Unexpected #{I/c:colors} for real client." ;;
esac
case "$($TMUX display-message -c "$RC" -p '#{I/f:256}')" in
0|1) ;;
*) fail "Unexpected #{I/f:256} for real client." ;;
esac

# Time variables through the pretty and relative modifiers: start_time is the
# recent server start, exercising the "last 24 hours" and "just now" paths.
[ -n "$($TMUX display-message -p '#{t/p:start_time}')" ] ||
    fail "Empty #{t/p:start_time}."
[ -n "$($TMUX display-message -p '#{t/r:start_time}')" ] ||
    fail "Empty #{t/r:start_time}."

# pane_start_command_list quotes each argv word for sh, so evaluating the
# expansion reconstructs the original argv exactly - including words with
# quotes, spaces, newlines and empty words.  sh -c ignores the extra words
# (they become positional parameters), so the pane stays alive.  -u stops
# the server sanitizing the newline away when printing to a non-UTF-8
# client (the test runs without a locale in the environment).
$TMUX new-session -d -s quot -x 80 -y 24 -- sh -c 'sleep 100' arg0 \
    "it's a 'test'" 'two words' '' 'new
line' || fail "Failed to create quoting test session."
LIST=$($TMUX -u display-message -t 'quot:0.0' -p '#{pane_start_command_list}')
eval "set -- $LIST"
GOT=$(for a; do printf '<%s>' "$a"; done)
EXP=$(for a in sh -c 'sleep 100' arg0 "it's a 'test'" 'two words' '' 'new
line'; do printf '<%s>' "$a"; done)
if [ "$GOT" != "$EXP" ]; then
	echo "pane_start_command_list did not round-trip."
	echo "Expected: $EXP"
	echo "But got:  $GOT"
	fail "Expansion was: $LIST"
fi
# A pane started with the default shell has an empty start command.
$TMUX new-window -d -t 'quot:' || fail "Failed to create shell window."
test_var pane_start_command_list "" -t "quot:1.0"

cleanup
exit 0
