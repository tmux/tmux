#!/bin/sh

# Mirror of floating-pane-drag-wide-character.sh for the *right* edge of a
# damage rectangle. redraw_damage_grow_span_clip() (screen-redraw.c) widens
# a damage rectangle's right edge by one cell whenever it isn't already at
# the span's own edge, to pull in a wide character's base half when the
# edge lands on its padding half - but it did this unconditionally, with no
# check of which half it was actually touching. When the edge instead
# already lands cleanly on a fresh character's base cell (a character fully
# outside the range), growing right pulls in just that base cell -
# tty_draw_line() sees it can't fit that character's full width in the
# remaining range (tty_draw_line_get_empty()'s gc->data.width > nx check)
# and clears it, exactly as the left-edge bug cleared a neighbouring
# character's padding half.
#
# As with the left-edge test, the exact column parity needs to be
# deterministic to actually catch it. This constructs it by creating the
# floating pane, checking its real position, and recreating it one column
# over if necessary until the vacated rectangle's right edge lands on a
# base cell.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lwidecharr-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lwidecharr-outer-$$ -f/dev/null"
EMITTER=$DIR/emitter.pl
BASE=$DIR/base
CAPTURE=$DIR/capture
FLOAT=
PANEWIDTH=12

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
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
		CLIENT=$($INNER list-clients -F '#{client_name}' 2>/dev/null)
		[ -n "$CLIENT" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

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

slice_columns()
{
	# Extract terminal columns [COL1, COL2) from lines [ROW1, ROW2] of
	# $1, decoding UTF-8 - the pane's own new position (well clear of
	# this range) must not affect the comparison, so this only looks at
	# the narrow strip actually vacated, not the whole line. capture-pane
	# text has one decoded character per double-width cell pair (every
	# character here is width 2), so terminal columns are converted to
	# character indices by halving before slicing.
	perl -CSD -e '
		my ($row1, $row2, $col1, $col2, $file) = @ARGV;
		open my $fh, "<:encoding(UTF-8)", $file or die $!;
		my @lines = <$fh>;
		my $c1 = int($col1 / 2);
		my $c2 = int(($col2 + 1) / 2);
		for my $n ($row1 .. $row2) {
			my $line = $lines[$n - 1];
			$line =~ s/\R\z//;
			print substr($line, $c1, $c2 - $c1), "\n";
		}
	' "$ROW1" "$ROW2" "$COL1" "$COL2" "$1"
}

wait_old_rows_restored()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		slice_columns "$BASE" >"$DIR/want"
		slice_columns "$CAPTURE" >"$DIR/got"
		cmp -s "$DIR/want" "$DIR/got" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "wide characters under the floating pane's vacated right edge were not restored"
}

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.1
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

binmode STDOUT, ':encoding(UTF-8)';
$| = 1;
for my $row (1 .. 10) {
	print "\e[$row;1H", chr(0x754c) x 20;
}
sleep 100;
PERL

$INNER new-session -d -s inner -x 40 -y 10 "perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-option -g pane-scrollbars off || exit 1

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lwidecharr-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
wait_outer_has '界界界'
$OUTER capture-pane -p -t outer:0.0 >"$BASE" || exit 1

# Create the floating pane, then check its actual resulting position. Try
# adjacent starting columns until the vacated rectangle's right edge
# (xoff + PANEWIDTH + 1, the border-grown exclusive end - see
# window_pane_damage_floating(), window.c) lands on an even (base-cell)
# column.
startx=5
tries=0
while [ "$tries" -lt 2 ]; do
	[ -n "$FLOAT" ] && $INNER kill-pane -t "$FLOAT" 2>/dev/null
	FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x "$PANEWIDTH" -y 3 \
	    -X "$startx" -Y 5 \
	    'sh -c "printf FLOATMARK; exec sleep 100"') || exit 1
	sleep 0.2
	XOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
	YOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_top}')
	oldright=$((XOFF + PANEWIDTH + 1))
	if [ $((oldright % 2)) -eq 0 ]; then
		break
	fi
	startx=$((startx + 1))
	tries=$((tries + 1))
done
[ $(((XOFF + PANEWIDTH + 1) % 2)) -eq 0 ] ||
	fail "could not find bad-parity starting column"

wait_outer_has FLOATMARK

ROW1=$((YOFF + 1))
ROW2=$((YOFF + 3))
COL1=$((XOFF + PANEWIDTH - 2))
COL2=$((XOFF + PANEWIDTH + 2))

# Grab the pane's top border a couple of columns in (avoiding the corner
# cells) and drag it well clear of its old rectangle - far enough right
# that its new position starts past the checked columns above (which sit
# just past the pane's *original* right edge).
GRABCOL=$((XOFF + 3))
BORDERROW=$((YOFF))

seq=$(printf '\033[<0;%s;%sM' "$GRABCOL" "$BORDERROW")
$OUTER send-keys -t outer:0.0 -l "$seq" || exit 1
sleep 0.1
seq=$(printf '\033[<32;%s;%sM' "$((GRABCOL + 20))" "$BORDERROW")
$OUTER send-keys -t outer:0.0 -l "$seq" || exit 1
sleep 0.1
seq=$(printf '\033[<0;%s;%sm' "$((GRABCOL + 20))" "$BORDERROW")
$OUTER send-keys -t outer:0.0 -l "$seq" || exit 1
sleep 0.1

NEWXOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
[ "$NEWXOFF" != "$XOFF" ] || fail "sanity: floating pane did not move (still at $XOFF)"

wait_old_rows_restored

exit 0
