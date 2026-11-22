#!/bin/sh

# The copy mode position indicator in a pane partly under a floating pane must
# still show its visible end.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX2 new -d -x30 -y15 "exec sleep 100" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g copy-mode-position-format "#[align=right]ABCDE" || exit 1
$TMUX2 new-pane -x30 -y3 -X-3 -Y0 "exec sleep 100" || exit 1

$TMUX new -d -x30 -y15 "$TMUX2 attach" || exit 1
$TMUX set -g status off || exit 1
wait_for_clients 1 $TMUX2 || exit 1
sleep 1

$TMUX2 copy-mode -t %0 || exit 1
sleep 1
$TMUX2 refresh-client || exit 1
sleep 1

line=$($TMUX capture-pane -p | head -1)
case "$line" in
*DE)
	;;
*)
	echo "row 0: '$line'" >&2
	exit 1
	;;
esac
exit 0
