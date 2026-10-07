#!/bin/sh

# Clearing an area of a pane that is only partly visible in a client smaller
# than the window must account for status lines at the top. The area is
# clamped in window coordinates but drawn in terminal coordinates, which are
# shifted down by the status lines.
#
# Phase 1: the viewport is at the top, so the bottom of the cleared area is
# hidden; the last rows on the terminal must still be cleared.
# Phase 2: the viewport is at the bottom, so the top of the cleared area is
# hidden; this used to fail with "tty_clamp_area: y too big".

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lclamp-area-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lclamp-area-outer-$$ -f/dev/null"
CAPTURE=$DIR/capture

fail()
{
	echo "$*" >&2
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
		count=$($INNER list-clients 2>/dev/null | wc -l)
		[ "$count" -eq 1 ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

wait_for_marker()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER has-session 2>/dev/null || fail "inner server exited"
		$OUTER capture-pane -p -t outer >"$CAPTURE" || exit 1
		grep -q "$marker" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client did not receive $marker"
}

# Fill all 20 rows, then clear from the top and print a marker on a row that
# is visible in the current viewport.
cat >"$DIR/emitter.pl" <<'PERL'
use strict;
use warnings;

sub wait_for {
	my ($file) = @_;
	select undef, undef, undef, 0.01 while !-e $file;
}

$| = 1;
for my $phase (1 .. 2) {
	wait_for("$ENV{TRIGGER}-fill-$phase");
	print "\e[H", join("\r\n", ("FILL") x 20);
	wait_for("$ENV{TRIGGER}-clear-$phase");
	my $row = $phase == 1 ? 1 : 20;
	print "\e[H\e[J\e[$row;1HCLEARED$phase";
}
sleep 100;
PERL

$INNER new-session -d -s inner -x 40 -y 20 \
    "TRIGGER='$DIR/trigger' perl '$DIR/emitter.pl'" || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g status 2 || exit 1
$INNER set -g status-position top || exit 1
$INNER set -g status-interval 0 || exit 1
$INNER set -g automatic-rename off || exit 1

# The client is as large as the window, so with two status lines only 18
# window rows are visible.
$OUTER new-session -d -s outer -x 40 -y 20 'sleep 100' || exit 1
$OUTER set -g status off || exit 1
$OUTER set -g window-size manual || exit 1
$OUTER set -g default-terminal screen || exit 1
$OUTER respawn-pane -k -t outer "$INNER attach-session -t inner" || exit 1
wait_for_client
NAME=$($OUTER display-message -p -t outer '#{pane_tty}') || exit 1

for phase in 1 2; do
	if [ "$phase" -eq 1 ]; then
		PAN=-U
	else
		PAN=-D
	fi
	: >"$DIR/trigger-fill-$phase"
	sleep 0.2
	$INNER refresh-client -t "$NAME" $PAN 100 || exit 1
	sleep 0.5

	: >"$DIR/trigger-clear-$phase"
	wait_for_marker "CLEARED$phase"
	sleep 0.2
	$INNER has-session 2>/dev/null || fail "inner server exited"
	$OUTER capture-pane -p -t outer >"$CAPTURE" || exit 1
	if grep -q FILL "$CAPTURE"; then
		cat "$CAPTURE" >&2
		fail "phase $phase left rows uncleared"
	fi
done

exit 0
