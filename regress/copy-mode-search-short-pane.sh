#!/bin/sh

. ./regress-common.inc

# Search match below the screen in a pane of fewer than four rows.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest$$"
stop_server $TMUX || exit 1

cleanup()
{
	stop_server $TMUX || exit 1
}
trap cleanup 0 1 15

# $1 is the pane height.
check()
{
	stop_server $TMUX || exit 1
	$TMUX new -d -x10 -y$1 "printf 'x\\n%.0s' 1 2 3 4 5 6; printf 'zz\\033]2;ready\\007'; cat" || exit 1
	wait_for_output ready $TMUX display -p '#{pane_title}' || exit 1
	$TMUX copy-mode || exit 1
	$TMUX send -X history-top || exit 1
	$TMUX send -X search-forward zz || exit 1

	out=$($TMUX display -p '#{copy_cursor_y}') || exit 1
	[ "$out" -eq $(($1 - 1)) ] || exit 1
	$TMUX send -X cursor-down || exit 1
	out=$($TMUX display -p '#{copy_cursor_y}') || exit 1
	[ "$out" -eq $(($1 - 1)) ] || exit 1
}

check 1
check 2
check 3

exit 0
