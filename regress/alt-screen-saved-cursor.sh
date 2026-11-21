#!/bin/sh

# 1049h saves the cursor when it enters the alternate screen, and neither 47l
# nor 1049l clears it. If the screen was resized after it was saved, 1049l can
# restore a row below the saved grid it reflows.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
DIR=$(mktemp -d)
trap '$TMUX kill-server 2>/dev/null; rm -rf $DIR' 0 1 15

exit_status=0

# Wait until the pane's program has set its path (OSC 7) to $1.
wait_path() {
	_i=0
	while [ "$($TMUX display -p '#{pane_path}' 2>/dev/null)" != "$1" ]; do
		if ! $TMUX has 2>/dev/null; then
			echo "FAIL: $2: the server died"
			exit 1
		fi
		_i=$((_i + 1))
		if [ $_i -ge 400 ]; then
			echo "FAIL: $2: timed out waiting for $1"
			exit_status=1
			return 1
		fi
		sleep 0.05
	done
}

# Respawn the pane, $2 by $3, running $1. Where the program waits for a file
# in $DIR, the test resizes the window and then creates it.
run() {
	rm -f $DIR/go*
	$TMUX resize-window -x $2 -y $3 || exit 1
	$TMUX respawn-pane -k "$1; exec sleep 1000" || exit 1
}

check() {
	if [ "$2" != "$3" ]; then
		echo "FAIL: $1: '$2', not '$3'"
		exit_status=1
	fi
}

$TMUX new -d -x 10 -y 5 'sleep 1000' || exit 1
$TMUX set -g window-size manual || exit 1

# Outside the alternate screen (47l): the cursor after a full line goes to
# the last column, as with no 47l. Unchanged.
run "printf 'abcdefghij\\033[?1049h\\033[?47l\\033[?1049lX\\033]7;done\\007'" 10 5
if wait_path done "1049l outside the alternate screen"; then
	check "1049l outside the alternate screen" \
	    "$($TMUX capture-pane -p | head -1)" abcdefghiX
fi

# A full line, 1049h, a wider screen, 1049l: the cursor is after the line's
# last character and X follows it. Unchanged.
run "printf 'abcdefghij\\033[?1049h\\033]7;in\\007'; while [ ! -e $DIR/go ]; do sleep 0.05; done; printf '\\033[?1049lX\\033]7;done\\007'" 10 5
if wait_path in "full line, then wider"; then
	$TMUX resize-window -x 20 || exit 1
	touch $DIR/go
	wait_path done "full line, then wider" &&
	    check "full line, then wider" \
	    "$($TMUX capture-pane -p | head -1)" abcdefghijX
fi

# cy 8 saved 10 high; 47l, 20x4, 47h, 40x4, 1049l. The cursor goes to the
# last row of the saved grid, which is 4 high.
run "printf '\\033[9;1Hrow\\033[?1049h\\033[?47l\\033]7;one\\007'; while [ ! -e $DIR/go1 ]; do sleep 0.05; done; printf '\\033[?47h\\033]7;two\\007'; while [ ! -e $DIR/go2 ]; do sleep 0.05; done; printf '\\033[?1049lX\\033]7;done\\007'" 40 10
if wait_path one "saved taller"; then
	$TMUX resize-window -x 20 -y 4 || exit 1
	touch $DIR/go1
	if wait_path two "saved taller"; then
		$TMUX resize-window -x 40 -y 4 || exit 1
		touch $DIR/go2
		wait_path done "saved taller" &&
		    check "saved taller" \
		    "$($TMUX capture-pane -p | grep row)" rowX
	fi
fi

exit $exit_status
