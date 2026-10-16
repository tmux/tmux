#!/bin/sh

# respawn-window -k destroys every pane but the first, including the active
# pane. The window must be left with the first pane active and must be drawn
# again for an attached client. The terminal is a pane of a second server.

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

$TMUX splitw 'sleep 1000' || exit 1
$TMUX splitw -h 'sleep 1000' || exit 1
sleep 1
$TMUX respawnw -k 'sleep 1000' || exit 1
sleep 1
$TMUX has || { echo "the server died"; exit 1; }
[ "$($TMUX lsp -F '#{pane_index}:#{pane_active}')" = "0:1" ] || exit 1

# The same with the window zoomed: it must be unzoomed and the window must
# then be destroyed cleanly.
$TMUX neww 'sleep 1000' || exit 1
$TMUX splitw 'sleep 1000' || exit 1
$TMUX resizep -Z || exit 1
sleep 1
$TMUX respawnw -k 'sleep 1000' || exit 1
sleep 1
[ "$($TMUX display -p '#{window_panes}:#{window_zoomed_flag}')" = "1:0" ] ||
    exit 1
$TMUX killw || exit 1
sleep 1
$TMUX has || { echo "the server died"; exit 1; }
exit 0
