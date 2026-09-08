#!/bin/sh

# When a write breaks up a wide character or a tab, the columns it covered must
# keep the background colour, both in the grid and on the terminal. The second
# server is attached to the first so that what is actually sent is checked, not
# just what tmux has stored.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

# 1. Write inside a tab. 2. Overwrite the right half of a wide character.
# 3. Two tabs of different colours, the first partly deleted so that padding
#    from both ends up next to each other, then written over.
# The sleep is so that the client is attached before anything is written and
# the changes are sent as they happen rather than as one redraw.
$TMUX2 -f/dev/null new -d -x24 -y3 "
	sleep 2
	printf '\033[44m\033[K\t\033[4GX'
	printf '\033[2;1H\033[44m\033[K\344\270\226\033[2GX'
	printf '\033[3;1H\033[42m\033[K\t\033[43m\033[K\t\033[0m\033[4G\033[8P\rx'
	cat" || exit 1
$TMUX2 set -g status off || exit 1

$TMUX -f/dev/null new -x24 -y4 -d "$TMUX2 attach" || exit 1
sleep 4

$TMUX capturep -pe -S0 -E2 >$TMP || exit 1
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

printf '\033[44m   X\n X\n\033[49mx\033[42m  \033[43m             \033[49m\n' |
	cmp - $TMP || exit 1

exit 0
