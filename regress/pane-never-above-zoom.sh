#!/bin/sh

# Restricted floats stay below fullscreen panes until explicitly selected.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
trap 'stop_server $TMUX; rm -rf "$DIR"' 0
trap 'exit 1' 1 2 15

# Report the failing assertion.
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

# Compare a pane format with its expected value.
check()
{
	got=$(run display -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# Read the saved stack index, which includes covered floating panes.
stack_index()
{
	run display -p -t "$1" '#{window_layout}' |
	    sed -n 's/.*"z":\([0-9][0-9]*\),"I":"'"$1"'".*/\1/p'
}

# Check that the first pane is strictly in front of the second.
above()
{
	front=$(stack_index "$1")
	back=$(stack_index "$2")
	[ -n "$front" ] && [ -n "$back" ] || fail 'missing stack index'
	[ "$front" -lt "$back" ] || fail "$1 is not above $2"
}

# Start with two tiled panes.
reset()
{
	stop_server $TMUX || fail 'could not stop previous server'
	A=$(run new-session -dP -F '#{pane_id}' -x80 -y24 'exec cat') || exit 1
	B=$(run split-window -dhP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
}

reset
N=$(run new-pane -NdP -F '#{pane_id}' -t "$A" -x20 -y8 'exec cat') || exit 1
case $(run display -p -t "$N" '#{pane_flags}') in
*N*) ;;
*) fail 'restricted pane has no N flag' ;;
esac
run resize-pane -Z -t "$A"
F=$(run new-pane -dP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
run resize-pane -Z -t "$B"

# Explicit stacking must respect every fullscreen pane in either direction.
for position in front back forward backward forward-loop backward-loop; do
	run move-pane -t "$N" -P "$position"
	above "$A" "$N"
	above "$B" "$N"
done
for z in 0 1 100; do
	run move-pane -t "$N" -z "$z"
	above "$A" "$N"
	above "$B" "$N"
	run move-pane -t "$B" -z "$z"
	above "$A" "$N"
	above "$B" "$N"
done
run move-pane -t "$A" -P back
above "$A" "$N"
above "$B" "$N"

# An ordinary float can still be raised without unzooming anything.
run select-pane -t "$F"
above "$F" "$A"
above "$F" "$B"
check "$A" '#{pane_zoomed_flag}' 1
check "$B" '#{pane_zoomed_flag}' 1
run select-pane -t "$N"
check "$N" '#{pane_active}:#{pane_floating_flag}' '1:1'
check "$A" '#{pane_zoomed_flag}' 0
check "$B" '#{pane_zoomed_flag}' 0

# Detached creation preserves zoom; selected creation exposes the new pane.
reset
run resize-pane -Z -t "$A"
N=$(run new-pane -NdP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
above "$A" "$N"
check "$A" '#{pane_zoomed_flag}:#{pane_active}' '1:1'
M=$(run new-pane -NP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
check "$M" '#{pane_active}' 1
check "$A" '#{pane_zoomed_flag}' 0

# A restricted pane can itself be fullscreen; unzoom restores its restriction.
reset
run resize-pane -Z -t "$A"
N=$(run new-pane -NZP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
check "$A" '#{pane_zoomed_flag}' 1
check "$N" '#{pane_zoomed_flag}:#{pane_active}' '1:1'
above "$N" "$A"
run resize-pane -Z -t "$B"
above "$B" "$N"
run resize-pane -Z -t "$N"
check "$N" '#{pane_zoomed_flag}:#{pane_floating_flag}' '0:1'
check "$A" '#{pane_zoomed_flag}' 1
check "$B" '#{pane_zoomed_flag}:#{pane_active}' '1:1'
above "$A" "$N"
above "$B" "$N"
run select-pane -t "$N"
check "$A" '#{pane_zoomed_flag}' 0
check "$B" '#{pane_zoomed_flag}' 0
check "$N" '#{pane_active}' 1

# Tiling and floating again preserve the pane's policy.
reset
N=$(run new-pane -NdP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
run join-pane -d -s "$N" -t "$N"
run resize-pane -Z -t "$A"
run break-pane -Wd -s "$N"
check "$A" '#{pane_zoomed_flag}' 1
above "$A" "$N"

# Swapping into a higher floating position cannot bypass the restriction.
F=$(run new-pane -dP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
above "$F" "$A"
run swap-pane -d -s "$N" -t "$F"
check "$A" '#{pane_zoomed_flag}' 1
above "$A" "$N"
run select-pane -t "$F"
above "$F" "$A"
check "$A" '#{pane_zoomed_flag}' 1

# Always-on-top and modal creation are incompatible with the new policy.
before=$(run display -p -t "$A" '#{window_panes}')
for option in -A -O; do
	$TMUX new-pane -N "$option" -t "$A" 'exec cat' 2>/dev/null &&
	    fail "-N accepted $option"
	check "$A" '#{window_panes}' "$before"
done
run resize-pane -aZ -t "$A"
T=$(run new-pane -NLdP -F '#{pane_id}' -t "$A" 'exec cat') || exit 1
case $(run display -p -t "$T" '#{pane_flags}') in
*N*) fail '-N applied to a tiled creation' ;;
esac

exit 0
