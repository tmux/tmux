#!/bin/sh

# Chooser menus are centred on the mouse using their displayed width. A
# hidden title must not move a borderless menu away from the mouse.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

$TMUX new-session -d -s short -x 100 -y 30 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status off || exit 1
$TMUX2 new-session -d -x 100 -y 30 "$TMUX attach" || exit 1
sleep 1

for name in short aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa; do
	$TMUX rename-session "$name" || exit 1
	for lines in none single padded; do
		$TMUX set -g menu-border-lines "$lines" || exit 1
		$TMUX choose-tree -s || exit 1
		sleep 1

		# Right-button press at window (60, 0), away from either edge.
		seq=$(printf '\033[<2;61;1M')
		$TMUX2 send-keys -l "$seq" || exit 1
		sleep 1

		row=2
		column=54
		if [ "$lines" = none ]; then
			row=1
		elif [ "$name" != short ]; then
			column=31
		fi
		text=$($TMUX2 capture-pane -p | awk -v row="$row" \
		    -v column="$column" 'NR == row {
			# Keep byte offsets equal to columns with non-Unicode awk.
			gsub(/│/, "|")
			print substr($0, column, 7)
		    }')
		[ "$text" = 'Select ' ] || \
		    fail "$lines menu for $name: expected Select at ($column, $row), got '$text'"

		# Close the menu, release the button, then leave the chooser.
		$TMUX2 send-keys Escape || exit 1
		seq=$(printf '\033[<2;61;1m')
		$TMUX2 send-keys -l "$seq" || exit 1
		$TMUX2 send-keys q || exit 1
		sleep 1
	done
done

exit 0
