#!/bin/sh

. ./regress-common.inc

# A synchronized update that changes a few cells of a line should only redraw
# those cells, not the whole line or the cells between them, and the client
# must still end up with the same contents as the pane.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR

INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
EMITTER=$DIR/emitter.pl

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

# Record what the client is sent while the application draws the frames of a
# stage, and fail if that is not under the given number of bytes.
frames()
{
	$OUTER pipe-pane -O -t outer:0.0 "cat >'$DIR/$1-stream'" || exit 1
	sleep 0.5
	: >"$DIR/$1"
	wait_for_file "$DIR/$1-done" "application did not draw $1 frames"
	sleep 0.5
	$OUTER pipe-pane -t outer:0.0 || exit 1
	bytes=$(wc -c <"$DIR/$1-stream")
	[ "$bytes" -lt "$2" ] || fail "four $1 frames sent $bytes bytes"
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
print "\e[9;1H", 'a' x 80;
my $rgb = "\e[38;2;200;100;50;48;2;20;30;40m";
print "\e[11;1H$rgb", 'a' x 80, "\e[0m";
mark('painted');

# Draw four synchronized frames once the test is recording them.
sub frames {
	my ($name, $draw) = @_;
	wait_for($name);
	for my $c ('b', 'c', 'd', 'e') {
		print "\e[?2026h";
		$draw->($c);
		print "\e[0m\e[10;1H\e[?2026l";
	}
	mark("$name-done");
}
my $link = "\e]8;id=l;https://example.com/a/longer/path\e\\";

# Two cells at opposite ends of a line, like two spinners.
frames('far', sub { print "\e[1;5H\e[38;2;255;0;0m$_[0]\e[1;75H$_[0]" });
# Every other cell of a line without colours.
frames('dense', sub { print "\e[9;", $_ * 2 + 1, "H$_[0]" for (0 .. 39) });
# Pairs of cells 7 columns apart on a line of one colour.
frames('pairs', sub {
	print $rgb;
	print "\e[11;", $_ * 7 + 1, "H$_[0]$_[0]" for (0 .. 11);
});
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
# Change one line in full and a cell on another, then scroll both.
print "\e[?2026h\e[20;5H\e[X\e[21;3HQ\e[24;1H\n\n\e[?2026l";
# Text for the floating pane added below to partly cover.
print "\e[12;1Habcdefghijklmnopqrstuvwxyz";
mark('done');
wait_for('float');

# Make a character wide when its left half is under a floating pane.
print "\e[?2026h\e[12;15H\x{2665}\x{fe0f}\e[?2026l";
mark('combined');
wait_for('styles');

# A line with a hyperlink.
print "\e[17;1H$link", 'a' x 80, "\e]8;;\e\\";
mark('styled');
# Pairs of cells 12 columns apart on the line with a hyperlink.
frames('link', sub {
	print $link;
	print "\e[17;", $_ * 12 + 1, "H$_[0]$_[0]" for (0 .. 6);
	print "\e]8;;\e\\";
});
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
    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_file "$DIR/painted" "application did not paint"
sleep 1

# Each frame changes two cells 70 columns apart: the whole line, or the cells
# between them, is well over 1000 bytes, the two cells and cursor movement are
# well under.
frames far 1000
check_screen "after far frames"

# Moving the cursor over each unchanged cell costs more than drawing it, so
# these frames should cost no more than redrawing the line each time, which is
# about 430 bytes.
frames dense 500
check_screen "after dense frames"

# Each pair keeps the colours already set for the one before, so they are sent
# once for each frame: these frames are about 350 bytes, against 2400 if they
# are sent again for every pair.
frames pairs 1000
check_screen "after pairs frames"

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

: >"$DIR/styles"
wait_for_file "$DIR/styled" "application did not draw the line with a hyperlink"
sleep 0.5

# Nor is the hyperlink closed and opened again for each pair: about 350 bytes,
# against 1800. The client has hyperlink IDs of its own, so it is not compared.
frames link 1000

exit 0
