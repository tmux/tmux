#!/bin/sh

# Test mouse ranges on the status column: clicking a window row selects
# that window, on the left and the right side, and clicks in the panes
# still resolve with the shifted viewport.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null
TMUX_OUTER="$TEST_TMUX -Ltest2 -f/dev/null"
$TMUX_OUTER kill-server 2>/dev/null

trap "$TMUX kill-server 2>/dev/null; $TMUX_OUTER kill-server 2>/dev/null" 0 1 15

$TMUX_OUTER new -d -x80 -y24 "$TMUX new" || exit 1
sleep 1

$TMUX set -g mouse on || exit 1
$TMUX set -g status-column 14 || exit 1
$TMUX rename-window zero
$TMUX neww -n alpha
$TMUX neww -n beta
sleep 1

# Send an SGR mouse press and release at 1-based x,y through the outer
# client to the inner one.
click() {
	printf '\033[<0;%u;%uM\033[<0;%u;%um' "$1" "$2" "$1" "$2" |
	    od -An -v -tx1 | tr -d '\n'
}
do_click() {
	$TMUX_OUTER send-keys -H $(click "$1" "$2") || exit 1
	sleep 1
}

current() {
	$TMUX display -p '#{window_name}'
}

# Click the second row (1:alpha) of the left column.
[ "$(current)" = "beta" ] || exit 1
do_click 3 2
[ "$(current)" = "alpha" ] || { echo "left column click failed"; exit 1; }

# Click the first row (0:zero).
do_click 3 1
[ "$(current)" = "zero" ] || { echo "left column click failed (row 1)"; exit 1; }

# A click on an empty column row changes nothing.
do_click 3 10
[ "$(current)" = "zero" ] || { echo "empty row click changed window"; exit 1; }

# Right side: rows start at terminal column 67.
$TMUX set -g status-column-position right || exit 1
sleep 1
do_click 68 3
[ "$(current)" = "beta" ] || { echo "right column click failed"; exit 1; }

# Pane clicks still work with the viewport shifted by the left column.
$TMUX set -g status-column-position left || exit 1
$TMUX splitw -h || exit 1
sleep 1
$TMUX selectp -t 0
do_click 60 5
out=$($TMUX display -p '#{pane_index}')
[ "$out" = "1" ] || { echo "pane click failed: pane $out"; exit 1; }

exit 0
