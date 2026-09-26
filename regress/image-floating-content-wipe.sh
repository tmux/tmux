#!/bin/sh

# Regression test for a fixed bug: on a client detected as SIXEL-capable, a
# pane-content redraw (PANE_REDRAW) erased the image backend's rectangle
# using the redrawing pane's raw nominal geometry (wp->xoff/yoff/sx/sy)
# instead of the cells it actually owns in the current scene. When a
# floating pane occluded part of that rectangle, the erase blanked the
# floating pane's on-screen area, and since the subsequent text redraw
# correctly only refills cells the redrawing pane owns, nothing repainted
# it back in - the floating pane's content stayed blank.
#
# Fixed in screen-redraw.c:redraw_draw_pane_lines() by erasing only the
# cell ranges the pane actually owns per line (its REDRAW_SPAN_PANE spans),
# not its raw rectangle. See tmux-sixel-erase-ignores-occlusion.md and
# IMAGE-REDRAW-DISCUSSION.md for the full write-up.
#
# Confirmed by direct A/B test against this exact sequence: reliably wipes
# the floating pane's content on the unfixed code, reliably leaves it
# intact on the fixed code.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lifcw$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lifcw-inner$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null; rm -f $TMP" 0 1 15

$TMUX2 new-session -d -x 60 -y 20 "sh -c 'i=0; while [ \$i -lt 30 ]; do printf \"TILE%02d\\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" \
	|| exit 1
$TMUX2 new-pane -x20 -y6 -X8 -Y3 -T FLOATTITLE "sh -c 'printf FLOATCONTENT; exec sleep 100'" \
	|| exit 1
[ "$($TMUX2 display-message -p '#{image_support}')" = 0 ] && exit 0

# The attaching client must be detected as SIXEL-capable - this bug does
# not reproduce on a fallback or Kitty client.
$TMUX2 set -as terminal-features ',*:sixel' || exit 1

$TMUX new-session -d -x 60 -y 20 || exit 1
$TMUX send-keys -l "$TMUX2 attach-session" || exit 1
$TMUX send-keys Enter || exit 1
sleep 1

# Sanity check: the floating pane's content is visible before anything
# disturbs it.
$TMUX capturep -p >$TMP || exit 1
grep -q 'FLOATCONTENT' $TMP || exit 1

# Click the tiled pane (making it active), enter copy-mode scrolled up
# (matching a PageUp binding), move up one line (matching an Up binding),
# then exit copy-mode - the exit is what triggers window_pane_reset_mode(),
# which is what actually caused the erase.
$TMUX2 select-pane -t%0 || exit 1
sleep 1
$TMUX2 copy-mode -u -t%0 || exit 1
sleep 1
$TMUX2 send-keys -t%0 -X cursor-up || exit 1
sleep 1
$TMUX2 send-keys -t%0 -X cancel || exit 1
sleep 1

$TMUX capturep -p >$TMP || exit 1
grep -q 'FLOATCONTENT' $TMP || exit 1

exit 0
