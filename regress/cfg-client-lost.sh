#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMPDIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$TMPDIR/tmux.sock"
CONF="$TMPDIR/tmux.conf"
READY="$TMPDIR/ready"

cleanup()
{
	stop_server $TMUX || exit 1
	rm -rf "$TMPDIR"
}
trap cleanup 0 1 15

cat <<EOF >$CONF
new-session -d -s keep 'sleep 1000'
run-shell -b 'touch $READY'
run-shell -d 1 true
source-file /dev/null
set -g @loaded 1
EOF

$TMUX -f$CONF new-session -d -s client 'sleep 100' &
pid=$!
wait_until test -f "$READY" || exit 1
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

wait_for_output 1 $TMUX show -gv @loaded || exit 1
$TMUX has-session -t keep || exit 1

exit 0
