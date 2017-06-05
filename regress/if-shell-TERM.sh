#!/bin/sh

# 882
# TERM should come from outside tmux for if-shell from the config file

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

cat <<EOF >$TMP
if '[ "\$TERM" = "xterm" ]' \
	'set -g default-terminal "vt220"' \
	'set -g default-terminal "ansi"'
EOF

TERM=xterm $TMUX -f$TMP new -d "echo \"#\$TERM\" >>$TMP" || exit 1
sleep 1 && [ "$(tail -1 $TMP)" = "#vt220" ] || exit 1

TERM=screen $TMUX -f$TMP new -d "echo \"#\$TERM\" >>$TMP" || exit 1
sleep 1 && [ "$(tail -1 $TMP)" = "#ansi" ] || exit 1

$TMUX has 2>/dev/null && exit 1

exit 0
