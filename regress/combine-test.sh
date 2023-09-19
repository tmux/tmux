#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX -f/dev/null new -d "
printf '\e[H\e[J'
printf '\e[3;1H\316\233\e[3;1H\314\2120\n'
printf '\e[4;1H\316\233\e[4;2H\314\2121\n'
printf '\e[5;1H👍\e[5;1H🏻2\n'
printf '\e[6;1H👍\e[6;3H🏻3\n'
printf '\e[7;1H👍\e[7;10H👍\e[7;3H🏻\e[7;12H🏻4\n'
printf '\e[8;1H\360\237\244\267\342\200\215\342\231\202\357\270\2175\n'
printf '\e[9;1H\360\237\244\267\e[9;1H\342\200\215\342\231\202\357\270\2176\n'
printf '\e[9;1H\360\237\244\267\e[9;1H\342\200\215\342\231\202\357\270\2177\n'
printf '\e[10;1H\360\237\244\267\e[10;3H\342\200\215\342\231\202\357\270\2178\n'
printf '\e[11;1H\360\237\244\267\e[11;3H\342\200\215\e[11;3H\342\231\202\357\270\2179\n'
printf '\e[12;1H\360\237\244\267\e[12;3H\342\200\215\342\231\202\357\270\21710\n'
printf '\e[13;1H\360\237\207\25211\n'
printf '\e[14;1H\360\237\207\270\360\237\207\25212\n'
printf '\e[15;1H\360\237\207\270  \010\010\360\237\207\25213\n'
$TMUX capturep -pe >>$TMP"

sleep 1

cmp $TMP combine-test.result || exit 1

$TMUX has 2>/dev/null && exit 1

exit 0
