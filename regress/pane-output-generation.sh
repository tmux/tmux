#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

test_dir=$(mktemp -d "${TMPDIR:-/tmp}/tmux-output-generation.XXXXXXXXXX") ||
	exit 1
test_socket="$test_dir/tmux.sock"
test_name="output-generation-$$"
test_session=""

fail()
{
	echo "$*" >&2
	exit 1
}

test_tmux()
{
	"$TEST_TMUX" -S "$test_socket" "$@"
}

cleanup()
{
	status=$?
	trap - 0 1 2 15

	sessions=$(test_tmux list-sessions -F '#{session_id}|#{session_name}' \
	    2>/dev/null) || sessions=""
	if [ -n "$sessions" ]; then
		if [ -z "$test_session" ] || \
		    [ "$sessions" != "$test_session|$test_name" ]; then
			echo "unexpected sessions on test socket $test_socket; refusing cleanup" >&2
			echo "$sessions" >&2
			exit 1
		fi
		test_tmux kill-session -t "$test_session" >/dev/null 2>&1 ||
			exit 1
	fi

	if test_tmux list-sessions >/dev/null 2>&1; then
		echo "test server still running on $test_socket; refusing cleanup" >&2
		exit 1
	fi
	[ ! -e "$test_socket" ] || rm -f "$test_socket"
	rmdir "$test_dir" || exit 1
	exit "$status"
}

generation()
{
	test_tmux display-message -p -t "$1" '#{pane_output_generation}'
}

wait_for_new_generation()
{
	pane=$1
	before=$2
	i=0
	while [ "$i" -lt 100 ]; do
		after=$(generation "$pane") || exit 1
		[ "$after" != "$before" ] && {
			echo "$after"
			return 0
		}
		sleep 0.01
		i=$((i + 1))
	done
	echo "pane $pane generation remained $before" >&2
	return 1
}

trap cleanup 0 1 2 15

test_session=$(test_tmux -f /dev/null new-session -d -P \
	-F '#{session_id}' -s "$test_name" 'exec sleep 1000') ||
	exit 1
pane0=$(test_tmux display-message -p -t "$test_session":0.0 '#{pane_id}') ||
	exit 1
pane1=$(test_tmux split-window -d -P -F '#{pane_id}' \
	-t "$test_session":0 'exec sleep 1000') || exit 1

generation0=$(generation "$pane0") || exit 1
generation1=$(generation "$pane1") || exit 1
[ -n "$generation0" ] || fail "initial generation for $pane0 is empty"
[ -n "$generation1" ] || fail "initial generation for $pane1 is empty"

test_tmux capture-pane -p -t "$pane0" >/dev/null || exit 1
[ "$(generation "$pane0")" = "$generation0" ] ||
	fail "capture-pane changed generation for $pane0"

pane0_tty=$(test_tmux display-message -p -t "$pane0" '#{pane_tty}') ||
	exit 1
printf 'visible output' >"$pane0_tty" || exit 1
generation0=$(wait_for_new_generation "$pane0" "$generation0") || exit 1
[ "$(generation "$pane1")" = "$generation1" ] ||
	fail "output in $pane0 changed generation for $pane1"

printf '\033]2;control-only output\007' >"$pane0_tty" || exit 1
wait_for_new_generation "$pane0" "$generation0" >/dev/null || exit 1

exit 0
