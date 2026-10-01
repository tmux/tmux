#!/bin/sh

# A pane can be hidden (minimised) and shown again, returning to exactly its
# previous state: tiled, floating and zoomed panes alike.
#
# This exercises:
# - resize-pane -H toggling a tiled pane, with its space given to a neighbour
#   and returned unchanged when it is shown;
# - hiding floating and zoomed panes and hiding every pane in a window;
# - activating a hidden pane showing it, and next/previous skipping hidden
#   panes;
# - resize-pane -a -H hiding every visible float and zoomed pane and showing
#   only the panes it hid;
# - floating, tiling, swapping and killing hidden panes.

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

# check $target $format $expected
check()
{
	got=$(run display-message -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# hidden $pane $expected
hidden()
{
	check "$1" '#{pane_hidden_flag}' "$2"
}

# geometry $pane $expected
geometry()
{
	check "$1" '#{pane_left}:#{pane_top}:#{pane_width}:#{pane_height}' "$2"
}

# layout $target: the window layout ignoring which panes are active or last.
layout()
{
	got=$(run display-message -p -t "$1" '#{window_layout}') || exit 1
	echo "$got" | sed 's/"a":true,//g; s/"l":[0-9]*,//g'
}

# same_layout $target: is the layout the same as when the test started?
same_layout()
{
	got=$(layout "$1")
	[ "$got" = "$layout" ] || fail "layout changed: $got, expected $layout"
}

reset()
{
	$TMUX kill-server 2>/dev/null
	run new-session -d -x 80 -y 24
	A=$(run display-message -p '#{pane_id}') || exit 1
	B=$(run split-window -dPF '#{pane_id}') || exit 1
	layout=$(layout "$A")
}

# A tiled pane gives its space away and gets it back unchanged.
reset
hidden "$B" 0
run resize-pane -H -t "$B"
hidden "$B" 1
check "$A" '#{pane_height}' 24
run resize-pane -H -t "$B"
hidden "$B" 0
same_layout "$A"

# Selecting a hidden pane shows it and makes it active.
run resize-pane -H -t "$B"
run select-pane -t "$B"
hidden "$B" 0
check "$B" '#{pane_active}' 1
same_layout "$A"

# Hiding the active pane moves the focus to a visible pane.
run resize-pane -H -t "$B"
check "$A" '#{pane_active}' 1

# Next and previous skip hidden panes.
reset
C=$(run split-window -dPF '#{pane_id}') || exit 1
run select-pane -t "$A"
run resize-pane -H -t "$B"
run select-pane -t :.+
check "$C" '#{pane_active}' 1
run select-pane -t :.-
check "$A" '#{pane_active}' 1

# Every tiled pane can be hidden and shown again.
reset
run resize-pane -H -t "$B"
run resize-pane -H -t "$A"
hidden "$A" 1
hidden "$B" 1
run display-message -p ok >/dev/null
run select-pane -t "$A"
run select-pane -t "$B"
hidden "$A" 0
hidden "$B" 0
same_layout "$A"

# A floating pane keeps its geometry and stacking position.
reset
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
Y=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 30 -Y 6 '') || exit 1
gx=$(run display-message -p -t "$X" '#{pane_left}:#{pane_top}:#{pane_width}:#{pane_height}')
zx=$(run display-message -p -t "$X" '#{pane_z}')
run resize-pane -H -t "$X"
hidden "$X" 1
check "$X" '#{pane_floating_flag}' 1
run select-pane -t "$X"
hidden "$X" 0
check "$X" '#{pane_active}' 1
geometry "$X" "$gx"
check "$Y" '#{pane_z}' 1

# A zoomed pane stays zoomed while hidden and when shown.
reset
run resize-pane -Z -t "$A"
run resize-pane -H -t "$A"
hidden "$A" 1
check "$A" '#{pane_zoomed_flag}' 1
run resize-pane -H -t "$A"
hidden "$A" 0
check "$A" '#{pane_zoomed_flag}:#{window_zoomed_flag}' '1:1'

# Hiding every pane in a window, floating ones included, leaves an empty
# window that still works.
reset
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
for p in "$X" "$A" "$B"; do
	run resize-pane -H -t "$p"
done
for p in "$X" "$A" "$B"; do
	hidden "$p" 1
done
run display-message -p '#{window_layout}' >/dev/null
run select-pane -t "$B"
hidden "$B" 0
check "$B" '#{pane_active}' 1

# resize-pane -a -H hides every visible float and zoomed pane and a second
# call shows only those, leaving panes that were already hidden alone.
reset
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
Y=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 30 -Y 6 '') || exit 1
run resize-pane -H -t "$Y"
run resize-pane -a -H -t "$A"
hidden "$X" 1
hidden "$Y" 1
hidden "$A" 0
hidden "$B" 0
run resize-pane -a -H -t "$A"
hidden "$X" 0
hidden "$Y" 1
hidden "$A" 0
run resize-pane -Z -t "$B"
run resize-pane -a -H -t "$A"
hidden "$B" 1
hidden "$X" 1
run resize-pane -a -H -t "$A"
hidden "$B" 0
check "$B" '#{pane_zoomed_flag}' 1

# Floating and tiling a hidden pane keeps it hidden.
reset
run resize-pane -H -t "$B"
run break-pane -W -s "$B"
hidden "$B" 1
check "$B" '#{pane_floating_flag}' 1
run join-pane -s "$B" -t "$B"
hidden "$B" 1
check "$B" '#{pane_floating_flag}' 0
run select-pane -t "$B"
hidden "$B" 0
check "$B" '#{pane_floating_flag}' 0

# A hidden pane can be swapped and killed. The hidden state belongs to the
# position in the layout.
reset
C=$(run split-window -dPF '#{pane_id}') || exit 1
run resize-pane -H -t "$B"
run swap-pane -s "$A" -t "$B"
hidden "$A" 1
hidden "$B" 0
run kill-pane -t "$A"
run display-message -p -t "$B" ok >/dev/null
check "$B" '#{window_panes}' 2

# Splitting a hidden tiled pane shows it first.
reset
run resize-pane -H -t "$B"
run split-window -d -t "$B"
hidden "$B" 0
check "$B" '#{window_panes}' 3

# Resizing a hidden tiled pane shows it first, but a hidden float is resized
# without being shown.
reset
run resize-pane -H -t "$B"
run resize-pane -t "$B" -y 5
hidden "$B" 0
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
run resize-pane -H -t "$X"
run resize-pane -t "$X" -x 30
hidden "$X" 1
run select-pane -t "$X"
check "$X" '#{pane_width}' 28

# A hidden pane moved to another window is shown there.
reset
C=$(run split-window -dPF '#{pane_id}') || exit 1
run resize-pane -H -t "$C"
run break-pane -d -s "$C"
hidden "$C" 0
check "$C" '#{window_panes}' 1
check "$A" '#{window_panes}' 2

# Layouts record hidden floating panes and leave hidden tiled panes out; a
# saved layout puts the hidden state back.
reset
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
run resize-pane -H -t "$X"
saved=$(run display-message -p -t "$A" '#{window_layout}') || exit 1
case "$saved" in
*'"H":true'*) ;;
*) fail "hidden floating pane not in layout: $saved" ;;
esac
run select-pane -t "$X"
hidden "$X" 0
run select-layout -t "$A" "$saved"
hidden "$X" 1
run resize-pane -H -t "$B"
out=$(run display-message -p -t "$A" '#{window_layout}') || exit 1
case "$out" in
*"\"$B\""*) fail "hidden tiled pane in layout: $out" ;;
*) ;;
esac

# Swapping with the next or previous pane when the only tiled pane is hidden
# has nothing to swap with and does nothing.
reset
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 20 -y 8 -X 8 -Y 3 '') || exit 1
run kill-pane -t "$B"
run resize-pane -H -t "$A"
run swap-pane -D -t "$A"
run swap-pane -U -t "$A"
hidden "$A" 1
check "$X" '#{window_panes}' 2

# Preset layouts lay out only the tiled panes that are not hidden and leave the
# hidden ones hidden.
reset
C=$(run split-window -dPF '#{pane_id}') || exit 1
run resize-pane -H -t "$B"
for layout in tiled even-horizontal even-vertical main-horizontal \
    main-vertical; do
	run select-layout -t "$A" $layout
	hidden "$B" 1
	hidden "$A" 0
	hidden "$C" 0
done
run next-layout -t "$A"
hidden "$B" 1
run resize-pane -H -t "$C"
run select-layout -t "$A" tiled
run next-layout -t "$A"
hidden "$C" 1
check "$A" '#{pane_width}:#{pane_height}' '80:24'

# The size a hidden tiled pane gets back follows the window size, so showing it
# after the window shrank does not squeeze its neighbours out.
reset
run kill-server
run new-session -d -x 80 -y 30
A=$(run display-message -p '#{pane_id}') || exit 1
B=$(run split-window -dPF '#{pane_id}') || exit 1
C=$(run split-window -dPF '#{pane_id}') || exit 1
run select-layout even-vertical
before=$(run display-message -p -t "$B" '#{pane_height}') || exit 1
run resize-pane -H -t "$B"
run resize-window -x 80 -y 16
run select-pane -t "$B"
hidden "$B" 0
for p in "$A" "$B" "$C"; do
	h=$(run display-message -p -t "$p" '#{pane_height}') || exit 1
	[ "$h" -ge 3 ] || fail "$p is only $h lines high after showing"
done
h=$(run display-message -p -t "$B" '#{pane_height}') || exit 1
[ "$h" -lt "$before" ] || fail "shown pane is $h lines, was $before"

# Without the window changing it gets back exactly the size it had.
run resize-window -x 80 -y 30
run select-layout even-vertical
run resize-pane -t "$A" -y 6
sizes=$(run list-panes -F '#{pane_id}:#{pane_height}') || exit 1
run resize-pane -H -t "$B"
run resize-pane -H -t "$B"
check "$A" '#{pane_height}' 6
after=$(run list-panes -F '#{pane_id}:#{pane_height}') || exit 1
[ "$sizes" = "$after" ] || fail "sizes changed: '$sizes' became '$after'"

# Showing a hidden pane only resizes it when it does not already have the right
# size, so a plain hide and show announces a layout change once each.
$TMUX kill-server 2>/dev/null
run new-session -d -x 80 -y 23
A=$(run display-message -p '#{pane_id}') || exit 1
B=$(run split-window -dPF '#{pane_id}') || exit 1
sizes=$(run list-panes -F '#{pane_id}:#{pane_height}') || exit 1
run set -g @count 0
run set-hook -g window-layout-changed "set -gF @count '#{e|+:#{@count},1}'"
run resize-pane -H -t "$B"
count=$(run show -gv @count) || exit 1
run resize-pane -H -t "$B"
got=$(run show -gv @count) || exit 1
[ $((got - count)) -eq 1 ] ||
    fail "showing the pane fired $((got - count)) layout changes, expected 1"
after=$(run list-panes -F '#{pane_id}:#{pane_height}') || exit 1
[ "$sizes" = "$after" ] || fail "sizes changed: '$sizes' became '$after'"
run set-hook -gu window-layout-changed

# Zooming a pane shows it, even if it is hidden and already the active pane.
reset
run resize-pane -H -t "$A"
run resize-pane -H -t "$B"
act=$(run display-message -p -t "$A" '#{?pane_active,1,0}') || exit 1
if [ "$act" = 1 ]; then
	p=$A
else
	p=$B
fi
hidden "$p" 1
run resize-pane -Z -t "$p"
hidden "$p" 0
check "$p" '#{pane_zoomed_flag}:#{pane_active}' '1:1'

# Moving to the pane at the left or right skips a hidden floating pane. The
# floating pane is flush with the right edge so moving left from the tiled pane
# wraps round to it.
reset
$TMUX kill-server 2>/dev/null
run new-session -d -x 80 -y 24
A=$(run display-message -p '#{pane_id}') || exit 1
X=$(run new-pane -dPF '#{pane_id}' -t "$A" -x 18 -y 6 -X 63 -Y 5 '') || exit 1
run select-pane -t "$A"
run select-pane -L -t "$A"
check "$X" '#{pane_active}' 1
run select-pane -t "$A"
run resize-pane -H -t "$X"
run select-pane -L -t "$A"
check "$A" '#{pane_active}' 1
hidden "$X" 1

# Mixed vertical and horizontal splits, including three or more panes side by
# side. Hiding and showing each pane puts every pane back where it was. Hiding
# several panes and showing them again in the reverse order they were hidden
# leaves every pane within two cells of where it was. In the same order the panes
# have the wrong neighbours when they are shown so their sizes cannot be exact,
# but every pane must be back, tiled and not squeezed. Floating panes, including
# ones that were never tiled, are part of this: they are not moved by hiding
# other panes and tile somewhere in the layout without squeezing it.
#
# geom: id, left, top, width and height of every pane, sorted by id.
geom()
{
	run list-panes -F '#{pane_id} #{pane_left} #{pane_top} #{pane_width} #{pane_height}' |
	    sort
}

# near $name $base $now $tolerance
near()
{
	echo "$2
$3" | awk -v tol="$4" -v name="$1" '
	NF == 5 && ($1 in seen) {
		for (i = 2; i <= 5; i++) {
			d = $i - seen[$1, i]
			if (d < 0) d = -d
			if (d > tol) {
				printf "%s: %s differs by %d (%s became %s)\n", name, $1, d, seen[$1, 0], $0
				bad = 1
			}
		}
		next
	}
	NF == 5 { seen[$1] = 1; for (i = 2; i <= 5; i++) seen[$1, i] = $i; seen[$1, 0] = $0 }
	END { exit bad }' || fail "layout not restored"
}

# usable $name $panes $minimum: the panes are tiled, shown and at least the
# minimum size in both directions.
usable()
{
	for p in $2; do
		check "$p" '#{pane_floating_flag}:#{pane_hidden_flag}' 0:0
		w=$(run display-message -p -t "$p" '#{pane_width}') || exit 1
		h=$(run display-message -p -t "$p" '#{pane_height}') || exit 1
		[ "$w" -ge "$3" ] && [ "$h" -ge "$3" ] ||
		    fail "$1: $p is only ${w}x$h"
	done
}

# build $size $splits...: a window made of splits, each a flag and a target.
build()
{
	size=$1
	shift
	$TMUX kill-server 2>/dev/null
	run new-session -d -x "${size%x*}" -y "${size#*x}" cat
	while [ $# -gt 0 ]; do
		run split-window -d "$1" -t "$2" ''
		shift 2
	done
}

mixed()
{
	name=$1
	size=$2
	shift 2
	splits="$*"
	build $size $splits
	base=$(geom)
	ids=$(run list-panes -F '#{pane_id}' | sort)
	n=$(echo "$ids" | wc -l)
	last=$(echo "$ids" | tail -1)

	# Each pane in turn.
	for p in $ids; do
		run resize-pane -H -t "$p"
		hidden "$p" 1
		run resize-pane -H -t "$p"
		hidden "$p" 0
		near "$name hiding $p" "$base" "$(geom)" 0
	done

	# Every pane but the last, shown in the reverse and in the same order.
	for order in reverse forward; do
		hide=
		for p in $ids; do
			[ "$p" = "$last" ] && continue
			hide="$hide $p"
			run resize-pane -H -t "$p"
		done
		show=$hide
		if [ "$order" = reverse ]; then
			show=
			for p in $hide; do
				show="$p $show"
			done
		fi
		for p in $show; do
			run resize-pane -H -t "$p"
		done
		for p in $ids; do
			hidden "$p" 0
		done
		if [ "$order" = reverse ]; then
			near "$name hiding $((n - 1)) panes, $order" "$base" "$(geom)" 2
		else
			usable "$name hiding $((n - 1)) panes, $order" "$ids" 1
		fi
	done

	# Any two panes, hidden and shown in the same order, even if they are
	# next to each other: nothing is squeezed.
	if [ "$size" = 120x40 ]; then
		for p in $ids; do
			for q in $ids; do
				[ "$p" = "$q" ] && continue
				build $size $splits
				run resize-pane -H -t "$p"
				run resize-pane -H -t "$q"
				run resize-pane -H -t "$p"
				run resize-pane -H -t "$q"
				usable "$name hiding $p and $q" "$ids" 2
			done
		done
	fi

	# A floating pane that was never tiled: hiding and showing it, or any
	# tiled pane, changes nothing else.
	build $size $splits
	f=$(run new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 5 -Y 5 '') || exit 1
	base=$(geom)
	for p in $ids $f; do
		run resize-pane -H -t "$p"
		run resize-pane -H -t "$p"
		near "$name with a float, hiding $p" "$base" "$(geom)" 0
	done
	check "$f" '#{pane_floating_flag}' 1

	# Tiling it puts it in the layout without squeezing the others, and it
	# can be floated and tiled again.
	run join-pane -s "$f" -t "$f"
	usable "$name tiling a new float" "$ids $f" 1
	run break-pane -W -s "$f"
	check "$f" '#{pane_floating_flag}' 1
	run join-pane -s "$f" -t "$f"
	usable "$name tiling the float again" "$ids $f" 1

	# Floating, hiding and tiling panes together.
	for p in $ids; do
		[ "$p" = "$last" ] && continue
		build $size $splits
		f=$(run new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 5 -Y 5 '') || exit 1
		run break-pane -d -W -s "$p"
		run resize-pane -H -t "$last"
		run resize-pane -H -t "$last"
		run join-pane -d -s "$p" -t "$p"
		run join-pane -d -s "$f" -t "$f"
		usable "$name floating $p, hiding $last, tiling both" "$ids $f" 1
	done
}

for size in 80x24 120x40; do
	mixed "A|(B/C) $size" $size -h %0 -v %1
	mixed "A/(B|C) $size" $size -v %0 -h %1
	mixed "(A|C)/B $size" $size -v %0 -h %0
	mixed "(A/C)|B $size" $size -h %0 -v %0
	mixed "2x2 $size" $size -v %0 -h %0 -h %1
	mixed "A|(B/(C|D)) $size" $size -h %0 -v %1 -h %2
	mixed "A/(B|(C/D)) $size" $size -v %0 -h %1 -v %2
done
# Three or more side by side need room.
mixed "A/(B|C|D)/E 120x40" 120x40 -v %0 -v %1 -v %2 -h %1 -h %1
mixed "(A/B/C)|(D|E) 120x40" 120x40 -h %0 -v %0 -v %0 -h %1
mixed "A|(B/C/D)|E 120x40" 120x40 -h %0 -h %1 -v %1 -v %1
mixed "A/(B|(C/D|E))/F 120x40" 120x40 -v %0 -v %1 -h %1 -v %3 -h %3

exit 0
