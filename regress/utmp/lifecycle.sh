#!/bin/sh
# Invoked by build-and-test.sh after both builds succeed. All commands use a
# private explicit socket, empty TMUX, and a private config (never user config).
# Logs retain operation, fd, tmux server/pane identity, and fd validity.

set -eu
build=$1
variant=$2
binary=$build/tmux
UTMP_FIXTURE_ZERO_SUCCESS=${UTMP_FIXTURE_ZERO_SUCCESS:-0}
export UTMP_FIXTURE_ZERO_SUCCESS
LC_ALL=C
TERM=screen
export LC_ALL TERM
unset TMUX TMUX_PANE LD_PRELOAD DYLD_INSERT_LIBRARIES

fail()
{
	echo "FAIL ($variant): $*" >&2
	exit 1
}

# No server may be started before this static-link safety gate succeeds.
nm "$binary" >"$build/symbols.log"
case $(uname -s) in
Darwin) otool -L "$binary" >"$build/libraries.log" ;;
*) ldd "$binary" >"$build/libraries.log" ;;
esac
if grep -i utempter "$build/libraries.log" >/dev/null; then
	fail "a dynamic utempter dependency is forbidden"
fi
case $variant in
enabled)
	grep '^DEFS =.* -DHAVE_UTEMPTER=1' "$build/Makefile" >/dev/null ||
	    fail "HAVE_UTEMPTER missing"
	for symbol in utmp_fixture_only utempter_add_record utempter_remove_record; do
		grep -E " [Tt] _?$symbol\$" "$build/symbols.log" >/dev/null ||
		    fail "missing static fixture symbol: $symbol"
	done
	;;
disabled)
	if grep '^DEFS =.* -DHAVE_UTEMPTER' "$build/Makefile" >/dev/null ||
	    grep utempter_ "$build/symbols.log" >/dev/null; then
		fail "disabled binary includes utempter"
	fi
	;;
*) fail "unknown variant" ;;
esac

# Unix socket paths have a small fixed limit; use a short private /tmp path.
socket_dir=$(mktemp -d /tmp/tmux-utmp.XXXXXXXX)
socket=$socket_dir/socket
holder_pid=
cleanup()
{
	"$binary" -S "$socket" -f /dev/null kill-server 2>/dev/null || :
	if [ -n "$holder_pid" ]; then
		kill "$holder_pid" 2>/dev/null || :
	fi
	rm -rf "$socket_dir"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

tm()
{
	"$binary" -S "$socket" -f "$config" "$@"
}

start_case()
{
	name=$1-zero-success-$UTMP_FIXTURE_ZERO_SUCCESS
	config=$build/$name.conf
	UTMP_FIXTURE_LOG=$build/$name.events
	UTMP_FIXTURE_FAIL_ADD=$build/$name.fail-add
	export UTMP_FIXTURE_LOG UTMP_FIXTURE_FAIL_ADD
	rm -f "$UTMP_FIXTURE_FAIL_ADD"
	: >"$config"
	: >"$UTMP_FIXTURE_LOG"
	expected=$build/$name.expected
	: >"$expected"
}

expect()
{
	if [ "$variant" = enabled ]; then
		printf '%s %s\n' "$1" "$2" >>"$expected"
	fi
}

check()
{
	# Check the complete history, including server identity and valid PTY fd.
	awk -v pid="$server_pid" '
	NF != 4 || $2 !~ /^[0-9]+$/ || $4 != "valid" { exit 1 }
	$3 !~ /^tmux\([0-9]+\)\.%[0-9]+$/ { exit 1 }
	{
		identity = $3
		sub(/^tmux\(/, "", identity)
		sub(/\).*/, "", identity)
		if (identity != pid) exit 1
		sub(/^.*\./, "", $3)
		print $1, $3
	}' "$UTMP_FIXTURE_LOG" >"$build/$name.actual" ||
	    fail "$name: invalid fixture event ($*)"
	diff -u "$expected" "$build/$name.actual" ||
	    fail "$name: unexpected accounting ($*)"
}

wait_dead()
{
	i=0
	while [ "$i" -lt 100 ]; do
		[ "$(tm display-message -p -t "$1" '#{pane_dead}')" = 1 ] && return
		sleep 0.1
		i=$((i + 1))
	done
	fail "pane $1 did not exit"
}

stop_case()
{
	tm kill-server
	i=0
	while kill -0 "$server_pid" 2>/dev/null; do
		[ "$i" -lt 100 ] || fail "$name: server did not exit"
		sleep 0.1
		i=$((i + 1))
	done
	check shutdown
	echo "PASS $variant: $name"
}

start_case lifecycle
p0=$(tm new-session -d -P -F '#{pane_id}' -s test 'sleep 600')
server_pid=$(tm display-message -p '#{pid}')
[ "$(tm show-options -sv utmp)" = on ] || fail "default is not on"
expect add "$p0"
check default-on
tm set-option -s utmp on
tm set-option -s utmp on
check repeated-on
tm set-option -s utmp off
expect remove "$p0"
tm set-option -s utmp off
check repeated-off
p1=$(tm split-window -d -P -F '#{pane_id}' -t "$p0" 'sleep 600')
tm respawn-pane -k -t "$p1" 'sleep 600'
tm kill-pane -t "$p1"
check creation-respawn-deletion-off
tm set-option -s utmp on
expect add "$p0"
check runtime-on
tm set-option -su utmp
check unset-on
tm set-option -s utmp off
expect remove "$p0"
tm set-option -su utmp
expect add "$p0"
[ "$(tm show-options -sv utmp)" = on ] || fail "unset did not restore on"
check unset-off-restores-default
tm respawn-pane -k -t "$p0" 'sleep 600'
expect remove "$p0"
expect add "$p0"
check respawn-on
p1=$(tm split-window -d -P -F '#{pane_id}' -t "$p0" 'sleep 600')
expect add "$p1"
check split-on
tm kill-pane -t "$p1"
expect remove "$p1"
check kill-pane
p1=$(tm new-window -d -P -F '#{pane_id}' -t test 'sleep 600')
expect add "$p1"
tm kill-window -t "$p1"
expect remove "$p1"
check new-and-kill-window
p1=$(tm new-window -d -P -F '#{pane_id}' -t test '')
check empty-pane
tm set-option -s utmp off
expect remove "$p0"
tm set-option -s utmp on
expect add "$p0"
check empty-pane-transitions
tm respawn-pane -k -t "$p1" 'sleep 600'
expect add "$p1"
tm kill-pane -t "$p1"
expect remove "$p1"
check empty-pane-respawn
p1=$(tm new-window -d -P -F '#{pane_id}' -t test 'read value')
expect add "$p1"
tm set-option -w -t "$p1" remain-on-exit on
tm send-keys -t "$p1" Enter
wait_dead "$p1"
expect remove "$p1"
check remain-on-exit
tm set-option -s utmp off
expect remove "$p0"
tm set-option -s utmp on
expect add "$p0"
check dead-pane-not-registered
tm respawn-pane -t "$p1" 'sleep 600'
expect add "$p1"
[ "$(tm display-message -p -t "$p1" '#{pane_dead}')" = 0 ] ||
    fail "respawned retained pane is still dead"
check dead-pane-respawn
tm kill-pane -t "$p1"
expect remove "$p1"
check respawned-pane-deletion
expect remove "$p0"
stop_case

start_case exited-draining
UTMP_FIXTURE_HOLDER=$build/$name.holder
export UTMP_FIXTURE_HOLDER
rm -f "$UTMP_FIXTURE_HOLDER"
p0=$(tm new-session -d -P -F '#{pane_id}' -s test 'sleep 600')
server_pid=$(tm display-message -p '#{pid}')
expect add "$p0"
p1=$(tm new-window -d -P -F '#{pane_id}' -t test /bin/sh -c \
    'read value; dd if=/dev/zero bs=65536 count=64 2>/dev/null; exit 23')
expect add "$p1"
# Pending pipe output prevents pane destruction after SIGCHLD and PTY EOF.
# shellcheck disable=SC2016
tm pipe-pane -O -t "$p1" 'echo "$$" > "$UTMP_FIXTURE_HOLDER"; exec sleep 60'
tm send-keys -t "$p1" Enter
i=0
while [ "$(tm display-message -p -t "$p1" '#{pane_dead_status}')" != 23 ]; do
	if [ "$i" -ge 100 ]; then
		tm display-message -p -t "$p1" \
		    'pid=#{pane_pid} dead=#{pane_dead} status=#{pane_dead_status} signal=#{pane_dead_signal}' >&2
		tm capture-pane -p -t "$p1" >&2
		fail "draining pane exit was not processed"
	fi
	sleep 0.1
	i=$((i + 1))
done
holder_pid=$(cat "$UTMP_FIXTURE_HOLDER")
case $holder_pid in
''|*[!0-9]*) fail "invalid PTY holder pid" ;;
esac
kill -0 "$holder_pid" || fail "PTY holder exited early"
[ "$(tm display-message -p -t "$p1" '#{pane_dead}')" = 0 ] ||
    fail "exited pane did not retain its PTY fd"
check exited-with-fd-retained
tm set-option -s utmp off
expect remove "$p0"
expect remove "$p1"
tm set-option -s utmp on
expect add "$p0"
tm set-option -s utmp on
check exited-pane-not-reregistered
[ "$(tm display-message -p -t "$p1" '#{pane_dead}')" = 0 ] ||
    fail "PTY closed before transition assertion"
tm kill-pane -t "$p1"
kill "$holder_pid"
holder_pid=
check exited-pane-deletion
expect remove "$p0"
stop_case

start_case startup-off
printf 'set-option -s utmp off\n' >"$config"
p0=$(tm new-session -d -P -F '#{pane_id}' -s test 'sleep 600')
server_pid=$(tm display-message -p '#{pid}')
[ "$(tm show-options -sv utmp)" = off ] || fail "startup setting ignored"
check startup-off
p1=$(tm split-window -d -P -F '#{pane_id}' -t "$p0" 'sleep 600')
p2=$(tm new-window -d -P -F '#{pane_id}' -t test 'sleep 600')
check multiple-panes-created-off
tm set-option -s utmp on
expect add "$p0"
expect add "$p1"
expect add "$p2"
tm set-option -s utmp on
check runtime-on-registers-all-live-panes
tm set-option -s utmp off
expect remove "$p0"
expect remove "$p1"
expect remove "$p2"
check runtime-off-removes-all-panes
tm set-option -s utmp on
expect add "$p0"
expect add "$p1"
expect add "$p2"
tm kill-pane -t "$p1"
expect remove "$p1"
tm kill-pane -t "$p2"
expect remove "$p2"
expect remove "$p0"
stop_case

start_case shutdown-off
printf 'set-option -s utmp off\n' >"$config"
p0=$(tm new-session -d -P -F '#{pane_id}' -s test 'sleep 600')
server_pid=$(tm display-message -p '#{pid}')
stop_case

start_case failures
: >"$UTMP_FIXTURE_FAIL_ADD"
p0=$(tm new-session -d -P -F '#{pane_id}' -s test 'sleep 600')
server_pid=$(tm display-message -p '#{pid}')
expect add-failed "$p0"
check add-failure
tm set-option -s utmp on
check failed-add-not-repeated
tm respawn-pane -k -t "$p0" 'sleep 600'
expect remove-after-failed-add "$p0"
expect add-failed "$p0"
check failed-add-respawn-cleanup
p1=$(tm new-window -d -P -F '#{pane_id}' -t test 'sleep 600')
expect add-failed "$p1"
tm kill-pane -t "$p1"
expect remove-after-failed-add "$p1"
check failed-add-deletion-cleanup
tm set-option -s utmp off
expect remove-after-failed-add "$p0"
tm set-option -s utmp off
check failed-add-removal-attempt-once
rm "$UTMP_FIXTURE_FAIL_ADD"
tm set-option -s utmp on
expect add "$p0"
check add-recovery
expect remove "$p0"
stop_case
