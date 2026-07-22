#!/bin/sh

# A synchronized pane update must not gain cursor visibility changes when tmux
# redraws it for a client that supports synchronized output.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR

INNER="$TEST_TMUX -Lsync-cursor-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lsync-cursor-outer-$$ -f/dev/null"
APP_BYTES=$DIR/app-bytes
CLIENT_BYTES=$DIR/client-bytes
EXPECTED=$DIR/expected
CONTROL=$DIR/control

MARKER=SYNC_CURSOR_MARKER
SYNC_ON=$(printf '\033[?2026h')
SYNC_OFF=$(printf '\033[?2026l')
CIVIS=$(printf '\033[?25l')
CNORM=$(printf '\033[?25h')

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

wait_for_bytes()
{
	file=$1
	needle=$2
	i=0
	while [ "$i" -lt 50 ]; do
		grep -F -q "$needle" "$file" 2>/dev/null && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "timed out waiting for bytes in $file"
}

wait_for_inner_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER list-clients -F '#{client_session}' 2>/dev/null |
		    grep -Fx -q inner && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

check_geometry()
{
	tmux=$1
	target=$2
	actual=$($tmux display-message -p -t "$target" \
	    '#{window_width}x#{window_height} #{pane_width}x#{pane_height}') ||
	    fail "cannot read geometry for $target"
	[ "$actual" = "80x24 80x24" ] ||
	    fail "$target geometry is $actual, expected 80x24 80x24"
}

wait_for_stable_bytes()
{
	file=$1
	previous=-1
	stable=0
	i=0
	while [ "$i" -lt 50 ]; do
		current=$(wc -c <"$file" 2>/dev/null) || current=0
		if [ "$current" -gt 0 ] && [ "$current" -eq "$previous" ]; then
			stable=$((stable + 1))
			[ "$stable" -eq 3 ] && return 0
		else
			stable=0
		fi
		previous=$current
		sleep 0.1
		i=$((i + 1))
	done
	fail "client byte stream did not become stable"
}

mkfifo "$CONTROL" || exit 1
$INNER new-session -d -s inner -x 80 -y 24 \
    "read signal <'$CONTROL'; printf '\033[?2026h%s\033[?2026l' '$MARKER'; exec sleep 30" ||
    exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:sync' || exit 1
$INNER pipe-pane -O -t inner:0.0 "cat >'$APP_BYTES'" || exit 1

$OUTER new-session -d -s outer -x 80 -y 24 \
    "$TEST_TMUX -Lsync-cursor-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1

wait_for_inner_client
check_geometry "$INNER" inner:0.0
check_geometry "$OUTER" outer:0.0

client_geometry=$($INNER list-clients -F '#{client_width}x#{client_height}') ||
    exit 1
[ "$client_geometry" = "80x24" ] ||
    fail "unexpected inner client geometry: $client_geometry"
client_features=$($INNER list-clients -F '#{client_termfeatures}') || exit 1
case ,$client_features, in
*,sync,*) ;;
*) fail "inner client does not advertise Sync: $client_features" ;;
esac

$OUTER pipe-pane -O -t outer:0.0 "cat >'$CLIENT_BYTES'" || exit 1
sleep 0.2

printf '%s%s%s' "$SYNC_ON" "$MARKER" "$SYNC_OFF" >"$EXPECTED"
printf 'go\n' >"$CONTROL" || exit 1

wait_for_bytes "$APP_BYTES" "$SYNC_OFF"
wait_for_bytes "$CLIENT_BYTES" "$MARKER"
wait_for_stable_bytes "$CLIENT_BYTES"
$OUTER pipe-pane -t outer:0.0 || exit 1
: >"$CLIENT_BYTES"
$OUTER pipe-pane -O -t outer:0.0 "cat >'$CLIENT_BYTES'" || exit 1
$INNER refresh-client || exit 1
wait_for_bytes "$CLIENT_BYTES" "$MARKER"
wait_for_bytes "$CLIENT_BYTES" "$SYNC_ON"
wait_for_bytes "$CLIENT_BYTES" "$SYNC_OFF"
wait_for_stable_bytes "$CLIENT_BYTES"

cmp -s "$EXPECTED" "$APP_BYTES" ||
    fail "application byte stream differs from the synchronized test frame"
grep -F -q "$CIVIS" "$APP_BYTES" &&
    fail "application unexpectedly emitted civis"
grep -F -q "$CNORM" "$APP_BYTES" &&
    fail "application unexpectedly emitted cnorm"
failed=0
if grep -F -q "$CIVIS" "$CLIENT_BYTES"; then
	echo "inner client emitted civis before synchronized output was committed" >&2
	failed=1
fi
if grep -F -q "$CNORM" "$CLIENT_BYTES"; then
	echo "inner client emitted cnorm after synchronized output was committed" >&2
	failed=1
fi
exit "$failed"
