#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX=
n=0
trap '[ -n "$TMUX" ] && $TMUX kill-server 2>/dev/null' 0 1 15

for i in conf/*.conf; do
	n=$((n + 1))
	TMUX="$TEST_TMUX -LtestA$$-$n -f/dev/null"
	$TMUX -f/dev/null start \; source -n $i || exit 1
done

exit 0
