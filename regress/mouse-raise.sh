#!/bin/sh

# Moving the mouse onto a pane with focus-follows-mouse makes it active without
# raising it.

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

# check PANE FORMAT EXPECTED
check()
{
	got=$($TMUX display-message -p -t "$1" "$2")
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# mouse SEQUENCE: send the escape sequence to the inner client.
mouse()
{
	$TMUX2 send-keys -t "$OUTER" -l "$(printf "$1")" || fail "send-keys failed"
	sleep 1
}

cleanup

$TMUX new-session -d -s inner -x 80 -y 24 'sleep 100' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g focus-follows-mouse on || exit 1
$TMUX set -g pane-border-status top || exit 1
A=$($TMUX display-message -p '#{pane_id}') || exit 1
X=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 5 -Y 8 'sleep 100') || exit 1
Y=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 45 -Y 8 'sleep 100') || exit 1

$TMUX2 new-session -d -x 80 -y 24 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

# Moving onto a floating pane that is behind another one focuses it but does not
# raise it, whatever pane-raise-on-focus says.
check "$A" '#{pane_active}' 1
check "$Y" '#{pane_z}' 0
mouse '\033[<35;12;12M\033[<35;14;13M'
check "$X" '#{pane_active}' 1
check "$Y" '#{pane_z}' 0

# Clicking inside a pane still raises a floating pane, as before.
mouse '\033[<0;54;12M\033[<0;54;12m'
check "$Y" '#{pane_active}:#{pane_z}' '1:0'
mouse '\033[<0;14;12M\033[<0;14;12m'
check "$X" '#{pane_active}:#{pane_z}' '1:0'

cleanup
exit 0
