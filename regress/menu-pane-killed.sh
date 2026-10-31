#!/bin/sh

# A menu opened for a pane is drawn again after the pane is killed. Drawing it
# must not use the pane. The terminal is a pane of a second server.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null' 0 1 15

$TMUX new -d 'sleep 1000' || exit 1
$TMUX2 new -d "unset TMUX; exec $TMUX attach" || exit 1
n=0
while [ -z "$($TMUX lsc 2>/dev/null)" ]; do
	n=$((n + 1))
	[ $n -gt 100 ] && exit 1
	sleep 0.1
done

P=$($TMUX splitw -d -P -F '#{pane_id}' 'sleep 1000') || exit 1
$TMUX display-menu -c "$($TMUX lsc -F '#{client_name}')" -t $P one 1 '' ||
    exit 1
$TMUX kill-pane -t $P
sleep 1
$TMUX has || { echo "the server died"; exit 1; }
exit 0
