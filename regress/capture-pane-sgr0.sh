#!/bin/sh

# 884
# capture-pane should send colours after SGR 0

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX -f/dev/null new -d \
	"printf '\033[31;42;1mabc\033[0;31mdef'; $TMUX capturep -peS0 -E0 >$TMP"
sleep 1
printf '\033[1m\033[31m\033[42mabc\033[0m\033[31m\033[49mdef\033[39m\n'| \
	cmp - $TMP || exit 1

$TMUX has 2>/dev/null && exit 1

exit 0
