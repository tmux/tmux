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

# Read only the server's user and system CPU time, excluding its children.
bench_cpu_time()
{
	if [ "$BENCH_OS" = Linux ]; then
		awk -v ticks="$BENCH_TICKS" '
		{
			sub(/^.*\) /, "")
			printf "%.6f\n", ($12 + $13) / ticks
		}' "/proc/$BENCH_PID/stat"
	else
		ps -o time= -p "$BENCH_PID" | awk '
		NF {
			n = split($1, parts, /[-:]/)
			seconds = parts[n] + 60 * parts[n - 1]
			if (n >= 3) seconds += 3600 * parts[n - 2]
			if (n >= 4) seconds += 86400 * parts[n - 3]
			printf "%.6f\n", seconds
			found = 1
		}
		END { if (!found) exit 1 }'
	fi
}

# Call after generating input and preparing panes. The command queue must wait
# for all pane completion reports before detaching the attached client.
bench_run()
{
	BENCH_PID=$(tmux display-message -p '#{pid}')
	BENCH_OS=$(uname -s)
	if [ "$BENCH_OS" = Linux ]; then
		BENCH_TICKS=$(getconf CLK_TCK)
	fi
	before=$(bench_cpu_time)
	tmux "$@"
	after=$(bench_cpu_time)
	awk -v before="$before" -v after="$after" \
	    'BEGIN { printf "%.2f\n", after - before }' \
	    >"${BENCH_TIME:-/dev/stderr}"
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
