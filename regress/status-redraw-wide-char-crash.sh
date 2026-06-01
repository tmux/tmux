#!/bin/sh

# Regression test: crash in screen_write_redraw_line when status bar
# contains a wide character (emoji).
#
# When the status line is redrawn, it is first filled with spaces
# (width 1). If the previous content had a wide character (width 2),
# writing a space over it triggers screen_write_overwrite() which sets
# redraw = 1. Then screen_write_redraw_line() dereferences ctx->wp,
# but wp is NULL for status-bar screens, causing a SIGSEGV.
#
# A real PTY client is required because control-mode clients skip
# status bar rendering entirely (CLIENT_CONTROL flag causes
# server_client_check_redraw to return early).

PATH=/bin:/usr/bin
TERM=xterm-256color
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX new -d -x 40 -y 10 || exit 1
SERVER_PID=$($TMUX display-message -p '#{pid}')

# Put a wide character in the status bar.
$TMUX set -g status-right '🤖' || exit 1

# Attach a PTY client via script(1).  Pipe from sleep to keep stdin
# open; redirect stdout/stderr to prevent escape sequence leakage.
sleep 10 | script -qfc "$TMUX attach" /dev/null >/dev/null 2>&1 &
SCRIPT_PID=$!

# Wait for the client to connect.
n=0
while [ "$n" -lt 50 ]; do
    CLIENT=$($TMUX list-clients -F '#{client_tty}' 2>/dev/null | head -1)
    [ -n "$CLIENT" ] && break
    sleep 0.1
    n=$((n + 1))
done
[ -z "$CLIENT" ] && exit 1

# Resize the PTY with stty to trigger a status bar redraw.  This sends
# SIGWINCH to the tmux client, unlike refresh-client -C which only
# works for control-mode clients.
stty -F "$CLIENT" rows 10 cols 40 2>/dev/null
sleep 0.5
n=0
while [ "$n" -lt 10 ]; do
    [ ! -e "/proc/$SERVER_PID" ] && break
    grep -q '^State:.*Z' "/proc/$SERVER_PID/status" 2>/dev/null && break
    stty -F "$CLIENT" cols $((41 + n % 2)) 2>/dev/null
    sleep 0.3
    stty -F "$CLIENT" cols 40 2>/dev/null
    sleep 0.3
    n=$((n + 1))
done

# Check if the server survived.
if [ -e "/proc/$SERVER_PID/status" ] &&    ! grep -q '^State:.*Z' "/proc/$SERVER_PID/status" 2>/dev/null; then
    $TMUX kill-server 2>/dev/null
    wait "$SCRIPT_PID" 2>/dev/null
    exit 0
fi

wait "$SCRIPT_PID" 2>/dev/null
exit 1
