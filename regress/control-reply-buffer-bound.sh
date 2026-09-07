#!/bin/sh

# A control client that sends commands producing large replies faster than it
# reads them used to make the server buffer the whole backlog with no limit,
# because command replies bypass the %output accounting. The server now drops
# the client with "too far behind" once the buffered replies exceed the limit,
# instead of growing until it is killed. The client must be dropped while its
# stdin is still open and the server must survive.
#
# The unread client is simulated by a fifo whose read end is held open but
# never read.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

DIR=$(mktemp -d)
FIFO=$DIR/fifo

mkfifo "$FIFO" || exit 1

cleanup() {
	$TMUX kill-server 2>/dev/null
	exec 8<&- 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

# A detached session whose pane has a full history of long lines, so each
# capture-pane reply is large.
$TMUX -f/dev/null new -d -x 80 -y 24 -s rt \
	'yes "$(printf %079d 0)" | head -n 2000; sleep 300' || exit 1
sleep 1

# Attach a control client with output down the fifo. Once it is attached,
# send it many capture-pane commands, then hold its stdin open so it does not
# exit on its own.
(
	while [ ! -f "$DIR/go" ]; do
		sleep 0.1
	done
	n=0
	while [ $n -lt 1000 ]; do
		echo 'capture-pane -p -S -'
		n=$((n + 1))
	done
	sleep 60
) | $TMUX -f/dev/null -C attach -t rt >"$FIFO" 2>&1 &
CLIENT=$!
exec 8<"$FIFO"

n=0
while [ $n -lt 50 ]; do
	$TMUX lsc -F '#{client_name}' 2>/dev/null | grep -q . && break
	sleep 0.1
	n=$((n + 1))
done
$TMUX lsc -F '#{client_name}' 2>/dev/null | grep -q . ||
	{ echo "control client did not attach"; exit 1; }
touch "$DIR/go"

# The server must drop the client itself once the replies it cannot deliver
# pass the limit, within the exit timeout (10 seconds) plus slack.
n=0
while [ $n -lt 150 ]; do
	kill -0 "$CLIENT" 2>/dev/null || break
	sleep 0.2
	n=$((n + 1))
done
kill -0 "$CLIENT" 2>/dev/null && {
	echo "control client was not dropped"
	exit 1
}

$TMUX has-session -t rt 2>/dev/null || {
	echo "server exited"
	exit 1
}

exit 0
