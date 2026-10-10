#!/bin/sh

# Regression test: clicking a pane's name in a second (#{P:}) status line to
# make it active must not retransmit images in other panes of the same
# window.
#
# The default MouseDown1Status binding is "switch-client -t=". When status
# is set to 2 or more, the second status line's default format lists each
# pane individually with a "pane" mouse range, so clicking a pane name there
# resolves that binding's target to a specific pane rather than a session or
# window - but switch-client still runs through server_client_set_session()
# (server-client.c), which used to call server_redraw_client() (forcing a
# full CLIENT_REDRAWWINDOW pass, wiping and retransmitting every image in
# the window) unconditionally, even though neither the client's session nor
# its current window actually changed - only the active pane within the
# already-current window did, which window_set_active_pane() and
# window_redraw_active_switch() already handle narrowly on their own.
#
# This is checked by counting DCS (\033P) sequences in the client's raw
# output while an inactive pane's own image (never touched by the click) is
# present: with the fix, none should appear.

. ./image-noflash-common.inc

# A distinctive SIXEL raster in the left (initially active) pane, and a
# second, plain pane to its right. Two status lines are enabled, so the
# second status line lists both panes individually and clicking a pane's
# name there is possible.
$TMUX new-session -d -s inner -x 60 -y 12 "printf '$SIXEL_HEADER'; exec sh" ||
	exit 1
$TMUX set -g mouse on || fail "set mouse failed"
$TMUX set -g status 2 || fail "set status failed"
$TMUX split-window -h -t inner 'exec sh' || fail "split-window failed"
$TMUX select-pane -t inner.0 || fail "select-pane failed"
sleep 0.3

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

start_capture 60 12

# Sanity check: the image reaches the client, and pane 0 (holding it) is
# active.
assert_image_reached
[ "$($TMUX display-message -p -t inner.0 '#{pane_active}')" = 1 ] ||
	fail "sanity: pane 0 not active before the click"
: >$TMP

# Locate pane 1's clickable entry in the second (bottom) status line - the
# default window-pane-status-format starts with the pane index, so its
# entry is identifiable by the literal text "1:[".
STATUSLINE=$($TMUX2 capture-pane -p -t "$OUTER" | tail -1)
COL=$(echo "$STATUSLINE" | awk '{print index($0, "1:[")}')
[ "$COL" -gt 0 ] || fail "sanity: could not find pane 1's status entry"
ROW=12

# Click pane 1's name in the second status line - this should make pane 1
# active without disturbing pane 0's already-displayed image.
seq=$(printf '\033[<0;%s;%sM' "$COL" "$ROW")
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 0.2
seq=$(printf '\033[<0;%s;%sm' "$COL" "$ROW")
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 1

[ "$($TMUX display-message -p -t inner.1 '#{pane_active}')" = 1 ] ||
	fail "click did not make pane 1 active"

assert_no_retransmit "after clicking another pane's status entry"

exit 0
