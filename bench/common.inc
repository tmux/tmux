# Common setup only. Each test creates its own panes, input and command queue.
BENCH_DIR=$(CDPATH= cd -P "$(dirname "$0")" && pwd) || exit 1
TEST_TMUX=$(CDPATH= cd -P "$BENCH_DIR/.." && pwd)/tmux
DIR=$(mktemp -d /tmp/tmux-bench.XXXXXX) || exit 1
SOCKET=$DIR/socket
BENCH_DRAIN="sh '$BENCH_DIR/drain.inc' '$TEST_TMUX' '$SOCKET' '#{client_name}'"
unset TMUX TMUX_PANE
TERM=xterm-256color
LC_ALL=C.UTF-8
export TERM LC_ALL
stty rows 40 cols 120

tmux()
{
	"$TEST_TMUX" -u -S "$SOCKET" -f "$DIR/config" "$@"
}

# Call after generating input and preparing panes. The command queue must wait
# for all pane completion reports before detaching the attached client.
bench_run()
{
	pid=$(tmux display-message -p '#{pid}') || return 1
	stats=$DIR/perf
	if ! perf stat -x, --no-big-num --no-scale --no-inherit \
	    -e instructions:u,cycles:u,duration_time -p "$pid" -o "$stats" -- \
	    "$TEST_TMUX" -u -S "$SOCKET" -f "$DIR/config" "$@"; then
		if [ -s "$stats" ]; then
			cat "$stats" >&2
		fi
		return 1
	fi

	# Sum the raw counts across PMUs on systems with mixed core types.
	if ! awk -F, '
	    $1 !~ /^[0-9]+$/ { next }
	    $3 ~ /(^|\/)instructions([:\/]u)?$/ { instructions += $1 }
	    $3 ~ /(^|\/)cycles([:\/]u)?$/ { cycles += $1 }
	    $3 == "duration_time" { elapsed = $1 / 1000000000 }
	    END {
		    if (instructions == 0 || cycles == 0 || elapsed == 0)
			    exit 1
		    printf "%.0f %.0f %.1f\n", instructions, cycles, elapsed
	    }' "$stats" >"${BENCH_COUNTS:-/dev/stderr}"; then
		printf '%s\n' 'perf stat did not report all measurements' >&2
		cat "$stats" >&2
		return 1
	fi
}

cleanup()
{
	tmux kill-server >/dev/null 2>&1 || :
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

cat >"$DIR/config" <<'EOF'
set -g status off
set -g window-size manual
set -g automatic-rename off
set -g default-terminal tmux
set -s terminal-features 'xterm*:RGB:sync:ecmasgr'
set -s disable-backoff on
EOF
# "default" leaves the option unset, using tmux's built-in history limit.
case ${BENCH_HISTORY_LIMIT:-0} in
default) ;;
*) printf 'set -g history-limit %s\n' "${BENCH_HISTORY_LIMIT:-0}" >>"$DIR/config" ;;
esac

# Repeat fixed recordings.
bench_recordings()
(
	count=$1
	shift
	for recording; do
		gzip -dc "$BENCH_DIR/recordings/$recording.out.gz"
	done >"$DIR/recordings"
	LC_ALL=C awk -v count="$count" '
	BEGIN { RS = "\0"; ORS = "" }
	{ for (i = 0; i < count; i++) printf "%s", $0 }
	' "$DIR/recordings"
)

# Single-application redraw test: repeat count, width, height, recording args.
bench_redraw()
{
	count=$1
	width=$2
	height=$3
	shift 3
	stty rows "$height" cols "$width"
	bench_recordings "$count" "$@" >"$DIR/input-0-0"
	tmux new-session -d -s bench -x "$width" -y "$height" \
	    /bin/sh "$BENCH_DIR/pane.inc" "$TEST_TMUX" "$SOCKET" 0 "$DIR" 1
	tmux wait-for ready-0-0
	bench_run attach-session -t bench \; wait-for -S start-0-0 \; \
	    wait-for done-0-0 \; run-shell "$BENCH_DRAIN" \; detach-client
}
