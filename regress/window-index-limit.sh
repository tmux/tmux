#!/bin/sh

# inserting a window next to the highest index

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestWIL$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX new -d -s one
$TMUX set -g base-index 2147483647
$TMUX neww -d -t one

$TMUX neww -d -a -t one:2147483647
$TMUX neww -d -b -t one:2147483647

echo $($TMUX lsw -t one -F'#{window_index}') >$TMP
(echo "0 2147483647"|cmp -s - $TMP) || exit 1
$TMUX kill-server 2>/dev/null

exit 0
