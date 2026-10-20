#!/bin/sh

# pane_at_left/right include reserved scrollbar width and padding, but not
# overlay scrollbars. Use current geometry when a pane is zoomed.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	exit 1
}

check_edges()
{
	got=$($TMUX display-message -p -t "$1" \
	    '#{pane_at_left} #{pane_at_right}') || exit 1
	[ "$got" = "$2" ] ||
		fail "$tag: $1: expected '$2', got '$got'"
}

$TMUX new-session -d -s edges -x 90 -y 30 'cat' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1

for position in left right; do
	$TMUX set -g pane-scrollbars-position "$position" || exit 1
	for style in width=1,pad=0 width=3,pad=2; do
		$TMUX set -g pane-scrollbars-style "$style" || exit 1
		for mode in off on modal auto-hide; do
			tag="$position/$style/$mode"
			$TMUX set -g pane-scrollbars "$mode" || exit 1
			check_edges edges:0.0 '1 1'

			$TMUX split-window -dh -t edges:0.0 'cat' || exit 1
			$TMUX split-window -dh -t edges:0.0 'cat' || exit 1
			check_edges edges:0.0 '1 0'
			check_edges edges:0.1 '0 0'
			check_edges edges:0.2 '0 1'

			for pane in 0 1 2; do
				$TMUX resize-pane -Z -t edges:0.$pane || exit 1
				check_edges edges:0.$pane '1 1'
				$TMUX resize-pane -Z -t edges:0.$pane || exit 1
				check_edges edges:0.0 '1 0'
				check_edges edges:0.1 '0 0'
				check_edges edges:0.2 '0 1'
			done

			# Modal scrollbars overlay the pane in copy mode, so they
			# must not change which window edges it touches.
			if [ "$mode" = modal ]; then
				$TMUX copy-mode -t edges:0.2 || exit 1
				check_edges edges:0.2 '0 1'
				$TMUX send-keys -t edges:0.2 -X cancel || exit 1
			fi
			$TMUX kill-pane -a -t edges:0.0 || exit 1
		done
	done
done

exit 0
