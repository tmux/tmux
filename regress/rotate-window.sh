#!/bin/sh

# Rotation preserves excluded positions, active position, and resize event order.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
trap 'stop_server $TMUX; rm -rf "$DIR"' 0
trap 'exit 1' 1 2 15

# Report a failed assertion.
fail()
{
	echo "$*" >&2
	exit 1
}

# Run a command against the isolated server.
run()
{
	$TMUX "$@" || fail "failed: $*"
}

# Compare a format in the given pane's context.
check()
{
	got=$(run display -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# Capture pane order independently of geometry and focus.
order()
{
	run list-panes -F '#{pane_id}' | paste -sd ' ' -
}

# Check the complete pane order.
check_order()
{
	got=$(order) || exit 1
	[ "$got" = "$1" ] || fail "pane order: got '$got', expected '$1'"
}

# Capture floating geometry and stacking order in a fixed pane order.
floats()
{
	for float in "$F0" "$F1" "$F2" "$F3"; do
		run display -p -t "$float" \
		    "#{pane_id}:#{pane_floating_flag}:#{pane_z}:$geometry" ||
		    exit 1
	done
}

# Rotation must leave floating geometry and stacking order unchanged.
check_floats()
{
	got=$(floats) || exit 1
	[ "$got" = "$floating" ] || fail 'floating positions or stacking changed'
}

# Capture enough state to check rotations with fewer than two tiled panes.
state()
{
	run list-panes -F \
	    "#{pane_id}:#{pane_floating_flag}:#{pane_active}:$geometry"
}

A=$(run new-session -dP -F '#{pane_id}' -x121 -y61 'exec cat') || exit 1
B=$(run split-window -dvP -F '#{pane_id}' -t "$A" -l24 'exec cat') || exit 1
C=$(run split-window -dvP -F '#{pane_id}' -t "$B" -l7 'exec cat') || exit 1
F0=$(run new-pane -bdP -F '#{pane_id}' -t "$A" -x20 -y8 'exec cat') || exit 1
F1=$(run new-pane -bdP -F '#{pane_id}' -t "$B" -x20 -y8 'exec cat') || exit 1
F2=$(run new-pane -bdP -F '#{pane_id}' -t "$C" -x20 -y8 'exec cat') || exit 1
F3=$(run new-pane -dP -F '#{pane_id}' -t "$C" -x20 -y8 'exec cat') || exit 1
geometry='#{pane_left}:#{pane_top}:#{pane_width}:#{pane_height}'
ga=$(run display -p -t "$A" "$geometry") || exit 1
gb=$(run display -p -t "$B" "$geometry") || exit 1
gc=$(run display -p -t "$C" "$geometry") || exit 1
check_order "$F0 $A $F1 $B $F2 $C $F3"
run set-hook -g pane-resized \
    'set -gF @rotation-events "#{@rotation-events} #{hook_pane}"'

# Unequal heights make every tiled move emit one resize event. Check all
# active tiled positions, as well as an active float which must stay active.
for active in "$A" "$B" "$C" "$F1"; do
	run select-pane -t "$active"
	floating=$(floats) || exit 1
	case $active in
	"$A") next=$B ;;
	"$B") next=$C ;;
	"$C") next=$A ;;
	"$F1") next=$F1 ;;
	esac
	run set -g @rotation-events ''
	run rotate-window -U
	check_order "$F0 $B $F1 $C $F2 $A $F3"
	check "$B" "$geometry" "$ga"
	check "$C" "$geometry" "$gb"
	check "$A" "$geometry" "$gc"
	check "$next" '#{pane_active}' 1
	check_floats
	wait_for_output " $B $C $A" run show -gv @rotation-events ||
	    fail 'upward resize event order changed'

	run set -g @rotation-events ''
	run rotate-window -D
	check_order "$F0 $A $F1 $B $F2 $C $F3"
	check "$A" "$geometry" "$ga"
	check "$B" "$geometry" "$gb"
	check "$C" "$geometry" "$gc"
	check "$active" '#{pane_active}' 1
	check_floats
	wait_for_output " $A $B $C" run show -gv @rotation-events ||
	    fail 'downward resize event order changed'
done
run set-hook -gu pane-resized

# With one tiled pane, neither its position nor the active float changes.
run break-pane -Wd -s "$B"
run break-pane -Wd -s "$C"
for active in "$A" "$F1"; do
	run select-pane -t "$active"
	before=$(state) || exit 1
	for direction in -U -D; do
		run rotate-window "$direction"
		after=$(state) || exit 1
		[ "$after" = "$before" ] || fail 'one-tile rotation changed state'
	done
done

# With no tiled panes, rotation leaves the list, geometry, and focus unchanged.
run break-pane -Wd -s "$A"
before=$(state) || exit 1
for direction in -U -D; do
	run rotate-window "$direction"
	after=$(state) || exit 1
	[ "$after" = "$before" ] || fail 'all-floating rotation changed state'
done

exit 0
