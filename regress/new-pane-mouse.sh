#!/bin/sh

# Test new-pane -M creates a floating pane from a mouse drag.

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
must_equal()
{
	got=$1
	want=$2
	[ "$got" = "$want" ] || fail "got '$got', expected '$want'"
}

# drag STARTCOL STARTROW ENDCOL ENDROW
#
# Write an SGR Ctrl-mouse press, drag update and release at 1-based positions
# to the outer pane holding the inner client.
drag()
{
	scol="$1"
	srow="$2"
	ecol="$3"
	erow="$4"

	seq=$(printf '\033[<16;%s;%sM' "$scol" "$srow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<48;%s;%sM' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<16;%s;%sm' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

cleanup

$TMUX new-session -d -s inner -x 80 -y 24 || exit 1
$TMUX set -g mouse on
$TMUX set -g default-command 'sleep 100'

$TMUX2 new-session -d -x 80 -y 24 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
BASE=$($TMUX list-panes -F '#{pane_id}' | head -1)
[ -n "$BASE" ] || fail "No base pane."

# Drag from SGR column 3,row 1 to column 10,row 5. These are window positions
# 2,0 to 9,4. With the default border, the floating pane content is inset by
# one cell and has size 6x3.
drag 3 1 10 5

id=$($TMUX list-panes -F '#{?pane_floating_flag,#{pane_id},}' | tail -n 1)
[ -n "$id" ] || fail "no floating pane created"

must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')" 3
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')" 1
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')" 6
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 3

$TMUX new-pane -d -M -L -t "$BASE" 'sleep 100' || fail "new-pane -M -L failed"

cleanup
exit 0
