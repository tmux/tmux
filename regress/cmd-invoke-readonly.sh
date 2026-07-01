#!/bin/sh

# Read-only client enforcement.
#
# When the invoking client is read-only, cmd_invoke builds each command and
# rejects it (without running it) unless the command entry is marked read-only.
# A read-only client is created by attaching with -r; the nested-tmux pattern
# from cmd-invoke-deferred.sh is used so a real read-only client presses a key.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT="$TEST_TMUX -Ltest -f/dev/null"
IN="$TEST_TMUX -Ltest2 -f/dev/null"

$OUT kill-server 2>/dev/null
$IN kill-server 2>/dev/null
trap "$OUT kill-server 2>/dev/null; $IN kill-server 2>/dev/null" 0 1 15

fail() { echo "[FAIL] $1" >&2; exit 1; }
settle() { sleep 0.5; }

# Inner session: a key bound to a non-read-only command.
$IN new -d -x80 -y23 -nbase "sh -c 'exec sleep 1000'" || exit 1
$IN set -g status on || exit 1
$IN set -g status-keys emacs || exit 1
$IN set -g window-size manual || exit 1
$IN bind -n M-w new-window -dn ROTRY || exit 1

# Outer session hosts the inner client, attached read-only with -r.
$OUT new -d -x80 -y24 || exit 1
$OUT set -g status off || exit 1
$OUT set -g window-size manual || exit 1
$OUT send-keys -l "$IN attach -r" || exit 1
$OUT send-keys Enter || exit 1
sleep 1

[ "$($IN list-clients -F '#{client_readonly}')" = 1 ] || fail "client is not read-only"

# Pressing the key runs new-window through the read-only client: it must be
# rejected, leave no window, and report the error.
$OUT send-keys M-w || exit 1
settle
$IN list-windows -F '#{window_name}' | grep -qx ROTRY && \
	fail "read-only client was allowed to create a window"
$OUT capture-pane -p | grep -qi 'read-only' || fail "no read-only error was shown"

# Positive control: the same command with no client is not read-only and runs,
# proving the command itself is valid and only the read-only client blocked it.
$IN new-window -dn OKWIN || exit 1
$IN list-windows -F '#{window_name}' | grep -qx OKWIN || fail "control new-window did not run"

$OUT kill-server 2>/dev/null
$IN kill-server 2>/dev/null
exit 0
