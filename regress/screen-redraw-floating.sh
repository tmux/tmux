#!/bin/sh

# Exercise screen-redraw.c drawing of floating panes: their borders and titles,
# clipping at the window edge, and the interaction with the area outside the
# window (when the window is smaller than the attached client).
#
# Each scene is rendered in an inner tmux attached inside an outer tmux pane.
# The outer pane is captured (the full client scene drawn by screen-redraw.c)
# and compared with a golden file in screen_redraw_results/.
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
	$TMUX capturep -p $2 >$TMP || exit 1
	if [ -n "$GENERATE" ]; then
		cp $TMP "$RESULTS/$1.result" || exit 1
		echo "generated $1"
	else
		cmp -s $TMP "$RESULTS/$1.result" || \
			fail "scene $1 differs from $RESULTS/$1.result"
	fi
}

# new_scene <width> <height>: fresh inner window of the given window size.
new_scene() {
	$TMUX2 neww -d "sh -c 'printf base; exec sleep 100'" || exit 1
	$TMUX2 selectw -t:\$ || exit 1
	$TMUX2 resizew -x$1 -y$2 || exit 1
}

# tile_2x2: split the current window into a 2x2 grid of tiled panes.
tile_2x2() {
	C="sh -c 'exec sleep 100'"
	$TMUX2 splitw -h "$C" || exit 1
	$TMUX2 splitw -v "$C" || exit 1
	$TMUX2 selectp -t0 || exit 1
	$TMUX2 splitw -v "$C" || exit 1
	$TMUX2 select-layout tiled || exit 1
}

# fill_base_pattern: make clipped floating panes visibly cover every column.
fill_base_pattern() {
	$TMUX2 respawnp -k "sh -c 'i=0; while [ \$i -lt 12 ]; do printf \"%02d:abcdefghijklmnopqrstuvwxyz0123456789\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
}

# fill_all_panes: make each pane visibly nonblank.
fill_all_panes() {
	for pane in $($TMUX2 list-panes -F '#{pane_id}'); do
		$TMUX2 respawnp -k -t "$pane" "sh -c 'i=0; while [ \$i -lt 5 ]; do printf \"TILE%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
	done
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX2 new -d -x40 -y12 "sh -c 'printf base; exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1
$TMUX2 set -g pane-border-format " #{pane_title} " || exit 1

$TMUX new -d -x40 -y12 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

# Basic floating pane, well inside the window.
new_scene 40 12
$TMUX2 new-pane -x20 -y6 -X8 -Y3 "sh -c 'printf FLOAT; exec sleep 100'" || exit 1
compare floating-basic

# Floating pane with a title on its border. pane-border-status also draws the
# base pane's title, so set it explicitly (the default is the hostname, which is
# not stable).
new_scene 40 12
$TMUX2 setw pane-border-status top || exit 1
$TMUX2 selectp -t:.0 -T base || exit 1
$TMUX2 new-pane -x20 -y6 -X8 -Y2 -T title \
	"sh -c 'printf FLOAT; exec sleep 100'" || exit 1
compare floating-title

# Larger floating pane with double border lines.
new_scene 40 12
$TMUX2 new-pane -x28 -y8 -X4 -Y1 -B double \
	"sh -c 'printf FLOAT; exec sleep 100'" || exit 1
compare floating-border-double

# Floating pane with no border lines: redraw_mark_pane_borders returns early so
# the float has no border at all, only its (clipped) content over the base pane.
new_scene 40 12
$TMUX2 new-pane -x20 -y6 -X10 -Y3 -B none \
	"sh -c 'printf NOBORDER; exec sleep 100'" || exit 1
compare floating-noborder

# Floating pane positioned past the right and bottom edges: must clip.
new_scene 40 12
$TMUX2 new-pane -x20 -y6 -X30 -Y8 "sh -c 'printf CLIP; exec sleep 100'" || exit 1
compare floating-clip-edge

# Window smaller than the client (outside area filled), with a floating pane
# that overlaps the boundary into the outside region.
new_scene 28 8
$TMUX2 new-pane -x16 -y5 -X18 -Y4 "sh -c 'printf OUT; exec sleep 100'" || exit 1
compare floating-outside

# Floating pane whose right border is exactly on the window right edge. The
# floating pane is clipped so the tiled pane's right border remains unbroken.
new_scene 28 8
$TMUX2 respawnp -k "sh -c 'i=0; while [ \$i -lt 8 ]; do printf \"%02d:abcdefghijklmnopq\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 new-pane -x10 -y5 -X19 -Y2 "sh -c 'printf EDGE; exec sleep 100'" || exit 1
compare floating-clip-window-edge

# Floating pane clipped at the top-left corner (negative offsets).
new_scene 40 12
$TMUX2 new-pane -x18 -y6 -X-4 -Y-2 "sh -c 'printf TL; exec sleep 100'" || exit 1
compare floating-clip-topleft

# Floating panes clipped at each edge. The patterned base pane makes sure the
# clipped edge still obscures both content and borders underneath.
new_scene 40 12
fill_base_pattern
$TMUX2 new-pane -x20 -y6 -X-10 -Y3 "sh -c 'printf LEFT; exec sleep 100'" || exit 1
compare floating-clip-left

new_scene 40 12
fill_base_pattern
$TMUX2 new-pane -x20 -y6 -X30 -Y3 "sh -c 'printf RIGHT; exec sleep 100'" || exit 1
compare floating-clip-right

new_scene 40 12
fill_base_pattern
$TMUX2 new-pane -x20 -y6 -X10 -Y-3 "sh -c 'printf TOP; exec sleep 100'" || exit 1
compare floating-clip-top

new_scene 40 12
fill_base_pattern
$TMUX2 new-pane -x20 -y6 -X10 -Y9 "sh -c 'printf BOTTOM; exec sleep 100'" || exit 1
compare floating-clip-bottom

# Floating pane over a tiled 2x2 grid: the float draws a complete box and must
# NOT merge its borders with the tiled pane borders underneath.
new_scene 40 12
tile_2x2
fill_all_panes
$TMUX2 new-pane -x16 -y6 -X11 -Y3 "sh -c 'printf FLT; exec sleep 100'" || exit 1
compare floating-over-tiled

# Same, but the float uses double border lines while the tiled panes use single:
# the two border styles must coexist without merging.
new_scene 40 12
tile_2x2
fill_all_panes
$TMUX2 new-pane -x16 -y6 -X11 -Y3 -B double \
	"sh -c 'printf FLT; exec sleep 100'" || exit 1
compare floating-over-tiled-double

# Two overlapping floating panes: the later (top) float draws over the earlier.
new_scene 40 12
$TMUX2 new-pane -x16 -y6 -X4 -Y2 "sh -c 'printf AAA; exec sleep 100'" || exit 1
$TMUX2 new-pane -x16 -y6 -X14 -Y6 "sh -c 'printf BBB; exec sleep 100'" || exit 1
compare floating-overlap

# Two floating panes with different per-pane configuration: one has its status on
# top with single borders, the other has its status on the bottom with heavy
# borders (pane-border-status and pane-border-lines are per-pane options).
new_scene 40 12
$TMUX2 setw pane-border-format " #{pane_title} " || exit 1
$TMUX2 new-pane -x16 -y4 -X3 -Y1 -T one \
	"sh -c 'printf ONE; exec sleep 100'" || exit 1
$TMUX2 set -p pane-border-status top || exit 1
$TMUX2 set -p pane-border-lines single || exit 1
$TMUX2 new-pane -x16 -y4 -X18 -Y6 -T two \
	"sh -c 'printf TWO; exec sleep 100'" || exit 1
$TMUX2 set -p pane-border-status bottom || exit 1
$TMUX2 set -p pane-border-lines heavy || exit 1
compare floating-mixed-config

# Floating pane over a pane that has a scrollbar: the float must draw over the
# scrollbar. Captured with -e since the scrollbar is drawn with styles.
new_scene 40 12
$TMUX2 setw pane-scrollbars on || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=0" || exit 1
$TMUX2 new-pane -x16 -y6 -X22 -Y3 "sh -c 'printf OVERSB; exec sleep 100'" || exit 1
compare floating-over-scrollbar -e

# Floating pane overlapping the client status line: the status line is not part
# of the window scene, so the float is clipped at the window's bottom edge.
new_scene 40 11
$TMUX2 set status on || exit 1
$TMUX2 set status-position bottom || exit 1
$TMUX2 set status-format[0] "" || exit 1
$TMUX2 new-pane -x20 -y6 -X8 -Y7 "sh -c 'printf OVERST; exec sleep 100'" || exit 1
compare floating-over-status

# A window left with only a floating pane: killing the single tiled pane removes
# it from the layout, so the area it occupied is no longer owned by any pane and
# is drawn as EMPTY cells (middle dots) around the float. This is the only way to
# produce a REDRAW_SPAN_EMPTY span.
new_scene 40 12
$TMUX2 new-pane -x20 -y6 -X8 -Y3 "sh -c 'printf FLOAT; exec sleep 100'" || exit 1
tiled=$($TMUX2 list-panes -F '#{pane_floating_flag} #{pane_id}' | \
	awk '$1==0{print $2; exit}') || exit 1
$TMUX2 kill-pane -t "$tiled" || exit 1
compare floating-empty

# Same after pane-border-lines is set to none: empty window background is not a
# pane border and should still use the dotted window fill.
new_scene 40 12
$TMUX2 breakp -W || exit 1
$TMUX2 set pane-border-lines none || exit 1
compare floating-empty-noborder

exit 0
