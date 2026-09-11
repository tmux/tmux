#!/bin/sh

# Regression test for a floating-pane drag bug: cmd_resize_pane_redraw_floating()
# (cmd-resize-pane.c) reported damage for just a dragged floating pane's
# content rectangle, not the one-cell border frame drawn around it (see the
# "floating" case in screen-redraw.c, which draws that frame at
# xoff-1/yoff-1 through xoff+sx/yoff+sy - one cell outside the pane's own
# content area). Damage scoped to only the content area left the frame's
# previous position undrawn as the pane moved, so dragging it left a trail
# of un-erased border frames behind - visible as several "corners" stacked
# up rather than just the pane's current one.
#
# This bug has nothing to do with images - it reproduces with a plain
# floating pane and no image support required.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

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

# drag STARTCOL STARTROW ENDCOL ENDROW
#
# Write a plain (unmodified) SGR button-1 press, drag update and release at
# 1-based positions to the outer pane holding the inner client - this
# matches the default MouseDown1Border/MouseDrag1Border bindings used to
# move or resize a floating pane by its border.
drag()
{
	scol="$1"
	srow="$2"
	ecol="$3"
	erow="$4"

	seq=$(printf '\033[<0;%s;%sM' "$scol" "$srow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<32;%s;%sM' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<0;%s;%sm' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

cleanup

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

$TMUX new-session -d -s inner -x 60 -y 20 'sh -c "sleep 100"' || exit 1
$TMUX set -g mouse on
$TMUX set -g default-command 'sh -c "sleep 100"'

FLOAT=$($TMUX new-pane -d -PF '#{pane_id}' -x 16 -y 5 -X 5 -Y 5) ||
	fail "new-pane -X -Y failed"
FTOP=$($TMUX display-message -p -t "$FLOAT" '#{pane_top}')
FLEFT=$($TMUX display-message -p -t "$FLOAT" '#{pane_left}')
FWIDTH=$($TMUX display-message -p -t "$FLOAT" '#{pane_width}')

$TMUX2 new-session -d -x 60 -y 20 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

# Sanity check: exactly one floating pane, so exactly one top-left corner,
# before dragging anything.
$TMUX2 capturep -p -t "$OUTER" >$TMP || fail "capture failed"
n=$(grep -o '┌' $TMP | wc -l)
[ "$n" -eq 1 ] || fail "sanity: expected 1 corner before drag, found $n"

# Drag the floating pane by its top border (row FTOP-1, some column within
# its width) down several rows in a few separate steps, then release. A
# single drag() call already does press/motion/release, so call it several
# times in a row to simulate a multi-step real drag.
GRABCOL=$((FLEFT + FWIDTH / 2))
STARTROW=$FTOP
i=0
while [ $i -lt 6 ]; do
	newrow=$((STARTROW + i + 1))
	drag $((GRABCOL + 1)) $((STARTROW + i)) $((GRABCOL + 1)) $newrow
	i=$((i + 1))
done

$TMUX2 capturep -p -t "$OUTER" >$TMP || fail "capture failed"

# Exactly one top-left corner should remain - the pane's current position.
# This is expected to fail before the fix: multiple corners (a trail of
# un-erased frames) would remain from the intermediate drag positions.
n=$(grep -o '┌' $TMP | wc -l)
[ "$n" -eq 1 ] || fail "expected exactly 1 corner after drag, found $n (ghost frames left behind)"

exit 0
