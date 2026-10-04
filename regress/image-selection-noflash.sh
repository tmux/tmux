#!/bin/sh

# Moving a copy-mode selection cursor must not retransmit unchanged images.

. ./image-noflash-common.inc

# The image, at the top of the pane, followed by enough plain lines that the
# cursor can move down through it and past it without the view needing to
# scroll.
$TMUX new-session -d -s inner -x 40 -y 20 \
    "printf '$SIXEL_HEADER'; for i in \$(seq 1 15); do echo line\$i; done; exec sh" ||
	exit 1
sleep 0.5

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

start_capture 40 20
assert_image_reached
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

assert_no_retransmit "while just moving the selection cursor"

exit 0
