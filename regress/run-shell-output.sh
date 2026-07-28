#!/bin/sh

# 4476
# run-shell should go to stdout if present without -t

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; rm -f $TMP" 0 1 15

$TMUX -f/dev/null new -d "$TMUX run 'echo foo' >$TMP; sleep 10" || exit 1
sleep 1 && [ "$(cat $TMP)" = "foo" ] || exit 1

$TMUX -f/dev/null new -d "$TMUX run -t: 'echo foo' >$TMP; sleep 10" || exit 1
sleep 1 && [ "$(cat $TMP)" = "" ] || exit 1
[ "$($TMUX display -p '#{pane_mode}')" = "view-mode" ] || exit 1

$TMUX -f/dev/null new -d -s t1 'sleep 10' || exit 1
$TMUX -f/dev/null run -d 1 'echo delayed' >$TMP 2>&1 &
pid=$!
sleep 0.2
kill -9 "$pid" 2>/dev/null
wait "$pid" 2>/dev/null
sleep 2
$TMUX has-session -t t1 || exit 1

$TMUX kill-server 2>/dev/null

exit 0
