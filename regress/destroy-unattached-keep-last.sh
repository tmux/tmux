#!/bin/sh

# destroy-unattached must check every session, including the one after a
# session it destroys.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0 1 15

$TMUX new -d -s base || exit 1
$TMUX new -d -s x || exit 1
$TMUX new -d -s y -t x || exit 1
$TMUX new -d -s z -t x || exit 1
$TMUX set -g destroy-unattached keep-last || exit 1

[ "$($TMUX ls -F '#{session_name}' | tr '\n' ' ')" = "base z " ] || exit 1

exit 0
