#!/bin/sh

# 869
# new with no client (that is, from the config file) should imply -d and
# not attach

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

cat <<EOF >$TMP
new -stest
EOF

$TMUX -f$TMP start || exit 1
sleep 1 && $TMUX has -t=test: || exit 1
$TMUX kill-server 2>/dev/null

exit 0
