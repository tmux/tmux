#!/bin/sh

# Exercise the scene caching in screen-redraw.c. A scene is built once and reused
# until it is invalidated; redraw_get_scene rebuilds it when the window changes,
# the generation number is bumped (panes moved/resized/swapped), or the offset
# changes. These tests make such a change in place and capture afterwards: if the
# matching invalidation did not fire, the stale cached scene would be drawn and
# the capture would not match the golden.
#
# Each scene is rendered in an inner tmux attached inside an outer tmux pane. The
# outer pane is captured and compared with a golden in screen-redraw-results/.
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

new_scene() {
	$TMUX2 neww -d "sh -c 'i=0; while [ \$i -lt 13 ]; do printf \"BASE%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
	$TMUX2 selectw -t:\$ || exit 1
	$TMUX2 resizew -x40 -y14 || exit 1
}

C="sh -c 'i=0; while [ \$i -lt 5 ]; do printf \"PANE%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'"

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX2 new -d -x40 -y14 "sh -c 'exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1

$TMUX new -d -x40 -y14 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

# --- Generation change: a pane is resized in place. ---

# Two columns; first capture pins the initial divider position. The scene is
# built here and cached.
new_scene
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
compare cache-resize-before

# Move the divider left. resize-pane bumps the generation, so the cached scene
# must be rebuilt and the divider must appear in its new position.
$TMUX2 resize-pane -t0 -L 6 || exit 1
compare cache-resize-after

# --- Generation change: panes are swapped/rotated in place. ---

# Three columns, each pane titled so the order is visible. The pane status line
# makes a swap show up in the captured scene.
new_scene
$TMUX2 setw pane-border-format " #{pane_title} " || exit 1
$TMUX2 setw pane-border-status top || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t:.0 -T AAA || exit 1
$TMUX2 selectp -t:.1 -T BBB || exit 1
$TMUX2 selectp -t:.2 -T CCC || exit 1
$TMUX2 selectp -t:.0 || exit 1
compare cache-rotate-before

# Rotate the panes; the titles must move with them in the rebuilt scene.
# rotate-window keeps the cell geometry, so only its scene invalidation makes
# this differ from cache-rotate-before.
$TMUX2 rotate-window || exit 1
$TMUX2 selectp -t:.0 || exit 1
compare cache-rotate-after

# --- Generation change: two panes swapped in place. ---

# swap-pane likewise moves panes between cells without changing geometry; the
# swapped titles must appear in the rebuilt scene.
new_scene
$TMUX2 setw pane-border-format " #{pane_title} " || exit 1
$TMUX2 setw pane-border-status top || exit 1
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 selectp -t:.0 -T LEFT || exit 1
$TMUX2 selectp -t:.1 -T RIGHT || exit 1
$TMUX2 selectp -t:.0 || exit 1
compare cache-swap-before

$TMUX2 swap-pane -d -s:.0 -t:.1 || exit 1
$TMUX2 selectp -t:.0 || exit 1
compare cache-swap-after

# --- Window change: the client switches between two differently laid out
# windows and back. The scene is keyed on the window, so each must show its own
# layout and switching back must not show the other window's cached scene. ---

# Window A: a single pane (no internal border).
$TMUX2 neww -d "sh -c 'i=0; while [ \$i -lt 13 ]; do printf \"WINA%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 selectw -t:\$ || exit 1
$TMUX2 resizew -x40 -y14 || exit 1
A=$($TMUX2 display -p '#{window_id}') || exit 1

# Window B: a top/bottom split (a horizontal border).
$TMUX2 neww -d "sh -c 'i=0; while [ \$i -lt 13 ]; do printf \"WINB%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 selectw -t:\$ || exit 1
$TMUX2 resizew -x40 -y14 || exit 1
$TMUX2 splitw -v "$C" || exit 1
B=$($TMUX2 display -p '#{window_id}') || exit 1

$TMUX2 selectw -t$A || exit 1
compare cache-window-a
$TMUX2 selectw -t$B || exit 1
compare cache-window-b
$TMUX2 selectw -t$A || exit 1
compare cache-window-a-again

# --- Size change: the client (and so the scene) is resized. ---

# With window-size latest the inner window tracks the client size, so resizing
# the outer window resizes the inner client and the scene. The cached scene must
# be rebuilt at the new size (the scene->sx/sy mismatch path in
# redraw_get_scene), not drawn at the old size.
$TMUX2 set -g window-size latest || exit 1
$TMUX2 neww -d "sh -c 'i=0; while [ \$i -lt 13 ]; do printf \"SIZE%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 selectw -t:\$ || exit 1
$TMUX2 splitw -v "$C" || exit 1
compare cache-resizeclient-before

$TMUX resizew -x30 -y10 || exit 1
compare cache-resizeclient-after

exit 0
