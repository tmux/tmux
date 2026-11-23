#!/bin/sh

# Floating pane sizes include their border and any reserved scrollbar. Pane
# status is drawn on the existing border and must not add another row.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lfloating-resize-$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0 1 15

fail()
{
	echo "$type/$lines/$status: $*" >&2
	exit 1
}

check_size()
{
	got=$($TMUX display -p -t "$id" '#{pane_width}x#{pane_height}')
	[ "$got" = "$1" ] || fail "size $got, expected $1"
}

# Keep panes alive until cleanup, even on slow test runners.
$TMUX new-session -d -x 80 -y 24 'cat' || exit 1

for type in joined separate separate-active; do
	$TMUX set -w pane-border-type "$type" || exit 1
	for lines in single none; do
		pad=1
		[ "$lines" = none ] && pad=0
		full="$((80 - 2 * pad))x$((24 - 2 * pad))"
		for status in off top bottom top-floating bottom-floating; do
			$TMUX set -w pane-border-status "$status" || exit 1
			id=$($TMUX new-pane -dPF '#{pane_id}' -B "$lines" \
			    -x 20 -y 6 -X 0 -Y 0 'cat') || exit 1

			# Starting at the top edge used to add an unwanted row.
			$TMUX resize-pane -t "$id" -x 100% -y 100% || exit 1
			check_size "$full"
			$TMUX resize-pane -t "$id" -x 100% -y 100% || exit 1
			check_size "$full"

			# Explicit sizes and a pane at the bottom edge behave alike.
			$TMUX resize-pane -t "$id" -x 30 -y 10 || exit 1
			check_size "$((30 - 2 * pad))x$((10 - 2 * pad))"
			$TMUX move-pane -t "$id" -P bottom-right || exit 1
			$TMUX resize-pane -t "$id" -x 100% -y 100% || exit 1
			check_size "$full"

			# Pane options may override the window's status setting.
			$TMUX set -p -t "$id" pane-border-status off || exit 1
			$TMUX move-pane -t "$id" -P top-left || exit 1
			$TMUX resize-pane -t "$id" -y 100% || exit 1
			check_size "$full"
			$TMUX kill-pane -t "$id" || exit 1
		done
	done
done

# Only an always-on scrollbar reserves space, inside the requested footprint.
# Modal and auto-hide scrollbars overlay the pane, including in copy mode.
for lines in single none; do
	pad=1
	[ "$lines" = none ] && pad=0
	for mode in off on modal auto-hide; do
		$TMUX set -w pane-scrollbars "$mode" || exit 1
		for position in left right; do
			$TMUX set -w pane-scrollbars-position "$position" || exit 1
			for style in 'width=1,pad=0' 'width=3,pad=2'; do
				id=$($TMUX new-pane -dPF '#{pane_id}' -B "$lines" \
				    -x 20 -y 6 -X 0 -Y 0 'cat') || exit 1
				$TMUX set -p -t "$id" pane-scrollbars-style "$style" || exit 1
				reserved=0
				if [ "$mode" = on ]; then
					reserved=1
					[ "$style" = 'width=3,pad=2' ] && reserved=5
				fi
				$TMUX copy-mode -t "$id" || exit 1
				$TMUX resize-pane -t "$id" -x 100% -y 100% || exit 1
				check_size "$((80 - 2 * pad - reserved))x$((24 - 2 * pad))"
				left=$pad
				[ "$position" = left ] && left=$((left + reserved))
				got=$($TMUX display -p -t "$id" '#{pane_left}')
				[ "$got" = "$left" ] ||
				    fail "$mode/$position/$style: left $got != $left"
				$TMUX kill-pane -t "$id" || exit 1
			done
		done
	done
done

exit 0
