#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX -f/dev/null new -d -x10 -y3 "printf 'abcdef\n'; cat" || exit 1
$TMUX set -g window-size manual || exit 1

$TMUX display -pF '#{cursor_x} #{cursor_y} #{cursor_character}' >>$TMP
$TMUX capturep -p|awk '{print NR-1,$0}' >>$TMP
$TMUX resizew -x20 || exit 1
$TMUX display -pF '#{cursor_x} #{cursor_y} #{cursor_character}' >>$TMP
$TMUX capturep -p|awk '{print NR-1,$0}' >>$TMP
$TMUX resizew -x3 || exit 1
$TMUX display -pF '#{cursor_x} #{cursor_y} #{cursor_character}' >>$TMP
$TMUX capturep -p|awk '{print NR-1,$0}' >>$TMP
$TMUX resizew -x10 || exit 1
$TMUX display -pF '#{cursor_x} #{cursor_y} #{cursor_character}' >>$TMP
$TMUX capturep -p|awk '{print NR-1,$0}' >>$TMP

cmp -s $TMP cursor-test4.result || exit 1

$TMUX kill-server 2>/dev/null
exit 0
