#!/bin/sh

# Tests for copy-mode-keep-column: when moving vertically in copy mode, the
# cursor either snaps to the end of a shorter line (default) or keeps its
# column, matching screen's copy mode (copy-mode-keep-column on).

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -LtestA$$"
$TMUX kill-server 2>/dev/null

# Pane with a short first line and a longer second line in a window 20 wide.
$TMUX new -d -x20 -y5 \
      "printf 'abc\ndefghijklmnop\n'; exec cat" || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set-window-option -g mode-keys vi

# Move to the end of the long line, then up to the short line. With the
# default behaviour the cursor snaps to the end of the short line, with
# copy-mode-keep-column it keeps the column.
move_to_long_line()
{
	$TMUX copy-mode
	$TMUX send-keys -X history-top
	$TMUX send-keys -X start-of-line
	$TMUX send-keys -X cursor-down
	i=0
	while [ $i -lt 12 ]; do
		$TMUX send-keys -X cursor-right
		i=$((i + 1))
	done
	$TMUX send-keys -X cursor-up
}

cursor()
{
	$TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}'
}

# Default: the cursor snaps to the end of the short line.
move_to_long_line
[ "$(cursor)" = "2,0" ] || {
	echo "default behaviour failed."
	echo "Expected: '2,0'"
	echo "But got:  '$(cursor)'"
	exit 1
}
$TMUX send-keys -X cancel

# copy-mode-keep-column on: the cursor keeps column 12.
$TMUX set-window-option -g copy-mode-keep-column on
move_to_long_line
[ "$(cursor)" = "12,0" ] || {
	echo "copy-mode-keep-column failed."
	echo "Expected: '12,0'"
	echo "But got:  '$(cursor)'"
	exit 1
}

# Moving down again keeps the column as well.
$TMUX send-keys -X cursor-down
[ "$(cursor)" = "12,1" ] || {
	echo "copy-mode-keep-column down failed."
	echo "Expected: '12,1'"
	echo "But got:  '$(cursor)'"
	exit 1
}
$TMUX send-keys -X cancel

# copy-mode-keep-column off restores the default behaviour.
$TMUX set-window-option -g copy-mode-keep-column off
move_to_long_line
[ "$(cursor)" = "2,0" ] || {
	echo "copy-mode-keep-column off failed."
	echo "Expected: '2,0'"
	echo "But got:  '$(cursor)'"
	exit 1
}
$TMUX send-keys -X cancel

# Moving up from the column one past the end of a line that fills the window
# (where copy mode shows '$') must keep that column too.
$TMUX set-window-option -g copy-mode-keep-column on
$TMUX set-window-option -g mode-keys emacs
$TMUX new-session -d -x20 -y5 -s right \
      "printf 'abc\nabcdefghijklmnopqrst\n'; exec cat"
$TMUX copy-mode -t right:0.0
$TMUX send-keys -t right:0.0 -X history-top
$TMUX send-keys -t right:0.0 -X start-of-line
$TMUX send-keys -t right:0.0 -X cursor-down
i=0
while [ $i -lt 20 ]; do
	$TMUX send-keys -t right:0.0 -X cursor-right
	i=$((i + 1))
done
[ "$($TMUX display -p -t right:0.0 '#{copy_cursor_x},#{copy_cursor_y}')" = "20,1" ] || {
	echo "right-edge position failed."
	echo "Expected: '20,1'"
	echo "But got:  '$($TMUX display -p -t right:0.0 '#{copy_cursor_x},#{copy_cursor_y}')'"
	exit 1
}
$TMUX send-keys -t right:0.0 -X cursor-up
[ "$($TMUX display -p -t right:0.0 '#{copy_cursor_x},#{copy_cursor_y}')" = "20,0" ] || {
	echo "right-edge keep failed."
	echo "Expected: '20,0'"
	echo "But got:  '$($TMUX display -p -t right:0.0 '#{copy_cursor_x},#{copy_cursor_y}')'"
	exit 1
}
$TMUX send-keys -t right:0.0 -X cancel

$TMUX kill-server 2>/dev/null
exit 0