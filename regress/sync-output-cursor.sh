#!/bin/sh

. ./regress-common.inc

# A synchronized update elsewhere in the pane must not hide a stationary
# cursor while tmux waits for the end of the update. On a client without
# synchronized output, the cursor must stay hidden while the update is drawn
# and only be shown again once it is back in place.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

if [ "$#" -eq 0 ]; then
	TEST_TMUX="$TEST_TMUX" sh "$0" pane || exit 1
	TEST_TMUX="$TEST_TMUX" sh "$0" prompt || exit 1
	TEST_TMUX="$TEST_TMUX" sh "$0" nosync || exit 1
	exit 0
fi
case $1 in
pane|prompt|nosync) MODE=$1 ;;
*) echo "usage: $0 [pane|prompt|nosync]" >&2; exit 1 ;;
esac

DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR

INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
CLIENT_BYTES=$DIR/client-bytes
CONTROL=$DIR/control
EMITTER=$DIR/emitter.pl
ASSERT=$DIR/assert-cursor.pl
ASSERT_NOSYNC=$DIR/assert-nosync.pl

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	stop_server $OUTER || exit 1
	stop_server $INNER || exit 1
	rm -rf "$DIR"
}
trap cleanup 0 1 15

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER list-clients -F '#{client_termfeatures}' 2>/dev/null |
		    grep -q "$1" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client did not attach"
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

cat >"$ASSERT_NOSYNC" <<'PERL'
use strict;
use warnings;

# Follow the cursor through the client bytes: every character must be drawn
# while it is hidden, and it must be back at the prompt (line 24, column 3)
# whenever it is shown.
my $path = shift;
open my $fh, '<:raw', $path or die "$path: $!\n";
local $/;
my $bytes = <$fh>;
my ($hidden, $at_prompt, $drawn, $shows) = (0, 1, 0, 0);

while ($bytes =~ /\G(?:
    (\e\[\?25l) |
    (\e\[\?25h) |
    \e\[(\d*);(\d*)H |
    \e\[[0-9;?>=]*[\x20-\x2f]*([\x40-\x7e]) |
    \e\][^\a\e]*(?:\a|\e\\) |
    \e[()*+][\x20-\x7e] |
    \e[\x20-\x7e] |
    ([\x20-\x7e\x80-\xff]) |
    ([\b\t\n\r]) |
    [\x00-\x1f\x7f]
    )/gcsx) {
	if (defined $1) {
		$hidden = 1;
	} elsif (defined $2) {
		die "cursor shown away from the prompt\n" unless $at_prompt;
		$hidden = 0;
		$shows++;
	} elsif (defined $3) {
		$at_prompt = ($3 eq '24' && $4 eq '3');
	} elsif (defined $5) {
		$at_prompt = 0 if $5 =~ /[A-Hdf`]/;
	} elsif (defined $6) {
		die "character drawn with the cursor visible\n" unless $hidden;
		$at_prompt = 0;
		$drawn++;
	} elsif (defined $7) {
		$at_prompt = 0;
	}
}

printf "nosync characters drawn: %d; cursor shows: %d\n", $drawn, $shows;
die "no update was drawn\n" if $drawn == 0 || $shows == 0;
PERL

$INNER new-session -d -s inner -x 80 -y 24 \
    "CONTROL='$CONTROL' perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
if [ "$MODE" = nosync ]; then
	$INNER set-option -as terminal-overrides '*:Sync@' || exit 1
else
	$INNER set-option -as terminal-features '*:sync' || exit 1
fi

$OUTER new-session -d -s outer -x 80 -y 24 \
    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
if [ "$MODE" = nosync ]; then
	wait_for_client .
else
	wait_for_client sync
fi

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

if [ "$MODE" = nosync ]; then
	perl "$ASSERT_NOSYNC" "$CLIENT_BYTES"
else
	MODE=$MODE perl "$ASSERT" "$CLIENT_BYTES"
fi
