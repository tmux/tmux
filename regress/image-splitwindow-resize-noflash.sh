#!/bin/sh

# Sizing a new floating pane must not retransmit images in other panes.
# Wait for the initial creation redraw before checking subsequent drag steps.

. ./image-noflash-common.inc

$TMUX new-session -d -s inner -x 60 -y 20 "printf '$SIXEL_HEADER'; exec sh" ||
	exit 1
$TMUX set -g mouse on || fail "set mouse failed"
sleep 0.3

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

start_capture 60 20
assert_image_reached

# Ctrl-drag within the pane, well clear of the image, to create and begin
# sizing a new floating pane. This first press+motion pair both creates the
# pane (one legitimate redraw) and makes the initial resize call.
seq=$(printf '\033[<16;30;10M')
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 0.2
seq=$(printf '\033[<48;35;12M')
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 0.3

n=$($TMUX list-panes | wc -l)
[ "$n" -eq 2 ] || fail "sanity: floating pane was not created (found $n panes)"
: >$TMP

# Continue the drag - only ongoing resize-motion events from here, which is
# what the fix scopes.
seq=$(printf '\033[<48;38;13M')
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 0.15
seq=$(printf '\033[<48;40;15M')
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 0.15
seq=$(printf '\033[<16;40;15m')
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 1

assert_no_retransmit "while sizing a new floating pane"

exit 0
