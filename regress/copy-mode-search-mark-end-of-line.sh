#!/bin/sh

. ./regress-common.inc

# Search match with the copy mode cursor after the last column.

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

# $1 is the text of each line, $2 the expected match.
check()
{
	stop_server $TMUX || exit 1
	$TMUX new -d -x20 -y5 "printf '$1%.0s' 1 2 3 4 5; printf '\\033]2;ready\\007'; cat" || exit 1
	wait_for_output ready $TMUX display -p '#{pane_title}' || exit 1
	$TMUX set -g mode-keys vi || exit 1
	$TMUX copy-mode || exit 1
	$TMUX send -X begin-selection || exit 1
	$TMUX send -X rectangle-on || exit 1
	$TMUX send -X search-backward a || exit 1
	$TMUX send -X bottom-line || exit 1
	$TMUX send -X end-of-line || exit 1

	out=$($TMUX display -p '#{copy_cursor_x} #{search_present}') || exit 1
	[ "$out" = "20 1" ] || exit 1
	out=$($TMUX display -p '#{search_match}') || exit 1
	[ "$out" = "$2" ] || exit 1
}

check aaaaaaaaaaaaaaaaaaaa a
check aaaaaaaaaaaaaaaaaaab ""

exit 0
