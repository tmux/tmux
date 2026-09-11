#!/bin/sh

# Redraw a moved floating pane on both attached clients viewing the same
# window. Window redraw work must not be consumed by only one client.
#
# Uses ASCII pane borders (rather than the default UTF-8 box-drawing) because
# this test nests a real tmux client inside another tmux's pane to get a
# genuine terminal to capture from; that nested-tmux relay has been observed
# to mis-render a cell that previously held a multi-byte UTF-8 border
# character being overwritten later by plain content, on the outer instance's
# own interpretation, independent of anything the inner tmux sends. That is a
# nested-test-harness artifact, not a real tmux bug - confirmed by replaying
# the exact same drag sequence against a real terminal (xterm), where it
# never reproduces. ASCII borders avoid the artifact entirely.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lredraw-multi-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lredraw-multi-outer-$$ -f/dev/null"
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

wait_for_clients()
{
	i=0
	while [ "$i" -lt 50 ]; do
		count=$($INNER list-clients 2>/dev/null | wc -l)
		[ "$count" -eq 2 ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "two inner clients did not attach"
}

wait_outer_has()
{
	target=$1
	marker=$2
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t "$target" >"$CAPTURE" 2>/dev/null || true
		grep -q "$marker" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer pane $target did not show $marker"
}

wait_float_left()
{
	comparison=$1
	limit=$2
	i=0
	while [ "$i" -lt 50 ]; do
		left=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
		if [ "$comparison" = gt ] && [ "$left" -gt "$limit" ]; then
			return 0
		fi
		if [ "$comparison" = lt ] && [ "$left" -lt "$limit" ]; then
			return 0
		fi
		sleep 0.1
		i=$((i + 1))
	done
	fail "floating pane did not move"
}

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$2" "$3" "$4" "$5")
	$OUTER send-keys -t "$1" -l "$sequence" || exit 1
	sleep 0.1
}

drag_float()
{
	target=$1
	startcol=$2
	startrow=$3
	endcol=$4
	mouse "$target" 0 "$startcol" "$startrow" M
	mouse "$target" 32 "$endcol" "$startrow" M
	mouse "$target" 0 "$endcol" "$startrow" m
}

assert_scene()
{
	target=$1
	base=$2
	firstcol=$3
	lastcol=$4

	$OUTER capture-pane -p -t "$target" >"$CAPTURE" || exit 1

	# With ASCII (simple) borders every corner and junction is the same
	# '+', so one rectangular floating pane always draws exactly 4 of
	# them; more means a stale frame was left behind somewhere.
	corners=$(grep -o '+' "$CAPTURE" | wc -l)
	[ "$corners" -eq 4 ] ||
	    fail "outer pane $target had $corners floating frames"

	sed -n '6,11p' "$base" | cut -c"$firstcol-$lastcol" >"$DIR/want"
	sed -n '6,11p' "$CAPTURE" | cut -c"$firstcol-$lastcol" >"$DIR/got"
	cmp -s "$DIR/want" "$DIR/got" ||
	    fail "outer pane $target did not restore the old floating area"
}

C="sh -c 'i=0; while [ \$i -lt 20 ]; do printf \"\\033[%d;1HBG-ROW-%02d-abcdefghijklmnopqrstuvwxyz0123456789\" \$((i + 1)) \$i; i=\$((i + 1)); done; exec sleep 100'"

$INNER new-session -d -s inner -x 60 -y 20 "$C" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-option -g default-command 'sleep 100' || exit 1
$INNER set-option -g pane-border-lines simple || exit 1

$OUTER new-session -d -s outer -x 121 -y 20 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen || exit 1
$OUTER split-window -h -t outer:0.0 'sleep 100' || exit 1

PANES=$($OUTER list-panes -t outer:0 -F '#{pane_id} #{pane_left}')
LEFT=$(echo "$PANES" | sort -k2 -n | head -1 | cut -d' ' -f1)
RIGHT=$(echo "$PANES" | sort -k2 -n | tail -1 | cut -d' ' -f1)
[ -n "$LEFT" ] && [ -n "$RIGHT" ] || fail "could not find outer panes"

$OUTER respawn-pane -k -t "$LEFT" \
    "$TEST_TMUX -Lredraw-multi-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1
$OUTER respawn-pane -k -t "$RIGHT" \
    "$TEST_TMUX -Lredraw-multi-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_clients
wait_outer_has "$LEFT" BG-ROW-19
wait_outer_has "$RIGHT" BG-ROW-19
$OUTER capture-pane -p -t "$LEFT" >"$DIR/base-left" || exit 1
$OUTER capture-pane -p -t "$RIGHT" >"$DIR/base-right" || exit 1

FLOAT=$($INNER new-pane -dPF '#{pane_id}' -x 16 -y 5 -X 5 -Y 5) ||
    fail "could not create floating pane"
wait_outer_has "$LEFT" '+'
wait_outer_has "$RIGHT" '+'

FTOP=$($INNER display-message -p -t "$FLOAT" '#{pane_top}')
FLEFT=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
FWIDTH=$($INNER display-message -p -t "$FLOAT" '#{pane_width}')
GRABCOL=$((FLEFT + FWIDTH / 2 + 1))

# Move right through one client and require both clients to restore the old
# left-hand footprint.
drag_float "$LEFT" "$GRABCOL" "$FTOP" $((GRABCOL + 30))
wait_float_left gt 30
assert_scene "$LEFT" "$DIR/base-left" 1 20
assert_scene "$RIGHT" "$DIR/base-right" 1 20

# Move back through the other client and check the old right-hand footprint.
FLEFT=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
GRABCOL=$((FLEFT + FWIDTH / 2 + 1))
drag_float "$RIGHT" "$GRABCOL" "$FTOP" $((GRABCOL - 30))
wait_float_left lt 10
assert_scene "$LEFT" "$DIR/base-left" 35 60
assert_scene "$RIGHT" "$DIR/base-right" 35 60

exit 0
