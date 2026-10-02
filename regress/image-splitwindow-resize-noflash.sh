#!/bin/sh

# Regression test: interactively sizing a new floating pane by dragging
# (new-pane -M, bound by default to C-MouseDrag1Pane/C-MouseDrag1Empty)
# must not retransmit images in other panes of the same window on ongoing
# drag steps.
#
# cmd_split_window_mouse_resize() (cmd-split-window.c) used to call
# server_redraw_window() unconditionally on every motion event while
# sizing the new floating pane, wiping and retransmitting every image in
# the window on each step even though only the new pane's own rectangle
# actually changed. See tmux-image-redraw-known-bugs.md for the full
# write-up.
#
# Creating the floating pane itself causes one legitimate, unrelated
# redraw (pane creation always redraws the session - see
# server_redraw_session() in cmd-split-window.c), so this checks DCS
# (\033P) counts only for the *ongoing* drag-motion steps after that
# initial creation, once c->tty.mouse_drag_update is already bound to the
# resize callback: with the fix, none should appear there.

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
