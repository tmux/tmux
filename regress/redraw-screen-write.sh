#!/bin/sh

# Check that full and region screen-write fallbacks update an attached client,
# not only tmux's internal pane grid.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
EMITTER=$DIR/emitter.pl
CAPTURE=$DIR/capture
INNER=
OUTER=
N=0

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
	exit 1
}

cleanup()
{
	[ -n "$OUTER" ] && $OUTER kill-server 2>/dev/null
	[ -n "$INNER" ] && $INNER kill-server 2>/dev/null
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

wait_outer_lacks()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		grep -q "$marker" "$CAPTURE" || return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer client still showed $marker"
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

wait_inner_lacks()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER capture-pane -p -t inner:0.0 >"$CAPTURE" 2>/dev/null || true
		grep -q "$marker" "$CAPTURE" || return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner pane still contained $marker"
}

setup()
{
	mode=$1
	[ -n "$OUTER" ] && $OUTER kill-server 2>/dev/null
	[ -n "$INNER" ] && $INNER kill-server 2>/dev/null
	N=$((N + 1))
	INNER="$TEST_TMUX -Lredraw-write-inner-$$-$N -f/dev/null"
	OUTER="$TEST_TMUX -Lredraw-write-outer-$$-$N -f/dev/null"

	$INNER new-session -d -s inner -x 40 -y 12 \
	    "MODE=$mode READY='$DIR/ready-$N' TRIGGER='$DIR/trigger-$N' perl '$EMITTER'" ||
	    exit 1
	$INNER set-option -g status off || exit 1
	$INNER set-option -g window-size manual || exit 1

	$OUTER new-session -d -s outer -x 40 -y 12 'sleep 100' || exit 1
	$OUTER set-option -g status off || exit 1
	$OUTER set-option -g window-size manual || exit 1
	$OUTER set-option -g default-terminal screen || exit 1
	$OUTER respawn-pane -k -t outer:0.0 \
	    "$TEST_TMUX -Lredraw-write-inner-$$-$N -f/dev/null attach-session -t inner" ||
	    exit 1
}

trigger()
{
	: >"$DIR/trigger-$N-${1:-1}"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

$| = 1;
my $mode = $ENV{MODE};
my $ready = $ENV{READY};
my $trigger = $ENV{TRIGGER};

sub fill_screen {
	my ($prefix) = @_;
	print "\e[2J\e[H";
	for my $row (0 .. 11) {
		printf "\e[%d;1H%s-ROW-%02d", $row + 1, $prefix, $row;
	}
}

if ($mode eq 'ris') {
	fill_screen('RIS');
} elsif ($mode eq 'alternate') {
	fill_screen('BASE');
} elsif ($mode eq 'scroll') {
	fill_screen('SCROLL');
} else {
	die "unknown mode $mode\n";
}

open my $fh, '>', $ready or die "$ready: $!\n";
close $fh;
while (!-e "$trigger-1") {
	select undef, undef, undef, 0.01;
}

if ($mode eq 'ris') {
	print "\ec";
} elsif ($mode eq 'alternate') {
	print "\e[?1049h";
	fill_screen('ALT');
	while (!-e "$trigger-2") {
		select undef, undef, undef, 0.01;
	}
	print "\e[?1049l";
} else {
	print "\e[12;1H\r\nSCROLL-NEW";
}
sleep 100;
PERL

# RIS clears the complete screen. The source pane and attached client must both
# lose every old row.
setup ris
wait_outer_has RIS-ROW-11
trigger
wait_inner_lacks RIS-ROW
wait_outer_lacks RIS-ROW

# Leaving the alternate screen restores every row of the base screen.
setup alternate
wait_outer_has BASE-ROW-11
trigger
wait_inner_has ALT-ROW-11
wait_outer_has ALT-ROW-11
wait_outer_lacks BASE-ROW
trigger 2
wait_inner_has BASE-ROW-11
wait_outer_has BASE-ROW-11
wait_outer_lacks ALT-ROW

# Scrolling a pane which is narrower than the terminal redraws its complete
# region. Check the physical client row by row after the source grid shifts.
setup scroll
$INNER split-window -h -t inner:0 'sleep 100' || exit 1
wait_outer_has SCROLL-ROW-11
trigger
wait_inner_has SCROLL-NEW
wait_outer_has SCROLL-NEW
row=1
while [ "$row" -le 11 ]; do
	expected=$(printf 'SCROLL-ROW-%02d' "$row")
	actual=$(sed -n "${row}p" "$CAPTURE")
	case "$actual" in
	"$expected"*) ;;
	*) fail "outer row $row was not redrawn as $expected" ;;
	esac
	row=$((row + 1))
done
actual=$(sed -n '12p' "$CAPTURE")
case "$actual" in
SCROLL-NEW*) ;;
*) fail "outer bottom row was not redrawn as SCROLL-NEW" ;;
esac

exit 0
