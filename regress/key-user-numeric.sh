#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestKUN$$ -f/dev/null"

cleanup() {
        $TMUX kill-server 2>/dev/null
}
trap cleanup 0 1 15

$TMUX new-session -d || exit 1

# A valid User key must be accepted.
$TMUX bind-key User1 display-message valid || exit 1

# Trailing junk must not be accepted as part of a User key.
if $TMUX bind-key User1abc display-message invalid 2>/dev/null; then
        echo "User1abc was incorrectly accepted as User1" >&2
        exit 1
fi

exit 0