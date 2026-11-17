#!/bin/sh

# Switching windows in the same session must redraw the attached client. The
# session object is shared, so its current winlink cannot be compared after it
# has already been changed.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lswitch-redraw-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lswitch-redraw-outer-$$ -f/dev/null"
CAPTURE=$DIR/capture

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CLIENT=$($INNER list-clients -F '#{client_name}' 2>/dev/null)
		[ -n "$CLIENT" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

wait_outer_has()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		grep -q "$marker" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer client did not show $marker"
}

$INNER new-session -d -s inner -x 40 -y 8 \
    "printf '\033[2J\033[HA-WINDOW'; exec sleep 100" || exit 1
$INNER new-window -d -t inner:1 \
    "printf '\033[2J\033[HB-WINDOW'; exec sleep 100" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1

$OUTER new-session -d -s outer -x 40 -y 8 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lswitch-redraw-inner-$$ -f/dev/null attach-session -t inner:0" ||
    exit 1

wait_for_client
wait_outer_has A-WINDOW

$INNER switch-client -c "$CLIENT" -t inner:1.0 || exit 1
[ "$($INNER display-message -p -t inner '#{window_index}')" -eq 1 ] ||
    fail "server did not select window 1"
wait_outer_has B-WINDOW

exit 0
