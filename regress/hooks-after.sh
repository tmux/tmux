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

	while [ $i -lt 10 ]; do
		value=$($TMUX show -gqv "$option" 2>/dev/null || true)
		[ "$value" = "$expected" ] || \
			fail "expected $option to remain '$expected' but got '$value'"
		i=$((i + 1))
		sleep 0.2
	done
}

$TMUX new -d -s one || fail "new-session one failed"
$TMUX new -d -s two || fail "new-session two failed"

# An after hook fires with the command arguments as formats.
$TMUX set -g @after 0 || fail "set @after failed"
$TMUX set-hook -g after-rename-window \
	'set -gF @after "#{hook}|#{hook_arguments}|#{hook_argument_0}|#{hook_flag_t}"' ||
	fail "set-hook -g after-rename-window failed"
$TMUX rename-window -t one:0 first || fail "rename-window first failed"
wait_for @after 'after-rename-window|-t one:0 first|first|one:0'

# An appended hook command runs after the first.
$TMUX set -g @after2 0 || fail "set @after2 failed"
$TMUX set-hook -ga after-rename-window \
	'set -gF @after2 "#{@after}+2"' ||
	fail "set-hook -ga after-rename-window failed"
$TMUX rename-window -t one:0 second || fail "rename-window second failed"
wait_for @after 'after-rename-window|-t one:0 second|second|one:0'
wait_for @after2 'after-rename-window|-t one:0 second|second|one:0+2'
$TMUX set-hook -gu after-rename-window || fail "set-hook -gu failed"

# A hook command is inserted after the command that fired it, before the next
# command in the same command list.
$TMUX set -g @order '' || fail "set @order failed"
$TMUX set-hook -g after-rename-window \
	'set -gF @order "#{@order}H"' ||
	fail "set-hook -g after-rename-window order failed"
$TMUX rename-window -t one:0 ordered \; set -gF @order '#{@order}N' ||
	fail "rename-window order failed"
wait_for @order 'HN'
$TMUX set-hook -gu after-rename-window || fail "set-hook -gu order failed"

# Repeated flag values are available as numbered hook flag formats.
$TMUX set -g @flags 0 || fail "set @flags failed"
$TMUX set-hook -g after-split-window \
	'set -gF @flags "#{hook_arguments}|#{hook_argument_0}|#{hook_flag_t}|#{hook_flag_e}|#{hook_flag_e_0}|#{hook_flag_e_1}"' ||
	fail "set-hook -g after-split-window failed"
$TMUX split-window -d -t one:0 -e A=1 -e B=2 'sleep 60' ||
	fail "split-window flags failed"
wait_for @flags '-d -e A=1 -e B=2 -t one:0 "sleep 60"|sleep 60|one:0|B=2|A=1|B=2'
$TMUX set-hook -gu after-split-window ||
	fail "set-hook -gu after-split-window failed"

# A session after hook only fires for commands targeting that session and
# the hook commands run with the command target as current state.
$TMUX set -g @safter 0 || fail "set @safter failed"
$TMUX set-hook -t two after-rename-window \
	'set -gF @safter "#{hook}:#{session_name}"' ||
	fail "set-hook -t two after-rename-window failed"
$TMUX rename-window -t one:0 third || fail "rename-window third failed"
assert_unchanged @safter 0
$TMUX rename-window -t two:0 fourth || fail "rename-window fourth failed"
wait_for @safter 'after-rename-window:two'
$TMUX set-hook -u -t two after-rename-window ||
	fail "set-hook -u -t two failed"

# The command-error hook fires when a command fails.
$TMUX set -g @error 0 || fail "set @error failed"
$TMUX set-hook -g command-error 'set -gF @error "#{hook}"' ||
	fail "set-hook -g command-error failed"
if $TMUX rename-window -t nosuchsession:0 x 2>/dev/null; then
	fail "rename-window to missing session succeeded"
fi
wait_for @error 'command-error'
$TMUX set-hook -gu command-error || fail "set-hook -gu command-error failed"

# Commands run from a hook do not fire their own after hooks.
$TMUX set -g @copy 0 || fail "set @copy failed"
$TMUX set-hook -g after-copy-mode 'set -gF @copy "#{hook}"' ||
	fail "set-hook -g after-copy-mode failed"
$TMUX copy-mode -t one:0 || fail "copy-mode failed"
wait_for @copy 'after-copy-mode'
$TMUX send-keys -t one:0 -X cancel || fail "cancel failed"
$TMUX set -g @copy 0 || fail "reset @copy failed"
$TMUX set -g @ran 0 || fail "set @ran failed"
$TMUX set-hook -g after-rename-window 'copy-mode -t one:0' ||
	fail "set-hook -g after-rename-window nested failed"
$TMUX set-hook -ga after-rename-window 'set -g @ran 1' ||
	fail "set-hook -ga after-rename-window nested failed"
$TMUX rename-window -t one:0 fifth || fail "rename-window fifth failed"
wait_for @ran 1
mode=$($TMUX display -pt one:0 '#{pane_in_mode}') ||
	fail "display pane_in_mode failed"
[ "$mode" = 1 ] || fail "hook did not enter copy mode"
assert_unchanged @copy 0

exit 0
