#!/bin/sh

# One terminal stops reading while another consumes floating-pane damage.
# The slow client must catch up after the window's shared damage is cleared.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lblocked-inner-$$ -f/dev/null"
FAST="$TEST_TMUX -Lblocked-fast-$$ -f/dev/null"
SLOW="$TEST_TMUX -Lblocked-slow-$$ -f/dev/null"
STOPPED=
cleanup()
{
	[ -n "$STOPPED" ] && kill -CONT "$STOPPED" 2>/dev/null
	$FAST kill-server 2>/dev/null
	$SLOW kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15
fail()
{
	echo "$*" >&2
	exit 1
}
wait_marker()
{
	terminal=$1
	marker=$2
	i=0
	while [ "$i" -lt 100 ]; do
		$terminal capture-pane -p >"$DIR/capture" || exit 1
		grep -q "$marker" "$DIR/capture" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client did not receive $marker"
}
mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$FAST send-keys -l "$sequence" || exit 1
	sleep 0.2
}
cat >"$DIR/emitter.pl" <<'PERL'
use strict;
use warnings;
$| = 1;
print 'READY';
while (!-e "$ENV{TRIGGER}-flood") {
	select undef, undef, undef, 0.01;
}
my $frame = 0;
while (!-e "$ENV{TRIGGER}-stop") {
	my $output = '';
	for my $row (1 .. 20) {
		$output .= "\e[$row;1H" . join('', map { chr(33 + ($_ + $frame) % 80) } 0 .. 77);
	}
	print $output;
	$frame++;
	select undef, undef, undef, 0.01;
}
for my $row (1 .. 20) {
	printf "\e[%d;1HFINAL%02d-%s", $row, $row, '0123456789' x 7;
}
print "\e[20;1HDONE";
sleep 100;
PERL
$INNER new-session -d -s inner -x 160 -y 80 \
    "TRIGGER='$DIR/trigger' perl '$DIR/emitter.pl'" || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g automatic-rename off || exit 1
$INNER set -g status-interval 0 || exit 1
$INNER set -g mouse on || exit 1
$INNER set -g pane-border-lines simple || exit 1
FLOAT=$($INNER new-pane -PF '#{pane_id}' -x 16 -y 5 -X 5 -Y 5 \
    'printf FLOAT; exec sleep 100') || exit 1
for terminal in "$FAST" "$SLOW"; do
	$terminal new-session -d -x 160 -y 80 'sleep 100' || exit 1
	$terminal set -g status off || exit 1
	$terminal set -g window-size manual || exit 1
	$terminal set -g default-terminal screen || exit 1
	$terminal respawn-pane -k "$INNER attach -t inner" || exit 1
	wait_marker "$terminal" READY
done
FASTCLIENT=$($FAST display -p '#{pane_tty}') || exit 1
SLOWCLIENT=$($SLOW display -p '#{pane_tty}') || exit 1
SLOWPID=$($SLOW display -p '#{pid}') || exit 1
before=$($INNER display -p -c "$FASTCLIENT" '#{client_written}') || exit 1
slowbefore=$($INNER display -p -c "$SLOWCLIENT" '#{client_written}') || exit 1
# The large terminal keeps this backlog below the discard threshold, so
# automatic recovery from discarded output cannot mask lost damage.
# Stop only our outer server, leaving its inner client attached to a PTY
# whose master is no longer read. This creates real terminal backpressure.
STOPPED=$SLOWPID
kill -STOP "$SLOWPID" || exit 1
: >"$DIR/trigger-flood"
i=0
while :; do
	written=$($INNER display -p -c "$SLOWCLIENT" '#{client_written}') || exit 1
	fastwritten=$($INNER display -p -c "$FASTCLIENT" '#{client_written}') || exit 1
	# More than a PTY can buffer has been queued for the stopped terminal,
	# while the other terminal is still receiving the same output.
	[ "$written" -gt "$((slowbefore + 65536))" ] &&
	    [ "$fastwritten" -gt "$((before + 65536))" ] && break
	[ "$i" -lt 100 ] || fail "slow client did not become blocked"
	sleep 0.1
	i=$((i + 1))
done
: >"$DIR/trigger-stop"
wait_marker "$FAST" DONE
[ "$($INNER display -p -c "$FASTCLIENT" '#{client_discarded}')" -eq 0 ] ||
    fail "fast client also became blocked"
mouse 0 12 6 M
mouse 32 42 6 M
mouse 0 42 6 m
[ "$($INNER display -p -t "$FLOAT" '#{pane_left}')" -eq 36 ] || fail "pane did not move"
$FAST capture-pane -p >"$DIR/fast-before" || exit 1
# Ensure the fast client already restored the vacated frame.
sed -n '6p' "$DIR/fast-before" | grep -q '^FINAL06-0123456789' ||
    fail "fast client did not restore old footprint"
kill -CONT "$SLOWPID" || exit 1
STOPPED=
wait_marker "$SLOW" DONE
# Allow queued terminal output and deferred redraws to drain.
sleep 0.5
[ "$($INNER display -p -c "$SLOWCLIENT" '#{client_discarded}')" -eq 0 ] ||
    fail "discard recovery could mask lost deferred damage"
$SLOW capture-pane -p >"$DIR/slow-before" || exit 1
diff -u "$DIR/fast-before" "$DIR/slow-before" || fail "slow client did not catch up"
$INNER refresh-client -t "$FASTCLIENT" || exit 1
sleep 0.2
$FAST capture-pane -p >"$DIR/after" || exit 1
diff -u "$DIR/fast-before" "$DIR/after" || fail "damage redraw differed from full redraw"
exit 0
