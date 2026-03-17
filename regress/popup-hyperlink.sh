#!/bin/sh

# Test that OSC 8 hyperlinks in display-popup survive a redraw.
#
# Uses nested tmux: an outer server captures terminal output from an inner
# server whose popup contains a hyperlink. After forcing a full redraw of
# the inner server (which exercises popup_draw_cb), the outer pane's
# captured content must still contain the hyperlink URI.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
TMUX2="$TEST_TMUX -Ltest2"
TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

# Start the inner tmux session (detached).
$TMUX2 -f/dev/null new -d -x60 -y20 || exit 1

# Start the outer tmux with the inner tmux attached as its pane command.
# The inner tmux draws to the outer pane's PTY so we can capture its output.
$TMUX -f/dev/null new -d -x80 -y24 "$TMUX2 a" || exit 1
sleep 1

# Open a popup in the inner tmux containing an OSC 8 hyperlink.
# display-popup blocks until the popup closes, so run it in the background.
$TMUX2 display-popup -w40 -h6 \
  "printf '\033]8;;https://example.com\033\\Click\033]8;;\033\\\n'; sleep 30" &
sleep 1

# Force a full client redraw, which triggers popup_draw_cb.
$TMUX2 refresh-client
sleep 1

# Capture the outer pane content with escape sequences (-e includes OSC 8).
$TMUX capturep -peS0 -E23 >$TMP

# The captured content must contain the hyperlink URI after the redraw.
grep -q 'example.com' $TMP || exit 1

exit 0
