#!/bin/sh

# A new pane has to start in the directory it was given even if the server's
# own working directory has been removed. The chdir was guarded by getcwd,
# which fails once there is nothing to come back to, so -c was ignored and the
# child inherited the removed directory instead.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp -d) || exit 1
DOOMED="$TMP/doomed"
TARGET="$TMP/target"
OUT="$TMP/out"
mkdir "$DOOMED" "$TARGET" || exit 1
trap "$TMUX kill-server 2>/dev/null; rm -rf $TMP" 0 1 15

WANT=$(cd "$TARGET" && pwd -P) || exit 1

# Start the server from a directory that is then taken away.
cd "$DOOMED" || exit 1
$TMUX new-session -d 'sleep 1000' || exit 1
cd "$TMP" || exit 1
rmdir "$DOOMED" || exit 1

$TMUX split-window -d -c "$TARGET" "pwd -P >$OUT; sleep 1000" || exit 1

i=0
while [ ! -s "$OUT" ] && [ $i -lt 50 ]; do
	sleep 0.1
	i=$((i + 1))
done

[ "$(cat "$OUT")" = "$WANT" ] || exit 1

exit 0
