#!/bin/sh

# Check a redraw callback which has no accompanying client redraw flags. A
# wrapped row crossing a panned viewport cannot use the direct tty path.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Ldamage-only-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Ldamage-only-outer-$$ -f/dev/null"
EMITTER=$DIR/emitter.pl
TRIGGER=$DIR/trigger
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

wait_inner_has()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER capture-pane -p -t inner:0.0 2>/dev/null |
		    grep -q "$marker" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner pane did not contain $marker"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

$| = 1;
for my $row (1 .. 12) {
	print "\e[$row;1H", 'o' x 79;
}
print "\e[1;1H";
while (!-e $ENV{TRIGGER}) {
	select undef, undef, undef, 0.01;
}

my $second = ('B' x 24) . 'DAMAGE-ONLY' . ('B' x 45);
print "\e[5;1H", ('A' x 80), $second;
sleep 100;
PERL

$INNER new-session -d -s inner -x 80 -y 12 \
    "TRIGGER='$TRIGGER' perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1

$OUTER new-session -d -s outer -x 40 -y 12 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Ldamage-only-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
$INNER refresh-client -t "$CLIENT" -R 20 || exit 1
wait_outer_has oooooooooo

: >"$TRIGGER"
wait_inner_has DAMAGE-ONLY
wait_outer_has DAMAGE-ONLY

exit 0
