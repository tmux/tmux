#!/bin/sh

# time modifiers with a value outside the range the system can convert

PATH=/bin:/usr/bin
TERM=screen
TZ=UTC
export TZ

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null" 0 1 15

$TMUX new -d
$TMUX set -g @ok 1700000000
$TMUX set -g @huge 3093527980800
$TMUX set -g @bad 9223372036854775807

# A convertible time still formats, so an empty result below means the value
# was rejected and not that the modifier stopped working.
$TMUX display -p '[#{t:@ok}]' >$TMP
(echo "[Tue Nov 14 22:13:20 2023]"|cmp -s - $TMP) || exit 1
$TMUX display -p '[#{t/f/zz:@ok}]' >$TMP
(echo "[zz]"|cmp -s - $TMP) || exit 1
$TMUX display -p '[#{t/p:@ok}]' >$TMP
(echo "[Nov23]"|cmp -s - $TMP) || exit 1

# A year with more digits than ctime has room for must not be printed cut
# short, but other modifiers can still format it.
$TMUX display -p '[#{t:@huge}]' >$TMP
grep -Eqx '\[\]|\[Sat Jan  1 00:00:00 100000\]' $TMP || exit 1
$TMUX display -p '[#{t/f/zz:@huge}]' >$TMP
(echo "[zz]"|cmp -s - $TMP) || exit 1

$TMUX display -p '[#{t:@bad}]' >$TMP
(echo "[]"|cmp -s - $TMP) || exit 1
$TMUX display -p '[#{t/p:@bad}]' >$TMP
(echo "[]"|cmp -s - $TMP) || exit 1
$TMUX display -p '[#{t/f/zz:@bad}]' >$TMP
(echo "[]"|cmp -s - $TMP) || exit 1

exit 0
