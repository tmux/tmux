#!/bin/sh

# Spreading must include split cells containing tiled panes, but exclude
# floating leaves and subtrees containing only floating panes.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

check()
{
	got=$($TMUX display-message -p -t "$1" "$format") || exit 1
	if [ "$got" != "$2" ]; then
		echo "$session $1: expected '$2', got '$got'" >&2
		exit 1
	fi
}

floating_geometry()
{
	$TMUX list-panes -t "$session" -f '#{pane_floating_flag}' \
	    -F '#{pane_id} #{pane_width} #{pane_height} #{pane_left} #{pane_top}'
}

for axis in h v; do
	if [ "$axis" = h ]; then
		cross=v
		format='#{pane_width} #{pane_left} #{pane_height} #{pane_top}'
	else
		cross=h
		format='#{pane_height} #{pane_top} #{pane_width} #{pane_left}'
	fi
	for number in 2 3; do
		for floating in no yes; do
			session=spread-$axis-$number-$floating
			p0=$($TMUX new-session -dP -F '#{pane_id}' \
			    -s "$session" -x200 -y200 'exec sleep 100') || exit 1
			p1=$($TMUX split-window -dP -F '#{pane_id}' \
			    -"$axis" -t "$p0" 'exec sleep 100') || exit 1
			target=$p1
			first=100
			last=99
			if [ "$number" = 3 ]; then
				target=$($TMUX split-window -dP -F '#{pane_id}' \
				    -"$axis" -t "$p1" 'exec sleep 100') || exit 1
				first=66
				last=66
			fi
			p2=$($TMUX split-window -dP -F '#{pane_id}' \
			    -"$cross" -t "$p0" 'exec sleep 100') || exit 1

			if [ "$floating" = yes ]; then
				# Floating leaves at the root and inside the tiled split.
				for pane in "$target" "$p0"; do
					$TMUX new-pane -d -t "$pane" -x20 -y8 \
					    -X10 -Y5 'exec sleep 100' || exit 1
				done
				# Retain an extra split at the root, then float all its
				# panes so it must not be counted or resized.
				f0=$($TMUX split-window -dP -F '#{pane_id}' \
				    -"$axis" -t "$target" 'exec sleep 100') || exit 1
				f1=$($TMUX split-window -dP -F '#{pane_id}' \
				    -"$cross" -t "$f0" 'exec sleep 100') || exit 1
				for pane in "$f0" "$f1"; do
					$TMUX break-pane -dW -s "$pane" \
					    -x20 -y8 -X30 -Y15 || exit 1
				done
			fi
			before=$(floating_geometry) || exit 1
			$TMUX select-layout -E -t "$target" || exit 1
			check "$p0" "$first 0 100 0"
			check "$p2" "$first 0 99 101"
			check "$p1" "$last $((first + 1)) 200 0"
			if [ "$number" = 3 ]; then
				check "$target" '66 134 200 0'
			fi
			[ "$(floating_geometry)" = "$before" ] || exit 1
		done
	done
done
