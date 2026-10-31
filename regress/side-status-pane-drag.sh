#!/bin/sh

# Creating, moving and resizing panes must use window coordinates with a
# side status line, including when the horizontal status is at the top.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$side/$status: $*" >&2
	exit 1
}

# Send window coordinates as one-based terminal coordinates.
mouse()
{
	seq=$(printf '\033[<%s;%s;%s%s' "$1" \
	    "$(( $2 + xoff + 1 ))" "$(( $3 + yoff + 1 ))" "$4")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail "send mouse failed"
	sleep 0.2
}

drag()
{
	mouse "$1" "$2" "$3" M
	mouse "$(( $1 + 32 ))" "$4" "$5" M
	mouse "$1" "$4" "$5" m
}

check()
{
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display -p -t "$1" \
		    '#{pane_left},#{pane_top},#{pane_width},#{pane_height}')
		[ "$got" = "$2" ] && return
		sleep 0.1
		i=$((i + 1))
	done
	fail "pane $1: got '$got', expected '$2'"
}

$TMUX new-session -d -s inner -x 80 -y 24 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g default-command 'sleep 100' || exit 1
$TMUX set -g side-status-width 14 || exit 1
BASE=$($TMUX display -p '#{pane_id}') || exit 1
$TMUX2 new-session -d -s outer -x 80 -y 24 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

for side in left right; do
	$TMUX set -g side-status "$side" || exit 1
	xoff=0
	[ "$side" = left ] && xoff=14
	for status in off top bottom; do
		yoff=0
		if [ "$status" = off ]; then
			$TMUX set -g status off || exit 1
		else
			$TMUX set -g status 2 || exit 1
			$TMUX set -g status-position "$status" || exit 1
			[ "$status" = top ] && yoff=2
		fi
		sleep 0.5

		# Ctrl-drag creates a floating pane inside the rectangle.
		drag 16 2 1 10 5
		id=$($TMUX list-panes -F \
		    '#{?pane_floating_flag,#{pane_id},}' | tail -1)
		[ -n "$id" ] || fail "drag did not create a floating pane"
		check "$id" '3,2,7,3'
		$TMUX kill-pane -t "$id" || exit 1

		# Alt-drag moves a floating pane without changing its size.
		id=$($TMUX new-pane -dPF '#{pane_id}' \
		    -x 14 -y 7 -X 5 -Y 3 'sleep 100') || exit 1
		check "$id" '6,4,12,5'
		drag 8 8 5 12 7
		check "$id" '10,6,12,5'

		# Dragging the bottom right border resizes both dimensions.
		drag 0 22 11 25 13
		check "$id" '10,6,15,7'
		$TMUX kill-pane -t "$id" || exit 1

		# The same terminal offsets apply to tiled pane borders.
		id=$($TMUX split-window -dhPF '#{pane_id}' \
		    -t "$BASE" 'sleep 100') || exit 1
		width=$($TMUX display -p -t "$BASE" '#{pane_width}')
		drag 0 "$width" 5 "$((width + 4))" 5
		got=$($TMUX display -p -t "$BASE" '#{pane_width}')
		[ "$got" = "$((width + 4))" ] ||
		    fail "tiled width: got '$got', expected '$((width + 4))'"
		$TMUX kill-pane -t "$id" || exit 1
	done
done

exit 0
