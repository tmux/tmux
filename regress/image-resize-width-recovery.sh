#!/bin/sh

# Regression test: an image wider than the pane at the time it was
# displayed must show more of itself once the pane grows wide enough,
# instead of staying clipped to its original width forever.
#
# image_write() (image.c) clips an image's width to whatever fit in the
# pane when it was first displayed, and never revisits that decision -
# unlike height, which recovers naturally through ordinary scrollback,
# there is no "scroll right", so the clipped columns were permanently
# discarded. window_pane_resize() (window.c) now calls
# image_grid_resize_width() after a pane grows wider, which extends each
# existing placement's spans - using the image's own retained, immutable
# pixel data - up to whichever is smaller: the image's full width or the
# new pane width. See tmux-image-redraw-known-bugs.md for the full
# write-up.
#
# This is checked via the SIXEL raster widths reported in the client's raw
# output before and after widening the window. The redraw triggered by the
# resize is damage-based (only the newly-uncovered columns are dirtied), so
# it does not redraw the whole row as one wider raster - it sends the
# already-correct clipped portion's width again untouched, plus a *second*,
# separate raster covering just the newly-added columns. So rather than
# looking for a single wider raster, this checks that the widths seen
# across both redraws, added together, account for the fixture's full
# pixel width - i.e. the previously-clipped remainder actually appeared.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}
fail()
{
	echo "$*" >&2
	cleanup
	exit 1
}

cleanup

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

FIXTURE=$(pwd)/monkey-2.sixel.txt
FULL=$(grep -oa '"1;1;[0-9]*;[0-9]*' $FIXTURE | head -1 | cut -d';' -f3)
[ -n "$FULL" ] || fail "could not read fixture's own raster width"

# The fixture is 360px wide - narrow enough that a 12-column pane clips it,
# comfortably below the 360px it would take to show in full.
$TMUX new-session -d -s inner -x 12 -y 40 "cat '$FIXTURE'; exec sh" || exit 1
sleep 0.5

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

# Start the outer session with a plain shell, then start capturing before
# triggering the attach - starting the attach as the outer pane's initial
# command would mean pipe-pane only starts after the attach-driven initial
# redraw (which sends the image) has already happened, missing it.
$TMUX2 new-session -d -x 12 -y 40 || exit 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
# The outer client is just another tmux client receiving raw PTY output
# that happens to contain sixel DCS sequences - left alone, it would
# independently decode and place its own second, differently-sized image
# on top of the inner session's, confounding the raster-width measurement
# below. Disable sixel on the outer client so only the inner session's
# placement exists.
$TMUX2 set -as terminal-features ',*:sixel@' || fail "disable outer sixel failed"
$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" || fail "send attach failed"
$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
sleep 1

before=$(grep -oa '"1;1;[0-9]*;' $TMP | grep -o '[0-9]*' | sort -n | tail -1)
[ -n "$before" ] || fail "sanity: image never reached the client"
[ "$before" -lt 360 ] || fail "sanity: image was not clipped by the narrow pane ($before)"
: >$TMP

# Widen the window well past the image's full width - resize the outer
# client, since the inner session's displayed size follows whatever
# terminal size its attached client actually has.
$TMUX2 resize-window -t "$OUTER" -x 30 -y 40 || fail "resize-window failed"
sleep 1
$TMUX refresh-client || fail "refresh-client failed"
sleep 1

# The widths seen after the resize, added together (deduplicated - the
# same chunk may legitimately be retransmitted), should account for the
# fixture's full pixel width: this is expected to fail before the fix,
# where the newly-uncovered columns are never redrawn at all and only the
# original clipped width ever appears - see the header comment.
after_total=$(grep -oa '"1;1;[0-9]*;' $TMP | grep -o '[0-9]*' | sort -nu |
	awk '{s+=$1} END{print s+0}')
[ "$after_total" -gt 0 ] ||
	fail "no image raster was sent at all after widening the window"
[ "$after_total" -ge $((FULL - 20)) ] ||
	fail "image stayed clipped at ${before}px after widening the window (widths summed to only ${after_total}px, expected close to ${FULL}px)"

exit 0
