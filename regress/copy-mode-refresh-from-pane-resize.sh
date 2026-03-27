#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX new -d -x40 -y5 \
    "for i in \$(seq 1 200); do echo \"line \$i\"; done; cat" || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g history-limit 500 || exit 1

sleep 1

$TMUX copy-mode
$TMUX send-keys -X history-top

$TMUX set -g history-limit 20 || exit 1
$TMUX resize-window -x40 -y6 || exit 1

$TMUX send-keys -X refresh-from-pane

$TMUX display-message -p "ok" >/dev/null || exit 1

$TMUX kill-server 2>/dev/null
exit 0
