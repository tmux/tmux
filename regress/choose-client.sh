#!/bin/sh

# Tests of client mode (window-client.c) as driven by choose-client: that the
# -f filter removes clients that do not match, that a filter matching nothing
# falls back to showing everything, and that Enter runs the default command
# (detach-client) on the selected client. Sort orders are not tested here
# because clients are named after their ttys, which are not predictable.
#
# The list is drawn on a mode screen which capture-pane does not show, so a
# second server provides the clients: two inner "tmux attach" commands run in
# panes of the second server, and the pane holding the client the mode is
# displayed on is captured. Each choose-client call uses a distinct -F marker
# so a capture can be tied to the call it belongs to.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2 -f/dev/null"

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

# capture the screen rendered by the inner client attached to aaa
capture()
{
	$TMUX2 capture-pane -p -t out:0 2>/dev/null
}

# wait_for $marker
#
# Wait (up to ~10s) until the rendered screen contains $marker, so the
# capture is known to show the mode instance under test.
wait_for()
{
	i=0
	while [ "$i" -lt 50 ]; do
		if capture | grep -q "$1"; then
			sleep 0.5
			return 0
		fi
		sleep 0.5
		i=$((i + 1))
	done
	fail "timed out waiting for '$1'"
}

# wait_clients $n
#
# Wait (up to ~10s) until the test server has exactly $n clients attached.
wait_clients()
{
	i=0
	while [ "$i" -lt 10 ]; do
		c=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$c" -eq "$1" ] && return 0
		sleep 1
		i=$((i + 1))
	done
	return 1
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

# One client attached to each of two sessions; the mode is displayed on the
# client attached to aaa (in window 0 of the outer server) and the filters
# tell the clients apart by their attached session.
$TMUX new-session -d -s aaa -x 80 -y 24 || exit 1
$TMUX new-session -d -s bbb -x 80 -y 24 || exit 1

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t aaa" || exit 1
$TMUX2 new-window -d -t out: "$TMUX attach -t bbb" || exit 1
wait_clients 2 || fail "expected two clients attached to test server"

# --- filter keeping only the aaa client -------------------------------------
$TMUX choose-client -t aaa:0 -F 'C1=#{client_session}' \
	-f '#{==:#{client_session},aaa}' || exit 1
wait_for 'C1='
out=$(capture)
echo "$out" | grep -q 'C1=aaa' || fail "aaa client missing when it matches"
echo "$out" | grep -q 'C1=bbb' && fail "bbb client shown but does not match"
[ "$(echo "$out" | grep -c 'C1=')" -eq 1 ] || fail "expected 1 client"
$TMUX send-keys -t aaa:0 q

# --- filter keeping only the bbb client -------------------------------------
$TMUX choose-client -t aaa:0 -F 'C2=#{client_session}' \
	-f '#{==:#{client_session},bbb}' || exit 1
wait_for 'C2='
out=$(capture)
echo "$out" | grep -q 'C2=bbb' || fail "bbb client missing when it matches"
echo "$out" | grep -q 'C2=aaa' && fail "aaa client shown but does not match"
[ "$(echo "$out" | grep -c 'C2=')" -eq 1 ] || fail "expected 1 client"
$TMUX send-keys -t aaa:0 q

# --- no filter shows both clients -------------------------------------------
$TMUX choose-client -t aaa:0 -F 'C3=#{client_session}' || exit 1
wait_for 'C3='
out=$(capture)
echo "$out" | grep -q 'C3=aaa' || fail "aaa client missing with no filter"
echo "$out" | grep -q 'C3=bbb' || fail "bbb client missing with no filter"
[ "$(echo "$out" | grep -c 'C3=')" -eq 2 ] || fail "expected 2 clients"
$TMUX send-keys -t aaa:0 q

# --- filter matching nothing ------------------------------------------------
#
# Everything is shown and the filter indicator reports no matches.
$TMUX choose-client -t aaa:0 -F 'C4=#{client_session}' \
	-f '#{==:#{client_session},nosuch}' || exit 1
wait_for 'C4='
out=$(capture)
echo "$out" | grep -q 'C4=aaa' || fail "aaa client missing with no-match filter"
echo "$out" | grep -q 'C4=bbb' || fail "bbb client missing with no-match filter"
echo "$out" | grep -q 'no matches' || fail "no matches indicator missing"
$TMUX send-keys -t aaa:0 q

# --- Enter runs the default command (detach-client) --------------------------
#
# The filter leaves only the bbb client listed and selected; Enter detaches
# it, leaving only the aaa client attached.
$TMUX choose-client -t aaa:0 -F 'G1=#{client_session}' \
	-f '#{==:#{client_session},bbb}' || exit 1
wait_for 'G1=bbb'
$TMUX send-keys -t aaa:0 Enter
wait_clients 1 || fail "bbb client did not detach"
[ "$($TMUX list-clients -F '#{client_session}')" = "aaa" ] || \
	fail "wrong client detached"

cleanup
exit 0
