#!/bin/sh

# source-file must give its nesting depth back to whatever it took it from.
# A command item with no client of its own is lent one while it runs, so the
# depth was taken from that client but given back to the global counter by the
# completion callback, which runs afterwards. Every source-file from a hook
# therefore left the client one deeper than it found it and after 50 of them
# the client could not source anything at all.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp -d) || exit 1
CONF="$TMP/tmux.conf"
LEAF="$TMP/leaf.conf"
FIFO="$TMP/fifo"
OUT="$TMP/out"
COUNT="$TMP/count"
mkfifo "$FIFO" || exit 1
: >"$COUNT"
trap "$TMUX kill-server 2>/dev/null; rm -rf $TMP" 0 1 15

echo "run-shell -b 'printf . >>$COUNT'" >"$LEAF"
echo "set-hook -g session-window-changed 'source-file $LEAF'" >"$CONF"

$TMUX -f"$CONF" new-session -d -x80 -y24 'sleep 1000' || exit 1
$TMUX new-window -d 'sleep 1000' || exit 1

# The depth is lost on the client the hook borrows, so one client has to fire
# the hook more times than the nesting limit for it to run out.
exec 3<>"$FIFO"
$TMUX -C attach <&3 >"$OUT" 2>&1 &
CLIENT=$!
sleep 1

i=0
while [ $i -lt 60 ]; do
	printf 'next-window\n' >&3
	sleep 0.05
	i=$((i + 1))
done
sleep 1

kill $CLIENT 2>/dev/null
wait $CLIENT 2>/dev/null
exec 3>&-

grep -q 'too many nested files' "$OUT" && exit 1
[ "$(($(wc -c <"$COUNT")))" -eq 60 ] || exit 1

exit 0
