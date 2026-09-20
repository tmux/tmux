#!/bin/sh

# Popups require a tty overlay and cannot be displayed by a control client.
# A popup command from control mode must be ignored cleanly, leaving the
# client command queue and server usable.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
DIR=$(mktemp -d) || exit 1
FIFO=$DIR/input
OUT=$DIR/output
RAN=$DIR/popup-ran

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	exec 3>&-
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

mkfifo "$FIFO" || exit 1
$TMUX new-session -d -s control -x40 -y10 'exec sleep 100' || exit 1
$TMUX -C attach-session -t control <"$FIFO" >"$OUT" 2>&1 &
control_pid=$!
exec 3>"$FIFO"

i=0
while [ "$i" -lt 50 ]; do
	$TMUX list-clients -F '#{client_flags}' 2>/dev/null |
	    grep -q 'control-mode' && break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "control client did not attach"

printf '%s\n' "display-popup -E 'touch $RAN'" >&3
printf '%s\n' "display-message -p CONTROL-POPUP-ALIVE" >&3

i=0
while [ "$i" -lt 50 ]; do
	grep -q 'CONTROL-POPUP-ALIVE' "$OUT" 2>/dev/null && break
	if ! kill -0 "$control_pid" 2>/dev/null; then
		fail "control client exited after display-popup"
	fi
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "command after control-client popup did not run"
$TMUX has-session -t control || fail "server exited after control-client popup"
[ ! -e "$RAN" ] || fail "control client started a popup command"

exit 0
