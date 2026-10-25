#!/bin/sh

# pane_current_path is the path from the system while it can tell, else the
# path the pane reported with OSC 7 if it is on this host. A pane whose
# process has exited has no path from the system.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)
trap 'stop_server $TMUX; rm -rf "$TMP"' 0 1 15

exit_status=0
mkdir "$TMP/real" "$TMP/a b" || exit 1

$TMUX new-session -d -s test 'sleep 60' || exit 1
$TMUX set-option -g remain-on-exit on || exit 1

# Every case reports a valid OSC 7 string, so pane_path is set when the pane
# has exited.
pane_ready()
{
	[ "$($TMUX display-message -p -t "=$1:" '#{pane_dead}')" = 1 ] &&
	    [ -n "$($TMUX display-message -p -t "=$1:" '#{pane_path}')" ]
}

# Report $2 with OSC 7 from a pane that then exits; expect $3.
check_dead()
{
	name=$1
	$TMUX new-session -d -s "$name" "printf '\\033]7;%s\\033\\\\' '$2'" ||
	    exit 1
	wait_until pane_ready "$name" || exit 1
	got=$($TMUX display-message -p -t "=$name:" '#{pane_current_path}')
	if [ "$got" != "$3" ]; then
		echo "FAIL: $name: pane_current_path is '$got', not '$3'"
		exit_status=1
	fi
}

check_dead empty-host "file://$TMP/real" "$TMP/real"
check_dead localhost "file://localhost$TMP/real" "$TMP/real"
check_dead upper-localhost "file://LOCALHOST$TMP/real" "$TMP/real"
check_dead this-host "file://$(uname -n)$TMP/real" "$TMP/real"
check_dead escaped "file://localhost$TMP/a%20b" "$TMP/a b"
check_dead plain "$TMP/real" "$TMP/real"
check_dead other-host "file://other.invalid$TMP/real" ''
check_dead bad-escape "file://localhost$TMP/a%2" ''
check_dead nul "file://localhost$TMP/a%00b" ''
check_dead relative 'real' ''
check_dead trailing-escape "file://localhost$TMP/a%" ''
check_dead bad-hex "file://localhost$TMP/a%zz" ''
check_dead not-utf8 "file://localhost$TMP/a%FF" ''
check_dead newline "file://localhost$TMP/a%0Ab" ''
check_dead escape "file://localhost$TMP/a%1Bb" ''
check_dead job "file://localhost$TMP/%23(id)" ''

# The rest needs a system which can tell the path of a live process.
$TMUX new-session -d -s probe -c "$TMP/real" 'sleep 60' || exit 1
if [ -z "$($TMUX display-message -p -t =probe: '#{pane_current_path}')" ]
then
	exit $exit_status
fi

# A live pane uses the path from the system, not the one it reported.
$TMUX new-session -d -s live -c "$TMP/real" \
    "printf '\\033]7;file://localhost/nonexistent\\033\\\\'; sleep 60" || exit 1
wait_for_output file://localhost/nonexistent \
    $TMUX display-message -p -t =live: '#{pane_path}' || exit 1
got=$($TMUX display-message -p -t =live: '#{pane_current_path}')
if [ "$got" != "$TMP/real" ]; then
	echo "FAIL: live: pane_current_path is '$got', not '$TMP/real'"
	exit_status=1
fi

exit $exit_status
