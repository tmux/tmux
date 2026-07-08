#!/bin/sh

# Tests of pane management command semantics (not parsing), as implemented in
# cmd-split-window.c, cmd-break-pane.c, cmd-join-pane.c (join-pane and
# move-pane), cmd-swap-pane.c, cmd-kill-pane.c, cmd-respawn-pane.c,
# cmd-respawn-window.c, cmd-resize-pane.c and cmd-select-pane.c.
#
# This exercises:
# - split-window -h/-v with -l in cells and percent, -b placing the new pane
#   before (left/top of) the target and -f spanning the full window size;
# - break-pane moving a pane into a new window (-d, -n name, -a after, -P -F
#   printing the new location);
# - join-pane moving a window's only pane into another window (destroying the
#   source window), -b before, -l size, and the identical-panes error;
# - move-pane as an alias for join-pane;
# - swap-pane -U/-D/-s/-t, -d keeping the active pane, and the marked pane
#   (select-pane -m/-M) as the default swap source;
# - kill-pane, kill-pane -a keeping only the target;
# - respawn-pane/respawn-window refusing a live pane without -k, working on a
#   dead pane (remain-on-exit) and killing with -k;
# - resize-pane -x/-y in cells and percent, -L/-R/-U/-D adjustments and -Z
#   zoom/unzoom (including implicit unzoom on split).
#
# window-ops.sh covers window-level commands and buffers.sh paste buffers.

PATH=/bin:/usr/bin
TERM=screen
LANG=C.UTF-8
LC_ALL=C.UTF-8
export TERM LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

# check_ok $cmd...
#
# Run a command and require that it succeeds.
check_ok()
{
	if ! $TMUX "$@"; then
		echo "Command failed (expected success): $*"
		exit 1
	fi
}

# check_fail $expected_error $cmd...
#
# Run a command and require that it fails with the given error message.
check_fail()
{
	exp="$1"
	shift
	out=$($TMUX "$@" 2>&1)
	if [ $? -eq 0 ]; then
		echo "Command succeeded (expected failure): $*"
		exit 1
	fi
	if [ "$out" != "$exp" ]; then
		echo "Wrong error for: $*"
		echo "Expected: '$exp'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_fmt $target $format $expected
#
# Expand a format in a target's context and compare with $expected.
check_fmt()
{
	out=$($TMUX display-message -p -t "$1" "$2" 2>&1)
	if [ "$out" != "$3" ]; then
		echo "Format '$2' for '$1' wrong."
		echo "Expected: '$3'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_panes $target $expected
#
# Compare the pane list of a window (as "index:id ...", in index order) with
# $expected.
check_panes()
{
	out=$(echo $($TMUX list-panes -t "$1" -F '#{pane_index}:#{pane_id}'))
	if [ "$out" != "$2" ]; then
		echo "Pane list of '$1' wrong."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		exit 1
	fi
}

assert_alive()
{
	if [ "$($TMUX display-message -p alive 2>&1)" != "alive" ]; then
		echo "Server died: $1"
		exit 1
	fi
}

# ---------------------------------------------------------------------------
# split-window geometry.

check_ok new-session -d -s P -x 80 -y 24 -n main
p0=$($TMUX display-message -p -t P:0.0 '#{pane_id}')

# Horizontal split with -l in cells: new pane gets exactly that width and the
# old pane the rest minus the separator line.
check_ok split-window -d -h -l 20 -t "$p0"
p1=$($TMUX display-message -p -t P:0.1 '#{pane_id}')
check_fmt "$p1" '#{pane_width}x#{pane_height}' '20x24'
check_fmt "$p0" '#{pane_width}x#{pane_height}' '59x24'

# Vertical split with a percentage of the pane being split.
check_ok split-window -d -v -l 25% -t "$p0"
p2=$($TMUX display-message -p -t P:0.1 '#{pane_id}')
check_fmt "$p2" '#{pane_width}x#{pane_height}' '59x6'
check_fmt "$p0" '#{pane_width}x#{pane_height}' '59x17'

# -b puts the new pane to the left of the target; -f makes it span the full
# window height.
check_ok split-window -d -h -b -f -l 10 -t "$p0"
p3=$($TMUX display-message -p -t P:0.0 '#{pane_id}')
check_fmt "$p3" '#{pane_width}x#{pane_height}' '10x24'
check_fmt "$p3" '#{pane_left},#{pane_top}' '0,0'
check_fmt "$p0" '#{pane_width}x#{pane_height}' '50x17'
check_panes P:0 "0:$p3 1:$p0 2:$p2 3:$p1"

# The new pane becomes active unless -d is given.
check_ok select-pane -t "$p0"
check_ok split-window -d -v -t "$p0"
check_fmt 'P:0' '#{pane_id}' "$p0"
p4=$($TMUX display-message -p -t P:0.2 '#{pane_id}')
check_ok split-window -v -t "$p4"
p5=$($TMUX display-message -p -t 'P:0' '#{pane_id}')
check_ok kill-pane -t "$p5"
check_ok kill-pane -t "$p4"
check_panes P:0 "0:$p3 1:$p0 2:$p2 3:$p1"

# ---------------------------------------------------------------------------
# break-pane and join-pane.

# break-pane moves a pane to a new window; -P -F prints where it went and -n
# names the new window.
out=$($TMUX break-pane -d -P -F '#{window_index}:#{pane_id}' -n broken \
	-s "$p1" -t P:)
if [ "$out" != "1:$p1" ]; then
	echo "break-pane -P output wrong: '$out'"
	exit 1
fi
check_fmt 'P:1' '#{window_name}:#{window_panes}' 'broken:1'
check_fmt 'P:0' '#{window_panes}' '3'

# join-pane -v moves it back (the source window, left empty, is destroyed).
check_ok join-pane -d -v -s P:broken.0 -t "$p2"
check_fmt 'P:0' '#{window_panes}' '4'
if $TMUX has-session -t P:broken 2>/dev/null; then
	echo "Window 'broken' still exists after join-pane."
	exit 1
fi

# The joined pane is below the target (-v, no -b).
top=$($TMUX display-message -p -t "$p2" '#{pane_bottom}')
joined=$($TMUX display-message -p -t "$p1" '#{pane_top}')
if [ "$joined" -le "$top" ]; then
	echo "Joined pane is not below target ($joined <= $top)."
	exit 1
fi

# join-pane -h -b puts the source to the left of the target; -l sets size.
check_ok break-pane -d -n broken -s "$p1" -t P:
check_ok join-pane -d -h -b -l 30 -s P:broken.0 -t "$p2"
check_fmt "$p1" '#{pane_width}' '30'
l1=$($TMUX display-message -p -t "$p1" '#{pane_left}')
l2=$($TMUX display-message -p -t "$p2" '#{pane_left}')
if [ "$l1" -ge "$l2" ]; then
	echo "Joined pane is not left of target ($l1 >= $l2)."
	exit 1
fi

# Joining a pane to itself is an error.
check_fail 'source and target panes must be different' \
	join-pane -d -s "$p0" -t "$p0"

# break-pane to an occupied window index or with an invalid (non-UTF-8) name
# is an error.
check_fail 'index in use: 0' break-pane -d -s "$p1" -t P:0
check_fail "invalid window name: $(printf 'a\377b')" \
	break-pane -d -n "$(printf 'a\377b')" -s "$p1" -t P:

# join-pane can move a pane from one window to another without destroying
# the source window if other panes remain. (On this branch move-pane is
# reserved for floating panes, covered by floating-pane-geometry.sh.)
check_ok new-window -d -t P:5 -n other
check_ok join-pane -d -s "$p1" -t P:5.0
check_fmt 'P:5' '#{window_panes}' '2'
check_fmt 'P:0' '#{window_panes}' '3'
check_ok join-pane -d -v -s "$p1" -t "$p2"
check_fmt 'P:0' '#{window_panes}' '4'
check_fmt 'P:5' '#{window_panes}' '1'

# ---------------------------------------------------------------------------
# swap-pane.

check_panes P:0 "0:$p3 1:$p0 2:$p2 3:$p1"

# -s/-t swap two panes.
check_ok swap-pane -d -s "$p3" -t "$p1"
check_panes P:0 "0:$p1 1:$p0 2:$p2 3:$p3"
check_ok swap-pane -d -s "$p3" -t "$p1"
check_panes P:0 "0:$p3 1:$p0 2:$p2 3:$p1"

# -U swaps the target pane with the previous pane, -D with the next; without
# -s the target is the active pane.
check_ok swap-pane -d -U -t "$p0"
check_panes P:0 "0:$p0 1:$p3 2:$p2 3:$p1"
check_ok swap-pane -d -D -t "$p0"
check_panes P:0 "0:$p3 1:$p0 2:$p2 3:$p1"

# Without -d the target pane becomes the active pane (it arrives at the
# source pane's position).
check_ok select-pane -t "$p0"
check_ok swap-pane -s "$p0" -t "$p2"
check_fmt 'P:0' '#{pane_id}' "$p2"
check_panes P:0 "0:$p3 1:$p2 2:$p0 3:$p1"
check_ok swap-pane -s "$p2" -t "$p0"
check_fmt 'P:0' '#{pane_id}' "$p0"
check_panes P:0 "0:$p3 1:$p0 2:$p2 3:$p1"

# With a marked pane and no -s, the marked pane is the swap source.
check_ok select-pane -m -t "$p3"
check_fmt "$p3" '#{pane_marked}' '1'
check_ok swap-pane -d -t "$p1"
check_panes P:0 "0:$p1 1:$p0 2:$p2 3:$p3"
check_ok swap-pane -d -t "$p1"
check_panes P:0 "0:$p3 1:$p0 2:$p2 3:$p1"

# select-pane -M clears the mark.
check_ok select-pane -M
check_fmt "$p3" '#{pane_marked}' '0'
check_fmt 'P:0' '#{pane_marked_set}' '0'

# ---------------------------------------------------------------------------
# resize-pane and zoom.

# Absolute -x on a horizontal split and percentage.
check_ok resize-pane -t "$p3" -x 20
check_fmt "$p3" '#{pane_width}' '20'
check_ok resize-pane -t "$p3" -x 25%
check_fmt "$p3" '#{pane_width}' '20'
check_ok resize-pane -t "$p3" -x 10
check_fmt "$p3" '#{pane_width}' '10'

# Relative adjustments: -R grows a left pane, -L shrinks it back; a count may
# be given.
check_ok resize-pane -t "$p3" -R
check_fmt "$p3" '#{pane_width}' '11'
check_ok resize-pane -t "$p3" -L
check_fmt "$p3" '#{pane_width}' '10'
check_ok resize-pane -t "$p3" -R 5
check_fmt "$p3" '#{pane_width}' '15'
check_ok resize-pane -t "$p3" -L 5
check_fmt "$p3" '#{pane_width}' '10'

# -y on a vertical split.
check_ok resize-pane -t "$p2" -y 10
check_fmt "$p2" '#{pane_height}' '10'

# p2 is the bottom pane, so its bottom border cannot move down: -D instead
# grows it by taking lines from the pane above and -U gives them back.
check_ok resize-pane -t "$p2" -D 2
check_fmt "$p2" '#{pane_height}' '12'
check_ok resize-pane -t "$p2" -U 2
check_fmt "$p2" '#{pane_height}' '10'

# Bad adjustment, width and height are errors.
check_fail 'adjustment invalid' resize-pane -t "$p2" -U nonsense
check_fail 'width invalid' resize-pane -t "$p2" -x nonsense
check_fail 'height invalid' resize-pane -t "$p2" -y nonsense

# -Z zooms: the pane temporarily fills the window and the flags show it.
check_ok resize-pane -Z -t "$p0"
check_fmt "$p0" '#{window_zoomed_flag}:#{pane_width}x#{pane_height}' \
	'1:80x24'

# Zoom is transparent to pane commands on other panes, and -Z again unzooms.
check_ok resize-pane -Z -t "$p0"
check_fmt "$p0" '#{window_zoomed_flag}' '0'

# Splitting while zoomed unzooms first.
check_ok resize-pane -Z -t "$p0"
check_fmt 'P:0' '#{window_zoomed_flag}' '1'
check_ok split-window -d -v -t "$p0"
check_fmt 'P:0' '#{window_zoomed_flag}' '0'
check_fmt 'P:0' '#{window_panes}' '5'
p6=$($TMUX display-message -p -t P:0.2 '#{pane_id}')
check_ok kill-pane -t "$p6"

# ---------------------------------------------------------------------------
# kill-pane.

check_fmt 'P:0' '#{window_panes}' '4'
check_ok kill-pane -t "$p3"
check_panes P:0 "0:$p0 1:$p2 2:$p1"

# -a kills every pane except the target.
check_ok kill-pane -a -t "$p0"
check_panes P:0 "0:$p0"

# Killing the last pane in a window kills the window.
check_ok new-window -d -t P:7 -n goner
check_ok kill-pane -t P:7.0
if $TMUX has-session -t P:goner 2>/dev/null; then
	echo "Window 'goner' still exists after killing its only pane."
	exit 1
fi

# ---------------------------------------------------------------------------
# respawn-pane and respawn-window.

# Respawning a pane whose process is alive fails without -k.
check_fail "respawn pane failed: pane P:0.0 still active" \
	respawn-pane -t P:0.0
check_fail "respawn window failed: window P:0 still active" \
	respawn-window -t P:0

# With remain-on-exit a pane whose command exited stays as a dead pane and
# may be respawned without -k.
check_ok set-option -g remain-on-exit on
check_ok new-window -d -t P:8 -n dead 'true'
i=0
while [ "$($TMUX display-message -p -t P:8.0 '#{pane_dead}')" != "1" ]; do
	i=$((i + 1))
	[ $i -gt 50 ] && { echo "Pane did not die."; exit 1; }
	sleep 0.1
done
check_ok respawn-pane -t P:8.0 'sleep 100'
check_fmt 'P:8.0' '#{pane_dead}' '0'

# -k kills the live process and respawns.
check_ok respawn-pane -k -t P:8.0 'sleep 200'
check_fmt 'P:8.0' '#{pane_dead}' '0'

# respawn-window -k replaces the whole window (all panes) with one pane.
check_ok split-window -d -t P:8
check_fmt 'P:8' '#{window_panes}' '2'
check_ok respawn-window -k -t P:8 'sleep 300'
check_fmt 'P:8' '#{window_panes}' '1'
check_fmt 'P:8.0' '#{pane_dead}' '0'
check_ok set-option -g remain-on-exit off

# ---------------------------------------------------------------------------
# select-pane.

# A 2x2-ish arrangement: q0 on top, q1 bottom-left, q2 bottom-right.
check_ok new-window -d -t P:2 -n sel
q0=$($TMUX display-message -p -t P:2.0 '#{pane_id}')
check_ok split-window -d -v -t "$q0"
q1=$($TMUX display-message -p -t P:2.1 '#{pane_id}')
check_ok split-window -d -h -t "$q1"
q2=$($TMUX display-message -p -t P:2.2 '#{pane_id}')

# Directional selection: -D, -R and -U move by pane position.
check_ok select-pane -t "$q0"
check_ok select-pane -D -t P:2
check_fmt 'P:2' '#{pane_id}' "$q1"
check_ok select-pane -R -t P:2
check_fmt 'P:2' '#{pane_id}' "$q2"
check_ok select-pane -U -t P:2
check_fmt 'P:2' '#{pane_id}' "$q0"

# -l returns to the previously active pane; a window that never had another
# active pane has no last pane.
check_ok select-pane -l -t P:2
check_fmt 'P:2' '#{pane_id}' "$q2"
check_fail 'no last pane' select-pane -l -t P:8.0

# -d disables input to a pane, -e enables it again and -T sets the title.
check_ok select-pane -d -t "$q0"
check_fmt "$q0" '#{pane_input_off}' '1'
check_ok select-pane -e -t "$q0"
check_fmt "$q0" '#{pane_input_off}' '0'
check_ok select-pane -T mytitle -t "$q0"
check_fmt "$q0" '#{pane_title}' 'mytitle'
check_ok kill-window -t P:2

# ---------------------------------------------------------------------------
# more split-window variants.

check_ok new-window -d -t P:2 -n splits

# new-window -E creates an empty initial pane, running no command.
check_ok new-window -d -E -t P:9 -n empty
check_fmt 'P:9.0' '#{pane_dead}' '0'
check_fmt 'P:9.0' '#{pane_pid}' ''
check_fail 'command cannot be given for empty pane' \
    new-window -d -E -t P:10 -n empty 'true'

# An empty string as the sole argument is equivalent to -E: the pane is
# created empty, running no command.
check_ok new-window -d -t P:12 -n empty-str ''
check_fmt 'P:12.0' '#{pane_dead}' '0'
check_fmt 'P:12.0' '#{pane_pid}' ''
check_ok kill-window -t P:12

# A missing command (rather than an empty one) runs the default command, so
# the pane is not empty and has a process.
check_ok new-window -d -t P:12 -n default-cmd
check_fmt 'P:12.0' '#{?pane_pid,live,empty}' 'live'
check_ok kill-window -t P:12

# respawn-pane -E stores the command and cwd without starting it.
tmp=${TMPDIR:-/tmp}/tmux-pane-ops-empty-$$
rm -f "$tmp"
check_ok new-window -d -E -t P:10 -n empty-respawn
check_ok respawn-pane -E -c /tmp -t P:10.0 "pwd > $tmp"
if [ -e "$tmp" ]; then
	echo "respawn-pane -E started command unexpectedly"
	exit 1
fi
check_ok respawn-pane -t P:10.0
i=0
while [ ! -e "$tmp" ]; do
	i=$((i + 1))
	[ $i -gt 50 ] && echo "respawn-pane did not start stored command" && \
	    exit 1
	sleep 0.1
done
if [ "$(cat "$tmp")" != "/tmp" ]; then
	echo "respawn-pane did not use stored cwd"
	exit 1
fi
rm -f "$tmp"
check_ok kill-window -t P:9

# respawn-window -E stores the command and cwd without starting it.
tmp=${TMPDIR:-/tmp}/tmux-window-ops-empty-$$
rm -f "$tmp"
check_ok new-window -d -E -t P:11 -n empty-respawn-window
check_ok respawn-window -E -c /tmp -t P:11 "pwd > $tmp"
check_fmt 'P:11.0' '#{pane_pid}' ''
if [ -e "$tmp" ]; then
	echo "respawn-window -E started command unexpectedly"
	exit 1
fi
check_ok respawn-window -t P:11
i=0
while [ ! -e "$tmp" ]; do
	i=$((i + 1))
	[ $i -gt 50 ] && echo "respawn-window did not start stored command" && \
	    exit 1
	sleep 0.1
done
if [ "$(cat "$tmp")" != "/tmp" ]; then
	echo "respawn-window did not use stored cwd"
	exit 1
fi
rm -f "$tmp"

# -E splits with an empty pane, running no command; giving one is an error.
check_ok split-window -d -E -t P:2.0
check_fmt 'P:2' '#{window_panes}' '2'
check_fail 'command cannot be given for empty pane' \
	split-window -d -E -t P:2.0 'sleep 5'

# An empty string as the sole argument splits with an empty pane, like -E.
eid=$($TMUX split-window -d -P -F '#{pane_id}' -t P:2.0 '')
check_fmt "$eid" '#{pane_dead}' '0'
check_fmt "$eid" '#{pane_pid}' ''
check_ok kill-pane -t "$eid"
check_fmt 'P:2' '#{window_panes}' '2'

# -e adds to the new pane's environment.
eid=$($TMUX split-window -d -P -F '#{pane_id}' -e GREETING=hello -t P:2.0 \
	'echo $GREETING; exec cat')
i=0
while out=$($TMUX capture-pane -p -t "$eid" | sed -n 1p) && \
    [ "$out" != "hello" ]; do
	i=$((i + 1))
	[ $i -gt 50 ] && { echo "split-window -e wrong: '$out'"; exit 1; }
	sleep 0.1
done

# A bad -l size is an error.
check_fail 'invalid tiled geometry invalid' \
	split-window -d -v -l invalid -t P:2.0

# -Z zooms the new pane.
check_ok split-window -d -Z -t P:2.0
check_fmt 'P:2' '#{window_zoomed_flag}' '1'
check_ok kill-window -t P:2

# ---------------------------------------------------------------------------
# more swap-pane: wrapping, cross-window, self and zoomed swaps.

check_ok new-window -d -t P:3 -n swaps
check_ok split-window -d -v -t P:3.0
check_ok split-window -d -v -t P:3.0
r0=$($TMUX display-message -p -t P:3.0 '#{pane_id}')
r1=$($TMUX display-message -p -t P:3.1 '#{pane_id}')
r2=$($TMUX display-message -p -t P:3.2 '#{pane_id}')
o0=$($TMUX display-message -p -t P:5.0 '#{pane_id}')

# -D on the last pane and -U on the first wrap around to the other end.
check_ok swap-pane -d -D -t "$r2"
check_panes P:3 "0:$r2 1:$r1 2:$r0"
check_ok swap-pane -d -s "$r0" -t "$r2"
check_ok swap-pane -d -U -t "$r0"
check_panes P:3 "0:$r2 1:$r1 2:$r0"
check_ok swap-pane -d -s "$r0" -t "$r2"
check_panes P:3 "0:$r0 1:$r1 2:$r2"

# Swapping a pane with itself quietly does nothing.
check_ok swap-pane -d -s "$r1" -t "$r1"
check_panes P:3 "0:$r0 1:$r1 2:$r2"

# Panes can be swapped between different windows.
check_ok swap-pane -d -s "$o0" -t "$r1"
check_panes P:3 "0:$r0 1:$o0 2:$r2"
check_panes P:5 "0:$r1"
check_ok swap-pane -d -s "$r1" -t "$o0"
check_panes P:3 "0:$r0 1:$r1 2:$r2"
check_panes P:5 "0:$o0"

# -Z keeps the window zoomed across the swap.
check_ok resize-pane -Z -t "$r0"
check_ok swap-pane -d -Z -s "$r0" -t "$r1"
check_fmt 'P:3' '#{window_zoomed_flag}' '1'
check_ok resize-pane -Z -t P:3
check_panes P:3 "0:$r1 1:$r0 2:$r2"

# kill-pane -a -f only kills other panes matching the filter.
check_ok kill-pane -a -f '#{==:#{pane_id},'"$r2"'}' -t "$r1"
check_panes P:3 "0:$r1 1:$r0"
check_ok kill-window -t P:3

# ---------------------------------------------------------------------------
# split-window -I and -s.

check_ok new-window -d -t P:3 -n splits2

# -I fills the new (empty) pane from standard input.
printf 'stdin-stuff' | $TMUX split-window -d -I -t P:3.0
if [ $? -ne 0 ]; then
	echo "split-window -I failed."
	exit 1
fi
i=0
while out=$($TMUX capture-pane -p -t P:3.1 | sed -n 1p) && \
    [ "$out" != "stdin-stuff" ]; do
	i=$((i + 1))
	[ $i -gt 50 ] && { echo "split-window -I wrong: '$out'"; exit 1; }
	sleep 0.1
done

# -s sets the new pane's window-style.
sid=$($TMUX split-window -d -P -F '#{pane_id}' -s 'bg=red' -t P:3.0)
out=$($TMUX show-options -v -p -t "$sid" window-style)
if [ "$out" != "bg=red" ]; then
	echo "split-window -s style wrong: '$out'"
	exit 1
fi
check_ok kill-window -t P:3

# ---------------------------------------------------------------------------
# more break-pane: -a insertion, selection and single-pane windows.

# check_windows $session $expected
#
# Compare the window list of a session (as "index:name ...") with $expected.
check_windows()
{
	out=$(echo $($TMUX list-windows -t "$1" -F \
	    '#{window_index}:#{window_name}'))
	if [ "$out" != "$2" ]; then
		echo "Window list of '$1' wrong."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		exit 1
	fi
}

check_ok new-session -d -s Q -x 80 -y 24 -n q0

# -a breaks into a new window inserted after the target index, shuffling the
# following windows up.
check_ok new-window -d -t Q:1 -n q1
check_ok split-window -d -t Q:1
check_ok break-pane -d -a -s Q:1.1 -n qa -t Q:0
check_windows Q '0:q0 1:qa 2:q1'

# Without -d the new window is selected.
check_ok split-window -d -t Q:2
check_ok select-window -t Q:0
check_ok break-pane -s Q:2.1 -n qcur -t Q:
check_fmt 'Q:' '#{window_name}' 'qcur'
check_windows Q '0:q0 1:qa 2:q1 3:qcur'

# Breaking the only pane of a window relinks the window at a new index; -n
# still renames it.
check_ok new-window -d -t Q:5 -n qsolo
out=$($TMUX break-pane -d -P -F '#{window_name}' -s Q:5.0 -n qmoved -t Q:)
if [ "$out" != "qmoved" ]; then
	echo "single-pane break-pane output wrong: '$out'"
	exit 1
fi
if $TMUX has-session -t Q:qsolo 2>/dev/null; then
	echo "Window 'qsolo' still exists after single-pane break-pane."
	exit 1
fi
check_ok has-session -t Q:qmoved
check_ok kill-session -t Q

# ---------------------------------------------------------------------------
# resize-pane -T.

# -T trims the history: lines below the cursor position are removed and the
# cursor moves to the bottom. seq writes 100 lines (leaving 77 in history on
# a 24-line screen) and the escape sequence puts the cursor on line 5.
check_ok new-window -d -t P:2 'seq 1 100; printf "\033[5;1H"; exec cat'
i=0
while [ "$($TMUX display-message -p -t P:2.0 '#{history_size}')" != "77" ]
do
	i=$((i + 1))
	[ $i -gt 50 ] && { echo "History did not fill."; exit 1; }
	sleep 0.1
done
check_fmt 'P:2.0' '#{cursor_y}' '4'
check_ok resize-pane -T -t P:2.0
check_fmt 'P:2.0' '#{history_size}' '58'
check_fmt 'P:2.0' '#{cursor_y}' '23'
check_ok kill-window -t P:2

assert_alive

$TMUX kill-server 2>/dev/null
exit 0
