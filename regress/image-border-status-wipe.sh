#!/bin/sh

# Regression test for a border-status caching bug: window_make_pane_status()
# (window-border.c) gates the physical redraw of a pane's border-status
# title on a logical content diff (grid_compare against a cached copy), not
# on whether the physical screen cells were disturbed by something else in
# the meantime - such as a floating pane's own border, drawn on top of a
# tiled pane's border-status row, sliding across it and then away again.
# See tmux-image-redraw-known-bugs.md ("border-status text cache ignores
# physical damage") for the full write-up.
#
# Reproduction: a tiled pane with pane-border-status on has a floating pane
# dragged, by mouse, from directly over its border-status row to somewhere
# else. The tiled pane's title should reappear once the floating pane has
# moved off it; without the fix it stays blank.
#
# The drag starts and ends away from row 0 rather than grabbing the
# floating pane's border while it is already sitting on row 0: when a
# floating pane's own border-status row exactly coincides with the tiled
# pane's row 0, mouse hit-testing on that row attributes clicks to the
# tiled pane, not the floating one on top of it (an unrelated tmux quirk,
# not what this test is about). That only matters for the initial press,
# though - once a drag is under way, further motion events go straight to
# the already-bound per-pane callback without re-resolving which pane owns
# the coordinates, so starting the grab on an unambiguous row and dragging
# through row 0 works fine.

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

# drag COL ROW [ROW ...]
#
# Write one continuous mouse-1 gesture at column COL: a press at the first
# ROW, a drag update at each subsequent ROW in turn, and a release at the
# last ROW - all 1-based positions sent to the outer pane holding the
# inner client. This matches the default MouseDown1Border/MouseDrag1Border
# bindings used to move or resize a floating pane by its border.
#
# This must be one continuous press/drag/.../release gesture, not several
# separate drag() calls chained together: releasing clears the per-pane
# drag callback binding, so a later press has to be re-hit-tested from
# scratch, which only works reliably away from row 0 (see the header
# comment). A drag already under way is not re-hit-tested per motion
# event, so it can safely pass through row 0 as an intermediate waypoint.
drag()
{
	col="$1"
	shift

	row="$1"
	shift
	seq=$(printf '\033[<0;%s;%sM' "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2

	while [ $# -gt 1 ]; do
		row="$1"
		shift
		seq=$(printf '\033[<32;%s;%sM' "$col" "$row")
		$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
		sleep 0.15
	done

	row="$1"
	seq=$(printf '\033[<0;%s;%sm' "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

cleanup

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

$TMUX new-session -d -s inner -x 60 -y 20 'sh -c "sleep 100"' || exit 1
$TMUX set -g mouse on
$TMUX set -g pane-border-status top
$TMUX set -g default-command 'sh -c "sleep 100"'

BASE=$($TMUX list-panes -F '#{pane_id}' | head -1)
[ -n "$BASE" ] || fail "No base pane."
$TMUX select-pane -t "$BASE" -T TILEDTITLE || fail "set base title failed"

# Float a pane well clear of row 0 to start - its own top border must not
# coincide with the base pane's border-status row for the initial mouse
# press to unambiguously hit it (see the header comment). -x/-y are the
# pane's outer size including its 2-cell border frame, so request 2 more
# than the desired 14x3 content area (see layout_floating_args_parse()).
FLOAT=$($TMUX new-pane -d -PF '#{pane_id}' -x 16 -y 5 -X 5 -Y 5 -T FLOATTITLE) ||
	fail "new-pane -X -Y failed"
FTOP=$($TMUX display-message -p -t "$FLOAT" '#{pane_top}')
FLEFT=$($TMUX display-message -p -t "$FLOAT" '#{pane_left}')
FWIDTH=$($TMUX display-message -p -t "$FLOAT" '#{pane_width}')

$TMUX2 new-session -d -x 60 -y 20 "$TMUX attach -t inner" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

# Sanity check: the base pane's title is visible now (the float starts
# well clear of row 0).
$TMUX2 capturep -p -t "$OUTER" >$TMP || fail "capture failed"
head -1 $TMP | grep -q TILEDTITLE ||
	fail "base title not visible before the drag - test setup is wrong"

# Drag the float up by its top border (an unambiguous row - not row 0) so
# it ends up covering row 0, hiding the base pane's title behind its own
# border, then continue the same drag back down again so it ends clear of
# row 0 once more - one continuous gesture the whole way (see drag()'s
# comment for why).
GRABCOL=$((FLEFT + FWIDTH / 2))
waypoints="$FTOP"
row=$FTOP
while [ "$row" != 1 ]; do
	row=$((row - 1))
	waypoints="$waypoints $row"
done
while [ "$row" != "$FTOP" ]; do
	row=$((row + 1))
	waypoints="$waypoints $row"
done
drag $((GRABCOL + 1)) $waypoints
drag $((GRABCOL + 1)) 1 $((GRABCOL + 1)) "$FTOP"

$TMUX2 capturep -p -t "$OUTER" >$TMP || fail "capture failed"

# The base pane's title should be visible again now the floating pane has
# moved off its border-status row. This is expected to fail before the fix
# - see the header comment.
head -1 $TMP | grep -q TILEDTITLE ||
	fail "base pane title still blank after floating pane moved away"

exit 0
