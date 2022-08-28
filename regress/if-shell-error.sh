#!/bin/sh

# 883
# if-shell with an error should not core :-)

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
OUT=$(mktemp)
trap "rm -f $TMP $OUT" 0 1 15

cat <<EOF >$TMP
if 'true' 'wibble wobble'
EOF

$TMUX -f$TMP -C new <<EOF >$OUT
EOF
grep -q "^%config-error $TMP:1: $TMP:1: unknown command: wibble$" $OUT
