#!/bin/sh

# A scroll held back by a synchronized update may be sent to all clients while
# a later client is being redrawn. An earlier client which cannot scroll the
# pane itself must still end up with the right contents.

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

wait_for_clients()
{
	i=0
	while [ "$i" -lt 50 ]; do
		n=$($INNER list-clients 2>/dev/null | wc -l)
		[ "$n" -eq "$1" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client $1 did not attach"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

my $control = $ENV{CONTROL};
(my $dir = $control) =~ s{/[^/]+$}{};

my @rows;
for my $row (1 .. 24) {
	push @rows, sprintf("INIT_ROW_%02d_", $row) . ('X' x 16);
}
syswrite STDOUT, "\e[H" . join("\r\n", @rows);

open my $painted, '>', "$dir/painted" or die "$dir/painted: $!\n";
close $painted;
while (!-e $control) {
	select undef, undef, undef, 0.01;
}

# Start a synchronized update and scroll, but do not end it.
syswrite STDOUT, "\e[?2026h\e[24;1H\r\nSCROLLED_LINE_";
open my $scrolled, '>', "$dir/scrolled" or die "$dir/scrolled: $!\n";
close $scrolled;
select undef, undef, undef, 3;
PERL

$INNER new-session -d -s inner -x 80 -y 24 \
    "CONTROL='$CONTROL' perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:sync' || exit 1

# The pane is not full width, so clients cannot scroll it.
$INNER split-window -h -d -t inner:0.0 'sleep 30' || exit 1

for name in a b; do
	$OUTER new-session -d -s $name -x 80 -y 24 \
	    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" || exit 1
	$OUTER set-option -t $name status off || exit 1
	if [ "$name" = a ]; then
		wait_for_clients 1
	fi
done
$OUTER set-option -g window-size manual || exit 1
wait_for_clients 2
B=$($OUTER display-message -p -t b: '#{pane_tty}') || exit 1

wait_for_file "$DIR/painted" "application did not paint"
sleep 1
: >"$CONTROL"
wait_for_file "$DIR/scrolled" "application did not scroll"
sleep 0.2

# Redraw only the second client while the update is still in progress.
$INNER refresh-client -t "$B" || exit 1
sleep 1

$OUTER capture-pane -p -t a: >"$DIR/a" || exit 1
$OUTER capture-pane -p -t b: >"$DIR/b" || exit 1
sed -n 1p "$DIR/b" | grep -q '^INIT_ROW_02_' ||
    fail "second client not scrolled"
sed -n 24p "$DIR/b" | grep -q '^SCROLLED_LINE_' ||
    fail "second client missing scrolled line"
cmp -s "$DIR/a" "$DIR/b" || fail "first client differs from second client"
exit 0
