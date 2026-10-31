#!/bin/sh

. ./regress-common.inc

# new-session without clients should be the right size

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMP=$(mktemp)
trap 'rm -f "$TMP"; stop_server $TMUX || exit 1' 0 1 15

$TMUX -f/dev/null new -d </dev/null || exit 1
$TMUX ls -F "#{window_width} #{window_height}" >$TMP
printf "80 24\n"|cmp -s $TMP - || exit 1
stop_server $TMUX || exit 1

$TMUX -f/dev/null new -d -x 100 -y 50 </dev/null || exit 1
$TMUX ls -F "#{window_width} #{window_height}" >$TMP
printf "100 50\n"|cmp -s $TMP - || exit 1
stop_server $TMUX || exit 1

exit 0
