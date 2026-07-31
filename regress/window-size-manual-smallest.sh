#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lmanualsmallest$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
FIFO="$TMP.fifo"
mkfifo "$FIFO"
trap '$TMUX kill-server 2>/dev/null; rm -f "$TMP" "$FIFO"' 0 1 15

fail()
{
	echo "$1"
	exit 1
}

wait_size()
{
	expected=$1
	n=0
	while [ $n -lt 50 ]; do
		actual=$($TMUX display -t test: -p \
		    '#{window_width}x#{window_height}' 2>/dev/null)
		[ "$actual" = "$expected" ] && return 0
		sleep 0.1
		n=$((n + 1))
	done
	fail "expected size $expected, got $actual"
}

$TMUX new-session -d -s test -x 100 -y 40 || exit 1
$TMUX resize-window -t test: -x 100 -y 40 || exit 1
$TMUX set-option -w -t test: window-size manual-or-smallest || exit 1

manual=$($TMUX display -t test: -p \
    '#{window_manual_width}x#{window_manual_height}')
[ "$manual" = "100x40" ] || fail "unexpected manual size: $manual"

$TMUX -C attach -t test <"$FIFO" >"$TMP" 2>&1 &
client_pid=$!
exec 3>"$FIFO"
echo 'refresh-client -C 80,24' >&3
wait_size 80x24

# A manual resize keeps the mode and changes the size cap.
$TMUX resize-window -t test: -x 70 -y 20 || exit 1
mode=$($TMUX show-option -wv -t test: window-size)
[ "$mode" = "manual-or-smallest" ] || fail "unexpected mode: $mode"
wait_size 70x20

# A larger manual size remains capped by the attached client.
$TMUX resize-window -t test: -x 100 -y 40 || exit 1
wait_size 80x24

# With no attached client, the window returns to its manual size.
client=$($TMUX list-clients -F '#{client_name}')
[ -n "$client" ] || fail "control client did not attach"
exec 3>&-
wait "$client_pid" 2>/dev/null || true
wait_size 100x40

exit 0
