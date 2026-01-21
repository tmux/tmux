#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

# testing switch-client
$TMUX new-session -s two || exit 1
sleep 1 && $TMUX rename one || exit 1
# $TMUX new-session -ds three

$TMUX kill-server 2>/dev/null

exit 0

