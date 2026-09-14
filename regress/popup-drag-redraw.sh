#!/bin/sh

# Moving and resizing a popup must restore its old rectangle, including pane
# border status, on the attached client's terminal.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lpopup-drag-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lpopup-drag-outer-$$ -f/dev/null"
BASE=$DIR/base
CAPTURE=$DIR/capture
RAW=$DIR/raw
POPUP_PID=

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
	exit 1
}

cleanup()
{
	[ -n "$POPUP_PID" ] && kill "$POPUP_PID" 2>/dev/null
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

wait_rows_match()
{
	first=$1
	last=$2
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		sed -n "${first},${last}p" "$BASE" >"$DIR/want"
		sed -n "${first},${last}p" "$CAPTURE" >"$DIR/got"
		cmp -s "$DIR/want" "$DIR/got" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer rows $first-$last were not restored"
}

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.1
}

# A left-button border drag moves the popup. The first motion starts the drag;
# the second changes its position.
move_popup()
{
	mouse 0 10 1 M
	mouse 32 11 1 M
	mouse 32 26 8 M
	mouse 0 26 8 m
}

# A right-button bottom-right-border drag resizes the popup.
resize_popup()
{
	mouse 2 "$1" "$2" M
	mouse 34 "$3" "$4" M
	mouse 34 "$5" "$6" M
	mouse 2 "$5" "$6" m
}

C="sh -c 'i=0; while [ \$i -lt 20 ]; do printf \"\\033[%d;1HBG-ROW-%02d-abcdefghijklmnopqrstuvwxyz0123456789\" \$((i + 1)) \$i; i=\$((i + 1)); done; printf \"\\033[19;45H\\033[38;5;201mSCOPE\\033[0m\"; exec sleep 100'"

$INNER new-session -d -s inner -x 60 -y 20 "$C" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-option -w pane-border-status top || exit 1
$INNER set-option -w pane-border-format 'DAMAGE-STATUS-RESTORED' || exit 1

$OUTER new-session -d -s outer -x 60 -y 20 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER pipe-pane -O -t outer:0.0 "cat >'$RAW'" || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lpopup-drag-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
wait_outer_has SCOPE
grep -qa '38;5;201' "$RAW" || fail "scope colour was not drawn initially"
$OUTER capture-pane -p -t outer:0.0 >"$BASE" || exit 1
grep -q DAMAGE-STATUS-RESTORED "$BASE" ||
    fail "pane border status was not visible before popup"

$INNER display-popup -t "$CLIENT" -x 0 -y 0 -w 28 -h 6 -E \
    "sh -c 'printf POPUP-MARKER; exec sleep 100'" &
POPUP_PID=$!
wait_outer_has POPUP-MARKER
$OUTER pipe-pane -t outer:0.0 || exit 1
$OUTER pipe-pane -O -t outer:0.0 "cat >'$RAW'" || exit 1

move_popup
wait_rows_match 1 6
$OUTER pipe-pane -t outer:0.0 || exit 1
grep -qa '38;5;201' "$RAW" &&
    fail "popup move redrew content outside its old and new rectangles"
grep -q DAMAGE-STATUS-RESTORED "$CAPTURE" ||
    fail "pane border status was not restored after popup move"

# The moved popup is 28x6 at zero-based 16,7. Shrink it to 21x3, then require
# the three rows vacated at the bottom to match the original scene.
resize_popup 44 13 43 13 38 11
wait_rows_match 11 13

# Grow it back to 28x6 and require a single, intact popup frame.
resize_popup 37 10 36 10 45 14
wait_outer_has POPUP-MARKER
corners=$(grep -o '┌' "$CAPTURE" | wc -l)
[ "$corners" -eq 1 ] || fail "expected one popup frame, found $corners"

exit 0
