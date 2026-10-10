#!/bin/sh

# Alt-dragging a floating pane must not retransmit images in other panes.

. ./image-noflash-common.inc

$TMUX new-session -d -s inner -x 60 -y 20 "printf '$SIXEL_HEADER'; exec sh" ||
	exit 1
$TMUX set -g mouse on || fail "set mouse failed"
FLOAT=$($TMUX new-pane -d -PF '#{pane_id}' -x 16 -y 5 -X 30 -Y 5) ||
	fail "new-pane -X -Y failed"
sleep 0.3

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

start_capture 60 20
assert_image_reached
: >$TMP

FTOP=$($TMUX display-message -p -t "$FLOAT" '#{pane_top}')
FLEFT=$($TMUX display-message -p -t "$FLOAT" '#{pane_left}')
GRABCOL=$((FLEFT + 3))
GRABROW=$((FTOP + 2))

# Alt-drag (Cb meta bit 8, drag bit 32 added while moving) the floating
# pane by a point inside its body, a few steps in a row.
seq=$(printf '\033[<8;%s;%sM' "$GRABCOL" "$GRABROW")
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 0.2
row=$GRABROW
i=0
while [ $i -lt 4 ]; do
	row=$((row + 1))
	seq=$(printf '\033[<40;%s;%sM' "$GRABCOL" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.15
	i=$((i + 1))
done
seq=$(printf '\033[<8;%s;%sm' "$GRABCOL" "$row")
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 1

NEWTOP=$($TMUX display-message -p -t "$FLOAT" '#{pane_top}')
[ "$NEWTOP" != "$FTOP" ] || fail "sanity: floating pane did not move (still at $FTOP)"

assert_no_retransmit "while Alt-dragging an unrelated floating pane"

exit 0
