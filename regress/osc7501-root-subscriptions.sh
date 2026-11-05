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
FORMAT='#{pane_program_state}|#{pane_program_name}|#{pane_program_message}|#{pane_program_result_unseen}'

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

tree_reorder()
{
	selected=$($TMUX display-message -p -t status: '#{window_id}|#{pane_id}')
	$TMUX new-window -d -t status:1 /bin/sh || exit 1
	for key in J K; do
		$TMUX choose-tree -w -O index -t "$pane" || exit 1
		$TMUX send-keys -t "$pane" "$key" || exit 1
		case $key in J) index=1 ;; K) index=0 ;; esac
		wait_for_output "$index" $TMUX display-message -p -t "$pane" '#{window_index}' || exit 1
		wait_for_output "$selected" $TMUX display-message -p -t status: '#{window_id}|#{pane_id}' || exit 1
		wait_for_output "$state|clnkr|hello|1" $TMUX display-message -p -t "$pane" "$FORMAT" || exit 1
		$TMUX send-keys -t "$pane" q || exit 1
	done
	$TMUX kill-window -t status:1 || exit 1
}

result_acknowledgement()
{
	other=$($TMUX split-window -d -P -F '#{pane_id}' -t "$pane" /bin/sh) || exit 1
	for state in done error; do
		: >"$OUT"
		emit "state=$state:app=clnkr:msg=aGVsbG8=" "$state|clnkr|hello|1"
		changed "$state|clnkr|hello|1"
		tree_reorder
		# Redraw, input and selecting the current pane/window/session are no-ops.
		send 'refresh-client'
		send "select-pane -t $pane"
		send 'select-window -t status:0'
		send 'switch-client -t status'
		send 'display-message -p noops-complete'
		wait_for_text noops-complete cat "$OUT" || exit 1
		wait_for_output "$state|clnkr|hello|1" $TMUX display-message -p -t "$pane" "$FORMAT" || exit 1
		: >"$OUT"
		$TMUX select-pane -t "$other" || exit 1
		$TMUX select-pane -t "$pane" || exit 1
		changed "$state|clnkr|hello|0"
		: >"$OUT"
		emit "state=$state:app=clnkr:msg=aGVsbG8=" "$state|clnkr|hello|1"
		changed "$state|clnkr|hello|1"
		# A second client's attach acknowledges the pane for both clients.
		$TMUX -C attach-session -t status </dev/null >"$TMPDIR/second" 2>&1 || exit 1
		changed "$state|clnkr|hello|0"
		: >"$OUT"
		emit "state=$state:app=clnkr:msg=aGVsbG8=" "$state|clnkr|hello|1"
		changed "$state|clnkr|hello|1"
		# Session changes acknowledge even through a link to the same pane.
		$TMUX new-session -d -s linked -t status || exit 1
		emit "state=$state:app=clnkr:msg=aGVsbG8=" "$state|clnkr|hello|1"
		: >"$OUT"
		status_prefix=$prefix
		linked_fields=$($TMUX display-message -p -t linked: '#{session_id} #{window_id} #{window_index} #{pane_id}')
		prefix="%subscription-changed status $linked_fields : "
		send 'switch-client -t linked'
		send 'display-message -p switched'
		wait_for_text switched cat "$OUT" || exit 1
		changed "$state|clnkr|hello|0"
		send 'switch-client -t status'
		send 'display-message -p returned'
		wait_for_text returned cat "$OUT" || exit 1
		prefix=$status_prefix
		$TMUX kill-session -t linked || exit 1
	done
}

$TMUX new-session -d -s status -x 80 -y 24 /bin/sh || exit 1
pane=$($TMUX display-message -p -t status '#{pane_id}')
fields=$($TMUX display-message -p -t status '#{session_id} #{window_id} #{window_index} #{pane_id}')
prefix="%subscription-changed status $fields : "
mkfifo "$IN" || exit 1
attach
changed '|||0'
result_acknowledgement
emit 'state=working:app=clnkr:msg=aGVsbG8=' 'working|clnkr|hello|0'
changed 'working|clnkr|hello|0'
# Discard previous notifications so the initial empty snapshot cannot pass.
: >"$OUT"
emit 'state=done:app=clnkr' 'done|clnkr||1'
changed 'done|clnkr||1'
emit 'state=clear' '|||0'
changed '|||0'
emit 'state=blocked:app=clnkr' 'blocked|clnkr||0'
changed 'blocked|clnkr||0'
detach
wait_for_output 'blocked|clnkr||0' $TMUX display-message -p -t "$pane" "$FORMAT" || exit 1
attach
changed 'blocked|clnkr||0'
detach
echo 'PASS: subscriptions'
