#!/bin/sh

# Exercise screen-redraw.c pane-border-indicators: the arrow indicators that
# point at the active pane, and the two-pane border colour split.
#
# Arrows are drawn as glyphs (captured plain). The two-pane colour split is
# drawn with styles, so that scene is captured with escapes (-e) and uses
# distinct active/inactive border styles so the split is visible.
#
# Each scene is rendered in an inner tmux attached inside an outer tmux pane.
# The outer pane is captured and compared with a golden in screen-redraw-results/.
#
# Run with GENERATE=1 to (re)create the golden files.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2 -f/dev/null"
RESULTS=screen-redraw-results

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" \
	0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

# compare <name> [-e]: capture the outer pane and compare (or generate).
compare() {
	sleep 1
	$TMUX capturep -p $2 >$TMP || exit 1
	if [ -n "$GENERATE" ]; then
		cp $TMP "$RESULTS/$1.result" || exit 1
		echo "generated $1"
	else
		cmp -s $TMP "$RESULTS/$1.result" || \
			fail "scene $1 differs from $RESULTS/$1.result"
	fi
}

new_scene() {
	$TMUX2 neww -d "sh -c 'exec sleep 100'" || exit 1
	$TMUX2 selectw -t:\$ || exit 1
	$TMUX2 resizew -x40 -y12 || exit 1
}

C="sh -c 'exec sleep 100'"

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX2 new -d -x40 -y12 "sh -c 'exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1

$TMUX new -d -x40 -y12 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

# --- Arrows: must appear for whichever pane is active (GitHub #4780). ---

$TMUX2 set -g pane-border-indicators arrows || exit 1

# Two panes, left active: arrow points left.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
compare arrows-2pane-left

# Two panes, right active: arrow points right (the case that regressed).
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t1 || exit 1
compare arrows-2pane-right

# Three columns, middle active.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t1 || exit 1
compare arrows-3pane

# Four panes (2x2), one active.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 select-layout tiled || exit 1
$TMUX2 selectp -t0 || exit 1
compare arrows-4pane

# --- Two-pane border colour split. ---

# Distinct active/inactive styles so the coloured halves are visible; captured
# with -e to record the SGR.
$TMUX2 set -g pane-border-indicators colour || exit 1

# Left/right split: the border is vertical and is split into a top half (active
# pane colour) and a bottom half (the LAYOUT_LEFTRIGHT case).
new_scene
$TMUX2 setw pane-active-border-style fg=red || exit 1
$TMUX2 setw pane-border-style fg=green || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
compare two-pane-colour-vertical -e

# Top/bottom split: the border is horizontal and is split into a left half and a
# right half instead (the LAYOUT_TOPBOTTOM case).
new_scene
$TMUX2 setw pane-active-border-style fg=red || exit 1
$TMUX2 setw pane-border-style fg=green || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
compare two-pane-colour-horizontal -e

# The colour split only applies with exactly two tiled panes. With three panes
# the split is suppressed: the whole border uses the active pane's colour and
# there is no coloured half (redraw_check_two_pane_colours returns 0).
new_scene
$TMUX2 setw pane-active-border-style fg=red || exit 1
$TMUX2 setw pane-border-style fg=green || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
compare colour-three-suppressed -e

# A floating pane is ignored by the two-pane count, so two tiled panes plus a
# float still split. The float itself is never coloured by the indicator.
new_scene
$TMUX2 setw pane-active-border-style fg=red || exit 1
$TMUX2 setw pane-border-style fg=green || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 new-pane -x16 -y6 -X10 -Y2 "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
compare colour-two-plus-float -e

# --- Both indicators together. ---

# pane-border-indicators both enables the arrow and the colour split at once
# (it satisfies both the arrow and the colour branches). Two panes so the colour
# split also applies.
$TMUX2 set -g pane-border-indicators both || exit 1
new_scene
$TMUX2 setw pane-active-border-style fg=red || exit 1
$TMUX2 setw pane-border-style fg=green || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
compare indicators-both -e

# --- Marked pane. ---

# A marked pane (select-pane -m) has its border drawn reversed. Captured with -e
# to record the reverse attribute. The marked pane is made non-active so its
# reversed border is distinct from the active pane.
$TMUX2 set -g pane-border-indicators off || exit 1

# Left/right split, right pane marked.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 selectp -t1 -m || exit 1
compare marked-pane-lr -e

# Top/bottom split, bottom pane marked.
new_scene
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 selectp -t1 -m || exit 1
compare marked-pane-tb -e

# Three columns, middle pane marked (reversed border on both sides).
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 selectp -t1 -m || exit 1
compare marked-pane-three -e

# Floating pane marked: the whole floating box border is reversed.
new_scene
$TMUX2 new-pane -x20 -y6 -X8 -Y3 "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 selectp -t1 -m || exit 1
compare marked-pane-float -e

# Marked pane together with a pane status line: the title border is reversed too.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 setw pane-border-format " #{pane_index}:#{pane_title} " || exit 1
$TMUX2 setw pane-border-status top || exit 1
$TMUX2 selectp -t:.0 -T left || exit 1
$TMUX2 selectp -t:.1 -T right || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 selectp -t1 -m || exit 1
compare marked-pane-status -e

exit 0
