#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

$TMUX new -d -x40 -y10 \
	'i=0; while [ $i -lt 80 ]; do echo "line $i"; i=$((i + 1)); done; cat' ||
	exit 1
$TMUX set -g window-size manual || exit 1

$TMUX copy-mode -e || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -X start-of-line || exit 1
$TMUX send-keys -X begin-selection || exit 1
$TMUX send-keys -X cursor-down || exit 1

[ "$($TMUX display-message -p '#{selection_present}')" = "1" ] || exit 1
$TMUX send-keys -N200 -X scroll-down || exit 1
[ "$($TMUX display-message -p '#{pane_in_mode} #{scroll_position}')" = "1 0" ] ||
	exit 1

$TMUX send-keys -X clear-selection || exit 1
$TMUX send-keys -X scroll-down || exit 1
[ "$($TMUX display-message -p '#{pane_in_mode}')" = "0" ] || exit 1

exit 0
