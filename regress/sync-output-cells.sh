#!/bin/sh

# A synchronized update that changes a few cells of a line should only redraw
# those cells, not the whole line, and the client must still end up with the
# same contents as the pane.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lsync-cells-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lsync-cells-outer-$$ -f/dev/null"
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

check_screen()
{
	$INNER capture-pane -p -e -t inner:0.0 >"$DIR/inner" || exit 1
	$OUTER capture-pane -p -e -t outer:0.0 >"$DIR/outer" || exit 1
	cmp -s "$DIR/inner" "$DIR/outer" || {
		diff "$DIR/inner" "$DIR/outer" >&2
		fail "client contents differ from pane $1"
	}

	# Lines joined by a wrap must be joined in the client too. Contents are
	# compared above and spaces differ where a wide character did not fit,
	# so leave them out.
	$INNER capture-pane -p -J -t inner:0.0 >"$DIR/capture" || exit 1
	sed 's/ //g' "$DIR/capture" >"$DIR/inner"
	$OUTER capture-pane -p -J -t outer:0.0 >"$DIR/capture" || exit 1
	sed 's/ //g' "$DIR/capture" >"$DIR/outer"
	cmp -s "$DIR/inner" "$DIR/outer" || {
		diff "$DIR/inner" "$DIR/outer" >&2
		fail "client wraps lines differently from pane $1"
	}
}

wait_for_file()
{
	i=0
	while [ "$i" -lt 100 ] && [ ! -e "$1" ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ -e "$1" ] || fail "$2"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;
use utf8;
binmode STDOUT, ':utf8';
$| = 1;

my $dir = $ENV{DIR};
sub mark { open my $f, '>', "$dir/$_[0]" or die; close $f; }
sub wait_for { select undef, undef, undef, 0.01 until -e "$dir/$_[0]"; }

# Every cell of rows 1 to 3 has its own background colour.
print "\e[2J";
for my $row (1 .. 3) {
	print "\e[$row;1H";
	for my $x (0 .. 79) {
		printf "\e[48;2;%d;%d;%dm%s", $x * 3, $row * 40, 255 - $x * 3,
		    chr(0x61 + ($x + $row) % 26);
	}
	print "\e[0m";
}
print "\e[5;1H\x{4e2d}\x{6587}\x{5b57}\x{7b26}tail";
mark('painted');
wait_for('go');

# One cell per frame, like a spinner.
for my $c ('|', '/', '-', '\\') {
	print "\e[?2026h\e[1;40H\e[38;2;255;0;0m$c\e[0m\e[10;1H\e[?2026l";
}
mark('spun');
wait_for('more');

# Changes left and right of an earlier change on one line, and one on another
# line, in a single update.
print "\e[?2026h\e[2;40HM\e[2;5H\x{263a}\e[2;70HZ\e[3;79H\x{263a}\e[?2026l";
# Overwrite the second half of a wide character.
print "\e[?2026h\e[5;4H!\e[?2026l";
# Clear from the second half of a wide character.
print "\e[?2026h\e[5;6H\e[44m\e[K\e[0m\e[?2026l";
# Write a wide character that does not fit at the end of a line, so it wraps
# on to the next.
print "\e[?2026h\e[7;76Hwra\x{263a}\x{4e2d}\e[?2026l";
# Change a cell, then scroll inside the same update. A non-ASCII character
# is not collected, so it is marked dirty before the scroll moves it.
print "\e[?2026h\e[24;10H\x{263a}\e[24;1H\n\e[23;3HW\e[?2026l";
# Text for the floating pane added below to partly cover.
print "\e[12;1Habcdefghijklmnopqrstuvwxyz";
mark('done');
wait_for('float');

# Make a character wide when its left half is under a floating pane.
print "\e[?2026h\e[12;15H\x{2665}\x{fe0f}\e[?2026l";
mark('combined');
sleep 100;
PERL

$INNER new-session -d -s inner -x 80 -y 24 "DIR='$DIR' perl '$EMITTER'" ||
    exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:sync:RGB' || exit 1

$OUTER new-session -d -s outer -x 80 -y 24 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lsync-cells-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_file "$DIR/painted" "application did not paint"
sleep 1
$OUTER pipe-pane -O -t outer:0.0 "cat >'$DIR/stream'" || exit 1
sleep 0.5
: >"$DIR/go"
wait_for_file "$DIR/spun" "application did not spin"
sleep 0.5
$OUTER pipe-pane -t outer:0.0 || exit 1

# Each frame changes one cell: a whole line of 80 coloured cells is well over
# 1000 bytes, the cell and cursor movement are well under.
bytes=$(wc -c <"$DIR/stream")
[ "$bytes" -lt 1000 ] || fail "four one-cell frames sent $bytes bytes"
check_screen "after one-cell frames"

: >"$DIR/more"
wait_for_file "$DIR/done" "application did not finish"
sleep 1

check_screen "after more updates"

$INNER new-pane -d -x 10 -y 3 -X 5 -Y 10 -t inner:0 'sleep 100' || exit 1
[ "$($INNER list-panes -t inner:0 -F '#{pane_left},#{pane_right},#{pane_top}' |
    tail -1)" = "6,13,11" ] || fail "floating pane not where expected"
: >"$DIR/float"
wait_for_file "$DIR/combined" "application did not combine"
sleep 1

# Column 14 is under the floating pane's border, so column 15 shows the
# visible half of the wide character as a space.
c=$($OUTER capture-pane -p -t outer:0.0 | sed -n 12p | perl -CS -ne \
    'print substr($_, 15, 1)')
[ "$c" = " " ] || fail "visible half of wide character shows '$c'"

exit 0
