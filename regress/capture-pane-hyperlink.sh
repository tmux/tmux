#!/bin/sh

# capture-pane -e for OSC 8 hyperlink

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
TMP=$(mktemp)
TMP2=$(mktemp)
trap "rm -f $TMP $TMP2" 0 1 15
$TMUX kill-server 2>/dev/null

do_test() {
  $TMUX -f/dev/null new -d "
  printf '$1'
  $TMUX capturep -peS0 -E1 >$TMP"
  echo $2 > $TMP2
  sleep 1
  cmp $TMP $TMP2 || exit 1
  return 0
}

do_test '\033]8;id=1;https://github.com\033\\test1\033]8;;\033\\\n' '\033]8;id=1;https://github.com\033\\test1\033]8;;\033\\\n' || exit 1
do_test '\033]8;;https://github.com/tmux/tmux\033\\test1\033]8;;\033\\\n' '\033]8;;https://github.com/tmux/tmux\033\\test1\033]8;;\033\\\n' || exit 1

$TMUX has 2>/dev/null && exit 1

exit 0
