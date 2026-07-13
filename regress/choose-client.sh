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
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
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
		CAPTURED=$(capture)
		if printf '%s\n' "$CAPTURED" | grep -F -q "$1"; then
			return 0
		fi
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1'"
}

# wait_gone $marker
#
# Wait (up to ~10s) until the rendered screen no longer contains $marker.
wait_gone()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CAPTURED=$(capture)
		if ! printf '%s\n' "$CAPTURED" | grep -F -q "$1"; then
			return 0
		fi
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1' to disappear"
}

# wait_count $marker $n
#
# Wait (up to ~10s) until exactly $n rendered lines contain $marker. The
# matching capture is left in CAPTURED.
wait_count()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CAPTURED=$(capture)
		c=$(printf '%s\n' "$CAPTURED" | grep -F -c "$1")
		[ "$c" -eq "$2" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for $2 lines of '$1' (have $c)"
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

# wait_mode $target $state
#
# Wait (up to ~10s) until a pane enters or leaves mode.
wait_mode()
{
	t=$1
	want=$2

	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display-message -p -t "$t" '#{pane_in_mode}' \
		    2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "pane $t mode state is $got, expected $want"
}

exit_mode()
{
	marker=$1
	shift

	$TMUX send-keys -t aaa:0 "$@" || fail "send-keys $* failed"
	wait_mode aaa:0 0
	wait_gone "$marker"
}

# One client attached to each of two sessions; the mode is displayed on the
# client attached to aaa (in window 0 of the outer server) and the filters
# tell the clients apart by their attached session.
$TMUX new-session -d -s aaa -x 80 -y 24 'cat' || exit 1
$TMUX new-session -d -s bbb -x 80 -y 24 'cat' || exit 1

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t aaa" || exit 1
$TMUX2 new-window -d -t out: "$TMUX attach -t bbb" || exit 1
wait_clients 2 || fail "expected two clients attached to test server"

# --- filter keeping only the aaa client -------------------------------------
$TMUX choose-client -t aaa:0 -F 'C1=#{client_session}' \
	-f '#{==:#{client_session},aaa}' || exit 1
wait_count 'C1=' 1
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'C1=aaa' || \
	fail "aaa client missing when it matches"
printf '%s\n' "$out" | grep -F -q 'C1=bbb' && \
	fail "bbb client shown but does not match"
[ "$(printf '%s\n' "$out" | grep -F -c 'C1=')" -eq 1 ] || \
	fail "expected 1 client"
exit_mode 'C1=' q

# --- filter keeping only the bbb client -------------------------------------
$TMUX choose-client -t aaa:0 -F 'C2=#{client_session}' \
	-f '#{==:#{client_session},bbb}' || exit 1
wait_count 'C2=' 1
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'C2=bbb' || \
	fail "bbb client missing when it matches"
printf '%s\n' "$out" | grep -F -q 'C2=aaa' && \
	fail "aaa client shown but does not match"
[ "$(printf '%s\n' "$out" | grep -F -c 'C2=')" -eq 1 ] || \
	fail "expected 1 client"
exit_mode 'C2=' q

# --- no filter shows both clients -------------------------------------------
$TMUX choose-client -t aaa:0 -F 'C3=#{client_session}' || exit 1
wait_count 'C3=' 2
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'C3=aaa' || \
	fail "aaa client missing with no filter"
printf '%s\n' "$out" | grep -F -q 'C3=bbb' || \
	fail "bbb client missing with no filter"
[ "$(printf '%s\n' "$out" | grep -F -c 'C3=')" -eq 2 ] || \
	fail "expected 2 clients"
exit_mode 'C3=' q

# --- filter matching nothing ------------------------------------------------
#
# Everything is shown and the filter indicator reports no matches.
$TMUX choose-client -t aaa:0 -F 'C4=#{client_session}' \
	-f '#{==:#{client_session},nosuch}' || exit 1
wait_count 'C4=' 2
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'C4=aaa' || \
	fail "aaa client missing with no-match filter"
printf '%s\n' "$out" | grep -F -q 'C4=bbb' || \
	fail "bbb client missing with no-match filter"
printf '%s\n' "$out" | grep -F -q 'no matches' || \
	fail "no matches indicator missing"
exit_mode 'C4=' q

# --- Enter runs the default command (detach-client) --------------------------
#
# The filter leaves only the bbb client listed and selected; Enter detaches
# it, leaving only the aaa client attached.
$TMUX choose-client -t aaa:0 -F 'G1=#{client_session}' \
	-f '#{==:#{client_session},bbb}' || exit 1
wait_for 'G1=bbb'
$TMUX send-keys -t aaa:0 Enter || fail "send-keys Enter failed"
wait_clients 1 || fail "bbb client did not detach"
wait_mode aaa:0 0
wait_gone 'G1='
[ "$($TMUX list-clients -F '#{client_session}')" = "aaa" ] || \
	fail "wrong client detached"

exit 0
