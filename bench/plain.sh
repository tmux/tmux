#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"

# Build/log text: tabs, varied line lengths, and wrapping.
generate()
{
	awk 'function pick(n) {
		r = (r * 48271) % 2147483647
		return 1 + r % n
	}
	BEGIN {
		r = 1
		split("core net cache storage parser ui jobs protocol", dirs)
		split("worker connection table reader parser writer request index",
		    files)
		split("lookup decode flush resolve load save dispatch process",
		    funcs)
		split("request session record index module task entry connection",
		    objects)
		split("loaded saved updated removed validated queued completed retried",
		    actions)
		split("INFO WARN DEBUG ERROR", levels)
		split("warning error note", diagnostics)
		messages[1] = "unused variable"
		messages[2] = "implicit declaration of function"
		messages[3] = "incompatible pointer type"
		messages[4] = "comparison of different signedness"
		for (i = 0; i < 1100000; i++) {
			dir = dirs[pick(8)]
			file = files[pick(8)]
			func = funcs[pick(8)]
			object = objects[pick(8)]
			if (i % 3 == 0)
				printf "cc -O2 -Wall -Wextra -Iinclude -Isrc -c " \
				    "src/%s/%s.c -o obj/%s/%s.o", dir, file, dir, file
			else if (i % 3 == 1)
				printf "src/%s/%s.c:%d:%d:\t%s: %s " \
				    "\047%s\047 in %s()", dir, file, pick(997),
				    pick(80), diagnostics[pick(3)], messages[pick(4)],
				    object, func
			else
				printf "[%s] worker %d %s %s/%s id=%08x bytes=%d",
				    levels[pick(4)], pick(32), actions[pick(8)], dir,
				    object, i, pick(65536)
			if (i % 11 == 0) {
				printf " detail="
				for (j = 0; j < 24; j++)
					printf "%s=%d ", objects[1 + j % 8], i + j
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
