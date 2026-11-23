#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"

# Two adjacent panes scrolling below two fixed header rows.
generate()
{
	awk -v pane="$1" -v height=40 'BEGIN {
		printf "Build output: pane %d\033[2;1HFile                              Result", pane
		printf "\033[3;%dr\033[3;1H", height
		for (i = 0; i < 960000; i++)
			printf "\r\033[32m%06d\033[0m compiled src/module_%03d.c\r\n", i, i % 997
	}'
}

for pane in 0 1; do
	generate "$pane" >"$DIR/input-$pane-0"
done
tmux new-session -d -s bench -x 120 -y 40 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1
tmux split-window -h -t bench:0 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 1 "$DIR" 1
tmux wait-for ready-0-0
tmux wait-for ready-1-0
bench_run attach-session -t bench \; wait-for -S start-0-0 \; \
    wait-for -S start-1-0 \; wait-for done-0-0 \; \
    wait-for done-1-0 \; run-shell "$BENCH_DRAIN" \; detach-client
