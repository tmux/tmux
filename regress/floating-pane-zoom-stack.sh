#!/bin/sh

# Zoom is a state of a pane inside a single stacking order rather than a
# window-wide layout swap. Any pane, tiled or floating, may be zoomed, several
# panes may be zoomed at once, and floats may sit above a zoomed pane.
#
# This exercises:
# - creating a float while a pane is zoomed leaving that zoom alone;
# - resize-pane -Z toggling only the target pane and resize-pane -a -Z clearing
#   every zoom;
# - activating a covered float raising it, and activating a tiled pane that is
#   not zoomed unzooming everything but leaving the floats floating;
# - pane-raise-on-focus deciding whether activating a visible zoomed pane
#   raises it;
# - new-pane -A staying above everything raised later;
# - commands that only change the tiled layout leaving zooms alone, and
#   select-layout/next-layout still unzooming;
# - break-pane -W, move-pane and kill-pane working while panes are zoomed.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	exit 1
}

run()
{
	$TMUX "$@" || fail "failed: $*"
}

# check $target $format $expected
check()
{
	got=$(run display-message -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# zoomed $pane $expected: is the pane itself zoomed?
zoomed()
{
	check "$1" '#{pane_zoomed_flag}' "$2"
}

# window_zoomed $pane $expected: is any pane in the window zoomed?
window_zoomed()
{
	check "$1" '#{window_zoomed_flag}' "$2"
}

# front $pane: is the pane at the front of the stacking order?
front()
{
	check "$1" '#{pane_z}' 0
}

# ahead $pane1 $pane2: is the first pane above the second in the stack?
ahead()
{
	z1=$(run display-message -p -t "$1" '#{pane_z}') || exit 1
	z2=$(run display-message -p -t "$2" '#{pane_z}') || exit 1
	[ "$z1" -lt "$z2" ] || fail "$1 (z=$z1) not above $2 (z=$z2)"
}

reset()
{
	$TMUX kill-server 2>/dev/null
	run new-session -d -x 80 -y 24
	A=$(run display-message -p '#{pane_id}') || exit 1
	B=$(run split-window -dPF '#{pane_id}') || exit 1
	layout=$(run display-message -p '#{window_layout}') || exit 1
}

# Creating a float while a pane is zoomed must not unzoom it.
reset
run resize-pane -Z -t "$A"
window_zoomed "$A" 1
X=$(run new-pane -PF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
zoomed "$A" 1
zoomed "$X" 0
check "$X" '#{pane_floating_flag}:#{pane_active}' '1:1'
ahead "$X" "$A"
window_zoomed "$A" 1

# Zooming a second pane stacks it above the first; toggling it leaves the
# first zoomed (this is the failing case from GitHub issue 5135).
Y=$(run new-pane -PF '#{pane_id}' -t "$A" -x 20 -y 8 -X 40 -Y 6 '') || exit 1
run resize-pane -Z -t "$Y"
zoomed "$Y" 1
zoomed "$A" 1
window_zoomed "$A" 1
check "$Y" '#{pane_width}:#{pane_height}' '80:24'
run resize-pane -Z -t "$Y"
zoomed "$Y" 0
zoomed "$A" 1
window_zoomed "$A" 1
check "$Y" '#{pane_floating_flag}' 1

# A zoomed float is still a float.
run resize-pane -Z -t "$Y"
check "$Y" '#{pane_floating_flag}:#{pane_zoomed_flag}' '1:1'
run resize-pane -Z -t "$Y"

# Unzooming the bottom zoom leaves the floats where they are.
run resize-pane -Z -t "$A"
zoomed "$A" 0
window_zoomed "$A" 0
check "$X" '#{pane_floating_flag}' 1
check "$Y" '#{pane_floating_flag}' 1
check "$A" '#{pane_width}:#{pane_height}' '80:12'

# resize-pane -a -Z clears every zoom.
run resize-pane -Z -t "$A"
run resize-pane -Z -t "$B"
run resize-pane -Z -t "$X"
zoomed "$A" 1
zoomed "$B" 1
zoomed "$X" 1
run resize-pane -a -Z -t "$A"
for p in "$A" "$B" "$X" "$Y"; do
	zoomed "$p" 0
done
window_zoomed "$A" 0

# Activating a float that is covered by another zoomed pane raises it and
# leaves every zoom alone.
reset
run resize-pane -Z -t "$A"
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
Y=$(run new-pane -PF '#{pane_id}' -t "$A" -x 20 -y 8 -X 40 -Y 6 '') || exit 1
run resize-pane -Z -t "$Y"
run select-pane -t "$X"
check "$X" '#{pane_active}' 1
front "$X"
zoomed "$Y" 1
zoomed "$A" 1
window_zoomed "$X" 1

# Activating a tiled pane that is not zoomed unzooms every pane but the
# floats stay floats.
run select-pane -t "$B"
check "$B" '#{pane_active}' 1
for p in "$A" "$B" "$X" "$Y"; do
	zoomed "$p" 0
done
window_zoomed "$B" 0
check "$X" '#{pane_floating_flag}' 1
check "$Y" '#{pane_floating_flag}' 1

# Activating a tiled pane that is itself zoomed does not unzoom it.
run resize-pane -Z -t "$A"
run select-pane -t "$A"
zoomed "$A" 1
window_zoomed "$A" 1
run resize-pane -a -Z -t "$A"

# pane-raise-on-focus: by default only floats are raised.
reset
run resize-pane -Z -t "$A"
X=$(run new-pane -PF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
Y=$(run new-pane -PF '#{pane_id}' -t "$A" -x 20 -y 8 -X 40 -Y 6 '') || exit 1
front "$Y"
run select-pane -t "$A"
front "$Y"
ahead "$X" "$A"
run select-pane -t "$X"
front "$X"
run set -w -t "$A" pane-raise-on-focus off
run select-pane -t "$Y"
ahead "$X" "$Y"
run set -w -t "$A" pane-raise-on-focus all
run select-pane -t "$A"
front "$A"
run select-pane -t "$Y"
front "$Y"
run set -wu -t "$A" pane-raise-on-focus

# new-pane -A stays above everything raised later.
reset
run resize-pane -Z -t "$A"
X=$(run new-pane -APF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
Y=$(run new-pane -PF '#{pane_id}' -t "$A" -x 20 -y 8 -X 40 -Y 6 '') || exit 1
run resize-pane -Z -t "$Y"
ahead "$X" "$Y"
run select-pane -t "$Y"
ahead "$X" "$Y"
zoomed "$A" 1

# Losing the active pane goes to the last-used pane when it is visible, and
# does not unzoom.
reset
run resize-pane -Z -t "$A"
X=$(run new-pane -PF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
run kill-pane -t "$X"
check "$A" '#{pane_active}' 1
window_zoomed "$A" 1

# Commands that only change the tiled layout leave the zoom alone.
run split-window -d -t "$B"
C=$(run display-message -p -t "$B" '#{pane_id}') || exit 1
zoomed "$A" 1
window_zoomed "$A" 1
run kill-pane -t "$B"
zoomed "$A" 1
window_zoomed "$A" 1

# ... but select-layout and next-layout still unzoom.
run next-layout -t "$A"
window_zoomed "$A" 0
run resize-pane -Z -t "$A"
run select-layout -t "$A" even-horizontal
window_zoomed "$A" 0

# A zoomed pane can be floated and a float can be zoomed and tiled again.
reset
run resize-pane -Z -t "$B"
run break-pane -W -s "$B"
check "$B" '#{pane_floating_flag}:#{pane_zoomed_flag}' '1:1'
run join-pane -s "$B" -t "$B"
check "$B" '#{pane_floating_flag}:#{pane_zoomed_flag}' '0:1'
run resize-pane -Z -t "$B"

# A float covered by a zoomed pane can still be moved and resized without
# unzooming anything.
reset
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
run resize-pane -Z -t "$A"
run move-pane -t "$X" -X 20 -Y 5
check "$X" '#{pane_left}:#{pane_top}' '21:6'
run resize-pane -t "$X" -x 30
check "$X" '#{pane_width}' 28
zoomed "$A" 1
window_zoomed "$A" 1

# The window can be resized with panes zoomed and the zooms follow.
run resize-window -t "$A" -x 100 -y 30
check "$A" '#{pane_width}:#{pane_height}' '100:30'
zoomed "$A" 1

exit 0
