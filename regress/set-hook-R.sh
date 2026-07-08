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

$TMUX new -d -s one || fail "new-session one failed"
$TMUX new -d -s two || fail "new-session two failed"

pane=$($TMUX display -pt two:0.0 '#{pane_id}') ||
	fail "display-message pane failed"

$TMUX set -g @seen 0 || fail "set @seen failed"
$TMUX set-hook -g @manual \
	'set -gF @seen "#{hook}:#{session_name}:#{window_index}:#{pane_id}"' ||
	fail "set-hook @manual failed"

[ "$($TMUX show -gqv @seen)" = 0 ] ||
	fail "hook ran before set-hook -R"

$TMUX set-hook -g -R -t two:0.0 @manual ||
	fail "set-hook -R @manual failed"
wait_for @seen "@manual:two:0:$pane"

exit 0
