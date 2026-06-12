#!/bin/sh

# Test status-column geometry: the window viewport shrinks by the column
# width on either side, the column hides when the terminal is too narrow
# and the column is independent of the status line.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null
TMUX_OUTER="$TEST_TMUX -Ltest2 -f/dev/null"
$TMUX_OUTER kill-server 2>/dev/null

trap "$TMUX kill-server 2>/dev/null; $TMUX_OUTER kill-server 2>/dev/null" 0 1 15

# Outer 80x24 terminal hosting the inner tmux client.
$TMUX_OUTER new -d -x80 -y24 "$TMUX new" || exit 1
sleep 1

check_size() {
	out=$($TMUX display -p '#{window_width}x#{window_height}')
	if [ "$out" != "$1" ]; then
		echo "size check failed for '$2': want $1, got $out"
		exit 1
	fi
}

# No column: 80 wide, 23 high (status line).
check_size 80x23 "column off"

# Left column of 12.
$TMUX set -g status-column 12 || exit 1
sleep 1
check_size 68x23 "left column"

# Right column of 12.
$TMUX set -g status-column-position right || exit 1
sleep 1
check_size 68x23 "right column"

# Too-narrow terminal hides the column.
$TMUX set -g status-column 200 || exit 1
sleep 1
check_size 80x23 "too narrow"

# Column on with status line off.
$TMUX set -g status-column 12 || exit 1
$TMUX set -g status off || exit 1
sleep 1
check_size 68x24 "status off"

# Both off restores the full terminal.
$TMUX set -g status-column 0 || exit 1
sleep 1
check_size 80x24 "both off"

exit 0
