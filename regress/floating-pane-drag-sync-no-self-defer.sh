#!/bin/sh

# server_client_key_callback()'s mouse-drag dispatch opens a synchronized-
# output frame (tty_sync_start()) before running the drag callback, on
# every single drag motion event. server_client_check_redraw() then checks
# EVBUFFER_LENGTH(tty->out) != 0 later in the same pass to decide whether
# to defer this pass's redraw - nothing drains tty->out in between, so the
# frame-open sequence just queued (8 bytes: "\033[?2026h") makes that check
# see "outstanding output" and defer against itself, escalating the drag's
# damage to a full-window redraw on every motion event on any
# synchronized-output-capable terminal. This checks the server's own -vv
# log for that exact self-inflicted "8 left" deferral pattern during a
# drag, and requires it never appears.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
INNER="$TEST_TMUX -vv -Lsyncdefer-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lsyncdefer-outer-$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	cd /
	rm -rf "$DIR"
}
trap cleanup 0 1 15

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.15
}

$INNER new-session -d -s inner -x 40 -y 10 'sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 15 -y 5 -X 5 -Y 2 \
    'sleep 100') || exit 1

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER set-option -as terminal-features ',screen-256color:sync' || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lsyncdefer-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1
sleep 0.5

XOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
YOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_top}')
GRABCOL=$((XOFF + 3))
BORDERROW=$YOFF

# Plain (non-Alt) top-border drag: "MouseDrag1Border" -> resize-pane -M ->
# a move, since grabbing the top border moves rather than resizes. Several
# small steps, each its own drag-motion event and so its own pass through
# the code under test.
mouse 0 "$GRABCOL" "$BORDERROW" M
i=0
while [ $i -lt 6 ]; do
	GRABCOL=$((GRABCOL + 1))
	mouse 32 "$GRABCOL" "$BORDERROW" M
	i=$((i + 1))
done
mouse 0 "$GRABCOL" "$BORDERROW" m
sleep 0.3

NEWXOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
[ "$NEWXOFF" != "$XOFF" ] || fail "sanity: floating pane did not move (still at $XOFF)"

LOG=$(ls tmux-server*.log 2>/dev/null | head -1)
[ -n "$LOG" ] || fail "sanity: no server -vv log was produced"

n=$(grep -c "redraw deferred (8 left)" "$LOG")
[ "$n" -eq 0 ] ||
	fail "drag self-deferred against its own queued sync bytes $n time(s)"

exit 0
