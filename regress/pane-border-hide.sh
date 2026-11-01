#!/bin/sh

. ./regress-common.inc

# The pane-border hide button targets tiled, floating and fullscreen panes.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

# Stop the isolated servers after success or failure.
cleanup()
{
	stop_servers
}
trap cleanup 0
trap 'exit 1' 1 2 15

# Report a failed assertion.
fail()
{
	echo "$*" >&2
	exit 1
}

# Wait for a pane format to reach its expected value.
check()
{
	wait_for_output "$3" $TMUX display-message -p -t "$1" "$2" ||
	    fail "$1 $2 did not become '$3'"
}

# Find the hide control after the title and send a one-based mouse click.
click_hide()
{
	wait_for_text "$1" $TMUX2 capture-pane -p || fail 'border not drawn'
	position=$($TMUX2 capture-pane -p | awk -v title="$1" '
	    index($0, title) {
	        start = index($0, title) + length(title)
	        button = index(substr($0, start), "[h]")
	        if (button != 0) {
	            print start + button, NR
	            exit
	        }
	    }')
	[ -n "$position" ] || fail "no hide button for $1"
	set -- $position
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' "$1" "$2" "$1" "$2")
	$TMUX2 send-keys -l "$seq" || fail 'send mouse failed'
	sleep 0.4
}

$TMUX new-session -d -s inner -x 80 -y 24 'sleep 180' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g pane-border-status top || exit 1
$TMUX set -g pane-border-lines simple || exit 1
A=$($TMUX display-message -p '#{pane_id}') || exit 1
B=$($TMUX split-window -hdPF '#{pane_id}' 'sleep 180') || exit 1
$TMUX select-pane -t "$A" -T tile-A || exit 1
$TMUX select-pane -t "$B" -T tile-B || exit 1

$TMUX2 new-session -d -s outer -x 80 -y 24 'sleep 180' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k "$TMUX attach -t inner" || exit 1
wait_for_clients 1 $TMUX || fail 'inner client did not attach'

# Hiding an inactive pane leaves the active pane alone.
check "$A" '#{pane_active}' 1
click_hide tile-B
check "$B" '#{pane_hidden_flag}' 1
check "$A" '#{pane_hidden_flag}:#{pane_active}' 0:1

# Hiding the active pane focuses the exposed tile.
$TMUX select-pane -t "$B" || exit 1
click_hide tile-B
check "$B" '#{pane_hidden_flag}:#{pane_active}' 1:0
check "$A" '#{pane_active}' 1

# Floating and fullscreen panes keep their placement while hidden.
$TMUX resize-pane -Z -t "$A" || exit 1
F=$($TMUX new-pane -PF '#{pane_id}' -x 32 -y 8 -X 10 -Y 5 'sleep 180') || exit 1
$TMUX select-pane -t "$F" -T float-F || exit 1
click_hide float-F
check "$F" '#{pane_hidden_flag}:#{pane_floating_flag}' 1:1
check "$A" '#{pane_zoomed_flag}:#{pane_active}' 1:1
$TMUX resize-pane -h || exit 1
click_hide tile-A
check "$A" '#{pane_hidden_flag}:#{pane_zoomed_flag}' 1:1
check "$F" '#{pane_hidden_flag}:#{pane_active}' 0:1

# The same control works on a bottom pane-border status line.
$TMUX resize-pane -aZ || exit 1
$TMUX resize-pane -H -t "$F" || exit 1
$TMUX select-pane -t "$A" || exit 1
$TMUX select-pane -t "$B" || exit 1
$TMUX set -g pane-border-status bottom || exit 1
click_hide tile-B
check "$B" '#{pane_hidden_flag}:#{pane_active}' 1:0
check "$A" '#{pane_active}' 1

exit 0
