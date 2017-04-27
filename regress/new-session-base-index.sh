#!/bin/sh

# new session base-index

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

cat <<EOF >$TMP
set -g base-index 100
new
set base-index 200
neww
EOF

$TMUX -f$TMP start
echo $($TMUX lsw -F'#{window_index}') >$TMP
(echo "100 200"|cmp -s - $TMP) || exit 1
$TMUX kill-server 2>/dev/null

exit 0
