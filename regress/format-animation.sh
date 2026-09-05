#!/bin/sh

# Exercise the A format modifier (format_cycle in format.c). It is expanded
# only in a status format and arms the client cycle timer to redraw the status
# line as the frames advance, so the only place its behaviour shows is a
# rendered status line.
#
# The status line is rendered by an inner tmux attached inside an outer tmux
# pane; capturing the outer pane shows what the inner tmux drew. The inner
# window runs sleep and every other status line is blanked, so the capture
# contains the frame and nothing else.
#
# Each frame lasts 700 milliseconds and the capture is repeated once a second,
# so consecutive samples always land on a different frame: no exact period is
# assumed and nothing in the test asks for a redraw, so seeing the frame change
# means the status line animated on its own.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" \
	0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

# Capture the outer pane and return the text the inner tmux drew.
capture() {
	$TMUX capturep -p >$TMP || fail "capture failed"
	tr -d '[:space:]' <$TMP
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

# Inner: a window which draws nothing and a status line which is only the
# animation, so the outer capture is the current frame on its own.
$TMUX2 new -d -x30 -y8 "sh -c 'exec sleep 100'" || exit 1
$TMUX2 set -g window-size latest || exit 1
i=1
while [ $i -le 4 ]; do
	$TMUX2 set -g status-format[$i] "" || exit 1
	i=$((i + 1))
done
$TMUX2 set -g status-format[0] "#{A/7:AAAA,BBBB,CCCC}" || exit 1

$TMUX new -d -x30 -y8 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

# Sample the frames. Every sample must be one of the frames and at least two
# different ones must be seen; status-interval is 15 seconds so only the cycle
# timer can have redrawn the status line.
seen=""
i=0
while [ $i -lt 4 ]; do
	[ $i -eq 0 ] || sleep 1
	frame=$(capture)
	case "$frame" in
	AAAA|BBBB|CCCC) ;;
	*) fail "status line is '$frame', not a frame" ;;
	esac
	case " $seen " in
	*" $frame "*) ;;
	*) seen="$seen $frame" ;;
	esac
	i=$((i + 1))
done
set -- $seen
[ $# -ge 2 ] || fail "status line did not animate, only saw$seen"

# Outside a status format there is no animation at all.
out=$($TMUX2 display-message -p "#{A:ZZZZ,YYYY}") || exit 1
[ -z "$out" ] || fail "display-message gave '$out', want empty"
out=$($TMUX2 list-panes -F "#{A:ZZZZ,YYYY}") || exit 1
[ -z "$out" ] || fail "list-panes gave '$out', want empty"

# Nor inside #(), where the frames would change the command on every frame.
$TMUX2 set -g status-format[0] '#(echo "[#{A:ZZZZ,YYYY}]")' || exit 1
sleep 1
out=$(capture)
case "$out" in
*ZZZZ*|*YYYY*) fail "job command saw a frame: '$out'" ;;
*"[]"*) ;;
*) fail "job output is '$out', want []" ;;
esac

# Empty frames and a missing or bad count must not upset the server.
for f in '#{A:}' '#{A/0:a,b}' '#{A/x:a,b}' '#{A:,,,}'; do
	$TMUX2 set -g status-format[0] "$f" || fail "setting $f failed"
	$TMUX2 has-session >/dev/null 2>&1 || fail "server lost with $f"
done
sleep 1
$TMUX2 has-session >/dev/null 2>&1 || fail "server lost after empty frames"

exit 0
