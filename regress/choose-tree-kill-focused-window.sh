#!/bin/sh

. ./regress-common.inc

# GitHub issue 5864: killing the focused (and only) window of the attached
# session from choose-tree -w (choose-window / prefix-w) used to crash the
# server. The tree is drawn in that window, so the confirmation callback must
# finish without redrawing a destroyed mode, whether detach-on-destroy is on
# (the client leaves) or off (the client is moved to another session).
#
# A second server provides the client, as in choose-tree.sh.

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

wait_mark()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$TMUX2 capture-pane -p -t out:0 2>/dev/null | grep -F -q "$1" && \
		    return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1'"
}

# start_tree $detach_on_destroy $marker
start_tree()
{
	stop_server $TMUX 2>/dev/null || true
	stop_server $TMUX2 2>/dev/null || true
	$TMUX new-session -d -s a -x 80 -y 24 'cat' || fail "new-session a failed"
	$TMUX new-session -d -s b -x 80 -y 24 'cat' || fail "new-session b failed"
	if [ "$1" = "off" ]; then
		$TMUX set-option -g detach-on-destroy off || exit 1
	fi
	$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t a" || \
	    fail "attach failed"
	wait_for_clients 1 $TMUX || fail "client did not attach"
	$TMUX choose-tree -w -t a:0 -F "$2" || fail "choose-tree failed"
	wait_mark "$2"
}

# --- default detach-on-destroy (on): client leaves, session b remains --------
start_tree on 'W1'
$TMUX send-keys -t a:0 x || fail "send-keys x failed"
wait_mark 'Kill'
$TMUX send-keys -t a:0 y || fail "send-keys y failed"
i=0
while [ "$i" -lt 50 ]; do
	sessions=$($TMUX list-sessions -F '#{session_name}' 2>/dev/null) || {
		fail "server exited after killing the focused window"
	}
	nclients=$($TMUX list-clients 2>/dev/null | wc -l)
	[ "$sessions" = 'b' ] && [ "$nclients" -eq 0 ] && break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "expected only session b and no clients, have sessions='$sessions' clients='$nclients'"

# --- detach-on-destroy off: client moves to session b ------------------------
start_tree off 'W2'
$TMUX send-keys -t a:0 x || fail "send-keys x failed"
wait_mark 'Kill'
$TMUX send-keys -t a:0 y || fail "send-keys y failed"
i=0
while [ "$i" -lt 50 ]; do
	sessions=$($TMUX list-sessions -F '#{session_name}' 2>/dev/null) || {
		fail "server exited after killing the focused window"
	}
	client_session=$($TMUX list-clients -F '#{client_session}' 2>/dev/null)
	[ "$sessions" = 'b' ] && [ "$client_session" = 'b' ] && break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "expected client to move to b, have sessions='$sessions' client='$client_session'"

exit 0
