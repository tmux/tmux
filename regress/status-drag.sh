#!/bin/sh

. ./regress-common.inc

# Status drags must target the window under the pointer as it moves.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
DIR=$(test_tmpdir) || exit 1
CAPTURE=$DIR/capture

# Stop both isolated servers.
cleanup()
{
	stop_server $TMUX2 || exit 1
	stop_server $TMUX || exit 1
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

# Report a failed check.
fail()
{
	echo "$*" >&2
	exit 1
}

# Send a one-based SGR mouse report through the attached client's terminal.
mouse()
{
	seq=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail 'send mouse failed'
}

# Check the window order after each drag update.
check_order()
{
	wait_for_output "$(printf '%s\n' "$@")" \
	    $TMUX list-windows -F '#{window_name}' ||
	    fail "window order did not become '$*'"
}

# Check the actual terminal columns before sending mouse input to a side line.
side_drawn()
{
	$TMUX2 capture-pane -p -S0 -E2 >"$CAPTURE" || return 1
	awk -v column="$1" '
	    { text = substr($0, column, 12); gsub(/[^A-Z]/, "", text); print text }
	' "$CAPTURE"
}

$TMUX new-session -d -s inner -n AAA -x 60 -y 20 'sleep 180' || exit 1
$TMUX new-window -d -n BBB 'sleep 180' || exit 1
$TMUX new-window -d -n CCC 'sleep 180' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status-left '' || exit 1
$TMUX set -g status-right '' || exit 1
$TMUX set -g window-status-format '#W' || exit 1
$TMUX set -g window-status-current-format '#W' || exit 1

# The interactive reordering workaround from GitHub issue 4549.
$TMUX bind -n MouseDown1Status \
    'selectw ; set -F @drag_window "#{window_id}"' || exit 1
$TMUX bind -n MouseDrag1Status run -C 'swapw -ds#{@drag_window}' || exit 1
$TMUX bind -n MouseDragEnd1Status set -u @drag_window || exit 1
$TMUX2 new-session -d -s outer -x 60 -y 20 'sleep 180' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
wait_for_clients 1 $TMUX || fail 'inner client did not attach'

for position in bottom top; do
	$TMUX set -g status-position "$position" || exit 1
	row=20
	[ "$position" = top ] && row=1
	wait_for_text 'AAA BBB CCC' $TMUX2 capture-pane -p \
	    -S "$((row - 1))" -E "$((row - 1))" || fail 'status did not draw'

	# Establish the drag on AAA, then cross BBB and CCC without releasing.
	mouse 0 1 "$row" M
	mouse 32 2 "$row" M
	mouse 32 5 "$row" M
	check_order BBB AAA CCC
	mouse 32 9 "$row" M
	check_order BBB CCC AAA
	mouse 0 9 "$row" m
	wait_for_output '' $TMUX show -qv @drag_window ||
	    fail 'drag end did not clear the saved window'

	# Restore the order for the next position.
	$TMUX swap-window -d -s inner:2 -t inner:1 || exit 1
	$TMUX swap-window -d -s inner:1 -t inner:0 || exit 1
done

# The same targeting must work vertically on either side status line.
$TMUX set -g status off || exit 1
$TMUX set -g side-status-format \
    '#{W:#[range=window|#{window_index}]#W#[norange]#[nl]}' || exit 1
for side in left right; do
	$TMUX set -g side-status "$side" || exit 1
	column=2
	[ "$side" = right ] && column=48
	start=1
	[ "$side" = right ] && start=49
	wait_for_output "$(printf 'AAA\nBBB\nCCC')" side_drawn "$start" ||
	    fail 'side status did not draw'
	mouse 0 "$column" 1 M
	mouse 32 "$((column + 1))" 1 M
	mouse 32 "$column" 2 M
	check_order BBB AAA CCC
	mouse 32 "$column" 3 M
	check_order BBB CCC AAA
	mouse 0 "$column" 3 m
	wait_for_output '' $TMUX show -qv @drag_window ||
	    fail 'side drag end did not clear the saved window'
	$TMUX swap-window -d -s inner:2 -t inner:1 || exit 1
	$TMUX swap-window -d -s inner:1 -t inner:0 || exit 1
done

# Keep real coordinates for bindings that use motion outside the status area.
$TMUX set -g side-status off || exit 1
$TMUX set -g status-position top || exit 1
$TMUX set -g status 2 || exit 1
$TMUX set -g 'status-format[0]' '#[range=window|0]AAA#[norange]' || exit 1
$TMUX set -g 'status-format[1]' '#[range=control|0]CONTROL#[norange]' || exit 1
$TMUX bind -n MouseDrag1Status \
    set -gF @position '#{mouse_absolute_x},#{mouse_absolute_y}' || exit 1
$TMUX bind -n MouseDrag1Control0 set -g @control yes || exit 1
$TMUX bind -n MouseDrag1Pane set -g @pane yes || exit 1
$TMUX bind -n MouseDrag1Empty set -g @pane yes || exit 1
wait_for_text CONTROL $TMUX2 capture-pane -p -S1 -E1 ||
    fail 'second status line did not draw'
mouse 0 1 1 M
mouse 32 2 1 M
mouse 32 2 2 M
wait_for_output yes $TMUX show -gv @control ||
    fail 'drag did not target the second status line'
mouse 32 3 6 M
wait_for_output 2,5 $TMUX show -gv @position ||
    fail 'status drag did not retain the pointer coordinates outside status'

# Changing the status height during a drag must leave it usable.
$TMUX set -g status on || exit 1
mouse 32 3 5 M
wait_for_output 2,4 $TMUX show -gv @position ||
    fail 'status drag stopped after the status height changed'
[ -z "$($TMUX show -qgv @pane)" ] ||
    fail 'status drag invoked a pane binding'
mouse 0 2 1 m

exit 0
