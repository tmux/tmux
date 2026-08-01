#!/bin/sh

# Side status line rendering: geometry on the left and right, coexistence
# with the horizontal status line, width changes and turning it off. Scenes
# are rendered in an inner tmux attached inside an outer tmux pane; the outer
# pane is captured and compared with goldens in side-status-results/.
#
# Run with GENERATE=1 to (re)create the golden files.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
RESULTS=side-status-results

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" \
	0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

compare() {
	sleep 1
	$TMUX capturep -p >$TMP || exit 1
	if [ -n "$GENERATE" ]; then
		cp $TMP "$RESULTS/$1.result" || exit 1
	else
		cmp -s $TMP "$RESULTS/$1.result" ||
		    fail "scene $1 differs from $RESULTS/$1.result"
	fi
}

mkdir -p "$RESULTS"

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
$TMUX new-session -d -x 80 -y 24 || exit 1

# Inner server with fixed pane content so captures are stable.
$TMUX2 new-session -d -s work -n editor "sh -c 'printf EDITOR; exec sleep 100'"
$TMUX2 new-window -n server "sh -c 'printf SERVER; exec sleep 100'"
$TMUX2 new-window -n logs "sh -c 'printf LOGS; exec sleep 100'"
$TMUX2 select-window -t:1
$TMUX2 set -g status off
$TMUX2 set -g status-left "[test]"
$TMUX2 set -g status-right ""
$TMUX2 set -g side-status left

$TMUX send-keys "exec $TEST_TMUX -LtestB$$ -f/dev/null attach" Enter
sleep 1
compare side-left

$TMUX2 set -g side-status right
compare side-right

$TMUX2 set -g status on
compare side-right-status

$TMUX2 set -g side-status-width 12
compare side-right-narrow

$TMUX2 set -g side-status off
compare side-off

# A second session collapses the bar into session rows with the current
# session's windows indented below its row.
$TMUX2 set -g side-status left
$TMUX2 set -g side-status-width 24
$TMUX2 set -g status off
$TMUX2 new-session -d -s beta -n shell "sh -c 'printf SHELL; exec sleep 100'"
$TMUX2 refresh-client -S
compare side-sessions

exit 0
