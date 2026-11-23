#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"

# Build/log text: tabs, varied line lengths, and wrapping.
generate()
{
	awk 'BEGIN {
		for (i = 0; i < 1100000; i++) {
			printf "src/worker.c:%d:\tworker %d completed request %08x status=200 path=/api/items/%d",
			    i % 997, i % 32, i, i
			if (i % 11 == 0) {
				printf " detail="
				for (j = 0; j < 24; j++) printf "argument "
			}
			printf "\r\n"
		}
	}'
}

generate >"$DIR/input-0-0"
tmux new-session -d -s bench -x 120 -y 40 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1
tmux wait-for ready-0-0
bench_run attach-session -t bench \; wait-for -S start-0-0 \; \
    wait-for done-0-0 \; run-shell "$BENCH_DRAIN" \; detach-client
