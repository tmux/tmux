#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"

# Source/diff output with both palette and truecolour attributes.
generate()
{
	awk 'BEGIN {
		for (i = 0; i < 900000; i++) {
			if (i % 2)
				printf "\033[38;2;80;190;110m+\treturn \033[38;5;180mlookup(%d, \"item-%08x\");\033[0m\r\n", i % 97, i
			else
				printf "\033[38;5;167m-\tconst char *name = \033[38;2;210;160;80m\"item-%08x\";\033[0m\r\n", i
		}
	}'
}

generate >"$DIR/input-0-0"
tmux new-session -d -s bench -x 120 -y 40 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1
tmux wait-for ready-0-0
bench_run attach-session -t bench \; wait-for -S start-0-0 \; \
    wait-for done-0-0 \; run-shell "$BENCH_DRAIN" \; detach-client
