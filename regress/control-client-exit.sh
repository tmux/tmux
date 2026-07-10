#!/bin/sh

# Once a control client is marked for exit (by detach-client here, or by the
# "too far behind" eviction in control_check_age), the server must stop
# queueing new output and notifications for it. The exit handshake only waits
# for already-queued blocks to drain (control_all_done), so anything queued
# after that point is written to a client that will never read it, and can hold
# the handshake - and the pane buffer it pins - open. This checks that a
# notification generated while the client is exiting never reaches its output.
#
# To see this in seconds rather than waiting out CONTROL_MAXIMUM_AGE (300s) the
# control client's output goes down a fifo that a helper drains only slowly.
# The slow drain keeps a backlog stuck in the server so the exit handshake
# cannot complete; while it is stuck we detach the client (which sets the exit
# flag) and rename the window to a marker. Draining the fifo must then not turn
# up the marker as a %window-renamed line.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

DIR=$(mktemp -d)
FIFO=$DIR/fifo
OUT=$DIR/out
MARKER=exitmarker
READER=

mkfifo "$FIFO" || exit 1
: >"$OUT"

cleanup() {
	[ -n "$READER" ] && kill "$READER" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	exec 8<&- 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

# A detached session whose pane floods printable output forever.
$TMUX -f/dev/null new -d -x 80 -y 24 -s rt 'cat /dev/zero | tr "\000" x' || exit 1
$TMUX setw -g automatic-rename off || exit 1

# Attach a control client: output to the fifo, stdin held open by sleep so it
# stays attached while we drive the session from outside. Open the read end
# after the client has opened the write end.
( sleep 30 ) | $TMUX -f/dev/null -C attach -t rt >"$FIFO" 2>&1 &
exec 8<"$FIFO"

n=0
while [ $n -lt 50 ]; do
	$TMUX lsc -F '#{client_name}' 2>/dev/null | grep -q . && break
	sleep 0.1
	n=$((n + 1))
done
$TMUX lsc -F '#{client_name}' 2>/dev/null | grep -q . ||
	{ echo "control client did not attach"; exit 1; }

# Drain the control stream slowly (~2.5KB/s). The pane floods far faster, so a
# backlog stays stuck in the server and the fifo stays full, which is what
# keeps the exit handshake from completing below.
( while :; do
	dd bs=256 count=1 <&8 >>"$OUT" 2>/dev/null || exit 0
	sleep 0.1
done ) &
READER=$!

# Wait until the client is demonstrably backed up: the output only keeps
# growing at the slow-drain rate while the flood is outrunning the reader.
n=0
while [ $n -lt 100 ]; do
	[ "$(wc -c <"$OUT")" -ge 8000 ] && break
	sleep 0.1
	n=$((n + 1))
done
[ "$(wc -c <"$OUT")" -ge 8000 ] || { echo "client output did not back up"; exit 1; }

# Detach the control client: this sets CLIENT_EXIT immediately. The stuck
# backlog holds the window between CLIENT_EXIT and the exit handshake open.
$TMUX detach-client -s rt

# While the client is exiting, rename the window to the marker. Vanilla queues
# these %window-renamed notifications for the exiting client; the fix drops them.
i=0
while [ $i -lt 5 ]; do
	$TMUX rename-window -t rt:0 "$MARKER$i"
	i=$((i + 1))
done
sleep 0.5

# Drain fast (bounded) so the backlog, and any notification queued behind it,
# reaches OUT. A watchdog keeps this from blocking.
kill "$READER" 2>/dev/null
READER=
( dd bs=4096 count=128 <&8 >>"$OUT" 2>/dev/null ) &
DRAIN=$!
( sleep 3; kill "$DRAIN" 2>/dev/null ) &
WATCHDOG=$!
wait "$DRAIN" 2>/dev/null
kill "$WATCHDOG" 2>/dev/null

# The marker must never appear as a notification to the exiting client.
if grep -q "%window-renamed .*$MARKER" "$OUT"; then
	echo "notification queued to exiting control client:"
	grep "%window-renamed .*$MARKER" "$OUT"
	exit 1
fi

exit 0
