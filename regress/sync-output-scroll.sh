#!/bin/sh

# A scroll inside a synchronized update must be replayed to the client as a
# scroll when the update ends, not by redrawing every line in the scroll
# region.

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

# Fill the screen outside a synchronized update.
my @rows;
for my $row (1 .. 24) {
	push @rows, substr(sprintf("INIT_ROW_%02d_", $row) . ('X' x 79), 0, 79);
}
syswrite STDOUT, "\e[H" . join("\r\n", @rows);

open my $painted, '>', "$dir/painted" or die "$dir/painted: $!\n";
close $painted;
while (!-e $control) {
	select undef, undef, undef, 0.01;
}

# Inside a synchronized update, scroll one line and change one cell.
my $frame = "\e[?2026h" .
    "\e[24;1H\r\nSCROLLED_LINE_" .
    "\e[23;60HCHANGED_" .
    "\e[?2026l";
my $written = syswrite STDOUT, $frame;
die "short synchronized frame write\n"
    unless defined $written && $written == length $frame;
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
$OUTER pipe-pane -O -t outer:0.0 "cat >'$CLIENT_BYTES'" || exit 1
: >"$CONTROL"
i=0
while [ "$i" -lt 50 ]; do
	grep -q 'SCROLLED_LINE_' "$CLIENT_BYTES" 2>/dev/null && break
	sleep 0.1
	i=$((i + 1))
done
grep -q 'SCROLLED_LINE_' "$CLIENT_BYTES" || fail "scrolled line not sent"
wait_for_stable_bytes
$OUTER pipe-pane -t outer:0.0 || exit 1

# The changed cell must have been sent, but lines which were only moved by
# the scroll must not have been redrawn.
grep -q 'CHANGED_' "$CLIENT_BYTES" || fail "changed cell not sent"
if grep -q 'INIT_ROW_05_' "$CLIENT_BYTES"; then
	fail "unchanged line redrawn after synchronized scroll"
fi

# The client must show the scrolled screen.
$OUTER capture-pane -p -t outer:0.0 >"$DIR/screen" || exit 1
sed -n 1p "$DIR/screen" | grep -q '^INIT_ROW_02_' ||
    fail "first line not scrolled"
sed -n 23p "$DIR/screen" | grep -q 'INIT_ROW_24_.*CHANGED_' ||
    fail "changed cell not on screen"
sed -n 24p "$DIR/screen" | grep -q '^SCROLLED_LINE_' ||
    fail "scrolled line not on screen"
exit 0
