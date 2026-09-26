#!/bin/sh

# renumber-windows with more windows than fit above base-index

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestRWL$$"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
CONF=$(mktemp)
trap "rm -f $TMP $CONF" 0 1 15

cat <<EOF >$CONF
set -g base-index 10
set -g renumber-windows on
EOF

$TMUX -f$CONF new -d -s a
$TMUX neww -d -t a
$TMUX neww -d -t a
echo $($TMUX lsw -t a -F'#{window_index}') >$TMP
(echo "10 11 12"|cmp -s - $TMP) || exit 1
$TMUX killw -t a:11
echo $($TMUX lsw -t a -F'#{window_index}') >$TMP
(echo "10 11"|cmp -s - $TMP) || exit 1
$TMUX kill-server 2>/dev/null

cat <<EOF >$CONF
set -g base-index 2147483646
set -g renumber-windows on
EOF

$TMUX -f$CONF new -d -s a
$TMUX neww -d -t a
$TMUX neww -d -t a
$TMUX neww -d -t a
echo $($TMUX lsw -t a -F'#{window_index}') >$TMP
(echo "0 1 2147483646 2147483647"|cmp -s - $TMP) || exit 1

# Three windows do not fit from 2147483646, so the indexes stay as they are.
$TMUX killw -t a:2147483647
echo $($TMUX lsw -t a -F'#{window_index}') >$TMP
(echo "0 1 2147483646"|cmp -s - $TMP) || exit 1
$TMUX kill-server 2>/dev/null

exit 0
