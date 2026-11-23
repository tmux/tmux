#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
stty rows 163 cols 241

# Eight 120x40 tiled panes behind eight overlapping 60x60 floating panes.
for recording in vim emacs top; do
	bench_recordings 8 "$recording-120x40" >"$DIR/input-$recording-tiled"
	bench_recordings 8 "$recording-60x60" >"$DIR/input-$recording-floating"
done
tmux new-session -d -P -F '#{pane_id} 0' -s bench -x 241 -y 163 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1 >"$DIR/panes"
tmux set-option -t bench:0 tiled-layout-max-columns 2
pane=1
while [ "$pane" -lt 8 ]; do
	tmux split-window -d -P -F "#{pane_id} $pane" -t bench:0 \
	    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" "$pane" "$DIR" 1 >>"$DIR/panes"
	tmux select-layout -t bench:0 tiled
	pane=$((pane + 1))
done
while [ "$pane" -lt 16 ]; do
	row=$(((pane - 8) / 4))
	column=$(((pane - 8) % 4))
	x=$((12 + 51 * column))
	y=$((12 + 75 * row + 8 * (column % 2)))
	tmux new-pane -d -P -F "#{pane_id} $pane" -t bench:0 \
	    -x 62 -y 62 -X "$x" -Y "$y" \
	    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" "$pane" "$DIR" 1 >>"$DIR/panes"
	pane=$((pane + 1))
done
while read -r target pane; do
	set -- $(tmux display-message -p -t "$target" '#{pane_width} #{pane_height}')
	if [ "$pane" -lt 8 ]; then
		[ "$1,$2" = 120,40 ]
		kind=tiled
	else
		[ "$1,$2" = 60,60 ]
		kind=floating
	fi
	case $((pane % 3)) in
	0) recording=vim ;;
	1) recording=emacs ;;
	2) recording=top ;;
	esac
	ln "$DIR/input-$recording-$kind" "$DIR/input-$pane-0"
	tmux wait-for "ready-$pane-0"
done <"$DIR/panes"

set -- attach-session -t bench
pane=0
while [ "$pane" -lt 16 ]; do
	set -- "$@" \; wait-for -S "start-$pane-0"
	pane=$((pane + 1))
done
pane=0
while [ "$pane" -lt 16 ]; do
	set -- "$@" \; wait-for "done-$pane-0"
	pane=$((pane + 1))
done
bench_run "$@" \; run-shell "$BENCH_DRAIN" \; detach-client
