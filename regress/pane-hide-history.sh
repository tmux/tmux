#!/bin/sh

# Restore hidden panes in reverse order and activate each restored pane.

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
run resize-pane -h -t "$B"
run resize-pane -h -t "$X"
run resize-pane -Z -t "$A"
run resize-pane -h -t "$A"
run resize-pane -H
check "$A" '#{pane_hidden_flag}:#{pane_zoomed_flag}:#{pane_active}' 0:1:1
check "$X" '#{pane_hidden_flag}' 1
check "$B" '#{pane_hidden_flag}' 1
run resize-pane -H
check "$X" '#{pane_hidden_flag}:#{pane_active}' 0:1
run resize-pane -H
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1
check "$A" '#{pane_hidden_flag}:#{pane_zoomed_flag}' 1:1

# Selecting the restored tile hid its fullscreen cover. The next call restores
# it.
run resize-pane -H
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:1
run resize-pane -H
check "$A" '#{pane_active}' 1

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
run resize-pane -H
check "$C" '#{pane_hidden_flag}:#{pane_active}' 0:1
check "$A" '#{pane_hidden_flag}' 1
run resize-pane -H
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Deletion drops history; swapping carries it with the hidden position.
run resize-pane -h -t "$C"
run kill-pane -t "$C"
run resize-pane -h -t "$B"
run swap-pane -d -s "$A" -t "$B"
check "$A" '#{pane_hidden_flag}' 1
check "$B" '#{pane_hidden_flag}' 0
run resize-pane -H
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Desktop restoration leaves independently hidden panes in the history.
run resize-pane -h -t "$B"
run resize-pane -C
check "$X" '#{pane_hidden_flag}' 1
run resize-pane -C
check "$X" '#{pane_hidden_flag}' 0
check "$B" '#{pane_hidden_flag}' 1
run resize-pane -H
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Detached restoration leaves focus alone and removes the pane from history.
run resize-pane -h -t "$A"
run resize-pane -dH
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:0
check "$B" '#{pane_active}' 1
run resize-pane -h -t "$A"
run resize-pane -dH -t "$A"
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:0
check "$B" '#{pane_active}' 1

# Ordinary selection still shows a hidden pane and runs the selection hook.
run set-hook -g after-select-pane 'set -gF @restored-pane "#{pane_id}"'
run resize-pane -h -t "$A"
got=$(run select-pane -t "$A" ';' display-message -p '#{pane_id}') || exit 1
[ "$got" = "$A" ] || fail "restored command target is '$got', expected '$A'"
check "$A" '#{@restored-pane}' "$A"

# An explicit target in another window leaves the current history alone.
run resize-pane -h -t "$B"
Y=$(run new-window -dPF '#{pane_id}' -n other) || exit 1
run resize-pane -h -t "$Y"
run resize-pane -H -t "$Y"
check "$Y" '#{pane_hidden_flag}:#{pane_active}' 0:1
check "$B" '#{pane_hidden_flag}' 1
run resize-pane -H
check "$B" '#{pane_hidden_flag}:#{pane_active}' 0:1

# -d retains its existing input-disabling meaning on select-pane.
run resize-pane -h -t "$A"
run select-pane -d -t "$A"
check "$A" '#{pane_hidden_flag}:#{pane_input_off}' 1:1
check "$B" '#{pane_active}' 1
run select-pane -e -t "$A"
run resize-pane -H
check "$A" '#{pane_hidden_flag}:#{pane_input_off}:#{pane_active}' 0:0:1

exit 0
