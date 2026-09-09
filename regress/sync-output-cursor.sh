#!/bin/sh

# A synchronized update elsewhere in the pane must not hide a stationary
# cursor while tmux waits for the end of the update.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

if [ "$#" -eq 0 ]; then
	TEST_TMUX="$TEST_TMUX" sh "$0" pane || exit 1
	TEST_TMUX="$TEST_TMUX" sh "$0" prompt || exit 1
	exit 0
fi
case $1 in
pane|prompt) MODE=$1 ;;
*) echo "usage: $0 [pane|prompt]" >&2; exit 1 ;;
esac

DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR

INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
CLIENT_BYTES=$DIR/client-bytes
CONTROL=$DIR/control
EMITTER=$DIR/emitter.pl
ASSERT=$DIR/assert-cursor.pl

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

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

my $control = $ENV{CONTROL};
(my $dir = $control) =~ s{/[^/]+$}{};

# Draw a prompt and leave its cursor stationary on the last line.
syswrite STDOUT, "\e[2J\e[HWorking .\e[24;1H> ";
open my $ready, '>', "$dir/ready" or die "$dir/ready: $!\n";
close $ready;
while (!-e $control) {
	select undef, undef, undef, 0.01;
}

# Deliberately split BSU, content, and ESU across pane reads. This mirrors an
# animation producer which begins a frame before it has finished rendering it.
for my $spinner (qw(| / - \\)) {
	syswrite STDOUT, "\e[?2026h";
	select undef, undef, undef, 0.05;
	syswrite STDOUT, "\e[1;9H$spinner\e[24;3H";
	select undef, undef, undef, 0.01;
	syswrite STDOUT, "\e[?2026l";
	select undef, undef, undef, 0.15;
}
select undef, undef, undef, 5;
PERL

cat >"$ASSERT" <<'PERL'
use strict;
use warnings;

my $path = shift;
open my $fh, '<:raw', $path or die "$path: $!\n";
local $/;
my $bytes = <$fh>;
my ($bsu, $esu, $hide, $show) =
    ("\e[?2026h", "\e[?2026l", "\e[?25l", "\e[?25h");
my ($sync, $outside, $hides, $shows) = (0, 0, 0, 0);

for (my $i = 0; $i < length($bytes);) {
	if (substr($bytes, $i, length($bsu)) eq $bsu) {
		$sync++;
		$i += length($bsu);
		next;
	}
	if (substr($bytes, $i, length($esu)) eq $esu) {
		$sync-- if $sync;
		$i += length($esu);
		next;
	}
	if (substr($bytes, $i, length($hide)) eq $hide) {
		$hides++;
		$outside++ unless $sync;
		$i += length($hide);
		next;
	}
	if (substr($bytes, $i, length($show)) eq $show) {
		$shows++;
		$outside++ unless $sync;
		$i += length($show);
		next;
	}
	$i++;
}

printf "%s cursor hides: %d; cursor shows: %d; changes outside sync: %d\n",
    $ENV{MODE}, $hides, $shows, $outside;
die "stationary cursor changed outside synchronized output\n" if $outside;
PERL

$INNER new-session -d -s inner -x 80 -y 24 \
    "CONTROL='$CONTROL' perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:sync' || exit 1

$OUTER new-session -d -s outer -x 80 -y 24 \
    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
wait_for_client

if [ "$MODE" = prompt ]; then
	$OUTER send-keys -t outer:0.0 C-b : || exit 1
	sleep 0.2
fi

i=0
while [ "$i" -lt 50 ] && [ ! -e "$DIR/ready" ]; do
	sleep 0.1
	i=$((i + 1))
done
[ -e "$DIR/ready" ] || fail "application emitter did not become ready"

$OUTER pipe-pane -O -t outer:0.0 "cat >'$CLIENT_BYTES'" || exit 1
: >"$CONTROL"
sleep 2
$OUTER pipe-pane -t outer:0.0 || exit 1

MODE=$MODE perl "$ASSERT" "$CLIENT_BYTES"
