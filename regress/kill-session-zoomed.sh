#!/bin/sh

# Killing a session whose window is zoomed must not crash the server:
# window_destroy used to unzoom the window, which resized the panes and
# fired pane-resized with the window in the payload, and releasing that
# reference destroyed the window a second time.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

$TMUX -f/dev/null new -d -sfoo || exit 1
$TMUX split-window -d -h -tfoo:0 || exit 1
$TMUX resize-pane -Z -tfoo:0 || exit 1
$TMUX new -d -sbar || exit 1
$TMUX kill-session -tfoo || exit 1
$TMUX has-session -tbar || exit 1

# The same with the zoomed window in a session killed by kill-server.
$TMUX new -d -sbaz || exit 1
$TMUX split-window -d -v -tbaz:0 || exit 1
$TMUX resize-pane -Z -tbaz:0 || exit 1
$TMUX kill-server || exit 1

exit 0
