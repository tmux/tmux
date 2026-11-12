#!/bin/sh

# Clicking an inactive pane in the second status line selects and unhides it.
# Clicking the active pane hides it and focuses the exposed top of the stack,
# whether the clicks are quick or separated by the mouse click timeout.
# The button at the right cycles desktop visibility.

. ./regress-common.inc

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

# Clicking another pane selects it without hiding it.
check "$a" '#{pane_active}' 1
check "$b" '#{pane_active}:#{pane_hidden_flag}' '0:0'
click $entry_b $row
check "$b" '#{pane_active}:#{pane_hidden_flag}' '1:0'

# A separate click after the timeout hides the active pane.
click $entry_b $row
check "$b" '#{pane_hidden_flag}:#{pane_active}' '1:0'
check "$f" '#{pane_active}' 1

# A later single click shows the hidden pane and selects it again.
click $entry_b $row
check "$b" '#{pane_hidden_flag}:#{pane_active}' '0:1'

# Two quick clicks on an inactive pane select it, then hide it.
$TMUX select-pane -t "$a" || fail "select failed"
seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' \
    "$entry_b" "$row" "$entry_b" "$row")
$TMUX2 send-keys -t "$OUTER" -l "$seq$seq" || fail "send-keys failed"
wait_for_output 1 $TMUX display-message -p -t "$b" '#{pane_hidden_flag}' ||
    fail 'second quick click did not hide the pane'
check "$f" '#{pane_active}' 1
click $entry_b $row
check "$b" '#{pane_hidden_flag}:#{pane_active}' '0:1'

# Three quick clicks also show the pane again on the third press.
$TMUX select-pane -t "$a" || fail "select failed"
$TMUX2 send-keys -t "$OUTER" -l "$seq$seq$seq" || fail "send-keys failed"
sleep 1
check "$b" '#{pane_hidden_flag}:#{pane_active}' '0:1'

# The button hides the floating pane but not the tiled ones, then shows it.
check "$f" '#{pane_hidden_flag}' 0
click $button $row
check "$f" '#{pane_hidden_flag}' 1
check "$a" '#{pane_hidden_flag}' 0
check "$b" '#{pane_hidden_flag}' 0
click $button $row
check "$f" '#{pane_hidden_flag}' 0

# With a fullscreen pane, the same button restores floats before fullscreen.
$TMUX resize-pane -Z -t "$a" || fail "zoom failed"
$TMUX select-pane -t "$f" || fail "select failed"
click $button $row
check "$a" '#{pane_hidden_flag}:#{pane_zoomed_flag}' 1:1
check "$f" '#{pane_hidden_flag}' 1
check "$b" '#{pane_hidden_flag}' 0
click $button $row
check "$f" '#{pane_hidden_flag}' 0
check "$a" '#{pane_hidden_flag}' 1
click $button $row
check "$a" '#{pane_hidden_flag}:#{pane_zoomed_flag}' 0:1
check "$f" '#{pane_hidden_flag}' 0
$TMUX resize-pane -aZ || fail "unzoom failed"

# With every pane hidden the active pane is a hidden one; clicking its entry
# must still show it.
$TMUX select-pane -t "$b" || fail "select failed"
for p in "$f" "$a" "$b"; do
	$TMUX resize-pane -h -t "$p" || fail "hide failed"
done
check "$b" '#{pane_hidden_flag}:#{pane_active}' '1:1'
click $entry_b $row
check "$b" '#{pane_hidden_flag}:#{pane_active}' '0:1'

# The default desktop control is independent of a custom status-click binding.
$TMUX bind -n MouseDown1Status set -g @generic-click yes || exit 1
$TMUX bind -n SecondClick1Status set -g @generic-click yes || exit 1
$TMUX bind -n TripleClick1Status set -g @generic-click yes || exit 1
for position in bottom top; do
	$TMUX set -g status-position "$position" || exit 1
	row=12
	[ "$position" = top ] && row=2
	wait_for_text '[_]' $TMUX2 capture-pane -p \
	    -S "$((row - 1))" -E "$((row - 1))" || fail 'desktop not drawn'
	$TMUX resize-pane -aZ || exit 1
	$TMUX resize-pane -Z -t "$a" || exit 1
	$TMUX resize-pane -H -t "$f" || exit 1
	$TMUX set -g @generic-click no || exit 1
	$TMUX set -g @desktop-clicks 0 || exit 1
	$TMUX set-hook -g after-resize-pane \
	    'set -gF @desktop-clicks "#{e|+:#{@desktop-clicks},1}"' || exit 1

	# Three quick presses must perform all three desktop stages.
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' \
	    "$button" "$row" "$button" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq$seq$seq" || exit 1
	wait_for_output 3 $TMUX show -gv @desktop-clicks ||
	    fail 'rapid desktop clicks did not run all three stages'
	check "$a" '#{pane_hidden_flag}:#{pane_zoomed_flag}' 0:1
	check "$f" '#{pane_hidden_flag}' 0
	check "$a" '#{@generic-click}' no
	$TMUX set-hook -gu after-resize-pane || exit 1
done

cleanup
exit 0
