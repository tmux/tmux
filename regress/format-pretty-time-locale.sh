#!/bin/sh

# The t/p format modifier must not overflow its buffer when the locale has
# long month names.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=ru_RU.UTF-8
export PATH TERM LC_ALL

locale -a 2>/dev/null | grep -qi '^ru_RU\.utf-\?8$' || exit 0

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
trap "$TMUX kill-server 2>/dev/null" 0 1 15

$TMUX new -d || exit 1

# 15 March 2020, well over a year ago so formatted as "%h%y".
$TMUX set -g @t 1584273600 || exit 1
out=$($TMUX display -p '#{t/p:@t}') || exit 1
case "$out" in
*20)
	;;
*)
	echo "bad pretty time: $out"
	exit 1
	;;
esac
$TMUX has || exit 1

exit 0
