#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
}
fail()
{
	echo "$1"
	cleanup
	exit 1
}
expect_buffer()
{
	expected=$1
	actual=$($TMUX show-buffer)
	[ "$actual" = "$expected" ] ||
		fail "unexpected buffer: expected [$expected], got [$actual]"
}
wheel()
{
	button=$1
	col=$2
	row=$3
	seq=$(printf '\033[<%s;%s;%sM' "$button" "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

$TMUX new -d -x40 -y10 \
	'i=0; while [ $i -lt 80 ]; do printf "line %02d xxxxxxxxxx\n" $i; i=$((i + 1)); done; cat' ||
	exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g mouse on || exit 1

$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -N10 -X cursor-down || exit 1
$TMUX send-keys -X start-of-line || exit 1
$TMUX send-keys -X begin-selection || exit 1
$TMUX send-keys -N2 -X cursor-down || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1

initial=$(printf 'line 10 xxxxxxxxxx\nline 11 xxxxxxxxxx')
expect_buffer "$initial"

$TMUX send-keys -X stop-selection || exit 1
$TMUX send-keys -N3 -X scroll-down || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -N2 -X scroll-up || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X scroll-middle || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X scroll-bottom || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X scroll-top || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X recentre-top-bottom || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X other-end || exit 1
$TMUX send-keys -X cursor-down || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1

extended_end=$(printf 'line 10 xxxxxxxxxx\nline 11 xxxxxxxxxx\nline 12 xxxxxxxxxx')
expect_buffer "$extended_end"

$TMUX send-keys -X stop-selection || exit 1
$TMUX send-keys -X other-end || exit 1
$TMUX send-keys -X other-end || exit 1
$TMUX send-keys -X cursor-up || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1

extended_start=$(printf 'line 09 xxxxxxxxxx\nline 10 xxxxxxxxxx\nline 11 xxxxxxxxxx\nline 12 xxxxxxxxxx')
expect_buffer "$extended_start"

$TMUX2 new-session -d -x40 -y10 "$TMUX attach" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "no outer pane"

wheel 65 5 5
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$extended_start"

wheel 64 5 5
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$extended_start"

exit 0
