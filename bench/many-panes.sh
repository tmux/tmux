#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
stty rows 243 cols 243

# Sixteen 60x60 panes running a mixture of recorded editor and top sessions.
# Three separator columns and rows make the window 243x243.
bench_recordings 20 vim-60x60 >"$DIR/input-vim"
bench_recordings 20 emacs-60x60 >"$DIR/input-emacs"
bench_recordings 20 top-60x60 >"$DIR/input-top"
tmux new-session -d -P -F '#{pane_id} 0' -s bench -x 243 -y 243 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1 >"$DIR/panes"
pane=1
while [ "$pane" -lt 16 ]; do
	tmux split-window -d -P -F "#{pane_id} $pane" -t bench:0 \
	    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" "$pane" "$DIR" 1 >>"$DIR/panes"
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
	ln "$DIR/input-$recording" "$DIR/input-$pane-0"
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
