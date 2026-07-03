#!/bin/sh

# Test legacy and extended custom layout strings.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Llayout-custom-test -f/dev/null"

fail()
{
	echo "$*" >&2
	$TMUX kill-server 2>/dev/null
	exit 1
}

must_equal()
{
	[ "$1" = "$2" ] || fail "got '$1', expected '$2'"
}

must_fail()
{
	"$@" >/dev/null 2>&1 && fail "unexpected success: $*"
	return 0
}

$TMUX kill-server 2>/dev/null
sleep 0.5
$TMUX new-session -d -x 80 -y 24 || exit 1
$TMUX split-window -h || fail "split-window failed"

# Legacy comma-separated layouts are accepted and emitted in the new form.
legacy='89f5,80x24,0,0{39x24,0,0,0,40x24,40,0,1}'
$TMUX select-layout "$legacy" || fail "legacy layout was rejected"
layout=$($TMUX display-message -p '#{window_layout}')
must_equal "$layout" \
    '93fc,80x24+0+0{%0,0:39x24+0+0;%1,1:40x24+40+0}'

# Both current and legacy layouts may omit the outer checksum.
$TMUX select-layout '80x24+0+0{%0,0:39x24+0+0;%1,1:40x24+40+0}' || \
    fail "checksumless current layout was rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"
$TMUX select-layout '80x24,0,0{39x24,0,0,0,40x24,40,0,1}' || \
    fail "checksumless legacy layout was rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"
must_fail $TMUX select-layout \
    '0000,80x24+0+0{%0,0:39x24+0+0;%1,1:40x24+40+0}'

# Top-bottom containers are accepted in both current and legacy formats.
topbottom='081e,80x24+0+0[%0,0:80x11+0+0;%1,1:80x12+0+12]'
$TMUX select-layout "$topbottom" || fail "top-bottom layout was rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$topbottom"
must_equal "$($TMUX list-panes -F '#{pane_width}x#{pane_height},#{pane_left},#{pane_top}')" \
    '80x11,0,0
80x12,0,12'
$TMUX select-layout '80x24,0,0[80x11,0,0,0,80x12,0,12,1]' || \
    fail "legacy top-bottom layout was rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$topbottom"
$TMUX select-layout "$layout" || fail "layout could not be restored"

# Exercise select-layout command paths around custom layout parsing.
$TMUX select-layout even-vertical || fail "named layout failed"
$TMUX select-layout -n || fail "next layout failed"
$TMUX select-layout -p || fail "previous layout failed"
$TMUX select-layout -o || fail "old layout failed"
$TMUX select-layout -E || fail "spread layout failed"
$TMUX select-layout "$layout" || fail "layout could not be restored"

# A command-zoomed tiled pane emits the z flag without changing its saved
# tiled geometry.
$TMUX resize-pane -t %0 -Z || fail "tiled zoom failed"
must_equal "$($TMUX display-message -p '#{window_layout}')" \
    '8e46,80x24+0+0{%0,0:39x24+0+0:z;%1,1:40x24+40+0}'
$TMUX resize-pane -t %0 -Z || fail "tiled unzoom failed"
$TMUX select-layout "$layout" || fail "layout could not be restored"

# Whitespace and newlines between tokens do not affect the checksum.
indented='93fc,
  80x24 +0 +0 {
    %0, 0: 39x24 +0 +0;
    %1, 1: 40x24 +40 +0
  }'
$TMUX select-layout "$indented" || fail "indented layout was rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"

# Reported legacy compatibility case: resize two existing panes to 113x28.
reported='e6db,113x28,0,0{56x28,0,0,0,56x28,57,0,1}'
$TMUX select-layout "$reported" || fail "reported legacy layout was rejected"
must_equal "$($TMUX display-message -p '#{window_width}x#{window_height}')" \
    '113x28'
must_equal "$($TMUX list-panes -F '#{pane_width}x#{pane_height},#{pane_left},#{pane_top}')" \
    '56x28,0,0
56x28,57,0'

# Layouts from before pane IDs were added remain accepted.
historical='bb62,159x48,0,0{79x48,0,0,79x48,80,0}'
$TMUX select-layout "$historical" || fail "legacy layout without IDs failed"
must_equal "$($TMUX display-message -p '#{window_width}x#{window_height}')" \
    '159x48'

# Return to the smaller layout for the remaining parser tests.
$TMUX select-layout "$legacy" || fail "legacy layout could not be reapplied"
layout=$($TMUX display-message -p '#{window_layout}')

# A checksum may be attached to each nested cell but is not emitted.
nested='73a1,80x24+0+0{6806,%0,0:39x24+0+0;6bda,%1,1:40x24+40+0}'
$TMUX select-layout "$nested" || fail "nested checksums were rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"

# A bad nested checksum is rejected without changing the current layout.
badnested='2fa4,80x24+0+0{dead,%0,0:39x24+0+0;6bda,%1,1:40x24+40+0}'
must_fail $TMUX select-layout "$badnested"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"

# Hidden state is retained in the serialized cell flags.
hidden='659f,80x24+0+0{%0,0:39x24+0+0;%1,1:40x24+40+0:h}'
$TMUX select-layout "$hidden" || fail "hidden layout was rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$hidden"

# Pane IDs map by identity when all match and by tree order when none match.
disjoint='80x24+0+0{%100,0:39x24+0+0;%101,1:40x24+40+0}'
$TMUX select-layout "$disjoint" || fail "disjoint pane IDs were rejected"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"
identity='80x24+0+0{%1,0:39x24+0+0;%0,1:40x24+40+0}'
$TMUX select-layout "$identity" || fail "matching pane IDs were rejected"
must_equal "$($TMUX display-message -p -t %0 '#{pane_left}')" 40
$TMUX select-layout "$legacy" || fail "legacy layout could not be restored"

# Duplicate or partially matching IDs fail when pane counts match.
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0;%100,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%100,0:39x24+0+0;%100,1:40x24+40+0}'

# A target with fewer panes drops cells from the end, compacts z-indexes and
# assigns the remaining cells in tree order. A target with more panes fails.
fewer='80x24+0+0{%100,1:26x24+0+0;%101,2:26x24+27+0;%102,0:26x24+54+0:z}'
$TMUX select-layout "$fewer" || fail "surplus current cells were not removed"
must_equal "$($TMUX display-message -p '#{window_panes}')" 2
must_equal "$($TMUX display-message -p -t %0 '#{pane_left},#{pane_width}')" \
    '0,26'
must_equal "$($TMUX display-message -p -t %1 '#{pane_left},#{pane_width}')" \
    '27,53'
must_equal "$($TMUX display-message -p -t %0 '#{pane_zoomed_flag}')" 0
case "$($TMUX display-message -p '#{window_layout}')" in
*'%0,0:26x24+0+0;%1,1:53x24+27+0}'*) ;;
*) fail "z-indexes were not compacted after removing a cell" ;;
esac
# The complete tree must be structurally valid before surplus cells are
# removed; the last child has the wrong height here.
must_fail $TMUX select-layout \
    '80x24+0+0{%100,0:39x24+0+0;%101,1:20x24+40+0;%102,2:19x10+61+0}'
must_fail $TMUX select-layout '%100,0:80x24+0+0'
must_fail $TMUX select-layout '80x24,0,0,0'
$TMUX select-layout "$legacy" || fail "legacy layout could not be restored"

# Mixed legacy and new syntax, malformed containers, duplicate z-indices,
# non-contiguous z-indices, invalid z-ordering and unknown flags fail.
must_fail $TMUX select-layout \
    '93ed,80x24+0+0{%0,0:39x24+0+0,%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24,0,0{39x24,0,0,0;%1,1:40x24+40+0}'
must_fail $TMUX select-layout '80x24+0+0{}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0;}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0|%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '93ec,80x24+0+0{%0,0:39x24+0+0;%1,0:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0;%1,2:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0;%1,1:40x24+40+0:f}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:38x24+0+0;%1,1:40x24+39+0}'
must_fail $TMUX select-layout \
    '0e42,80x24+0+0{%0,0:39x24+0+0:q;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0:ff;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0:hh;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0:zz;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0:z;%1,1:40x24+40+0:z}'
must_fail $TMUX select-layout \
    '80x24,0,0{39x24,0,0,0,40x24,40,0,}'

# Truncated and overflowing values must fail without terminating the server.
must_fail $TMUX select-layout '0000,'
must_fail $TMUX select-layout \
    'c523,999999999999999999999999x24,0,0,0'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:0x24+0+0;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+2147483647+0;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+0;%1,1:40x24+-10001+0}'
must_fail $TMUX select-layout \
    '80x24-1+0{%0,0:39x24+0+0;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24-1+0;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24+0+;%1,1:40x24+40+0}'
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:39x24++ +0;%1,1:40x24+40+0}'
long='80x24+0+0{%0,0:39x24+0+0;%1,1:40x24+40+0}'
i=0
while [ $i -lt 9000 ]; do
	long="${long}x"
	i=$((i + 1))
done
must_fail $TMUX select-layout "$long"
$TMUX list-windows >/dev/null || fail "server exited after invalid layouts"

# Invalid custom layouts must not unzoom the target window.
$TMUX resize-pane -t %0 -Z || fail "zoom failed"
must_fail $TMUX select-layout \
    '80x24+0+0{%0,0:0x24+0+0;%1,1:40x24+40+0}'
must_equal "$($TMUX display-message -p -t %0 '#{pane_zoomed_flag}')" 1
$TMUX resize-pane -t %0 -Z || fail "unzoom failed"

# The parser accepts its maximum nesting depth and safely rejects one more.
$TMUX kill-server 2>/dev/null
sleep 0.5
$TMUX new-session -d -x 80 -y 24 || exit 1
legacy_negative='a9af,%0,0:20x8+0+0'
$TMUX select-layout '20x8,-10,-20,0' || fail "legacy negative layout failed"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$legacy_negative"
deep='%0,0:1x1+0+0'
i=1
while [ $i -lt 64 ]; do
	deep="1x1+0+0{$deep}"
	i=$((i + 1))
done
$TMUX select-layout "$deep" || fail "maximum layout depth was rejected"
must_fail $TMUX select-layout "1x1+0+0{$deep}"

# X-style geometry supports right/bottom-relative offsets, absolute negative
# offsets, doubled plus signs, and omitted positions.
$TMUX kill-server 2>/dev/null
sleep 0.5
$TMUX new-session -d -x 120 -y 40 || exit 1
$TMUX new-pane -d -x 20 -y 8 'sleep 100' || fail "floating pane failed"

# A real floating pane moved partly outside the window emits absolute negative
# offsets in canonical X-style form.
$TMUX move-pane -t %1 -X -10 -Y -20 || fail "negative move-pane failed"
must_equal "$($TMUX display-message -p -t %1 '#{pane_left},#{pane_top},#{pane_width},#{pane_height}')" \
    '-9,-19,18,6'
case "$($TMUX display-message -p '#{window_layout}')" in
*'%1,0:18x6+-9+-19:f'*) ;;
*) fail "negative floating pane was not serialized with +- offsets" ;;
esac

relative='1442,120x40+0+0{%0,1:120x40+0+0;%1,0:30x10-10-20:f}'
$TMUX select-layout "$relative" || fail "relative geometry was rejected"
must_equal "$($TMUX display-message -p -t %1 '#{pane_left},#{pane_top}')" \
    '80,10'
must_equal "$($TMUX display-message -p '#{window_layout}')" \
    '0041,120x40+0+0{%0,1:120x40+0+0;%1,0:30x10+80+10:f}'

bottomright='120x40+0+0{%0,1:120x40+0+0;%1,0:30x10-0-0:f}'
$TMUX select-layout "$bottomright" || fail "bottom-right geometry failed"
must_equal "$($TMUX display-message -p -t %1 '#{pane_left},#{pane_top}')" \
    '90,30'
must_equal "$($TMUX display-message -p '#{window_layout}')" \
    '2242,120x40+0+0{%0,1:120x40+0+0;%1,0:30x10+90+30:f}'

absolute='3a22,120x40+0+0{%0,1:120x40+0+0;%1,0:20x8+-10+-20:f}'
$TMUX select-layout "$absolute" || fail "absolute negative geometry failed"
must_equal "$($TMUX display-message -p -t %1 '#{pane_left},#{pane_top}')" \
    '-10,-20'
must_equal "$($TMUX display-message -p '#{window_layout}')" "$absolute"

hiddenfloat='120x40+0+0{%0,1:120x40+0+0;%1,0:20x8+-10+-20:fh}'
$TMUX select-layout "$hiddenfloat" || fail "hidden floating layout failed"
must_equal "$($TMUX display-message -p -t %1 '#{pane_left},#{pane_top},#{pane_width},#{pane_height}')" \
    '-10,-20,20,8'
must_equal "$($TMUX display-message -p '#{window_layout}')" \
    '9d83,120x40+0+0{%0,1:120x40+0+0;%1,0:20x8+-10+-20:fh}'
must_fail $TMUX select-layout \
    '120x40+0+0{%0,1:120x40+0+0;%1,0:20x8+-10+-20:fhz}'
must_equal "$($TMUX display-message -p '#{window_layout}')" \
    '9d83,120x40+0+0{%0,1:120x40+0+0;%1,0:20x8+-10+-20:fh}'
must_fail $TMUX select-layout \
    '120x40+0+0{%0,1:120x40+0+0;%1,0:10000x8-10000+0:f}'

defaults='fcb6,120x40{%0,1:120x40;%1,0:30x10++80+10:f}'
$TMUX select-layout "$defaults" || fail "default or doubled offsets failed"
must_equal "$($TMUX display-message -p '#{window_layout}')" \
    '0041,120x40+0+0{%0,1:120x40+0+0;%1,0:30x10+80+10:f}'

oneoffset='8f93,120x40{%0,1:120x40;%1,0:30x10+80:f}'
$TMUX select-layout "$oneoffset" || fail "single offset default failed"
must_equal "$($TMUX display-message -p '#{window_layout}')" \
    '5fb9,120x40+0+0{%0,1:120x40+0+0;%1,0:30x10+80+0:f}'

$TMUX kill-server 2>/dev/null
sleep 0.5
$TMUX new-session -d -x 80 -y 24 || exit 1
$TMUX split-window -h || fail "split-window failed"

# Multiple @window-id:layout records may be applied in one command. The
# newline produced by list-windows is insignificant whitespace.
$TMUX new-window -d -n second || fail "second window failed"
$TMUX split-window -d -h -t @1 || fail "second window split failed"
layouts=$($TMUX list-windows -F '#{window_id}:#{window_layout}')
$TMUX select-layout "$layouts" || fail "multiple-window layout was rejected"
must_equal "$($TMUX list-windows -F '#{window_id}:#{window_layout}')" \
    "$layouts"
compact=$(printf %s "$layouts" | tr -d '\n')
$TMUX select-layout "$compact" || fail "compact multiple-window layout failed"

# All records are validated before any window is changed.
before=$($TMUX display-message -p -t @0 '#{window_width}x#{window_height}')
must_fail $TMUX select-layout "@0:$reported@1:0000,"
must_equal "$($TMUX display-message -p -t @0 '#{window_width}x#{window_height}')" \
    "$before"
must_fail $TMUX select-layout '@0'
must_fail $TMUX select-layout '@0:'
must_fail $TMUX select-layout "@0:$legacy@0:$legacy"
must_fail $TMUX select-layout "@999999:$legacy"

# Add floating panes and verify exact z-order, semicolon output and zoom state.
$TMUX kill-server 2>/dev/null
sleep 0.5
$TMUX new-session -d -x 120 -y 40 || exit 1
$TMUX split-window -h || fail "split-window failed"
$TMUX new-pane -d -x 30 -y 10 -X 10 -Y 5 'sleep 100' || \
    fail "first floating pane failed"
$TMUX new-pane -d -x 24 -y 8 -X 50 -Y 12 'sleep 100' || \
    fail "second floating pane failed"

layout=$($TMUX display-message -p '#{window_layout}')
must_equal "$layout" \
    'a6f2,120x40+0+0{%0,2:60x40+0+0;%1,3:59x40+61+0;%3,0:22x6+51+13:f;%2,1:28x8+11+6:f}'
case "$layout" in
*'<'*|*'>'*) fail "obsolete floating delimiters were emitted" ;;
esac
case "$layout" in
*';'*',0:'*':f'*) ;;
*) fail "new layout does not contain semicolons and front z-index: $layout" ;;
esac
case "$layout" in
*',1:'*':f'*) ;;
*) fail "new layout does not contain the second floating z-index: $layout" ;;
esac
$TMUX select-layout "$layout" || fail "new layout did not round trip"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$layout"

$TMUX resize-pane -t %2 -Z || fail "zoom failed"
zoomed=$($TMUX display-message -p '#{window_layout}')
case "$zoomed" in
*':fz'*) ;;
*) fail "zoom flag was not emitted: $zoomed" ;;
esac
$TMUX select-layout "$zoomed" || fail "zoomed layout did not round trip"
must_equal "$($TMUX display-message -p '#{window_layout}')" "$zoomed"
must_equal "$($TMUX display-message -p -t %2 '#{pane_zoomed_flag}')" 1

$TMUX kill-server 2>/dev/null
