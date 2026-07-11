#!/bin/sh

# Zero-block watchdog. A pane keeps producing while its sole control client is
# wedged (stdout blocked). With control-resync on, the watchdog resyncs the pane
# so it keeps running; a frozen pane would not advance its counter at all.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

MARK="control_resync_recover_producer_$$"
CTR=$(mktemp)
FIFO=$(mktemp -u)
trap 'kill $CPID 2>/dev/null; exec 7<&- 2>/dev/null; pkill -f "$MARK" 2>/dev/null; $TMUX kill-server 2>/dev/null; rm -f "$CTR" "$CTR.w" "$FIFO"' 0 1 15

echo 0 >"$CTR" || exit 1
mkfifo "$FIFO" || exit 1

# Pane producer: bump a counter file (written atomically via rename so a reader
# never sees a truncated value) and blast 256k of output each iteration. The
# unique MARK lives in the command line so pkill can find the loop.
CMD="i=0; while :; do i=\$((i+1)); echo \$i >$CTR.w; mv $CTR.w $CTR; dd if=/dev/zero bs=262144 count=1 2>/dev/null | tr '\\0' x; done # $MARK"
$TMUX -f/dev/null new -d -x80 -y24 "$CMD" || exit 1
$TMUX set -g control-resync on || exit 1
sleep 1

# Hold the FIFO read end open on fd 7 but never read it (opening read+write
# never blocks and needs no helper process), then attach the sole control client
# with its stdout going into that FIFO. Once the pipe fills the client's writes
# block, it makes no drain progress and stalls.
exec 7<>"$FIFO" || exit 1
$TMUX -C attach >"$FIFO" 2>/dev/null &
CPID=$!
sleep 1

START=$(cat "$CTR") || exit 1
sleep 12
END=$(cat "$CTR") || exit 1

kill $CPID 2>/dev/null
exec 7<&-
pkill -f "$MARK" 2>/dev/null
$TMUX kill-server 2>/dev/null

# A frozen pane advances by 0; the resync watchdog keeps it running (baseline
# observed well over a thousand in this window). Require a conservative floor.
case "$START" in ''|*[!0-9]*) exit 1;; esac
case "$END" in ''|*[!0-9]*) exit 1;; esac
[ "$((END - START))" -ge 20 ] || exit 1

exit 0
