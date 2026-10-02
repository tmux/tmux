#!/bin/sh

# The cursor must not be drawn for a hidden active pane, for example in the
# middle of an empty desktop after "show desktop" with every other pane hidden.

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

# cursor EXPECTED
#
# Check whether the outer pane showing the inner client has a visible cursor.
cursor()
{
	i=0
	while [ "$i" -lt 30 ]; do
		got=$($TMUX2 display-message -p -t "$OUTER" '#{cursor_flag}')
		[ "$got" = "$1" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "cursor_flag: got '$got', expected '$1'"
}

cleanup

$TMUX new-session -d -s inner -x 60 -y 12 'cat' || exit 1
a=$($TMUX display-message -p '#{pane_id}') || exit 1
b=$($TMUX split-window -dPF '#{pane_id}' 'cat') || exit 1
f=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 4 -X 20 -Y 2 'cat') || exit 1

$TMUX2 new-session -d -x 60 -y 12 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

# A zoomed pane with a floating pane over it and another tiled pane: hide
# everything above the layout, then the remaining tiled pane.
$TMUX resize-pane -Z -t "$a" || fail "zoom failed"
$TMUX select-pane -t "$f" || fail "select failed"
cursor 1
$TMUX resize-pane -a -H || fail "show desktop failed"
$TMUX resize-pane -H -t "$b" || fail "hide failed"
for p in "$a" "$b" "$f"; do
	[ "$($TMUX display-message -p -t "$p" '#{pane_hidden_flag}')" = 1 ] ||
	    fail "$p is not hidden"
done
cursor 0

# Showing a pane brings the cursor back.
$TMUX select-pane -t "$b" || fail "select failed"
cursor 1

cleanup
exit 0
