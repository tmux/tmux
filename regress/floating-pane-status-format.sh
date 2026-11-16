#!/bin/sh

# Resizing a floating pane must refresh session status formats which depend on
# its geometry, not only the pane scene and borders.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lfloating-status-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lfloating-status-outer-$$ -f/dev/null"
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

wait_outer_has_status()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		tail -1 "$CAPTURE" | grep -q "$marker" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer client status did not show $marker"
}

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.1
}

$INNER new-session -d -s inner -x 50 -y 12 'sleep 100' || exit 1
FLOAT=$($INNER new-pane -PF '#{pane_id}' -x 10 -y 5 -X 5 -Y 3 \
    'sleep 100') || fail "could not create floating pane"
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-option -g status on || exit 1
$INNER set-option -g status-position bottom || exit 1
$INNER set-option -g status-left 'WIDTH=#{pane_width}' || exit 1
$INNER set-option -g status-right '' || exit 1
$INNER set-option -g status-interval 0 || exit 1

$OUTER new-session -d -s outer -x 50 -y 12 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lfloating-status-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
OLD_WIDTH=$($INNER display-message -p -t "$FLOAT" '#{pane_width}')
wait_outer_has_status "WIDTH=$OLD_WIDTH"

RIGHT=$($INNER display-message -p -t "$FLOAT" '#{pane_right}')
TOP=$($INNER display-message -p -t "$FLOAT" '#{pane_top}')
X=$((RIGHT + 2))
Y=$((TOP + 2))

# Grab the right frame and enlarge the floating pane.
mouse 0 "$X" "$Y" M
mouse 32 "$((X + 1))" "$Y" M
mouse 32 "$((X + 8))" "$Y" M
mouse 0 "$((X + 8))" "$Y" m

NEW_WIDTH=$($INNER display-message -p -t "$FLOAT" '#{pane_width}')
[ "$NEW_WIDTH" -ne "$OLD_WIDTH" ] || fail "floating pane was not resized"
wait_outer_has_status "WIDTH=$NEW_WIDTH"

exit 0
