#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMPDIR=$(mktemp -d)
IN1="$TMPDIR/in1"
IN2="$TMPDIR/in2"
OUT1="$TMPDIR/out1"
OUT2="$TMPDIR/out2"
PID1=
PID2=

cleanup()
{
	[ -n "$PID1" ] && kill "$PID1" 2>/dev/null
	[ -n "$PID2" ] && kill "$PID2" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMPDIR"
}
trap cleanup EXIT

fail()
{
	echo "$1"
	echo "client1:"
	cat "$OUT1" 2>/dev/null
	echo "client2:"
	cat "$OUT2" 2>/dev/null
	exit 1
}

wait_for()
{
	file=$1
	pattern=$2
	i=0

	while [ "$i" -lt 60 ]; do
		if grep -F -- "$pattern" "$file" >/dev/null 2>&1; then
			return 0
		fi
		sleep 0.1
		i=$((i + 1))
	done
	fail "missing '$pattern' in $file"
}

send1()
{
	printf '%s\n' "$*" >&3
}

send2()
{
	printf '%s\n' "$*" >&4
}

check_server()
{
	out=$($TMUX display-message -p -t "$1" "$2")
	[ "$out" = "$3" ] || fail "expected '$3' for $1 $2, got '$out'"
}

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -s aw -x 80 -y 24 -n w0 || exit 1
$TMUX new-window -d -t aw: -n w1 || exit 1
$TMUX new-window -d -t aw: -n w2 || exit 1
$TMUX new-session -d -s aw2 -x 80 -y 24 -n x0 || exit 1
$TMUX new-window -d -t aw2: -n x1 || exit 1

mkfifo "$IN1" "$IN2" || exit 1
: >"$OUT1"
: >"$OUT2"
$TMUX -C attach-session -t aw <"$IN1" >"$OUT1" 2>&1 &
PID1=$!
$TMUX -C attach-session -t aw <"$IN2" >"$OUT2" 2>&1 &
PID2=$!
exec 3>"$IN1"
exec 4>"$IN2"

send1 'display-message -p C1READY'
send2 'display-message -p C2READY'
wait_for "$OUT1" C1READY
wait_for "$OUT2" C2READY

# Default shared selection: client 1 changes session current, client 2 follows.
send1 'display-message -p "C1A #{active_window_index} #{local_active_window} #{window_index}"'
wait_for "$OUT1" 'C1A 0 0 0'
send1 'select-window -t :1'
wait_for "$OUT2" '%session-window-changed $0 @1'
send2 'display-message -p "C2A #{active_window_index} #{local_active_window} #{window_index}"'
wait_for "$OUT2" 'C2A 1 0 1'
check_server 'aw:' '#{window_index}' '1'

# Local selection changes only client 1 and becomes the default command target.
send1 'select-window -L'
send1 'select-window -t :2'
send1 'display-message -p "C1B #{active_window_index} #{local_active_window} #{window_index}"'
wait_for "$OUT1" 'C1B 2 1 2'
send2 'display-message -p "C2B #{active_window_index} #{local_active_window} #{window_index}"'
wait_for "$OUT2" 'C2B 1 0 1'
check_server 'aw:' '#{window_index}' '1'

# Local last-window history is independent of the session shared stack.
send1 'last-window'
send1 'display-message -p "C1C #{active_window_index} #{window_index}"'
wait_for "$OUT1" 'C1C 1 1'
send1 'next-window'
send1 'display-message -p "C1D #{active_window_index} #{window_index}"'
wait_for "$OUT1" 'C1D 2 2'
check_server 'aw:' '#{window_index}' '1'

# Session switches preserve per-session local selections.
send1 'switch-client -t aw2'
send1 'select-window -L'
send1 'select-window -t :1'
send1 'display-message -p "C1E #{session_name} #{active_window_index} #{local_active_window}"'
wait_for "$OUT1" 'C1E aw2 1 1'
send1 'switch-client -t aw'
send1 'display-message -p "C1F #{session_name} #{active_window_index} #{local_active_window}"'
wait_for "$OUT1" 'C1F aw 2 1'

# Returning to shared mode immediately follows the session current window.
send1 'select-window -S'
send1 'display-message -p "C1G #{active_window_index} #{local_active_window} #{window_index}"'
wait_for "$OUT1" 'C1G 1 0 1'

# Invalid local windows recover to a valid session window without changing curw.
send1 'select-window -L'
send1 'select-window -t :2'
$TMUX kill-window -t aw:2 || fail 'kill-window failed'
send1 'display-message -p "C1H #{active_window_index} #{local_active_window} #{window_index}"'
wait_for "$OUT1" 'C1H 1 1 1'
check_server 'aw:' '#{window_index}' '1'

# Creating a window without -d in local mode selects it locally only.
send1 'new-window -n w3'
send1 'display-message -p "C1I #{local_active_window} #{window_name}"'
wait_for "$OUT1" 'C1I 1 w3'
send2 'display-message -p "C2C #{local_active_window} #{window_name}"'
wait_for "$OUT2" 'C2C 0 w1'
check_server 'aw:' '#{window_name}' 'w1'

exit 0
