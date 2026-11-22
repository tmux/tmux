#!/bin/sh

# With status off and status-position top, a pane prompt is drawn on the top
# row of the pane and the cursor must be there too.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

$TMUX new-session -d -s inner -x 40 -y 8 'exec sleep 100' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g status-position top || exit 1
$TMUX2 new-session -d -s outer -x 40 -y 8 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1

i=0
while [ "$i" -lt 50 ]; do
	CLIENT=$($TMUX list-clients -F '#{client_name}' 2>/dev/null)
	[ -n "$CLIENT" ] && break
	sleep 0.1
	i=$((i + 1))
done
[ -n "$CLIENT" ] || fail "inner client did not attach"

$TMUX command-prompt -b -P -t "$CLIENT" -p PROMPTX 'display -p %%' ||
    exit 1

i=0
while [ "$i" -lt 50 ]; do
	$TMUX2 capture-pane -p -t outer:0.0 | head -n 1 | grep -q PROMPTX &&
	    break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "prompt not on the top row"
sleep 0.5

y=$($TMUX2 display -p -t outer:0.0 '#{cursor_y}')
[ "$y" = 0 ] || fail "cursor on row $y, expected 0"

exit 0
