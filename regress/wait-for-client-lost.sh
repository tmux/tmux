#!/bin/sh

# A disconnected waiter must release both its wait and its command queue.
PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT=$(mktemp -d)
TMUX="$TEST_TMUX -S$OUT/socket -f/dev/null"
pids=

cleanup()
{
	for pid in $pids; do
		kill "$pid" 2>/dev/null || true
	done
	$TMUX kill-server 2>/dev/null || true
	rm -rf "$OUT"
}
trap cleanup EXIT

fail()
{
	echo "$*" >&2
	exit 1
}

wait_list()
{
	expected=$1
	shift
	i=0
	while [ $i -lt 100 ]; do
		value=$($TMUX wait-for -l "$@") || fail "list waiters failed"
		[ "$value" = "$expected" ] && return
		i=$((i + 1))
		sleep 0.05
	done
	fail "expected waiters '$expected', got '$value'"
}

wait_free()
{
	client_pid=$1
	i=0
	while [ $i -lt 100 ]; do
		awk -v pid="$client_pid" '
		    $4 == "IDENTIFY_CLIENTPID" && $5 == pid { address = $3 }
		    address != "" && $2 == "free" && $3 == "client" &&
		    $4 == address && $5 == "(0" { freed = 1 }
		    END { exit !freed }
		' "$OUT"/tmux-server-*.log && return
		i=$((i + 1))
		sleep 0.05
	done
	fail "client $client_pid was not freed"
}

cd "$OUT" || exit 1
$TMUX -vv new-session -d -s test || fail "new-session failed"
$TMUX set -g @continued 0 || fail "set option failed"

for signal in TERM KILL; do
	for kind in channel lock event; do
		name="$kind-$signal"
		case $kind in
		channel) args="$name"; list_args="$name" ;;
		lock)
			$TMUX wait-for -L "$name" || fail "lock failed"
			args="-L $name"; list_args="$name"
			;;
		event) args="-E @$name"; list_args="-E @$name" ;;
		esac

		$TMUX wait-for $args \; set -g @continued 1 &
		dead=$!
		pids="$dead"
		wait_list "client-$dead" $list_args

		$TMUX wait-for $args &
		live=$!
		pids="$dead $live"
		wait_list "$(printf 'client-%s\nclient-%s' "$dead" "$live")" \
		    $list_args

		kill -"$signal" "$dead" || fail "kill failed"
		wait "$dead" 2>/dev/null || true
		pids="$live"
		wait_list "client-$live" $list_args
		wait_free "$dead"
		[ "$($TMUX show -gqv @continued)" = 0 ] ||
			fail "disconnected client's next command ran"

		case $kind in
		channel) $TMUX wait-for -S "$name" || fail "signal failed" ;;
		lock) $TMUX wait-for -U "$name" || fail "unlock failed" ;;
		event)
			$TMUX wait-for -E -w "client-$live" "@$name" ||
				fail "wake event waiter failed"
			;;
		esac
		wait_list "" $list_args
		wait_free "$live"
		wait "$live" || fail "surviving waiter failed"
		pids=
		if [ "$kind" = lock ]; then
			$TMUX wait-for -U "$name" || fail "final unlock failed"
		fi
	done
done

# Removing the last waiter must remove its otherwise unused channel, and must
# not create a pending signal. Unrelated pending signals must be preserved.
$TMUX wait-for -S saved || fail "save signal failed"
$TMUX wait-for last &
dead=$!
pids="$dead"
wait_list "client-$dead" last
kill -KILL "$dead" || fail "kill last waiter failed"
wait "$dead" 2>/dev/null || true
pids=
wait_list "" last
wait_free "$dead"
grep -F 'remove empty wait channel last' "$OUT"/tmux-server-*.log \
    >/dev/null || fail "empty channel was not removed"

$TMUX wait-for saved \; wait-for last &
live=$!
pids="$live"
wait_list "client-$live" last
$TMUX wait-for -S last || fail "signal last failed"
wait_free "$live"
wait "$live" || fail "pending signal was not preserved"
pids=

# Source-file leaves a cleanup callback behind the blocked command.
printf 'wait-for sourced\nset -g @continued 1\n' >"$OUT/source.conf"
$TMUX source-file "$OUT/source.conf" &
dead=$!
pids="$dead"
wait_list "client-$dead" sourced
kill -KILL "$dead" || fail "kill source-file waiter failed"
wait "$dead" 2>/dev/null || true
pids=
wait_list "" sourced
wait_free "$dead"
[ "$($TMUX show -gqv @continued)" = 0 ] ||
	fail "disconnected source-file client's next command ran"

# Firing an event after cancellation must not call the removed event sink.
$TMUX wait-for -E window-renamed &
dead=$!
pids="$dead"
wait_list "client-$dead" -E window-renamed
kill -KILL "$dead" || fail "kill event waiter failed"
wait "$dead" 2>/dev/null || true
pids=
wait_list "" -E window-renamed
wait_free "$dead"
$TMUX rename-window -t test:0 renamed || fail "rename-window failed"
$TMUX display-message -p '#{pid}' >/dev/null || fail "server exited"

exit 0
