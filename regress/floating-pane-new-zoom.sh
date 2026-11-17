#!/bin/sh

# new-pane -Z must finish the zoom, including for floating panes. Check the
# flags and the actual terminal, then unzoom and compare with an ordinary
# floating pane to check that the saved geometry and background are restored.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -LtestA$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestB$$ -f/dev/null"
DIR=$(mktemp -d) || exit 1

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	exit 1
}

check()
{
	got=$($INNER display-message -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

wait_capture()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p >"$DIR/capture" || exit 1
		cmp -s "$1" "$DIR/capture" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	diff -u "$1" "$DIR/capture" >&2
	fail "terminal did not match $1"
}

$INNER new-session -d -x 60 -y 20 'sleep 100' || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g default-command 'sleep 100' || exit 1
initial=$($INNER display-message -p '#{pane_id}') || exit 1
base=$($INNER split-window -PF '#{pane_id}' '') || exit 1
$INNER kill-pane -t "$initial" || exit 1
other=$($INNER split-window -h -PF '#{pane_id}' '') || exit 1
printf 'LEFT_BASE' | $INNER display-message -I -t "$base" || exit 1
printf 'RIGHT_BASE' | $INNER display-message -I -t "$other" || exit 1

$OUTER new-session -d -x 60 -y 20 'sleep 100' || exit 1
$OUTER set -g status off || exit 1
$OUTER set -g default-terminal screen || exit 1
$OUTER respawn-pane -k "$INNER attach-session" || exit 1

# Build a reference with the requested floating geometry, without zoom.
float=$($INNER new-pane -PF '#{pane_id}' -x 80% -y 80% -X 10% -Y 10% '') ||
	exit 1
sleep 1
$OUTER capture-pane -p >"$DIR/floating" || exit 1
grep -q LEFT_BASE "$DIR/floating" || fail "missing background"
grep -q '┌' "$DIR/floating" || fail "missing floating border"
$INNER kill-pane -t "$float" || exit 1

# The zoomed pane is empty and must cover the whole terminal.
awk 'BEGIN { for (i = 0; i < 20; i++) print "" }' >"$DIR/zoomed"
for mode in default command empty already-zoomed; do
	$INNER select-pane -t "$base" || exit 1
	if [ "$mode" = already-zoomed ]; then
		$INNER resize-pane -Z -t "$base" || exit 1
	fi
	case "$mode" in
	default) set -- ;;
	command) set -- 'sleep 100' ;;
	*) set -- '' ;;
	esac
	float=$($INNER new-pane -Z -PF '#{pane_id}' \
	    -x 80% -y 80% -X 10% -Y 10% "$@") || exit 1
	check "$float" '#{pane_zoomed_flag}:#{window_zoomed_flag}:#{pane_floating_flag}' \
	    '1:1:1'
	check "$float" '#{pane_width}:#{pane_height}:#{pane_left}:#{pane_top}' \
	    '60:20:0:0'
	wait_capture "$DIR/zoomed"

	# Output from the hidden tiled panes must not overwrite the zoomed pane.
	printf '\033[HLEFT_BASE' | $INNER display-message -I -t "$base" || exit 1
	printf '\033[HRIGHT_BASE' | $INNER display-message -I -t "$other" || exit 1
	sleep 0.2
	wait_capture "$DIR/zoomed"

	$INNER resize-pane -Z -t "$float" || exit 1
	check "$float" '#{pane_zoomed_flag}:#{window_zoomed_flag}:#{pane_floating_flag}' \
	    '0:0:1'
	check "$float" '#{pane_width}:#{pane_height}:#{pane_left}:#{pane_top}' \
	    '46:14:7:3'
	wait_capture "$DIR/floating"
	$INNER kill-pane -t "$float" || exit 1
done

# A detached creation zooms the active pane and retains the new float's
# requested geometry for unzooming.
$INNER select-pane -t "$base" || exit 1
float=$($INNER new-pane -dZ -PF '#{pane_id}' \
    -x 80% -y 80% -X 10% -Y 10% '') || exit 1
check "$base" '#{pane_zoomed_flag}:#{window_zoomed_flag}' '1:1'
check "$float" '#{pane_zoomed_flag}:#{pane_active}' '0:0'
$INNER resize-pane -Z -t "$base" || exit 1
check "$float" '#{pane_width}:#{pane_height}:#{pane_left}:#{pane_top}' \
    '46:14:7:3'
wait_capture "$DIR/floating"
$INNER kill-pane -t "$float" || exit 1

exit 0
