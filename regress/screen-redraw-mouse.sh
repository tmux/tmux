#!/bin/sh

# Test that a redraw does not churn the mouse mode.
#
# When the mouse is on, redraw_draw() must preserve the input modes and only
# drop the (visual) cursor while drawing a frame. A buggy redraw turned every
# mode off (including mouse) and let reset_state turn it back on, so each
# refresh emitted a full disable/enable of the mouse (ESC[?1000l ... ESC[?1000h)
# to the client. That flicker is what this test guards against.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2 -f/dev/null"
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" \
	0 1 15

# Inner session with the mouse enabled, attached inside an outer pane so the
# outer pane captures whatever the inner tmux writes to its client terminal.
$TMUX2 new -d -x80 -y24 || exit 1
$TMUX2 set -g mouse on || exit 1
$TMUX new -d -x80 -y24 "$TMUX2 attach" || exit 1
sleep 1

# Capture the inner tmux's raw client output, then force several redraws. A
# buggy redraw disables the mouse every time, so a few attempts make the test
# reliable without depending on the timing of a single refresh.
$TMUX pipe-pane -O "cat >$TMP" || exit 1
sleep 1
i=0
while [ $i -lt 5 ]; do
	$TMUX2 refresh-client || exit 1
	sleep 0.5
	i=$((i + 1))
done
$TMUX pipe-pane || exit 1

# After startup, a redraw must not disable the mouse (ESC[?1000l).
if grep -q '\[?1000l' "$TMP"; then
	echo "[FAIL] redraw churned the mouse mode"
	exit 1
fi

[ -n "$VERBOSE" ] && echo "[PASS] redraw preserved the mouse mode"
exit 0
