#!/bin/sh

. ./regress-common.inc

# 947
# size in control mode should change after refresh-client -C, per-window
# control client sizes should work, and -x and -y should work without -d for
# control clients

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMP=$(mktemp)
OUT=$(mktemp)
trap 'rm -f "$TMP" "$OUT"; stop_server $TMUX || exit 1' 0 1 15

$TMUX -f/dev/null new -d || exit 1
cat <<EOF|$TMUX -C a >$TMP
ls -F':#{window_width} #{window_height}'
refresh -C 100,50
EOF
grep ^: $TMP >$OUT
$TMUX ls -F':#{window_width} #{window_height}' >>$OUT
printf ":80 24\n:100 50\n"|cmp -s $OUT - || exit 1
stop_server $TMUX || exit 1

$TMUX -f/dev/null new -d || exit 1
cat <<EOF|$TMUX -f/dev/null -C a >$TMP
ls -F':#{window_width} #{window_height}'
refresh -C 80,24
EOF
grep ^: $TMP >$OUT
$TMUX ls -F':#{window_width} #{window_height}' >>$OUT
printf ":80 24\n:80 24\n"|cmp -s $OUT - || exit 1
stop_server $TMUX || exit 1

cat <<EOF|$TMUX -f/dev/null -C new -x 100 -y 50 >$TMP
ls -F':#{window_width} #{window_height}'
refresh -C 80,24
EOF
grep ^: $TMP >$OUT
$TMUX ls -F':#{window_width} #{window_height}' >>$OUT
printf ":100 50\n:80 24\n"|cmp -s $OUT - || exit 1
stop_server $TMUX || exit 1

$TMUX -f/dev/null new -d || exit 1
$TMUX neww || exit 1
w0=$($TMUX display -p -t :0 '#{window_id}')
w1=$($TMUX display -p -t :1 '#{window_id}')
cat <<EOF|$TMUX -f/dev/null -C a >$TMP
refresh -C 80,24
refresh -C ${w0}:40x10
display -p -t ${w0} 'w0 #{window_width} #{window_height}'
display -p -t ${w1} 'w1 #{window_width} #{window_height}'
refresh -C ${w0}:
display -p -t ${w0} 'w0c #{window_width} #{window_height}'
EOF
grep ^w $TMP >$OUT
printf "w0 40 10\nw1 80 24\nw0c 80 24\n"|cmp -s $OUT - || exit 1
stop_server $TMUX || exit 1

exit 0
