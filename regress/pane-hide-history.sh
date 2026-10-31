#!/bin/sh

# Restore hidden panes in reverse order without hiding fullscreen covers again.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	exit 1
}

run()
{
	$TMUX "$@" || fail "failed: $*"
}

check()
{
	got=$(run display-message -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

run new-session -d -x 80 -y 24
A=$(run display-message -p '#{pane_id}') || exit 1
B=$(run split-window -hdPF '#{pane_id}') || exit 1
X=$(run new-pane -dPF '#{pane_id}' -x 20 -y 8 '') || exit 1

# Tile, ordinary float and fullscreen tile share one history.
run resize-pane -H -t "$B"
run resize-pane -H -t "$X"
run resize-pane -Z -t "$A"
run resize-pane -H -t "$A"
run resize-pane -h
check "$A" '#{pane_hidden_flag}:#{pane_zoomed_flag}:#{pane_active}' 0:1:1
check "$X" '#{pane_hidden_flag}' 1
check "$B" '#{pane_hidden_flag}' 1
run resize-pane -h
check "$X" '#{pane_hidden_flag}:#{pane_active}' 0:1
run resize-pane -h
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:0
check "$A" '#{pane_hidden_flag}:#{pane_zoomed_flag}' 0:1
check "$X" '#{pane_active}' 1
run resize-pane -h
check "$X" '#{pane_active}' 1

# Selecting a covered tile still hides its fullscreen covers.
run select-pane -t "$B"
check "$A" '#{pane_hidden_flag}:#{pane_zoomed_flag}' 1:1
check "$B" '#{pane_active}' 1

# Marking a hidden pane changes neither focus nor its hidden state.
run select-pane -m -t "$A"
check "$A" '#{pane_marked}:#{pane_hidden_flag}:#{pane_active}' 1:1:0
check "$B" '#{pane_active}' 1
run select-pane -M

# A detached split of a hidden fullscreen tile leaves both panes hidden.
C=$(run split-window -dPF '#{pane_id}' -t "$A") || exit 1
check "$A" '#{pane_hidden_flag}:#{pane_zoomed_flag}:#{pane_floating_flag}' 1:0:0
check "$C" '#{pane_hidden_flag}:#{pane_active}' 1:0
check "$B" '#{pane_active}' 1
run resize-pane -h
check "$C" '#{pane_hidden_flag}:#{pane_active}' 0:1
check "$A" '#{pane_hidden_flag}' 1
run resize-pane -h
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Deletion drops history; swapping carries it with the hidden position.
run resize-pane -H -t "$C"
run kill-pane -t "$C"
run resize-pane -H -t "$B"
run swap-pane -d -s "$A" -t "$B"
check "$A" '#{pane_hidden_flag}' 1
check "$B" '#{pane_hidden_flag}' 0
run resize-pane -h
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Desktop restoration leaves independently hidden panes in the history.
run resize-pane -H -t "$B"
run resize-pane -aH
check "$X" '#{pane_hidden_flag}' 1
run resize-pane -aH
check "$X" '#{pane_hidden_flag}' 0
check "$B" '#{pane_hidden_flag}' 1
run resize-pane -h
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Detached restoration leaves focus alone and removes the pane from history.
run resize-pane -H -t "$A"
run resize-pane -dh
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:0
check "$B" '#{pane_active}' 1
run resize-pane -H -t "$A"
run resize-pane -dH -t "$A"
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:0
check "$B" '#{pane_active}' 1

exit 0
