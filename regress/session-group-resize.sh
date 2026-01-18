#!/bin/sh

# Test that window-size=latest resizes windows correctly when switching
# windows in session groups. When a client switches to a window, it should
# resize immediately to match that client's size.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP1=$(mktemp)
TMP2=$(mktemp)
trap "rm -f $TMP1 $TMP2" 0 1 15

# Create a session with two windows, staying on window 0.
$TMUX -f/dev/null new -d -s test -x 20 -y 6 || exit 1
$TMUX neww -t test || exit 1
$TMUX selectw -t test:0 || exit 1

# Attach a small 20x6 client in control-mode and have it select window 1. This makes
# the small client the "latest" for window 1. The sleep keeps stdin open so the
# control client stays attached.
(echo "refresh-client -C 20,6"; echo "selectw -t :1"; sleep 5) |
	$TMUX -f/dev/null -C attach -t test >$TMP1 2>&1 &

# Wait for small client to be on window 1.
n=0
while [ $n -lt 20 ]; do
	$TMUX lsc -F '#{client_name} #{window_index}' 2>/dev/null | grep -q " 1$" && break
	sleep 0.1
	n=$((n + 1))
done

# Create a grouped session with a larger 30x10 client, also in control mode. It
# starts on window 0 (inherited), then switches to window 1 with
# `switch-client`.
(echo "refresh-client -C 30,10"; echo "switch-client -t :=1"; sleep 5) |
	$TMUX -f/dev/null -C new -t test -x 30 -y 10 >$TMP2 2>&1 &

# Wait briefly for the switch-client command to execute, then check.
# The resize should happen immediately (within 0.2s).
sleep 0.2
OUT=$($TMUX display -t test:1 -p '#{window_width}x#{window_height}' 2>/dev/null)

# Clean up - kill server (terminates clients). Don't wait for background
# sleeps; they'll be orphaned but harmless.
$TMUX kill-server 2>/dev/null

# Window 1 should have resized to 30x10 (the second client's size).
[ "$OUT" = "30x10" ] || exit 1

exit 0
