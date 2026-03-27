#!/bin/sh
# Regression test for https://github.com/tmux/tmux/issues/4956
#
# When in copy mode with a large scroll offset (data->oy) and then resizing
# the terminal to reduce history, calling refresh-from-pane would trigger an
# unsigned integer underflow in window_copy_cmd_refresh_from_pane because
# data->oy could be larger than the new screen_hsize. This caused a crash via
# window_copy_clear_selection -> window_copy_find_length -> grid_line_length
# with a huge out-of-bounds row index.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"
$TMUX kill-server 2>/dev/null

# Use a large-ish history limit and a narrow, short window.
# The window command fills scrollback with many lines then sits idle.
$TMUX new -d -x40 -y5 \
    "for i in \$(seq 1 200); do echo \"line \$i\"; done; cat" || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g history-limit 500 || exit 1

# Give the fill command time to run.
sleep 0.3

# Enter copy mode and scroll to the very top of history.
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1

# Reduce the history limit dramatically so the backing grid shrinks.
# This makes data->oy (scroll offset) exceed the new screen_hsize,
# setting up the unsigned underflow in refresh-from-pane.
$TMUX set -g history-limit 20 || exit 1

# Resize the window to force a grid resize that trims history.
$TMUX resize-window -x40 -y6 || exit 1
sleep 0.1

# This used to crash the server with a segfault (SIGSEGV).
$TMUX send-keys -X refresh-from-pane || exit 1

# If the server is still alive the bug is fixed.
$TMUX display-message -p "ok" >/dev/null || exit 1

$TMUX kill-server 2>/dev/null
exit 0
