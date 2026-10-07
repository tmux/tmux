#!/bin/sh

# A pane prompt redrawn after the pane scrolls stays beside the side status
# line. With side-status left the pane is not full width, so scrolling is
# redrawn as damage, which draws the prompt again over the pane.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-prompt-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-prompt-outer-$$ -f/dev/null"

DIR=$(mktemp -d) || exit 1
TRIGGER=$DIR/trigger
CAPTURE=$DIR/capture

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
	rm -rf "$DIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
	exit 1
}

wait_outer_has()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$TMUX2 capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null
		grep -q "$1" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer client did not show $1"
}

# The pane waits for the trigger, then prints enough lines to scroll.
$TMUX new-session -d -s inner -n work -x 60 -y 20 "sh -c '
	while [ ! -e $TRIGGER ]; do sleep 0.1; done
	i=0
	while [ \$i -lt 40 ]; do i=\$((i + 1)); echo line\$i; done
	exec sleep 100'" || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g automatic-rename off || exit 1
$TMUX set -g side-status left || exit 1
$TMUX2 new-session -d -s outer -x 60 -y 20 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1

i=0
while [ "$i" -lt 50 ]; do
	CLIENT=$($TMUX list-clients -F '#{client_name}' 2>/dev/null)
	[ -n "$CLIENT" ] && break
	sleep 0.1
	i=$((i + 1))
done
[ -n "$CLIENT" ] || fail "inner client did not attach"

$TMUX command-prompt -b -P -t "$CLIENT" -p PROMPTX 'display -p %%' ||
    exit 1
wait_outer_has PROMPTX

: >"$TRIGGER"
wait_outer_has line40
sleep 0.5

# The bottom row is the side status line followed by the prompt.
$TMUX2 capture-pane -p -t outer:0.0 >"$CAPTURE" || exit 1
want="$(printf '%13s' '')│PROMPTX"
got=$(tail -n 1 "$CAPTURE")
[ "$got" = "$want" ] || fail "bottom row is '$got', expected '$want'"

exit 0
