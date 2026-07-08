#!/bin/sh

# Exercise screen-redraw.c border drawing for non-floating (tiled) panes: the
# border junctions where panes meet, and pane status lines/titles.
#
# Four layouts cover every junction type that tiled panes produce:
#   cross    - a 2x2 grid: a full crossing            (+ / CELL_LRUD)
#   tee-lr   - three columns with the middle split:   left and right tees
#                                                      (|- and -| / URD, ULD)
#   tee-up   - a top/bottom split with the top split:  a bottom tee (_|_ / LRU)
#   tee-down - a top/bottom split with the bottom split: a top tee (T / LRD)
# Each layout is rendered once for every value of pane-border-lines, so every
# junction is checked in every border style. One layout per result file.
#
# Each scene is rendered in an inner tmux attached inside an outer tmux pane.
# The outer pane is captured (the full client scene drawn by screen-redraw.c)
# and compared with a golden file in screen-redraw-results/.
#
# Run with GENERATE=1 to (re)create the golden files.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
RESULTS=screen-redraw-results

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" \
	0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

compare() {
	sleep 1
	$TMUX capturep -p >$TMP || exit 1
	if [ -n "$GENERATE" ]; then
		cp $TMP "$RESULTS/$1.result" || exit 1
		echo "generated $1"
	else
		cmp -s $TMP "$RESULTS/$1.result" || \
			fail "scene $1 differs from $RESULTS/$1.result"
	fi
}

# Fresh inner window of fixed size, with one command running.
new_scene() {
	$TMUX2 selectw -t:0 || exit 1
	$TMUX2 kill-window -a 2>/dev/null
	$TMUX2 neww -d "sh -c 'exec sleep 100'" || exit 1
	$TMUX2 selectw -t:\$ || exit 1
	$TMUX2 resizew -x40 -y14 || exit 1
}

C="sh -c 'exec sleep 100'"

# Layouts. Each produces one kind of junction. Splits at fixed window size are
# deterministic.
layout_cross() {		# 2x2 grid: a full crossing
	new_scene
	$TMUX2 splitw -h "$C" || exit 1
	$TMUX2 splitw -v "$C" || exit 1
	$TMUX2 selectp -t0 || exit 1
	$TMUX2 splitw -v "$C" || exit 1
	$TMUX2 select-layout tiled || exit 1
}
layout_tee_lr() {		# three columns, middle split: left and right tees
	new_scene
	$TMUX2 splitw -h "$C" || exit 1
	$TMUX2 selectp -t0 || exit 1
	$TMUX2 splitw -h "$C" || exit 1
	$TMUX2 selectp -t1 || exit 1
	$TMUX2 splitw -v "$C" || exit 1
}
layout_tee_up() {		# top/bottom, top split: a bottom tee
	new_scene
	$TMUX2 splitw -v "$C" || exit 1
	$TMUX2 selectp -t0 || exit 1
	$TMUX2 splitw -h "$C" || exit 1
}
layout_tee_down() {		# top/bottom, bottom split: a top tee
	new_scene
	$TMUX2 splitw -v "$C" || exit 1
	$TMUX2 selectp -t1 || exit 1
	$TMUX2 splitw -h "$C" || exit 1
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX2 new -d -x40 -y14 "sh -c 'exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1
$TMUX2 set -g pane-border-format " #{pane_index} " || exit 1

$TMUX new -d -x40 -y14 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

# Every junction in every border style. New windows inherit the global option,
# so set it before building each layout.
for style in single double heavy simple number spaces none; do
	$TMUX2 set -g pane-border-lines $style || exit 1
	layout_cross;    compare cross-$style
	layout_tee_lr;   compare tee-lr-$style
	layout_tee_up;   compare tee-up-$style
	layout_tee_down; compare tee-down-$style
done
$TMUX2 set -g pane-border-lines single || exit 1

# Pane status lines and titles (one layout each). Use a format that includes the
# explicitly-set title; the default title is the hostname, which is not stable,
# so every pane's title must be set.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 setw pane-border-format " #{pane_index}:#{pane_title} " || exit 1
$TMUX2 setw pane-border-status top || exit 1
$TMUX2 selectp -t:.0 -T left || exit 1
$TMUX2 selectp -t:.1 -T right || exit 1
$TMUX2 selectp -t:.0 || exit 1
compare pane-status-top

new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 setw pane-border-format " #{pane_index}:#{pane_title} " || exit 1
$TMUX2 setw pane-border-status bottom || exit 1
$TMUX2 selectp -t:.0 -T left || exit 1
$TMUX2 selectp -t:.1 -T right || exit 1
$TMUX2 selectp -t:.0 || exit 1
compare pane-status-bottom

# title_all <title-prefix>: set every pane's title (the default is the hostname).
title_all() {
	for p in $($TMUX2 list-panes -F '#{pane_index}'); do
		$TMUX2 selectp -t:.$p -T $1$p || exit 1
	done
	$TMUX2 selectp -t:.0 || exit 1
}

# Three columns with a status line on top, then on the bottom.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 setw pane-border-format " #{pane_title} " || exit 1
$TMUX2 setw pane-border-status top || exit 1
title_all p
compare pane-status-3-top

new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 setw pane-border-format " #{pane_title} " || exit 1
$TMUX2 setw pane-border-status bottom || exit 1
title_all p
compare pane-status-3-bottom

# A 2x2 grid with a status line on top: status borders meet pane borders at the
# internal junctions.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 select-layout tiled || exit 1
$TMUX2 setw pane-border-format " #{pane_title} " || exit 1
$TMUX2 setw pane-border-status top || exit 1
title_all p
compare pane-status-2x2-top

# A zoomed pane: the active pane fills the whole window and the other panes and
# their borders are not drawn at all (a single full-window pane scene).
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 resize-pane -Z || exit 1
compare zoomed-pane

exit 0
