#!/bin/sh

# Exercise tty_draw_line through a real client. An inner tmux is attached
# inside an outer tmux pane; the outer pane is then captured to inspect what
# the inner client actually drew.

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

capturee() {
	$TMUX capturep -peS0 -E- >$TMP || exit 1
}

capturen() {
	$TMUX capturep -pNS0 -E- >$TMP || exit 1
}

captureen() {
	$TMUX capturep -peNS0 -E- >$TMP || exit 1
}

check_line() {
	line=$1
	want=$2
	got=$(sed -n "$line"p $TMP)
	[ "$got" = "$want" ] || fail "line $line: expected '$want', got '$got'"
}

check_grep() {
	pattern=$1
	grep -q "$pattern" $TMP || fail "missing pattern: $pattern"
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX2 -f/dev/null new -d -x20 -y6 -s test \
	"printf 'abcdefghijklmnopqrst'; exec sleep 100" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g mode-style "fg=white,bg=red" || exit 1
$TMUX2 setw -g mode-keys vi || exit 1
$TMUX2 neww -d \
	"printf '\033[31;44;1mRED\033[0m\tTAIL\nu:e\314\201:\347\225\214:\360\237\207\272\360\237\207\270:Z\nAAA      BBB'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf '12345678\347\225\214Z'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf 'wrap-ABCDEFGHIJKLMNOZ'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf 'AA    BB'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf 'XYZ'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf '\347\225\214\347\225\214\347\225\214\347\225\214\347\225\214'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf '123456789Z'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf 'abcdef\347\225\214GHIJKLMNOPQRSTUVWXYZ'; printf '\033[H'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf 'abcdefghijklmnopqrst\r\033[KXYZ\nnext'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf 'ab\tcdefghijklmnopqrstuvwxyz'; printf '\033[H'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"printf '\033(0x\033(B'; exec sleep 100" || exit 1
$TMUX2 neww -d \
	"awk 'BEGIN { for (i = 0; i < 1100; i++) printf \"a\" }'; exec sleep 100" || exit 1
$TMUX2 selectw -t:0 || exit 1

$TMUX -f/dev/null new -d -x20 -y6 || exit 1
$TMUX set -g status off || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1
CLIENT=$($TMUX2 list-clients -F '#{client_name}' | head -1)
[ -n "$CLIENT" ] || fail "no inner client"

# Long line, then short line: default cells after cellsize must clear stale text.
capture
check_line 1 "abcdefghijklmnopqrst"
$TMUX2 selectw -t:5 || exit 1
sleep 1
capture
check_line 1 "XYZ"
grep -q '^XYZdefghijklmnopqrst$' $TMP && fail "short redraw left stale tail"

# Styles, tabs, same runs, Unicode combining/wide/flag cells.
$TMUX2 selectw -t:1 || exit 1
sleep 1
capture
check_grep '^RED[	 ]*TAIL$'
check_grep '^u:e.*:.*:.*:Z$'
check_line 3 "AAA      BBB"
capturee
esc=$(printf '\033')
grep -q "$esc" $TMP || fail "styled redraw did not preserve attributes"

# Wide character clipping and padding after resize.
$TMUX2 selectw -t:2 || exit 1
$TMUX resizew -x10 -y6 || exit 1
sleep 1
$TMUX2 respawnp -k "printf '12345678\347\225\214Z'; exec sleep 100" || exit 1
sleep 1
capture
check_line 1 "12345678界"
$TMUX resizew -x9 -y6 || exit 1
sleep 1
capture
check_line 1 "12345678"
grep -q '^12345678Z$' $TMP && fail "wide clipping left stale cell"

# Repeated wide characters at the right edge should not leave orphan padding.
$TMUX resizew -x9 -y6 || exit 1
$TMUX2 selectw -t:6 || exit 1
sleep 1
$TMUX2 respawnp -k "printf '\347\225\214\347\225\214\347\225\214\347\225\214\347\225\214'; exec sleep 100" || exit 1
sleep 1
capture
check_line 1 "界界界界"

# Tabs should clear stale cells as an empty run.
$TMUX resizew -x10 -y6 || exit 1
$TMUX2 selectw -t:7 || exit 1
sleep 1
capture
check_line 1 "123456789Z"
$TMUX2 respawnp -k "printf '123456789\t'; exec sleep 100" || exit 1
sleep 1
capture
check_line 1 "123456789"
grep -q '^123456789.*Z$' $TMP && fail "tab clipping left stale cell"

# Tabs clipped at both ends, at the right, and at the left. This is like
# drawing spans over the middle of a tab when an overlay or viewport clips the
# pane line.
$TMUX resizew -x4 -y6 || exit 1
$TMUX2 selectw -t:10 || exit 1
$TMUX2 resizew -t:10 -x26 -y6 || exit 1
$TMUX2 respawnp -k "printf 'ab\tcdefghijklmnopqrstuvwxyz'; printf '\033[H'; exec sleep 100" || exit 1
$TMUX2 refresh -t"$CLIENT" -c || exit 1
sleep 1
capturen
check_line 1 "ab  "
$TMUX2 refresh -t"$CLIENT" -R 2 || exit 1
sleep 1
capturen
check_line 1 "    "
$TMUX2 refresh -t"$CLIENT" -R 4 || exit 1
sleep 1
capturen
check_line 1 "  cd"
$TMUX2 refresh -t"$CLIENT" -R 2 || exit 1
sleep 1
capturen
check_line 1 "cdef"
$TMUX2 refresh -t"$CLIENT" -L 8 || exit 1

# Horizontal clipping that starts in or near a wide character should not draw
# partial padding or stale cells.
$TMUX resizew -x20 -y6 || exit 1
$TMUX2 selectw -t:8 || exit 1
$TMUX2 resizew -t:8 -x26 -y6 || exit 1
$TMUX2 respawnp -k "printf 'abcdef\347\225\214GHIJKLMNOPQRSTUVWXYZ'; printf '\033[H'; exec sleep 100" || exit 1
$TMUX2 refresh -t"$CLIENT" -c || exit 1
sleep 1
$TMUX2 refresh -t"$CLIENT" -R 4 || exit 1
sleep 1
capture
check_line 1 "ef界GHIJKLMNOPQRSTUV"
$TMUX2 refresh -t"$CLIENT" -R 1 || exit 1
sleep 1
capture
check_line 1 "f界GHIJKLMNOPQRSTUVW"
$TMUX2 refresh -t"$CLIENT" -L 5 || exit 1

# Wrapped line redraw.
$TMUX resizew -x20 -y6 || exit 1
$TMUX2 selectw -t:3 || exit 1
sleep 1
capture
check_line 1 "wrap-ABCDEFGHIJKLMNO"
check_line 2 "Z"

# Selection over spaces should still paint attributes for otherwise empty cells.
$TMUX2 selectw -t:4 || exit 1
sleep 1
$TMUX2 copy-mode -H || exit 1
$TMUX2 send -X start-of-line || exit 1
$TMUX2 send -X -N 2 cursor-right || exit 1
$TMUX2 send -X begin-selection || exit 1
$TMUX2 send -X -N 3 cursor-right || exit 1
sleep 1
capture
check_line 1 "AA    BB"
capturee
sed -n 1p $TMP | grep -q "$esc" || fail "selected spaces did not draw attributes"

# Selection on a short line should still draw attributes correctly. This line
# was previously expanded, then cleared and rewritten shorter, so cellsize
# remains larger than the visible text.
$TMUX2 selectw -t:9 || exit 1
sleep 1
$TMUX2 copy-mode -H || exit 1
$TMUX2 send -X history-top || exit 1
$TMUX2 send -X start-of-line || exit 1
$TMUX2 send -X begin-selection || exit 1
$TMUX2 send -X cursor-down || exit 1
sleep 1
capture
check_line 1 "XYZ"
check_line 2 "next"
captureen
sed -n 1p $TMP | grep -q "$esc" || \
	fail "selected short-line tail did not draw attributes"

# ACS/charset cells should be redrawn correctly.
$TMUX resizew -x20 -y6 || exit 1
$TMUX2 selectw -t:11 || exit 1
sleep 1
capture
check_line 1 "│"

# A long run with the same attributes should flush the internal draw buffer.
$TMUX resizew -x1100 -y6 || exit 1
$TMUX2 resizew -t:12 -x1100 -y6 || exit 1
$TMUX2 selectw -t:12 || exit 1
sleep 1
capture
len=$(sed -n 1p $TMP | wc -c)
[ "$len" -ge 1100 ] || fail "long same-style line was truncated"

exit 0
