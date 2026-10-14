#!/bin/sh

# Insert and delete a line behind a floating pane. The affected SIXEL
# image must be sent again even when the pane is obscured.

. ./image-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}
fail()
{
	echo "$*" >&2
	cleanup
	exit 1
}

cleanup

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

# Hold the pane after its image until capture is ready, then trigger the
# insert/delete-line operation explicitly instead of racing a fixed sleep.
SIXEL='\033Pq"1;1;26;26#0;2;100;100;100#0!26~-!26~-!26~-!26~-!26B\033\\'
TRIGGER='\033[H\033[L\033[M'
$TMUX new-session -d -s inner -x 40 -y 15 \
    "printf '$SIXEL'; $TMUX wait-for image-ops-$$; printf '$TRIGGER'; sleep 100" || exit 1
sleep 0.5
BASE=$($TMUX list-panes -F '#{pane_id}' | head -1)
[ -n "$BASE" ] || fail "No base pane."

image_check_support $TMUX
$TMUX set -as terminal-features ',*:sixel' || exit 1

# Float a small pane so it overlaps the top rows of the base pane, where
# the image sits, making the base pane obscured.
$TMUX new-pane -d -x 12 -y 4 -X 5 -Y 0 || fail "new-pane -X -Y failed"

$TMUX2 new-session -d -x 40 -y 15 "$TMUX attach -t inner" || exit 1
sleep 0.5
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX refresh-client || fail "initial refresh failed"
sleep 1
grep -aq '"1;1;26;26' $TMP || fail "initial image was not sent"
: >$TMP
$TMUX wait-for -S image-ops-$$ || fail "trigger failed"
sleep 1

grep -a -q '"1;1;26;26' $TMP ||
	fail "image was not retransmitted after insert/delete-line on an obscured pane"

exit 0
