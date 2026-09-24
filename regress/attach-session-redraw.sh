#!/bin/sh

# Selecting a pane with attach-session from an already attached client must
# redraw pane contents when window-style and window-active-style differ.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lattach-redraw-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lattach-redraw-outer-$$ -f/dev/null"
BEFORE=$DIR/before
AFTER=$DIR/after

fail()
{
	echo "$*" >&2
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

LEFT=$($INNER new-session -dPF '#{pane_id}' -s inner -x 40 -y 8 \
    "printf 'LEFT'; exec sleep 100") || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g default-terminal screen || exit 1
$INNER split-window -h -t "$LEFT" "printf 'RIGHT'; exec sleep 100" || exit 1
$INNER set-option -w -t "$LEFT" window-style bg=red || exit 1
$INNER set-option -w -t "$LEFT" window-active-style bg=blue || exit 1
$INNER bind-key -n x attach-session -t "$LEFT" || exit 1

$OUTER new-session -d -s outer -x 40 -y 8 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lattach-redraw-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
sleep 1
$OUTER send-keys -t outer:0.0 x || exit 1
sleep 1
[ "$($INNER display-message -p -t inner '#{pane_id}')" = "$LEFT" ] ||
    fail "attach-session did not select the target pane"
$OUTER capture-pane -pe -t outer:0.0 >"$BEFORE" || exit 1

# A forced redraw produces the correct scene. It must be identical to the
# scene drawn immediately by attach-session.
$INNER refresh-client -t "$CLIENT" || exit 1
sleep 1
$OUTER capture-pane -pe -t outer:0.0 >"$AFTER" || exit 1
cmp -s "$BEFORE" "$AFTER" ||
    fail "attach-session left stale active/inactive pane styles"

exit 0
