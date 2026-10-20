#!/bin/sh

# When only some lines are redrawn after a synchronized update, a line
# following a wrapped line must be drawn at its own position, not where the
# cursor happens to be.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR

INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
CONTROL=$DIR/control
EMITTER=$DIR/emitter.pl

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

wait_for_file()
{
	i=0
	while [ "$i" -lt 50 ] && [ ! -e "$1" ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ -e "$1" ] || fail "$2"
}

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER list-clients -F '#{client_termfeatures}' 2>/dev/null |
		    grep -q 'sync' && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "sync-capable client did not attach"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

my $control = $ENV{CONTROL};
(my $dir = $control) =~ s{/[^/]+$}{};

# Fill the screen with 80 column lines and no newlines so every line wraps.
my $out = "\e[H\e[2J";
for my $row (1 .. 24) {
	$out .= substr(sprintf("ROW%02d_", $row) . ('x' x 80), 0, 80);
}
syswrite STDOUT, $out . "\e[1;1H";

open my $painted, '>', "$dir/painted" or die "$dir/painted: $!\n";
close $painted;
while (!-e $control) {
	select undef, undef, undef, 0.01;
}

# Change rows 2 and 6 in one synchronized update.
my $frame = "\e[?2026h" .
    "\e[2;1H" . substr("NEW02_" . ('#' x 80), 0, 80) .
    "\e[6;1H" . substr("NEW06_" . ('%' x 80), 0, 80) .
    "\e[24;1H" .
    "\e[?2026l";
my $written = syswrite STDOUT, $frame;
die "short synchronized frame write\n"
    unless defined $written && $written == length $frame;
open my $done, '>', "$dir/done" or die "$dir/done: $!\n";
close $done;
select undef, undef, undef, 3;
PERL

$INNER new-session -d -s inner -x 80 -y 24 \
    "CONTROL='$CONTROL' perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:sync' || exit 1

$OUTER new-session -d -s outer -x 80 -y 24 \
    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" ||
    exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
wait_for_client

wait_for_file "$DIR/painted" "application did not paint"
sleep 1
: >"$CONTROL"
wait_for_file "$DIR/done" "application did not finish"
sleep 1

$OUTER capture-pane -p -t outer:0.0 >"$DIR/screen" || exit 1
sed -n 2p "$DIR/screen" | grep -q '^NEW02_#' || fail "row 2 not redrawn"
sed -n 3p "$DIR/screen" | grep -q '^ROW03_x' || fail "row 3 overwritten"
sed -n 6p "$DIR/screen" | grep -q '^NEW06_%' || fail "row 6 not redrawn"
exit 0
