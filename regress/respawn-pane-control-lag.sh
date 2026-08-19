#!/bin/sh

# respawn-pane frees the pane's buffer and creates an empty one. The pane
# offsets and each control client's offsets used to keep pointing into the old
# buffer, so if an attached control client had not caught up on the pane's
# output, the next read from the new process ran off the end of the new buffer
# and the server crashed in input_parse.
#
# Two ways for a client to be behind are tested: a control client whose output
# goes down a fifo that is never read (a second, healthy control client keeps
# the pane being read), and a healthy control client viewing a session that
# the pane's window has been moved out of.

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

alive() {
	if ! kill -0 "$SERVER" 2>/dev/null; then
		SERVER=
		echo "server died after $1"
		exit 1
	fi
	$TMUX has -t rt || exit 1
}

wait_clients() {
	n=0
	while [ $n -lt 50 ]; do
		[ "$($TMUX lsc 2>/dev/null | wc -l)" -ge $1 ] && return
		sleep 0.1
		n=$((n + 1))
	done
	echo "control clients did not attach"; exit 1
}

# A detached session with a pane that writes a line every 10 milliseconds.
$TMUX -f/dev/null new -d -x 80 -y 24 -s rt \
	'while :; do echo xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; sleep 0.01; done' ||
	exit 1
SERVER=$($TMUX display -pt rt '#{pid}')

# One control client with output down the unread fifo and one reading
# normally; stdin of both held open by sleep so they stay attached.
( sleep 60 ) | $TMUX -f/dev/null -C attach -t rt >"$FIFO" 2>&1 &
exec 8<"$FIFO"
( sleep 60 ) | $TMUX -f/dev/null -C attach -t rt >/dev/null 2>&1 &
wait_clients 2

# Let the first client fall behind, then respawn the pane. The new process
# writes at once and the server must survive reading it.
sleep 3
$TMUX respawn-pane -k -t rt:0 'echo respawned; sleep 60' || exit 1
sleep 1
alive "respawn-pane with a lagging control client"

# Now a pane whose window is moved out of the session both clients view: their
# offsets for it stop advancing, so they fall behind until it is respawned.
$TMUX neww -d -t rt \
	'while :; do echo yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy; sleep 0.01; done' ||
	exit 1
sleep 1
$TMUX new -d -s other || exit 1
$TMUX movew -d -s rt:1 -t other: || exit 1
sleep 2
$TMUX respawn-pane -k -t other:1 'echo respawned; sleep 60' || exit 1
sleep 1
alive "respawn-pane on a window moved out of the clients' session"

exit 0
