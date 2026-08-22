#!/bin/sh

# Regression test: extending a copy-mode selection by cursor movement, with
# the view otherwise unmoved (no scrolling), must not retransmit an image
# whose row the cursor passes through.
#
# window_copy_write_one() (window-copy.c) used to write text/highlight
# styling directly over image-covered cells, which - since a character
# write typically clears whatever pixel content a terminal was showing
# there - erased the image with nothing to redraw it back in. Separately,
# window_copy_write_line()'s call to image_redraw_area() used to fire
# unconditionally on every redraw, so even after fixing the erasure, the
# image would still be needlessly recomposited (and briefly flash) on
# every single cursor step even though nothing about it had changed. See
# tmux-image-redraw-known-bugs.md for the full write-up.
#
# This is checked by counting DCS (\033P) sequences in the client's raw
# output during the cursor movement: with the fix, extending a selection
# without scrolling never touches the image, so none should appear.

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

# A small, distinctive SIXEL raster (26x26 pixels) at the top of the pane,
# matching the fixture already used in image-support.sh, followed by
# enough plain lines that the cursor can move down through the image and
# past it without the view needing to scroll.
SIXEL='\033Pq"1;1;26;26#0;2;100;100;100#0!26~-!26~-!26~-!26~-!26B\033\\'
$TMUX new-session -d -s inner -x 40 -y 20 \
    "printf '$SIXEL'; for i in \$(seq 1 15); do echo line\$i; done; exec sh" ||
	exit 1
sleep 0.5

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

# Start the outer session with a plain shell, then start capturing before
# triggering the attach - starting the attach as the outer pane's initial
# command would mean pipe-pane only starts after the attach-driven initial
# redraw (which sends the image) has already happened, missing it.
$TMUX2 new-session -d -x 40 -y 20 || exit 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" || fail "send attach failed"
$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
sleep 1

# Sanity check: the image reached the client at all.
grep -qa '"1;1;26;26' $TMP || fail "sanity: image never reached the client"
: >$TMP

# Enter copy-mode, scroll to the top (where the image is) and select down
# through it one cursor step at a time - the view does not need to scroll
# for any of this, since the image is already at the top of what is
# visible.
$TMUX copy-mode -t inner || fail "copy-mode failed"
$TMUX send-keys -X history-top || fail "history-top failed"
sleep 0.2
: >$TMP
$TMUX send-keys -X begin-selection || fail "begin-selection failed"
i=0
while [ $i -lt 6 ]; do
	$TMUX send-keys -X cursor-down || fail "cursor-down failed"
	sleep 0.15
	i=$((i + 1))
done
sleep 0.5

# No DCS sequence should have been sent - the image's row was never
# disturbed by any of this. This is expected to fail before the fix - see
# the header comment.
dcs=$(grep -ac "$(printf '\033P')" $TMP)
[ "$dcs" -eq 0 ] ||
	fail "image was retransmitted ($dcs times) while just moving the selection cursor"

exit 0
