#!/bin/sh

# Moving, resizing and reordering an over-zoom float must leave the tiled
# pane zoomed, and the changes must survive an explicit unzoom.

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

check_zoom()
{
	check "$base" '#{window_zoomed_flag}:#{pane_zoomed_flag}' '1:1'
	check "$float" '#{pane_floating_flag}:#{pane_active}' "1:$active"
	check "$base" '#{@unzoomed}' ''
}

run new-session -d -x 80 -y 24
base=$(run display-message -p '#{pane_id}') || exit 1
other=$(run split-window -dPF '#{pane_id}') || exit 1
layout=$(run display-message -p '#{window_layout}') || exit 1
run set-hook -g window-unzoomed 'set -g @unzoomed 1'

# Cover creation before and after zoom, with both the float and the tiled
# pane active. Modal panes use the same over-zoom flag.
for mode in before after detached modal; do
	if [ "$mode" != before ]; then
		run resize-pane -Z -t "$base"
	fi
	case "$mode" in
	detached) flags=-Ad; active=0 ;;
	modal) flags=-O; active=1 ;;
	*) flags=-A; active=1 ;;
	esac
	float=$(run new-pane "$flags" -PF '#{pane_id}' -t "$base" \
	    -x 20 -y 8 -X 8 -Y 3 '') || exit 1
	if [ "$mode" = before ]; then
		run resize-pane -Z -t "$base"
	fi
	run set -gu @unzoomed
	check_zoom

	run move-pane -t "$float" -D
	check "$float" '#{pane_top}' 5
	check_zoom
	run move-pane -t "$float" -U 2 -R 3 -L 1
	check "$float" '#{pane_left}:#{pane_top}' '11:3'
	check_zoom
	run move-pane -t "$float" -X 25% -Y 25%
	check "$float" '#{pane_left}:#{pane_top}' '21:7'
	check_zoom
	run move-pane -t "$float" -P bottom-right
	check "$float" '#{pane_left}:#{pane_top}' '61:17'
	check_zoom
	run resize-pane -t "$float" -x 30 -y 10
	check "$float" '#{pane_width}:#{pane_height}' '28:8'
	check_zoom
	run resize-pane -t "$float" -U 1 -L 2
	check "$float" '#{pane_left}:#{pane_top}:#{pane_width}:#{pane_height}' \
	    '59:16:30:9'
	check_zoom
	run move-pane -t "$float" -P centre
	check "$float" '#{pane_left}:#{pane_top}' '25:7'
	check_zoom

	# Failed commands must not change zoom either.
	for command in 'move-pane -D invalid' 'move-pane -P invalid' \
	    'move-pane -z invalid' 'resize-pane -x invalid' \
	    'resize-pane -D invalid'; do
		$TMUX $command -t "$float" 2>/dev/null &&
		    fail "unexpected success: $command"
		check_zoom
	done

	run resize-pane -Z -t "$base"
	check "$float" '#{pane_left}:#{pane_top}:#{pane_width}:#{pane_height}' \
	    '25:7:30:9'
	run kill-pane -t "$float"
	check "$base" '#{window_layout}' "$layout"
done

# Put an ordinary (hidden while zoomed) float between two over-zoom floats
# in the stacking order. Reordering must skip it when counting visible
# positions, but keep all floats ahead of the tiled panes after unzoom.
back=$(run new-pane -AdPF '#{pane_id}' -t "$base" '') || exit 1
hidden=$(run new-pane -dPF '#{pane_id}' -t "$base" '') || exit 1
float=$(run new-pane -AdPF '#{pane_id}' -t "$base" '') || exit 1
active=0
run resize-pane -Z -t "$base"
run set -gu @unzoomed

for position in backward back forward-loop; do
	run move-pane -t "$float" -P front
	run move-pane -t "$float" -P "$position"
	check "$float" '#{pane_z}' 1
	check "$back" '#{pane_z}' 0
	check_zoom
done
for position in forward front backward-loop; do
	run move-pane -t "$float" -P back
	run move-pane -t "$float" -P "$position"
	check "$float" '#{pane_z}' 0
	check "$back" '#{pane_z}' 1
	check_zoom
done
for z in 1 0 99; do
	run move-pane -t "$float" -z "$z"
	want=$z
	[ "$z" = 99 ] && want=1
	check "$float" '#{pane_z}' "$want"
	check_zoom
done
run resize-pane -Z -t "$base"
check "$float" '#{pane_z}' 2
check "$base" '#{pane_z}' 4
run kill-pane -t "$hidden"
run kill-pane -t "$back"

# Explicit zoom toggling and operations on tiled panes retain their
# existing behaviour. The over-zoom flag alone does not make a pane a float:
# zooming the float itself gives it a tiled cell until unzoom.
run resize-pane -Z -t "$float"
check "$float" '#{pane_zoomed_flag}:#{pane_floating_flag}' '1:0'
run resize-pane -t "$float" -x 25
check "$float" '#{window_zoomed_flag}:#{pane_floating_flag}:#{pane_width}' \
    '0:1:23'
run resize-pane -Z -t "$base"
run resize-pane -t "$other" -D 1
check "$base" '#{window_zoomed_flag}' 0

# A hidden ordinary float must still be unzoomed before resizing it.
hidden=$(run new-pane -dPF '#{pane_id}' -t "$base" '') || exit 1
run resize-pane -Z -t "$base"
run resize-pane -t "$hidden" -x 25
check "$hidden" '#{window_zoomed_flag}:#{pane_floating_flag}:#{pane_width}' \
    '0:1:23'

exit 0
