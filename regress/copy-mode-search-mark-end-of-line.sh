#!/bin/sh

# Search match with the copy mode cursor after the last column.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest$$"
n=0
$TMUX kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0 1 15

# $1 is the text of each line, $2 the expected match.
check()
{
	$TMUX kill-server 2>/dev/null
	n=$((n + 1))
	TMUX="$TEST_TMUX -f/dev/null -Ltest$$-$n"
	$TMUX new -d -x20 -y5 "printf '$1%.0s' 1 2 3 4 5; cat" || exit 1
	sleep 1
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
