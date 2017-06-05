#!/bin/sh

# 883
# if-shell with an error should not core :-)

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

cat <<EOF >$TMP
if 'true' 'wibble wobble'
EOF

$TMUX -f$TMP new -d || exit 1
sleep 1
E=$($TMUX display -p '#{pane_in_mode}')
$TMUX kill-server 2>/dev/null
[ "$E" = "1" ] || exit 1

exit 0
