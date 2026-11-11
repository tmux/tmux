#!/bin/sh

# Hide and show are separate operations, with explicit targets or hide history.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

# Report a failed assertion.
fail()
{
	echo "$*" >&2
	exit 1
}

# Run a command and require success.
run()
{
	$TMUX "$@" || fail "failed: $*"
}

# Check a format in the given pane's context.
check()
{
	got=$(run display-message -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

run new-session -d -x 80 -y 24 'sleep 100'
A=$(run display-message -p '#{pane_id}') || exit 1
B=$(run split-window -hdPF '#{pane_id}' 'sleep 100') || exit 1
X=$(run new-pane -dPF '#{pane_id}' -x 20 -y 8 'sleep 100') || exit 1

# Repeated hiding preserves the hidden state and the order of the history.
run resize-pane -h -t "$B"
run resize-pane -h -t "$X"
run resize-pane -h -t "$B"
check "$B" '#{pane_hidden_flag}' 1
run resize-pane -H
check "$X" '#{pane_hidden_flag}:#{pane_active}' 0:1
check "$B" '#{pane_hidden_flag}' 1

# An explicit show restores that target, regardless of the history order.
run resize-pane -h -t "$X"
run resize-pane -H -t "$B"
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1
check "$X" '#{pane_hidden_flag}' 1
run resize-pane -H -t "$B"
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1
run resize-pane -H
check "$X" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Detached shows neither change focus nor consume another pane's history.
run resize-pane -h -t "$B"
run resize-pane -h -t "$X"
run select-pane -t "$A"
run resize-pane -dH -t "$B"
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:0
check "$A" '#{pane_active}' 1
run resize-pane -dH
check "$X" '#{pane_hidden_flag}:#{pane_active}' 0:0
check "$A" '#{pane_active}' 1

# With no target, hide applies to the active pane; show walks the history.
run resize-pane -h
check "$A" '#{pane_hidden_flag}' 1
run resize-pane -H
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:1
run resize-pane -H
check "$A" '#{pane_active}' 1

# An unhide alias uses resize-pane for explicit targets and history.
run set -s 'command-alias[unhide-pane]' 'unhide-pane=resize-pane -H'
run resize-pane -h -t "$B"
run unhide-pane -t "$B"
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1
run resize-pane -h -t "$B"
run unhide-pane
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Selection has no separate history-unhide flag.
for flag in -h -H; do
	if $TMUX select-pane "$flag" >/dev/null 2>&1; then
		fail "select-pane unexpectedly accepted $flag"
	fi
done

exit 0
