#!/bin/sh

# Check that attributes listed in ncv are not used with colours.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

fail() {
	echo "$*" >&2
	exit 1
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX2 new -d -x20 -y5 \
	"printf '\033[2;4mPLAIN\033[0m'; exec sleep 100" || exit 1
$TMUX2 set -g status-left '' || exit 1
$TMUX2 set -g status-right '' || exit 1
$TMUX2 set -g status-format[0] \
	'#[fg=black,bg=green,underscore,dim]MARK' || exit 1
$TMUX2 set -as terminal-overrides ',screen:ncv=18' || exit 1

$TMUX new -d -x20 -y5 "$TMUX2 attach" || exit 1
$TMUX set -g status off || exit 1
wait_for_clients 1 $TMUX2 || fail "inner client did not attach"
sleep 1

$TMUX capturep -pe >$TMP || exit 1
ESC=$(printf '\033')
grep -q "${ESC}\\[30m${ESC}\\[42mMARK" $TMP ||
	fail "coloured status text missing"
grep -q "${ESC}\\[2;4mPLAIN" $TMP ||
	fail "attributes without colours were filtered"
tail -1 $TMP | grep -q "${ESC}\\[4m" && fail "underline not filtered"
tail -1 $TMP | grep -q "${ESC}\\[2m" && fail "dim not filtered"

exit 0
