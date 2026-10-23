#!/bin/sh

. ./regress-common.inc

# A tree mode menu opened by one client may be chosen from by another client
# viewing the same window. If the first client has gone by then, the menu
# must not use it.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(test_tmpdir) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	stop_server $TMUX || exit 1
	stop_server $TMUX2 || exit 1
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	exit 1
}

$TMUX new-session -d -s main -x 80 -y 24 'cat' || exit 1
$TMUX new-session -d -s other -x 80 -y 24 'cat' || exit 1
$TMUX set -g mouse on || exit 1

# Two clients attached to main, each in a window of a second server.
$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t main" || exit 1
wait_for_clients 1 $TMUX || fail "first client did not attach"
$TMUX2 new-window -d -t out:1 "$TMUX attach -t main" || exit 1
wait_for_clients 2 $TMUX || fail "second client did not attach"

# The first client opens tree mode, then its menu with a right click.
$TMUX2 send-keys -t out:0 C-b s || fail "send-keys failed"
wait_for_output 1 $TMUX display-message -p -t main '#{pane_in_mode}' ||
	fail "tree mode did not open"
$TMUX2 send-keys -t out:0 -l "$(printf '\033[<2;10;3M')" ||
	fail "send-keys failed"
wait_for_text Select $TMUX2 capture-pane -p -t out:1 ||
	fail "menu did not open"

# The first client goes, and the server is given a moment to free it. The
# second client chooses from the menu, which then closes.
$TMUX2 kill-window -t out:0 || exit 1
wait_for_clients 1 $TMUX || fail "first client did not go"
sleep 1
$TMUX2 send-keys -t out:1 Enter || fail "send-keys failed"
i=0
while :; do
	screen=$($TMUX2 capture-pane -p -t out:1 2>/dev/null) || {
		$TMUX has-session -t main 2>/dev/null || fail "server exited"
		fail "capture-pane failed"
	}
	printf '%s\n' "$screen" | grep -q Select || break
	i=$((i + 1))
	[ $i -lt 100 ] || fail "menu did not close"
	sleep 0.05
done

$TMUX has-session -t main 2>/dev/null || fail "server exited"

exit 0
