#!/bin/sh

# Automatic redistribution respects explicit sizing; local redistribution
# resets it without changing enclosing groups or pane states.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
OUTER="$TEST_TMUX -S$DIR/outer -f/dev/null"

# Stop both isolated servers and remove their temporary files.
cleanup()
{
	stop_server $TMUX
	stop_server $OUTER
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

# Report a failed assertion.
fail()
{
	echo "$*" >&2
	exit 1
}

# Run a command against the test server.
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

# Capture pane rectangles for comparisons across layout operations.
geometry()
{
	run list-panes -F '#{pane_id}:#{pane_left},#{pane_top},#{pane_width},#{pane_height}'
}

# Start with three unsized horizontal splits in a fresh server.
reset()
{
	stop_server $TMUX || fail 'could not stop previous test server'
	A=$(run new-session -dP -F '#{pane_id}' -x 122 -y 42 'exec cat') || exit 1
	B=$(run split-window -hdP -F '#{pane_id}' -t "$A" '') || exit 1
	C=$(run split-window -hdP -F '#{pane_id}' -t "$B" '') || exit 1
}

# Default splits keep their normal sizes, then membership changes equalize.
reset
check "$A" '#{pane_width}' 61
check "$B" '#{pane_width}' 30
check "$C" '#{pane_width}' 29
run break-pane -Wd -s "$C"
check "$A" '#{pane_width}' 61
check "$B" '#{pane_width}' 60
run join-pane -d -s "$C" -t "$C"
for p in "$A" "$B" "$C"; do
	check "$p" '#{pane_width}' 40
done
run break-pane -Wd -s "$C"
check "$A" '#{pane_width}' 61
check "$B" '#{pane_width}' 60

# Explicit resizing protects a group, including when its sizes end up equal.
run join-pane -d -s "$C" -t "$C"
run resize-pane -x 41 -t "$A"
run resize-pane -x 40 -t "$A"
before=$(geometry)
run break-pane -Wd -s "$C"
check "$A" '#{pane_width}' 40
run join-pane -d -s "$C" -t "$C"
[ "$(geometry)" = "$before" ] || fail 'manual allocation was not restored'
run select-layout -EPm -t "$A"
[ "$(geometry)" = "$before" ] || fail '-m changed manual sizing'
run break-pane -Wd -s "$C"
check "$A" '#{pane_width}' 40
run join-pane -d -s "$C" -t "$C"

# Explicit redistribution clears the marker even if nothing needs resizing.
run select-layout -EP -t "$A"
run break-pane -Wd -s "$C"
check "$A" '#{pane_width}' 61
check "$B" '#{pane_width}' 60

# Manual changes while a pane floats are not overwritten when it returns.
run resize-pane -x 70 -t "$A"
run join-pane -d -s "$C" -t "$C"
check "$A" '#{pane_width}' 70

# Preset equal layouts reset manual sizing, while terminal resizing does not
# mark an automatic group or clear a manual one.
run select-layout even-horizontal
run resize-window -x 152 -y 42
run resize-pane -h -t "$C"
check "$A" '#{pane_width}' 76
check "$B" '#{pane_width}' 75
run resize-pane -dH -t "$C"
for p in "$A" "$B" "$C"; do
	check "$p" '#{pane_width}' 50
done
run resize-pane -x 80 -t "$A"
run resize-window -x 182 -y 42
width=$(run display -p -t "$A" '#{pane_width}')
run resize-pane -h -t "$C"
check "$A" '#{pane_width}' "$width"
run resize-pane -dH -t "$C"

# Equal presets also clear marks when every group is collapsed.
for p in "$A" "$B" "$C"; do
	run resize-pane -h -t "$p"
done
run select-layout even-horizontal
for p in "$A" "$B" "$C"; do
	run resize-pane -dH -t "$p"
done
for p in "$A" "$B" "$C"; do
	check "$p" '#{pane_width}' 60
done

# Loading explicit geometry protects its allocations until redistributed.
run resize-pane -x 90 -t "$A"
saved=$(run display -p '#{window_layout}')
run select-layout even-horizontal
run select-layout "$saved"
run break-pane -Wd -s "$C"
check "$A" '#{pane_width}' 90
run join-pane -d -s "$C" -t "$C"

# The same policy applies vertically.
run select-layout even-vertical
run resize-pane -h -t "$C"
check "$A" '#{pane_height}' 21
check "$B" '#{pane_height}' 20
run resize-pane -dH -t "$C"
check "$A" '#{pane_height}' 14
check "$B" '#{pane_height}' 13
check "$C" '#{pane_height}' 13

# The local form stops at an already equal parent; ordinary -E still ascends.
reset
run kill-pane -t "$C"
run resize-pane -x 80 -t "$A"
C=$(run split-window -vdP -F '#{pane_id}' -t "$A" '') || exit 1
run select-layout -EP -t "$A"
before=$(geometry)
run select-layout -EP -t "$A"
[ "$(geometry)" = "$before" ] || fail 'local redistribution changed outer sizes'
run select-layout -Em -t "$A"
[ "$(geometry)" = "$before" ] || fail '-m crossed a manual outer group'
run select-layout -E -t "$A"
check "$A" '#{pane_width}' 61
check "$B" '#{pane_width}' 60

# Redistributing underneath fullscreen panes preserves their state and stack.
reset
run resize-pane -Z -t "$C"
F=$(run new-pane -dP -F '#{pane_id}' -x 20 -y 8 '') || exit 1
run resize-pane -Z -t "$F"
run resize-pane -h -t "$F"
active=$(run display -p '#{pane_id}')
stack=$(run list-panes -F '#{pane_id}:#{pane_z}')
run select-layout -EP -t "$A"
check "$C" '#{pane_zoomed_flag}:#{pane_floating_flag}' '1:1'
check "$F" '#{pane_zoomed_flag}:#{pane_hidden_flag}' '1:1'
[ "$(run list-panes -F '#{pane_id}:#{pane_z}')" = "$stack" ] ||
	fail 'redistribution changed stacking'
check "$active" '#{pane_active}' 1
run select-layout even-horizontal
check "$C" '#{pane_zoomed_flag}' 1
check "$F" '#{pane_zoomed_flag}:#{pane_hidden_flag}' '1:1'

# Existing after-split-window hooks can still explicitly equalize all siblings.
reset
run set-hook -g after-split-window 'select-layout -E'
run resize-pane -x 80 -t "$A"
D=$(run split-window -hdP -F '#{pane_id}' -t "$C" '') || exit 1
wait_for_output 30 $TMUX display -p -t "$A" '#{pane_width}' ||
	fail 'after-split-window did not equalize panes'
for p in "$B" "$C"; do
	check "$p" '#{pane_width}' 30
done
check "$D" '#{pane_width}' 29
run set-hook -gu after-split-window

# The menu offers an explicit local reset.
run list-keys -T prefix '>' > "$DIR/menu"
grep -q 'Redistribute Panes.*select-layout -EP' "$DIR/menu" ||
	fail 'pane menu is missing redistribution'

# The modifiers require an equalization request.
$TMUX select-layout -P 2>/dev/null && fail '-P accepted without -E'
$TMUX select-layout -m 2>/dev/null && fail '-m accepted without -E'

# Right-clicking an inactive pane redistributes its own parent, not the
# active pane's parent, and leaves focus and the outer divider unchanged.
reset
run kill-pane -t "$C"
run resize-pane -x 80 -t "$A"
C=$(run split-window -vdP -F '#{pane_id}' -t "$A" '') || exit 1
run resize-pane -y 28 -t "$A"
run select-pane -t "$B"
run set -g status off
run set -g mouse on
run respawn-pane -k -t "$B" 'printf "MENU-READY\n"; exec cat'
$OUTER new-session -d -x 122 -y 43 "$TMUX attach-session" ||
	fail 'could not attach menu test client'
wait_for_clients 1 $TMUX || fail 'menu test client did not attach'
wait_for_text MENU-READY $OUTER capture-pane -p ||
	fail 'menu test client did not draw'
height=$(run display -p -t "$C" '#{window_height}')
row=$(run display -p -t "$C" '#{pane_top}')
row=$((row + 2))
mouse=$(printf '\033[<2;2;%sM' "$row")
$OUTER send-keys -l "$mouse" || fail 'could not right-click pane'
wait_for_text 'Redistribute Panes' $OUTER capture-pane -p ||
	fail 'pane menu did not open'
$OUTER send-keys E || fail 'could not choose redistribution'
wait_for_output "$((height / 2))" $TMUX display -p -t "$A" '#{pane_height}' ||
	fail 'menu did not redistribute the clicked parent'
check "$C" '#{pane_height}' "$(((height - 1) / 2))"
check "$A" '#{pane_width}' 80
check "$B" '#{pane_active}' 1

exit 0
