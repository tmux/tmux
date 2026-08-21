#!/bin/sh

# Regression test for a KNOWN, NOT YET FIXED bug - this test currently
# FAILS, and is expected to keep failing until the general damage-tracking
# redraw work lands (IMAGE-REDRAW-PLAN.md, steps 2-4). Left failing
# deliberately rather than skipped, so `make` in regress/ shows accurate,
# live status of whether this is fixed yet.
#
# On a client detected as SIXEL-capable, entering copy-mode on a pane and
# moving the cursor can blank a floating pane's border-status title text
# (and can also disturb window-level border/title chrome), without
# redrawing it back in. Root cause: window_make_pane_status()
# (window-border.c) gates the physical redraw of border-status text on a
# logical content diff (grid_compare against a cached copy), not on
# whether the physical screen cells were disturbed by something else in
# the meantime. See tmux-image-redraw-known-bugs.md ("border-status text
# cache ignores physical damage") and IMAGE-REDRAW-DISCUSSION.md for the
# full write-up.
#
# A proper damage-tracking compositor gates redraw on "was this region
# touched", not "did the logical content change", so the disturbing erase
# would itself register as damage and force recomposition regardless of
# content diff. Design requirement for that work: border/status/scrollbar
# spans must participate in the same damage-driven composition as pane
# content, or this bug survives the rewrite unchanged.
#
# No mouse is required to reproduce this - entering copy-mode (matching a
# PageUp key binding) and moving the cursor once is enough.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

CONF=$(mktemp)
cat > "$CONF" <<EOF
set -g mouse on
set -g pane-scrollbars on
set -g pane-border-status top
set -g status 2
bind-key -n PPage "copy-mode -u"
bind-key -n NPage "copy-mode -d"
EOF

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Libsw$$ -f/dev/null"
TMUX2="$TEST_TMUX -Libsw-inner$$ -f $CONF"
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null; rm -f $TMP $CONF" 0 1 15

$TMUX2 new-session -d -x 60 -y 20 "sh -c 'i=0; while [ \$i -lt 30 ]; do printf \"TILE%02d\\n\" \$i; i=\$((i + 1)); done; exec sleep 100'"
$TMUX2 new-pane -x20 -y6 -X8 -Y3 -T FLOATTITLE "sh -c 'printf FLOATCONTENT; exec sleep 100'"
[ "$($TMUX2 display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX2 set -as terminal-features ',*:sixel'

$TMUX new-session -d -x 60 -y 20
$TMUX send -l "$TMUX2 attach"
$TMUX send Enter
sleep 1

# Sanity check: the floating pane's OWN border row shows its title before
# anything disturbs it. (A bare 'FLOATTITLE' grep on the whole capture is
# not enough - the outer client's title bar separately reflects the pane
# title via an unrelated OSC title escape, regardless of this bug.)
$TMUX capturep -p >$TMP || exit 1
grep '┌' $TMP | grep -q 'FLOATTITLE' || exit 1

# Enter copy-mode on the (active) floating pane, matching the PageUp
# binding, then move the cursor once, matching the Up binding.
$TMUX2 copy-mode -u -t%1
sleep 1
$TMUX2 send-keys -t%1 -X cursor-up
sleep 1

$TMUX capturep -p >$TMP || exit 1

# The border frame and its title text should both still be there. This is
# expected to fail today - see the header comment.
BORDERLINE=$(grep '┌' $TMP)
[ -n "$BORDERLINE" ] || exit 1
echo "$BORDERLINE" | grep -q 'FLOATTITLE' || exit 1

exit 0
