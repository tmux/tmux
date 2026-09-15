#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
}
fail()
{
	echo "$1"
	cleanup
	exit 1
}
expect_buffer()
{
	expected=$1
	actual=$($TMUX show-buffer)
	[ "$actual" = "$expected" ] ||
		fail "unexpected buffer: expected [$expected], got [$actual]"
}
wheel()
{
	button=$1
	col=$2
	row=$3
	seq=$(printf '\033[<%s;%s;%sM' "$button" "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

$TMUX new -d -x40 -y10 \
	'i=0; while [ $i -lt 80 ]; do printf "line %02d xxxxxxxxxx\n" $i; i=$((i + 1)); done; cat' ||
	exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g mouse on || exit 1

$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -N10 -X cursor-down || exit 1
$TMUX send-keys -X start-of-line || exit 1
$TMUX send-keys -X begin-selection || exit 1
$TMUX send-keys -N2 -X cursor-down || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1

initial=$(printf 'line 10 xxxxxxxxxx\nline 11 xxxxxxxxxx')
expect_buffer "$initial"

$TMUX send-keys -X stop-selection || exit 1
$TMUX send-keys -N3 -X scroll-down || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -N2 -X scroll-up || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X scroll-middle || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X scroll-bottom || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X scroll-top || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X recentre-top-bottom || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$initial"

$TMUX send-keys -X other-end || exit 1
$TMUX send-keys -X cursor-down || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1

extended_end=$(printf 'line 10 xxxxxxxxxx\nline 11 xxxxxxxxxx\nline 12 xxxxxxxxxx')
expect_buffer "$extended_end"

$TMUX send-keys -X stop-selection || exit 1
$TMUX send-keys -X other-end || exit 1
$TMUX send-keys -X other-end || exit 1
$TMUX send-keys -X cursor-up || exit 1
$TMUX send-keys -X copy-selection-no-clear || exit 1

extended_start=$(printf 'line 09 xxxxxxxxxx\nline 10 xxxxxxxxxx\nline 11 xxxxxxxxxx\nline 12 xxxxxxxxxx')
expect_buffer "$extended_start"

$TMUX2 new-session -d -x40 -y10 "$TMUX attach" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "no outer pane"

wheel 65 5 5
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$extended_start"

wheel 64 5 5
$TMUX send-keys -X copy-selection-no-clear || exit 1
expect_buffer "$extended_start"

# Perform a mouse drag across a sequence of col,row coordinates (1-based).
# SGR mouse format: \033[<btn;col;rowM (btn 0=press, 32=drag) and
# \033[<0;col;rowm (release).
drag()
{
	# Mouse button 1 down (press) at the starting coordinate.
	col=${1%,*}
	row=${1#*,}
	seq=$(printf '\033[<0;%s;%sM' "$col" "$row")
	shift
	# Mouse drag (button 1 held) across each subsequent coordinate.
	for pos; do
		col=${pos%,*}
		row=${pos#*,}
		seq="$seq$(printf '\033[<32;%s;%sM' "$col" "$row")"
	done
	# Mouse button 1 up (release) at the final coordinate.
	seq="$seq$(printf '\033[<0;%s;%sm' "$col" "$row")"
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.5
}

# Turn off status line so all 10 pane rows are visible in the outer client,
# and write a 24-char line on the bottom row (row 10, longer than the 18-char
# line 79 above it).
$TMUX set -g status off || exit 1
$TMUX send-keys -X cancel || exit 1
$TMUX send-keys -l "line 80 xxxxxxxxxxxxxxxx" || exit 1
sleep 0.2

for mode in emacs vi; do
	$TMUX setw -g mode-keys "$mode" || exit 1

	# In emacs mode the selection end column is exclusive, so selecting all
	# characters of an 18- or 24-char line requires placing the cursor one
	# column past the last character; in vi mode the end column is inclusive.
	if [ "$mode" = emacs ]; then
		ecol18=19
		ecol24=25
	else
		ecol18=18
		ecol24=24
	fi

	# Drag along the bottom row (row 10) past the end of the line and back
	# when already at the bottom of history; the selection must stay on
	# line 80 instead of jumping up to line 79.
	$TMUX copy-mode || exit 1
	drag 1,10 "$ecol24,10" "$((ecol24 + 1)),10" "$ecol24,10"
	expect_buffer "line 80 xxxxxxxxxxxxxxxx"

	# Scroll up 1 line so line 80 is off-screen below, then drag from row 9
	# to the bottom row (row 10) to auto-scroll down and select lines 78..80.
	$TMUX copy-mode || exit 1
	$TMUX send-keys -X scroll-up || exit 1
	drag 1,9 23,10 "$ecol24,10"
	expect_buffer "$(printf 'line 78 xxxxxxxxxx\nline 79 xxxxxxxxxx\nline 80 xxxxxxxxxxxxxxxx')"

	# Scroll 1 line below the top of history so line 00 is off-screen above,
	# then drag from row 2 to the top row (row 1) to auto-scroll up and
	# select lines 00..02.
	$TMUX copy-mode || exit 1
	$TMUX send-keys -X history-top || exit 1
	$TMUX send-keys -X scroll-down || exit 1
	drag "$ecol18,2" 1,1
	expect_buffer "$(printf 'line 00 xxxxxxxxxx\nline 01 xxxxxxxxxx\nline 02 xxxxxxxxxx')"
done

exit 0
