#!/bin/sh

# A control client with the wait-exit flag lingers after %exit until its input
# sends an empty line or closes. The terminating empty line must be honoured
# even when several lines arrive in a single read (as from a pipe or a raw
# terminal): reading input with stdio would pull them all into the FILE buffer
# and leave the empty line where poll never sees it, hanging the client.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

FIFO=$(mktemp -u)
OUT=$(mktemp)
mkfifo "$FIFO" || exit 1
trap "$TMUX kill-server 2>/dev/null; rm -f $FIFO $OUT" 0 1 15

# Start a control client that reads its input from the fifo. Keep the write
# end open (fd 3) so the client never sees EOF: it must exit on the empty
# line, not on the pipe closing.
$TMUX -f/dev/null -C new -s wait-exit <"$FIFO" >"$OUT" 2>&1 &
CLIENT=$!
exec 3>"$FIFO"
sleep 1

# Ask to linger after exit, then detach so the client prints %exit and enters
# the wait-exit loop.
printf 'refresh-client -f wait-exit\n' >&3
sleep 1
$TMUX detach-client -s wait-exit

# Wait for the client to print %exit and enter the wait-exit loop.
i=0
while [ $i -lt 5 ]; do
	grep -q '^%exit' "$OUT" && break
	sleep 1
	i=$((i + 1))
done
grep -q '^%exit' "$OUT" || exit 1

# Deliver the terminating empty line in one write, after a non-empty line, so
# a single read returns "a\n" and "\n" together. The empty line must still end
# the wait.
printf 'a\n\n' >&3

# The client should exit promptly. If it is still alive after the timeout the
# empty line was lost in a buffer and the client has hung.
i=0
while [ $i -lt 10 ]; do
	kill -0 $CLIENT 2>/dev/null || break
	sleep 1
	i=$((i + 1))
done
if kill -0 $CLIENT 2>/dev/null; then
	kill $CLIENT 2>/dev/null
	exit 1
fi
exec 3>&-

exit 0
