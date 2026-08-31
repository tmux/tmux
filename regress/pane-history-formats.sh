#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

test_dir=$(mktemp -d "${TMPDIR:-/tmp}/tmux-history-formats.XXXXXXXXXX") ||
	exit 1
test_socket="$test_dir/tmux.sock"
test_name="history-formats-$$"
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

history_value()
{
	test_tmux display-message -p -t "$1" "#{$2}"
}

wait_for_capture()
{
	pane=$1
	value=$2
	i=0
	while [ "$i" -lt 100 ]; do
		test_tmux capture-pane -p -t "$pane" | grep -Fq "$value" &&
			return 0
		sleep 0.01
		i=$((i + 1))
	done
	echo "pane $pane did not show $value" >&2
	return 1
}

trap cleanup 0 1 2 15

test_session=$(test_tmux -f /dev/null new-session -d -P \
	-F '#{session_id}' -x 20 -y 5 -s "$test_name" 'exec sleep 1000') ||
	exit 1
test_tmux set-option -g history-limit 5 || exit 1
test_tmux respawn-pane -k -t "$test_session":0.0 'exec sleep 1000' || exit 1
pane=$(test_tmux display-message -p -t "$test_session":0.0 '#{pane_id}') ||
	exit 1
pane_tty=$(test_tmux display-message -p -t "$pane" '#{pane_tty}') || exit 1

added=$(history_value "$pane" history_added) || exit 1
collected=$(history_value "$pane" history_collected) || exit 1
generation=$(history_value "$pane" history_generation) || exit 1
hsize=$(history_value "$pane" history_size) || exit 1
[ "$added|$collected|$generation|$hsize" = "0|0|0|0" ] ||
	fail "initial history values are $added|$collected|$generation|$hsize"

i=1
while [ "$i" -le 20 ]; do
	printf 'line-%s\n' "$i"
	i=$((i + 1))
done >"$pane_tty" || exit 1
wait_for_capture "$pane" line-20 || exit 1

added=$(history_value "$pane" history_added) || exit 1
collected=$(history_value "$pane" history_collected) || exit 1
hsize=$(history_value "$pane" history_size) || exit 1
[ "$added" -gt 0 ] || fail "history_added did not advance"
[ "$collected" -gt 0 ] || fail "history_collected did not advance"
[ "$hsize" -eq 5 ] || fail "history_size is $hsize, not 5"
[ $((added - collected)) -eq "$hsize" ] ||
	fail "history counters do not reconcile: $added - $collected != $hsize"

test_tmux clear-history -t "$pane" || exit 1
[ "$(history_value "$pane" history_size)" -eq 0 ] ||
	fail "clear-history did not empty history"
[ "$(history_value "$pane" history_added)" -eq "$added" ] ||
	fail "clear-history changed history_added"
[ "$(history_value "$pane" history_collected)" -eq "$collected" ] ||
	fail "clear-history changed history_collected"
cleared_generation=$(history_value "$pane" history_generation) || exit 1
[ "$cleared_generation" -gt "$generation" ] ||
	fail "clear-history did not advance history_generation"

i=1
while [ "$i" -le 20 ]; do
	printf 'after-%s-abcdefghijklmnop\n' "$i"
	i=$((i + 1))
done >"$pane_tty" || exit 1
wait_for_capture "$pane" after-20 || exit 1

added_after=$(history_value "$pane" history_added) || exit 1
collected_after=$(history_value "$pane" history_collected) || exit 1
hsize=$(history_value "$pane" history_size) || exit 1
[ $((added_after - added - collected_after + collected)) -eq "$hsize" ] ||
	fail "post-clear history counters do not reconcile"
[ "$(history_value "$pane" history_generation)" -eq \
	"$cleared_generation" ] || fail "ordinary output changed history_generation"

test_tmux resize-window -t "$test_session":0 -x 10 -y 5 || exit 1
reflow_generation=$(history_value "$pane" history_generation) || exit 1
[ "$reflow_generation" -gt "$cleared_generation" ] ||
	fail "width reflow did not advance history_generation"

exit 0
