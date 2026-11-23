#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"

# Latin, CJK, Hangul and combining characters mixed with ASCII and tabs.
generate()
{
	awk 'BEGIN {
		for (i = 0; i < 600000; i++)
			printf "src/worker.c:%d:\tcafé naïve 日本語 中文 한글 é Ångström request=%08x\r\n", i % 997, i
	}'
}

generate >"$DIR/input-0-0"
tmux new-session -d -s bench -x 120 -y 40 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1
tmux wait-for ready-0-0
bench_run attach-session -t bench \; wait-for -S start-0-0 \; \
    wait-for done-0-0 \; run-shell "$BENCH_DRAIN" \; detach-client
