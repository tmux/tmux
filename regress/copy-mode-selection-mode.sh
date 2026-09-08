#!/bin/sh

# Changing an existing character selection to line mode must reset both ends
# to complete lines, including when the cursor subsequently crosses the fixed
# end. An invalid regular-expression search must also discard marks left by a
# previous successful search.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0 1 15

$TMUX new-session -d -x30 -y6 \
	"printf 'alpha\nbeta\ngamma\ndelta\n'; exec sleep 100" || exit 1
$TMUX set-option -g window-size manual || exit 1
sleep 1

# A valid regular-expression search creates marks. Replacing it with an
# invalid expression, growing the mode, and scrolling must not leave an old
# mark array indexed with the new geometry (the original bug was an ASan heap
# overflow in this sequence).
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -X search-forward beta || exit 1
[ "$($TMUX display-message -p '#{search_present}')" = 1 ] ||
	fail "successful search did not create marks"
$TMUX send-keys -X search-forward '[' || exit 1
$TMUX resize-window -x40 -y8 || exit 1
$TMUX send-keys -X scroll-down || exit 1
[ "$($TMUX display-message -p '#{search_present}')" = 0 ] ||
	fail "invalid regular expression left stale marks after resize"
$TMUX send-keys -X cancel || exit 1

# Select part of alpha through part of beta, then change the existing
# selection to line mode. Both lines, and no partial columns, must be copied.
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -N2 -X cursor-right || exit 1
$TMUX send-keys -X begin-selection || exit 1
$TMUX send-keys -X cursor-down || exit 1
$TMUX send-keys -N2 -X cursor-right || exit 1
$TMUX send-keys -X selection-mode line || exit 1
[ "$($TMUX display-message -p '#{selection_mode}')" = line ] ||
	fail "selection did not change to line mode"
$TMUX send-keys -X copy-selection || exit 1
expected=$(printf 'alpha\nbeta')
[ "$($TMUX show-buffer)" = "$expected" ] ||
	fail "line-mode selection did not expand to complete lines"

# Start with the cursor above the fixed end so selection-mode itself takes the
# reverse-direction path, then extend it through another line.
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -N2 -X cursor-down || exit 1
$TMUX send-keys -N2 -X cursor-right || exit 1
$TMUX send-keys -X begin-selection || exit 1
$TMUX send-keys -X cursor-up || exit 1
$TMUX send-keys -N2 -X cursor-right || exit 1
$TMUX send-keys -X selection-mode line || exit 1
$TMUX send-keys -X cursor-up || exit 1
$TMUX send-keys -X copy-selection || exit 1
expected=$(printf 'alpha\nbeta\ngamma')
[ "$($TMUX show-buffer)" = "$expected" ] ||
	fail "reversed line-mode selection did not keep complete lines"

exit 0
