#!/bin/sh

# capture-pane -e for OSC 8 hyperlink

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMP=$(mktemp)
TMP2=$(mktemp)
trap "rm -f $TMP $TMP2" 0 1 15
$TMUX kill-server 2>/dev/null

do_test() {
  $TMUX -f/dev/null new -d "
  printf '$1'
  $TMUX capturep -peS0 -E1 >$TMP"
  printf "$2\n" > $TMP2
  sleep 1
  cmp $TMP $TMP2 || exit 1
  return 0
}

do_test '\033]8;id=1;https://github.com\033\\test1\033]8;;\033\\\n' '\033]8;id=1;https://github.com\033\\test1\033]8;;\033\\\n' || exit 1
do_test '\033]8;;https://github.com/tmux/tmux\033\\test1\033]8;;\033\\\n' '\033]8;;https://github.com/tmux/tmux\033\\test1\033]8;;\033\\\n' || exit 1

# A URI longer than an OAuth authorization URL is kept, one past the limit is
# dropped (the pane then has no hyperlink at all).
link_sequence() {
  printf '\\033]8;;%s\\033\\\\%s\\033]8;;\\033\\\\\\n' "$1" "$2"
}
LONG_URI=https://example.com/?$(head -c 1500 /dev/zero | tr '\0' a)
do_test "$(link_sequence "$LONG_URI" test2)" \
    "$(link_sequence "$LONG_URI" test2)" || exit 1
HUGE_URI=https://example.com/?$(head -c 5000 /dev/zero | tr '\0' a)
do_test "$(link_sequence "$HUGE_URI" test3)" 'test3\n' || exit 1

$TMUX has 2>/dev/null && exit 1

exit 0
