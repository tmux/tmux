#!/bin/sh

# Tests for pane-border-type joined|separate|separate-active:
#
#   - option parsing and default;
#   - separate insets each tiled pane and leaves a double border between
#     adjacent panes;
#   - separate-active uses the same inset geometry as separate;
#   - resize-pane respects a larger layout-cell minimum so panes stay inside
#     the window;
#   - full-size split and select-layout -E honour separate cell minimums;
#   - resize-window will not shrink below the separate layout floor;
#   - select-pane -L/-R/-U/-D and {left}/{top}/… targets work with the
#     double-border gap;
#   - both cells of a separate double border are mouse borders, not pane
#     content.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	exit 1
}

must_equal()
{
	got=$1
	want=$2
	msg=${3:-"got '$got', expected '$want'"}
	[ "$got" = "$want" ] || fail "$msg"
}

pane_fmt()
{
	$TMUX display-message -p -t "$1" "$2"
}

# layout_cell_w layout width — true if JSON #{window_layout} has a cell w:WIDTH.
layout_cell_w()
{
	echo "$1" | grep -E '"w":'"$2"'(,|})' >/dev/null
}

# layout_no_cell_w layout width — true if no cell has w:WIDTH.
layout_no_cell_w()
{
	echo "$1" | grep -E '"w":'"$2"'(,|})' >/dev/null && return 1
	return 0
}

# click COL ROW — SGR button-1 press+release at 1-based outer coords.
click()
{
	col=$1
	row=$2
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' "$col" "$row" "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.4
}

# ---------------------------------------------------------------------------
# Option
# ---------------------------------------------------------------------------
$TMUX new-session -d -s opt -x 80 -y 24 'cat' || exit 1
must_equal "$($TMUX show -gv pane-border-type)" "joined"
$TMUX set -g pane-border-type separate || fail "set separate failed"
must_equal "$($TMUX show -gv pane-border-type)" "separate"
$TMUX set -g pane-border-type separate-active || fail "set separate-active failed"
must_equal "$($TMUX show -gv pane-border-type)" "separate-active"
$TMUX set -g pane-border-type joined || fail "set joined failed"
must_equal "$($TMUX show -gv pane-border-type)" "joined"
$TMUX set -g pane-border-type nope >/dev/null 2>&1 &&
	fail "invalid pane-border-type value accepted"
$TMUX kill-server

# ---------------------------------------------------------------------------
# Geometry: separate insets panes; joined shares a single border column
# ---------------------------------------------------------------------------
$TMUX new-session -d -s geo -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX split-window -h -t geo:0 'cat' || fail "split -h failed"
p0=$($TMUX display-message -p -t geo:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t geo:0.1 '#{pane_id}')

$TMUX set -w -t geo:0 pane-border-type joined || fail "set joined failed"
must_equal "$(pane_fmt "$p0" '#{pane_left}')" "0"
must_equal "$(pane_fmt "$p0" '#{pane_top}')" "0"
must_equal "$(pane_fmt "$p1" '#{pane_left}')" "41"
# Single shared border column between panes.
must_equal "$(pane_fmt "$p0" '#{e|+:#{pane_right},1}')" "40"
must_equal "$(pane_fmt "$p1" '#{e|-:#{pane_left},1}')" "40"

$TMUX set -w -t geo:0 pane-border-type separate || fail "set separate failed"
must_equal "$(pane_fmt "$p0" '#{pane_left}')" "1"
must_equal "$(pane_fmt "$p0" '#{pane_top}')" "1"
must_equal "$(pane_fmt "$p1" '#{pane_left}')" "42"
must_equal "$(pane_fmt "$p1" '#{pane_top}')" "1"
# Double border: left pane's right border and right pane's left border.
left_right_border=$(pane_fmt "$p0" '#{e|+:#{pane_left},#{pane_width}}')
right_left_border=$(pane_fmt "$p1" '#{e|-:#{pane_left},1}')
must_equal "$left_right_border" "40"
must_equal "$right_left_border" "41"
[ "$left_right_border" != "$right_left_border" ] ||
	fail "separate should use two border columns, got one at $left_right_border"

# separate-active must keep the same inset geometry as separate.
$TMUX set -w -t geo:0 pane-border-type separate-active ||
	fail "set separate-active failed"
must_equal "$(pane_fmt "$p0" '#{pane_left}')" "1"
must_equal "$(pane_fmt "$p0" '#{pane_top}')" "1"
must_equal "$(pane_fmt "$p1" '#{pane_left}')" "42"
must_equal "$(pane_fmt "$p1" '#{pane_top}')" "1"
must_equal "$(pane_fmt "$p0" '#{e|+:#{pane_left},#{pane_width}}')" "40"
must_equal "$(pane_fmt "$p1" '#{e|-:#{pane_left},1}')" "41"
# Switching focus must not change pane sizes.
w0=$(pane_fmt "$p0" '#{pane_width}')
h0=$(pane_fmt "$p0" '#{pane_height}')
w1=$(pane_fmt "$p1" '#{pane_width}')
h1=$(pane_fmt "$p1" '#{pane_height}')
$TMUX select-pane -t "$p0" || fail "select left failed"
must_equal "$(pane_fmt "$p0" '#{pane_width}')" "$w0"
must_equal "$(pane_fmt "$p0" '#{pane_height}')" "$h0"
must_equal "$(pane_fmt "$p1" '#{pane_width}')" "$w1"
must_equal "$(pane_fmt "$p1" '#{pane_height}')" "$h1"
$TMUX select-pane -t "$p1" || fail "select right failed"
must_equal "$(pane_fmt "$p0" '#{pane_width}')" "$w0"
must_equal "$(pane_fmt "$p0" '#{pane_height}')" "$h0"
must_equal "$(pane_fmt "$p1" '#{pane_width}')" "$w1"
must_equal "$(pane_fmt "$p1" '#{pane_height}')" "$h1"

# Vertical split gets a double border too.
$TMUX set -w -t geo:0 pane-border-type separate || fail "set separate failed"
$TMUX kill-pane -t "$p1" || fail "kill right pane failed"
$TMUX split-window -v -t geo:0 'cat' || fail "split -v failed"
p1=$($TMUX display-message -p -t geo:0.1 '#{pane_id}')
must_equal "$(pane_fmt "$p0" '#{pane_top}')" "1"
must_equal "$(pane_fmt "$p1" '#{pane_top}')" "14"
top_bottom_border=$(pane_fmt "$p0" '#{e|+:#{pane_top},#{pane_height}}')
bot_top_border=$(pane_fmt "$p1" '#{e|-:#{pane_top},1}')
must_equal "$top_bottom_border" "12"
must_equal "$bot_top_border" "13"
$TMUX kill-server

# ---------------------------------------------------------------------------
# Resize minimum: panes must stay inside the window
# ---------------------------------------------------------------------------
$TMUX new-session -d -s rsz -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
$TMUX split-window -h -t rsz:0 'cat' || fail "split failed"
p0=$($TMUX display-message -p -t rsz:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t rsz:0.1 '#{pane_id}')
win_w=$($TMUX display-message -p -t rsz:0 '#{window_width}')
win_h=$($TMUX display-message -p -t rsz:0 '#{window_height}')

# Active pane after split is the right one; -x 1 must not push it outside.
$TMUX select-pane -t "$p1" || fail "select right failed"
$TMUX resize-pane -x 1 || fail "resize-pane -x 1 failed"
must_equal "$(pane_fmt "$p1" '#{pane_width}')" "1"
right=$(pane_fmt "$p1" '#{pane_right}')
left=$(pane_fmt "$p1" '#{pane_left}')
# pane_right is inclusive; content must end before window_width.
[ "$right" -lt "$win_w" ] ||
	fail "right pane outside window after -x 1: L=$left R=$right win=$win_w"
[ "$left" -ge 0 ] || fail "right pane left < 0: $left"
# Layout cell must keep room for both side borders (width 3).
layout=$($TMUX display-message -p -t rsz:0 '#{window_layout}')
layout_cell_w "$layout" 3 ||
	fail "expected 3-column right cell, got $layout"

# Crushing the right pane by growing the left must also stop at the minimum.
$TMUX select-pane -t "$p0" || fail "select left failed"
$TMUX resize-pane -x 78 || fail "resize-pane -x 78 failed"
must_equal "$(pane_fmt "$p1" '#{pane_width}')" "1"
right=$(pane_fmt "$p1" '#{pane_right}')
[ "$right" -lt "$win_w" ] ||
	fail "right pane outside after crush: R=$right win=$win_w"

# Same for a vertical crush.
$TMUX kill-pane -t "$p1" || fail "kill pane failed"
$TMUX split-window -v -t rsz:0 'cat' || fail "split -v failed"
p1=$($TMUX display-message -p -t rsz:0.1 '#{pane_id}')
$TMUX select-pane -t "$p1" || fail "select bottom failed"
$TMUX resize-pane -y 1 || fail "resize-pane -y 1 failed"
must_equal "$(pane_fmt "$p1" '#{pane_height}')" "1"
bottom=$(pane_fmt "$p1" '#{pane_bottom}')
[ "$bottom" -lt "$win_h" ] ||
	fail "bottom pane outside window after -y 1: B=$bottom win=$win_h"
$TMUX kill-server

# ---------------------------------------------------------------------------
# Full-size split: separate mins apply when the layout root is a node
# ---------------------------------------------------------------------------
# splitw -fh -l 1 after a vertical stack used to create a 1-column cell (the
# size path could not see the window when lc->wp was NULL on a node root).
$TMUX new-session -d -s fsplit -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
$TMUX split-window -v -t fsplit:0 'cat' || fail "split -v failed"
$TMUX split-window -fh -l 1 -t fsplit:0 'cat' || fail "split -fh -l 1 failed"
layout=$($TMUX display-message -p -t fsplit:0 '#{window_layout}')
# New pane is the last index after two splits (0,1 stacked; 2 full-height).
right_w=$($TMUX display-message -p -t fsplit:0.2 '#{pane_width}')
right_l=$($TMUX display-message -p -t fsplit:0.2 '#{pane_left}')
right_r=$($TMUX display-message -p -t fsplit:0.2 '#{pane_right}')
win_w=$($TMUX display-message -p -t fsplit:0 '#{window_width}')
must_equal "$right_w" "1" "full-size -l 1 content width"
# Content one cell in from both edges of a 3-wide cell: left = win_w - 2.
must_equal "$right_l" "$((win_w - 2))" "full-size -l 1 left inset"
[ "$right_r" -lt "$win_w" ] ||
	fail "full-size -l 1 pane outside window: R=$right_r win=$win_w"
# Edge cell must be 3 columns wide.
layout_cell_w "$layout" 3 ||
	fail "full-size -l 1 must use a 3-column edge cell, got $layout"
$TMUX kill-server

# ---------------------------------------------------------------------------
# select-layout -E: every cell keeps its separate minimum
# ---------------------------------------------------------------------------
$TMUX new-session -d -s even -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
$TMUX split-window -h -t even:0 'cat' || fail "split 1 failed"
$TMUX split-window -h -t even:0 'cat' || fail "split 2 failed"
$TMUX split-window -h -t even:0 'cat' || fail "split 3 failed"
# Wide enough that four panes can evenize (mins sum to 12 with separators).
$TMUX resize-window -t even:0 -x 20 || fail "resize-window -x 20 failed"
$TMUX select-layout -E -t even:0 || fail "select-layout -E failed"
layout=$($TMUX display-message -p -t even:0 '#{window_layout}')
# No 1-column layout cells (non-edge min is 2, edge min is 3).
layout_no_cell_w "$layout" 1 ||
	fail "evenize left a 1-column cell under separate: $layout"
win_w=$($TMUX display-message -p -t even:0 '#{window_width}')
must_equal "$win_w" "20" "evenize should keep requested window width 20"
# Every pane stays inset and inside the window.
$TMUX list-panes -t even:0 -F \
	'#{pane_index} #{pane_width} #{pane_left} #{pane_right}' >"$TMP/even.panes"
while read -r idx w left right; do
	[ "$w" -ge 1 ] || fail "evenize pane $idx content width $w"
	[ "$left" -ge 1 ] || fail "evenize pane $idx left $left (missing inset)"
	[ "$right" -lt "$win_w" ] ||
		fail "evenize pane $idx outside window: R=$right win=$win_w"
done <"$TMP/even.panes"
$TMUX kill-server

# ---------------------------------------------------------------------------
# resize-window: layout and window clamp to the separate floor
# ---------------------------------------------------------------------------
# Five side-by-side separate panes need width 15 (mins 2+2+2+2+3 + 4 seps).
# Requesting 10 must clamp to 15, not crush the borders.
$TMUX new-session -d -s floor -x 80 -y 12 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
$TMUX split-window -h -t floor:0 'cat' || fail "split 1 failed"
$TMUX split-window -h -t floor:0 'cat' || fail "split 2 failed"
$TMUX split-window -h -t floor:0 'cat' || fail "split 3 failed"
$TMUX split-window -h -t floor:0 'cat' || fail "split 4 failed"
$TMUX resize-window -t floor:0 -x 10 || fail "resize-window -x 10 failed"
must_equal "$($TMUX display-message -p -t floor:0 '#{window_width}')" "15" \
	"window must clamp to separate floor of 15"
layout=$($TMUX display-message -p -t floor:0 '#{window_layout}')
layout_cell_w "$layout" 15 || fail "layout floor width should be 15, got $layout"
echo "$layout" | grep -E '"h":12(,|})' >/dev/null ||
	fail "layout floor height should be 12, got $layout"
$TMUX list-panes -t floor:0 -F '#{pane_width} #{pane_left} #{pane_right}' \
	>"$TMP/floor.panes"
while read -r w left right; do
	must_equal "$w" "1" "crushed floor content width"
	[ "$left" -ge 1 ] || fail "floor pane missing left inset ($left)"
	[ "$right" -lt 15 ] || fail "floor pane outside: R=$right"
done <"$TMP/floor.panes"
$TMUX kill-server

# ---------------------------------------------------------------------------
# main-* layouts leave room for the borders; resize-pane -y is the pane height
# ---------------------------------------------------------------------------
for sb in off on; do
	for l in main-vertical main-vertical-mirrored main-horizontal \
	    main-horizontal-mirrored; do
		$TMUX new-session -d -s main -x 80 -y 24 'cat' || exit 1
		$TMUX set -g pane-border-type separate &&
		$TMUX set -g pane-scrollbars "$sb" ||
			fail "set options failed"
		$TMUX split-window -t main:0 'cat' &&
		$TMUX split-window -t main:0 'cat' &&
		$TMUX select-layout -t main:0 "$l" || fail "$l failed"
		$TMUX list-panes -t main:0 -F \
		    '#{pane_left} #{pane_top} #{pane_right} #{pane_bottom}' \
		    >"$TMP/main.panes"
		while read -r left top right bottom; do
			[ "$left" -ge 1 ] && [ "$top" -ge 1 ] &&
			[ "$right" -le 78 ] && [ "$bottom" -le 22 ] ||
				fail "$l sb=$sb: pane outside borders ($left,$top-$right,$bottom)"
		done <"$TMP/main.panes"
		$TMUX kill-server
	done
done

$TMUX new-session -d -s mpw -x 80 -y 24 'cat' || exit 1
$TMUX set -g pane-border-type separate &&
$TMUX set -w -t mpw:0 main-pane-width 30 &&
$TMUX split-window -t mpw:0 'cat' &&
$TMUX select-layout -t mpw:0 main-vertical || fail "main-pane-width failed"
must_equal "$(pane_fmt mpw:0.0 '#{pane_width}')" "30" "main-pane-width 30"
$TMUX kill-server

# A small main-pane-width with scrollbars still leaves both borders.
$TMUX new-session -d -s mpw -x 80 -y 24 'cat' || exit 1
$TMUX set -g pane-border-type separate &&
$TMUX set -g pane-scrollbars on &&
$TMUX set -w -t mpw:0 main-pane-width 1 &&
$TMUX split-window -t mpw:0 'cat' &&
$TMUX select-layout -t mpw:0 main-vertical || fail "main-pane-width 1 failed"
must_equal "$(pane_fmt mpw:0.1 '#{pane_left}')" "5" "main-pane-width 1 borders"
$TMUX kill-server

# Too many stacked panes grow the window so each keeps its borders.
$TMUX new-session -d -s grow -x 80 -y 10 'cat' || exit 1
$TMUX set -g pane-border-type separate || fail "set separate failed"
for i in 1 2 3 4; do
	$TMUX split-window -h -t grow:0 'cat' || fail "split $i failed"
done
$TMUX select-layout -t grow:0 main-vertical || fail "main-vertical failed"
must_equal "$($TMUX display -p -t grow:0 '#{window_height}')" "12" \
	"main-vertical grows window"
must_equal "$($TMUX list-panes -t grow:0 -F '#{pane_top}' | tr '\n' ' ')" \
	"1 1 4 7 10 " "main-vertical stacked panes"
$TMUX resize-window -t grow:0 -x 12 -y 8 &&
$TMUX select-layout -t grow:0 even-vertical || fail "even-vertical failed"
$TMUX list-panes -t grow:0 -F '#{pane_top} #{pane_bottom}' >"$TMP/grow.panes"
last=-2
while read -r top bottom; do
	[ "$top" -eq $((last + 3)) ] || fail "even-vertical overlap at $top"
	last=$bottom
done <"$TMP/grow.panes"
$TMUX kill-server

# Every preset must grow a small window enough to keep pane content inside the
# outer separate border. This covers both axes and nested cells in tiled.
for sb in off on; do
	for l in even-horizontal even-vertical main-horizontal \
	    main-horizontal-mirrored main-vertical main-vertical-mirrored tiled; do
		$TMUX new-session -d -s preset -x 80 -y 24 'cat' || exit 1
		$TMUX set -g status off &&
		$TMUX set -g pane-border-type separate &&
		$TMUX set -g pane-scrollbars "$sb" || fail "set options failed"
		for i in 1 2 3 4; do
			$TMUX split-window -h -t preset:0 'cat' ||
				fail "split $i failed"
		done
		$TMUX resize-window -t preset:0 -x 5 -y 5 ||
			fail "$l sb=$sb: resize failed"
		$TMUX select-layout -t preset:0 "$l" || fail "$l sb=$sb failed"
		win_w=$($TMUX display-message -p -t preset:0 '#{window_width}')
		win_h=$($TMUX display-message -p -t preset:0 '#{window_height}')
		$TMUX list-panes -t preset:0 -F \
		    '#{pane_width} #{pane_height} #{pane_left} #{pane_top} #{pane_right} #{pane_bottom}' \
		    >"$TMP/preset.panes"
		while read -r width height left top right bottom; do
			[ "$width" -ge 1 ] && [ "$height" -ge 1 ] &&
			    [ "$left" -ge 1 ] && [ "$top" -ge 1 ] &&
			    [ "$right" -lt $((win_w - 1)) ] &&
			    [ "$bottom" -lt $((win_h - 1)) ] ||
				fail "$l sb=$sb: pane outside borders ($left,$top-$right,$bottom in ${win_w}x${win_h})"
		done <"$TMP/preset.panes"
		$TMUX kill-server
	done
done

# Pane-scoped scrollbar widths must be included for their own layout cell, not
# taken from whichever pane happens to be active.
$TMUX new-session -d -s mixed -x 30 -y 10 'cat' || exit 1
$TMUX set -g status off &&
$TMUX set -g pane-border-type separate &&
$TMUX set -g pane-scrollbars on || fail "set mixed scrollbar options failed"
$TMUX split-window -h -t mixed:0 'cat' || fail "mixed scrollbar split failed"
$TMUX set -p -t mixed:0.0 pane-scrollbars-style 'width=5,pad=3' &&
$TMUX set -p -t mixed:0.1 pane-scrollbars-style 'width=1,pad=0' ||
	fail "set mixed scrollbar styles failed"
$TMUX select-pane -t mixed:0.1 || fail "select narrow-scrollbar pane failed"
$TMUX resize-window -t mixed:0 -x 5 -y 5 || fail "mixed resize failed"
$TMUX select-layout -t mixed:0 even-horizontal ||
	fail "mixed even-horizontal failed"
must_equal "$($TMUX display-message -p -t mixed:0 '#{window_width}')" "15" \
	"mixed scrollbar widths must clamp window to 15"
must_equal "$($TMUX display-message -p -t mixed:0.0 '#{pane_width}')" "1" \
	"wide-scrollbar pane content width"
must_equal "$($TMUX display-message -p -t mixed:0.1 '#{pane_width}')" "1" \
	"narrow-scrollbar pane content width"
$TMUX kill-server

# Changing scrollbar style with no tiled panes must not resize or reposition
# the floating pane.
$TMUX new-session -d -s floatonly -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off &&
$TMUX resize-window -t floatonly:0 -x 40 -y 12 &&
$TMUX new-pane -t floatonly:0 -x 60 -y 6 -X 0 -Y 2 'cat' &&
$TMUX kill-pane -t floatonly:0.0 || fail "create floating-only layout failed"
before=$($TMUX list-panes -t floatonly:0 \
	-F '#{pane_width}x#{pane_height}@#{pane_left},#{pane_top}')
$TMUX set -g pane-scrollbars-style fg=red ||
	fail "change floating-only scrollbar style failed"
after=$($TMUX list-panes -t floatonly:0 \
	-F '#{pane_width}x#{pane_height}@#{pane_left},#{pane_top}')
must_equal "$after" "$before" "scrollbar style moved floating-only pane"
must_equal "$($TMUX display -p -t floatonly:0 '#{window_width}x#{window_height}')" \
	"40x12" "scrollbar style resized floating-only window"
$TMUX kill-server

# Fitting scrollbar minimums must not enlarge a small floating-only cell.
$TMUX new-session -d -s floattiny -x 40 -y 12 'cat' || exit 1
$TMUX set -g status off &&
$TMUX new-pane -t floattiny:0 -x 3 -y 3 -X 5 -Y 2 'cat' &&
$TMUX kill-pane -t floattiny:0.0 &&
$TMUX set -g pane-border-type separate ||
	fail "create small floating-only layout failed"
before=$($TMUX display -p -t floattiny:0 '#{window_layout}')
$TMUX set -g pane-scrollbars-style 'width=5,pad=0' &&
$TMUX set -g pane-scrollbars on || fail "enable small floating scrollbar failed"
after=$($TMUX display -p -t floattiny:0 '#{window_layout}')
must_equal "$after" "$before" "fitting enlarged small floating-only cell"
$TMUX kill-server

# Style changes which do not grow the layout must not report a window resize.
$TMUX new-session -d -s noresize -x 20 -y 5 'cat' || exit 1
$TMUX set-hook -g window-resized 'set -g @unexpected-resize yes' ||
	fail "set resize hook failed"
$TMUX set -p -t noresize:0.0 pane-scrollbars-style 'width=2,pad=1' ||
	fail "set unused scrollbar style failed"
must_equal "$($TMUX show -gv @unexpected-resize 2>/dev/null || true)" "" \
	"unused scrollbar style must not resize window"
$TMUX kill-server

# A fit which redistributes a saved zoom layout without growing it must also
# update cell offsets before the layout is restored.
$TMUX new-session -d -s zoomfit -x 20 -y 5 'cat' || exit 1
$TMUX set -g status off &&
$TMUX set -g pane-border-type separate &&
$TMUX set -g pane-scrollbars on || fail "set no-growth zoom options failed"
$TMUX split-window -h -t zoomfit:0 'cat' &&
$TMUX select-layout -t zoomfit:0 even-horizontal ||
	fail "create no-growth zoom layout failed"
$TMUX resize-pane -Z -t zoomfit:0.0 || fail "zoom no-growth pane failed"
$TMUX set -p -t zoomfit:0.1 pane-scrollbars-style 'width=8,pad=0' ||
	fail "widen hidden scrollbar without growth failed"
must_equal "$($TMUX display -p -t zoomfit:0 '#{window_width}')" "20" \
	"saved layout redistribution must not grow window"
$TMUX resize-pane -Z -t zoomfit:0.0 || fail "unzoom no-growth pane failed"
must_equal "$($TMUX list-panes -t zoomfit:0 -F '#{pane_width} #{pane_left}' | tr '\n' ',')" \
	"6 1,1 10," "saved layout redistribution pane geometry"
$TMUX kill-server

# Changing a minimum-width joined layout to separate borders must grow both
# the layout and its window.
$TMUX new-session -d -s dynamicborder -x 3 -y 5 'cat' || exit 1
$TMUX set -g status off || fail "disable dynamic border status failed"
$TMUX split-window -h -t dynamicborder:0 'cat' &&
$TMUX select-layout -t dynamicborder:0 even-horizontal ||
	fail "create dynamic border layout failed"
$TMUX set -g pane-border-type separate ||
	fail "enable dynamic separate borders failed"
layout=$($TMUX display-message -p -t dynamicborder:0 '#{window_layout}')
layout_cell_w "$layout" 6 ||
	fail "enabling separate borders must refit layout to 6: $layout"
must_equal "$($TMUX display-message -p -t dynamicborder:0 '#{window_width}')" \
	"6" "enabling separate borders must grow window"
$TMUX kill-server

# Enabling or widening scrollbars after creating a minimum-sized layout must
# refit it immediately rather than leaving the scrollbar over pane content.
$TMUX new-session -d -s dynamic -x 6 -y 5 'cat' || exit 1
$TMUX set -g status off &&
$TMUX set -g pane-border-type separate || fail "set dynamic options failed"
$TMUX split-window -h -t dynamic:0 'cat' &&
$TMUX select-layout -t dynamic:0 even-horizontal ||
	fail "create dynamic layout failed"
must_equal "$($TMUX display-message -p -t dynamic:0 '#{window_width}')" "6" \
	"dynamic initial width"
$TMUX set -g pane-scrollbars on || fail "enable dynamic scrollbars failed"
layout=$($TMUX display-message -p -t dynamic:0 '#{window_layout}')
layout_cell_w "$layout" 8 ||
	fail "enabling scrollbars must refit layout to 8: $layout"
must_equal "$($TMUX display-message -p -t dynamic:0 '#{window_width}')" "8" \
	"enabling scrollbars must grow window"
$TMUX set -p -t dynamic:0.0 pane-scrollbars-style 'width=5,pad=3' ||
	fail "widen dynamic scrollbar failed"
layout=$($TMUX display-message -p -t dynamic:0 '#{window_layout}')
layout_cell_w "$layout" 15 ||
	fail "widening scrollbar must refit layout to 15: $layout"
must_equal "$($TMUX display-message -p -t dynamic:0 '#{window_width}')" "15" \
	"widening scrollbar must grow window"
$TMUX kill-server

# Scrollbar changes while zoomed must fit the saved full layout, not only the
# temporary one-pane zoom layout.
$TMUX new-session -d -s zoomopt -x 6 -y 5 'cat' || exit 1
$TMUX set -g status off &&
$TMUX set -g pane-border-type separate || fail "set zoom options failed"
$TMUX split-window -h -t zoomopt:0 'cat' &&
$TMUX select-layout -t zoomopt:0 even-horizontal ||
	fail "create zoom option layout failed"
$TMUX resize-pane -Z -t zoomopt:0.0 || fail "zoom option pane failed"
$TMUX set -g pane-scrollbars on || fail "enable zoomed scrollbars failed"
must_equal "$($TMUX display-message -p -t zoomopt:0 '#{window_width}')" "8" \
	"zoomed scrollbar change must grow window"
$TMUX set -p -t zoomopt:0.1 pane-scrollbars-style 'width=5,pad=3' ||
	fail "widen zoomed scrollbar failed"
must_equal "$($TMUX display-message -p -t zoomopt:0 '#{window_width}')" "15" \
	"zoomed scrollbar style change must grow window"
$TMUX resize-pane -Z -t zoomopt:0.0 || fail "unzoom option pane failed"
layout=$($TMUX display-message -p -t zoomopt:0 '#{window_layout}')
layout_cell_w "$layout" 15 ||
	fail "zoomed scrollbar change must refit saved layout: $layout"
must_equal "$($TMUX list-panes -t zoomopt:0 -F '#{pane_width}' | tr '\n' ' ')" \
	"1 1 " "zoomed scrollbar change content widths"
$TMUX kill-server

# A custom layout may have been created for joined borders and be too small for
# separate borders. Applying it must grow the tree before fixing pane geometry.
$TMUX new-session -d -s custom -x 3 -y 5 'cat' || exit 1
$TMUX set -g status off || fail "custom status off failed"
$TMUX split-window -h -t custom:0 'cat' || fail "custom split failed"
custom_layout=$($TMUX display-message -p -t custom:0 '#{window_layout}')
$TMUX set -g pane-border-type separate || fail "custom separate failed"
$TMUX select-layout -t custom:0 "$custom_layout" ||
	fail "apply undersized custom layout failed"
layout=$($TMUX display-message -p -t custom:0 '#{window_layout}')
layout_cell_w "$layout" 6 ||
	fail "custom layout must grow to separate minimum 6: $layout"
must_equal "$($TMUX display-message -p -t custom:0 '#{window_width}')" "6" \
	"custom layout must grow window"
must_equal "$($TMUX list-panes -t custom:0 -F '#{pane_width}' | tr '\n' ' ')" \
	"1 1 " "custom layout content widths"
$TMUX kill-server

for st in off top bottom; do
	for p in 0 1; do
		$TMUX new-session -d -s ry -x 80 -y 24 'cat' || exit 1
		$TMUX set -g pane-border-type separate &&
		$TMUX set -g pane-border-status "$st" &&
		$TMUX split-window -t ry:0 'cat' &&
		$TMUX select-layout -t ry:0 even-vertical &&
		$TMUX resize-pane -t ry:0.$p -y 10 || fail "resize-pane -y failed"
		must_equal "$(pane_fmt ry:0.$p '#{pane_height}')" "10" \
			"status $st: resize-pane -t $p -y 10"
		$TMUX kill-server
	done
done

# ---------------------------------------------------------------------------
# Navigation: select-pane directions and positional targets
# ---------------------------------------------------------------------------

# Side-by-side: -L/-R and {left-of}/{right-of}.
$TMUX new-session -d -s nav -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
$TMUX split-window -h -t nav:0 'cat' || fail "split -h failed"
p0=$($TMUX display-message -p -t nav:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t nav:0.1 '#{pane_id}')

$TMUX select-pane -t "$p1" || fail "select p1 failed"
$TMUX select-pane -L || fail "select-pane -L failed"
must_equal "$($TMUX display-message -p -t nav:0 '#{pane_id}')" "$p0" \
	"select-pane -L"
$TMUX select-pane -R || fail "select-pane -R failed"
must_equal "$($TMUX display-message -p -t nav:0 '#{pane_id}')" "$p1" \
	"select-pane -R"
$TMUX select-pane -t "$p0" || fail "select p0 failed"
must_equal "$($TMUX display-message -p -t '{right-of}' '#{pane_id}')" "$p1" \
	"{right-of}"
$TMUX select-pane -t "$p1" || fail "select p1 failed"
must_equal "$($TMUX display-message -p -t '{left-of}' '#{pane_id}')" "$p0" \
	"{left-of}"

# Stacked: -U/-D and {up-of}/{down-of}.
$TMUX kill-pane -t "$p1" || fail "kill pane failed"
$TMUX split-window -v -t nav:0 'cat' || fail "split -v failed"
p0=$($TMUX display-message -p -t nav:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t nav:0.1 '#{pane_id}')

$TMUX select-pane -t "$p1" || fail "select bottom failed"
$TMUX select-pane -U || fail "select-pane -U failed"
must_equal "$($TMUX display-message -p -t nav:0 '#{pane_id}')" "$p0" \
	"select-pane -U"
$TMUX select-pane -D || fail "select-pane -D failed"
must_equal "$($TMUX display-message -p -t nav:0 '#{pane_id}')" "$p1" \
	"select-pane -D"
must_equal "$($TMUX display-message -p -t '{up-of}' '#{pane_id}')" "$p0" \
	"{up-of}"
$TMUX select-pane -t "$p0" || fail "select top failed"
must_equal "$($TMUX display-message -p -t '{down-of}' '#{pane_id}')" "$p1" \
	"{down-of}"

# Wrap selectp -D/-U with separate + pane-border-status bottom.
$TMUX set -w pane-border-status bottom || fail "pane-border-status bottom failed"
$TMUX select-pane -t "$p1" || fail "select bottom for wrap failed"
$TMUX select-pane -D || fail "select-pane -D wrap failed"
must_equal "$($TMUX display-message -p -t nav:0 '#{pane_id}')" "$p0" \
	"select-pane -D wrap to top (separate + bottom status)"
$TMUX select-pane -U || fail "select-pane -U wrap failed"
must_equal "$($TMUX display-message -p -t nav:0 '#{pane_id}')" "$p1" \
	"select-pane -U wrap to bottom (separate + bottom status)"
$TMUX set -w pane-border-status off || fail "pane-border-status off failed"

# 2x2 grid for absolute edge targets. Same split order as targets-panes.sh so
# pane ids are deterministic: %0 TL, %1 TR, %2 BL, %3 BR.
$TMUX kill-server
$TMUX new-session -d -s pos -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
$TMUX split-window -h -t pos:0 'cat' || fail "split -h failed"
$TMUX split-window -v -t pos:0.%0 'cat' || fail "split left -v failed"
$TMUX split-window -v -t pos:0.%1 'cat' || fail "split right -v failed"

must_equal "$($TMUX display-message -p -t '{top-left}' '#{pane_id}')" "%0" \
	"{top-left}"
must_equal "$($TMUX display-message -p -t '{top-right}' '#{pane_id}')" "%1" \
	"{top-right}"
must_equal "$($TMUX display-message -p -t '{bottom-left}' '#{pane_id}')" "%2" \
	"{bottom-left}"
must_equal "$($TMUX display-message -p -t '{bottom-right}' '#{pane_id}')" "%3" \
	"{bottom-right}"
must_equal "$($TMUX display-message -p -t '{left}' '#{pane_id}')" "%0" \
	"{left}"
must_equal "$($TMUX display-message -p -t '{top}' '#{pane_id}')" "%0" \
	"{top}"
must_equal "$($TMUX display-message -p -t '{right}' '#{pane_id}')" "%1" \
	"{right}"
must_equal "$($TMUX display-message -p -t '{bottom}' '#{pane_id}')" "%2" \
	"{bottom}"
$TMUX kill-server

# ---------------------------------------------------------------------------
# Mouse: both columns of the separate double border are borders
# ---------------------------------------------------------------------------
$TMUX new-session -d -s mouse -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -g mouse on || fail "mouse on failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
$TMUX split-window -h -t mouse:0 'cat' || fail "split failed"
p0=$($TMUX display-message -p -t mouse:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t mouse:0.1 '#{pane_id}')

left_right_border=$(pane_fmt "$p0" '#{e|+:#{pane_left},#{pane_width}}')
right_left_border=$(pane_fmt "$p1" '#{e|-:#{pane_left},1}')
# SGR columns are 1-based.
sgr_left=$((left_right_border + 1))
sgr_right=$((right_left_border + 1))

$TMUX set -g @hit none || fail "set @hit failed"
$TMUX bind -n MouseDown1Border "set -g @hit border" ||
	fail "bind MouseDown1Border failed"
$TMUX bind -n MouseDown1Pane "set -g @hit pane" ||
	fail "bind MouseDown1Pane failed"

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t mouse" || exit 1
sleep 0.5
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "no outer pane"

# Mid-window row, away from status/edges. SGR row 12 -> window row 11.
click "$sgr_left" 12
must_equal "$($TMUX show -gv @hit)" "border" \
	"left cell of double border (window col $left_right_border) not a border"

$TMUX set -g @hit none || fail "reset @hit failed"
click "$sgr_right" 12
must_equal "$($TMUX show -gv @hit)" "border" \
	"right cell of double border (window col $right_left_border) not a border"

# A click inside the right pane content must still be a pane hit.
$TMUX set -g @hit none || fail "reset @hit failed"
content_col=$(pane_fmt "$p1" '#{pane_left}')
click $((content_col + 1)) 12
must_equal "$($TMUX show -gv @hit)" "pane" \
	"content click not classified as pane"

# ---------------------------------------------------------------------------
# Mouse: pane-border-status controls and dragging the border above a pane
# ---------------------------------------------------------------------------
# mouse_session TYPE STATUS SPLIT — two panes attached from an outer client.
mouse_session()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	$TMUX new-session -d -s ms -x 80 -y 24 'cat' || exit 1
	$TMUX set -g status off || fail "status off failed"
	$TMUX set -g mouse on || fail "mouse on failed"
	$TMUX set -g pane-border-type "$1" || fail "set $1 failed"
	$TMUX set -g pane-border-status "$2" || fail "set status $2 failed"
	$TMUX split-window "$3" -t ms:0 'cat' || fail "split $3 failed"
	$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t ms" ||
		exit 1
	sleep 0.5
	OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
}

for type in joined separate; do
	for status in top bottom; do
		tag="$type/$status"

		# Click [f], [z] and [x] on the right pane's status line.
		mouse_session "$type" "$status" -h
		$TMUX bind -n MouseDown1Control7 "set -g @hit f" &&
		$TMUX bind -n MouseDown1Control8 "set -g @hit z" &&
		$TMUX bind -n MouseDown1Control9 "set -g @hit x" ||
			fail "bind controls failed"
		if [ "$status" = top ]; then row=1; else row=24; fi
		line=$($TMUX2 capture-pane -p -t "$OUTER" | sed -n "${row}p")
		for ctl in f z x; do
			# Last match is the right pane; click inside the brackets.
			pre=${line%\[$ctl\]*}
			[ "$pre" != "$line" ] || fail "$tag: no [$ctl] on row $row"
			col=$(($(printf '%s' "$pre" | wc -m) + 2))
			$TMUX set -g @hit none || fail "reset @hit failed"
			click "$col" "$row"
			must_equal "$($TMUX show -gv @hit)" "$ctl" \
				"$tag: click on [$ctl] at col $col did not fire"
		done

		# Drag the border above the bottom pane up four rows. Rows
		# are 1-based; separate has two border rows, try both.
		mouse_session "$type" "$status" -v
		top=$($TMUX display-message -p -t ms:0.1 '#{pane_top}')
		if [ "$type" = separate ]; then
			rows="$((top - 1)) $top"
		else
			rows=$top
		fi
		for row in $rows; do
			mouse_session "$type" "$status" -v
			seq=$(printf '\033[<0;40;%sM\033[<32;40;%sM\033[<32;40;%sM\033[<0;40;%sm' \
			    "$row" $((row - 2)) $((row - 4)) $((row - 4)))
			$TMUX2 send-keys -t "$OUTER" -l "$seq"
			sleep 0.4
			after=$($TMUX display-message -p -t ms:0.1 '#{pane_top}')
			[ "$after" -lt "$top" ] ||
				fail "$tag: dragging border at row $row did not resize ($top -> $after)"
		done
	done
done
$TMUX2 kill-server 2>/dev/null

# ---------------------------------------------------------------------------
# pane_at_* and display-panes sizes use layout geometry, not content inset
# ---------------------------------------------------------------------------
$TMUX kill-server
$TMUX new-session -d -s fmt -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
p0=$($TMUX display-message -p -t fmt:0.0 '#{pane_id}')
must_equal "$(pane_fmt "$p0" '#{pane_at_left} #{pane_at_right} #{pane_at_top} #{pane_at_bottom}')" \
	"1 1 1 1" "pane_at_* for lone separate pane"
must_equal "$(pane_fmt "$p0" '#{pane_unzoomed_width}x#{pane_unzoomed_height}')" \
	"78x22" "display-panes size for lone separate pane"
$TMUX kill-server

# ---------------------------------------------------------------------------
# even-horizontal with scrollbars: panes stay inside after narrow + grow
# ---------------------------------------------------------------------------
$TMUX new-session -d -s evsb -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -g pane-scrollbars on || fail "pane-scrollbars on failed"
$TMUX split-window -h -t evsb:0 'cat' || fail "split 1 failed"
$TMUX split-window -h -t evsb:0 'cat' || fail "split 2 failed"
$TMUX resize-window -t evsb:0 -x 5 || fail "resize-window -x 5 failed"
$TMUX select-layout -t evsb:0 even-horizontal || fail "even-horizontal failed"
$TMUX resize-window -t evsb:0 -x 80 || fail "resize-window -x 80 failed"
win_w=$($TMUX display-message -p -t evsb:0 '#{window_width}')
$TMUX list-panes -t evsb:0 -F '#{pane_right}' >"$TMP/evsb.panes"
while read -r right; do
	[ "$right" -lt "$win_w" ] ||
		fail "even-horizontal scrollbar pane outside window: R=$right win=$win_w"
done <"$TMP/evsb.panes"
$TMUX kill-server

exit 0
