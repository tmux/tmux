#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"

# Latin, CJK, Hangul and combining characters mixed with ASCII and tabs.
generate()
{
	awk 'function pick(n) {
		r = (r * 48271) % 2147483647
		return 1 + r % n
	}
	BEGIN {
		r = 1
		split("worker parser session cache reader writer request index", files)
		split("café résumé façade entrée", a)
		split("naïve élève piñata jalapeño", b)
		split("日本語 東京 大阪 京都", c)
		split("中文 上海 北京 深圳", d)
		split("한글 서울 부산 대전", e)
		split("é á ö ü", f)
		split("Ångström München Zürich Göteborg", g)
		for (i = 0; i < 600000; i++)
			printf "src/%s.c:%d:\t%s %s %s %s %s %s %s " \
			    "request=%08x\r\n", files[pick(8)], i % 997,
			    a[pick(4)], b[pick(4)], c[pick(4)], d[pick(4)],
			    e[pick(4)], f[pick(4)], g[pick(4)], i
	}'
}

generate >"$DIR/input-0-0"
tmux new-session -d -s bench -x 120 -y 40 \
    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1
tmux wait-for ready-0-0
bench_run attach-session -t bench \; wait-for -S start-0-0 \; \
    wait-for done-0-0 \; run-shell "$BENCH_DRAIN" \; detach-client
