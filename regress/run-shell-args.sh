#!/bin/sh

# run-shell argument interpolation: #1, #{1}, #{argumentN}

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

# #1 shorthand: first argument
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo #1 >$TMP" hello || exit 1
sleep 1 && [ "$(cat $TMP)" = "hello" ] || exit 1

# #{1} format syntax: first argument
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo #{1} >$TMP" world || exit 1
sleep 1 && [ "$(cat $TMP)" = "world" ] || exit 1

# #{argument1} named form
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo #{argument1} >$TMP" named || exit 1
sleep 1 && [ "$(cat $TMP)" = "named" ] || exit 1

# multiple arguments: #1 and #2
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo #1 #2 >$TMP" foo bar || exit 1
sleep 1 && [ "$(cat $TMP)" = "foo bar" ] || exit 1

# missing argument expands to empty string (not literal '#2')
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo '#2' >$TMP" onlyone || exit 1
sleep 1 && [ "$(cat $TMP)" = "" ] || exit 1

# no arguments: shell-command still works normally
$TMUX -f/dev/null new-session -d -s main \; \
    run-shell "echo noargs >$TMP" || exit 1
sleep 1 && [ "$(cat $TMP)" = "noargs" ] || exit 1

$TMUX kill-server 2>/dev/null

exit 0
