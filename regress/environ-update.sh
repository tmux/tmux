#!/bin/sh

# Tests of update-environment handling (environ_update() in environ.c), which
# runs when a client attaches to a session: for each pattern in the session's
# update-environment option, a matching variable in the attaching client's
# environment is copied into the session environment, and a pattern that
# matches nothing clears that name in the session (a NULL-valued entry).
#
# This needs a real attached client with a controllable environment, so - as in
# format-variables.sh - a second server provides one: an inner "tmux attach"
# runs inside a pane of the second server, and the variables to import are set
# in that inner command's own environment.
#
# environ.sh covers the set-environment/show-environment commands themselves.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
# A second server on its own socket hosts the pane that runs the inner client.
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

# check_value $var $expected
#
# Compare show-environment of $var on the session with $expected.
check_value()
{
	out=$($TMUX show-environment -t main "$1" 2>&1)
	if [ "$out" != "$2" ]; then
		echo "show-environment $1 failed."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		cleanup
		exit 1
	fi
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

$TMUX new-session -d -s main -x 80 -y 24 || exit 1

# The session imports MYVAR and ABSENTVAR by exact name and anything matching
# the glob TEST_*; nothing else is imported.
$TMUX set -g update-environment "MYVAR ABSENTVAR TEST_*" || exit 1

# Seed the session so the effect of attaching is visible: MYVAR will be
# overwritten by the client's value and ABSENTVAR will be cleared.
$TMUX set-environment -t main MYVAR oldvalue || exit 1
$TMUX set-environment -t main ABSENTVAR pre-existing || exit 1

# --- attach a client whose environment carries the imported variables ------
#
# MYVAR and TEST_GLOB are present in the inner client's environment; ABSENTVAR
# is deliberately absent; OTHER is present but not named by update-environment.
$TMUX2 new-session -d -x 90 -y 30 \
	"MYVAR=fromclient TEST_GLOB=globval OTHER=nope $TMUX attach -t main" \
	|| fail "could not start inner client"
wait_clients 1 || fail "no client attached to test server"

# MYVAR matched by name and present in the client -> imported (overwrites).
check_value MYVAR "MYVAR=fromclient"
# TEST_GLOB matched by the TEST_* glob and present -> imported.
check_value TEST_GLOB "TEST_GLOB=globval"
# ABSENTVAR named but not in the client environment -> cleared (NULL value,
# printed as -NAME).
check_value ABSENTVAR "-ABSENTVAR"
# OTHER is in the client environment but not named by update-environment, so it
# is not imported at all.
out=$($TMUX show-environment -t main OTHER 2>&1)
[ "$out" = "unknown variable: OTHER" ] || \
	fail "OTHER was imported but should not have been: '$out'"

# --- -E disables the update-environment import -----------------------------
#
# Detach the client (kill its host server), reset the session variables, then
# reattach with -E: the session values must be left untouched.
$TMUX2 kill-server 2>/dev/null
wait_clients 0 || fail "client did not detach"

$TMUX set-environment -t main MYVAR oldvalue2 || exit 1
$TMUX set-environment -t main ABSENTVAR pre2 || exit 1

$TMUX2 new-session -d -x 90 -y 30 \
	"MYVAR=fromclientE $TMUX attach -E -t main" \
	|| fail "could not start inner -E client"
wait_clients 1 || fail "no -E client attached to test server"

# With -E neither variable is touched by the attach.
check_value MYVAR "MYVAR=oldvalue2"
check_value ABSENTVAR "ABSENTVAR=pre2"

if [ "$($TMUX display-message -p alive)" != "alive" ]; then
	fail "server died after update-environment tests"
fi

cleanup
exit 0
