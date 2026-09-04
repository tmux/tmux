#!/bin/sh

# Moving a popup away from a pane prompt must recompose the prompt over the
# pane contents restored by damage redraw.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lpopup-prompt-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lpopup-prompt-outer-$$ -f/dev/null"
CAPTURE=$DIR/capture
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

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.1
}

C="sh -c 'i=1; while [ \$i -le 10 ]; do printf \"\\033[%d;1HAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\" \$i; i=\$((i + 1)); done; exec sleep 100'"

$INNER new-session -d -s inner -x 40 -y 10 "$C" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lpopup-prompt-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
$INNER command-prompt -b -P -t "$CLIENT" -p 'PROMPT-MARK>' \
    'display-message -- %1' || exit 1
wait_outer_has PROMPT-MARK

$INNER display-popup -t "$CLIENT" -x 0 -y 10 -w 16 -h 3 -E \
    "sh -c 'printf POPUP-MARKER; exec sleep 100'" &
POPUP_PID=$!
wait_outer_has POPUP-MARKER

mouse 0 10 8 M
mouse 32 11 8 M
mouse 32 25 4 M
mouse 0 25 4 m
wait_outer_has PROMPT-MARK

exit 0
