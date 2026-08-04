#!/bin/sh

# save-buffer has to report a failed write. The data is handed to the client to
# write and the server used to report success as soon as it had sent all of it
# rather than waiting to hear how the write went, so a failure was silent - and
# with O_TRUNC the previous contents of the destination were already gone.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp -d) || exit 1
PIPE="$TMP/pipe"
DATA="$TMP/data"
OUT="$TMP/out"
mkfifo "$PIPE" || exit 1
trap "$TMUX kill-server 2>/dev/null; rm -rf $TMP" 0 1 15

# The buffer needs to be bigger than the pipe so that the write is still going
# when the reader disappears.
i=0
while [ $i -lt 2000 ]; do
	echo '0123456789012345678901234567890123456789012345678901234567890123'
	i=$((i + 1))
done >"$DATA"

$TMUX new-session -d 'sleep 1000' || exit 1
$TMUX load-buffer -b big "$DATA" || exit 1

# Take a few bytes and go away, leaving save-buffer with a broken pipe.
(head -c 8 <"$PIPE" >/dev/null) &
sleep 1
$TMUX save-buffer -b big "$PIPE" 2>"$OUT" && exit 1
[ -s "$OUT" ] || exit 1
wait

# A write that works must still be reported as working.
$TMUX save-buffer -b big "$TMP/copy" || exit 1
cmp -s "$DATA" "$TMP/copy" || exit 1

exit 0
