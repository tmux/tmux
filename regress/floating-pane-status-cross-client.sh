#!/bin/sh

# Damage redraws must show each client's own pane status, even though the
# cached status screen is shared between clients.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lstatuscc-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lstatuscc-outer-$$ -f/dev/null"
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

wait_for_marker()
{
	target=$1
	marker=$2
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t "$target" >"$CAPTURE" || exit 1
		grep -q "$marker" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client in $target did not receive $marker"
}

cat >"$DIR/emitter.pl" <<'PERL'
use strict;
use warnings;

$| = 1;
for my $phase (1 .. 4) {
	while (!-e "$ENV{TRIGGER}-$phase") {
		select undef, undef, undef, 0.01;
	}
	# Change an unused palette entry and acknowledge it in the pane body.
	print "\e]4;200;rgb:11/22/0$phase\a\e[1;1HDAMAGE$phase";
}
sleep 100;
PERL

$INNER new-session -d -s inner -x 60 -y 20 \
    "TRIGGER='$DIR/trigger' perl '$DIR/emitter.pl'" || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g automatic-rename off || exit 1
$INNER set -g status-interval 0 || exit 1
$INNER set -g pane-border-lines simple || exit 1
$INNER set -g pane-border-status top || exit 1
$INNER set -g pane-border-format 'CLIENT=<#{client_name}>' || exit 1
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 35 -y 6 -X 5 -Y 5 \
    'sleep 100') || exit 1

$OUTER new-session -d -s outer -x 121 -y 20 'sleep 100' || exit 1
$OUTER set -g status off || exit 1
$OUTER set -g window-size manual || exit 1
$OUTER set -g default-terminal screen || exit 1
LEFT=$($OUTER display-message -p -t outer:0.0 '#{pane_id}') || exit 1
RIGHT=$($OUTER split-window -h -PF '#{pane_id}' 'sleep 100') || exit 1
for target in "$LEFT" "$RIGHT"; do
	$OUTER respawn-pane -k -t "$target" "$INNER attach-session -t inner" ||
	    exit 1
done
wait_for_clients

NAME1=$($OUTER display-message -p -t "$LEFT" '#{pane_tty}') || exit 1
NAME2=$($OUTER display-message -p -t "$RIGHT" '#{pane_tty}') || exit 1

# Disable periodic status updates above and trigger damage without a command
# that also requests a status redraw. Each client must keep its own title.
$INNER refresh-client -t "$NAME1" || exit 1
$INNER refresh-client -t "$NAME2" || exit 1
sleep 0.5
for phase in 0 1 2; do
	if [ "$phase" -ne 0 ]; then
		: >"$DIR/trigger-$phase"
		wait_for_marker "$LEFT" "DAMAGE$phase"
		wait_for_marker "$RIGHT" "DAMAGE$phase"
		sleep 0.2
	fi
	for target in "$LEFT" "$RIGHT"; do
		if [ "$target" = "$LEFT" ]; then
			name=$NAME1
			other=$NAME2
		else
			name=$NAME2
			other=$NAME1
		fi
		$OUTER capture-pane -p -S 5 -E 10 -t "$target" \
		    >"$CAPTURE" || exit 1
		grep -Fq "CLIENT=<$name>" "$CAPTURE" ||
		    fail "phase $phase: missing $name's pane status"
		if grep -Fq "CLIENT=<$other>" "$CAPTURE"; then
			fail "phase $phase: $name received $other's pane status"
		fi
	done
done

# Leave just one client so a cache keyed only by client would remain stale.
$OUTER respawn-pane -k -t "$RIGHT" 'sleep 100' || exit 1
i=0
while [ "$($INNER list-clients | wc -l)" -ne 1 ]; do
	[ "$i" -lt 50 ] || fail "second client did not detach"
	sleep 0.1
	i=$((i + 1))
done
$INNER set-environment -g TEST_STATUS_VALUE initial || exit 1
$INNER set -g pane-border-format 'VALUE=#{TEST_STATUS_VALUE}' || exit 1
$INNER refresh-client -t "$NAME1" || exit 1
wait_for_marker "$LEFT" VALUE=initial
for phase in 3 4; do
	# Changing the environment does not itself request a status redraw.
	$INNER set-environment -g TEST_STATUS_VALUE "phase$phase" || exit 1
	: >"$DIR/trigger-$phase"
	wait_for_marker "$LEFT" "DAMAGE$phase"
	wait_for_marker "$LEFT" "VALUE=phase$phase"
done

exit 0
