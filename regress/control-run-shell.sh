#!/bin/sh

# A foreground run-shell reply belongs in one command block in control mode.
# Pane output and notifications must still reach the client during the wait.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
OUT="$DIR/out"
PID=

cleanup()
{
	: >"$DIR/release"
	exec 3>&-
	if [ -n "$PID" ]; then
		kill "$PID" 2>/dev/null
		wait "$PID" 2>/dev/null
	fi
	stop_server $TMUX || exit 1
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$1" >&2
	[ -s "$OUT" ] && cat "$OUT" >&2
	exit 1
}

$TMUX new-session -d -s shell 'read line; echo pane-test-output; sleep 60' ||
	exit 1
mkfifo "$DIR/in" || exit 1
$TMUX -C attach-session -t shell <"$DIR/in" >"$OUT" 2>&1 &
PID=$!
exec 3>"$DIR/in"
wait_until grep -q '^%session-changed ' "$OUT" || fail "client did not attach"

GATE="touch '$DIR/ready'; while [ ! -f '$DIR/release' ]; do sleep 0.05; done"
printf 'run-shell "%s; echo shell-stdout; exit 77"\n' "$GATE" >&3 || exit 1
wait_until test -f "$DIR/ready" || fail "run-shell did not start"
$TMUX send-keys -t shell go Enter || exit 1
$TMUX rename-session -t shell renamed || exit 1
wait_until grep -q '^%output .*pane-test-output' "$OUT" ||
	fail "missing pane output while run-shell was waiting"
wait_until grep -q '^%session-renamed ' "$OUT" ||
	fail "missing notification while run-shell was waiting"
: >"$DIR/release"

printf '%s\n' \
	"if-shell 'false' { display-message ok } { run-shell 'exit 78' }" \
	'detach-client' >&3 || exit 1
wait_until grep -q '^%exit' "$OUT" || fail "missing control client exit"
wait_for_exit "$PID" && wait "$PID" ||
	fail "control client did not exit cleanly"
PID=

awk '
/^%begin [0-9]+ [0-9]+ [0-9]+$/ {
	if (open)
		bad = 1
	open = $2 " " $3 " " $4
	next
}
/^%(end|error) [0-9]+ [0-9]+ [0-9]+$/ {
	if (open != $2 " " $3 " " $4)
		bad = 1
	if ($1 == "%end")
		ended[open] = 1
	open = ""
	next
}
/^%(output |session-renamed )/ {
	if (open)
		bad = 1
	next
}
/^%/ { next }
!open {
	print "unframed reply: " $0 > "/dev/stderr"
	bad = 1
	next
}
/^shell-stdout$/ { output = open }
/returned 77$/ { status = open }
/^\047exit 78\047 returned 78$/ { branch = open }
END {
	exit (bad || open || !output || output != status || !ended[output] ||
	    !branch || branch == output || !ended[branch])
}
' "$OUT" || fail "bad run-shell reply in control mode"

exit 0
