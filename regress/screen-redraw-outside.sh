#!/bin/sh

# Exercise screen-redraw.c when the window is smaller than the attached client,
# so part of the client is outside the window. screen-redraw.c fills the outside
# area and draws a real border along the window's right and/or bottom edge (not
# just where panes meet). This checks redraw_get_window_offset and the OUTSIDE
# span handling, including how the window-edge border joins the pane borders.
#
# Each scene is rendered in an inner tmux attached inside an outer tmux pane.
# The outer client is 40x14; the inner window is made smaller with resizew. The
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

# new_scene <width> <height>: fresh inner window smaller than the 40x14 client.
new_scene() {
	$TMUX2 neww -d "sh -c 'i=0; while [ \$i -lt $(($2 - 1)) ]; do printf \"OUT%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
	$TMUX2 selectw -t:\$ || exit 1
	$TMUX2 resizew -x$1 -y$2 || exit 1
}

C="sh -c 'i=0; while [ \$i -lt 8 ]; do printf \"PAN%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'"

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX2 new -d -x40 -y14 "sh -c 'i=0; while [ \$i -lt 13 ]; do printf \"OUT%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1

$TMUX new -d -x40 -y14 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

# Single pane, narrower than the client: a border along the right edge.
new_scene 28 14
compare outside-right-single

# Narrower than the client with a left/right split: the inter-pane border plus a
# real border on the window's right edge, then the outside area.
new_scene 28 14
$TMUX2 splitw -h "$C" || exit 1
compare outside-right-split

# Shorter than the client with a top/bottom split: a real border on the window's
# bottom edge.
new_scene 40 9
$TMUX2 splitw -v "$C" || exit 1
compare outside-bottom-split

# Smaller in both dimensions with a 2x2 grid: borders on the right and bottom
# edges meeting the internal pane borders at the corner.
new_scene 28 9
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 select-layout tiled || exit 1
compare outside-both-2x2

# Window BIGGER than the client: only part of the window is viewed and the view
# can be panned (refresh-client). This exercises a non-zero scene offset.
# A 2x2 grid in a 60x20 window viewed through the 40x14 client.
new_scene 60 20
$TMUX2 splitw -h "$C" || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 selectp -t0 || exit 1
$TMUX2 splitw -v "$C" || exit 1
$TMUX2 select-layout tiled || exit 1

# Default view: the top-left of the window.
$TMUX2 refresh-client -U 100 || exit 1
$TMUX2 refresh-client -L 100 || exit 1
compare bigger-topleft

# Panned to the bottom-right of the window (down and right to the limit).
$TMUX2 refresh-client -D 100 || exit 1
$TMUX2 refresh-client -R 100 || exit 1
compare bigger-bottomright

exit 0
