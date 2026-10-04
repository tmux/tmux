#!/bin/sh

# Kitty placeholder rows stay cell-aligned when a client becomes narrower.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Limage-placeholder-outer$$ -f/dev/null"
TMUX2="$TEST_TMUX -u -Limage-placeholder-inner$$ -f/dev/null"
TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null; rm -f $TMP" 0 1 15

$TMUX2 new-session -d -x 10 -y 4 "
	i=0
	while [ \$i -lt 10 ]; do
		printf '\\364\\216\\273\\256\\314\\205'
		i=\$((i + 1))
	done
	sleep 30" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX new-session -d -x 10 -y 4 "$TMUX2 attach-session" || exit 1
$TMUX set -g status off || exit 1
sleep 1

$TMUX resize-window -x 5 -y 4 || exit 1
sleep 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ -n "$(sed -n 1p $TMP)" ] || exit 1
[ -z "$(sed -n 2p $TMP)" ] || exit 1
$TMUX resize-window -x 10 -y 4 || exit 1
sleep 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ "$(sed -n 1p $TMP | wc -c)" = 61 ] || exit 1
[ -z "$(sed -n 2p $TMP)" ] || exit 1

exit 0
