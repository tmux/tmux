#!/bin/sh

# Application synchronized updates must remain one physical client transaction,
# including when BSU, printable cells and ESU arrive in one pane read.

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
CLIENT_BYTES=$DIR/client-bytes
CONTROL=$DIR/control
EMITTER=$DIR/emitter.pl
ASSERT=$DIR/assert-sync-output.pl

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
		$INNER list-clients -F '#{client_termfeatures}' 2>/dev/null |
		    grep -q 'sync' && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "sync-capable client did not attach"
}

wait_for_marker()
{
	i=0
	while [ "$i" -lt 50 ]; do
		grep -q 'FRAME_000001_ROW_23' "$CLIENT_BYTES" 2>/dev/null && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client did not receive both synchronized frames"
}

wait_for_stable_bytes()
{
	previous=-1
	stable=0
	i=0
	while [ "$i" -lt 50 ]; do
		current=$(wc -c <"$CLIENT_BYTES" 2>/dev/null) || current=0
		if [ "$current" -gt 0 ] && [ "$current" -eq "$previous" ]; then
			stable=$((stable + 1))
			[ "$stable" -eq 5 ] && return 0
		else
			stable=0
		fi
		previous=$current
		sleep 0.1
		i=$((i + 1))
	done
	fail "client byte stream did not become stable"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

my $control = $ENV{CONTROL};
(my $dir = $control) =~ s{/[^/]+$}{};
open my $ready, '>', "$dir/ready" or die "$dir/ready: $!\n";
close $ready;
while (!-e $control) {
	select undef, undef, undef, 0.01;
}
for my $frame (0 .. 1) {
	my @rows;
	for my $row (0 .. 23) {
		my $marker = sprintf "FRAME_%06d_ROW_%02d_", $frame, $row;
		push @rows, substr($marker . ('X' x 79), 0, 79);
	}
	my $frame = "\e[?2026h\e[H" . join("\r\n", @rows) .
	    "\e[?2026l";
	my $written = syswrite STDOUT, $frame;
	die "short synchronized frame write\n"
	    unless defined $written && $written == length $frame;
	select undef, undef, undef, 0.2;
}
select undef, undef, undef, 2;
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

i=0
while [ "$i" -lt 50 ] && [ ! -e "$DIR/ready" ]; do
	sleep 0.1
	i=$((i + 1))
done
[ -e "$DIR/ready" ] || fail "application emitter did not become ready"
$OUTER pipe-pane -O -t outer:0.0 "cat >'$CLIENT_BYTES'" || exit 1
: >"$CONTROL"
wait_for_marker
wait_for_stable_bytes
$OUTER pipe-pane -t outer:0.0 || exit 1

cat >"$ASSERT" <<'PERL'
use strict;
use warnings;

my $path = shift;
open my $fh, '<:raw', $path or die "$path: $!\n";
local $/;
my $bytes = <$fh>;
my ($on, $off) = ("\e[?2026h", "\e[?2026l");
my ($active, %seen);

for (my $i = 0; $i < length($bytes);) {
	if (substr($bytes, $i, length($on)) eq $on) {
		die "duplicate synchronized-output start at byte $i\n" if $active;
		$active = 1;
		$i += length($on);
		next;
	}
	if (substr($bytes, $i, length($off)) eq $off) {
		die "unmatched synchronized-output end at byte $i\n" unless $active;
		$active = 0;
		$i += length($off);
		next;
	}
	if (substr($bytes, $i) =~ /\AFRAME_(\d{6})_ROW_(\d{2})_/) {
		my $marker = "$1:$2";
		die "marker $marker outside synchronized output\n" unless $active;
		$seen{$marker}++;
	}
	$i++;
}
die "unterminated synchronized-output transaction\n" if $active;
for my $frame (0 .. 1) {
	for my $row (0 .. 23) {
		my $marker = sprintf "%06d:%02d", $frame, $row;
		die "missing marker $marker\n" unless $seen{$marker};
	}
}
PERL

perl "$ASSERT" "$CLIENT_BYTES" || exit 1
