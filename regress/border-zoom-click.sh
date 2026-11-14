#!/bin/sh

# The zoom button on a pane's border must zoom the pane that was clicked, not
# the active pane.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}
fail()
{
	echo "$*" >&2
	cleanup
	exit 1
}

cleanup

$TMUX new-session -d -s inner -x 80 -y 24 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g pane-border-status top || exit 1
$TMUX split-window -d -h 'sleep 100' || exit 1
active=$($TMUX display-message -p '#{pane_id}') || exit 1
other=$($TMUX list-panes -F '#{pane_id}' | grep -v "^$active\$") || exit 1

$TMUX2 new-session -d -x 80 -y 24 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

# The right hand pane's border ends with the buttons [f][z][x]; the zoom one is
# the middle button, at column 76 (1-based) on the first row.
seq=$(printf '\033[<0;76;1M\033[<0;76;1m')
$TMUX2 send-keys -t "$OUTER" -l "$seq" || fail "send-keys failed."
sleep 1

got=$($TMUX display-message -p -t "$other" '#{pane_zoomed_flag}')
[ "$got" = "1" ] || fail "clicked pane zoomed flag '$got', expected '1'"
got=$($TMUX display-message -p -t "$active" '#{pane_zoomed_flag}')
[ "$got" = "0" ] || fail "active pane zoomed flag '$got', expected '0'"

# Only the zoomed pane shows the unzoom button. A floating pane above it is not
# zoomed so it shows the zoom button, whatever else is zoomed.
$TMUX new-pane -d -x 24 -y 5 -X 20 -Y 3 'sleep 100' || fail "new-pane failed."
sleep 1
screen=$($TMUX2 capture-pane -p -t "$OUTER")
u=$(echo "$screen" | grep -o '\[u\]' | wc -l)
z=$(echo "$screen" | grep -o '\[z\]' | wc -l)
[ "$u" -eq 1 ] && [ "$z" -eq 1 ] ||
	fail "found $u unzoom and $z zoom buttons, expected one of each"

cleanup
exit 0
