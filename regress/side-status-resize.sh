#!/bin/sh

# Drag the separator on either side, including across the moving boundary and
# horizontal status. Check limits, release, text bindings and a second client.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$side/$status: $*" >&2
	exit 1
}

mouse()
{
	seq=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail "send mouse failed"
	sleep 0.1
}

check()
{
	i=0
	while [ "$i" -lt 50 ]; do
		width=$($TMUX show -Av -t inner side-status-width)
		window=$($TMUX display -p -t inner '#{window_width}')
		if [ "$width" = "$1" ] && [ "$window" = "$((80 - $1))" ]; then
			break
		fi
		sleep 0.1
		i=$((i + 1))
	done
	[ "$width,$window" = "$1,$((80 - $1))" ] ||
	    fail "side/window widths $width/$window, expected $1/$((80 - $1))"
	column=$1
	[ "$side" = right ] && column=$((81 - $1))
	padding=$(printf '%*s' "$((column - 1))" '')
	for target in outer other; do
		i=0
		while [ "$i" -lt 50 ]; do
			row=$($TMUX2 capture-pane -p -t "$target:0.0" |
			    sed -n "$((offset + 2))p")
			case "$row" in
			"${padding}│"*) break ;;
			esac
			sleep 0.1
			i=$((i + 1))
		done
		[ "$i" -lt 50 ] || fail "$target: wrong separator column: $row"
	done
}

$TMUX new-session -d -s inner -n zero -x 80 -y 20 'sleep 180' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g side-status-format \
    '#[range=window|0]window#[norange]' || exit 1
$TMUX bind -n MouseDrag1Status set -g @text-drag yes || exit 1
$TMUX bind -n MouseDrag1StatusDefault set -g @blank-drag yes || exit 1
$TMUX2 new-session -d -s outer -x 80 -y 20 'sleep 180' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 new-session -d -s other -x 80 -y 20 'sleep 180' || exit 1
for target in outer other; do
	$TMUX2 respawn-pane -k -t "$target:0.0" \
	    "$TMUX attach -t inner" || exit 1
done
sleep 1

for side in left right; do
	$TMUX set -t inner side-status "$side" || exit 1
	for status in off top bottom; do
		offset=0
		if [ "$status" = off ]; then
			$TMUX set -t inner status off || exit 1
		else
			$TMUX set -t inner status 2 || exit 1
			$TMUX set -t inner status-position "$status" || exit 1
			[ "$status" = top ] && offset=2
		fi
		$TMUX set -t inner side-status-width 14 || exit 1
		check 14
		if [ "$side" = left ]; then
			start=14 grow=20 shrink=10 smallest=1 largest=80
		else
			start=67 grow=61 shrink=71 smallest=80 largest=1
		fi

		# Grow into the window, then shrink past the starting point.
		mouse 0 "$start" "$((offset + 6))" M
		mouse 32 "$grow" "$((offset + 6))" M
		check 20
		mouse 32 "$shrink" 1 M
		check 10
		mouse 32 "$largest" "$((offset + 6))" M
		check 79
		mouse 32 "$smallest" "$((offset + 6))" M
		check 2
		mouse 0 "$smallest" "$((offset + 6))" m

		# Motion after release must not resize, and the separator stays
		# draggable at the minimum width.
		mouse 35 "$grow" "$((offset + 6))" M
		check 2
		sleep 0.5
		start=2
		[ "$side" = right ] && start=79
		mouse 0 "$start" "$((offset + 6))" M
		mouse 32 "$grow" "$((offset + 6))" M
		check 20
		mouse 0 "$grow" "$((offset + 6))" m
		sleep 0.5

		# Dragging text or blank content dispatches its usual binding.
		text=2
		[ "$side" = right ] && text=63
		$TMUX set -g @text-drag no || exit 1
		mouse 0 "$text" "$((offset + 1))" M
		mouse 32 "$((text + 1))" "$((offset + 1))" M
		mouse 32 40 "$((offset + 6))" M
		mouse 0 40 "$((offset + 6))" m
		got=$($TMUX show -gv @text-drag)
		[ "$got" = yes ] || fail "text drag binding did not fire"
		check 20
		sleep 0.5
		$TMUX set -g @blank-drag no || exit 1
		mouse 0 "$text" "$((offset + 8))" M
		mouse 32 40 "$((offset + 8))" M
		mouse 0 40 "$((offset + 8))" m
		got=$($TMUX show -gv @blank-drag)
		[ "$got" = yes ] || fail "blank drag binding did not fire"
		check 20

		# A modified drag on the separator does not resize it.
		mouse 16 "$grow" "$((offset + 6))" M
		mouse 48 40 "$((offset + 6))" M
		mouse 16 40 "$((offset + 6))" m
		check 20
		if [ "$status" != off ]; then
			line=1
			[ "$status" = bottom ] && line=19
			mouse 0 "$grow" "$line" M
			mouse 32 40 "$((offset + 6))" M
			mouse 0 40 "$((offset + 6))" m
			check 20
		fi
		[ "$($TMUX show -gv side-status-width)" = 14 ] ||
		    fail "drag changed the global width"

		# Double-click removes the local width and inherits the default.
		sleep 0.5
		seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' \
		    "$grow" "$((offset + 6))" "$grow" "$((offset + 6))")
		$TMUX2 send-keys -t outer:0.0 -l "$seq$seq" || exit 1
		check 14
		[ -z "$($TMUX show -v -q -t inner side-status-width)" ] ||
		    fail "double-click did not remove the local width"
	done
done

# The resize action is an ordinary binding and can be replaced or removed.
$TMUX set -t inner side-status-width 20 || exit 1
check 20
$TMUX set -g @border-drag no || exit 1
$TMUX set -g @border-end no || exit 1
$TMUX bind -n MouseDrag1StatusBorder \
    set -gF @border-drag '#{mouse_absolute_x},#{mouse_absolute_y}' || exit 1
$TMUX bind -n MouseDragEnd1StatusBorder set -g @border-end yes || exit 1
mouse 0 61 6 M
mouse 32 40 6 M
mouse 32 42 6 M
mouse 0 42 6 m
check 20
[ "$($TMUX show -gv @border-drag)" = 41,5 ] ||
    fail "custom border binding did not get the terminal coordinates"
[ "$($TMUX show -gv @border-end)" = yes ] ||
    fail "border drag end binding did not fire"
$TMUX unbind -n MouseDrag1StatusBorder || exit 1
mouse 0 61 6 M
mouse 32 40 6 M
mouse 0 40 6 m
check 20

exit 0
