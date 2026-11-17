#!/bin/sh

# Trimming a blank source must leave a visible line for cursor reflow.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

$TMUX new-session -d -x80 -y24 -s test 'sleep 100' || exit 1
$TMUX set-option -g window-size manual || exit 1
$TMUX new-window -t test:1 'sleep 100' || exit 1
$TMUX resize-window -t test:0 -x20 -y24 || exit 1

# Exercise widening, narrowing, and equal widths with no source history.
for pair in '0 1' '1 0' '1 2'; do
	set -- $pair
	if [ "$2" = 2 ]; then
		$TMUX new-window -t test:2 'sleep 100' || exit 1
		$TMUX resize-window -t test:2 -x80 -y24 || exit 1
	fi
	source=test:$1.0
	target=test:$2.0
	[ "$($TMUX display-message -p -t "$source" '#{history_size}')" = 0 ] ||
		fail "source has unexpected history"
	$TMUX copy-mode -s "$source" -t "$target" || exit 1
	$TMUX has-session -t test || fail "server died copying blank source"
	[ "$($TMUX display-message -p -t "$target" \
	    '#{pane_in_mode} #{copy_cursor_x} #{copy_cursor_y}')" = '1 0 0' ] ||
		fail "blank source did not enter copy mode at the first cell"
	$TMUX send-keys -t "$target" -X cancel || exit 1
done

# Real content still trims trailing empty lines, clamping the cursor to beta.
$TMUX respawn-pane -k -t test:0.0 \
	"printf 'alpha\r\nbeta\r\n\r\n\r\n'; exec sleep 100" || exit 1
i=0
while [ "$($TMUX display-message -p -t test:0.0 '#{cursor_y}')" != 4 ]; do
	i=$((i + 1))
	[ "$i" -lt 10 ] || fail "source output did not arrive"
	sleep 1
done
$TMUX copy-mode -s test:0.0 -t test:1.0 || exit 1
[ "$($TMUX display-message -p -t test:1.0 \
    '#{pane_in_mode} #{copy_cursor_y} #{copy_cursor_line}')" = '1 1 beta' ] ||
	fail "nonblank source content or trailing-line trimming changed"

exit 0
