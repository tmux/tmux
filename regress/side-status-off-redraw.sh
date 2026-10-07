#!/bin/sh

# With the side status line off, a full redraw after changing window still
# draws pane border status lines that have not changed.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-off-redraw-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-off-redraw-outer-$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

check()
{
	got=$($TMUX2 capture-pane -p -t outer:0.0 | head -n 1)
	[ "$got" = "$2" ] || fail "$1: top row is '$got', expected '$2'"
}

$TMUX new-session -d -s inner -x 40 -y 10 'sleep 100' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g pane-border-status top || exit 1
$TMUX set -g pane-border-format ' #{pane_index} ' || exit 1
$TMUX split-window -h 'sleep 100' || exit 1
$TMUX new-window 'sleep 100' || exit 1
$TMUX select-window -t:0 || exit 1
$TMUX2 new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

SPLIT='── 0 ───────────────┬── 1 ──────────────'
check attach "$SPLIT"

$TMUX select-window -t:1 || exit 1
sleep 0.5
$TMUX select-window -t:0 || exit 1
sleep 0.5
check "after select-window" "$SPLIT"

exit 0
