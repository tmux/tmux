#!/bin/sh

# when we kill a session, processes running in it should be killed

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX new -d 'sleep 1000' || exit 1
P=$($TMUX display -pt0:0.0 '#{pane_pid}')
$TMUX new -d || exit 1
sleep 1
$TMUX kill-session -t0:
sleep 1
kill -0 $P 2>/dev/null && exit 1
$TMUX kill-server 2>/dev/null

exit 0
