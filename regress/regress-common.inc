# Poll immediately, then at short intervals. Keep the counter in a subshell:
# predicates and callers often have their own loop variables. Allow at least
# ten seconds on slow CI machines without delaying successful checks.
wait_until()
(
	regress_tries=0
	until "$@"; do
		if [ "$regress_tries" -eq 200 ]; then
			echo "timed out: $*" >&2
			return 1
		fi
		regress_tries=$((regress_tries + 1))
		sleep 0.05
	done
)

output_is()
(
	regress_expected=$1
	shift
	regress_actual=$("$@") || return 1
	[ "$regress_actual" = "$regress_expected" ]
)

wait_for_output()
{
	wait_until output_is "$@"
}

output_has()
(
	regress_text=$1
	shift
	regress_output=$("$@") || return 1
	printf '%s\n' "$regress_output" | grep -Fq -- "$regress_text"
)

wait_for_text()
{
	wait_until output_has "$@"
}

clients_are()
(
	regress_count=$1
	shift
	regress_clients=$("$@" list-clients -F x 2>/dev/null) || return 1
	regress_actual=$(printf '%s\n' "$regress_clients" | grep -c x)
	[ "$regress_actual" -eq "$regress_count" ]
)

wait_for_clients()
{
	wait_until clients_are "$@"
}

capture_matches_file()
(
	regress_expected=$1
	regress_capture=$2
	shift 2
	"$@" >"$regress_capture" || return 1
	cmp -s "$regress_capture" "$regress_expected"
)

process_exited()
{
	! kill -0 "$1" 2>/dev/null
}

wait_for_exit()
{
	wait_until process_exited "$1"
}

# kill-server can return before the old server has closed its socket and
# reaped its panes. Wait for the process, not merely for the socket to vanish.
# Pass the tmux command and its flags, for example: stop_server $TMUX.
stop_server()
(
	[ "$#" -ne 0 ] || return 0
	regress_pid=$("$@" display-message -p '#{pid}' 2>/dev/null)
	"$@" kill-server >/dev/null 2>&1
	[ -n "$regress_pid" ] || return 0
	wait_for_exit "$regress_pid" || exit 1
)

stop_servers()
{
	stop_server $TMUX || exit 1
	stop_server $TMUX2 || exit 1
}

# macOS's default temporary directory can leave too little room for a Unix
# socket name. Use a short, private directory for tests which create sockets.
test_tmpdir()
{
	mktemp -d /tmp/tmux-regress.XXXXXX
}
