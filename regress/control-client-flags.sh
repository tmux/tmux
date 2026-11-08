#!/bin/sh

PATH=/bin:/usr/bin TERM=screen LC_ALL=C.UTF-8 LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMPDIR=$(mktemp -d)
IN="$TMPDIR/in"
OUT="$TMPDIR/out"
PID=

cleanup() {
        exec 3>&- 2>/dev/null
        [ -n "$PID" ] && kill "$PID" 2>/dev/null
        $TMUX kill-server 2>/dev/null
        rm -rf "$TMPDIR"
}
trap cleanup EXIT

fail() {
        echo "$1" >&2
        [ -s "$OUT" ] && cat "$OUT" >&2
        exit 1
}

wait_for() {
        pattern=$1
        i=0

        while [ "$i" -lt 60 ]; do
                grep -F -- "$pattern" "$OUT" >/dev/null 2>&1 && return 0
                sleep 0.1
                i=$((i + 1))
        done
        fail "missing: $pattern"
}

send() {
        printf '%s\n' "$1" >&3 || fail "failed to send: $1"
}

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -s flags || exit 1

mkfifo "$IN" || exit 1
: >"$OUT"

$TMUX -C attach-session -t flags <"$IN" >"$OUT" 2>&1 &
PID=$!
exec 3>"$IN"

send "display-message -p ready"
wait_for ready

# Test that a valid value is accepted.
send "refresh-client -f pause-after=10"
send "display-message -p valid-flags-check-done"
wait_for valid-flags-check-done

flags=$($TMUX list-clients -F '#{client_flags}') || fail "list-clients failed"

case "$flags" in
	*pause-after=10*)
		;;
	*)
		fail "valid pause-after value was not accepted: $flags"
		;;
esac

# Clear the flag before testing invalid input.
send "refresh-client -f '!pause-after'"
send "display-message -p flag-reset-done"
wait_for flag-reset-done

flags=$($TMUX list-clients -F '#{client_flags}') || fail "list-clients failed"

case "$flags" in
	*pause-after=*)
		fail "failed to reset pause-after flag: $flags"
		;;
esac

# Test that trailing junk is rejected.
send "refresh-client -f pause-after=10abc"
send "display-message -p flags-check-done"
wait_for flags-check-done

flags=$($TMUX list-clients -F '#{client_flags}') || fail "list-clients failed"

case "$flags" in
	*pause-after=*)
		fail "invalid pause-after value was accepted: $flags"
		;;
esac

exit 0