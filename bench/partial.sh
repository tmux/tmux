#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
stty rows 80 cols 120

# Pan a smaller client across fully visible, clipped and hidden 60x60 panes.
bench_recordings 4 vim-60x60 >"$DIR/input-vim"
bench_recordings 4 emacs-60x60 >"$DIR/input-emacs"
bench_recordings 4 top-60x60 >"$DIR/input-top"
tmux new-session -d -P -F '#{pane_id} 0' -s bench -x 243 -y 243 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 6 >"$DIR/panes"
pane=1
while [ "$pane" -lt 16 ]; do
	tmux split-window -d -P -F "#{pane_id} $pane" -t bench:0 \
	    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" "$pane" "$DIR" 6 >>"$DIR/panes"
	tmux select-layout -t bench:0 tiled
	pane=$((pane + 1))
done
while read -r target pane; do
	set -- $(tmux display-message -p -t "$target" '#{pane_width} #{pane_height}')
	[ "$1,$2" = 60,60 ]
	case $((pane % 3)) in
	0) recording=vim ;;
	1) recording=emacs ;;
	2) recording=top ;;
	esac
	for phase in 0 1 2 3 4 5; do
		ln "$DIR/input-$recording" "$DIR/input-$pane-$phase"
	done
	tmux wait-for "ready-$pane-0"
done <"$DIR/panes"

set -- attach-session -t bench
phase=0
for pan in origin R37 D31 R83 D97 L53; do
	if [ "$pan" != origin ]; then
		direction=$(printf '%.1s' "$pan")
		set -- "$@" \; refresh-client "-$direction" "${pan#?}"
	fi
	pane=0
	while [ "$pane" -lt 16 ]; do
		set -- "$@" \; wait-for -S "start-$pane-$phase"
		pane=$((pane + 1))
	done
	pane=0
	while [ "$pane" -lt 16 ]; do
		set -- "$@" \; wait-for "done-$pane-$phase"
		pane=$((pane + 1))
	done
	phase=$((phase + 1))
done
bench_run "$@" \; run-shell "$BENCH_DRAIN" \; detach-client
