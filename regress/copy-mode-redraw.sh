#!/bin/sh

# Exercise copy mode redraw through a real client. An inner tmux is attached
# inside an outer tmux pane; the outer pane is captured to inspect what the
# inner client actually drew.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

fail() {
	echo "$*" >&2
	exit 1
}

capture() {
	$TMUX capturep -pS0 -E- >$TMP || exit 1
}

check_line() {
	line=$1
	want=$2
	got=$(sed -n "$line"p $TMP)
	[ "$got" = "$want" ] || fail "line $line: expected '$want', got '$got'"
}

check_grep() {
	pattern=$1
	grep -Fq "$pattern" $TMP || fail "missing pattern: $pattern"
}

check_no_grep() {
	pattern=$1
	grep -Fq "$pattern" $TMP && fail "unexpected pattern: $pattern"
}

redraw() {
	$TMUX2 send -X cursor-right || exit 1
	sleep 1
	capture
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
BEFORE=$(mktemp)
AFTER=$(mktemp)
trap "rm -f $TMP $BEFORE $AFTER; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX2 -f/dev/null new -d -x48 -y8 -s test \
	"awk 'BEGIN { for (i = 0; i < 90; i++) { if (i % 2 == 0) printf \"L%03d-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-END\\n\", i; else printf \"S%03d\\n\", i } }'; exec sleep 100" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g mode-keys vi || exit 1
$TMUX2 set -g copy-mode-line-numbers off || exit 1
$TMUX2 set -g copy-mode-position-format "#[align=right][23/100-LONGTAIL]" || exit 1

$TMUX -f/dev/null new -d -x48 -y8 || exit 1
$TMUX set -g status off || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

CLIENT=$($TMUX2 list-clients -F '#{client_name}' | head -1)
[ -n "$CLIENT" ] || fail "no inner client"

$TMUX2 copy-mode || exit 1
$TMUX2 send -X history-top || exit 1
sleep 1

# Shrinking and moving the position indicator should not leave stale text from
# the previous indicator, for different widths and alignments.
for size in 20 48; do
	$TMUX resizew -x$size -y8 || exit 1
	$TMUX2 refresh -t"$CLIENT" -c || exit 1
	sleep 1
	for align in left centre right; do
		$TMUX2 set -g copy-mode-position-format "#[align=$align][23/100-LONGTAIL]" || exit 1
		redraw
		check_grep "[23/100-LONGTAIL]"

		$TMUX2 set -g copy-mode-position-format "#[align=$align][1/100]" || exit 1
		redraw
		check_grep "[1/100]"
		check_no_grep "LONGTAIL"
		check_no_grep "[1/100]]"
	done

	$TMUX2 set -g copy-mode-position-format "#[align=right][1/100]" || exit 1
	redraw
	$TMUX2 set -g copy-mode-position-format "#[align=right][23/100]" || exit 1
	redraw
	check_grep "[23/100]"
done

# Scrolling from long pane content to short pane content should clear the
# remainder of the old line without relying on a full pre-clear.
$TMUX resizew -x48 -y8 || exit 1
$TMUX2 refresh -t"$CLIENT" -c || exit 1
$TMUX2 send -X cancel || exit 1
$TMUX2 copy-mode -H || exit 1
$TMUX2 send -X history-top || exit 1
sleep 1
capture
check_line 1 "L000-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-END"

$TMUX2 send -X scroll-down || exit 1
sleep 1
capture
check_line 1 "S001"

$TMUX2 send -X scroll-down || exit 1
sleep 1
capture
check_line 1 "L002-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-END"

$TMUX2 send -X scroll-down || exit 1
sleep 1
capture
check_line 1 "S003"

# Scrolling from long content to a tabbed short line should still clear the old
# line tail.
$TMUX2 send -X cancel || exit 1
$TMUX resizew -x40 -y8 || exit 1
$TMUX2 refresh -t"$CLIENT" -c || exit 1
$TMUX2 new-window \
	"printf 'LONGTAIL-ABCDEFGHIJKLMNOPQRSTUV\nA\tB\nLONGTAIL-123456789012345678\nS\n'; i=0; while [ \$i -lt 20 ]; do printf 'FILLER-%02d\n' \$i; i=\$((i + 1)); done; exec sleep 100" || \
	exit 1
sleep 1
$TMUX2 copy-mode -H || exit 1
$TMUX2 send -X history-top || exit 1
sleep 1
capture
check_line 1 "LONGTAIL-ABCDEFGHIJKLMNOPQRSTUV"

$TMUX2 send -X scroll-down || exit 1
sleep 1
capture
check_line 1 "A       B"
sed -n 1p $TMP | grep -Fq "LONGTAIL" && \
	fail "short tabbed line left stale tail"

# Reflow can leave padding from tabs at the start of wrapped lines. Entering
# copy mode should redraw those padding cells as spaces, not skip them and move
# the following text left.
$TMUX2 send -X cancel || exit 1
$TMUX resizew -x80 -y24 || exit 1
$TMUX2 refresh -t"$CLIENT" -c || exit 1
$TMUX2 new-window "cat ../tmux.c; exec sleep 100" || exit 1
sleep 1
$TMUX2 splitw -hd || exit 1
sleep 1
$TMUX capturep -pS0 -E- >$BEFORE || exit 1
$TMUX2 copy-mode -H -t:.0 || exit 1
sleep 1
$TMUX capturep -pS0 -E- >$AFTER || exit 1

if ! cmp -s "$BEFORE" "$AFTER"; then
	diff -u "$BEFORE" "$AFTER" >&2
	fail "copy-mode redraw moved reflowed tab padding"
fi

exit 0
