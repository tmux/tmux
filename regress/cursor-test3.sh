#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX new -d -x7 -y2 "printf 'abcdefabcdefab'; printf '\e[2;7H'; cat" || exit 1
$TMUX set -g window-size manual || exit 1

$TMUX display -pF '#{cursor_x} #{cursor_y} #{cursor_character}' >>$TMP
$TMUX capturep -p|awk '{print NR-1,$0}' >>$TMP
$TMUX resizew -x5 || exit 1
$TMUX display -pF '#{cursor_x} #{cursor_y} #{cursor_character}' >>$TMP
$TMUX capturep -p|awk '{print NR-1,$0}' >>$TMP
$TMUX resizew -x7 || exit 1
$TMUX display -pF '#{cursor_x} #{cursor_y} #{cursor_character}' >>$TMP
$TMUX capturep -p|awk '{print NR-1,$0}' >>$TMP

cmp -s $TMP cursor-test3.result || exit 1

$TMUX kill-server 2>/dev/null
exit 0
