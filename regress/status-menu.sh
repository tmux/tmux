#!/bin/sh

# Status menus are displayed in the visible window and their commands
# target the clicked window, including when it is not current.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

CAPTURE=$(mktemp) || exit 1
cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
	rm -f "$CAPTURE"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	cat "$CAPTURE" >&2
	exit 1
}

# Right-button press at column $1, row $2 (1-based), targeting window $3.
check_menu()
{
	seq=$(printf '\033[<2;%s;%sM' "$1" "$2")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || exit 1
	i=0
	while [ "$i" -lt 50 ]; do
		$TMUX2 capture-pane -p -t outer:0.0 >"$CAPTURE" || exit 1
		grep -q 'Swap Left' "$CAPTURE" && break
		sleep 0.1
		i=$((i + 1))
	done
	grep -q 'Swap Left' "$CAPTURE" || fail "window $3 menu not visible"
	grep -q "─$3:" "$CAPTURE" || fail "window $3 title missing"

	# Mark acts on the clicked window without selecting it. Other windows
	# exercise dragging into the menu and releasing the right button.
	if [ "$3" = 1 ]; then
		pos=$(awk '{
			gsub(/│/, "|")
			if (index($0, " Mark "))
				print index($0, " Mark ") + 1, NR
		}' "$CAPTURE")
		[ -n "$pos" ] || fail "Mark item missing"
		seq=$(printf '\033[<34;%s;%sM' $pos)
		$TMUX2 send-keys -t outer:0.0 -l "$seq" || exit 1
		sleep 0.1
		seq=$(printf '\033[<2;%s;%sm' $pos)
		$TMUX2 send-keys -t outer:0.0 -l "$seq" || exit 1
	else
		$TMUX2 send-keys -t outer:0.0 m || exit 1
		seq=$(printf '\033[<2;%s;%sm' "$1" "$2")
		$TMUX2 send-keys -t outer:0.0 -l "$seq" || exit 1
	fi
	i=0
	while [ "$i" -lt 50 ]; do
		marked=$($TMUX display -p -t inner:"$3" '#{pane_marked}')
		[ "$marked" = 1 ] && break
		sleep 0.1
		i=$((i + 1))
	done
	[ "$marked" = 1 ] || fail "Mark did not target window $3"
	current=$($TMUX display -p '#{window_index}')
	[ "$current" = 0 ] || fail "menu changed current window to $current"
	$TMUX select-pane -M || exit 1
	sleep 0.5
}

$TMUX new-session -d -s inner -n zero -x 80 -y 24 'sleep 100' || exit 1
$TMUX new-window -d -n one 'sleep 100' || exit 1
$TMUX new-window -d -n two 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g automatic-rename off || exit 1
$TMUX set -g status 2 || exit 1
$TMUX set -g 'status-format[0]' \
    '#[range=window|0]0:zero#[norange] #[range=window|1]1:one#[norange]' || \
    exit 1
$TMUX2 new-session -d -s outer -x 80 -y 24 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

for position in top bottom; do
	$TMUX set -g status-position "$position" || exit 1
	if [ "$position" = top ]; then
		row=1
	else
		row=23
	fi
	sleep 0.5
	check_menu 2 "$row" 0
	check_menu 9 "$row" 1
done

# A noncurrent window's dimensions must not constrain the visible menu.
$TMUX resize-window -t inner:1 -x 10 -y 5 || exit 1
check_menu 9 23 1

exit 0
