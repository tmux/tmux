#!/bin/sh

# Inserting, deleting or erasing characters partway through a tab must not
# leave a tab cell that is wider than the padding cells following it. The
# second server is attached to the first so that what is actually sent is
# checked, not just what tmux has stored. The client is attached only after
# everything has been written so that it is sent a redraw of the whole grid.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
stop_servers

TMP=$(mktemp)
EXP=$(mktemp)
trap 'rm -f "$TMP" "$EXP"; stop_servers' 0 1 15

# 1. Insert two characters at the start of a line of text, using a tab to move
#    over the new blanks (as Emacs does), then delete them again with DCH
#    from the middle of the tab.
# 2. Delete two characters from the middle of a tab.
# 3. Erase two characters from the middle of a tab.
$TMUX2 -f/dev/null new -d -x24 -y3 "
	printf '      alpha: one,'
	printf '\033[1;7H\033[1@\033[1;8H\033[1@'
	printf '\033[1;3H\t\033[1@\033[1;4H\t\033[C\033[1@'
	printf '\033[1;5H\033[C\033[C\033[4P'
	printf '\033[2;1Hab\tX\033[2;5H\033[2P'
	printf '\033[3;1Hab\tX\033[3;5H\033[2X'
	printf '\033]2;input-ready\007'
	cat" || exit 1
$TMUX2 set -g status off || exit 1
wait_for_output input-ready $TMUX2 display-message -p '#{pane_title}' || exit 1

$TMUX -f/dev/null new -x24 -y4 -d "$TMUX2 attach" || exit 1

printf '      alpha: one,\nab    X\nab      X\n' >"$EXP"
if ! wait_until capture_matches_file "$EXP" "$TMP" \
    $TMUX capturep -p -S0 -E2; then
	diff -u "$EXP" "$TMP"
	exit 1
fi

exit 0
