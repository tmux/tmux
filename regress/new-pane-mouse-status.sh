#!/bin/sh

# A Ctrl-drag starting on the status line must not create a floating pane.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lmouse-status-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lmouse-status-outer-$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

wait_status()
{
	i=0
	while [ "$i" -lt 100 ]; do
		screen=$($TMUX2 capture-pane -p -t outer:0.0) || exit 1
		if printf '%s\n' "$screen" | grep -q 'WINDOW WINDOW WINDOW' &&
		    printf '%s\n' "$screen" | grep -q 'PANE PANE PANE PANE' &&
		    printf '%s\n' "$screen" | grep -q 'CONTROL CONTROL CONTROL'; then
			return
		fi
		sleep 0.05
		i=$((i + 1))
	done
	fail "status lines were not redrawn"
}

wait_option()
{
	option=$1
	want=$2
	i=0
	while [ "$i" -lt 100 ]; do
		got=$($TMUX show -gv "$option" 2>/dev/null)
		[ "$got" = "$want" ] && return
		sleep 0.05
		i=$((i + 1))
	done
	fail "got $option '$got', expected '$want'"
}

mouse()
{
	seq=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail "send mouse failed"
	sleep 0.2
}

check_drag()
{
	start=$1
	end=$2

	# First move along the status line, then into the pane. The first
	# movement establishes the drag origin without dispatching new-pane.
	mouse 16 10 "$start" M
	mouse 48 11 "$start" M
	mouse 48 16 "$end" M
	mouse 48 18 "$end" M
	mouse 16 18 "$end" m

	panes=$($TMUX list-panes -F '#{pane_id}') || fail "server exited"
	[ "$panes" = "$BASE" ] || fail "status-line drag created a pane: $panes"
}

$TMUX new-session -d -s inner -x 60 -y 20 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g default-command 'sleep 100' || exit 1
BASE=$($TMUX list-panes -F '#{pane_id}') || exit 1
$TMUX2 new-session -d -s outer -x 60 -y 20 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

check_drag 20 15

$TMUX set -g status 2 || exit 1
sleep 0.5
check_drag 20 14

$TMUX set -g status-position top || exit 1
sleep 0.5
check_drag 2 8

$TMUX set -g status on || exit 1
sleep 0.5
check_drag 1 7

# Window and pane ranges on different status rows still dispatch Ctrl-drag
# Status bindings; a control range dispatches its Ctrl-drag Control binding.
$TMUX set -g status 3 || exit 1
$TMUX set -g 'status-format[0]' '#[range=window|0]WINDOW WINDOW WINDOW#[norange]' || exit 1
$TMUX set -g 'status-format[1]' "#[range=pane|$BASE]PANE PANE PANE PANE#[norange]" || exit 1
$TMUX set -g 'status-format[2]' '#[range=control|0]CONTROL CONTROL CONTROL#[norange]' || exit 1
$TMUX bind -n C-MouseDrag1Status set -g @status-drag status || exit 1
$TMUX bind -n C-MouseDrag1Control0 set -g @status-drag control || exit 1
wait_status

row=1
for range in window pane control; do
	want=status
	[ "$range" = control ] && want=control

	$TMUX set -g @status-drag '' || exit 1
	check_drag "$row" 9
	wait_option @status-drag "$want"

	row=$((row + 1))
done

# A new drag inside the pane must still use the default Pane binding.
$TMUX set -g status on || exit 1
sleep 0.5
mouse 16 3 4 M
mouse 48 10 8 M
mouse 16 10 8 m
panes=$($TMUX list-panes -F '#{pane_floating_flag}') || fail "server exited"
[ "$panes" = "$(printf '0\n1')" ] || fail "pane drag did not create a pane"

# Starting on the status line must not dispatch Empty bindings either.
id=$($TMUX list-panes -F '#{pane_id}' | tail -n 1)
$TMUX kill-pane -t "$id" || exit 1
$TMUX break-pane -W -s "$BASE" || exit 1
$TMUX resize-pane -t "$BASE" -x 10 -y 4 || exit 1
$TMUX move-pane -t "$BASE" -P top-left || exit 1
sleep 0.5
check_drag 1 10

exit 0
