#!/bin/sh

# Returning from the alternate screen must redraw the visible part of a
# floating pane clipped at the left edge, the top edge, or both.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Loffscreen-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Loffscreen-outer-$$ -f/dev/null"
EMITTER=$DIR/emitter.pl
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

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

$| = 1;
for my $row (1 .. 5) {
	print "\e[$row;1H", 'A' x 15;
}
while (!-e "$ENV{TRIGGER}-alternate") {
	select undef, undef, undef, 0.01;
}
print "\e[?1049h";
for my $row (1 .. 5) {
	print "\e[$row;1H", 'B' x 15;
}
while (!-e "$ENV{TRIGGER}-restore") {
	select undef, undef, undef, 0.01;
}
print "\e[?1049l";
sleep 100;
PERL

$INNER new-session -d -s inner -x 40 -y 10 'sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Loffscreen-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

for position in left top both; do
	case "$position" in
	left) x=-5; y=2 ;;
	top) x=5; y=-2 ;;
	both) x=-5; y=-2 ;;
	esac
	FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 15 -y 5 -X "$x" -Y "$y" \
	    "TRIGGER='$DIR/$position' perl '$EMITTER'") || exit 1
	[ "$($INNER display-message -p -t "$FLOAT" '#{pane_left},#{pane_top}')" = "$((x + 1)),$((y + 1))" ] ||
	    fail "$position: floating pane has unexpected position"
	wait_outer_has AAAAA
	cp "$CAPTURE" "$DIR/primary"
	: >"$DIR/$position-alternate"
	wait_outer_has BBBBB
	: >"$DIR/$position-restore"
	wait_outer_has AAAAA
	cmp -s "$DIR/primary" "$CAPTURE" ||
	    fail "$position: primary screen was not completely restored"
	$INNER kill-pane -t "$FLOAT" || exit 1
done

exit 0
