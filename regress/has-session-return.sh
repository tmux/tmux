#!/bin/sh

. ./regress-common.inc

# 971
# has-session should return 1 on error

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
stop_server $TMUX || exit 1

$TMUX -f/dev/null has -tfoo </dev/null 2>/dev/null && exit 1
$TMUX -f/dev/null start \; set -s exit-empty off \; \
    has -tfoo </dev/null 2>/dev/null && exit 1
$TMUX -f/dev/null new -d\; has -tfoo </dev/null 2>/dev/null && exit 1
$TMUX -f/dev/null new -dsfoo\; has -tfoo </dev/null 2>/dev/null || exit 1
stop_server $TMUX || exit 1

exit 0
