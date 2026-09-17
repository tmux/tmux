#!/bin/sh

# session_activity_flag, session_bell_flag and session_silence_flag are 1
# if any window in the session has the alert, not only the current window.
# The current window (index 0) has all monitoring off so only the other
# windows raise alerts.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	$TMUX kill-server 2>/dev/null
	exit 1
}

wait_for_fmt()
{
	target=$1
	fmt=$2
	expected=$3
	i=0

	while [ $i -lt 30 ]; do
		value=$($TMUX display -pt "$target" "$fmt" 2>/dev/null)
		[ "$value" = "$expected" ] && return 0
		i=$((i + 1))
		sleep 0.2
	done
	fail "expected $fmt for $target to be '$expected' but got '$value'"
}

$TMUX kill-server 2>/dev/null
trap '$TMUX kill-server 2>/dev/null' EXIT

$TMUX new -d -s s -n w0 cat || fail "new-session failed"
$TMUX set -wt s:w0 monitor-bell off || fail "set monitor-bell failed"
$TMUX neww -d -t s:1 -n act cat || fail "new-window act failed"
$TMUX neww -d -t s:2 -n bell cat || fail "new-window bell failed"
$TMUX neww -d -t s:3 -n sil cat || fail "new-window sil failed"
$TMUX set -wt s:bell monitor-bell off || fail "set monitor-bell failed"
$TMUX set -wt s:sil monitor-bell off || fail "set monitor-bell failed"
$TMUX set -wt s:act monitor-bell off || fail "set monitor-bell failed"

wait_for_fmt s: '#{session_activity_flag}' 0
wait_for_fmt s: '#{session_bell_flag}' 0
wait_for_fmt s: '#{session_silence_flag}' 0

$TMUX set -wt s:act monitor-activity on || fail "set monitor-activity failed"
$TMUX send-keys -t s:act -l x || fail "send-keys activity failed"
wait_for_fmt s:act '#{window_activity_flag}' 1
wait_for_fmt s:w0 '#{window_activity_flag}' 0
wait_for_fmt s: '#{session_activity_flag}' 1
[ "$($TMUX ls -F '#{session_activity_flag}')" = 1 ] ||
	fail "list-sessions session_activity_flag is not 1"

$TMUX set -wt s:bell monitor-bell on || fail "set monitor-bell failed"
$TMUX send-keys -t s:bell -H 07 0a || fail "send-keys bell failed"
wait_for_fmt s:bell '#{window_bell_flag}' 1
wait_for_fmt s:w0 '#{window_bell_flag}' 0
wait_for_fmt s: '#{session_bell_flag}' 1

$TMUX set -wt s:sil monitor-silence 1 || fail "set monitor-silence failed"
wait_for_fmt s:sil '#{window_silence_flag}' 1
wait_for_fmt s:w0 '#{window_silence_flag}' 0
wait_for_fmt s: '#{session_silence_flag}' 1

exit 0
