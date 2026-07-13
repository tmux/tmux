#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT=$(mktemp -d)
TMUX_TMPDIR="$OUT"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	$TMUX kill-server 2>/dev/null || true
	rm -rf "$OUT"
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null || true
	rm -rf "$OUT"
}
trap cleanup EXIT

wait_channel()
{
	channel=$1

	if command -v timeout >/dev/null 2>&1; then
		timeout 10 $TMUX wait-for "$channel" ||
			fail "wait-for $channel timed out"
		return
	fi

	$TMUX wait-for "$channel" &
	pid=$!
	i=0
	while kill -0 "$pid" 2>/dev/null; do
		[ $i -lt 50 ] || {
			kill "$pid" 2>/dev/null || true
			fail "wait-for $channel timed out"
		}
		i=$((i + 1))
		sleep 0.2
	done
	wait "$pid" || fail "wait-for $channel failed"
}

wait_for()
{
	option=$1
	expected=$2
	i=0

	while [ $i -lt 30 ]; do
		value=$($TMUX show -gqv "$option" 2>/dev/null || true)
		[ "$value" = "$expected" ] && return 0
		i=$((i + 1))
		sleep 0.2
	done
	fail "expected $option to be '$expected' but got '$value'"
}

assert_unchanged()
{
	option=$1
	expected=$2
	count=${3:-15}
	i=0

	while [ $i -lt "$count" ]; do
		value=$($TMUX show -gqv "$option" 2>/dev/null || true)
		[ "$value" = "$expected" ] ||
			fail "expected $option to remain '$expected' but got '$value'"
		i=$((i + 1))
		sleep 0.2
	done
}

wait_list()
{
	name=$1
	i=0

	while [ $i -lt 50 ]; do
		value=$($TMUX wait-for -E -l "$name" 2>/dev/null || true)
		if [ -n "$value" ]; then
			printf '%s\n' "$value" | sed -n '1p'
			return
		fi
		i=$((i + 1))
		sleep 0.2
	done
	fail "wait-for -E -l $name found no waiters"
}

$TMUX new -d -s one || fail "new-session one failed"
$TMUX new -d -s two || fail "new-session two failed"

$TMUX set -g @event_seen 0 || fail "set @event_seen failed"
$TMUX wait-for -E @manual-event \; set -g @event_seen 1 \; \
	wait-for -S she-event &
event_pid=$!
wait_list @manual-event >/dev/null

$TMUX set-hook -E @manual-event || fail "set-hook -E @manual-event failed"
wait_channel she-event
wait "$event_pid" || fail "wait-for -E @manual-event failed"
wait_for @event_seen 1

$TMUX set-hook -E @no-sink || fail "set-hook -E @no-sink failed"

pane=$($TMUX display -pt two:0.0 '#{pane_id}') ||
	fail "display-message pane failed"
$TMUX set -g @hook_seen 0 || fail "set @hook_seen failed"
$TMUX set-hook -g @manual-hook \
	'set -gF @hook_seen "#{hook}:#{session_name}:#{window_index}:#{pane_id}"' ||
	fail "set-hook @manual-hook failed"

$TMUX set-hook -E -t two:0.0 @manual-hook ||
	fail "set-hook -E @manual-hook failed"
wait_for @hook_seen "@manual-hook:two:0:$pane"

$TMUX set -g @r_hook 0 || fail "set @r_hook failed"
$TMUX set -g @r_event 0 || fail "set @r_event failed"
$TMUX set-hook -g @manual-r 'set -g @r_hook 1' ||
	fail "set-hook @manual-r failed"
$TMUX wait-for -E @manual-r \; set -g @r_event 1 \; wait-for -S she-r &
r_pid=$!
r_client=$(wait_list @manual-r)

$TMUX set-hook -R @manual-r || fail "set-hook -R @manual-r failed"
wait_for @r_hook 1
assert_unchanged @r_event 0 5

$TMUX wait-for -E -w "$r_client" @manual-r ||
	fail "wait-for -E -w @manual-r failed"
wait_channel she-r
wait "$r_pid" || fail "wait-for -E @manual-r failed"

if $TMUX set-hook -E window-renamed 2>/dev/null; then
	fail "set-hook -E window-renamed succeeded"
fi

exit 0
