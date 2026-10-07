#!/bin/sh

# With more windows than rows, the default side status format scrolls the
# window list to keep the current window visible and replaces the first and
# last visible rows with up and down markers.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-scroll-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-scroll-outer-$$ -f/dev/null"

TMP=$(mktemp) || exit 1
cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
	rm -f "$TMP"
}
trap cleanup 0 1 15

$TMUX new-session -d -s inner -n w0 -x 60 -y 10 'sleep 100' || exit 1
i=1
while [ "$i" -lt 15 ]; do
	$TMUX new-window -d -n "w$i" 'sleep 100' || exit 1
	i=$((i + 1))
done
$TMUX set -g status off || exit 1
$TMUX set -g side-status left || exit 1
$TMUX select-window -t inner:7 || exit 1
$TMUX2 new-session -d -s outer -x 60 -y 10 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

# Compare the outer pane with the expected rows on standard input.
compare()
{
	$TMUX2 capture-pane -p -t outer:0.0 >"$TMP" || exit 1
	if ! cmp -s - "$TMP"; then
		echo "$1 differs:" >&2
		cat "$TMP" >&2
		exit 1
	fi
}

compare "scrolled side status" <<EOF
↑            │
3:w3         │
4:w4         │
5:w5         │
6:w6         │
7:w7*        │
8:w8         │
9:w9         │
10:w10       │
↓            │
EOF

# A long current window name in the first row is cut on the right like the
# other rows.
$TMUX rename-window -t inner:0 dotfiles-repo || exit 1
$TMUX select-window -t inner:0 || exit 1
sleep 1
compare "first row" <<EOF
0:dotfiles-re│
1:w1         │
2:w2         │
3:w3         │
4:w4         │
5:w5         │
6:w6         │
7:w7-        │
8:w8         │
↓            │
EOF

exit 0
