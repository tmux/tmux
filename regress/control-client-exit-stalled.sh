#!/bin/sh

# A control client whose terminal has stopped accepting output (for example
# its ssh connection or terminal emulator died) never drains the output that
# server_client_check_exit waits on before sending MSG_EXIT, and may then
# never close its socket either. This must not prevent the server from
# exiting: after kill-server the server previously stayed alive forever with
# server_exit set, closing every new connection immediately, so every new
# client failed with "server exited unexpectedly" until the server was killed
# by hand. The server now bounds the exit handshake: it discards output it
# can never deliver and drops the client if it still does not close.
#
# The stalled terminal is simulated by a fifo whose read end is held open but
# never read.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

DIR=$(mktemp -d)
FIFO=$DIR/fifo
SERVER=

mkfifo "$FIFO" || exit 1

cleanup() {
	[ -n "$SERVER" ] && kill -9 "$SERVER" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	exec 8<&- 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

# A detached session whose pane floods printable output forever.
$TMUX -f/dev/null new -d -x 80 -y 24 -s rt 'cat /dev/zero | tr "\000" x' || exit 1
SERVER=$($TMUX display -pt rt '#{pid}')

# Attach a control client with output down the fifo; stdin held open by sleep
# so it stays attached. The fifo fills and is never read, so a backlog forms
# in the server that can never be delivered.
( sleep 60 ) | $TMUX -f/dev/null -C attach -t rt >"$FIFO" 2>&1 &
exec 8<"$FIFO"

n=0
while [ $n -lt 50 ]; do
	$TMUX lsc -F '#{client_name}' 2>/dev/null | grep -q . && break
	sleep 0.1
	n=$((n + 1))
done
$TMUX lsc -F '#{client_name}' 2>/dev/null | grep -q . ||
	{ echo "control client did not attach"; exit 1; }

# Let the pane flood fill the fifo so the client's output is stuck.
sleep 2

# Ask the server to exit. This sets CLIENT_EXIT on the stalled client, whose
# output can never drain.
$TMUX kill-server 2>/dev/null

# The server must exit within the exit timeout (10 seconds) plus slack,
# rather than staying in limbo forever rejecting new clients.
n=0
while [ $n -lt 100 ]; do
	if ! kill -0 "$SERVER" 2>/dev/null; then
		SERVER=
		exit 0
	fi
	sleep 0.2
	n=$((n + 1))
done

echo "server did not exit after kill-server; new clients see:"
$TMUX ls 2>&1
exit 1
