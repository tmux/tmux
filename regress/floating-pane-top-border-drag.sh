#!/bin/sh

# With pane-border-status top, a tiled pane's status line is on the top row of
# the window. A floating pane dragged to the top of the window has its own top
# border on the same row, so it must still be draggable by that border rather
# than the click going to the tiled pane underneath.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMUX="$TEST_TMUX -Ltopborder-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Ltopborder-outer-$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX2 kill-server 2>/dev/null
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0 1 15

# Press, drag and release with SGR mouse sequences, sent as 1-based positions
# to the outer pane holding the inner client. This matches the default
# MouseDown1Border/MouseDrag1Border bindings used to move a floating pane by
# its border.
drag()
{
	seq=$(printf '\033[<0;%s;%sM' "$1" "$2")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<32;%s;%sM' "$3" "$4")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<0;%s;%sm' "$3" "$4")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.5
}

pane_top()
{
	$TMUX display-message -p -t "$FLOAT" '#{pane_top}'
}

$TMUX new-session -d -s inner -x 60 -y 20 'sleep 100' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g pane-border-status top || exit 1

FLOAT=$($TMUX new-pane -d -PF '#{pane_id}' -x 20 -y 8 -X 15 -Y 8 'sleep 100') ||
    fail "new-pane failed"
FLEFT=$($TMUX display-message -p -t "$FLOAT" '#{pane_left}')
COL=$((FLEFT + 6))

$TMUX2 new-session -d -s outer -x 60 -y 20 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "no outer pane"

# Drag the floating pane up by its top border until it reaches the top of the
# window, where the border shares a row with the tiled pane's status line.
i=0
while [ "$(pane_top)" -gt 1 ] && [ "$i" -lt 10 ]; do
	top=$(pane_top)
	# The border is on row top-1 (0-based); positions are 1-based.
	dest=$((top - 3))
	[ "$dest" -lt 1 ] && dest=1
	drag $((COL + 1)) "$top" $((COL + 1)) "$dest"
	i=$((i + 1))
done
[ "$(pane_top)" -eq 1 ] || fail "could not drag the pane to the top ($(pane_top))"

# Now drag it back down by the top border in row 0.
drag $((COL + 1)) 1 $((COL + 1)) 4
[ "$(pane_top)" -gt 1 ] ||
    fail "floating pane at the top could not be dragged by its top border"

exit 0
