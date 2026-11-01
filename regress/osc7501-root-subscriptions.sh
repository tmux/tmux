#!/bin/sh

. ./regress-common.inc
PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMPDIR=$(test_tmpdir)
IN="$TMPDIR/in"
OUT="$TMPDIR/out"
PID=
FORMAT='#{pane_program_state}|#{pane_program_name}|#{pane_program_message}'

cleanup()
{
	result=$?
	trap - 0 1 15
	if [ -n "$PID" ]; then
		kill "$PID" 2>/dev/null
		wait "$PID" 2>/dev/null
	fi
	stop_server $TMUX || result=1
	rm -rf "$TMPDIR"
	exit "$result"
}
trap cleanup 0 1 15

send()
{
	printf '%s\n' "$*" >&3
}
attach()
{
	: >"$OUT"
	$TMUX -C attach-session -t status <"$IN" >"$OUT" 2>&1 &
	PID=$!
	exec 3>"$IN"
	send 'display-message -p ready'
	wait_for_text ready cat "$OUT" || exit 1
	send "refresh-client -B 'status:$pane:$FORMAT'"
}
detach()
{
	send detach-client
	exec 3>&-
	wait "$PID" || exit 1
	PID=
}
emit()
{
	$TMUX send-keys -t "$pane" -l "printf '\033]7501;$1\007'" || exit 1
	$TMUX send-keys -t "$pane" Enter || exit 1
	wait_for_output "$2" $TMUX display-message -p -t "$pane" "$FORMAT" || exit 1
}
changed()
{
	wait_for_text "$prefix$1" cat "$OUT" || exit 1
}

$TMUX new-session -d -s status -x 80 -y 24 /bin/sh || exit 1
pane=$($TMUX display-message -p -t status '#{pane_id}')
fields=$($TMUX display-message -p -t status '#{session_id} #{window_id} #{window_index} #{pane_id}')
prefix="%subscription-changed status $fields : "
mkfifo "$IN" || exit 1
attach
changed '||'
emit 'state=working:app=clnkr:msg=aGVsbG8=' 'working|clnkr|hello'
changed 'working|clnkr|hello'
emit 'state=clear' '||'
# Discard previous notifications so the initial empty snapshot cannot pass.
: >"$OUT"
emit 'state=done:app=clnkr' 'done|clnkr|'
changed 'done|clnkr|'
emit 'state=clear' '||'
changed '||'
emit 'state=blocked:app=clnkr' 'blocked|clnkr|'
changed 'blocked|clnkr|'
detach
wait_for_output 'blocked|clnkr|' $TMUX display-message -p -t "$pane" "$FORMAT" || exit 1
attach
changed 'blocked|clnkr|'
detach
echo 'PASS: subscriptions'
