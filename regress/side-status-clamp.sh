#!/bin/sh

# With side-status left, a client narrower than the window must clamp pane
# output to its visible part without killing the server or drawing over the
# side status line. Each inner client runs in an outer pane of its size.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-clamp-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-clamp-outer-$$ -f/dev/null"

DIR=$(mktemp -d) || exit 1
TRIGGER=$DIR/trigger
CAPTURE=$DIR/capture

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
	rm -f "$TRIGGER" "$CAPTURE"
	rmdir "$DIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
	exit 1
}

# Start the inner server with one pane running $1, then attach a big and a
# small client from outer panes of sizes $2 and $3 (WxH).
start()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
	rm -f "$TRIGGER"

	$TMUX new-session -d -s inner -x 200 -y 50 "$1" || exit 1
	$TMUX set -g status off || exit 1
	$TMUX set -g automatic-rename off || exit 1
	$TMUX set -g window-size largest || exit 1
	$TMUX set -g side-status left || exit 1
}
attach()
{
	$TMUX2 new-session -d -s big -x "${1%x*}" -y "${1#*x}" 'sleep 100' ||
	    exit 1
	$TMUX2 set -g status off || exit 1
	$TMUX2 set -g window-size manual || exit 1
	$TMUX2 new-session -d -s small -x "${2%x*}" -y "${2#*x}" 'sleep 100' ||
	    exit 1
	$TMUX2 respawn-pane -k -t big:0.0 "$TMUX attach -t inner" || exit 1
	sleep 0.5
	$TMUX2 respawn-pane -k -t small:0.0 "$TMUX attach -t inner" || exit 1
	sleep 1
	$TMUX has-session 2>/dev/null || fail "server exited on attach"
	SMALL=$($TMUX list-clients -F '#{client_width} #{client_name}' |
	    sort -n | head -n 1 | cut -d' ' -f2)
}
alive()
{
	$TMUX has-session 2>/dev/null || fail "server exited: $1"
}

# A clear below the cursor in a pane wider than the small client's view.
start "sh -c 'while [ ! -e $TRIGGER ]; do sleep 0.1; done
	printf \"\\033[J\"; exec sleep 100'"
$TMUX set -g side-status-width 31 || exit 1
attach 167x49 40x12
: >"$TRIGGER"
sleep 1
alive "clear in a pane wider than the client"

# The same clear with the small client panned right, so the pane is cut off
# on both sides. The side status line keeps the window on its first row.
start "sh -c 'while [ ! -e $TRIGGER ]; do sleep 0.1; done
	printf \"\\033[J\"; exec sleep 100'"
$TMUX rename-window -t inner:0 work || exit 1
$TMUX set -g side-status-width 31 || exit 1
attach 167x49 40x12
$TMUX refresh-client -t "$SMALL" -R 5 || exit 1
sleep 0.5
: >"$TRIGGER"
sleep 1
alive "clear in a pane cut off on both sides"
$TMUX2 capture-pane -p -t small:0.0 >"$CAPTURE" || exit 1
case "$(head -n 1 "$CAPTURE")" in
0:work*) ;;
*) fail "side status first row cleared" ;;
esac

# Output in the right pane of a split, cut off on the right.
start 'sleep 100'
$TMUX split-window -h -d "sh -c 'while [ ! -e $TRIGGER ]; do sleep 0.1; done
	printf ab; exec sleep 100'" || exit 1
attach 120x30 80x30
: >"$TRIGGER"
sleep 1
alive "output in a pane cut off on the right"

# Panned right by less than the side status width, the left pane is cut off
# on the left. Its output is drawn after the side status line, not over it.
start "sh -c 'while [ ! -e $TRIGGER ]; do sleep 0.1; done
	printf \"%053d\" 0 | tr 0 X; exec sleep 100'"
$TMUX split-window -h -d 'sleep 100' || exit 1
attach 120x30 80x30
$TMUX refresh-client -t "$SMALL" -R 10 || exit 1
sleep 0.5
: >"$TRIGGER"
sleep 1
alive "output in a pane cut off on the left"
$TMUX2 capture-pane -p -t small:0.0 >"$CAPTURE" || exit 1
row=$(head -n 1 "$CAPTURE")
case "$row" in
?????????????│XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX│*) ;;
*) fail "row is '$row', expected the side status then 43 X" ;;
esac

# Panned into the middle of the right pane, which is cut off on both sides.
# The visible columns of the pane are drawn, counted from the pane's left.
start 'sleep 100'
$TMUX split-window -h -d -l 100 "sh -c 'while [ ! -e $TRIGGER ]; do sleep 0.1; done
	printf \"%010d\" 0 | sed s/0/0123456789/g; exec sleep 100'" || exit 1
attach 214x30 40x30
$TMUX refresh-client -t "$SMALL" -R 120 || exit 1
sleep 0.5
: >"$TRIGGER"
sleep 1
alive "output in a pane cut off on both sides"
$TMUX2 capture-pane -p -t small:0.0 >"$CAPTURE" || exit 1
row=$(head -n 1 "$CAPTURE")
case "$row" in
?????????????│01234567890123456789012345) ;;
*) fail "row is '$row', expected the side status then pane columns 20-45" ;;
esac

exit 0
