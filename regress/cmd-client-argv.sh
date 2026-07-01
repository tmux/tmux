#!/bin/sh

# Client argv parsing and the start-server boundary.
#
# The command line given to the client is parsed and invoked as one sequence.
# This checks: a multi-command argv runs every command and starts the server
# (new-session carries CMD_STARTSERVER); a command without start-server fails
# cleanly against no server; and a parse error leaves no stray server behind.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

# Multi-command argv: starts the server and runs all commands in order.
$TMUX new-session -d -swork -nbase \; set -g @a one \; new-window -dn W2 \; \
	set -g @b two || exit 1
[ "$($TMUX show -gv @a)" = one ] || { echo "argv cmd 2 did not run" >&2; exit 1; }
[ "$($TMUX show -gv @b)" = two ] || { echo "argv cmd 4 did not run" >&2; exit 1; }
got=$($TMUX list-windows -t work -F '#{window_name}' | tr '\n' ',')
[ "$got" = "base,W2," ] || { echo "argv windows: got [$got]" >&2; exit 1; }
# kill-server is asynchronous; wait for the server to actually exit before the
# checks below that require no server to be running.
$TMUX kill-server 2>/dev/null
i=0
while $TMUX has-session 2>/dev/null; do
	sleep 0.05
	i=$((i + 1))
	[ $i -gt 100 ] && break
done

# A command without start-server fails cleanly when no server is running and
# does not fork one.
$TMUX new-window -dn ZZ 2>/dev/null && { echo "new-window unexpectedly succeeded" >&2; exit 1; }
if $TMUX has-session 2>/dev/null; then
	echo "non-start-server command left a stray server" >&2
	exit 1
fi

# A parse error with no server running starts no server.
$TMUX 'if-shell true {' 2>/dev/null && { echo "parse error did not fail" >&2; exit 1; }
if $TMUX has-session 2>/dev/null; then
	echo "parse error left a stray server" >&2
	exit 1
fi

$TMUX kill-server 2>/dev/null
exit 0
