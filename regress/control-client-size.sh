#!/bin/sh

# 947
# size in control mode should change after refresh-client -C

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
OUT=$(mktemp)
trap "rm -f $TMP $OUT" 0 1 15

$TMUX -f/dev/null new -d </dev/null || exit 1
sleep 1
cat <<EOF|$TMUX -C a >$TMP
ls -F':#{session_width} #{session_height}'
refresh -C 100,50
ls -F':#{session_width} #{session_height}'
EOF
grep ^: $TMP >$OUT
printf ":80 24\n:100 50\n"|cmp -s $OUT || exit 1
$TMUX kill-server 2>/dev/null

$TMUX -f/dev/null new -d </dev/null || exit 1
sleep 1
cat <<EOF|$TMUX -C a >$TMP
ls -F':#{session_width} #{session_height}'
refresh -C 80,24
ls -F':#{session_width} #{session_height}'
EOF
grep ^: $TMP >$OUT
printf ":80 24\n:80 24\n"|cmp -s $OUT || exit 1
$TMUX kill-server 2>/dev/null

exit 0
