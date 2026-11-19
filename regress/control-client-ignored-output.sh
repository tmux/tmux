#!/bin/sh

# Output for a control client that is ignoring a pane (here because it is
# suspended) while blocks are still queued must not crash the server. The
# ignore path moved the offsets past the queued data and writing the blocks
# later hit fatalx("not enough data").
#
# The client's stdout is a fifo that is held open but not read until after
# the suspend, so output is queued in blocks.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$"
$TMUX kill-server 2>/dev/null

DIR=$(mktemp -d)
FIFO=$DIR/fifo
IN=$DIR/in
SERVER=
READER=
WRITER=

mkfifo "$FIFO" "$IN" || exit 1

cleanup() {
	[ -n "$READER" ] && kill "$READER" 2>/dev/null
	[ -n "$WRITER" ] && kill "$WRITER" 2>/dev/null
	[ -n "$SERVER" ] && kill -9 "$SERVER" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	exec 8<&- 9>&- 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

# A detached session whose pane copies whatever is written to $IN.
$TMUX -f/dev/null new -d -x 80 -y 24 -s rt "cat $IN" || exit 1
exec 9>"$IN"
SERVER=$($TMUX display -pt rt '#{pid}')

# Attach a control client with output down the fifo, which is held open but
# not read. Stdin is held open by sleep so it stays attached.
( sleep 60 ) | $TMUX -f/dev/null -C attach -t rt >"$FIFO" 2>&1 &
exec 8<"$FIFO"

n=0
while [ $n -lt 50 ]; do
	NAME=$($TMUX lsc -F '#{client_name}' 2>/dev/null)
	[ -n "$NAME" ] && break
	sleep 0.1
	n=$((n + 1))
done
[ -n "$NAME" ] || { echo "control client did not attach"; exit 1; }

# Queue more output than the fifo can hold (control characters are written as
# four bytes each) and suspend the client. Once the writer is done, produce a
# little more output, which takes the ignore path while blocks are queued.
yes | tr 'y\n' '\001\001' | head -c 100000 >&9 &
WRITER=$!
sleep 1
$TMUX suspend-client -t "$NAME" || exit 1
n=0
while kill -0 $WRITER 2>/dev/null; do
	[ $n -lt 50 ] || { echo "pane output stalled"; exit 1; }
	sleep 0.1
	n=$((n + 1))
done
echo y >&9
sleep 1

# Now read the fifo, so the server writes the queued blocks.
cat <&8 >/dev/null &
READER=$!
sleep 2

kill -0 "$SERVER" 2>/dev/null || { SERVER=; echo "server crashed"; exit 1; }
$TMUX has -t rt 2>/dev/null || { echo "server not responding"; exit 1; }
exit 0
