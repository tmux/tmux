#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

TMPDIR=$(mktemp -d)
IN="$TMPDIR/in"
OUT="$TMPDIR/out"
CONTROL_PID=

cleanup()
{
	exec 3>&- 2>/dev/null
	[ -n "$CONTROL_PID" ] && kill "$CONTROL_PID" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -rf "$TMPDIR"
}

fail()
{
	echo "$1" >&2
	[ -s "$OUT" ] && sed -n '1,120p' "$OUT" >&2
	cleanup
	exit 1
}

run_tmux()
{
	out=
	if command -v timeout >/dev/null 2>&1; then
		out=$(timeout 10 $TMUX "$@" 2>&1)
	else
		out=$($TMUX "$@" 2>&1)
	fi
	rc=$?
	[ "$rc" -eq 0 ] || fail "tmux $* failed ($rc): $out"
	printf '%s' "$out"
}

send_control()
{
	printf '%s\n' "$1" >&3 || fail "failed to write control command: $1"
}

wait_clients()
{
	want=$1
	i=0

	while [ "$i" -lt 50 ]; do
		have=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$have" -eq "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	return 1
}

wait_format()
{
	target=$1
	format=$2
	want=$3
	i=0

	while [ "$i" -lt 50 ]; do
		have=$($TMUX display-message -p -t "$target" "$format" 2>/dev/null)
		[ "$have" = "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	return 1
}

assert_alive()
{
	run_tmux has-session -t life >/dev/null
	fields=$(run_tmux display-message -p -t life \
	    '#{session_name}:#{window_id}:#{pane_id}:#{session_windows}:#{window_panes}')
	case "$fields" in
	life:@*:%*:*) ;;
	*) fail "bad current fields after $1: $fields" ;;
	esac
}

check_control_output()
{
	sleep 1

	if grep -E '(^%error |server exited|lost server|\(null\)|no current)' \
	    "$OUT" >/dev/null 2>&1; then
		fail "control client reported an error or invalid object"
	fi

	awk '
	$1 == "%session-window-changed" {
		if (NF != 3 || $2 !~ /^\$[0-9]+$/ || $3 !~ /^@[0-9]+$/)
			bad = 1
	}
	$1 == "%subscription-changed" {
		colon = 0
		for (i = 2; i <= NF; i++)
			if ($i == ":")
				colon = 1
		if (NF < 7 || !colon)
			bad = 1
	}
	END { exit bad }
	' "$OUT" || fail "control client received a malformed notification"
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

run_tmux new-session -d -s life -n prompt -x 80 -y 24 'sleep 1000' \
    >/dev/null
run_tmux set-option -g detach-on-destroy off >/dev/null
run_tmux new-window -t life -n tree 'sleep 1000' >/dev/null
run_tmux new-window -t life -n work 'sleep 1000' >/dev/null
run_tmux select-window -t life:prompt >/dev/null

run_tmux set-hook -g after-new-window \
    'display-message -p "hook-new #{session_name}:#{window_id}:#{pane_id}"' \
    >/dev/null
run_tmux set-hook -g after-split-window \
    'display-message -p "hook-split #{session_name}:#{window_id}:#{pane_id}"' \
    >/dev/null
run_tmux set-hook -g after-kill-pane \
    'display-message -p "hook-kill #{session_name}:#{window_id}:#{pane_id}"' \
    >/dev/null
run_tmux set-hook -g pane-exited \
    'display-message -p "hook-exit #{session_name}:#{window_id}:#{pane_id}"' \
    >/dev/null
run_tmux set-hook -g window-layout-changed \
    'display-message -p "hook-layout #{session_name}:#{window_id}:#{pane_id}"' \
    >/dev/null
run_tmux set-hook -g session-window-changed \
    'display-message -p "hook-current #{session_name}:#{window_id}:#{pane_id}"' \
    >/dev/null
run_tmux bind-key -n M-p command-prompt -P -p '(life)' \
    "set -g @lifecycle-prompt '%% #{session_name}:#{window_id}:#{pane_id}'" \
    >/dev/null

$TMUX2 new-session -d -s outer -n prompt -x 80 -y 24 "$TMUX attach -t life" \
    || fail "failed to start first attached client"
$TMUX2 new-window -t outer -n tree "$TMUX attach -t life" \
    || fail "failed to start second attached client"
wait_clients 2 || fail "normal clients did not attach"

$TMUX2 send-keys -t outer:prompt M-p || fail "failed to open command prompt"
sleep 1
run_tmux choose-tree -t life:tree.0 \
    -F 'tree #{session_name}:#{window_id}:#{pane_id}' >/dev/null
wait_format life:tree.0 '#{pane_mode}' tree-mode || \
    fail "choose-tree did not enter tree-mode"

mkfifo "$IN" || fail "failed to create control fifo"
(cat "$IN" | $TMUX -C attach -t life >"$OUT" 2>&1) &
CONTROL_PID=$!
exec 3>"$IN"
wait_clients 3 || fail "control client did not attach"

CONTROL_CLIENT=$($TMUX list-clients -F '#{client_name} #{client_control_mode}' |
    awk '$2 == 1 { print $1; exit }')
[ -n "$CONTROL_CLIENT" ] || fail "missing control client"

send_control "refresh-client -B 'all:%*:#{session_name}:#{window_id}:#{pane_id}:#{session_windows}:#{window_panes}'"
send_control "refresh-client -B 'windows:@*:#{session_name}:#{window_id}:#{window_index}:#{window_panes}'"
sleep 1

run_tmux kill-pane -t life:prompt.0 >/dev/null
run_tmux kill-window -t life:tree >/dev/null
assert_alive "killing prompt and tree panes"

i=1
while [ "$i" -le 20 ]; do
	# Keep this sequence fixed: failures should reproduce on the same pass.
	s=ld$i
	ctl=ctl$i
	idx=$((50 + i))

	run_tmux new-session -d -s "$s" -n base 'sleep 1000' >/dev/null
	run_tmux split-window -t "$s:base" 'sleep 1000' >/dev/null
	run_tmux respawn-pane -k -t "$s:base.1" 'sleep 1000' >/dev/null
	run_tmux kill-pane -t "$s:base.1" >/dev/null

	run_tmux new-window -t "$s" -n second 'sleep 1000' >/dev/null
	run_tmux link-window -s "$s:second" -t "life:$idx" >/dev/null
	run_tmux unlink-window -t "life:$idx" >/dev/null

	run_tmux new-window -t "$s" -n single 'sleep 1000' >/dev/null
	run_tmux kill-pane -t "$s:single.0" >/dev/null
	run_tmux kill-window -t "$s:base" >/dev/null

	run_tmux new-session -d -s "$ctl" -n ctl 'sleep 1000' >/dev/null
	run_tmux switch-client -c "$CONTROL_CLIENT" -t "$ctl" >/dev/null
	run_tmux kill-session -t "$ctl" >/dev/null

	run_tmux kill-session -t "$s" >/dev/null
	assert_alive "iteration $i"

	i=$((i + 1))
done

check_control_output
cleanup
exit 0
