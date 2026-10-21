#!/bin/sh

# Resize the horizontal status background without changing item drag bindings.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

# Stop both isolated servers.
cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0
trap 'exit 1' 1 2 15

# Report the status position and failed check.
fail()
{
	echo "$position: $*" >&2
	exit 1
}

# Send a one-based SGR mouse report through the attached client.
mouse()
{
	seq=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail 'send mouse failed'
	sleep 0.1
}

# Wait for the status option and window height to change together.
check_height()
{
	i=0
	while [ "$i" -lt 50 ]; do
		height=$($TMUX show -Av -t inner status) || fail 'server exited'
		[ "$height" = on ] && height=1
		window=$($TMUX display -p -t inner '#{window_height}')
		if [ "$height,$window" = "$1,$((20 - $1))" ]; then
			return
		fi
		sleep 0.05
		i=$((i + 1))
	done
	fail "status/window heights $height/$window, expected $1/$((20 - $1))"
}

# Check that item drags have swapped the intended windows.
check_order()
{
	got=$($TMUX list-windows -t inner -F '#{window_name}')
	[ "$got" = "$(printf '%s\n' "$@")" ] ||
	    fail "window order '$got', expected '$*'"
}

# Wait for the side status option and window width to change together.
check_width()
{
	i=0
	while [ "$i" -lt 50 ]; do
		width=$($TMUX show -Av -t inner side-status-width)
		window=$($TMUX display -p -t inner '#{window_width}')
		if [ "$width,$window" = "$1,$((60 - $1))" ]; then
			return
		fi
		sleep 0.05
		i=$((i + 1))
	done
	fail "side/window widths $width/$window, expected $1/$((60 - $1))"
}

$TMUX new-session -d -s inner -n AAA -x 60 -y 20 'sleep 180' || exit 1
$TMUX new-window -d -n BBB 'sleep 180' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g assume-paste-time 0 || exit 1
$TMUX set -g status-left '' || exit 1
$TMUX set -g status-right '' || exit 1
$TMUX set -g window-status-format '#W' || exit 1
$TMUX set -g window-status-current-format '#W' || exit 1
for line in 1 2 3 4; do
	$TMUX set -g "status-format[$line]" "ROW$line" || exit 1
done
BASE=$($TMUX display -p -t inner:0 '#{pane_id}') || exit 1
$TMUX2 new-session -d -s outer -x 60 -y 20 'sleep 180' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
$TMUX2 new-session -d -s other -x 60 -y 20 \
    "$TMUX attach -t inner" || exit 1
sleep 1

# A background drag has no action until a binding is installed.
position=bottom
if $TMUX list-keys -T root MouseDrag1StatusDefault >/dev/null 2>&1; then
	fail 'background drag has a default binding'
fi
mouse 0 30 20 M
mouse 32 30 17 M
mouse 0 30 17 m
check_height 1
$TMUX resize-status || exit 1
check_height 1

# The documented prefix bindings adjust height without a mouse event.
$TMUX bind + resize-status +1 || exit 1
$TMUX bind - resize-status -1 || exit 1
for expected in 2 3; do
	$TMUX2 send-keys -t outer:0.0 C-b + || exit 1
	check_height "$expected"
done
$TMUX2 send-keys -t outer:0.0 C-b - || exit 1
check_height 2
$TMUX resize-status -t inner 2147483647 || exit 1
check_height 5
$TMUX resize-status -t inner -2147483648 || exit 1
check_height 1
for adjustment in bad -1x -2147483649 +2147483648 -q; do
	if $TMUX resize-status -t inner "$adjustment" >/dev/null 2>&1; then
		fail "resize-status accepted invalid adjustment '$adjustment'"
	fi
done
check_height 1

# Numeric flags in other commands still parse as flags.
$TMUX bind -T resize-status-test x command-prompt -1 || exit 1
$TMUX bind -T resize-status-test y send-prefix -2 || exit 1
$TMUX list-keys -1 -T resize-status-test >/dev/null || exit 1
$TMUX bind -n MouseDragEnd1StatusDefault set -g @background-end yes || exit 1

for position in bottom top; do
	$TMUX bind -n MouseDrag1StatusDefault resize-status || exit 1
	$TMUX set -t inner status-position "$position" || exit 1
	$TMUX set -u -t inner status || exit 1
	start=20 grow=18 far=1
	[ "$position" = top ] && start=1 grow=3 far=20
	sleep 0.5

	# Horizontal movement has no effect; vertical movement grows and shrinks.
	mouse 0 30 "$start" M
	mouse 32 31 "$start" M
	check_height 1
	[ -z "$($TMUX show -qv -t inner status)" ] ||
	    fail 'horizontal movement changed the inherited status option'
	mouse 32 31 "$grow" M
	check_height 3
	mouse 32 31 "$far" M
	check_height 5
	mouse 32 31 "$start" M
	check_height 1
	mouse 0 31 "$start" m
	[ "$($TMUX show -gv @background-end)" = yes ] ||
	    fail 'background drag end binding did not fire'

	# Start at the inner row of a multiline bar, preserving the grab offset.
	$TMUX set -t inner status 3 || exit 1
	start=18 grow=16 shrink=19 edge=20 far=1 release=17
	if [ "$position" = top ]; then
		start=3 grow=5 shrink=2 edge=1 far=20 release=4
	fi
	sleep 0.3
	mouse 0 30 "$start" M
	mouse 32 31 "$start" M
	check_height 3
	mouse 32 31 "$grow" M
	check_height 5
	mouse 32 31 "$shrink" M
	check_height 2
	mouse 32 31 "$edge" M
	check_height 1
	mouse 32 31 "$far" M
	check_height 5
	mouse 32 31 "$start" M
	check_height 3

	# Release applies its final position, and later motion does not resize.
	mouse 0 31 "$release" m
	check_height 4
	mouse 35 31 "$edge" M
	check_height 4
	[ "$($TMUX show -gv status)" = on ] ||
	    fail 'drag changed the global status option'

	# Both attached clients show the newly revealed format rows.
	for target in outer other; do
		screen=$($TMUX2 capture-pane -p -t "$target:0.0")
		printf '%s\n' "$screen" | grep -q ROW3 ||
		    fail "$target did not redraw the fourth status line"
	done
done

# Item reordering and status resizing must coexist with vertical movement.
$TMUX set -t inner status on || exit 1
$TMUX set -t inner status-position bottom || exit 1
position=items
for key in MouseDown1Status SecondClick1Status TripleClick1Status; do
	$TMUX bind -n "$key" \
	    'selectw ; set -F @drag_window "#{window_id}"' || exit 1
done
$TMUX bind -n MouseDrag1Status \
    "if -F '#{@drag_window}' { run -C 'swapw -ds#{@drag_window}' }" || exit 1
$TMUX bind -n MouseDragEnd1Status set -u @drag_window || exit 1
sleep 0.3
mouse 0 2 20 M
mouse 32 3 20 M
mouse 32 6 20 M
check_order BBB AAA
mouse 32 30 20 M
mouse 32 30 16 M
check_height 1
mouse 32 2 20 M
check_order AAA BBB
mouse 0 2 20 m
[ -z "$($TMUX show -qv @drag_window)" ] || fail 'item drag did not end'

# Background motion across an item must not invoke its drag binding.
$TMUX bind -n MouseDrag1Status set -g @item-drag yes || exit 1
$TMUX set -gu @item-drag || exit 1
mouse 0 30 20 M
mouse 32 31 20 M
mouse 32 2 20 M
mouse 32 2 18 M
check_height 3
mouse 0 2 18 m
[ -z "$($TMUX show -qgv @item-drag)" ] ||
    fail 'background drag invoked an item binding'

# Spaces inside window, pane, user, left, right and control ranges are items.
$TMUX set -t inner status on || exit 1
for key in StatusLeft StatusRight Control0; do
	$TMUX bind -n "MouseDrag1$key" set -g @item-drag yes || exit 1
done
for range in 'window|0' "pane|$BASE" 'user|test' left right 'control|0'; do
	$TMUX set -t inner 'status-format[0]' \
	    "#[range=$range]A A#[norange]" || exit 1
	$TMUX set -gu @item-drag || exit 1
	sleep 0.3
	mouse 0 2 20 M
	mouse 32 3 20 M
	mouse 32 30 20 M
	mouse 32 30 17 M
	check_height 1
	mouse 0 30 20 m
	[ "$($TMUX show -gv @item-drag)" = yes ] ||
	    fail "$range did not invoke its item drag binding"
done

# The side separator changes width without changing horizontal status.
position=side
$TMUX set -g side-status-format '' || exit 1
$TMUX bind -n MouseDrag1SideStatusBorder resize-status || exit 1
for side in left right; do
	$TMUX set -t inner side-status "$side" || exit 1
	$TMUX set -t inner side-status-width 14 || exit 1
	$TMUX bind -n MouseDrag1SideStatus set -g @item-drag yes || exit 1
	start=14 grow=17 edge=1 far=60
	[ "$side" = right ] && start=47 grow=44 edge=60 far=1
	sleep 0.3
	mouse 0 "$start" 10 M
	mouse 32 "$start" 12 M
	check_width 14
	mouse 32 "$grow" 12 M
	check_width 17
	mouse 32 "$edge" 12 M
	check_width 2
	mouse 32 "$far" 12 M
	check_width 59
	mouse 32 "$start" 20 M
	check_width 14
	mouse 0 "$grow" 20 m
	check_width 17
	check_height 1
	[ "$($TMUX show -gv side-status-width)" = 14 ] ||
	    fail 'side drag changed the global width'

	# A separator drag crossing a side item keeps the resize captured.
	$TMUX set -t inner side-status-width 14 || exit 1
	$TMUX set -g side-status-format \
	    '#[range=window|0]A A#[norange]' || exit 1
	$TMUX set -gu @item-drag || exit 1
	start=14 grow=17 item=3 over_width=3
	if [ "$side" = right ]; then
		start=47 grow=44 item=49 over_width=12
	fi
	sleep 0.3
	mouse 0 "$start" 10 M
	mouse 32 "$start" 11 M
	mouse 32 "$item" 1 M
	check_width "$over_width"
	mouse 32 "$grow" 1 M
	check_width 17
	mouse 0 "$grow" 1 m
	[ -z "$($TMUX show -qgv @item-drag)" ] ||
	    fail 'separator drag invoked an item binding'

	# Side background remains available for another action, without resizing.
	$TMUX bind -n MouseDrag1SideStatusDefault \
	    set -g @background-drag yes || exit 1
	$TMUX set -gu @background-drag || exit 1
	$TMUX set -t inner side-status-width 14 || exit 1
	sleep 0.3
	mouse 0 "$item" 10 M
	mouse 32 "$item" 12 M
	mouse 32 30 12 M
	mouse 32 30 20 M
	check_width 14
	check_height 1
	mouse 0 30 20 m
	[ "$($TMUX show -gv @background-drag)" = yes ] ||
	    fail 'side background action did not run'

	# A separate binding resizes the side background across items.
	$TMUX bind -n MouseDrag1SideStatusDefault resize-status || exit 1
	background_grow=6
	[ "$side" = right ] && background_grow=46
	mouse 0 "$item" 10 M
	mouse 32 "$item" 11 M
	mouse 32 "$item" 1 M
	mouse 32 "$background_grow" 1 M
	check_width 17
	mouse 0 "$background_grow" 1 m
	[ -z "$($TMUX show -qgv @item-drag)" ] ||
	    fail 'side background resize invoked an item binding'

	# A side-only binding cannot resize the horizontal status line.
	$TMUX unbind -qn MouseDrag1StatusDefault || exit 1
	mouse 0 30 20 M
	mouse 32 30 18 M
	mouse 0 30 18 m
	check_height 1
	check_width 17

	# A space inside the side item must not start a resize over blank space.
	$TMUX set -t inner side-status-width 14 || exit 1
	sleep 0.3
	mouse 0 "$item" 1 M
	mouse 32 "$item" 2 M
	mouse 32 30 10 M
	mouse 32 30 12 M
	check_width 14
	mouse 0 30 12 m
	[ "$($TMUX show -gv @item-drag)" = yes ] ||
	    fail 'side item did not invoke its drag binding'
	$TMUX set -g side-status-format '' || exit 1
done

# Keep the separator and one content column, also without horizontal status.
$TMUX bind -n MouseDrag1StatusDefault resize-status || exit 1
$TMUX set -t inner status off || exit 1
$TMUX set -t inner side-status left || exit 1
$TMUX set -t inner side-status-width 2 || exit 1
sleep 0.3
mouse 0 2 10 M
mouse 32 5 10 M
check_width 5
mouse 32 1 10 M
check_width 2
mouse 0 1 10 m
$TMUX set -t inner status on || exit 1
$TMUX set -t inner side-status off || exit 1

# Hiding the status line externally cancels an active resize.
position=cancel
sleep 0.3
mouse 0 30 20 M
mouse 32 30 18 M
check_height 3
$TMUX set -t inner status off || exit 1
mouse 32 30 16 M
mouse 0 30 16 m
[ "$($TMUX show -Av -t inner status)" = off ] ||
    fail 'resize reenabled a hidden status line'

# Switching session cancels the drag until release, even with more motion.
$TMUX new-session -d -s untouched -x 60 -y 20 'sleep 180' || exit 1
$TMUX set -t inner status on || exit 1
CLIENT=$($TMUX2 display -p -t outer:0.0 '#{pane_tty}') || exit 1
sleep 0.3
mouse 0 30 20 M
mouse 32 30 18 M
check_height 3
$TMUX switch-client -c "$CLIENT" -t untouched || exit 1
mouse 32 30 17 M
mouse 32 30 16 M
mouse 0 30 16 m
[ "$($TMUX show -Av -t untouched status)" = on ] ||
    fail 'resize continued in a different session'
$TMUX switch-client -c "$CLIENT" -t inner || exit 1

# A short terminal must retain a row for the window.
$TMUX set -t inner status on || exit 1
$TMUX2 resize-window -t outer:0 -y 3 || exit 1
sleep 0.5
mouse 0 30 3 M
mouse 32 30 1 M
mouse 0 30 1 m
[ "$($TMUX show -Av -t inner status)" = 2 ] ||
    fail 'status resize left no row for the window'
$TMUX2 send-keys -t outer:0.0 C-b + || exit 1
sleep 0.2
[ "$($TMUX show -Av -t inner status)" = 2 ] ||
    fail 'keyboard resize left no row for the window'

exit 0
