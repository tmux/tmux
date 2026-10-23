#!/bin/sh

# A waited run-shell command can complete after a control client has closed its
# input and exited. The command queue still references the client until the job
# callback runs, but control_stop has already destroyed c->control_state. Late
# command output must be discarded instead of dereferencing the stopped control
# state and killing the server.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$"
$TMUX kill-server 2>/dev/null

OUT=$(mktemp)
FIFO=$(mktemp -u)
GO=$(mktemp -u)
STARTED=$(mktemp -u)
pid=
cleanup()
{
	touch $GO
	exec 3>&-
	if [ -n "$pid" ]; then
		kill -CONT $pid 2>/dev/null
		kill $pid 2>/dev/null
		wait $pid 2>/dev/null
	fi
	$TMUX kill-server 2>/dev/null
	rm -f $OUT $FIFO $GO $STARTED
}
trap cleanup 0
trap 'exit 1' 1 2 15

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

# A background command can fail to parse after its client has no session:
# the client has exited, or its session was destroyed while it was stopped.
# The error must not be shown on that client, only kept in the message log.
exit_status=0

# Wait until the error for command $1 is in the message log. If the server
# has gone instead, report it and start another.
late_error()
{
	i=0
	while [ $i -lt 100 ]; do
		if ! $TMUX has-session -t main 2>/dev/null; then
			echo "server exited after late error from: $2"
			exit_status=1
			$TMUX -f/dev/null new-session -d -s main || exit 1
			return
		fi
		$TMUX show-messages | grep -q "Unknown command: $1" && return
		sleep 0.05
		i=$((i + 1))
	done
	echo "no late error from: $2"
	exit_status=1
}

printf '%s\n' "if-shell -b 'sleep 0.2' wibble1" | $TMUX -f/dev/null -C >"$OUT" 2>&1
late_error wibble1 "if-shell -b"
printf '%s\n' "run-shell -b -d 0.2 -C wibble2" | $TMUX -f/dev/null -C >"$OUT" 2>&1
late_error wibble2 "run-shell -b -C"

# A stopped control client whose session is destroyed stays, with no session.
$TMUX new-session -d -s stopped || exit 1
mkfifo $FIFO || exit 1
$TMUX -f/dev/null -C attach -t stopped <$FIFO >"$OUT" 2>&1 &
pid=$!
exec 3>$FIFO
echo "if-shell -b 'touch $STARTED; while [ ! -e $GO ]; do sleep 0.05; done' wibble3" >&3
i=0
while [ ! -e $STARTED ]; do
	i=$((i + 1))
	[ $i -lt 100 ] || { echo "if-shell -b did not start"; exit 1; }
	sleep 0.05
done
kill -STOP $pid || exit 1
$TMUX kill-session -t stopped || exit 1
if $TMUX has-session -t stopped 2>/dev/null || ! kill -0 $pid; then
	echo "session not destroyed or stopped client gone"
	exit 1
fi
touch $GO
late_error wibble3 "a stopped client whose session was destroyed"

exit $exit_status
