#!/bin/sh

# Check that control client flags are available in every format expansion path
# used to produce layouts. The layout strings themselves are tested separately.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export PATH TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

DIR=$(mktemp -d) || exit 1
FIFO=$DIR/input
OUT=$DIR/output
CFG=$DIR/flags.conf
PID=

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	exec 3>&-
	[ -n "$PID" ] && kill "$PID" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup EXIT

wait_for()
{
	pattern=$1
	i=0

	while [ "$i" -lt 50 ]; do
		grep -F -- "$pattern" "$OUT" >/dev/null 2>&1 && return 0
		if [ -n "$PID" ] && ! kill -0 "$PID" 2>/dev/null; then
			fail "control client exited waiting for: $pattern"
		fi
		sleep 0.1
		i=$((i + 1))
	done
	fail "missing: $pattern"
}

send()
{
	printf '%s\n' "$*" >&3
}

check_commands()
{
	name=$1
	expected=$2
	match='#{m:*new-layouts*,#{client_flags}}'

	send "display-message -p 'DISPLAY-$name $match'"
	wait_for "DISPLAY-$name $expected"
	send "list-panes -F 'PANES-$name $match'"
	wait_for "PANES-$name $expected"
	send "list-windows -F 'WINDOWS-$name $match'"
	wait_for "WINDOWS-$name $expected"
	send "list-sessions -F 'SESSIONS-$name $match'"
	wait_for "SESSIONS-$name $expected"
	send "list-clients -F 'CLIENTS-$name $match'"
	wait_for "CLIENTS-$name $expected"
}

check_config()
{
	name=$1
	expected=$2

	send "source-file '$CFG'"
	send "display-message -p 'CONFIG-$name #{@config-new-layouts}'"
	wait_for "CONFIG-$name $expected"
}

cat >"$CFG" <<'EOF'
%if #{m:*new-layouts*,#{client_flags}}
set-option -g @config-new-layouts 1
%else
set-option -g @config-new-layouts 0
%endif
EOF

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -s layouts -x 80 -y 24 || exit 1
$TMUX split-window -h -t layouts: || exit 1

mkfifo "$FIFO" || exit 1
: >"$OUT"
$TMUX -C attach-session -t layouts <"$FIFO" >"$OUT" 2>&1 &
PID=$!
exec 3>"$FIFO"

send 'display-message -p READY'
wait_for READY

check_commands OFF 0
check_config OFF 0

send 'refresh-client -f new-layouts'
check_commands ON 1
check_config ON 1

send 'refresh-client -f !new-layouts'
check_commands OFF-AGAIN 0
check_config OFF-AGAIN 0

# The list commands also run for unattached command clients. Supplying that
# client to formats must not make the session loop dereference a NULL session.
$TMUX list-sessions -F '#{S:all,active}' >/dev/null ||
	fail "session loop failed for unattached command client"
$TMUX has-session -t layouts || fail "server exited"

exit 0
