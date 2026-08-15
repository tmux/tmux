#!/bin/sh

# next-word-end and next-space-end with vi keys have to cross the end of a
# line the way vi does. The character under the cursor was stepped over
# without wrapping, so at the last word of a line the same word end was found
# again and the cursor never moved.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp -d) || exit 1
trap "$TMUX kill-server 2>/dev/null; rm -rf $TMP" 0 1 15

$TMUX new-session -d -x40 -y10 cat || exit 1
$TMUX set -g mode-keys vi || exit 1
$TMUX send-keys 'hello world' Enter 'foo bar baz' Enter || exit 1
sleep 1

# Returns the cursor position in copy mode as "y,x".
pos() {
	$TMUX display-message -p '#{copy_cursor_y},#{copy_cursor_x}'
}

check() {
	[ "$(pos)" = "$1" ] || {
		echo "expected $1, got $(pos)" >&2
		exit 1
	}
}

$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -X start-of-line || exit 1

$TMUX send-keys -X next-word || exit 1
check '0,6'
$TMUX send-keys -X next-word-end || exit 1
check '0,10'
# The end of the last word on the line: this has to move on to the next line.
$TMUX send-keys -X next-word-end || exit 1
check '1,2'
$TMUX send-keys -X next-word-end || exit 1
check '1,6'

# The same for next-space-end.
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -X start-of-line || exit 1
$TMUX send-keys -X next-space-end || exit 1
check '0,4'
$TMUX send-keys -X next-space-end || exit 1
check '0,10'
$TMUX send-keys -X next-space-end || exit 1
check '1,2'

exit 0
