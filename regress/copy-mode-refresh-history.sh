#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap 'stop_server $TMUX' 0
trap 'exit 1' 1 2 15

set -e

check()
{
	expected=$1
	shift
	actual=$($TMUX "$@")
	if [ "$actual" != "$expected" ]; then
		printf '%s: expected [%s], got [%s]\n' "$*" "$expected" "$actual" >&2
		exit 1
	fi
}

pane()
{
	$TMUX new-window '' \; resize-window -x "$1" -y "$2"
}

input()
{
	printf '%b' "$1" | $TMUX display-message -I
}

$TMUX new-session -d 'exec sleep 300' \; set -g status off \; \
    set -g window-size manual

# Edit exposed history, then shrink back.
pane 4 3
input 'abcdwxyz\r\nB\r\nC\r\nD'
# Use clock mode so copy mode misses both resizes.
$TMUX copy-mode \; clock-mode \; resize-window -y5
input '\033[1;1H\033[2KXXXX\033[5;1H'
$TMUX resize-window -y3 \; copy-mode
check 'W abcd
- wxyz' capture-pane -pMF -S - -E -1
$TMUX send -X refresh-now
check '- XXXX
- wxyz' capture-pane -pMF -S - -E -1

pane 4 5
$TMUX copy-mode
input 'abcdefgh\r\nB\r\nC\r\nD'
$TMUX resize-window -y3 \; send -X refresh-now
check 'W abcd
- efgh' capture-pane -pMF -S - -E -1

# Check the wrap after trimming history.
$TMUX set -g history-limit 2
pane 4 4
input 'old\r\nAAAAvvvv\r\nB\r\nC\r\nD'
$TMUX copy-mode
input '\033[H\033[2K12345\033[4;1H\n'
check '- old
W AAAA' capture-pane -pMF -S - -E -1
$TMUX send -X refresh-now
check '- AAAA
W 1234' capture-pane -pMF -S - -E -1

exit 0
