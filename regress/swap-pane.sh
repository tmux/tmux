#!/bin/sh

# swap-pane swaps positions: the layout cell (and so floating or tiled), the
# geometry, and the zoomed and hidden state belong to the position and stay
# behind. Check this within a window and across windows.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	exit 1
}

run()
{
	$TMUX "$@" || fail "failed: $*"
}

# check <pane> <expected floating:zoomed:hidden:window>
check()
{
	got=$(run display-message -p -t "$1" \
	    '#{pane_floating_flag}:#{pane_zoomed_flag}:#{pane_hidden_flag}:#{window_index}') || exit 1
	[ "$got" = "$2" ] || fail "$3: $1 got '$got', expected '$2'"
}

# checkwin <window> <expected window_zoomed_flag:window_panes> <label>
checkwin()
{
	got=$(run display-message -p -t "$1" \
	    '#{window_zoomed_flag}:#{window_panes}') || exit 1
	[ "$got" = "$2" ] || fail "$3: window $1 got '$got', expected '$2'"
}

# checkgeom <pane> <expected WxH+X+Y> <label>
checkgeom()
{
	got=$(run display-message -p -t "$1" \
	    '#{pane_width}x#{pane_height}+#{pane_left}+#{pane_top}') || exit 1
	[ "$got" = "$2" ] || fail "$3: $1 geometry got '$got', expected '$2'"
}

# checkactive <window> <expected pane> <label>
checkactive()
{
	got=$(run display-message -p -t "$1" '#{pane_id}') || exit 1
	[ "$got" = "$2" ] || fail "$3: window $1 active got '$got', expected '$2'"
}

# The layout has to survive being read back, which catches a cell that no
# longer matches its pane.
checklayout()
{
	l=$(run display-message -p -t "$1" '#{window_layout}') || exit 1
	run select-layout -t "$1" "$l"
	got=$(run display-message -p -t "$1" '#{window_layout}') || exit 1
	[ "$got" = "$l" ] || fail "$2: layout of $1 changed on reload"
}

run new-session -d -x 80 -y 24
a=$(run display-message -p '#{pane_id}') || exit 1
b=$(run split-window -dPF '#{pane_id}') || exit 1
f=$(run new-pane -dPF '#{pane_id}' -x 20 -y 8 -X 8 -Y 3) || exit 1
run new-window -d
c=$(run display-message -p -t :1 '#{pane_id}') || exit 1
d=$(run split-window -dPF '#{pane_id}' -t :1) || exit 1

# a is the top tiled pane, b the bottom one, f is a float and c and d are the
# tiled panes in window 1.
check $a 0:0:0:0 start
check $f 1:0:0:0 start
check $c 0:0:0:1 start
checkgeom $a 80x12+0+0 start
checkgeom $b 80x11+0+13 start
checkgeom $f 18x6+9+4 start

# Two tiled panes in the same window trade places.
run swap-pane -d -s $a -t $b
checkgeom $a 80x11+0+13 'tiled swap'
checkgeom $b 80x12+0+0 'tiled swap'
check $a 0:0:0:0 'tiled swap'
check $b 0:0:0:0 'tiled swap'
checklayout :0 'tiled swap'
checkactive :0 $a 'tiled swap keeps active pane'
run swap-pane -d -s $a -t $b
checkgeom $a 80x12+0+0 'tiled swap back'

# A tiled pane and a float exchange state as well as place.
run swap-pane -d -s $b -t $f
check $b 1:0:0:0 'tiled-float swap'
check $f 0:0:0:0 'tiled-float swap'
checkgeom $b 18x6+9+4 'tiled-float swap'
checkgeom $f 80x11+0+13 'tiled-float swap'
checklayout :0 'tiled-float swap'
run swap-pane -d -s $b -t $f
check $b 0:0:0:0 'tiled-float swap back'
check $f 1:0:0:0 'tiled-float swap back'
checkgeom $f 18x6+9+4 'tiled-float swap back'

# The same across windows.
run swap-pane -d -s $f -t $d
check $f 0:0:0:1 'float to other window'
check $d 1:0:0:0 'float to other window'
checkgeom $f 80x11+0+13 'float to other window'
checkgeom $d 18x6+9+4 'float to other window'
checkwin :0 0:3 'float to other window'
checkwin :1 0:2 'float to other window'
checklayout :0 'float to other window'
checklayout :1 'float to other window'
run swap-pane -d -s $f -t $d
check $f 1:0:0:0 'float to other window back'
check $d 0:0:0:1 'float to other window back'

# Zoom stays with the position. Swapping the zoomed pane with an unzoomed one
# in the same window leaves the position zoomed.
run resize-pane -Z -t $a
checkwin :0 1:3 'zoom a'
run swap-pane -s $a -t $b
check $a 0:0:0:0 'zoomed-tiled swap'
check $b 0:1:0:0 'zoomed-tiled swap'
checkgeom $b 80x24+0+0 'zoomed-tiled swap'
checkwin :0 1:3 'zoomed-tiled swap'
run swap-pane -s $b -t $a
check $a 0:1:0:0 'zoomed-tiled swap back'
check $b 0:0:0:0 'zoomed-tiled swap back'

# With -d the active pane stays active, unless the swap covers it with the
# zoomed pane: the zoom must not be hidden, so the pane that took the active
# pane's place becomes active instead.
run select-pane -t $a
run swap-pane -d -s $a -t $b
check $a 0:0:0:0 'zoomed swap -d'
check $b 0:1:0:0 'zoomed swap -d'
checkwin :0 1:3 'zoomed swap -d'
checkactive :0 $b 'zoomed swap -d'
run swap-pane -d -s $b -t $a
check $a 0:1:0:0 'zoomed swap -d back'
check $b 0:0:0:0 'zoomed swap -d back'
checkwin :0 1:3 'zoomed swap -d back'
checkactive :0 $a 'zoomed swap -d back'

# A zoomed tiled pane and a float: the float fills the window, the tiled pane
# becomes the float.
run swap-pane -s $a -t $f
check $a 1:0:0:0 'zoomed-float swap'
check $f 0:1:0:0 'zoomed-float swap'
checkgeom $a 18x6+9+4 'zoomed-float swap'
checkgeom $f 80x24+0+0 'zoomed-float swap'
checkwin :0 1:3 'zoomed-float swap'
run swap-pane -s $f -t $a
check $a 0:1:0:0 'zoomed-float swap back'
check $f 1:0:0:0 'zoomed-float swap back'
run resize-pane -Z -t $a
checkwin :0 0:3 'unzoom a'

# A zoomed float swapped with a tiled pane: the tiled pane becomes the zoomed
# float.
run resize-pane -Z -t $f
check $f 1:1:0:0 'zoom f'
run swap-pane -s $f -t $b
check $f 0:0:0:0 'zoomed-float-tiled swap'
check $b 1:1:0:0 'zoomed-float-tiled swap'
checkgeom $b 80x24+0+0 'zoomed-float-tiled swap'
checkwin :0 1:3 'zoomed-float-tiled swap'
run swap-pane -s $b -t $f
run resize-pane -Z -t $f
checkwin :0 0:3 'unzoom f'

# Across windows: a zoomed pane swapped with an unzoomed one. Window 0 stays
# zoomed with the new pane in the zoomed position and window 1 stays unzoomed.
run resize-pane -Z -t $a
run swap-pane -s $a -t $c
check $a 0:0:0:1 'zoom across windows'
check $c 0:1:0:0 'zoom across windows'
checkgeom $c 80x24+0+0 'zoom across windows'
checkgeom $a 80x12+0+0 'zoom across windows'
checkwin :0 1:3 'zoom across windows'
checkwin :1 0:2 'zoom across windows'
checklayout :0 'zoom across windows'
checklayout :1 'zoom across windows'

# And back, this time with the unzoomed pane as the source.
run swap-pane -s $c -t $a
check $a 0:1:0:0 'zoom across windows back'
check $c 0:0:0:1 'zoom across windows back'
checkwin :0 1:3 'zoom across windows back'
checkwin :1 0:2 'zoom across windows back'

# Both windows zoomed and the zoomed panes swapped: both stay zoomed.
run resize-pane -Z -t $c
checkwin :1 1:2 'zoom both'
run swap-pane -s $a -t $c
check $a 0:1:0:1 'zoomed both swap'
check $c 0:1:0:0 'zoomed both swap'
checkwin :0 1:3 'zoomed both swap'
checkwin :1 1:2 'zoomed both swap'
run swap-pane -s $c -t $a
run resize-pane -Z -t $c
run resize-pane -Z -t $a
checkwin :0 0:3 'unzoom all'
checkwin :1 0:2 'unzoom all'

# A zoomed pane swapped with a covered tiled pane from another window.
run resize-pane -Z -t $a
run swap-pane -s $a -t $d
check $a 0:0:0:1 'zoomed-tiled with other window'
check $d 0:1:0:0 'zoomed-tiled with other window'
checkwin :0 1:3 'zoomed-tiled with other window'
checkwin :1 0:2 'zoomed-tiled with other window'
run swap-pane -s $d -t $a
run resize-pane -Z -t $a
checkwin :0 0:3 'unzoom a again'

# Hidden state also stays with the position.
run resize-pane -H -t $f
check $f 1:0:1:0 'hide f'
run swap-pane -d -s $f -t $b
check $f 0:0:0:0 'hidden float swap'
check $b 1:0:1:0 'hidden float swap'
checkgeom $f 80x11+0+13 'hidden float swap'
run swap-pane -d -s $f -t $b
check $f 1:0:1:0 'hidden float swap back'
check $b 0:0:0:0 'hidden float swap back'

# And across windows.
run swap-pane -d -s $f -t $d
check $f 0:0:0:1 'hidden float to other window'
check $d 1:0:1:0 'hidden float to other window'
checkwin :0 0:3 'hidden float to other window'
checkwin :1 0:2 'hidden float to other window'
run swap-pane -d -s $f -t $d
run resize-pane -H -t $f
check $f 1:0:0:0 'show f'

# Without -d the destination becomes active, within and across windows.
run select-pane -t $a
run swap-pane -s $a -t $b
checkactive :0 $b 'swap active'
run swap-pane -s $a -t $b
run select-pane -t $c
run swap-pane -s $a -t $c
checkactive :0 $c 'swap active across windows'
checkactive :1 $a 'swap active across windows'
run swap-pane -s $a -t $c
run select-window -t :0

# -D and -U wrap around the tiled panes, skipping floats.
run select-pane -t $a
run swap-pane -D -d
checkgeom $a 80x11+0+13 '-D'
checkgeom $b 80x12+0+0 '-D'
check $f 1:0:0:0 '-D'
run swap-pane -U -d
checkgeom $a 80x12+0+0 '-U'
checkgeom $b 80x11+0+13 '-U'
$TMUX swap-pane -D -t $f 2>/dev/null && fail '-D on a float succeeded'
$TMUX swap-pane -U -t $f 2>/dev/null && fail '-U on a float succeeded'

# With -Z, a window that was zoomed ends up with just the active pane zoomed.
run resize-pane -Z -t $a
run resize-pane -Z -t $f
check $a 0:1:0:0 'two zooms'
check $f 1:1:0:0 'two zooms'
run select-pane -t $a
run swap-pane -Z -s $a -t $b
check $b 0:1:0:0 '-Z'
check $f 1:0:0:0 '-Z'
checkwin :0 1:3 '-Z'
got=$(run list-panes -t :0 -F '#{pane_zoomed_flag}' | grep -c 1) || exit 1
[ "$got" = 1 ] || fail "-Z left $got panes zoomed, expected 1"
run resize-pane -a -Z
checkwin :0 0:3 'unzoom all'

# A modal pane cannot be swapped.
m=$(run new-pane -OPF '#{pane_id}' -t $a -x 10 -y 4 -X 2 -Y 2) || exit 1
$TMUX swap-pane -s $m -t $b 2>/dev/null && fail 'swapped a modal pane'
$TMUX swap-pane -s $b -t $m 2>/dev/null && fail 'swapped with a modal pane'

exit 0
