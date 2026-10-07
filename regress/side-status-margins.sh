#!/bin/sh

# On a terminal with left and right margins, scrolling a pane beside the side
# status line sets the margins to the pane's columns. The inner client is
# told the terminal has margins and its output is recorded raw from the outer
# pane, where ESC [ left ; right s sets the margins (1-based).

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-margins-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-margins-outer-$$ -f/dev/null"

DIR=$(mktemp -d) || exit 1
TRIGGER=$DIR/trigger
OUT=$DIR/out

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
	rm -f "$TRIGGER" "$OUT"
	rmdir "$DIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

# Scroll a full width pane with the side status line on side $1 and check the
# margins sent are $2.
check()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
	rm -f "$TRIGGER" "$OUT"

	$TMUX new-session -d -s inner -x 60 -y 10 "sh -c '
		while [ ! -e $TRIGGER ]; do sleep 0.1; done
		i=0
		while [ \$i -lt 15 ]; do i=\$((i + 1)); echo line\$i; done
		exec sleep 100'" || exit 1
	$TMUX set -g status off || exit 1
	$TMUX set -g side-status "$1" || exit 1
	$TMUX set -as terminal-features ',*:margins' || exit 1
	$TMUX2 new-session -d -s outer -x 60 -y 10 'sleep 100' || exit 1
	$TMUX2 set -g status off || exit 1
	$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
	sleep 1
	$TMUX2 pipe-pane -t outer:0.0 "cat >$OUT" || exit 1
	sleep 0.5
	: >"$TRIGGER"
	sleep 1

	got=$(tr '\033' E <"$OUT" | grep -o 'E\[[0-9]*;[0-9]*s' | sort -u |
	    tr -d 'E[s' | tr '\n' ' ')
	[ "$got" = "$2 " ] || fail "side-status $1: margins '$got', expected '$2'"
}

# Pane in columns 15 to 60 after a left side status line of width 14.
check left '15;60'

# Pane in columns 1 to 46 before a right side status line.
check right '1;46'

exit 0
