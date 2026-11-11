#!/bin/sh

# Fullscreen panes release tiled space and share the floating stacking order.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
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

geometry()
{
	run list-panes -F '#{pane_id}:#{pane_left},#{pane_top},#{pane_width},#{pane_height}'
}

reset()
{
	$TMUX kill-server 2>/dev/null
	run new-session -d -x 80 -y 24 'sleep 100'
	run set -g default-command 'sleep 100'
	A=$(run display-message -p '#{pane_id}') || exit 1
	B=$(run split-window -hdPF '#{pane_id}' -l 20) || exit 1
	C=$(run split-window -hdPF '#{pane_id}' -t "$B" -l 8) || exit 1
}

# Floating and fullscreen both retain an unchanged, uneven tiled allocation.
reset
before=$(geometry)
run break-pane -W -s "$C"
check "$B" '#{pane_width}' 20
run join-pane -s "$C" -t "$C"
[ "$(geometry)" = "$before" ] || fail 'floating changed tiled allocation'
run resize-pane -Z -t "$C"
check "$C" '#{pane_floating_flag}:#{pane_zoomed_flag}' '1:1'
check "$B" '#{pane_width}' 20
run resize-pane -Z -t "$C"
[ "$(geometry)" = "$before" ] || fail 'fullscreen changed tiled allocation'

# Ordinary floating panes do not change the tiled allocation being restored.
run resize-pane -Z -t "$C"
X=$(run new-pane -dPF '#{pane_id}' '') || exit 1
run kill-pane -t "$X"
run resize-pane -Z -t "$C"
[ "$(geometry)" = "$before" ] || fail 'closing a float changed tiled allocation'

# Restoring a stack unwinds the saved, uneven allocations in reverse order.
run resize-pane -Z -t "$A"
run resize-pane -Z -t "$B"
run resize-pane -Z -t "$C"
run resize-pane -aZ
[ "$(geometry)" = "$before" ] || fail 'fullscreen stack changed tiled allocation'

# A window with no tiled panes still follows the normal window-size policy.
run resize-pane -Z -t "$A"
run resize-pane -Z -t "$B"
run resize-pane -Z -t "$C"
run resize-window -x 60 -y 18
check "$C" '#{pane_width}:#{pane_height}:#{window_width}' '60:18:60'
run resize-pane -aZ
width=2
for p in "$A" "$B" "$C"; do
	check "$p" '#{pane_floating_flag}:#{pane_height}' '0:18'
	part=$(run display-message -p -t "$p" '#{pane_width}')
	[ "$part" -ge 1 ] || fail 'restored pane below its minimum width'
	width=$((width + part))
done
[ "$width" -eq 60 ] || fail 'restored panes do not fill the resized window'
run resize-window -x 80 -y 24

# Restoring after changes splits the neighbour and preserves the other tiles.
run resize-pane -Z -t "$C"
run resize-pane -x 40 -t "$B"
width=$(run display-message -p -t "$A" '#{pane_width}')
run resize-pane -Z -t "$C"
check "$A" '#{pane_width}' "$width"

# A float can sit between fullscreen panes, then be raised above both.
run resize-pane -Z -t "$A"
X=$(run new-pane -PF '#{pane_id}' -x 20 -y 8 '') || exit 1
run resize-pane -Z -t "$B"
run select-pane -t "$X"
check "$X" '#{pane_z}:#{pane_active}' '0:1'
check "$B" '#{pane_z}:#{pane_zoomed_flag}' '1:1'
check "$A" '#{pane_zoomed_flag}' 1
run select-pane -t "$B"
check "$X" '#{pane_z}' 0
check "$B" '#{pane_active}' 1

# Marking a pane never changes focus or stacking, even behind fullscreen.
run select-pane -m -t "$A"
check "$A" '#{pane_marked}:#{pane_zoomed_flag}' '1:1'
check "$B" '#{pane_active}' 1
check "$X" '#{pane_z}' 0
run select-pane -M -t "$A"
run select-pane -m -t "$X"
check "$B" '#{pane_active}' 1
run select-pane -M -t "$X"
run select-pane -m -t "$C"
check "$C" '#{pane_marked}' 1
check "$B" '#{pane_active}' 1

# Fullscreen geometry cannot be moved or resized; splitting first restores it.
$TMUX move-pane -X 10 -t "$B" 2>/dev/null && fail 'moved fullscreen pane'
$TMUX resize-pane -x 20 -t "$B" 2>/dev/null && fail 'resized fullscreen pane'
check "$B" '#{pane_width}:#{pane_height}' '80:24'

# Named stacking operations and numeric z-indices leave fullscreen intact.
for position in front back forward backward forward-loop backward-loop; do
	run move-pane -t "$B" -P "$position"
	check "$B" '#{pane_zoomed_flag}:#{pane_width}:#{pane_height}' '1:80:24'
done
run move-pane -t "$B" -z 0
check "$B" '#{pane_zoomed_flag}:#{pane_width}:#{pane_height}' '1:80:24'

# A geometry position remains forbidden when an ignored -z is also supplied.
for position in centre center bottom-right; do
	$TMUX move-pane -t "$B" -P "$position" 2>/dev/null &&
	    fail 'repositioned fullscreen pane'
	$TMUX move-pane -t "$B" -P "$position" -z 0 2>/dev/null &&
	    fail '-z allowed repositioning a fullscreen pane'
	check "$B" '#{pane_zoomed_flag}:#{pane_width}:#{pane_height}' '1:80:24'
done
run split-window -d -t "$B"
check "$B" '#{pane_floating_flag}:#{pane_zoomed_flag}' '0:0'

# Entire nested groups can leave the tiles and return without losing panes.
reset
run kill-pane -t "$C"
C=$(run split-window -vdPF '#{pane_id}' -t "$A" -l 8) || exit 1
before=$(geometry)
run resize-pane -Z -t "$A"
run resize-pane -Z -t "$C"
check "$B" '#{pane_width}:#{pane_height}' '80:24'
run resize-pane -Z -t "$C"
run resize-pane -Z -t "$A"
[ "$(geometry)" = "$before" ] || fail 'nested allocation was not restored'

# Loading a fullscreen layout retains its tiled origin and floating rectangle.
reset
run resize-pane -Z -t "$A"
X=$(run new-pane -PF '#{pane_id}' -x 20 -y 8 '') || exit 1
normal=$(run display-message -p -t "$X" '#{pane_left},#{pane_top},#{pane_width},#{pane_height}')
run resize-pane -Z -t "$X"
saved=$(run display-message -p '#{window_layout}')
run select-layout "$saved"
check "$X" '#{pane_zoomed_flag}:#{pane_floating_flag}' '1:1'
run resize-pane -Z -t "$X"
check "$X" '#{pane_left},#{pane_top},#{pane_width},#{pane_height}' "$normal"
run resize-pane -Z -t "$A"
check "$A" '#{pane_floating_flag}:#{pane_zoomed_flag}' '0:0'

# Restoring tiles below the physical minimum uses the existing viewport policy.
reset
run break-pane -Wd -s "$B"
run break-pane -Wd -s "$C"
run resize-window -x 1 -y 1
run join-pane -d -s "$B" -t "$B"
run join-pane -d -s "$C" -t "$C"
for p in "$A" "$B" "$C"; do
	check "$p" '#{pane_floating_flag}' 0
done
check "$A" '#{window_width}' 5

# With -Z the new pane is selected even when -d is supplied.
run resize-window -x 80 -y 24
X=$(run new-pane -dZPF '#{pane_id}' '') || exit 1
check "$X" '#{pane_zoomed_flag}:#{pane_active}' '1:1'

# Collapsed groups restore uneven widths after changes to their outer height.
run kill-server
run new-session -d -x 80 -y 24 'sleep 100'
run set -g default-command 'sleep 100'
run split-window -vd -t %0
run split-window -vd -t %1
run split-window -vd -t %2
run split-window -hd -t %1
run split-window -hd -t %1
before=$(geometry)
restore=
for p in %0 %1 %2 %3 %4; do
	run break-pane -Wd -s "$p"
	restore="$p $restore"
done
for p in $restore; do
	run join-pane -d -s "$p" -t "$p"
done
[ "$(geometry)" = "$before" ] || fail 'collapsed group widths were not restored'

exit 0
