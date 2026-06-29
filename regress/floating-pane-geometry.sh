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
TMUX="$TEST_TMUX -Ltest -f/dev/null"
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

$TMUX kill-server 2>/dev/null
exit 0
