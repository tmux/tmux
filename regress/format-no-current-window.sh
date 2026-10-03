#!/bin/sh

# active_window_index, last_window_index, session_stack and
# window_active_clients read the session current window. Expanding them must
# not kill the server. Covers the list-sessions, list-windows and list-panes
# -F form, and commands that unlink the current window.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*"
	exit 1
}

# Formats that read the current window, expanded together.
FMT='#{active_window_index} #{last_window_index} #{session_stack}'
FMT="$FMT #{window_active_clients} #{window_active_clients_list}"

$TMUX kill-server 2>/dev/null

# No session: the callbacks have nothing to read and must return empty.
out=$($TMUX start\; set -s exit-empty off \; display-message -p "$FMT") ||
    fail "display with no session"
[ "$out" = "    " ] || fail "no session expanded to '$out'"

$TMUX new-session -d -s src -n one -- sleep 30 || fail "new-session src"
[ "$($TMUX display-message -t src -p '#{active_window_index}')" = 0 ] ||
    fail "active window index"
[ "$($TMUX display-message -t src -p '#{last_window_index}')" = 0 ] ||
    fail "last window index"
[ "$($TMUX display-message -t src -p '#{session_stack}')" = 0 ] ||
    fail "session stack"

$TMUX new-window -d -t src -n two -- sleep 30 || fail "new-window"
[ "$($TMUX display-message -t src -p '#{active_window_index}')" = 0 ] ||
    fail "active window index after new-window"
[ "$($TMUX display-message -t src -p '#{last_window_index}')" = 1 ] ||
    fail "last window index after new-window"

$TMUX new-session -d -s dst -n only -- sleep 30 || fail "new-session dst"

# Replace and relink the current window, then expand again.
$TMUX set-hook -g window-linked "list-sessions -F '$FMT'" || fail "set-hook"
$TMUX new-window -k -t src:one -n replaced -- sleep 30 || fail "new-window -k"
$TMUX link-window -k -s src:two -t dst:only || fail "link-window -k"

[ "$($TMUX display-message -t src -p '#{active_window_index}')" = 0 ] ||
    fail "active window index after replace"
[ "$($TMUX display-message -t src -p '#{last_window_index}')" = 1 ] ||
    fail "last window index after replace"
[ "$($TMUX display-message -t dst -p '#{active_window_index}')" = 0 ] ||
    fail "active window index after link"
[ "$($TMUX display-message -t dst -p '#{window_active_clients}')" = 0 ] ||
    fail "window active clients"

$TMUX list-sessions -F "$FMT" >/dev/null || fail "list-sessions"
$TMUX list-windows -a -F "$FMT" >/dev/null || fail "list-windows"
$TMUX list-panes -a -F "$FMT" >/dev/null || fail "list-panes"
$TMUX has-session -t src || fail "src session gone"
$TMUX has-session -t dst || fail "dst session gone"

exit 0
