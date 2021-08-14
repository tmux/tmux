#!/bin/sh

# new session command

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

cat <<EOF >$TMP
new sleep 101
new -- sleep 102
new "sleep 103"
EOF

$TMUX -f$TMP start
[ $($TMUX ls|wc -l) -eq 3 ] || exit 1
$TMUX kill-server 2>/dev/null

exit 0
