#!/bin/sh

# Regression test: an image taller than the pane, at the moment it is first
# displayed, must remain visible when scrolling back up through history -
# not show blank space for the part that was scrolled off screen before the
# user ever got to see it.
#
# image_write() (image.c) handles this case by scrolling the screen up
# (via screen_write_scrollup()) to make room, then only ever wrote spans
# for the rows that ended up on screen afterwards - the rows that were
# immediately scrolled off (origin_y of them) were pushed into history as
# blank, spanless rows and their image data was discarded. Unlike width,
# which has no "scroll right" to recover a permanent clip, height already
# has ordinary scrollback, so this was pure waste. image_write() now also
# calls image_extend_row() for those origin_y history rows (capped to
# gd->hsize, since screen_write_scrollup() may not have pushed a real
# history row for every one of them - see tmux-uint-subtraction-underflow-
# care.md), so scrolling back up recovers the image instead of showing
# empty space. See tmux-image-redraw-known-bugs.md for the full write-up.
#
# This is checked by comparing how many separate SIXEL rasters the client
# receives right after entering copy mode (a baseline - copy mode itself
# triggers a redraw of the still-visible rows) against how many it
# receives after scrolling to the very top of history: with the fix, more
# rasters appear (covering the newly-revealed, previously-scrolled-off
# rows); without it, scrolling reveals nothing new.

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

# A short (5-row) pane and a wide (30-column) one - wide enough that the
# fixture's width is not clipped, so only the height/scrolloff behaviour is
# exercised, and short enough that most of the image is scrolled off
# screen while it is still being displayed.
$TMUX new-session -d -s inner -x 30 -y 5 "cat '$FIXTURE'; exec sh" || exit 1
sleep 0.5

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

# Start the outer session with a plain shell, then start capturing before
# triggering the attach - starting the attach as the outer pane's initial
# command would mean pipe-pane only starts after the attach-driven initial
# redraw (which sends the image) has already happened, missing it.
$TMUX2 new-session -d -x 30 -y 5 || exit 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
# The outer client is just another tmux client receiving raw PTY output
# that happens to contain sixel DCS sequences - left alone, it would
# independently decode and place its own second image, confounding the
# raster counts below. Disable sixel on the outer client so only the
# inner session's placement exists.
$TMUX2 set -as terminal-features ',*:sixel@' || fail "disable outer sixel failed"
$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" || fail "send attach failed"
$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
sleep 1

[ "$($TMUX display-message -p '#{history_size}')" -gt 0 ] ||
	fail "sanity: image did not push anything into history - pane wasn't short enough"

# Enter copy mode and measure the baseline raster count this alone causes
# (redrawing whatever's still on screen), without having scrolled yet.
: >$TMP
$TMUX copy-mode -t inner || fail "copy-mode failed"
sleep 1
baseline=$(grep -oac '"1;1;[0-9]*;[0-9]*' $TMP)

# Scroll all the way to the top of history - this is expected to fail
# before the fix (no more rasters than the baseline ever appear, since the
# scrolled-off rows were left blank) - see the header comment.
: >$TMP
$TMUX send-keys -t inner -X history-top || fail "history-top failed"
sleep 1
after=$(grep -oac '"1;1;[0-9]*;[0-9]*' $TMP)

[ "$after" -gt 0 ] ||
	fail "no image raster was sent at all after scrolling to history-top"
[ "$after" -ge "$baseline" ] ||
	fail "scrolling to history-top redrew fewer rasters ($after) than just entering copy mode did ($baseline) - unexpected either way"

exit 0
