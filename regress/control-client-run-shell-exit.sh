#!/bin/sh

# A waited run-shell command can complete after a control client has closed its
# input and exited. The command queue still references the client until the job
# callback runs, but control_stop has already destroyed c->control_state. Late
# command output must be discarded instead of dereferencing the stopped control
# state and killing the server.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

OUT=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; rm -f $OUT" 0 1 15

$TMUX -f/dev/null new-session -d -s main || exit 1

printf '%s\n' 'run-shell "sleep 0.2; echo TEST"' |
	$TMUX -f/dev/null -C >"$OUT" 2>&1

sleep 1

$TMUX has-session -t main 2>/dev/null || {
	echo "server exited after late run-shell output"
	cat "$OUT"
	exit 1
}

grep -q '^%exit' "$OUT" || {
	echo "control client did not exit"
	cat "$OUT"
	exit 1
}

exit 0
