#!/bin/sh

# The copy mode position indicator in a pane partly under a floating pane must
# still show its visible end.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX2 new -d -x30 -y15 "exec sleep 100" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g copy-mode-position-format "#[align=right]ABCDE" || exit 1
$TMUX2 new-pane -x30 -y3 -X-3 -Y0 "exec sleep 100" || exit 1

$TMUX new -d -x30 -y15 "$TMUX2 attach" || exit 1
$TMUX set -g status off || exit 1
wait_for_clients 1 $TMUX2 || exit 1
sleep 1

$TMUX2 copy-mode -t %0 || exit 1
sleep 1
$TMUX2 refresh-client || exit 1
sleep 1

line=$($TMUX capture-pane -p | head -1)
case "$line" in
*DE)
	;;
*)
	echo "row 0: '$line'" >&2
	exit 1
	;;
esac

# The visible half of a clipped wide character should be blank, and a
# fully visible wide character past the floating panes should still be drawn.
$TMUX2 copy-mode -qt %0 || exit 1
$TMUX2 kill-pane -t %1 || exit 1
$TMUX2 respawn-pane -kt %0 'printf "......X            Y"; exec sleep 100' ||
	exit 1
left=$($TMUX2 new-pane -d -PF '#{pane_id}' -B none -x6 -y3 -X0 -Y0 \
    'printf LLLLLL; exec sleep 100') || exit 1
right=$($TMUX2 new-pane -d -PF '#{pane_id}' -B none -x6 -y3 -X20 -Y0 \
    'printf RRRRRR; exec sleep 100') || exit 1

# The floating panes cover columns 0-5 and 20-25. The first two wide
# characters straddle columns 5-6 and 19-20, where X and Y were visible.
wide=$(printf '\344\270\255')
middle=AAAAAAAAAAAA
indicator="12345${wide}${middle}${wide}12345Z${wide}Z"
hidden='LLLLLLX            YRRRRRR'
visible="LLLLLL ${middle} RRRRRRZ${wide}Z"
$TMUX2 set -g copy-mode-position-format "#[align=left]$indicator" || exit 1

check_row()
{
	wait_for_output "$1" $TMUX capture-pane -p -S0 -E0 && return 0
	echo "row 0: '$($TMUX capture-pane -p -S0 -E0)'" >&2
	exit 1
}

check_row "$hidden"
$TMUX2 copy-mode -t %0 || exit 1
check_row "$visible"
$TMUX2 send-keys -Xt %0 toggle-position || exit 1
check_row "$hidden"
$TMUX2 send-keys -Xt %0 toggle-position || exit 1
check_row "$visible"

# Removing the obstructions should reveal the complete indicator.
$TMUX2 kill-pane -t "$left" \; kill-pane -t "$right" || exit 1
check_row "$indicator"

exit 0
