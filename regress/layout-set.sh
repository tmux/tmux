#!/bin/sh

# Exercise every predefined layout, next/previous cycling, lookup and restore
# paths, one-pane early returns, minimum-size handling and floating panes.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
SEEN="$DIR/layouts"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

layout()
{
	$TMUX display-message -p -t layouts:many '#{window_layout}'
}

check_layout()
{
	name=$1
	$TMUX select-layout -t layouts:many "$name" ||
		fail "select-layout $name failed"
	value=$(layout)
	[ -n "$value" ] || fail "$name produced an empty layout"
	if grep -Fxq "$value" "$SEEN"; then
		fail "$name produced the same layout as an earlier preset"
	fi
	printf '%s\n' "$value" >>"$SEEN"
	[ "$($TMUX list-panes -t layouts:many -F x | wc -l)" -eq 5 ] ||
		fail "$name lost a pane"
	[ "$($TMUX list-panes -t layouts:many -F '#{pane_floating_flag}' | \
	    grep -c '^1$')" -eq 1 ] || fail "$name did not preserve the floating pane"
}

$TMUX new-session -d -s layouts -n many -x100 -y40 'exec sleep 100' || exit 1
$TMUX set-option -g window-size manual || exit 1
$TMUX split-window -d -h -t layouts:many 'exec sleep 100' || exit 1
$TMUX split-window -d -v -t layouts:many 'exec sleep 100' || exit 1
$TMUX split-window -d -h -t layouts:many 'exec sleep 100' || exit 1
floating=$($TMUX new-pane -dP -F '#{pane_id}' -t layouts:many \
    -x20 -y8 -X10 -Y5 'exec sleep 100') || exit 1
: >"$SEEN"

# Options exercise percentage and explicit secondary-size calculations.
$TMUX set-option -w -t layouts:many main-pane-height 30% || exit 1
$TMUX set-option -w -t layouts:many other-pane-height 12 || exit 1
$TMUX set-option -w -t layouts:many main-pane-width 35% || exit 1
$TMUX set-option -w -t layouts:many other-pane-width 20 || exit 1

for name in even-horizontal even-vertical main-horizontal \
    main-horizontal-mirrored main-vertical main-vertical-mirrored tiled; do
	check_layout "$name"
done

# Exact and unique-prefix lookup work; ambiguous and invalid names fail.
$TMUX select-layout -t layouts:many even-h || fail "unique layout prefix failed"
$TMUX select-layout -t layouts:many main >/dev/null 2>&1 &&
	fail "ambiguous layout prefix succeeded"
$TMUX select-layout -t layouts:many no-such-layout >/dev/null 2>&1 &&
	fail "invalid layout name succeeded"

# Cycle through all layouts in both directions, including wraparound. The
# aliases and select-layout flags share layout_set_next/previous.
first=$(layout)
i=0
while [ "$i" -lt 7 ]; do
	$TMUX next-layout -t layouts:many || exit 1
	i=$((i + 1))
done
[ "$(layout)" = "$first" ] || fail "next-layout did not wrap"
i=0
while [ "$i" -lt 7 ]; do
	$TMUX previous-layout -t layouts:many || exit 1
	i=$((i + 1))
done
[ "$(layout)" = "$first" ] || fail "previous-layout did not wrap"
$TMUX select-layout -t layouts:many -n || exit 1
$TMUX select-layout -t layouts:many -p || exit 1

# No argument reapplies the last preset; -o restores the previous layout; -E
# spreads the current cell without changing the pane set.
$TMUX kill-pane -t "$floating" || exit 1
$TMUX select-layout -t layouts:many tiled || exit 1
tiled=$(layout)
$TMUX select-layout -t layouts:many even-horizontal || exit 1
$TMUX select-layout -t layouts:many -o || exit 1
[ "$(layout)" = "$tiled" ] || fail "select-layout -o did not restore layout"
$TMUX select-layout -t layouts:many || exit 1
$TMUX select-layout -t layouts:many -E || exit 1

# Each arranger has a deliberate one-pane early return.
$TMUX new-window -d -t layouts: -n one 'exec sleep 100' || exit 1
one=$($TMUX display-message -p -t layouts:one '#{window_layout}')
for name in even-horizontal even-vertical main-horizontal \
    main-horizontal-mirrored main-vertical main-vertical-mirrored tiled; do
	$TMUX select-layout -t layouts:one "$name" || exit 1
	[ "$($TMUX display-message -p -t layouts:one '#{window_layout}')" = "$one" ] ||
		fail "$name changed a one-pane layout"
done

# Force the minimum-size paths without requiring a terminal client.
$TMUX resize-window -t layouts:many -x10 -y6 || exit 1
for name in even-horizontal even-vertical main-horizontal \
    main-horizontal-mirrored main-vertical main-vertical-mirrored tiled; do
	$TMUX select-layout -t layouts:many "$name" || exit 1
done

$TMUX has-session -t layouts || fail "server died during layout tests"
exit 0
