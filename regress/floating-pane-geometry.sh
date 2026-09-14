#!/bin/sh

# Test new-pane, resize-pane, and move-pane -x/-y/-X/-Y with floating panes.
# Verifies that size and position are correctly applied with the default border,
# with -B none, and with the pane-border-lines window option set to none. Also
# verifies that zero sizes are rejected.
#
# With a border, -x/-y/-X/-Y specify the onscreen footprint including the
# border: new-pane subtracts 2 from width/height and adds 1 to x/y-position
# so that the border lands on the specified screen coordinates. resize-pane
# -x/-y similarly subtracts 2 from the requested size. move-pane -X/-Y adds 1
# to the position. Without a border all values are used directly.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

fail()
{
	echo "$*" >&2
	$TMUX kill-server 2>/dev/null
	exit 1
}

must_fail()
{
	"$@" >/dev/null 2>&1 && fail "unexpected success: $*"
	return 0
}

must_equal()
{
	got=$1
	want=$2
	[ "$got" = "$want" ] || fail "got '$got', expected '$want'"
}

$TMUX new-session -d -x 80 -y 24 || exit 1

# --- Default border (single-line) ---

# Explicit values: border subtracts 2 from size and adds 1 to position.
# -x 20 -> pane_width 18; -y 6 -> pane_height 4;
# -X 8  -> pane_left  9;  -Y 3 -> pane_top    4.
id=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 8 -Y 3 'sleep 100') \
	|| fail "new-pane with explicit geometry failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')"   9
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"    4

# resize-pane with border: requested size decremented by 2.
$TMUX resize-pane -t "$id" -x 30 || fail "resize-pane -x 30 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')" 28
$TMUX resize-pane -t "$id" -y 10 || fail "resize-pane -y 10 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 8

# resize-pane with percentage and border.
# 75% of 80 = 60, 60 - 2 = 58; 75% of 24 = 18, 18 - 2 = 16.
$TMUX resize-pane -t "$id" -x 75% || fail "resize-pane -x 75% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')" 58
$TMUX resize-pane -t "$id" -y 75% || fail "resize-pane -y 75% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 16
$TMUX kill-pane -t "$id" || exit 1

# Percentage width, height and position with border.
# 50% of 80 = 40, 40-2 = 38; 50% of 24 = 12, 12-2 = 10.
# 10% of 80 =  8,  8+1 =  9; 10% of 24 =  2,  2+1 =  3.
id=$($TMUX new-pane -dPF '#{pane_id}' -x 50% -y 50% -X 10% -Y 10% 'sleep 100') \
	|| fail "new-pane with percentage geometry failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')"  38
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 10
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')"   9
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"    3

# move-pane -X/-Y with border: border lands at the given column/row, content
# is one cell inside. -X 5 -> pane_left 6; -Y 1 -> pane_top 2.
$TMUX move-pane -t "$id" -X 5 || fail "move-pane -X 5 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')" 6
$TMUX move-pane -t "$id" -Y 1 || fail "move-pane -Y 1 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"  2
# 25% of 80 = 20, 20+1 = 21; 25% of 24 = 6, 6+1 = 7.
$TMUX move-pane -t "$id" -X 25% || fail "move-pane -X 25% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')" 21
$TMUX move-pane -t "$id" -Y 25% || fail "move-pane -Y 25% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"  7
$TMUX kill-pane -t "$id" || exit 1

# --- -B none ---

# Without a border all values are used directly.
id=$($TMUX new-pane -dPF '#{pane_id}' -B none -x 20 -y 6 -X 8 -Y 3 'sleep 100') \
	|| fail "new-pane -B none with explicit geometry failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')"  20
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 6
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')"   8
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"    3

# resize-pane without border: size used directly.
$TMUX resize-pane -t "$id" -x 30 || fail "resize-pane -x 30 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')" 30
$TMUX resize-pane -t "$id" -y 10 || fail "resize-pane -y 10 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 10

# resize-pane with percentage, no border: 75% of 80 = 60; 75% of 24 = 18.
$TMUX resize-pane -t "$id" -x 75% || fail "resize-pane -x 75% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')" 60
$TMUX resize-pane -t "$id" -y 75% || fail "resize-pane -y 75% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 18
$TMUX kill-pane -t "$id" || exit 1

# Percentage geometry without border: 50% of 80 = 40; 10% of 80 = 8 etc.
id=$($TMUX new-pane -dPF '#{pane_id}' -B none -x 50% -y 50% -X 10% -Y 10% 'sleep 100') \
	|| fail "new-pane -B none with percentage geometry failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')"  40
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 12
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')"   8
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"    2

# move-pane without border: -X/-Y set the content position directly.
# -X 5 -> pane_left 5; -Y 1 -> pane_top 1.
$TMUX move-pane -t "$id" -X 5 || fail "move-pane -X 5 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')" 5
$TMUX move-pane -t "$id" -Y 1 || fail "move-pane -Y 1 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"  1
# 25% of 80 = 20; 25% of 24 = 6.
$TMUX move-pane -t "$id" -X 25% || fail "move-pane -X 25% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')" 20
$TMUX move-pane -t "$id" -Y 25% || fail "move-pane -Y 25% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"  6
$TMUX kill-pane -t "$id" || exit 1

# --- Window option pane-border-lines none ---

$TMUX set-option -w pane-border-lines none || exit 1

# Inherits none from window option: same behaviour as -B none.
id=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 8 -Y 3 'sleep 100') \
	|| fail "new-pane with window pane-border-lines none failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')"  20
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 6
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')"   8
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"    3

$TMUX resize-pane -t "$id" -x 30 || fail "resize-pane -x 30 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')" 30
$TMUX resize-pane -t "$id" -y 10 || fail "resize-pane -y 10 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 10

$TMUX resize-pane -t "$id" -x 75% || fail "resize-pane -x 75% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')" 60
$TMUX resize-pane -t "$id" -y 75% || fail "resize-pane -y 75% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 18
$TMUX kill-pane -t "$id" || exit 1

id=$($TMUX new-pane -dPF '#{pane_id}' -x 50% -y 50% -X 10% -Y 10% 'sleep 100') \
	|| fail "new-pane with window pane-border-lines none and percentages failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_width}')"  40
must_equal "$($TMUX display-message -p -t "$id" '#{pane_height}')" 12
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')"   8
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"    2

# move-pane with window pane-border-lines none: same behaviour as -B none.
$TMUX move-pane -t "$id" -X 5 || fail "move-pane -X 5 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')" 5
$TMUX move-pane -t "$id" -Y 1 || fail "move-pane -Y 1 failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"  1
$TMUX move-pane -t "$id" -X 25% || fail "move-pane -X 25% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_left}')" 20
$TMUX move-pane -t "$id" -Y 25% || fail "move-pane -Y 25% failed"
must_equal "$($TMUX display-message -p -t "$id" '#{pane_top}')"  6
$TMUX kill-pane -t "$id" || exit 1

# --- Invalid sizes ---

# Zero width and height must be rejected by new-pane with the default border.
$TMUX set-option -w -u pane-border-lines || exit 1
must_fail $TMUX new-pane -d -x 0 -y 6 -X 8 -Y 3 'sleep 100'
must_fail $TMUX new-pane -d -x 20 -y 0 -X 8 -Y 3 'sleep 100'

# Zero width and height must be rejected by resize-pane.
id=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 8 -Y 3 'sleep 100') \
	|| fail "new-pane for resize-pane invalid size tests failed"
must_fail $TMUX resize-pane -t "$id" -x 0
must_fail $TMUX resize-pane -t "$id" -y 0
$TMUX kill-pane -t "$id" || exit 1

# Same rejections apply with no border.
$TMUX set-option -w pane-border-lines none || exit 1
must_fail $TMUX new-pane -d -x 0 -y 6 -X 8 -Y 3 'sleep 100'
must_fail $TMUX new-pane -d -x 20 -y 0 -X 8 -Y 3 'sleep 100'

id=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 8 -Y 3 'sleep 100') \
	|| fail "new-pane for resize-pane invalid size tests (no border) failed"
must_fail $TMUX resize-pane -t "$id" -x 0
must_fail $TMUX resize-pane -t "$id" -y 0
$TMUX kill-pane -t "$id" || exit 1

# --- Tiled pane resize with floating cells in the layout ---

$TMUX set-option -w -u pane-border-lines || exit 1
base=$($TMUX display-message -p '#{pane_id}')

# A floating cell after the last tiled cell must not become the recipient of a
# relative or absolute resize. This is the sequence from GitHub issue 5135.
floating=$($TMUX new-pane -dPF '#{pane_id}' -t "$base" \
	-x 50% -y 50% -X 50% -Y 50% 'sleep 100') ||
	fail "new-pane for tiled resize test failed"
lower=$($TMUX split-window -dPF '#{pane_id}' -t "$base" 'sleep 100') ||
	fail "split-window for tiled resize test failed"

$TMUX resize-pane -t "$lower" -U 5 || fail "relative tiled resize failed"
must_equal "$($TMUX display-message -p -t "$base" '#{pane_height}')" 7
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_top}')" 8
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_height}')" 16
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_top}')" 13
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 10

$TMUX resize-pane -t "$lower" -y 11 || fail "absolute tiled reset failed"
$TMUX resize-pane -t "$lower" -y 16 || fail "absolute tiled resize failed"
must_equal "$($TMUX display-message -p -t "$base" '#{pane_height}')" 7
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_top}')" 8
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_height}')" 16
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 10

$TMUX kill-pane -t "$floating" || exit 1
$TMUX kill-pane -t "$lower" || exit 1

# A floating cell between tiled siblings must be skipped when finding the cell
# which donates space to a resize.
lower=$($TMUX split-window -dPF '#{pane_id}' -t "$base" 'sleep 100') ||
	fail "split-window for middle floating cell test failed"
floating=$($TMUX new-pane -dPF '#{pane_id}' -t "$base" \
	-x 20 -y 8 -X 30 -Y 8 'sleep 100') ||
	fail "new-pane for middle floating cell test failed"

$TMUX resize-pane -t "$base" -D 3 || fail "resize past floating cell failed"
must_equal "$($TMUX display-message -p -t "$base" '#{pane_height}')" 15
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_top}')" 16
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_height}')" 8
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 6

$TMUX kill-pane -t "$floating" || exit 1
$TMUX kill-pane -t "$lower" || exit 1

# A small floating child must not limit the available tiled space in a nested
# layout with a different split direction.
lower=$($TMUX split-window -dPF '#{pane_id}' -t "$base" 'sleep 100') ||
	fail "split-window for nested floating cell test failed"
right=$($TMUX split-window -dhPF '#{pane_id}' -t "$lower" 'sleep 100') ||
	fail "horizontal split for nested floating cell test failed"
floating=$($TMUX new-pane -dPF '#{pane_id}' -t "$lower" \
	-x 20 -y 3 -X 30 -Y 10 'sleep 100') ||
	fail "new-pane for nested floating cell test failed"

$TMUX resize-pane -t "$base" -D 3 || fail "nested tiled resize failed"
must_equal "$($TMUX display-message -p -t "$base" '#{pane_height}')" 15
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_top}')" 16
must_equal "$($TMUX display-message -p -t "$lower" '#{pane_height}')" 8
must_equal "$($TMUX display-message -p -t "$right" '#{pane_top}')" 16
must_equal "$($TMUX display-message -p -t "$right" '#{pane_height}')" 8
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 1

$TMUX kill-pane -t "$floating" || exit 1
$TMUX kill-pane -t "$right" || exit 1
$TMUX kill-pane -t "$lower" || exit 1

# --- Floating panes clamped when the window shrinks (issue #5581, PR #5582) ---
#
# layout_resize clamps floating panes back inside the window when it shrinks:
# move them and, only if they cannot fit, shrink them (never below
# PANE_MINIMUM). A floating cell is the pane's content, so with the default
# single-line border the clamp counts 1 cell of border per side, exactly as
# layout_floating_args_parse does on creation; with no border it counts 0.
# Each case below gets its own window, since resize-window fixes a window at
# a manual size for the rest of its life.

# Case 1: a lone floating pane -- break-pane -W on a window's only pane --
# takes the early-return path in layout_resize, added during PR #5582's
# review round, since there is no tiled tree to walk.
win=$($TMUX new-window -dPF '#{window_id}') ||
	fail "new-window for lone float failed"

# -x 20 -y 6 -> pane 18x4; -X 60 -Y 18 -> pane_left 61, pane_top 19: with the
# border, the footprint is columns 60-79, rows 18-23, flush with the 80x24
# window's right and bottom edges, so the float fits before it is shrunk.
$TMUX break-pane -W -s "$win" -x 20 -y 6 -X 60 -Y 18 ||
	fail "break-pane -W for lone float failed"
must_equal "$($TMUX display-message -p -t "$win" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$win" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$win" '#{pane_left}')"   61
must_equal "$($TMUX display-message -p -t "$win" '#{pane_top}')"    19

# Shrink to 40x16. The float still fits at its own size (18 <= 40-2, 4 <=
# 16-2) so only its position moves, flush to the new right/bottom edges:
# xoff = 40 - 18 - 1 = 21; yoff = 16 - 4 - 1 = 11.
$TMUX resize-window -t "$win" -x 40 -y 16 ||
	fail "resize-window (lone float) failed"
must_equal "$($TMUX display-message -p -t "$win" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$win" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$win" '#{pane_left}')"   21
must_equal "$($TMUX display-message -p -t "$win" '#{pane_top}')"    11

# Case 5: grow the window back. The clamp must not chase the window back
# outward -- it only ever pulls a float in, never restores where it was.
$TMUX resize-window -t "$win" -x 80 -y 24 ||
	fail "resize-window grow (lone float) failed"
must_equal "$($TMUX display-message -p -t "$win" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$win" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$win" '#{pane_left}')"   21
must_equal "$($TMUX display-message -p -t "$win" '#{pane_top}')"    11
$TMUX kill-window -t "$win" || exit 1

# Case 2: the same float, but with a tiled sibling surviving alongside it, so
# the window's root cell stays tiled and layout_resize takes the normal path
# (the clamp call after layout_fix_offsets) instead of the early return
# above. Same geometry as case 1, so the same numbers should come out.
win=$($TMUX new-window -dPF '#{window_id}') ||
	fail "new-window for tiled sibling failed"
$TMUX split-window -t "$win" 'sleep 100' ||
	fail "split-window for tiled sibling failed"
$TMUX break-pane -W -s "$win" -x 20 -y 6 -X 60 -Y 18 ||
	fail "break-pane -W for tiled sibling failed"
must_equal "$($TMUX display-message -p -t "$win" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$win" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$win" '#{pane_left}')"   61
must_equal "$($TMUX display-message -p -t "$win" '#{pane_top}')"    19

$TMUX resize-window -t "$win" -x 40 -y 16 ||
	fail "resize-window (tiled sibling) failed"
must_equal "$($TMUX display-message -p -t "$win" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$win" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$win" '#{pane_left}')"   21
must_equal "$($TMUX display-message -p -t "$win" '#{pane_top}')"    11
$TMUX kill-window -t "$win" || exit 1

# Case 3: new-pane float, the original repro from issue #5581 and the PR
# body -- also the normal path, via the tiled base pane new-window creates.
win=$($TMUX new-window -dPF '#{window_id}') ||
	fail "new-window for new-pane repro failed"
$TMUX resize-window -t "$win" -x 120 -y 40 ||
	fail "resize-window to 120x40 failed"

# -x 30 -y 10 -> pane 28x8; -X 85 -Y 25 -> pane_left 86, pane_top 26.
floating=$($TMUX new-pane -t "$win" -dPF '#{pane_id}' \
	-x 30 -y 10 -X 85 -Y 25 'sleep 100') ||
	fail "new-pane for new-pane repro failed"
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_width}')"  28
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 8
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_left}')"   86
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_top}')"    26

# Shrink to 60x20: xoff = 60 - 28 - 1 = 31; yoff = 20 - 8 - 1 = 11.
$TMUX resize-window -t "$win" -x 60 -y 20 ||
	fail "resize-window (new-pane repro) failed"
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_width}')"  28
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 8
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_left}')"   31
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_top}')"    11
$TMUX kill-window -t "$win" || exit 1

# Case 4: a float that already fits inside the shrunk window is left alone --
# assert position and size are both unchanged, not just one of them.
win=$($TMUX new-window -dPF '#{window_id}') ||
	fail "new-window for untouched float failed"

# -x 20 -y 6 -> pane 18x4; -X 8 -Y 3 -> pane_left 9, pane_top 4 (as at the top
# of this file). Footprint columns 8-27, rows 3-8: well inside 60x20 too.
floating=$($TMUX new-pane -t "$win" -dPF '#{pane_id}' \
	-x 20 -y 6 -X 8 -Y 3 'sleep 100') ||
	fail "new-pane for untouched float failed"
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_left}')"   9
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_top}')"    4

$TMUX resize-window -t "$win" -x 60 -y 20 ||
	fail "resize-window (untouched float) failed"
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_width}')"  18
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_height}')" 4
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_left}')"   9
must_equal "$($TMUX display-message -p -t "$floating" '#{pane_top}')"    4
$TMUX kill-window -t "$win" || exit 1

# Case 6: pad = 0, via the pane-border-lines window option set to none. The
# border arithmetic differs here (no -2/+1 adjustment either on creation or
# in the clamp).
win=$($TMUX new-window -dPF '#{window_id}') ||
	fail "new-window for pad=0 float failed"
$TMUX set-option -w -t "$win" pane-border-lines none ||
	fail "set pane-border-lines none failed"

# No border: -x 20 -y 6 -> pane 20x6 directly; -X 60 -Y 18 -> pane_left 60,
# pane_top 18 directly. Footprint (== content, no border) is columns 60-79,
# rows 18-23: flush right/bottom of the 80x24 window, same as case 1.
$TMUX break-pane -W -s "$win" -x 20 -y 6 -X 60 -Y 18 ||
	fail "break-pane -W for pad=0 float failed"
must_equal "$($TMUX display-message -p -t "$win" '#{pane_width}')"  20
must_equal "$($TMUX display-message -p -t "$win" '#{pane_height}')" 6
must_equal "$($TMUX display-message -p -t "$win" '#{pane_left}')"   60
must_equal "$($TMUX display-message -p -t "$win" '#{pane_top}')"    18

# Shrink to 40x16 with pad=0: xoff = 40 - 20 - 0 = 20; yoff = 16 - 6 - 0 = 10.
$TMUX resize-window -t "$win" -x 40 -y 16 ||
	fail "resize-window (pad=0 float) failed"
must_equal "$($TMUX display-message -p -t "$win" '#{pane_width}')"  20
must_equal "$($TMUX display-message -p -t "$win" '#{pane_height}')" 6
must_equal "$($TMUX display-message -p -t "$win" '#{pane_left}')"   20
must_equal "$($TMUX display-message -p -t "$win" '#{pane_top}')"    10
$TMUX kill-window -t "$win" || exit 1

$TMUX kill-server 2>/dev/null
exit 0
