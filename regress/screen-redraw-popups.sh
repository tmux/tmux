#!/bin/sh

# Exercise drawing of popups (display-popup) over the window scene. A popup is an
# overlay drawn on top of the redraw scene (the overlay_draw path in
# screen-redraw.c), so this guards against regressions in how popups appear.
#
# A popup is modal and stays open until its command exits, so each scene fully
# re-creates the servers and re-attaches; the popup is opened in the background
# (display-popup blocks the client that runs it) and the outer pane is captured
# while it is open.
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

C="sh -c 'i=0; while [ \$i -lt 13 ]; do printf \"POP%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'"

# setup: fresh inner window attached inside a fresh outer pane, 40x14.
setup() {
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	$TMUX2 new -d -x40 -y14 "$C" || exit 1
	$TMUX2 set -g status off || exit 1
	$TMUX2 set -g window-size manual || exit 1
	$TMUX2 resizew -x40 -y14 || exit 1
	$TMUX new -d -x40 -y14 || exit 1
	$TMUX set -g status off || exit 1
	$TMUX set -g window-size manual || exit 1
	$TMUX set -g default-terminal "tmux-256color" || exit 1
	$TMUX send -l "$TMUX2 attach" || exit 1
	$TMUX send Enter || exit 1
	sleep 1
}

# popup <args>: open a popup running a fixed command, in the background (it stays
# open because the command sleeps; the servers are killed at the next setup).
popup() {
	$TMUX2 display-popup "$@" -E "sh -c 'printf POPUP; exec sleep 100'" &
	sleep 1
}

# Basic popup over a single pane.
setup
popup -w20 -h6 -x6 -y3
compare popup-basic

# Popup over a split: drawn on top of the pane border.
setup
$TMUX2 splitw -h "$C" || exit 1
popup -w24 -h8 -x8 -y3
compare popup-over-split

# Popup with no border lines (-B).
setup
popup -B -w20 -h6 -x6 -y3
compare popup-noborder

# Popup with double border lines.
setup
popup -b double -w20 -h6 -x6 -y3
compare popup-double

exit 0
