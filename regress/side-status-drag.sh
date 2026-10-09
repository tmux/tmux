#!/bin/sh

# A Ctrl-drag starting on the side status line must not create a floating
# pane, and must still dispatch Status bindings. Ctrl-clicking a window on the
# side status line swaps windows, so panes are compared across the session.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-drag-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-drag-outer-$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

mouse()
{
	seq=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail "send mouse failed"
	sleep 0.2
}

check_drag()
{
	sx=$1
	sy=$2
	ex=$3
	ey=$4

	# First move along the side status line, then into the pane.
	mouse 16 "$sx" "$sy" M
	mouse 48 "$sx" $((sy + 1)) M
	mouse 48 "$ex" "$ey" M
	mouse 48 $((ex + 2)) "$ey" M
	mouse 16 $((ex + 2)) "$ey" m

	panes=$($TMUX list-panes -s -F '#{pane_id}') || fail "server exited"
	panes=$(echo "$panes" | sort)
	[ "$panes" = "$BASE" ] ||
	    fail "side status drag from $sx,$sy created a pane: $panes"
}

$TMUX new-session -d -s inner -x 60 -y 20 'sleep 100' || exit 1
$TMUX new-window -d 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g default-command 'sleep 100' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g side-status left || exit 1
BASE=$($TMUX list-panes -s -F '#{pane_id}') || exit 1
BASE=$(echo "$BASE" | sort)
$TMUX2 new-session -d -s outer -x 60 -y 20 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

# Blank rows and the window list on the left.
check_drag 3 10 30 12
check_drag 2 1 30 12
check_drag 2 2 30 12

$TMUX set -g side-status right || exit 1
sleep 0.5
check_drag 55 10 20 12

# The horizontal status line takes the bottom row below the side status.
$TMUX set -g side-status left || exit 1
$TMUX set -g status on || exit 1
sleep 0.5
check_drag 3 10 30 12

# A drag starting on a window range dispatches the Status binding.
$TMUX bind -n C-MouseDrag1Status set -g @status-drag status || exit 1
$TMUX set -g @status-drag '' || exit 1
check_drag 2 2 30 12
got=$($TMUX show -gv @status-drag)
[ "$got" = status ] || fail "got drag '$got', expected 'status'"

exit 0
