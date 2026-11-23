#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

check()
{
	format=$1
	expected=$2
	actual=$($TMUX display-message -p "$format") || exit 1
	if [ "$actual" != "$expected" ]; then
		printf 'format: %s\nexpected: %s\nactual: %s\n' \
		    "$format" "$expected" "$actual"
		exit 1
	fi
}

# Sixty commands, each marked with OSC 133 A and three lines of output.
$TMUX new -d -x80 -y24 "sh -c 'i=1; while [ \$i -le 60 ]; do printf \"\033]133;A\007 cmd%s\nout\nout\nout\n\" \$i; i=\$((i+1)); done; exec sh'" || exit 1
$TMUX set -g window-size manual || exit 1

# Wait for all the output to arrive before entering copy mode.
for i in 1 2 3 4 5 6 7 8 9 10; do
	$TMUX capture-pane -p | grep -qx ' cmd60' && break
	sleep 1
done
$TMUX capture-pane -p | grep -qx ' cmd60' || exit 1

for keys in vi emacs; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode || exit 1
	check '#{scroll_position}:#{copy_cursor_y}' '0:23'
	$TMUX send-keys -X page-up || exit 1
	check '#{scroll_position}:#{copy_cursor_y}' '22:23'
	$TMUX send-keys -X middle-line || exit 1
	check '#{scroll_position}:#{copy_cursor_y}' '22:11'

	# The previous prompt is already visible a few rows above the
	# cursor, so the view must stay where it is instead of scrolling
	# the prompt to the top row.
	$TMUX send-keys -X previous-prompt || exit 1
	check '#{scroll_position}:#{copy_cursor_y}' '22:9'
	$TMUX send-keys -X next-prompt || exit 1
	check '#{scroll_position}:#{copy_cursor_y}' '22:13'

	# A mark below the current view is scrolled into view with a gap,
	# like search does, not put on the top row.
	$TMUX send-keys -X set-mark || exit 1
	$TMUX send-keys -X history-bottom || exit 1
	check '#{scroll_position}:#{copy_cursor_y}' '0:23'
	$TMUX send-keys -X jump-to-mark || exit 1
	check '#{scroll_position}:#{copy_cursor_y}' '27:18'

	$TMUX send-keys -X cancel || exit 1
done

exit 0
