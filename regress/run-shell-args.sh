#!/bin/sh

# run-shell argument interpolation: #1, #{1}

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; rm -f $TMP" 0 1 15

# #1 shorthand: first argument
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo #1 >$TMP" hello || exit 1
sleep 1 && [ "$(cat $TMP)" = "hello" ] || exit 1
$TMUX kill-server 2>/dev/null

# #{1} format syntax: first argument
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo #{1} >$TMP" world || exit 1
sleep 1 && [ "$(cat $TMP)" = "world" ] || exit 1
$TMUX kill-server 2>/dev/null

# multiple arguments: #1 and #2
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo #1 #2 >$TMP" foo bar || exit 1
sleep 1 && [ "$(cat $TMP)" = "foo bar" ] || exit 1
$TMUX kill-server 2>/dev/null

# missing argument: #2 when only one arg given expands to empty
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo -n #{2} >$TMP" onlyone || exit 1
sleep 1 && [ "$(cat $TMP)" = "" ] || exit 1
$TMUX kill-server 2>/dev/null

# no arguments: shell-command still works normally
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo noargs >$TMP" || exit 1
sleep 1 && [ "$(cat $TMP)" = "noargs" ] || exit 1
$TMUX kill-server 2>/dev/null

exit 0
