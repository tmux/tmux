#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
OUT="$TEST_TMUX -LtestB$$ -f/dev/null"

# Stop the attached client before stopping the server under test.
cleanup()
{
	stop_server $OUT || exit 1
	stop_server $TMUX || exit 1
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

$TMUX new -d -x40 -y10 \
	'seq 500; printf "\033]2;ready\007"; exec cat' || exit 1
wait_for_output ready $TMUX display -p '#{pane_title}' || exit 1
numbers=$($TMUX display -p '#{window_id}') || exit 1
$TMUX set -g mode-keys emacs || exit 1
$TMUX set -g status-keys emacs || exit 1
$TMUX set -g window-size manual || exit 1
$OUT new -d -x40 -y11 "exec $TMUX attach" || exit 1
wait_for_clients 1 $TMUX || exit 1

# Send keys through the attached client's prompt and key tables.
keys()
{
	$TMUX send -K "$@" || exit 1
}

# Check the line and column, allowing queued prompt callbacks to run.
check_cursor()
{
	wait_for_output "$1|$2" $TMUX display -p \
	    '#{copy_cursor_line}|#{copy_cursor_x}' || exit 1
}

# Save the cursor and viewport before a search that should leave them alone.
save_position()
{
	position=$($TMUX display -p \
	    '#{copy_cursor_x}|#{copy_cursor_y}|#{scroll_position}') || exit 1
}

# Check that neither the cursor nor the viewport moved.
check_position()
{
	wait_for_output "$position" $TMUX display -p \
	    '#{copy_cursor_x}|#{copy_cursor_y}|#{scroll_position}' || exit 1
}

for wrap in on off; do
	$TMUX set -g wrap-search "$wrap" || exit 1
	for kind in text regex; do
		if [ "$kind" = text ]; then
			forward=C-s
			backward=C-r
		else
			forward=C-M-s
			backward=C-M-r
		fi

		$TMUX copy-mode || exit 1
		keys 'M-<'
		check_cursor 1 0
		keys "$forward" 1 2 3 Enter
		check_cursor 123 3

		# Opening a second prompt keeps the current cursor position.
		save_position
		keys "$backward"
		check_position
		keys 4 5 Enter
		check_cursor 45 0

		save_position
		keys "$forward"
		check_position
		keys 1 2 3 Enter
		check_cursor 123 3

		# Failed extensions keep the last match, including its viewport.
		keys "$backward" 4 5
		check_cursor 45 0
		save_position
		keys z
		check_position
		keys BSpace Enter
		check_cursor 45 0
		keys "$forward" 1 2 3
		check_cursor 123 3
		save_position
		keys z
		check_position
		keys BSpace Enter
		check_cursor 123 3

		# A wholly unsuccessful search preserves a nonzero column.
		keys Left
		check_cursor 123 2
		for direction in "$forward" "$backward"; do
			save_position
			keys "$direction" z z
			check_position
			keys Enter
			check_position
		done
		if [ "$kind" = regex ]; then
			# Invalid regexes also leave a nonzero cursor column alone.
			save_position
			keys "$forward" '['
			check_position
			keys Enter "$backward" '['
			check_position
			keys Enter
		fi
		$TMUX send -X cancel || exit 1
	done
done

$TMUX new-window \
	'printf "foo1 foo2\nfoo3\n\033]2;ready\007"; exec cat' || exit 1
wait_for_output ready $TMUX display -p '#{pane_title}' || exit 1

for kind in text regex; do
	if [ "$kind" = text ]; then
		forward=C-s
		backward=C-r
	else
		forward=C-M-s
		backward=C-M-r
	fi
	$TMUX copy-mode || exit 1
	keys 'M-<'
	check_cursor 'foo1 foo2' 0
	keys "$forward" f
	check_cursor 'foo1 foo2' 1
	keys o
	check_cursor 'foo1 foo2' 2
	keys o
	check_cursor 'foo1 foo2' 3

	# Repeating and reversing a search still step through the matches.
	keys C-s
	check_cursor 'foo1 foo2' 8
	keys C-r
	check_cursor 'foo1 foo2' 5
	keys C-r
	check_cursor 'foo1 foo2' 0
	keys C-s
	check_cursor 'foo1 foo2' 3
	keys BSpace BSpace BSpace
	check_cursor 'foo1 foo2' 0
	keys Enter

	# Backward incremental edits stay on the nearest match on this line.
	keys C-e "$backward" f
	check_cursor 'foo1 foo2' 5
	keys o o
	check_cursor 'foo1 foo2' 5
	keys BSpace BSpace BSpace
	check_cursor 'foo1 foo2' 9
	keys Enter
	$TMUX send -X cancel || exit 1
done

# Custom incremental bindings need not specify a search history type.
$TMUX select-window -t "$numbers" || exit 1
for host in status pane; do
	if [ "$host" = pane ]; then
		flags=-Pi
	else
		flags=-i
	fi
	$TMUX bind -T copy-mode C-s command-prompt "$flags" -p '(down)' \
	    'send -X search-forward-incremental -- "%%"' || exit 1
	$TMUX bind -T copy-mode C-r command-prompt "$flags" -p '(up)' \
	    'send -X search-backward-incremental -- "%%"' || exit 1
	$TMUX copy-mode || exit 1
	keys 'M-<' C-s 1 2 3 Enter
	check_cursor 123 3
	save_position
	keys C-r
	check_position
	keys 4 5 Enter
	check_cursor 45 0
	keys C-s 1 2 3 Enter
	check_cursor 123 3
	$TMUX send -X cancel || exit 1
done

# Command output uses the same searches in view mode.
$TMUX run-shell -t "$numbers" 'seq 500' || exit 1
wait_for_output view-mode $TMUX display -p '#{pane_mode}' || exit 1
keys 'M-<' C-s 1 2 3 Enter
check_cursor 123 3
save_position
keys C-r
check_position
keys 4 5 Enter
check_cursor 45 0

exit 0
