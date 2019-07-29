#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
TMUX2="$TEST_TMUX -Ltest2"
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX2 -f/dev/null new -d || exit 1
$TMUX2 set -as terminal-overrides ',*:xenl@' || exit 1
$TMUX2 set -g status-right 'RRR' || exit 1
$TMUX2 set -g status-left 'LLL' || exit 1
$TMUX2 set -g window-status-current-format 'WWW' || exit 1
$TMUX -f/dev/null new -x20 -y2 -d "$TMUX2 attach" || exit 1
sleep 1
$TMUX capturep -p|tail -1 >$TMP || exit 1
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
cat <<EOF|cmp -s $TMP - || exit 1
LLLWWW           RR
EOF

exit 0
