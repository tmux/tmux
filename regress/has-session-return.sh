#!/bin/sh

# 971
# has-session should return 1 on error

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX -f/dev/null has -tfoo </dev/null 2>/dev/null && exit 1
$TMUX -f/dev/null start\; has -tfoo </dev/null 2>/dev/null && exit 1
$TMUX -f/dev/null new -d\; has -tfoo </dev/null 2>/dev/null && exit 1
$TMUX -f/dev/null new -dsfoo\; has -tfoo </dev/null 2>/dev/null || exit 1
$TMUX kill-server 2>/dev/null

exit 0
