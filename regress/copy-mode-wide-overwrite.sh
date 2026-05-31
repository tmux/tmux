#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest-wide-overwrite"
$TMUX kill-server 2>/dev/null
trap "$TMUX kill-server 2>/dev/null" 0 1 15

$TMUX new -d -x40 -y10 \
      "printf '\350\241\250'; sleep 1; printf '\033[2G\347\225\214'; sleep 5" \
      || exit 1

$TMUX copy-mode || exit 1
sleep 2

$TMUX has-session || exit 1
exit 0
