#!/bin/sh

# Tests of the -k flag on the mode-entering commands (cmd-copy-mode.c and
# cmd-choose-tree.c). With -k the pane is killed when the mode is exited: this
# is stored on the mode entry in window_pane_set_mode() and acted on in
# window_pane_reset_mode() (window.c). It is exercised here for:
#
# - copy-mode -k (window-copy.c);
# - choose-tree -k (window-tree.c);
# - choose-buffer -k (window-buffer.c).
# - display-panes -k (window-panes.c).
#
# choose-tree and choose-buffer share cmd_choose_tree_exec(), which also backs
# choose-client, customize-mode, and display-panes, so choose-client and
# customize-mode are not repeated.
#
# Each mode is entered in the active pane of a two-pane window: exiting with
# -k must remove that pane and leave the other. copy-mode is left with the
# server-side "-X cancel" and needs no client, so those tests run first. The
# tree modes only act on a key once the client has drawn the mode, so - as in
# choose-tree.sh - a second server then provides a client (an inner "tmux
# attach") and its pane is captured to wait until the mode has rendered before
# the exit key is sent.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	exit 1
}

# capture the screen rendered by the inner client.
capture()
{
	$TMUX2 capture-pane -p -t out:0 2>/dev/null
}

# wait_clients $n: wait until the test server has exactly $n clients.
wait_clients()
{
	i=0
	while [ "$i" -lt 50 ]; do
		c=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$c" -eq "$1" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "expected $1 clients, have $c"
}

# wait_mode $target $state: wait until a pane enters (1) or leaves (0) mode.
wait_mode()
{
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display-message -p -t "$1" '#{pane_in_mode}' \
		    2>/dev/null)
		[ "$got" = "$2" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "pane $1 mode state is '$got', expected '$2'"
}

# wait_for $marker: wait until the rendered screen contains $marker.
wait_for()
{
	i=0
	while [ "$i" -lt 50 ]; do
		capture | grep -F -q "$1" && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1' to render"
}

# pane_gone $pane-id: true if the pane no longer exists. (display-message -t
# on a missing pane id falls back to a default target and succeeds, so the
# pane list is searched instead.)
pane_gone()
{
	! $TMUX list-panes -s -t m -F '#{pane_id}' 2>/dev/null | \
	    grep -q -x "$1"
}

# open_window: make a fresh two-pane window and leave the new (active) pane
# as $active and the other as $other.
open_window()
{
	$TMUX new-window -t m: -n w 'cat' || fail "new-window failed"
	$TMUX split-window -t m:w 'cat' || fail "split-window failed"
	other=$($TMUX display-message -p -t m:w.0 '#{pane_id}')
	active=$($TMUX display-message -p -t m:w '#{pane_id}')
}

# check_killed $label: wait for the active pane to be killed, leaving only the
# other pane, then drop the window.
check_killed()
{
	i=0
	while [ "$i" -lt 50 ]; do
		pane_gone "$active" && break
		sleep 0.2
		i=$((i + 1))
	done
	pane_gone "$active" || fail "$1: pane not killed on exit"
	panes=$($TMUX list-panes -t m:w -F '#{pane_id}' | tr '\n' ' ')
	[ "$panes" = "$other " ] || \
	    fail "$1: expected only $other left, have $panes"
	$TMUX kill-window -t m:w 2>/dev/null
}

# Session m; window 0 keeps a live pane so the session (and later the client)
# survives each test killing a pane.
$TMUX new-session -d -s m -x 80 -y 24 'cat' || exit 1

# --- copy-mode -k kills the pane, plain copy-mode does not -------------------
#
# copy-mode is exited with the server-side "-X cancel", so no client is needed
# and none is attached yet.
open_window
$TMUX copy-mode -k -t m:w || fail "copy-mode -k failed"
wait_mode "$active" 1
$TMUX send-keys -t m:w -X cancel || fail "copy cancel failed"
check_killed 'copy-mode -k'

open_window
$TMUX copy-mode -t m:w || fail "copy-mode failed"
wait_mode "$active" 1
$TMUX send-keys -t m:w -X cancel || fail "copy cancel failed"
wait_mode "$active" 0
pane_gone "$active" && fail 'copy-mode: pane killed without -k'
$TMUX kill-window -t m:w 2>/dev/null

# --- choose-tree -k, choose-buffer -k, and display-panes -k kill the pane ---
#
# These need the client to draw the mode before a key acts, so attach one now.
# A paste buffer is needed for choose-buffer to have something to show, and a
# distinct -F marker per call is waited for in the capture so the exit key is
# only sent once the mode is drawn.
$TMUX set-buffer 'mode-kill buffer' || exit 1
$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t m" || exit 1
wait_clients 1
open_window
$TMUX choose-tree -k -F 'TREEMARK' -t m:w || fail "choose-tree -k failed"
wait_for 'TREEMARK'
$TMUX send-keys -t m:w q || fail "choose-tree exit failed"
check_killed 'choose-tree -k'

open_window
$TMUX choose-buffer -k -F 'BUFMARK' -t m:w || fail "choose-buffer -k failed"
wait_for 'BUFMARK'
$TMUX send-keys -t m:w q || fail "choose-buffer exit failed"
check_killed 'choose-buffer -k'

open_window
$TMUX set -g display-panes-format 'DPMARK' || fail "set display-panes-format"
$TMUX display-panes -k -d 0 -t m:w || fail "display-panes -k failed"
wait_for 'DPMARK'
$TMUX send-keys -t m:w q || fail "display-panes exit failed"
check_killed 'display-panes -k'

exit 0
