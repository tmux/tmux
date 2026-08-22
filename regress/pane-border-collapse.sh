#!/bin/sh

# pane-border-collapse: a pane is drawn over the separator between it and the
# next pane instead of leaving room for a border, so that a one row pane can
# sit directly on top of another with nothing in between.
#
# Only the pane that claims the separator changes size, by one cell, and only
# when every pane along its edge claims it. The layout cells are not touched,
# so #{window_layout} is the same with the option set and unset. If the panes
# on both sides claim the separator the one before it wins.

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

must_equal()
{
	got=$1
	want=$2
	[ "$got" = "$want" ] || fail "got '$got', expected '$want'"
}

geometry()
{
	$TMUX display-message -p -t "$1" \
	    '#{pane_top} #{pane_height} #{pane_left} #{pane_width}'
}

collapse()
{
	$TMUX set-option -p -t "$1" pane-border-collapse "$2" ||
	    fail "set pane-border-collapse $2 failed"
}

uncollapse()
{
	$TMUX set-option -p -t "$1" -u pane-border-collapse ||
	    fail "unset pane-border-collapse failed"
}

$TMUX new-session -d -x 80 -y 24 || exit 1
upper=$($TMUX display-message -p '#{pane_id}') || exit 1
lower=$($TMUX split-window -v -l 22 -P -F '#{pane_id}') || exit 1

# A one row pane above a 22 row pane: the separator is row 1, so the lower
# pane starts at row 2.
must_equal "$(geometry "$upper")" "0 1 0 80"
must_equal "$(geometry "$lower")" "2 22 0 80"
layout=$($TMUX display-message -p '#{window_layout}')

# --- The pane after the separator claims it ---

# The lower pane grows up over row 1 and the upper pane is left alone, so one
# row of real pane content sits directly on top of the other.
collapse "$lower" top
must_equal "$(geometry "$upper")" "0 1 0 80"
must_equal "$(geometry "$lower")" "1 23 0 80"

# The layout cells are unchanged, so the layout string still describes the
# same tiling and can be parsed by a tmux without this option.
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"

uncollapse "$lower"
must_equal "$(geometry "$upper")" "0 1 0 80"
must_equal "$(geometry "$lower")" "2 22 0 80"

# --- The pane before the separator claims it ---

# The same option on the other side grows the upper pane down instead, which
# is what a status bar below the content needs.
collapse "$upper" bottom
must_equal "$(geometry "$upper")" "0 2 0 80"
must_equal "$(geometry "$lower")" "2 22 0 80"
uncollapse "$upper"

# --- The side facing away from a separator is ignored ---

# The lower pane has no separator below it, so claiming that side does
# nothing.
collapse "$lower" bottom
must_equal "$(geometry "$upper")" "0 1 0 80"
must_equal "$(geometry "$lower")" "2 22 0 80"
uncollapse "$lower"

# --- Both sides claiming the separator ---

# The pane before it wins, so the separator is only ever covered once.
collapse "$upper" bottom
collapse "$lower" top
must_equal "$(geometry "$upper")" "0 2 0 80"
must_equal "$(geometry "$lower")" "2 22 0 80"
uncollapse "$upper"
uncollapse "$lower"

# --- all ---

# Set for the whole window every separator is claimed by the pane before it.
$TMUX set-option -w pane-border-collapse all || exit 1
must_equal "$(geometry "$upper")" "0 2 0 80"
must_equal "$(geometry "$lower")" "2 22 0 80"
$TMUX set-option -w -u pane-border-collapse || exit 1

# --- A pane status line keeps its separator ---

# The status line is drawn in the separator, so a pane with one cannot claim
# it and neither can the pane on the other side of it.
$TMUX set-option -w pane-border-status top || exit 1
collapse "$lower" top
must_equal "$(geometry "$lower")" "2 22 0 80"
$TMUX set-option -w -u pane-border-status || exit 1
uncollapse "$lower"

# --- Every pane along the edge must claim the separator ---

# Split the top pane so the separator below it is shared by two panes. With
# only one of them claiming it the separator stays.
right=$($TMUX split-window -h -t "$upper" -P -F '#{pane_id}') || exit 1
must_equal "$(geometry "$upper")" "0 1 0 40"
must_equal "$(geometry "$right")" "0 1 41 39"
must_equal "$(geometry "$lower")" "2 22 0 80"

collapse "$upper" bottom
must_equal "$(geometry "$upper")" "0 1 0 40"
must_equal "$(geometry "$lower")" "2 22 0 80"

# With both of them claiming it they both grow over it.
collapse "$right" bottom
must_equal "$(geometry "$upper")" "0 2 0 40"
must_equal "$(geometry "$right")" "0 2 41 39"
must_equal "$(geometry "$lower")" "2 22 0 80"

# A single pane on the other side of a shared separator is enough on its own.
uncollapse "$upper"
uncollapse "$right"
collapse "$lower" top
must_equal "$(geometry "$upper")" "0 1 0 40"
must_equal "$(geometry "$right")" "0 1 41 39"
must_equal "$(geometry "$lower")" "1 23 0 80"
uncollapse "$lower"

# --- Left and right ---

# The same applies across a left to right split.
collapse "$right" left
must_equal "$(geometry "$upper")" "0 1 0 40"
must_equal "$(geometry "$right")" "0 1 40 40"
uncollapse "$right"

collapse "$upper" right
must_equal "$(geometry "$upper")" "0 1 0 41"
must_equal "$(geometry "$right")" "0 1 41 39"
uncollapse "$upper"

must_equal "$(geometry "$upper")" "0 1 0 40"
must_equal "$(geometry "$right")" "0 1 41 39"
must_equal "$(geometry "$lower")" "2 22 0 80"

$TMUX kill-server 2>/dev/null
exit 0
