#!/bin/sh

# Tests for neighbor-aware window format variables in #{W:...} loops.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new-session -d || exit 1

# test_wformat $format $expected_result
# Neighbor variables only exist inside the window loop.
test_wformat()
{
	fmt="$1"
	exp="$2"

	out=$($TMUX display-message -p "$fmt")

	if [ "$out" != "$exp" ]; then
		echo "Format test failed for '$fmt'."
		echo "Expected: '$exp'"
		echo "But got   '$out'"
		exit 1
	fi
}

# Single window (no neighbors)
test_wformat "#{W:#{window_before_active}}" "0"
test_wformat "#{W:#{window_after_active}}" "0"
test_wformat "#{W:#{next_window_active}}" ""
test_wformat "#{W:#{prev_window_active}}" ""

# Two windows: adjacency flags
$TMUX new-window

# Window 1 is now active.
test_wformat "#{W:#{window_index}=#{window_before_active} }" "0=1 1=0 "
test_wformat "#{W:#{window_index}=#{window_after_active} }" "0=0 1=0 "

# Switch to window 0.
$TMUX select-window -t :0
test_wformat "#{W:#{window_index}=#{window_before_active} }" "0=0 1=0 "
test_wformat "#{W:#{window_index}=#{window_after_active} }" "0=0 1=1 "

# Two windows: next/prev window index and active
test_wformat "#{W:#{window_index}:next_idx=#{next_window_index} }" "0:next_idx=1 1:next_idx= "
test_wformat "#{W:#{window_index}:prev_idx=#{prev_window_index} }" "0:prev_idx= 1:prev_idx=0 "
test_wformat "#{W:#{window_index}:next_act=#{next_window_active} }" "0:next_act=0 1:next_act= "

$TMUX select-window -t :1
test_wformat "#{W:#{window_index}:next_act=#{next_window_active} }" "0:next_act=1 1:next_act= "

# User option propagation (next_/prev_ prefixes)
$TMUX set -wt :0 @color "red"
$TMUX set -wt :1 @color "blue"
test_wformat "#{W:#{window_index}:#{next_color} }" "0:blue 1: "
test_wformat "#{W:#{window_index}:#{prev_color} }" "0: 1:red "

# Non-@ options are not propagated.
test_wformat "#{W:#{next_automatic-rename}}" ""

# Pre-expanded separator (window_status_separator)
$TMUX set -g window-status-separator " | "
test_wformat "#{W:#{window_index}#{?loop_last_flag,,#{window_status_separator}}}" "0 | 1"

# Conditional separator using neighbor variables.
$TMUX set -g window-status-separator "#{?window_before_active,>,|}"
$TMUX select-window -t :0
test_wformat "#{W:#{window_index}#{?loop_last_flag,,#{window_status_separator}}}" "0|1"
$TMUX select-window -t :1
test_wformat "#{W:#{window_index}#{?loop_last_flag,,#{window_status_separator}}}" "0>1"

# Three windows: middle window active
$TMUX select-window -t :0
$TMUX new-window    # creates window 2, now active
$TMUX set -wt :0 @bg "red"
$TMUX set -wt :1 @bg "green"
$TMUX set -wt :2 @bg "blue"
$TMUX set -g window-status-separator "#{?next_window_active,>,|}"
$TMUX select-window -t :1
test_wformat "#{W:#{window_index}#{?loop_last_flag,,#{window_status_separator}}}" "0>1|2"
$TMUX select-window -t :0
test_wformat "#{W:#{window_index}#{?loop_last_flag,,#{window_status_separator}}}" "0|1|2"

$TMUX kill-server 2>/dev/null
exit 0
