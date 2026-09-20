#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMPDIR=$(mktemp -d)
IN="$TMPDIR/in"
OUT="$TMPDIR/out"
BADCFG="$TMPDIR/bad.conf"
PID=

cleanup()
{
	exec 3>&- 2>/dev/null
	[ -n "$PID" ] && kill "$PID" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMPDIR"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	[ -s "$OUT" ] && cat "$OUT" >&2
	exit 1
}

wait_for()
{
	pattern=$1
	timeout=${2:-60}
	i=0

	while [ "$i" -lt "$timeout" ]; do
		grep -F -- "$pattern" "$OUT" >/dev/null 2>&1 && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "missing: $pattern"
}

wait_for_count()
{
	pattern=$1
	expected=$2
	timeout=${3:-60}
	i=0

	while [ "$i" -lt "$timeout" ]; do
		count=$(grep -F -c -- "$pattern" "$OUT" 2>/dev/null)
		[ "$count" -ge "$expected" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "missing count $expected for: $pattern"
}

send()
{
	printf '%s\n' "$1" >&3 || fail "failed to send: $1"
}

# A notification is a standalone protocol message. It must never be part of
# the response between the real %begin and matching %end or %error for a
# command. Ignore guard-looking command output while a real guard is open.
check_guards()
{
	awk '
	function notification(line) {
		return line ~ /^%(sessions-changed|session-changed |unlinked-window-add )/ ||
		    line ~ /^%(pause |continue |config-error )/
	}
	!open && $0 ~ /^%begin [0-9]+ [0-9]+ [0-9]+$/ {
		open = 1
		time = $2
		number = $3
		flags = $4
		next
	}
	open && ($0 == "%end " time " " number " " flags ||
	    $0 == "%error " time " " number " " flags) {
		open = 0
		next
	}
	notification($0) {
		seen[$1]++
		if (open) {
			print "notification inside guard: " $0 > "/dev/stderr"
			bad = 1
		}
	}
	END {
		if (open) {
			print "unterminated command guard" > "/dev/stderr"
			bad = 1
		}
		if (seen["%sessions-changed"] < 2 ||
		    seen["%session-changed"] < 1 ||
		    seen["%unlinked-window-add"] < 2 ||
		    seen["%pause"] < 1 || seen["%continue"] < 1 ||
		    seen["%config-error"] < 1) {
			print "missing expected notification" > "/dev/stderr"
			bad = 1
		}
		exit bad
	}' "$OUT" || fail "bad control protocol notification ordering"
}

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -s guard -x 80 -y 24 || exit 1
pane=$($TMUX display-message -p -t guard:0.0 '#{pane_id}') || exit 1

mkfifo "$IN" || exit 1
: >"$OUT"
$TMUX -C attach-session -t guard <"$IN" >"$OUT" 2>&1 &
PID=$!
exec 3>"$IN"

# Attaching generates a session notification synchronously from inside the
# attach command. It must follow the closing guard.
wait_for '%session-changed '

# Guard-looking output is arbitrary command output, not protocol state. Make
# new-session both generate notifications and print an unmatched fake %begin;
# it must not leave those notifications permanently deferred.
send "set-option -g @fake-begin '%begin 1 2 3'"
send "new-session -d -P -F '#{@fake-begin}' -s fake-begin"
wait_for_count '%sessions-changed' 1

# Likewise, fake %end output from a command that generates multiple
# notifications must not flush them before the command actually ends.
send "set-option -g @fake-end '%end 1 2 3'"
send "new-session -d -P -F '#{@fake-end}' -s fake-end"
wait_for_count '%sessions-changed' 2

# Exercise notification paths outside control-notify.c.
send "refresh-client -A '$pane:pause'"
wait_for "%pause $pane"
send "refresh-client -A '$pane:continue'"
wait_for "%continue $pane"

printf '%s\n' 'not-a-command' >"$BADCFG"
send "source-file '$BADCFG'"
wait_for '%config-error '

# A final command guarantees all earlier command guards have completed before
# the transcript is checked.
send "display-message -p 'guard-check-done'"
wait_for 'guard-check-done'
check_guards

exit 0
