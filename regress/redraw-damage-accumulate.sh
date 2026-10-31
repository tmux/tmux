#!/bin/sh

# Many wrapped rows crossing a panned viewport produce disjoint damage.
# Every row must survive the rectangle-count limit and subsequent merging.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Laccumulate-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Laccumulate-outer-$$ -f/dev/null"
cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15
fail()
{
	echo "$*" >&2
	exit 1
}
wait_marker()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$DIR/capture" || exit 1
		grep -q "$1" "$DIR/capture" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client did not receive $1"
}
cat >"$DIR/emitter.pl" <<'PERL'
use strict;
use warnings;
$| = 1;
for my $row (1 .. 80) {
	printf "\e[%d;1HROW%02d-%s", $row, $row, 'abcdefghij' x 6;
}
print "\e[1;1H";
while (!-e $ENV{TRIGGER}) {
	select undef, undef, undef, 0.01;
}
my $output = '';
# Each wrapped second row needs a separate damage rectangle. Keep the
# batch below the PTY read size so all 18 arrive in the same input pass.
for my $region (0 .. 17) {
	my $top = 1 + $region * 4;
	$output .= "\e[$top;1H" . ('A' x 80) . ('B' x 24) .
	    sprintf('DAMAGE%02d', $region) . ('B' x 48);
}
$output .= "\e[80;21HDONE";
syswrite STDOUT, $output;
while (!-e "$ENV{TRIGGER}-merge") {
	select undef, undef, undef, 0.01;
}
# Returning from the alternate screen requests a full-pane rectangle. A
# later wrapped-row rectangle overlaps it and must not shrink that damage.
$output = "\e[?1049h\e[40;21HALT-VISIBLE\e[?1049l";
$output .= "\e[1;1H" . ('C' x 80) . ('D' x 24) . 'MERGE00' . ('D' x 49);
$output .= "\e[80;21HMERGED";
syswrite STDOUT, $output;
sleep 100;
PERL
$INNER new-session -d -s inner -x 80 -y 80 \
    "TRIGGER='$DIR/trigger' perl '$DIR/emitter.pl'" || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g automatic-rename off || exit 1
$INNER set -g status-interval 0 || exit 1
$INNER set -as terminal-features ',screen:sync' || exit 1
$INNER set -g pane-border-lines simple || exit 1
$OUTER new-session -d -s outer -x 40 -y 80 'sleep 100' || exit 1
$OUTER set -g status off || exit 1
$OUTER set -g window-size manual || exit 1
$OUTER set -g default-terminal screen || exit 1
$OUTER respawn-pane -k -t outer:0.0 "$INNER attach -t inner" || exit 1
wait_marker ROW80
CLIENT=$($OUTER display -p -t outer:0.0 '#{pane_tty}') || exit 1
$INNER refresh-client -t "$CLIENT" -R 20 || exit 1
# Keep the emitter inactive so its writes use a synchronized frame. Otherwise
# its own queued output can force a full redraw and hide lost rectangles.
$INNER new-pane -x 6 -y 3 -X 65 -Y 74 'sleep 100' || exit 1
sleep 0.2
: >"$DIR/trigger"
wait_marker DONE
sleep 0.2
$OUTER capture-pane -p -t outer:0.0 >"$DIR/before" || exit 1
region=0
while [ "$region" -lt 18 ]; do
	marker=$(printf 'DAMAGE%02d' "$region")
	sed -n "$((2 + region * 4))p" "$DIR/before" | grep -q "$marker" ||
	    fail "missing damage for region $region"
	region=$((region + 1))
done
$INNER refresh-client -t "$CLIENT" || exit 1
sleep 0.2
$OUTER capture-pane -p -t outer:0.0 >"$DIR/after" || exit 1
diff -u "$DIR/before" "$DIR/after" || fail "accumulated redraw differs from full redraw"
: >"$DIR/trigger-merge"
wait_marker MERGED
sleep 0.2
$OUTER capture-pane -p -t outer:0.0 >"$DIR/before" || exit 1
sed -n '2p' "$DIR/before" | grep -q MERGE00 || fail "merged damage lost the wrapped row"
$INNER refresh-client -t "$CLIENT" || exit 1
sleep 0.2
$OUTER capture-pane -p -t outer:0.0 >"$DIR/after" || exit 1
diff -u "$DIR/before" "$DIR/after" || fail "merged redraw differs from full redraw"
exit 0
