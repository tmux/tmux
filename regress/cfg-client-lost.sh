#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMPDIR=$(mktemp -d) || exit 1
TMUX="$TEST_TMUX -S$TMPDIR/tmux.sock"
CONF="$TMPDIR/tmux.conf"
READY="$TMPDIR/ready"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMPDIR"
}
trap cleanup 0 1 15

cat <<EOF >$CONF
new-session -d -s keep 'sleep 1000'
run-shell -b 'touch $READY'
run-shell -d 1 true
source-file /dev/null
EOF

$TMUX -f$CONF new-session -d -s client 'sleep 100' &
pid=$!
i=0
while [ ! -f "$READY" ]; do
	[ "$i" -eq 50 ] && exit 1
	i=$((i + 1))
	sleep 0.1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

sleep 2
$TMUX has-session -t keep || exit 1

exit 0
