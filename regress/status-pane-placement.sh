#!/bin/sh

# Status clicks preserve the placement of one tiled pane and one floating pane.
# Slight mouse movement must not change placement with the default bindings.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/inner -f/dev/null"
TMUX2="$TEST_TMUX -S$DIR/outer -f/dev/null"
trap 'stop_server $TMUX; stop_server $TMUX2; rm -rf "$DIR"' 0
trap 'exit 1' 1 2 15

# Report a failed assertion.
fail()
{
	echo "$*" >&2
	exit 1
}

# Run a command against the inner server.
run()
{
	$TMUX "$@" || fail "failed: $*"
}

# Compare a pane format with its expected value.
check()
{
	got=$(run display -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# Check placement independently of focus and visibility.
placement()
{
	check "$a" '#{pane_floating_flag}' 0
	check "$f" '#{pane_floating_flag}' 1
	check "$f" "$geometry" "$floating"
}

# Send mouse events through the attached client, then check placement.
mouse()
{
	$TMUX2 send-keys -t "$outer" -l "$1" || fail 'send-keys failed'
	sleep 1
	placement
}

# Click a status entry once, twice, or three times without a pause.
click()
{
	press=$(printf '\033[<0;%s;12M\033[<0;%s;12m' "$1" "$1")
	case ${2:-1} in
	1) mouse "$press" ;;
	2) mouse "$press$press" ;;
	3) mouse "$press$press$press" ;;
	esac
}

a=$(run new-session -dP -F '#{pane_id}' -x60 -y12 'exec cat') || exit 1
run set -g mouse on
run set -g status 2
run set -g window-pane-status-format '#{pane_index}:#{pane_title}'
run set -g window-pane-current-status-format '#{pane_index}:#{pane_title}'
run select-pane -t "$a" -T tile
f=$(run new-pane -P -F '#{pane_id}' -x20 -y4 -X20 -Y2 'exec cat') || exit 1
run select-pane -t "$f" -T float

$TMUX2 new-session -d -x60 -y12 "$TMUX attach" || exit 1
outer=$($TMUX2 display -p '#{pane_id}') || exit 1
wait_for_text '1:float' $TMUX2 capture-pane -p -t "$outer" -S11 -E11 ||
    fail 'pane entries not drawn'
line=$($TMUX2 capture-pane -p -t "$outer" -S11 -E11) || exit 1
column_a=$(printf '%s\n' "$line" | awk '{print index($0, "0:tile") + 2}')
column_f=$(printf '%s\n' "$line" | awk '{print index($0, "1:float") + 2}')
geometry='#{pane_left},#{pane_top},#{pane_width},#{pane_height}'
floating=$(run display -p -t "$f" "$geometry") || exit 1
placement

# Without a custom pane-tab drag binding, slight movement during a click
# hides the active pane without swapping its tiled position with the float.
run select-pane -t "$a"
events=$(printf '\033[<0;%s;12M\033[<32;%s;12M\033[<0;%s;12m' \
    "$column_a" "$((column_a + 1))" "$((column_a + 1))")
mouse "$events"
check "$a" '#{pane_hidden_flag}:#{pane_active}' 1:0
check "$f" '#{pane_active}' 1

# Slow and quick clicks both activate, hide, and restore without changing type.
for pane in "$a" "$f"; do
	column=$column_a
	other=$f
	if [ "$pane" = "$f" ]; then
		column=$column_f
		other=$a
	fi
	run select-pane -t "$other"
	click "$column"
	check "$pane" '#{pane_hidden_flag}:#{pane_active}' 0:1
	click "$column"
	check "$pane" '#{pane_hidden_flag}:#{pane_active}' 1:0
	check "$other" '#{pane_active}' 1
	click "$column"
	check "$pane" '#{pane_hidden_flag}:#{pane_active}' 0:1
	run select-pane -t "$other"
	click "$column" 2
	check "$pane" '#{pane_hidden_flag}:#{pane_active}' 1:0
	check "$other" '#{pane_active}' 1
	click "$column"
	check "$pane" '#{pane_hidden_flag}:#{pane_active}' 0:1
	run select-pane -t "$other"
	click "$column" 3
	check "$pane" '#{pane_hidden_flag}:#{pane_active}' 0:1
done

# Hiding every pane and restoring them in either order preserves placement.
for order in "$column_a $column_f" "$column_f $column_a"; do
	run resize-pane -h -t "$a"
	run resize-pane -h -t "$f"
	placement
	for column in $order; do
		click "$column"
	done
	check "$a" '#{pane_hidden_flag}' 0
	check "$f" '#{pane_hidden_flag}' 0
done

exit 0
