#!/bin/sh

# With status off and status-position top, the prompt and its cursor use
# the top row of the pane. Mouse clicks use that row too, and the cursor
# is hidden when the prompt row is just below the viewport.

. ./regress-common.inc

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
$TMUX set -g mouse on || exit 1
$TMUX2 new-session -d -s outer -x 40 -y 8 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
wait_for_clients 1 $TMUX || fail "inner client did not attach"
CLIENT=$($TMUX list-clients -F '#{client_name}') || exit 1

$TMUX command-prompt -b -P -t "$CLIENT" -p PROMPTX -I abcdef \
    'set -g @prompt_input "%%"' || exit 1
wait_for_text 'PROMPTX abcdef' $TMUX2 capture-pane -pt outer:0.0 -S0 -E0 ||
    fail "prompt not on the top row"
wait_for_output '1 0' $TMUX2 display -pt outer:0.0 \
    '#{cursor_flag} #{cursor_y}' || fail "cursor not visible on the top row"

# Click before the second input character, then insert and submit.
$TMUX2 send-keys -lt outer:0.0 "$(printf '\033[<0;10;1M')" || exit 1
$TMUX2 send-keys -t outer:0.0 X Enter || exit 1
wait_for_output aXbcdef $TMUX show -gqv @prompt_input ||
    fail "click did not move the prompt cursor"

# The lower pane starts at row 8. Pan down one row so its prompt is
# visible, then pan back up so the prompt is just outside the client.
$TMUX resize-window -t inner:0 -y 16 || exit 1
$TMUX split-window -v -l 8 -t inner:0 'exec sleep 100' || exit 1
$TMUX refresh-client -Ut "$CLIENT" 16 || exit 1
$TMUX refresh-client -Dt "$CLIENT" 1 || exit 1
$TMUX command-prompt -b -P -t "$CLIENT" -p CLIPPED 'display -p %%' || exit 1
wait_for_text CLIPPED $TMUX2 capture-pane -pt outer:0.0 -S7 -E7 ||
    fail "lower pane prompt not on the last visible row"
wait_for_output '1 7' $TMUX2 display -pt outer:0.0 \
    '#{cursor_flag} #{cursor_y}' || fail "lower pane cursor not on last row"
$TMUX refresh-client -Ut "$CLIENT" 1 || exit 1
wait_for_output 0 $TMUX2 display -pt outer:0.0 '#{cursor_flag}' ||
    fail "cursor still visible below the viewport"

exit 0
