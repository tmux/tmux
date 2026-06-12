#!/bin/sh

# Test status-column-format rendering: #[newline] semantics, style
# carry-over, UTF-8, ## escaping, the per-window default format and the
# keep-focus-visible row trimming with top and bottom markers.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null
TMUX_OUTER="$TEST_TMUX -Ltest2 -f/dev/null"
$TMUX_OUTER kill-server 2>/dev/null

trap "$TMUX kill-server 2>/dev/null; $TMUX_OUTER kill-server 2>/dev/null" 0 1 15

$TMUX_OUTER new -d -x80 -y24 "$TMUX new" || exit 1
sleep 1

$TMUX set -g status-column 14 || exit 1
sleep 1

# Column cell range of a row of the outer screen (1-based for awk).
column_row() {
	$TMUX_OUTER capturep -p | awk -v n="$1" 'NR == n { print substr($0, 1, 14) }' | sed 's/ *$//'
}

check_row() {
	out=$(column_row "$1")
	if [ "$out" != "$2" ]; then
		echo "row $1 check failed for '$3': want '$2', got '$out'"
		exit 1
	fi
}

set_format() {
	$TMUX set -g status-column-format "$1" || exit 1
	$TMUX refresh-client
	sleep 1
}

# Simple rows.
set_format 'one#[newline]two#[newline]three'
check_row 1 "one" "simple rows"
check_row 2 "two" "simple rows"
check_row 3 "three" "simple rows"
check_row 4 "" "simple rows"

# Consecutive directives produce an empty row; a trailing directive does
# not add an empty final row.
set_format 'aa#[newline]#[newline]bb#[newline]'
check_row 1 "aa" "empty rows"
check_row 2 "" "empty rows"
check_row 3 "bb" "empty rows"
check_row 4 "" "empty rows"

# UTF-8 content.
set_format 'héllo#[newline]wörld'
check_row 1 "héllo" "utf8"
check_row 2 "wörld" "utf8"

# ## escaping: ## is a literal # and does not break rows.
set_format 'a##b#[newline]c####[d'
check_row 1 "a#b" "escaping"
check_row 2 "c##[d" "escaping"

# newline combined with other terms is not a row break.
set_format 'x#[newline,fg=red]y'
check_row 1 "xy" "newline not alone"
check_row 2 "" "newline not alone"

# Styles carry over a row boundary.
set_format '#[bg=red]r1#[newline]r2'
out=$($TMUX_OUTER capturep -ep | awk 'NR == 2 { print }')
case "$out" in
*41m*) ;;
*) echo "style carry-over failed: no red background in '$out'"; exit 1;;
esac

# Default format: one window per row, current window has the focus.
$TMUX set -gu status-column-format
$TMUX rename-window zero
$TMUX neww -n alpha
$TMUX neww -n beta
sleep 1
check_row 1 "0:zero" "window list"
check_row 2 "1:alpha-" "window list"
check_row 3 "2:beta*" "window list"

# More windows than rows: the focus stays visible and markers are drawn on
# the boundary rows.
i=3
while [ $i -lt 30 ]; do
	$TMUX neww -d -n "w$i"
	i=$((i + 1))
done
$TMUX selectw -t 15
sleep 1
out=$($TMUX_OUTER capturep -p | awk '{ print substr($0, 1, 14) }')
case "$out" in
*15:w15\**) ;;
*) echo "focus row not visible"; exit 1;;
esac
top=$(column_row 1)
case "$top" in
^*) ;;
*) echo "top marker missing: '$top'"; exit 1;;
esac
bottom=$(column_row 23)
case "$bottom" in
v*) ;;
*) echo "bottom marker missing: '$bottom'"; exit 1;;
esac

# First window selected: no top marker, bottom marker present.
$TMUX selectw -t 0
sleep 1
check_row 1 "0:zero*" "first focus"
bottom=$(column_row 23)
case "$bottom" in
v*) ;;
*) echo "bottom marker missing with first focus: '$bottom'"; exit 1;;
esac

# Last window selected: top marker, focus on the last row.
$TMUX selectw -t 29
sleep 1
top=$(column_row 1)
case "$top" in
^*) ;;
*) echo "top marker missing with last focus: '$top'"; exit 1;;
esac
check_row 23 "29:w29*" "last focus"

exit 0
