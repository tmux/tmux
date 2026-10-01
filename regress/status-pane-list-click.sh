#!/bin/sh

# Clicking a pane in the pane list on the second status line selects it, hides
# it if it is already the active pane and shows it again if it is hidden. The
# button at the right hides every floating and zoomed pane and, clicked again,
# shows just those.

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

# click COL ROW
#
# Write an SGR mouse press then release at a 1-based position to the outer pane
# holding the inner client.
click()
{
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' "$1" "$2" "$1" "$2")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" || fail "send-keys failed."
	sleep 1
}

# check PANE FORMAT EXPECTED
check()
{
	got=$($TMUX display-message -p -t "$1" "$2")
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

cleanup

$TMUX new-session -d -s inner -x 60 -y 12 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status 2 || exit 1
a=$($TMUX display-message -p '#{pane_id}') || exit 1
b=$($TMUX split-window -dPF '#{pane_id}' 'sleep 100') || exit 1
f=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 4 -X 20 -Y 2 'sleep 100') ||
	exit 1

$TMUX2 new-session -d -x 60 -y 12 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

# The second status line is the last row: "  P: " then one entry per pane in
# pane order (a, b, f), each about twelve columns wide, and the button at the
# far right.
row=12

entry_b=24
button=59

# Clicking another pane selects it.
check "$a" '#{pane_active}' 1
click $entry_b $row
check "$b" '#{pane_active}:#{pane_hidden_flag}' '1:0'

# Clicking the active pane hides it and the focus moves to a visible pane.
click $entry_b $row
check "$b" '#{pane_hidden_flag}' 1
check "$b" '#{pane_active}' 0

# Clicking a hidden pane shows it and selects it.
click $entry_b $row
check "$b" '#{pane_hidden_flag}:#{pane_active}' '0:1'

# The button hides the floating pane but not the tiled ones, then shows it.
check "$f" '#{pane_hidden_flag}' 0
click $button $row
check "$f" '#{pane_hidden_flag}' 1
check "$a" '#{pane_hidden_flag}' 0
check "$b" '#{pane_hidden_flag}' 0
click $button $row
check "$f" '#{pane_hidden_flag}' 0

cleanup
exit 0
