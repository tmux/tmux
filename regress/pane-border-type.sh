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
case "$($TMUX display-message -p -t rsz:0 '#{window_layout}')" in
*,3x*) ;;
*) fail "expected 3-column right cell, got $($TMUX display-message -p -t rsz:0 '#{window_layout}')" ;;
esac

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
case "$layout" in
*,3x*) ;;
*) fail "full-size -l 1 must use a 3-column edge cell, got $layout" ;;
esac
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
case "$layout" in
*,1x*)
	fail "evenize left a 1-column cell under separate: $layout"
	;;
esac
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
# Requesting 10 must clamp to 15, not crush border gutters.
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
case "$layout" in
*,15x12,*) ;;
*) fail "layout floor size should be 15x12, got $layout" ;;
esac
$TMUX list-panes -t floor:0 -F '#{pane_width} #{pane_left} #{pane_right}' \
	>"$TMP/floor.panes"
while read -r w left right; do
	must_equal "$w" "1" "crushed floor content width"
	[ "$left" -ge 1 ] || fail "floor pane missing left inset ($left)"
	[ "$right" -lt 15 ] || fail "floor pane outside: R=$right"
done <"$TMP/floor.panes"
$TMUX kill-server

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
# Switching to separate after a joined -l1 split grows undersized cells
# ---------------------------------------------------------------------------
# Full-height split -fhl1 under joined leaves a 1-column right cell. Separate
# needs at least 3 columns for the right edge (content + both gutters).
$TMUX new-session -d -s reflow -x 80 -y 12 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -w pane-border-type joined || fail "set joined failed"
$TMUX split-window -fh -l 1 -t reflow:0 'cat' || fail "split -fhl1 failed"
$TMUX set -w pane-border-type separate || fail "set separate failed"
right_w=$($TMUX display-message -p -t reflow:0.1 '#{pane_width}')
right_l=$($TMUX display-message -p -t reflow:0.1 '#{pane_left}')
right_r=$($TMUX display-message -p -t reflow:0.1 '#{pane_right}')
win_w=$($TMUX display-message -p -t reflow:0 '#{window_width}')
must_equal "$right_w" "1" "right content width after separate reflow"
[ "$right_l" -ge 1 ] || fail "right pane missing left inset ($right_l)"
[ "$right_r" -lt "$win_w" ] ||
	fail "right pane missing right inset: R=$right_r win=$win_w"
$TMUX kill-server

# ---------------------------------------------------------------------------
# Always-on scrollbars: joined horizontal split needs space for both bars
# ---------------------------------------------------------------------------
# Default style is width 1 pad 0: min is (1+1)+(1+1)+1 = 5. Width 4 must fail.
$TMUX new-session -d -s sball -x 4 -y 12 'cat' || exit 1
$TMUX set -g status off || fail "status off failed"
$TMUX set -g pane-scrollbars on || fail "pane-scrollbars on failed"
$TMUX set -w pane-border-type joined || fail "set joined failed"
$TMUX split-window -h -t sball:0 'cat' 2>"$TMP/sball.err" &&
	fail "joined split with scrollbars on width 4 should fail"
grep -q 'no space for a new pane' "$TMP/sball.err" ||
	fail "expected no-space error, got: $(cat "$TMP/sball.err")"
must_equal "$($TMUX list-panes -t sball:0 | wc -l | tr -d ' ')" "1" \
	"joined no-space must leave a single pane"
# Width 5 is the floor and must succeed with sane sizes.
$TMUX resize-window -t sball:0 -x 5 || fail "resize to 5 failed"
$TMUX split-window -h -t sball:0 'cat' || fail "joined split width 5 failed"
must_equal "$($TMUX list-panes -t sball:0 | wc -l | tr -d ' ')" "2" \
	"joined scrollbars split at floor width"
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

exit 0

