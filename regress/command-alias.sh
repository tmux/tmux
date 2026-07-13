#!/bin/sh

# command-alias expansion

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

$TMUX new-session -d -sfoo || exit 1
$TMUX split-window -d -tfoo:0.0 || exit 1
$TMUX set -s command-alias[100] zoom='resize-pane -Z' || exit 1
$TMUX zoom -tfoo:0.0 || exit 1
[ "$($TMUX display-message -p -tfoo:0.0 '#{window_zoomed_flag}')" = 1 ] || exit 1

$TMUX kill-server 2>/dev/null

exit 0
