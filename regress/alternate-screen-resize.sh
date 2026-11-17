#!/bin/sh

# A pending pane resize must still reach the pty when the application enters
# the alternate screen before the resize timer fires.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"

TMP=$(mktemp)
TMP2=$(mktemp)
trap 'rm -f "$TMP" "$TMP2"; $TMUX kill-server 2>/dev/null' 0 1 15

$TMUX new -d -x 80 -y 24 \
	"read x; printf '\033[?1049h'; read x; stty size >$TMP; sleep 100" \
	</dev/null || exit 1
sleep 1

# The first split resizes and starts the resize timer, the second queues
# another resize while the timer is pending.
$TMUX splitw -h -d 'sleep 100' || exit 1
$TMUX splitw -v -d -t:0.0 'sleep 100' || exit 1
$TMUX send -t:0.0 Enter || exit 1
sleep 1

$TMUX send -t:0.0 Enter || exit 1
sleep 1
$TMUX display -p -t:0.0 '#{pane_height} #{pane_width}' >$TMP2
cmp -s "$TMP" "$TMP2" || exit 1

$TMUX kill-server 2>/dev/null
exit 0
