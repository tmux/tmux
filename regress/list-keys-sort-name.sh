#!/bin/sh

# list-keys -O name orders by table name, then by key within each table

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestLKSN$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP*; $TMUX kill-server 2>/dev/null" 0 1 15

$TMUX new -d || exit 1

F='#{key_table} #{key_string}'

# Expected: each table in name order, keys in the table's own (key) order.
for t in $($TMUX list-keys -F '#{key_table}' | LC_ALL=C sort -fu); do
	$TMUX list-keys -T $t -F "$F"
done >$TMP.exp
[ -s $TMP.exp ] || exit 1

$TMUX list-keys -O name -F "$F" >$TMP.out
cmp -s $TMP.exp $TMP.out || exit 1

# Reversed is the exact reverse.
$TMUX list-keys -O name -r -F "$F" |
	awk '{ l[NR] = $0 } END { for (i = NR; i > 0; i--) print l[i] }' \
	>$TMP.out
cmp -s $TMP.exp $TMP.out || exit 1

exit 0
