#!/bin/sh

# Widening a pane restores clipped columns for every image placement.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Limage-resize-outer$$ -f/dev/null"
TMUX2="env -u TMUX LC_ALL=C $TEST_TMUX -Limage-resize-inner$$ -f/dev/null"
TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null; rm -f $TMP" 0 1 15

# White placements stay behind the final red placement.
$TMUX2 new-session -d -x 8 -y 4 "
	i=0
	while [ \$i -lt 64 ]; do
		printf '\\033_Ga=T,q=2,C=1,f=32,s=1,v=1,c=16,r=1;/////w==\\033\\\\'
		i=\$((i + 1))
	done
	printf '\\033_Ga=T,q=2,C=1,f=32,s=1,v=1,c=16,r=1;/wAA/w==\\033\\\\'
	sleep 30" || exit 1
[ "$($TMUX2 display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX2 set -g status off || exit 1
$TMUX new-session -d -x 8 -y 4 "$TMUX2 attach-session" || exit 1
$TMUX set -g status off || exit 1
sleep 1

$TMUX capture-pane -pS0 -E0 >$TMP || exit 1
[ "$(cat $TMP)" = '........' ] || exit 1
$TMUX resize-window -x 16 -y 4 || exit 1
sleep 1
$TMUX capture-pane -pS0 -E0 >$TMP || exit 1
[ "$(cat $TMP)" = '................' ] || exit 1

exit 0
