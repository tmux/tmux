#!/bin/sh

# 882
# tmux inside if-shell itself should work

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

cat <<EOF >$TMP
if '$TMUX run "true"' 'set -s @done yes'
EOF

TERM=xterm $TMUX -f$TMP new -d "$TMUX show -vs @done >>$TMP" || exit 1
sleep 1 && [ "$(tail -1 $TMP)" = "yes" ] || exit 1

$TMUX has 2>/dev/null && exit 1

exit 0
