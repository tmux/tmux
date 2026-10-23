#!/bin/sh

# An extended key (CSI u) above U+007F must match a binding for the same
# character.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

. ./regress-common.inc

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

cleanup()
{
	stop_server $OUTER || exit 1
	stop_server $INNER || exit 1
}
trap cleanup 0
trap 'exit 1' 1 2 15

cleanup
$INNER new-session -d -s inner -x40 -y10 'exec sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$OUTER new-session -d -s outer -x40 -y10 "$INNER attach -t inner" || exit 1
$OUTER set-option -g status off || exit 1
wait_for_clients 1 $INNER || exit 1

# U+00E4 is two bytes in UTF-8, U+20AC three.
$INNER bind-key -n ä set-option -g @a yes || exit 1
$INNER bind-key -n € set-option -g @e yes || exit 1
$OUTER send-keys -l "$(printf '\033[228;1u')" || exit 1
$OUTER send-keys -l "$(printf '\033[8364;1u')" || exit 1
wait_for_output yes $INNER show-option -gv @e || exit 1
wait_for_output yes $INNER show-option -gv @a || exit 1

exit 0
