#!/bin/sh

. ./regress-common.inc

# Late command output and parse errors must be safe after a control client
# has exited or lost its session. Keep a separate session alive throughout.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
OUT=$DIR/output
pid=

cleanup()
{
	# Release any shell still waiting, including on a failed assertion.
	touch "$DIR/go"
	exec 3>&-
	if [ -n "$pid" ]; then
		kill -CONT "$pid" 2>/dev/null
		kill "$pid" 2>/dev/null
		wait "$pid" 2>/dev/null
	fi
	stop_server $TMUX || exit 1
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	[ ! -f "$OUT" ] || cat "$OUT" >&2
	exit 1
}

# The shell signals that it has started, then waits for the test to release it.
# Neither the job callback nor its output can run before the client is lost.
GATE="touch '$DIR/started'; while [ ! -f '$DIR/go' ]; do sleep 0.05; done"

start_client()
{
	rm -f "$DIR/go" "$DIR/started"
	$TMUX -C attach -t "$1" <"$DIR/input" >"$OUT" 2>&1 &
	pid=$!
	exec 3>"$DIR/input"
	wait_for_clients 1 $TMUX || fail "control client did not attach"
}

lose_client()
{
	exec 3>&-
	wait_for_exit "$pid" || fail "control client did not exit"
	wait "$pid" || fail "control client failed"
	pid=
	wait_for_clients 0 $TMUX || fail "server did not lose control client"
	grep -q '^%exit' "$OUT" || fail "control client did not report exit"
}

error_or_exit()
{
	! $TMUX has-session -t main 2>/dev/null ||
	    output_has "Unknown command: $1" $TMUX show-messages
}

late_error()
{
	wait_until error_or_exit "$1" || fail "no late error from $1"
	$TMUX has-session -t main || fail "server exited after $1"
}

mkfifo "$DIR/input" || exit 1
$TMUX new-session -d -s main || exit 1

# A waited run-shell retains a queue reference after control_stop has freed
# the control state. Its output must be discarded once the client is lost.
start_client main
printf 'run-shell "%s; echo TEST"\n' "$GATE" >&3
wait_until test -f "$DIR/started" || fail "run-shell did not start"
lose_client
touch "$DIR/go"
# Wait for the job callback and its output handling, not just the shell exit.
wait_for_output '' $TMUX show-messages -J || fail "late run-shell did not finish"
$TMUX has-session -t main || fail "server exited after late run-shell output"

start_client main
printf 'if-shell -b "%s" wibble1\n' "$GATE" >&3
wait_until test -f "$DIR/started" || fail "if-shell did not start"
lose_client
touch "$DIR/go"
late_error wibble1

# A stopped client cannot finish the exit handshake when its session dies.
$TMUX new-session -d -s stopped || exit 1
start_client stopped
printf 'if-shell -b "%s" wibble3\n' "$GATE" >&3
wait_until test -f "$DIR/started" || fail "stopped if-shell did not start"
kill -STOP "$pid" || exit 1
# Arm the timer and destroy its target session in one command queue. The
# timer cannot parse wibble2 until that queue has cleared the client's session.
$TMUX run-shell -b -d 0.01 -t stopped -C wibble2 \; \
    kill-session -t stopped || fail "destroy stopped session failed"
$TMUX has-session -t stopped 2>/dev/null && fail "stopped session still exists"
kill -0 "$pid" || fail "stopped client disappeared"
touch "$DIR/go"
late_error wibble2
late_error wibble3

exit 0
