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

assert_unchanged()
{
	option=$1
	expected=$2
	i=0

	while [ $i -lt 15 ]; do
		value=$($TMUX show -gqv "$option" 2>/dev/null || true)
		[ "$value" = "$expected" ] || \
			fail "expected $option to remain '$expected' but got '$value'"
		i=$((i + 1))
		sleep 0.2
	done
}

$TMUX new -d -s one || fail "new-session failed"

$TMUX set -g @seen 0 || fail "set @seen failed"
$TMUX set-hook -g -B '@session-name::#{session_name}' \
	'set -g @seen "#{hook}:#{hook_value}"' ||
	fail "set-hook -B failed"
shown=$($TMUX show-hooks -g -B @session-name) ||
	fail "show-hooks -B failed"
[ "$shown" = '@session-name::#{session_name}' ] ||
	fail "unexpected show-hooks -B output: $shown"
shown=$($TMUX show-hooks -g) ||
	fail "show-hooks -g failed"
echo "$shown" | grep -q '^@session-name ' ||
	fail "show-hooks -g did not show monitor hook: $shown"
assert_unchanged @seen 0

$TMUX rename-session two || fail "rename-session two failed"
wait_for @seen '@session-name:two'

$TMUX set -g @seen-last 0 || fail "set @seen-last failed"
$TMUX set-hook -g -B '@session-name::#{session_name}' \
	'set -g @seen-last "#{hook_last}->#{hook_value}"' ||
	fail "set-hook -B replacement failed"
assert_unchanged @seen-last 0
$TMUX rename-session one || fail "rename-session one failed"
wait_for @seen-last 'two->one'

$TMUX set-hook -gu -B @session-name || fail "set-hook -gu -B failed"
shown=$($TMUX show-hooks -g -B @session-name) ||
	fail "show-hooks -B after remove failed"
[ -z "$shown" ] || fail "show-hooks -B showed removed monitor: $shown"
last=$($TMUX show -gqv @seen-last)
$TMUX rename-session three || fail "rename-session three failed"
assert_unchanged @seen-last "$last"

$TMUX set -gu @value || fail "unset @value failed"
$TMUX set -g @empty-seen 0 || fail "set @empty-seen failed"
$TMUX set-hook -g -B '@empty::#{@value}' \
	'set -g @empty-seen "#{hook_last}->#{hook_value}"' ||
	fail "set-hook -B empty failed"
assert_unchanged @empty-seen 0
$TMUX set -g @value changed || fail "set @value failed"
wait_for @empty-seen '->changed'

if $TMUX set-hook -g -B 'bad::#{session_name}' 'display-message x' \
	>"$OUT/bad.out" 2>"$OUT/bad.err"; then
	fail "non-@ monitor hook name was accepted"
fi

session=$($TMUX display -p '#{session_id}')
window=$($TMUX display -p '#{window_id}')
pane=$($TMUX display -p '#{pane_id}')
pane_number=${pane#%}

$TMUX set -gu @pane-value || fail "unset @pane-value failed"
$TMUX set -g @pane-seen 0 || fail "set @pane-seen failed"
$TMUX set-hook -g -B "@pane:%$pane_number:#{pane_width}" \
	'set -g @pane-seen "#{hook_session}:#{hook_window}:#{hook_window_index}:#{hook_pane}:#{hook_value}"' ||
	fail "set-hook -B pane selector failed"
assert_unchanged @pane-seen 0
$TMUX set-hook -g -B "@pane:%$pane_number:#{@pane-value}" \
	'set -g @pane-seen "#{hook_session}:#{hook_window}:#{hook_window_index}:#{hook_pane}:#{hook_value}"' ||
	fail "set-hook -B pane replacement failed"
assert_unchanged @pane-seen 0
$TMUX set -g @pane-value changed || fail "set @pane-value failed"
wait_for @pane-seen "$session:$window:0:$pane:changed"

$TMUX set -g @exact-value one || fail "set @exact-value failed"
$TMUX set -g @exact-seen 0 || fail "set @exact-seen failed"
$TMUX set -gw @foo 'set -g @exact-seen inherited' ||
	fail "set global @foo failed"
$TMUX set-hook -w -B '@foo::#{@exact-value}' ||
	fail "set-hook -B exact scope monitor failed"
assert_unchanged @exact-seen 0
$TMUX set -g @exact-value two || fail "set @exact-value two failed"
assert_unchanged @exact-seen 0
$TMUX set-hook -w -B '@foo::#{@exact-value}' \
	'set -g @exact-seen "#{hook_value}"' ||
	fail "set-hook -B exact scope command failed"
assert_unchanged @exact-seen 0
$TMUX set -g @exact-value three || fail "set @exact-value three failed"
wait_for @exact-seen three

target_pane=$($TMUX splitw -P -F '#{pane_id}') ||
	fail "split-window failed"
$TMUX set -g @target-pane 0 || fail "set @target-pane failed"
$TMUX set-hook -g -B '@target:%*:#{@target-value}' \
	'set -g @target-pane "#{pane_id}"' ||
	fail "set-hook -B target pane failed"
assert_unchanged @target-pane 0
$TMUX set -pt "$target_pane" @target-value changed ||
	fail "set pane @target-value failed"
wait_for @target-pane "$target_pane"

$TMUX new -d -s zzz-survivor || fail "new survivor session failed"
$TMUX set -g @global-after-destroy 0 ||
	fail "set @global-after-destroy failed"
$TMUX set-hook -g -t three -B '@global-after-destroy-session::#{session_name}' \
	'set -g @global-after-destroy "#{hook_last}->#{hook_value}"' ||
	fail "set-hook -B global after destroy failed"
assert_unchanged @global-after-destroy 0
$TMUX kill-session -t three || fail "kill destroyed monitor session failed"
wait_for @global-after-destroy 'three->zzz-survivor'

exit 0
