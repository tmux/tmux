#!/bin/sh

# Moving and interactively resizing floating panes must restore both clients,
# including a smaller client panned horizontally and vertically.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lviewports-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lviewports-outer-$$ -f/dev/null"

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
mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t wide:0.0 -l "$sequence" || exit 1
	sleep 0.2
}
assert_scene()
{
	# Compare what each terminal actually received with a fresh full redraw.
	# Capture both first: refreshing one client must not repair the other.
	for target in wide small; do
		$OUTER capture-pane -p -t "$target:0.0" >"$DIR/$target-before" || exit 1
	done
	$INNER refresh-client -t "$WIDE" || exit 1
	$INNER refresh-client -t "$SMALL" || exit 1
	sleep 0.2
	for target in wide small; do
		$OUTER capture-pane -p -t "$target:0.0" >"$DIR/$target-after" || exit 1
		diff -u "$DIR/$target-before" "$DIR/$target-after" ||
		    fail "$1: $target client differed from a full redraw"
	done
}

cat >"$DIR/background.pl" <<'PERL'
$| = 1;
for my $row (1 .. 24) {
	printf "\e[%d;1HROW%02d-", $row, $row;
	print '0123456789' x 7;
}
sleep 100;
PERL
$INNER new-session -d -s inner -x 80 -y 24 "perl '$DIR/background.pl'" || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g automatic-rename off || exit 1
$INNER set -g status-interval 0 || exit 1
$INNER set -g mouse on || exit 1
$INNER set -g default-command 'sleep 100' || exit 1
$INNER set -g pane-border-lines simple || exit 1
$OUTER new-session -d -s wide -x 80 -y 24 'sleep 100' || exit 1
$OUTER set -g status off || exit 1
$OUTER set -g window-size manual || exit 1
$OUTER set -g default-terminal screen || exit 1
$OUTER new-session -d -s small -x 40 -y 12 'sleep 100' || exit 1
for target in wide small; do
	$OUTER respawn-pane -k -t "$target:0.0" "$INNER attach -t inner" || exit 1
done
i=0
while [ "$($INNER list-clients | wc -l)" -ne 2 ]; do
	[ "$i" -lt 50 ] || fail "two clients did not attach"
	sleep 0.1
	i=$((i + 1))
done
WIDE=$($OUTER display -p -t wide:0.0 '#{pane_tty}') || exit 1
SMALL=$($OUTER display -p -t small:0.0 '#{pane_tty}') || exit 1
$INNER refresh-client -t "$SMALL" -R 20 || exit 1
$INNER refresh-client -t "$SMALL" -D 6 || exit 1
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 16 -y 6 -X 25 -Y 9) || exit 1
sleep 0.3
assert_scene initial
# Confirm that the two clients really have different viewports.
[ "$(head -1 "$DIR/wide-before" | cut -c1-6)" = ROW01- ] || fail "wrong wide viewport"
[ "$(head -1 "$DIR/small-before" | cut -c1-6)" != ROW01- ] || fail "small client was not panned"

# Meta-drag the body using move-pane -M, including a partially clipped position.
mouse 8 30 12 M
mouse 40 48 15 M
[ "$($INNER display -p -t "$FLOAT" '#{pane_left}')" -eq 44 ] || fail "Meta-drag did not move pane"
assert_scene move-right
mouse 40 28 10 M
assert_scene move-back
mouse 8 28 10 m

# Ctrl-drag creates a new floating pane, then changes its size while held.
mouse 16 52 14 M
mouse 48 75 23 M
NEW=$($INNER display -p '#{pane_id}') || exit 1
[ "$NEW" != "$FLOAT" ] || fail "Ctrl-drag did not create a pane"
[ "$($INNER display -p -t "$NEW" '#{pane_floating_flag}')" -eq 1 ] || fail "new pane is not floating"
assert_scene create
mouse 48 64 19 M
[ "$($INNER display -p -t "$NEW" '#{pane_width}')" -eq 11 ] || fail "Ctrl-drag did not shrink pane"
assert_scene shrink
mouse 16 64 19 m
assert_scene release
exit 0
