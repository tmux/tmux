#!/bin/sh

# suspend-client should leave a control client attached and receiving output

PATH=/bin:/usr/bin
TERM=screen

. ./regress-common.inc

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
FIFO=$TMP.fifo
trap 'rm -f "$TMP" "$FIFO"; $TMUX kill-server 2>/dev/null' 0 1 15
mkfifo $FIFO || exit 1

$TMUX new -d -s a || exit 1
$TMUX -C attach -t a <$FIFO >$TMP &
exec 3>$FIFO
wait_for_clients 1 $TMUX || exit 1

C=$($TMUX lsc -F '#{client_name}')
$TMUX suspend-client -t "$C" || exit 1
$TMUX send-keys -t a 'echo fo''o' Enter || exit 1

wait_until grep -q '^%output.*foo' $TMP || exit 1
[ "$($TMUX lsc -F '#{client_name}')" = "$C" ] || exit 1
[ "$($TMUX display -p -t a '#{session_attached}')" = 1 ] || exit 1

exec 3>&-
exit 0
