#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX new -d
$TMUX set -g remain-on-exit on

do_test() {
	$TMUX splitw "printf '$1'"
	sleep 0.25
	pbs="$($TMUX display -p '#{pane_progress_bar_state}')"
	pbp="$($TMUX display -p '#{pane_progress_bar_progress}')"
	$TMUX kill-pane
	[ "$pbs" != "$2" ] && \
		printf "test '%s' expected: $2, actual: $pbs\n" "$1" && return 1
	[ "$pbp" != "$3" ] && \
		printf "test '%s' expected: $3, actual: $pbp\n" "$1" && return 1
	return 0
}

# Initial state
do_test '' 'hidden' '0' || exit 1

# Zero progress
do_test '\033]9;4;0;0\007' 'hidden' '0' || exit 1
do_test '\033]9;4;1;0\007' 'normal' '0' || exit 1
do_test '\033]9;4;2;0\007' 'error' '0' || exit 1
do_test '\033]9;4;3;0\007' 'indeterminate' '0' | exit 1
do_test '\033]9;4;4;0\007' 'paused' '0' || exit 1

# 1% progress
do_test '\033]9;4;0;1\007' 'hidden' '1' || exit 1
do_test '\033]9;4;1;1\007' 'normal' '1' || exit 1
do_test '\033]9;4;2;1\007' 'error' '1' || exit 1
do_test '\033]9;4;3;1\007' 'indeterminate' '0' | exit 1
do_test '\033]9;4;4;1\007' 'paused' '1' || exit 1

# 50% progress
do_test '\033]9;4;0;50\007' 'hidden' '50' || exit 1
do_test '\033]9;4;1;50\007' 'normal' '50' || exit 1
do_test '\033]9;4;2;50\007' 'error' '50' || exit 1
do_test '\033]9;4;3;50\007' 'indeterminate' '0' | exit 1
do_test '\033]9;4;4;50\007' 'paused' '50' || exit 1

# 100% progress
do_test '\033]9;4;0;100\007' 'hidden' '100' || exit 1
do_test '\033]9;4;1;100\007' 'normal' '100' || exit 1
do_test '\033]9;4;2;100\007' 'error' '100' || exit 1
do_test '\033]9;4;3;100\007' 'indeterminate' '0' | exit 1
do_test '\033]9;4;4;100\007' 'paused' '100' || exit 1

# Short sequences
do_test '\033]9;4;1;50\007\033]9;4\007' 'hidden' '0' || exit 1
do_test '\033]9;4;1;50\007\033]9;4;\007' 'hidden' '0' || exit 1
do_test '\033]9;4;1;50\007\033]9;4;4\007' 'paused' '50' || exit 1
do_test '\033]9;4;1;50\007\033]9;4;4;\007' 'paused' '50' || exit 1

# Invalid codes, should be ignored
do_test '\033]9;4;foo\007' 'hidden' '0' || exit 1
do_test '\033]9;4;foo;10\007' 'hidden' '0' || exit 1
do_test '\033]9;4;1;foo\007' 'hidden' '0' || exit 1
do_test '\033]9;4;5;10\007' 'hidden' '0' || exit 1
do_test '\033]9;4;50;10\007' 'hidden' '0' || exit 1
do_test '\033]9;4;1;101\007' 'hidden' '0' || exit 1
do_test '\033]9;4;1;1000\007' 'hidden' '0' || exit 1

$TMUX -f/dev/null kill-server 2>/dev/null
exit 0
