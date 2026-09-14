#!/bin/sh

# Damage at a popup edge must redraw complete grid characters. Drawing only a
# wide character's base or padding cell leaves a two-cell hole behind.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lpopup-wide-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lpopup-wide-outer-$$ -f/dev/null"
EMITTER=$DIR/emitter.pl
BASE=$DIR/base
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

wait_old_rows_restored()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		sed -n '3,5p' "$BASE" >"$DIR/want"
		sed -n '3,5p' "$CAPTURE" >"$DIR/got"
		cmp -s "$DIR/want" "$DIR/got" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "wide characters under the old popup edge were not restored"
}

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.1
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

binmode STDOUT, ':encoding(UTF-8)';
$| = 1;
for my $row (1 .. 10) {
	print "\e[$row;1H", chr(0x754c) x 20;
}
sleep 100;
PERL

$INNER new-session -d -s inner -x 40 -y 10 "perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lpopup-wide-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
wait_outer_has '界界界'
$OUTER capture-pane -p -t outer:0.0 >"$BASE" || exit 1

$INNER display-popup -t "$CLIENT" -x 5 -y 5 -w 10 -h 3 -E \
    "sh -c 'printf POPUP; exec sleep 100'" &
POPUP_PID=$!
wait_outer_has POPUP

# The first motion starts the drag; the second moves the popup away from its
# old rectangle. Its odd x coordinate bisects the underlying double-width
# cells at both edges.
mouse 0 10 3 M
mouse 32 11 3 M
mouse 32 28 7 M
mouse 0 28 7 m
wait_old_rows_restored

exit 0
